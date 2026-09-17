#!/usr/bin/env python3
"""Check that the briefing actually drew a page.

IT EXISTS BECAUSE NOTHING ELSE ANSWERED THAT PROMPT. Fifteen gates in this
directory passed on a disk with no BRIEF.TXT on it, because ui_briefing()
does exactly what its comment says -- "NO FILE, NO BRIEFING. A disk without
BRIEF.TXT skips it silently and the game starts" -- so the failure had no
symptom a gate could see. Jamie found it in three minutes of playing.

A briefing page is many rows of text. The setup screen it would fall back to
is two. Counting rows is enough to tell those apart and needs no OCR.
"""
import sys

sys.path.insert(0, __file__.rsplit("/", 1)[0])
from count_colours import png_pixels                      # noqa: E402

MIN_TEXT_ROWS = 8


def main(argv):
    if len(argv) < 2:
        raise SystemExit("usage: check_briefing.py SNAPSHOT.png")
    w, h, rows = png_pixels(argv[1])

    def px(x, y):
        return rows[y][x * 3:x * 3 + 3]

    border = px(0, 0)
    xs = [x for x in range(w) if px(x, h // 2) != border]
    ys = [y for y in range(h) if px(w // 2, y) != border]
    if not xs or not ys:
        print("FAIL: the screen is one flat colour")
        return 1
    x0, y0 = min(xs), min(ys)
    black = b"\x00\x00\x00"

    lit_rows, straddle = 0, []
    for r in range(25):
        lit = [sum(1 for x in range(640) if px(x0 + x, y0 + r * 8 + i) != black)
               for i in range(8)]
        if sum(lit) > 40:
            lit_rows += 1
        # A ROW MUST NOT BLEED INTO THE NEXT. The font is six pixels tall in
        # an eight-pixel cell, so scanline 7 is blank under every GLYPH -- and
        # a small screenshot of tight text LOOKS like overlapping lines, which
        # I have now misread three times. This is the check that settles it.
        #
        # BUT A SOLID CELL LIGHTS ALL EIGHT SCANLINES AND IS NOT BLEED. The
        # cursor is one, the badge is one, and the console is full of them --
        # so the first version of this check failed the setup screen for the
        # cursor and would have failed every console screen there is. What
        # counts as bleed is a column lit on scanline 7 that is NOT lit on all
        # of 0..6, which is exactly "part of a glyph, in the gap".
        if lit[7] > 8:
            bleed = 0
            for x in range(640):
                if px(x0 + x, y0 + r * 8 + 7) == black:
                    continue
                if any(px(x0 + x, y0 + r * 8 + i) == black for i in range(7)):
                    bleed += 1
            if bleed > 8:
                straddle.append((r, bleed))

    print(f"  {lit_rows} rows of text on the page")
    if straddle:
        print(f"FAIL: {straddle} -- (row, pixels) lit in the gap under a "
              f"glyph rather than under a solid cell; the rows really are "
              f"bleeding into each other")
        return 1
    if lit_rows < MIN_TEXT_ROWS:
        print(f"FAIL: {lit_rows} rows of text; a briefing page has at least "
              f"{MIN_TEXT_ROWS}. A disk with no BRIEF.TXT skips the briefing "
              f"SILENTLY and shows the setup screen instead")
        return 1
    print(f"check_briefing: a page drew, {lit_rows} rows, no row bleeding into "
          f"the next")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
