#!/usr/bin/env python3
"""The 40-column panel geometry, checked without an emulator.

THE SAME ARITHMETIC verify_prg.py's check_junctions() does for the 80-column
table, made page-aware. A hand-written junction table is cheaper on the
machine than asking every panel what strokes it puts through a cell -- that
general form was measured at 209 bytes of resident code and the X16 had 150
bytes left -- so the table wins there and this pays its cost here, where bytes
are free.

PAGE-AWARE IS THE WHOLE DIFFERENCE. The chart occupies rows 0..10 across the
full width of PAGE TWO, which is exactly where SCAN and STATUS sit on PAGE
ONE. Considered together they overlap completely; considered per page neither
touches the other. A checker that forgot the split would report the layout as
broken, which is the kind of false red that teaches people to ignore a check.

It also proves the thing the whole 40-column argument rests on: that every
panel fits inside 40x25.
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
C128 = os.path.dirname(HERE)
SRC = os.path.join(C128, "src", "layout40.c")

COLS, ROWS = 40, 25
UP, DOWN, LEFT, RIGHT = 1, 2, 4, 8
GLYPH = {UP | DOWN: "G_VLINE", LEFT | RIGHT: "G_HLINE",
         DOWN | RIGHT: "G_TL", DOWN | LEFT: "G_TR",
         UP | RIGHT: "G_BL", UP | LEFT: "G_BR",
         UP | DOWN | RIGHT: "G_TEE_L", UP | DOWN | LEFT: "G_TEE_R",
         UP | LEFT | RIGHT: "G_TEE_U", DOWN | LEFT | RIGHT: "G_TEE_D",
         15: "G_CROSS"}
PLAIN = {"G_TL", "G_TR", "G_BL", "G_BR"}


def die(msg):
    print("layout40_check: " + msg)
    sys.exit(1)


def parse():
    src = open(SRC).read()
    m = re.search(r"const Panel panels\[PANEL_COUNT\] = \{(.*?)\n\};", src, re.S)
    if not m:
        die("cannot find the panel table in layout40.c")
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    rects = []
    for grp in re.findall(r"\{([^}]*)\}", body):
        f = [t.strip() for t in grp.split(",")]
        rects.append((int(f[0]), int(f[1]), int(f[2]), int(f[3])))

    m = re.search(r"const unsigned char panel40_page\[PANEL_COUNT\] = \{(.*?)\n\};",
                  src, re.S)
    if not m:
        die("cannot find panel40_page in layout40.c")
    pages = [0 if "TACTICAL" in t else 1
             for t in re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S).split(",")
             if t.strip()]

    if len(rects) != len(pages):
        die("%d panels but %d page tags" % (len(rects), len(pages)))
    if len(rects) < 8:
        die("parsed %d panels -- the table shape changed" % len(rects))

    m = re.search(r"static const unsigned char junctions_tactical\[\] = \{(.*?)\n\};",
                  src, re.S)
    if not m:
        die("cannot find junctions_tactical in layout40.c")
    toks = [t.strip() for t in
            re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S).replace("\n", " ").split(",")
            if t.strip()]
    if len(toks) % 3:
        die("the junction table is not whole x,y,glyph triples")
    have = {(int(toks[i]), int(toks[i + 1])): toks[i + 2]
            for i in range(0, len(toks), 3)}
    return rects, pages, have


def strokes(rects, x, y):
    mask = 0
    for px, py, w, h in rects:
        rx, by = px + w - 1, py + h - 1
        if not (px <= x <= rx and py <= y <= by):
            continue
        if y == py or y == by:
            if x < rx: mask |= RIGHT
            if x > px: mask |= LEFT
        if x == px or x == rx:
            if y < by: mask |= DOWN
            if y > py: mask |= UP
    return mask


def main():
    rects, pages, have = parse()

    # EVERY PANEL INSIDE 40x25. This is the claim the whole 40-column argument
    # rests on, so it is checked rather than asserted in a comment.
    bad = 0
    for i, (x, y, w, h) in enumerate(rects):
        if x + w > COLS or y + h > ROWS:
            print("  panel %d at %d,%d %dx%d runs past %dx%d"
                  % (i, x, y, w, h, COLS, ROWS))
            bad += 1
    if bad:
        die("%d panel(s) do not fit" % bad)
    print("  all %d panels fit inside %dx%d" % (len(rects), COLS, ROWS))

    for page, name in ((0, "tactical"), (1, "chart")):
        on = [r for r, p in zip(rects, pages) if p == page]
        want = {}
        for px, py, w, h in on:
            rx, by = px + w - 1, py + h - 1
            for x, y in ((px, py), (rx, py), (px, by), (rx, by)):
                g = GLYPH.get(strokes(on, x, y))
                if g is None:
                    die("no glyph for the strokes meeting at %d,%d on the %s "
                        "page" % (x, y, name))
                if want.get((x, y), g) != g:
                    die("panels disagree about the corner at %d,%d on the %s "
                        "page" % (x, y, name))
                if g not in PLAIN:
                    want[(x, y)] = g

        if page == 0:
            if want != have:
                for k in sorted(set(want) | set(have)):
                    if want.get(k) != have.get(k):
                        print("  %s at %s: geometry says %s, table says %s"
                              % (name, k, want.get(k, "nothing"),
                                 have.get(k, "nothing")))
                die("the %s junction table disagrees with the geometry" % name)
            print("  %s page: %d junctions, all recomputed from the geometry"
                  % (name, len(have)))
        else:
            if want:
                die("the %s page wants %d junction(s) and the source declares "
                    "none" % (name, len(want)))
            print("  %s page: no shared borders, so no junctions -- correct, "
                  "the chart ends at row 10 and the messages begin at 11" % name)

    print("layout40_check: the 40-column geometry is consistent")
    return 0


if __name__ == "__main__":
    sys.exit(main())
