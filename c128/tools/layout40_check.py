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
import os, re, subprocess, sys

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

    # ONE TABLE PER PAGE. This read only junctions_tactical until 2026-09-14,
    # and the moment the badge and SYSTEMS STATUS moved to the chart page it
    # reported "the chart page wants 2 junctions and the source declares none"
    # -- against a source that declared them. A checker that knows about one
    # of two tables is a checker that fails correct code, which is the kind of
    # false red that teaches people to stop reading it.
    have = {}
    for page, name in ((0, "tactical"), (1, "chart")):
        m = re.search(r"static const unsigned char junctions_%s\[\] = \{(.*?)\n\};"
                      % name, src, re.S)
        if not m:
            have[page] = {}
            continue
        toks = [t.strip() for t in
                re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S).replace("\n", " ").split(",")
                if t.strip()]
        if len(toks) % 3:
            die("the %s junction table is not whole x,y,glyph triples" % name)
        have[page] = {(int(toks[i]), int(toks[i + 1])): toks[i + 2]
                      for i in range(0, len(toks), 3)}
    if not have[0]:
        die("cannot find junctions_tactical in layout40.c")
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

        got = have[page]
        if want != got:
            for k in sorted(set(want) | set(got)):
                if want.get(k) != got.get(k):
                    print("  %s at %s: geometry says %s, table says %s"
                          % (name, k, want.get(k, "nothing"), got.get(k, "nothing")))
            die("the %s junction table disagrees with the geometry" % name)
        print("  %-8s page: %d junction(s), recomputed from the geometry"
              % (name, len(got)))

    check_text_width()

    print("layout40_check: the 40-column geometry is consistent")
    return 0


CC   = os.path.expanduser("~/llvm-mos/bin/mos-c128-clang")
CORE = os.path.join(os.path.dirname(C128), "core")
POOL = os.path.join(C128, "src", "strings.txt")


def pool_texts():
    """id -> text, numbered exactly as tools/gen_strings.py numbers them."""
    out, n = {}, 0
    for line in open(POOL, encoding="utf-8").read().splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        out[n] = line.split("\t", 1)[1] if "\t" in line else line
        n += 1
    return out


def check_text_width():
    """No scr_puts may start far enough right that its own text runs off.

    THIS IS NOT THE SAME CHECK AS THE PANELS ABOVE, and the difference is what
    it caught. The geometry check proves every BOX fits in 40x25. It says
    nothing about a string, and the three defects this found were all strings:
    the farewell screen in main.c drew "MISSION ENDE" over "BASIC IS ON THE 4",
    and ui_setup_briefing drew a ship's plate reading "U.S.S. LE" over "RCB-",
    because x=28, x=23, x=31 and x=36 are eighty-column positions that were
    never moved.

    NONE OF THE THREE WAS IN THE SCREEN BENCH. tools/screens40.py cycles
    fourteen screens; the farewell is drawn on the way out of main() and the
    briefing question sits in the gap between two screens the bench DOES have.
    A bench reaches the screens somebody listed. This reaches every call site.

    THE C PREPROCESSOR DOES THE #ifdefs, because deciding by regex which side
    of a TREK_40COL branch a line is on is how a checker ends up confidently
    wrong. Only literal coordinates and literal or pooled text can be checked
    -- a computed x is invisible here and is the checker's honest limit.
    """
    texts = pool_texts()
    bad = []
    for name in ("ui.c", "main.c"):
        path = os.path.join(C128, "src", name)
        cpp = subprocess.run(
            [CC, "-E", "-P", "-DTREK_40COL", "-DTREK_OVERLAYS",
             "-I", os.path.join(C128, "src"), "-I", CORE, path],
            capture_output=True, text=True)
        if cpp.returncode != 0:
            die("could not preprocess %s:\n%s" % (name, cpp.stderr[-600:]))
        src = cpp.stdout

        # S_nnn has already expanded to a bare number by this point.
        for m in re.finditer(
                r"\bscr_puts\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*S\s*\(\s*(\d+)\s*\)", src):
            x, y, sid = (int(g) for g in m.groups())
            t = texts.get(sid)
            if t is not None and x + len(t) > COLS:
                bad.append((name, x, y, "S_%d" % sid, t))
        for m in re.finditer(
                r'\bscr_puts\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*"((?:[^"\\]|\\.)*)"', src):
            x, y, t = int(m.group(1)), int(m.group(2)), m.group(3)
            if x + len(t) > COLS:
                bad.append((name, x, y, "literal", t))

        # A row is off the screen entirely, whatever the text is.
        for m in re.finditer(
                r"\bscr_(?:puts|put)\s*\(\s*(\d+)\s*,\s*(\d+)", src):
            x, y = int(m.group(1)), int(m.group(2))
            if x >= COLS or y >= ROWS:
                bad.append((name, x, y, "off-screen", ""))

    for name, x, y, what, t in sorted(set(bad)):
        print("  %s: x=%d y=%d %s runs to column %d -- %r"
              % (name, x, y, what, x + len(t), t[:44]))
    if bad:
        die("%d drawing call(s) do not fit in %d columns" % (len(bad), COLS))
    print("  text     width: every literal scr_puts fits in %d columns" % COLS)


if __name__ == "__main__":
    sys.exit(main())
