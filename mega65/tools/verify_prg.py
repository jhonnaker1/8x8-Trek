#!/usr/bin/env python3
"""Check the linked MEGA65 binary and its overlay images.

WHY THIS EXISTS, and why it is not a copy of the C128's. This port had NO
verify step of any kind until 2026-09-05, six days after the C128's grew the
overlay call-graph check that caught a guaranteed crash. Same architecture,
same one window, same `.ovl_*` sections -- and nothing was looking.

What is checked here, and what deliberately is not:

    load address      the PRG must start at $2001 or Xemu does not auto-run it
    resident space    the image against mega65.ld's own `ram` region
    overlay layout    shared with the C128 -- tools/overlay_check.py
    overlay calls     shared -- rules 2 and 3 of core/overlay.h
    OVERLAYS.BIN      ten 4K slots, each one byte-identical to its ELF section
    build stamp       the last two bytes really are this link's ovl_load

NOT here, and each for a reason rather than an omission:

  * The message, panel, dialog, briefing and linebuf widths are checks on
    SHARED sources (core/, c128/src/ui.c, c128/src/layout.c) against SHARED
    layout constants. They cannot come out differently for this target, and
    `make -C c128 verify` runs them. Duplicating them here would double the
    maintenance of a check that can only ever agree.
  * The key table. There ISN'T one -- $D610 hands this port ASCII directly
    (see src/m65input.c), so the PETSCII trap the C128 check exists for cannot
    occur, and there is no table whose coverage could be incomplete.

Every bound below is read from mega65.ld or the ELF rather than written down
here, because a limit stated twice is a limit that will disagree with itself.
THAT IS NOT HYPOTHETICAL: the Makefile printed "resident N bytes of 40959"
while the linker script's region was 40447, so every free-space figure this
port has ever reported was 512 bytes too generous.
"""
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve()
M65 = HERE.parents[1]
ROOT = M65.parent
sys.path.insert(0, str(ROOT / "tools"))
import overlay_check  # noqa: E402  -- needs the path above

LD = M65 / "mega65.ld"
ELF = M65 / "build" / "egatrek.elf"
MAP = M65 / "build" / "egatrek.map"
PRG = M65 / "build" / "egatrek.prg"
IMG = M65 / "build" / "OVERLAYS.BIN"

LLVM_MOS = pathlib.Path.home() / "llvm-mos"
OBJDUMP = str(LLVM_MOS / "bin" / "llvm-objdump")
OBJCOPY = str(LLVM_MOS / "bin" / "llvm-objcopy")
NM = str(LLVM_MOS / "bin" / "llvm-nm")

# The Makefile's own OVERLAYS list, in order -- the stamp goes in the LAST
# slot, so the order is not cosmetic.
OVERLAYS = ["eval", "hof", "front", "info", "repair",
            "msgs", "planet", "cmds", "title", "events", "xtra"]

STAMP = 2          # bytes of build stamp written into the image's tail


def die(msg):
    print("VERIFY FAILED: " + msg, file=sys.stderr)
    sys.exit(1)


def region(name):
    """(origin, length) of a MEMORY region, read out of mega65.ld itself."""
    m = re.search(r"^\s*%s\s*\([^)]*\)\s*:\s*ORIGIN\s*=\s*(0x[0-9a-fA-F]+)\s*,"
                  r"\s*LENGTH\s*=\s*(0x[0-9a-fA-F]+)" % re.escape(name),
                  LD.read_text(), re.M)
    if not m:
        die(f"mega65.ld has no `{name}` region -- this script is reading the "
            f"wrong linker script, or the region was renamed")
    return int(m.group(1), 16), int(m.group(2), 16)


def check_load_address():
    """$2001, or Xemu types a SYS line and waits -- which looks like a hang.

    tools/run.sh carries the same warning: a PRG that is not recognised as
    BASIC is not auto-run, and the symptom is a machine sitting at a prompt
    with no error anywhere. Two bugs in this port were diagnosed from the
    resulting black screen before anyone checked the first two bytes.
    """
    blob = PRG.read_bytes()
    load = blob[0] | (blob[1] << 8)
    if load != 0x2001:
        die(f"{PRG.name} loads at ${load:04x}, not $2001. Xemu will not "
            f"auto-run it;\n         it will type a SYS line and wait, which "
            f"looks exactly like a hang.")
    print(f"verify: PRG loads at $2001, so Xemu auto-runs it -- ok")


