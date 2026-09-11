#!/usr/bin/env python3
"""What the compiler, the linker and the test suite structurally cannot see.

THIS PORT NEEDS IT MORE THAN ANY OTHER, because it has THIRTEEN overlays where
the C128 has eleven, and two of them page code on the hot path. Rule 4 -- only
main() or a declared pair may call into a window -- is the one that let a
guaranteed crash reach a player on three ports at once, and the check caught
the missing run_turn/OVL_ENEMY pairing the first time that split was built.

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
ATARI = HERE.parents[1]
ROOT = ATARI.parent
sys.path.insert(0, str(ROOT / "tools"))
import overlay_check  # noqa: E402  -- needs the path above

LD = ATARI / "atari.ld"
# THE LINK THAT SHIPS, not the one budget.py measures. build/early.elf carries
# src/stubs.c and exists to report a number; this checks the game.
ELF = ATARI / "build" / "trekatari.xex.elf"
MAP = ATARI / "build" / "trekatari.map"
NOLTO = ATARI / "build" / "nolto"

LLVM_MOS = pathlib.Path.home() / "llvm-mos"
OBJDUMP = str(LLVM_MOS / "bin" / "llvm-objdump")

BASE, TOP = 0x3000, 0xC000


def die(msg):
    sys.exit("verify: " + msg)


def ld_number(pattern):
    """A number out of atari.ld, so this file never carries a second copy.

    The MEGA65's verify printed 'of 40959' for two days while its linker
    script said 40447 -- every free-space figure 512 bytes too generous,
    because the limit had been typed in twice. On 2026-09-09 this port had the
    same fault in a worse form: atari.ld held the window size as __ovl_size
    AND as 0x1200 literals in its MEMORY block, so changing one moved the
    REPORT and not the LINK. The literals are gone; this reads the survivor.
    """
    m = re.search(pattern, LD.read_text())
    if not m:
        die("atari.ld no longer matches /%s/ -- this check cannot bound itself"
            % pattern)
    return int(m.group(1), 0)


def check_staging_distinct(window):
    """THE STAGING REGIONS MUST NOT OVERLAP, and they did until 2026-09-09.

    They sat 0x1000 apart while being 0x1200 long, so each ran 512 bytes into
    the next -- and the link runs with --no-check-sections, so an overlay over
    4,096 bytes would have written into its neighbour's image with no error at
    all. Nothing had reached 4,096, which is the only reason it never bit.
    """
    text = LD.read_text()
    origins = [int(m, 16) for m in
               re.findall(r"ovlimg\d+\s*\(rw\)?\s*\)?\s*:\s*ORIGIN = (0x[0-9a-f]+)", text)]
    if not origins:
        die("atari.ld declares no staging regions")
    for a, b in zip(sorted(origins), sorted(origins)[1:]):
        if b - a < window:
            die("staging regions are %d apart but the window is %d, so image "
                "$%06x overlaps $%06x -- and --no-check-sections means the "
                "linker will not say so" % (b - a, window, a, b))
    print("verify: %d staging regions, %d apart, none can overlap a %d window"
          % (len(origins), min(b - a for a, b in zip(sorted(origins), sorted(origins)[1:])),
             window))


def check_lowram():
    """THE OS SPARE AREA, reported by name -- a resource nobody reports is a
       resource nobody manages, which is the lesson the C128's lowram check
       was written for and this port has two such pools rather than one.

       The linker would refuse an overflow, but it would not say how close it
       came, and 640 bytes with 626 of them spent is worth seeing every build.
    """
    text = LD.read_text()
    m = re.search(r"lowram\s*\(rw\)\s*:\s*ORIGIN = (0x[0-9a-f]+),\s*LENGTH = (0x[0-9a-f]+)",
                  text)
    if not m:
        print("verify: no lowram region -- writable data all competes with code")
        return
    origin, length = int(m.group(1), 0), int(m.group(2), 0)
    used = 0
    for ln in subprocess.run([OBJDUMP, "-h", str(ELF)],
                             capture_output=True, text=True).stdout.splitlines():
        f = ln.split()
        if len(f) >= 4 and f[0].isdigit() and f[1] == ".lowbss":
            used = int(f[2], 16)
    print("verify: lowram $%04X..$%04X, %d of %d used, %d free"
          % (origin, origin + length - 1, used, length, length - used))
    if used > length:
        die("lowbss has overrun the OS spare area by %d bytes" % (used - length))


def check_headroom(window):
    """THE TWO POOLS, reported by name every build.

    A resource nobody reports is a resource nobody manages -- the C128's
    lowram hit 77 bytes free before anyone looked.

    THERE ARE TWO NOW, and that is the whole point of dropping DOS. Until
    2026-09-11 the program's code and all of its data shared $3000..$ACFF, so
    one number described both. With no DOS resident, $0700..$1FFF is free RAM
    that nothing else wants, and atari.ld puts .rodata, .data, .bss and
    .noinit down there -- which hands their address space back to CODE, the
    only pool this port has ever been short of.

    So the figure this used to print -- the end of .noinit against the window
    -- now describes neither pool. It read $1147 and called 39,865 bytes free
    on a machine with 32K of program space.
    """
    # READ FROM THE MAP, NOT FROM nm -- the same trap tools/budget.py carries a
    # note about. OUTPUT_FORMAT makes the link emit an XEX rather than an ELF,
    # so llvm-nm has nothing to read: it returns an empty symbol list, and a
    # check that trusts it reports a free-space figure computed from zero.
    txt = MAP.read_text()

    # VMA, LMA, size, name -- the map's section rows. .data is the one that
    # differs: it RUNS at $0A00 and is LOADED high, after .rodata, so it
    # counts against both pools and in different columns.
    rows = {}
    for vma, lma, size, name in re.findall(
            r"^\s*([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+\d+\s+"
            r"(\.[A-Za-z_.]+)\s*$", txt, re.M):
        rows[name] = (int(vma, 16), int(lma, 16), int(size, 16))

    for need in (".text", ".rodata", ".data", ".bss"):  # .noinit may be empty
        if need not in rows:
            die("no %s row in the map -- cannot bound the pools" % need)

    def sym(name):
        m = re.search(r"^\s+([0-9a-f]+)\s+[0-9a-f]+\s+\d+\s+\d+\s+"
                      + re.escape(name) + r"\s*=", txt, re.M)
        return int(m.group(1), 16) if m else None

    # ---- the code pool: $3000 up to the window, less the stack's reserve.
    # THE LOAD ADDRESS, not the run address. .rodata runs AND loads low, so it
    # is not here at all; .data runs low but is LOADED up here beside the
    # code, and that byte count is what costs the pool.
    code_end = max(lma + size for _, lma, size in
                   (rows[n] for n in (".text", ".data")))
    top = TOP - window
    # THE SOFT STACK'S RESERVE COMES OFF THE FREE SPACE, and reporting the
    # figure without it is how this check would start lying. The stack lives in
    # $AD00..$AE00 now -- see atari.ld -- because llvm-mos points it at
    # MEMTOP+1, which landed INSIDE the overlay window and corrupted the
    # running image. Read from the map so the two cannot drift apart.
    # THE VALUE AFTER THE `=`, NOT THE VMA COLUMN. An absolute assignment in
    # the map reads `0 0 0 1 __stack_reserve = 0x100`, so sym() above -- which
    # returns the first column -- gives 0 for it. That silently disabled this
    # check on its first run.
    m = re.search(r"^\s+[0-9a-f]+\s+[0-9a-f]+\s+\d+\s+\d+\s+"
                  r"__stack_reserve\s*=\s*(0x[0-9a-fA-F]+|\d+)\s*$",
                  txt, re.M)
    reserve = int(m.group(1), 0) if m else 0
    stack_top = top - reserve
    free = stack_top - code_end
    print("verify: code $%04X..$%04X, %d bytes free below the stack reserve "
          "at $%04X" % (BASE, code_end, free, stack_top))
    if reserve:
        print("verify: soft stack $%04X..$%04X, %d bytes reserved (MEMTOP is "
              "set to $%04X at load)" % (stack_top, top, reserve, top - 1))
    else:
        die("no __stack_reserve in the map -- the soft stack starts at "
            "MEMTOP+1 and would grow into the overlay window")
    if free < 0:
        die("code has overrun the window by %d bytes" % -free)
    if free < 256:
        print("verify: NOTE -- under 256 bytes free, and .ovl_front grows with "
              "the save record")

    # ---- the low pool: where DOS used to be.
    low_base = ld_number(r"lowdata \(rw\)\s*:\s*ORIGIN\s*=\s*"
                         r"(0x[0-9a-fA-F]+)")
    low_len = ld_number(r"lowdata \(rw\)[^\n]*LENGTH\s*=\s*"
                        r"(0x[0-9a-fA-F]+)\s*-") - low_base
    low_end = max(vma + size for vma, _, size in
                  (rows[n] for n in (".rodata", ".data", ".bss", ".noinit")
                   if n in rows))
    low_used = low_end - low_base
    print("verify: low data $%04X..$%04X, %d of %d used, %d free (where DOS "
          "was)" % (low_base, low_end, low_used, low_len, low_len - low_used))
    if low_used > low_len:
        die("low data has overrun $%04X by %d bytes"
            % (low_base + low_len, low_used - low_len))
    # THE BOOT RECORD IS STILL LIVE WHILE THESE SEGMENTS LOAD -- see atari.ld.
    # $0A00 is the first address past its sector buffer, and a pool that
    # started lower would be loaded over the loader reading it in.
    if low_base < 0x0A00:
        die("low data starts at $%04X, below the boot record's buffer at $0900"
            % low_base)


def main():
    if not ELF.exists() or not MAP.exists():
        die("the game must be linked first -- run `make game`")
    window = ld_number(r"__ovl_size\s*=\s*(0x[0-9a-f]+)")
    print("verify: window %d bytes, from atari.ld" % window)

    check_staging_distinct(window)
    overlay_check.check_overlay_layout(MAP, window, die)
    overlay_check.check_overlay_calls(ELF, OBJDUMP, die)
    if NOLTO.is_dir():
        overlay_check.check_resident_calls(NOLTO, OBJDUMP, die)
    else:
        die("build/nolto is missing -- rule 4 can only be read WITHOUT LTO, "
            "which folds the offending caller into main()")
    check_headroom(window)
    check_lowram()
    print("verify: ok")


main()
