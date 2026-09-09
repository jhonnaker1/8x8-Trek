#!/usr/bin/env python3
"""How far off does EGA Trek link on this machine, and what would close it?

THE SCOPE ENDS WITH AN INSTRUCTION -- "Measure it properly before committing:
link the whole game early" -- and this is that measurement, kept runnable so
the number moves as code moves rather than being quoted from a note.

It expects the link to FAIL at this stage and reports by how much. That is the
point: the answer is information about how much has to be paged, not a build
that has to succeed yet.

AND IT NO LONGER MEASURES A FULLY STUBBED GAME. As each driver lands, its
stubs drop out of src/stubs.c and this link gets the real one -- so the
number converges on the truth instead of being corrected by hand at the end.
The header below says which seams are real, read out of the Makefile rather
than kept here as a second copy that could disagree.

WHY THAT MATTERS MORE THAN IT SOUNDS: replacing the video stubs with the real
driver cost 4,636 bytes, against the driver's own 1,559. The rest is main()
and the eight ui_draw_* routines growing, because a stub that folds to one
volatile write lets the optimiser collapse the argument setup at every call
site and a real driver does not. A seam costs more than its driver, and the
"4,539 bytes" this file used to quote for the X16's layers is a measurement
of drivers.
"""
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve()
ATARI = HERE.parents[1]
NM = str(pathlib.Path.home() / "llvm-mos/bin/llvm-nm")
ELF = ATARI / "build" / "early.elf"

BASE, TOP = 0x3000, 0xC000          # $C000 up is the XL's OS ROM


def real_seams():
    """Which seams are built for real in this link -- from the Makefile, so
       there is never a second list here to drift out of step with it."""
    m = re.search(r"^EARLY_DEFS\s*=\s*(.*)$", (ATARI / "Makefile").read_text(), re.M)
    if not m:
        return []
    return sorted(re.findall(r"-DATARI_HAVE_(\w+)", m.group(1)))


def window_size():
    """From atari.ld, so this file never carries a second copy of it."""
    m = re.search(r"__ovl_size\s*=\s*(0x[0-9a-f]+)", (ATARI / "atari.ld").read_text())
    if not m:
        sys.exit("budget: no __ovl_size in atari.ld")
    return int(m.group(1), 16)


def main():
    win = window_size()
    resident_space = (TOP - BASE) - win

    build = subprocess.run(["make", "-C", str(ATARI), "build/early.elf"],
                           capture_output=True, text=True)
    over = {}
    for m in re.finditer(r"section '(\S+)' will not fit in region '(\w+)': "
                         r"overflowed by (\d+) bytes", build.stderr + build.stdout):
        sec, region, n = m.group(1), m.group(2), int(m.group(3))
        if region == "ram":
            over[sec] = n
        elif region == "window":
            over.setdefault("WINDOW:" + sec, n)

    real = real_seams()
    print("atari/vbxe budget -- REAL seams: %s"
          % (", ".join(s.lower() for s in real) if real else "none, all stubbed"))
    print("  address space   $%04X..$%04X       %6d bytes" % (BASE, TOP - 1, TOP - BASE))
    print("  overlay window  %6d bytes" % win)
    print("  for resident    %6d bytes" % resident_space)
    print()

    wins = {k: v for k, v in over.items() if k.startswith("WINDOW:")}
    if wins:
        print("  AN OVERLAY DOES NOT FIT THE WINDOW:")
        for k, v in sorted(wins.items()):
            print("    %-16s over by %d" % (k[7:], v))
        print()

    if not over:
        """READ FROM THE MAP, NOT FROM nm. The link emits an XEX through
           OUTPUT_FORMAT, not an ELF, so llvm-nm has nothing to read -- it
           returned an empty symbol list and this printed 'resident ends
           $0000, 44544 bytes spare', which is a nonsense that looks like
           good news. The map is the only thing that knows."""
        txt = (ATARI / "build" / "early.map").read_text()
        def sym(name):
            m = re.search(r"^\s+([0-9a-f]+)\s+[0-9a-f]+\s+\d+\s+\d+\s+"
                          + re.escape(name) + r"\s*=", txt, re.M)
            return int(m.group(1), 16) if m else None
        end = sym("__heap_start") or sym("__bss_end")
        if end is None:
            sys.exit("budget: no end symbol in build/early.map")
        code_end = sym("__data_end") or end
        window = BASE + resident_space
        print("  IT LINKS.")
        print("    code+rodata      $%04X..$%04X   %6d bytes" % (BASE, code_end, code_end - BASE))
        print("    writable data    $%04X..$%04X   %6d bytes" % (code_end, end, end - code_end))
        print("    spare below the window          %6d bytes" % (window - end))
        print()
        print("  Seams still stubbed cost NOTHING here and something real")
        print("  later -- and more than their drivers measure, because the")
        print("  callers grow too. Video alone cost 4,636 for a 1,559-byte")
        print("  driver.")
        return

    worst = max(v for k, v in over.items() if not k.startswith("WINDOW:")) if \
        any(not k.startswith("WINDOW:") for k in over) else 0
    if worst:
        print("  RESIDENT IS OVER BY %d BYTES." % worst)
        print("  For scale, the C128 fits the same game in 37,823 bytes using")
        print("  eleven 4,096-byte overlays. THE THREE LEVERS, all measured:")
        print("    +3,520  a twelfth overlay for trek_enemy_turn and its")
        print("            private damage chain (built on the C128 2026-09-08)")
        print("      +772  writable data below the window WITH DOS RESIDENT.")
        print("            DOS 2.5 puts MEMLO at $1CFC, read off a booted")
        print("            machine, so $1CFC..$1FFF is all there is. It is")
        print("            2,282 with no DOS -- and the disk seam needs DOS,")
        print("            because saves are named by the player. README.md.")
        print("      +512  splitting msgs and planet so the window is 4,096")


main()
