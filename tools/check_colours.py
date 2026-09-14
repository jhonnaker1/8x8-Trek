#!/usr/bin/env python3
"""Can a machine carry this game's COLOUR, before anyone writes a driver for it?

WHY THIS EXISTS. The rule that kept machines out of this project was "80
columns", and deriving it on 2026-09-13 showed width was never the thing doing
the work -- two of the three panel bands already fit forty columns, and the
32-column machines are excluded by COLOUR, not by width. Bundling the two axes
is what produced a wrong rule that then sat in two documents disagreeing with
each other for weeks.

So this is the other axis, made mechanical. `scr_put(x, y, glyph, colour)`
carries ONE FOREGROUND PER CELL on a common background, and some of those
colours are game rules rather than decoration: the manual says what a Mongol
class looks like and what a base looks like. A machine that cannot show those
as DISTINCT colours is playing a different game -- which is the same argument
that kept the PET 8032 out for being monochrome.

THE SET IS DERIVED, NOT RECITED. Reciting is what this project keeps getting
wrong, so the information-bearing colours are read out of core/ega.h and
c128/src/ui.c every run. Add a new EGA_CHART_* or a new department colour and
the requirement GROWS BY ITSELF; nothing here has to be remembered.

It answers a question you can ask before writing a line of a port: hand it a
machine's sixteen-entry palette mapping and it says whether the game survives
on it.
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
EGA_H = os.path.join(ROOT, "core", "ega.h")
UI_C  = os.path.join(ROOT, "c128", "src", "ui.c")

# The VIC-II palette, in the order the hardware numbers it. The C64 is the
# first candidate because it is the ranked-easiest sub-80 target (item 57).
C64 = {
    "EGA_BLACK": 0, "EGA_WHITE": 1, "EGA_RED": 2, "EGA_CYAN": 3,
    "EGA_MAGENTA": 4, "EGA_GREEN": 5, "EGA_BLUE": 6, "EGA_YELLOW": 7,
    # EGA 6 is the manual's "orange" for a base; the C64 HAS a true orange (8)
    # and EGA renders that entry brown. 9 is the C64's brown -- either is
    # defensible and both are distinct from everything else, which is all this
    # check can speak to.
    "EGA_BROWN": 9, "EGA_DKGRAY": 11, "EGA_LTRED": 10, "EGA_LTGREEN": 13,
    "EGA_LTBLUE": 14, "EGA_LTGRAY": 15,
    # THE TWO COLLISIONS, and they are why this tool reports rather than just
    # passing: the C64 has no light cyan and no light magenta, so both fold
    # onto their dark siblings. Neither carries information -- LTCYAN is the
    # panel border and LTMAGENTA is unused -- but a machine where a collision
    # landed on an information colour would fail below.
    "EGA_LTCYAN": 3, "EGA_LTMAGENTA": 4,
}

CANDIDATES = {"c64": C64}


def ega_indices():
    """EGA name -> index, straight out of core/ega.h."""
    out = {}
    for m in re.finditer(r'^#define\s+(EGA_[A-Z]+)\s+(\d+)', open(EGA_H).read(), re.M):
        out[m.group(1)] = int(m.group(2))
    return out


def informational(names):
    """The colours that are GAME RULES. Derived from both places they live."""
    src = open(EGA_H).read()
    want = {}
    # core/ega.h says so itself: "Colour as information -- these are game
    # rules, not decoration". Each is an alias for a plain EGA name.
    for m in re.finditer(r'^#define\s+(EGA_(?:MONGOL|CHART)_[A-Z]+|EGA_VANDAL)\s+(EGA_[A-Z]+)', src, re.M):
        want[m.group(1)] = m.group(2)

    # AND THE DEPARTMENT COLOURS, which are equally measured off the original
    # (tools/msg_colours.py attributed every message site) but live in ui.c
    # rather than in the core. That split is worth noticing, not working
    # around -- see the note in NOTES.md.
    ui = open(UI_C).read()
    dept = re.search(r'static unsigned char dept_color\(.*?\n\}', ui, re.S)
    if not dept:
        sys.exit("check_colours: dept_color() not found in ui.c -- this tool "
                 "reads it, so a rename silently shrinks the requirement")
    for i, m in enumerate(re.finditer(r'EGA_TO_VDC\((EGA_[A-Z]+)\)', dept.group(0))):
        want["dept_%d" % i] = m.group(1)
    for m in re.finditer(r'^#define\s+(COL_MSG|COL_LABEL)\s+EGA_TO_VDC\((EGA_[A-Z]+)\)', ui, re.M):
        want[m.group(1)] = m.group(2)
    return want


def main():
    names = ega_indices()
    if len(names) < 16:
        sys.exit("check_colours: core/ega.h gave %d colours, expected 16 -- "
                 "the parse is wrong, not the palette" % len(names))
    want = informational(names)
    if not want:
        sys.exit("check_colours: derived ZERO information-bearing colours. "
                 "An empty requirement PASSES EVERYTHING, so this is a failure.")

    needed = sorted({v for v in want.values()}, key=lambda n: names[n])
    print("  information-bearing colours, derived: %d" % len(needed))
    for n in needed:
        users = sorted(k for k, v in want.items() if v == n)
        print("     %-14s EGA %2d   %s" % (n, names[n], ", ".join(users)))

    bad = 0
    for machine, table in sorted(CANDIDATES.items()):
        seen = {}
        # PER MACHINE. This counter used to be the outer `bad`, so once one
        # candidate failed no later one could ever report "all distinct" --
        # harmless with a single machine and wrong the moment a second lands,
        # which is the whole point of a template.
        lost = 0
        print("\n  %s:" % machine)
        for n in needed:
            if n not in table:
                print("     %-14s NOT MAPPED" % n); lost += 1; continue
            native = table[n]
            if native in seen:
                print("     %-14s -> %-3d COLLIDES with %s" % (n, native, seen[native]))
                lost += 1
            else:
                print("     %-14s -> %d" % (n, native))
                seen[native] = n
        if lost == 0:
            print("     all %d distinct -- the game's colour survives on %s"
                  % (len(needed), machine))
        bad += lost
    if bad:
        print("\ncheck_colours: %d information-bearing colour(s) lost" % bad)
        return 1
    print("\ncheck_colours: %d of %d candidate machine(s) carry every "
          "information-bearing colour" % (len(CANDIDATES), len(CANDIDATES)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
