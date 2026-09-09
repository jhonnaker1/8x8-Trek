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


def check_headroom(window):
    """THE RESIDENT POOL, reported by name every build.

    A resource nobody reports is a resource nobody manages -- the C128's
    lowram hit 77 bytes free before anyone looked, and this port links with a
    few hundred. Everything between the last variable and the window is also
    where the soft stack runs, and llvm-mos does not check it.
    """
    # READ FROM THE MAP, NOT FROM nm -- the same trap tools/budget.py carries a
    # note about. OUTPUT_FORMAT makes the link emit an XEX rather than an ELF,
    # so llvm-nm has nothing to read: it returns an empty symbol list, and a
    # check that trusts it reports a free-space figure computed from zero.
    txt = MAP.read_text()

    def sym(name):
        m = re.search(r"^\s+([0-9a-f]+)\s+[0-9a-f]+\s+\d+\s+\d+\s+"
                      + re.escape(name) + r"\s*=", txt, re.M)
        return int(m.group(1), 16) if m else None

    end = sym("__heap_start") or sym("__bss_end")
    if end is None:
        die("no __heap_start or __bss_end in build/early.map -- cannot bound "
            "resident")
    top = TOP - window
    free = top - end
    print("verify: resident $%04X..$%04X, %d bytes free below the window at $%04X"
          % (BASE, end, free, top))
    if free < 0:
        die("resident has overrun the window by %d bytes" % -free)
    if free < 256:
        print("verify: NOTE -- under 256 bytes free, and .ovl_front grows with "
              "the save record")


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
    print("verify: ok")


main()
