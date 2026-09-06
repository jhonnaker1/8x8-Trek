#!/usr/bin/env python3
"""What the compiler, the linker and the test suite structurally cannot see.

THIS PORT HAD NO VERIFY AT ALL until 2026-09-06, which is the same gap the
MEGA65 carried for six days -- and it is how a guaranteed crash reached a
player: main.c took the score AFTER swapping the overlay window, through a
resident function whose body lives in the window it had just replaced. The
machine dropped into its monitor at the hall of fame. Rule 4 in
tools/overlay_check.py is that bug, and it is checked here now.

Deliberately NOT re-checked here: message widths, dialog geometry, the
briefing, the key table. Those are properties of shared sources and belong to
`make -C c128 verify`, which reads the same files. Duplicating them would mean
two places to update and one of them going stale.
"""
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve()
X16 = HERE.parents[1]
ROOT = X16.parent
sys.path.insert(0, str(ROOT / "tools"))
import overlay_check  # noqa: E402  -- needs the path above

LD = X16 / "x16.ld"
ELF = X16 / "build" / "trekx16.elf"
PRG = X16 / "build" / "trekx16.prg"
IMG = X16 / "build" / "data" / "OVERLAYS.BIN"
NOLTO = X16 / "build" / "nolto"

LLVM_MOS = pathlib.Path.home() / "llvm-mos"
OBJDUMP = str(LLVM_MOS / "bin" / "llvm-objdump")
OBJCOPY = str(LLVM_MOS / "bin" / "llvm-objcopy")

OVERLAYS = ["eval", "hof", "front", "info", "repair", "msgs",
            "planet", "cmds", "title", "events", "xtra"]


def die(msg):
    sys.exit("verify: " + msg)


def ld_number(pattern):
    """A number out of x16.ld, so this file never carries a second copy.

    The MEGA65's verify printed 'of 40959' for two days while its linker
    script said 40447 -- every free-space figure 512 bytes too generous,
    because the limit had been typed in twice.
    """
    m = re.search(pattern, LD.read_text())
    if not m:
        die("x16.ld no longer matches /%s/ -- this check cannot bound itself"
            % pattern)
    return int(m.group(1), 0)


def check_load_address():
    d = PRG.read_bytes()
    addr = d[0] | (d[1] << 8)
    if addr != 0x0801:
        die("PRG loads at $%04X, not $0801 -- x16emu -run will not start it"
            % addr)
    print("verify: PRG loads at $0801 -- ok")


def check_window():
    """The window size lives in x16.ld, in the Makefile AND in the loader.
       One of them drifting is a silent wrong-length read at run time."""
    size = ld_number(r"__ovl_size\s*=\s*(0x[0-9a-f]+)")
    mk = re.search(r"^OVL_WINDOW\s*=\s*(\d+)", (X16 / "Makefile").read_text(),
                   re.M)
    if not mk or int(mk.group(1)) != size:
        die("x16.ld says window %d, Makefile says %s -- they must agree"
            % (size, mk.group(1) if mk else "nothing"))
    print("verify: window %d bytes, linker script and Makefile agree" % size)
    return size


def check_headroom(size):
    """THE TWO POOLS THIS PORT CAN RUN OUT OF, reported every build.

    LOW RAM is the tight one and it is not simply "free space": everything
    between the last variable and __stack is where the SOFT STACK runs, and
    llvm-mos does not check it. On 2026-09-06 a 68-byte cosmetic fix took the
    gap from 154 bytes to 86 before it was noticed, which is the reason this
    exists -- a resource nobody reports is a resource nobody manages, the same
    lesson the C128's lowram check was written for.

    THE WINDOW is the other, and it moves the opposite way: the two share a
    boundary, so bytes trimmed off the window land in low RAM one for one.
    Printing them together is what makes that trade visible.
    """
    nm = subprocess.run([str(LLVM_MOS / "bin" / "llvm-nm"), "--numeric-sort",
                         str(ELF)], capture_output=True, text=True).stdout
    stack = None
    for ln in nm.splitlines():
        p = ln.split()
        if p[-1:] == ["__stack"]:
            stack = int(p[0], 16)
    hdr = subprocess.run([OBJDUMP, "-h", str(ELF)],
                         capture_output=True, text=True).stdout
    last, biggest, bigname = 0, 0, "?"
    for ln in hdr.splitlines():
        f = ln.split()
        if len(f) < 5 or not f[0].isdigit():
            continue
        name, sz, vma = f[1], int(f[2], 16), int(f[3], 16)
        if name.startswith(".ovl_"):
            if sz > biggest:
                biggest, bigname = sz, name[5:]
        elif name in (".text", ".data", ".bss", ".noinit"):
            last = max(last, vma + sz)
    if stack is None:
        die("no __stack in the link -- cannot bound low RAM")
    gap = stack - last
    print("verify: low RAM ends $%04X, __stack $%04X -- %d bytes for the "
          "soft stack" % (last, stack, gap))
    if gap < 0:
        die("low RAM has overrun the overlay window")
    if gap < 64:
        die("only %d bytes below __stack -- the soft stack has no room" % gap)
    print("verify: largest overlay %s %d + 2 stamp of %d, %d spare"
          % (bigname, biggest, size, size - biggest - 2))
    if biggest + 2 > size:
        die("overlay %s does not fit the window" % bigname)


def check_images(size):
    """Every slot in OVERLAYS.BIN byte-identical to the section it came from.

    A STALE IMAGE FILE MIMICS ANY BUG: on the MEGA65 it reset the machine to
    BASIC and looked like a dozen different faults before anyone thought to
    compare the bytes. It is also how the autoplay build, which writes its own
    images over the same filename, can quietly poison a game disk.
    """
    if not IMG.exists():
        die("OVERLAYS.BIN missing -- run `make data`")
    blob = IMG.read_bytes()
    if len(blob) != size * len(OVERLAYS):
        die("OVERLAYS.BIN is %d bytes, expected %d (%d slots of %d)"
            % (len(blob), size * len(OVERLAYS), len(OVERLAYS), size))
    for i, name in enumerate(OVERLAYS):
        out = X16 / "build" / ("verify_%s.bin" % name)
        subprocess.run([OBJCOPY, "-O", "binary",
                        "--only-section=.ovl_" + name, str(ELF), str(out)],
                       check=True)
        want = out.read_bytes()
        if len(want) > size:
            die("overlay %s is %d bytes, window is %d"
                % (name, len(want), size))
        got = blob[i * size:i * size + len(want)]
        if got != want:
            die("overlay %s in OVERLAYS.BIN does not match the ELF -- "
                "the image file is stale" % name)
        out.unlink()
    print("verify: %d overlay images, each byte-identical to its ELF section "
          "-- ok" % len(OVERLAYS))


def main():
    if not PRG.exists():
        die("%s not built -- run `make game` first" % PRG)
    check_load_address()
    size = check_window()
    overlay_check.check_overlay_calls(ELF, OBJDUMP, die)
    overlay_check.check_resident_calls(NOLTO, OBJDUMP, die)
    check_headroom(size)
    check_images(size)


main()
