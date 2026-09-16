#!/usr/bin/env python3
"""Every glyph the shared UI draws must have a NAME, so a port can find it.

THE DEFECT THIS EXISTS FOR. A port whose font is not the C128's translates
layout.h's G_* set into whatever its machine has. Two codes -- the gauge bar
and the enemy silhouette's saucer -- were spelled as bare numbers at their
call sites in ui.c instead, so no port had a reason to notice them. The
card-less CoCo 3 drew SYSTEMS STATUS, both laser gauges, the badge and every
enemy silhouette as question marks, and it took Jamie playing the game to find
out: the console bench draws the FRAME, and the frame was fine.

So this is not a check that a port renders a glyph -- it cannot be, since
every port renders differently. It checks the thing that actually failed:
that the shared code names what it draws, so the set a port must translate is
readable off one header.

Anything 64 and above is a graphics code. Below that is ASCII/PETSCII that
every one of these machines has, and a literal is clearer than a name.
"""
import re, sys

SHARED  = ("c128/src/ui.c", "c128/src/layout.c", "c128/src/main.c")
HEADER  = "c128/src/layout.h"
DRAWERS = ("scr_put", "scr_hline", "scr_vline")
FLOOR   = 64


def named():
    """The G_* set, from the header that ports translate."""
    out = {}
    for line in open(HEADER):
        m = re.match(r"#define\s+(G_\w+)\s+(\d+)", line.strip())
        if m:
            out[int(m.group(2))] = m.group(1)
    return out


def main():
    names = named()
    if not names:
        print("check_glyphs: no G_* constants in %s -- the header moved?" % HEADER)
        return 1

    bad = []
    for path in SHARED:
        for n, line in enumerate(open(path), 1):
            for fn in DRAWERS:
                # The glyph is the third argument: scr_put(x, y, GLYPH, colour).
                # Matched loosely on purpose -- a miss here is a false PASS,
                # so the pattern errs towards catching too much and the
                # numbers below FLOOR are filtered out afterwards.
                for m in re.finditer(fn + r"\s*\(", line):
                    rest = line[m.end():]
                    for lit in re.findall(r"(?<![\w.])(\d+)(?![\w.])", rest):
                        v = int(lit)
                        if v >= FLOOR:
                            bad.append((path, n, v, names.get(v), line.strip()))

    for path, n, v, name, text in bad:
        hint = ("it is %s -- use the name" % name) if name else \
               "no G_* constant has this value; add one to layout.h"
        print("check_glyphs: %s:%d draws bare glyph %d -- %s" % (path, n, v, hint))
        print("    %s" % text[:100])

    if bad:
        print("check_glyphs: %d bare glyph code(s). A port translating layout.h "
              "cannot see these." % len(bad))
        return 1
    print("check_glyphs: %d named glyphs, no bare codes in %d shared file(s)"
          % (len(names), len(SHARED)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
