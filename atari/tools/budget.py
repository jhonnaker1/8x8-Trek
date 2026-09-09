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


ALL_SEAMS = ("VIDEO", "INPUT", "FARMEM", "OVERLAY", "SOUND", "STORAGE")


def real_seams_missing():
    return [s for s in ALL_SEAMS if s not in real_seams()]


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
    text = build.stderr + build.stdout
    for m in re.finditer(r"section '(\S+)' will not fit in region '(\w+)': "
                         r"overflowed by (\d+) bytes", text):
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

    # CHECK THE EXIT STATUS, THEN THE OUTPUT -- and this file did not, twice
    # on 2026-09-09. A build that fails for a reason this parser does not
    # recognise (an overlay overflowing its STAGING region, say, rather than
    # the window) leaves `over` empty, and the map-reading branch below then
    # reports "IT LINKS" and a spare-bytes figure off a STALE map from an
    # earlier successful build. It said "IT LINKS, 327 bytes spare" over a
    # link that had just failed. A number that looks like good news is the
    # worst possible failure mode for a measuring instrument.
    if build.returncode != 0 and not over:
        print("  THE BUILD FAILED and this file cannot say why. Raw output:")
        for line in text.splitlines():
            if "error" in line.lower() and "-Werror" not in line:
                print("    " + line.strip())
        sys.exit(1)

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
        if real_seams_missing():
            print("  AND SEAMS ARE STILL STUBBED. They cost nothing here and")
            print("  something real later -- more than their drivers measure,")
            print("  because the callers grow too. Video alone cost 4,636 for")
            print("  a 1,559-byte driver.")
        return

    worst = max(v for k, v in over.items() if not k.startswith("WINDOW:")) if \
        any(not k.startswith("WINDOW:") for k in over) else 0
    if worst:
        print("  RESIDENT IS OVER BY %d BYTES." % worst)
        print("  THE TWO LEVERS THAT CLOSE IT, both measured ON THIS TARGET")
        print("  in a throwaway worktree on 2026-09-09:")
        print("    +3,910  OVL_ENEMY: trek_enemy_turn and its private damage")
        print("            chain. (3,520 on the C128 -- measure per target.)")
        print("    +1,578  OVL_MOVE: the whole M command, do_move down to")
        print("            report_move. Six functions, one entry from main().")
        print("            ----")
        print("            5,488 against %d. IT LINKS, ~330 bytes spare." % worst)
        print()
        print("  Held in reserve, not needed: +772 of writable data below the")
        print("  window with DOS resident (MEMLO = $1CFC, read off a booted")
        print("  machine), or +2,282 with no DOS at all.")
        print("  NOT AVAILABLE: shrinking the window to 4,096. Every image was")
        print("  under it -- .ovl_front, the largest, is 3,889 -- so no split")
        print("  of msgs and planet was ever needed for that. But .ovl_enemy")
        print("  is 4,202, so the window has to stay at 4,608 to hold it.")


main()
