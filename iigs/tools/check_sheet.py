#!/usr/bin/env python3
"""Measure src/gssmoke.c's proof sheet instead of looking at it.

A console screenshot is the kind of picture that looks right at a glance and
is wrong in the one place that matters. Three things on this sheet cannot be
judged by eye and all three have bitten a port here:

  * DO THE RULES JOIN? A six-pixel glyph in an eight-pixel cell draws a rule
    with a two-pixel gap at every cell boundary. At this scale that reads as a
    slightly lighter line, not as a defect. It is why the box set is authored
    at eight wide rather than shifted like the text.
  * IS ANY GLYPH MISSING? A blank cell is invisible among 64 of them. On the
    Amiga two bracket glyphs were absent and the marker is what found them.
  * ARE THE COLOURS EGA'S, or merely sixteen different ones? The C128 maps EGA
    onto a fixed RGBI chip and loses BROWN to olive. This port writes EGA's own
    values into a programmable palette, and the claim "brown is brown" is worth
    exactly as much as the check behind it.

320-mode pixels are doubled horizontally in a MAME capture and 1:1 vertically,
which is measured from the content box rather than assumed.
"""
import sys

sys.path.insert(0, __file__.rsplit("/", 1)[0])
from count_colours import png_pixels           # noqa: E402

# EGA's sixteen, at the levels core/ega.h and mega65/src/m65vid.c both state.
EGA = ["000000", "0000aa", "00aa00", "00aaaa", "aa0000", "aa00aa", "aa5500",
       "aaaaaa", "555555", "5555ff", "55ff55", "55ffff", "ff5555", "ff55ff",
       "ffff55", "ffffff"]

# Codes that legitimately draw nothing: 32 is space, and 28 is deliberately
# absent from coco3/tools/gen_font.py's artwork.
BLANK_OK = {28, 32}


def main(argv):
    if len(argv) < 2:
        raise SystemExit("usage: check_sheet.py SNAPSHOT.png")
    path = argv[1]
    w, h, rows = png_pixels(path)

    def px(x, y):
        return rows[y][x * 3:x * 3 + 3]

    border = px(0, 0)
    xs = [x for x in range(w) if px(x, h // 2) != border]
    ys = [y for y in range(h) if px(w // 2, y) != border]
    if not xs or not ys:
        print(f"FAIL: {path} is one flat colour -- nothing was drawn")
        return 1
    x0, y0 = min(xs), min(ys)
    cw, ch = max(xs) - x0 + 1, max(ys) - y0 + 1
    if (cw, ch) != (640, 200):
        print(f"FAIL: content box is {cw}x{ch}, expected 640x200")
        return 1

    def cell(cx, cy, ix, iy):
        return px(x0 + (cx * 8 + ix) * 2, y0 + cy * 8 + iy)

    black = b"\x00\x00\x00"
    fails, notes = [], []

    # 1. The rule at row 11 runs across twenty cells with no gap.
    gaps = [(cx, ix) for cx in range(2, 22) for ix in range(8)
            if cell(cx, 11, ix, 3) == black]
    if gaps:
        fails.append(f"the horizontal rule has {len(gaps)} black pixels in "
                     f"160 -- first at cell {gaps[0][0]} pixel {gaps[0][1]}; "
                     f"the box glyphs are not reaching the cell edge")
    else:
        notes.append("horizontal rule continuous across 20 cells (160 px)")

    # 2. Every text screen code draws something.
    blank = []
    for i in range(16):
        for r, first in ((3, 0), (4, 16), (5, 32), (6, 48)):
            on = sum(1 for iy in range(8) for ix in range(8)
                     if cell(i + 2, r, ix, iy) != black)
            if on == 0:
                blank.append(first + i)
    unexpected = sorted(set(blank) - BLANK_OK)
    if unexpected:
        fails.append(f"screen codes draw nothing: {unexpected}")
    else:
        notes.append(f"64 text codes drawn, {sorted(set(blank))} blank as "
                     f"expected")

    # 3. The sixteen solid cells are EGA's sixteen, in order.
    got = [cell(i + 2, 18, 3, 3).hex() for i in range(16)]
    wrong = [(i, got[i], EGA[i]) for i in range(16) if got[i] != EGA[i]]
    if wrong:
        for i, g, e in wrong:
            fails.append(f"colour {i} is #{g}, EGA says #{e}")
    else:
        notes.append("sixteen colours, EGA's own values -- brown is #aa5500, "
                     "not olive")

    # 4. Row 24 really is drawn. A driver that clips at 24 rows and a console
    #    that uses row 24 is a bug nothing else here would show.
    on24 = sum(1 for cx in range(15) for ix in range(8) for iy in range(8)
               if cell(cx, 24, ix, iy) != black)
    if on24 == 0:
        fails.append("row 24 is blank -- the driver is clipping it")
    else:
        notes.append(f"row 24 drawn ({on24} lit pixels)")

    for n in notes:
        print(f"  {n}")
    if fails:
        for f in fails:
            print(f"FAIL: {f}")
        return 1
    print(f"check_sheet: {len(notes)} checks, 0 failures")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
