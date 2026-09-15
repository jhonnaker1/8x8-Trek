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

# ---------------------------------------------------------------------------
# THE CANDIDATES THE 40-COLUMN TEMPLATE PUT BACK IN PLAY (added 2026-09-14).
#
# Two of these were ruled out ON WIDTH ALONE and that reason is gone: the Atari
# ST line "320x200 in 16 colours (ONLY 40 COLUMNS)" and the Apple IIgs "320
# mode is 16 colours and 40 columns, which fails the other hard rule". The
# hard rule was 80 columns. There is no such rule any more, so they come back
# here to be judged on the axis that actually does the work.
#
# A PROGRAMMABLE 16-ENTRY PALETTE IS THE IDENTITY MAPPING. Where a machine
# lets the port choose all sixteen entries, there is nothing to check and the
# table says so by being the identity -- that is a real answer, not a dodge.
IDENTITY = {n: i for i, n in enumerate([
    "EGA_BLACK", "EGA_BLUE", "EGA_GREEN", "EGA_CYAN", "EGA_RED", "EGA_MAGENTA",
    "EGA_BROWN", "EGA_LTGRAY", "EGA_DKGRAY", "EGA_LTBLUE", "EGA_LTGREEN",
    "EGA_LTCYAN", "EGA_LTRED", "EGA_LTMAGENTA", "EGA_YELLOW", "EGA_WHITE"])}

# The CBM-II P500 has a REAL VIC-II, so its palette is the C64's exactly. What
# differs on that machine is where the chip lives, not what it can show.
P500 = dict(C64)

# The Plus/4's TED: 121 colours as (luminance 0-7) x (hue 0-15), hue 0 black.
# Encoded lum*16+hue purely so distinctness can be checked; the driver would
# write the two nibbles. THE POINT IS THE HEADROOM -- unlike the C64 there is
# no fold, because light and dark are separate luminances of the same hue
# rather than separate palette entries that may or may not exist.
def _ted(hue, lum): return lum * 16 + hue
PLUS4 = {
    "EGA_BLACK": _ted(0, 0),
    "EGA_BLUE": _ted(6, 3),   "EGA_GREEN": _ted(5, 3),
    "EGA_CYAN": _ted(3, 3),   "EGA_RED": _ted(2, 3),
    "EGA_MAGENTA": _ted(4, 3),
    "EGA_BROWN": _ted(9, 2),          # TED hue 9 is orange; brown is it, dim
    "EGA_LTGRAY": _ted(1, 4), "EGA_DKGRAY": _ted(1, 2),
    "EGA_LTBLUE": _ted(6, 6), "EGA_LTGREEN": _ted(5, 6),
    "EGA_LTCYAN": _ted(3, 6), "EGA_LTRED": _ted(2, 6),
    "EGA_LTMAGENTA": _ted(4, 6),
    "EGA_YELLOW": _ted(7, 7), "EGA_WHITE": _ted(1, 7),
}

# The CoCo 3's GIME in 80-column TEXT with ATTR, and NO SuperSprite card --
# the route to a port that runs on Jamie's actual machine (item 55).
#
# MEASURED, not read off a data sheet: a real CoCo 3 ROM booted in XRoar,
# `WIDTH 80` then `ATTR f,b` across all eight foreground values with an
# 80-character ruler. The capture holds EIGHT DISTINCT HUES -- the attribute
# byte's three bits of foreground, indexing palette slots that are themselves
# reprogrammable from 64.
#
# EIGHT SLOTS AND EIGHT INFORMATION-BEARING COLOURS. It fits with NOTHING TO
# SPARE, which this tool reports as a pass and the decorative-collapse count
# below reports as the cost. In 2026-09-05 this machine was dropped on the
# claim that "the console uses FIFTEEN colours, not eight" -- true, and it
# answers a different question: fifteen are USED, eight are LOAD-BEARING.
GIME8 = {
    "EGA_BLACK": -1,            # the background, not a foreground slot
    "EGA_GREEN": 0,
    "EGA_CYAN": 1,   "EGA_LTCYAN": 1,
    "EGA_RED": 2,    "EGA_LTRED": 2,
    "EGA_MAGENTA": 3, "EGA_LTMAGENTA": 3,
    "EGA_BROWN": 4,  "EGA_YELLOW": 4,
    "EGA_LTGRAY": 5, "EGA_WHITE": 5, "EGA_DKGRAY": 5,
    "EGA_LTBLUE": 6, "EGA_BLUE": 6,
    "EGA_LTGREEN": 7,
}

CANDIDATES = {
    "c64": C64,                 # BUILT and released, v0.16.0
    "plus4": PLUS4,             # TED, 40x25, 121 colours
    "p500": P500,               # CBM-II, a real VIC-II in a bank
    "atari-st": IDENTITY,       # 320x200x16, palette from 512 (4096 on an STE)
    "msx2": IDENTITY,           # V9938 SCREEN 5, 16 from 512
    "f256": IDENTITY,           # Vicky, 16-entry CLUT
    "iigs": IDENTITY,           # 320 mode, 16 from 4096
    "coco3-gime": GIME8,        # NO card: eight foreground slots, measured
}


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

        # THE COST OF A PASS, which the pass itself hides. A machine can carry
        # every GAME RULE and still fold colours the player can currently tell
        # apart -- the C64 folds two (LTCYAN, LTMAGENTA), a machine with eight
        # slots folds seven. That is not a failure and it is not nothing, so it
        # is counted rather than either ignored or treated as fatal.
        folds = {}
        for n, native in sorted(table.items()):
            if native < 0:
                continue
            folds.setdefault(native, []).append(n)
        merged = [v for v in folds.values() if len(v) > 1]
        if merged:
            print("     COST: %d of the 16 fold -- %s"
                  % (sum(len(v) for v in merged) - len(merged),
                     "; ".join("=".join(x.replace("EGA_", "") for x in v)
                               for v in merged)))
        else:
            print("     COST: none -- all sixteen stay distinct")
    if bad:
        print("\ncheck_colours: %d information-bearing colour(s) lost" % bad)
        return 1
    print("\ncheck_colours: %d of %d candidate machine(s) carry every "
          "information-bearing colour" % (len(CANDIDATES), len(CANDIDATES)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