def check_resident():
    """The resident image against mega65.ld's `ram`, not a number typed twice.

    The guard below the soft stack is part of that region's LENGTH already --
    overflowing .bss into it is a link error, which is the whole point of
    leaving it unallocated. So the only thing to report is how much of the
    region the image actually uses.
    """
    origin, length = region("ram")
    size = PRG.stat().st_size - 2          # less the load address
    free = length - size
    print("verify: resident %d bytes of %d at $%04x, %d free"
          % (size, length, origin, free))
    if free < 0:
        die(f"the resident image overruns `ram` by {-free} bytes -- the link "
            f"should have failed")


def check_images():
    """OVERLAYS.BIN is ten 4K slots, each byte-identical to its ELF section.

    THE PADDING ARITHMETIC IN THE MAKEFILE CANNOT REPORT ITS OWN FAILURE:

        open('OVERLAYS.BIN','ab').write(d + b'\\0'*(4096-len(d)))

    For an overlay over 4096 bytes the multiplier is negative, `b'\\0'*-n` is
    the empty string, and the slot is written OVER-LENGTH -- silently shifting
    every later slot's offset, so m65mem.c DMAs the wrong bytes into the
    window for every overlay after it. No exception, no warning, and the file
    is simply the wrong shape. The overlay-layout check catches the cause;
    this catches the effect, and either alone would have been enough.
    """
    window = region("window")[1]
    want = len(OVERLAYS) * window
    got = IMG.stat().st_size
    if got != want:
        die(f"{IMG.name} is {got} bytes, expected {len(OVERLAYS)} x {window} "
            f"= {want}.\n         An overlay over {window} bytes writes an "
            f"over-length slot and shifts\n         every one after it.")

    blob = IMG.read_bytes()
    for i, name in enumerate(OVERLAYS):
        raw = M65 / "build" / f"verify_ovl_{name}.raw"
        subprocess.run([OBJCOPY, "-O", "binary", "--only-section=.ovl_" + name,
                        str(ELF), str(raw)], check=True)
        want_bytes = raw.read_bytes()
        raw.unlink()
        slot = blob[i * window:(i + 1) * window]
        if slot[:len(want_bytes)] != want_bytes:
            die(f"slot {i} of {IMG.name} is not .ovl_{name} from this ELF -- "
                f"the file is\n         stale, or the slots have shifted.")
        # Padding must be zero, except the stamp in the very last slot.
        tail = slot[len(want_bytes):]
        if i == len(OVERLAYS) - 1:
            tail = tail[:-STAMP] if len(tail) >= STAMP else tail
        if any(tail):
            die(f"slot {i} (.ovl_{name}) has non-zero padding after its code, "
                f"so the\n         slots are not aligned the way m65mem.c "
                f"reads them.")
    print("verify: %d overlay images, each byte-identical to its ELF section -- ok"
          % len(OVERLAYS))


def check_stamp():
    """The last two bytes are THIS link's ovl_load. See m65mem.c.

    An overlay is linked WITH the resident half, so every call it makes into
    resident code is a fixed address from that same link. A stale OVERLAYS.BIN
    beside a fresh program jumps into the middle of some other function -- no
    error, nothing naming either file. That cost an afternoon here and
    produced two wrong diagnoses before the stamp existed.
    """
    nm = subprocess.check_output([NM, str(ELF)]).decode()
    addr = [int(l.split()[0], 16) for l in nm.splitlines()
            if l.split()[-1:] == ["ovl_load"]]
    if not addr:
        die("no ovl_load in the ELF -- the stamp has nothing to name")
    want = addr[0] & 0xFFFF
    tail = IMG.read_bytes()[-STAMP:]
    got = tail[0] | (tail[1] << 8)
    if got != want:
        die(f"{IMG.name} is stamped ${got:04x} but this link's ovl_load is "
            f"${want:04x}.\n         The images and the program are from "
            f"different builds.")
    print(f"verify: overlay images stamped ${got:04x}, matching this link -- ok")


def main():
    for f in (PRG, ELF, MAP, IMG):
        if not f.exists():
            die(f"{f} not built yet -- run make first")

    check_load_address()
    check_resident()
    overlay_check.check_overlay_layout(MAP, region("window")[1], die,
                                       reserve=STAMP)
    overlay_check.check_overlay_calls(ELF, OBJDUMP, die)
    overlay_check.check_resident_calls(M65 / "build" / "nolto", OBJDUMP, die)
    check_images()
    check_stamp()
    return 0


if __name__ == "__main__":
    sys.exit(main())
