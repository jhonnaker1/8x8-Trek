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
# WHICH ARGUMENT IS THE GLYPH, and it is not the same one for all three.
# scr_put(x, y, GLYPH, colour) but scr_hline(x, y, LEN, GLYPH, colour) -- the
# first version of this checker looked at argument three for all of them and
# reported the LENGTH of every panel rule as an unnamed glyph.
DRAWERS = {"scr_put": 2, "scr_hline": 3, "scr_vline": 3}
FLOOR   = 64


def named():
    """The G_* set, from the header that ports translate."""
    out = {}
    for line in open(HEADER):
        m = re.match(r"#define\s+(G_\w+)\s+(\d+)", line.strip())
        if m:
            out[int(m.group(2))] = m.group(1)
    return out


def local_defines(text):
    """A file's own #define NAME VALUE map, one level."""
    out = {}
    for m in re.finditer(r"^#define\s+(\w+)\s+([^\n/]+)", text, re.M):
        out[m.group(1)] = m.group(2).strip()
    return out


def nth_arg(rest, want):
    """Argument `want` (0-based) of a call, paren-aware.

    The first two arguments are nearly always casts, so splitting on commas
    without counting brackets picks the wrong one -- which is how the first
    version of this tool only ever looked at bare integers anywhere in the
    line and missed two named constants holding raw glyph codes."""
    depth, args, cur = 0, [], ""
    for c in rest:
        if c in "([":
            depth += 1
        elif c in ")]":
            if depth == 0:
                args.append(cur)
                break
            depth -= 1
        if c == "," and depth == 0:
            args.append(cur)
            cur = ""
            continue
        cur += c
    return args[want].strip() if len(args) > want else None


def judge(arg, names, local):
    """Nothing, or why this glyph argument is unreadable to a port."""
    if re.fullmatch(r"\d+", arg):
        v = int(arg)
        if v < FLOOR:
            return None                      # plain ASCII; every machine has it
        return ("bare glyph %d -- it is %s, use the name" % (v, names[v])
                if v in names else
                "bare glyph %d -- no G_* constant has this value" % v)
    if re.fullmatch(r"G_\w+", arg):
        return None
    if re.fullmatch(r"\w+", arg):
        # A LOCAL NAME IS AS INVISIBLE AS A NUMBER. A port translating this
        # console reads layout.h; a #define in ui.c is somewhere it has no
        # reason to look. Resolve one level and insist it lands on a G_*.
        seen, cur = set(), arg
        while cur in local and cur not in seen:
            seen.add(cur)
            cur = local[cur]
        if re.fullmatch(r"G_\w+", cur):
            return None
        if re.fullmatch(r"\d+", cur) and int(cur) < FLOOR:
            return None
        if cur == arg:
            # NOT A CONSTANT AT ALL -- a local variable holding whatever the
            # caller chose. A port's driver handles those generically, and
            # there is nothing here for a name to fix.
            return None
        return ("`%s` resolves to %s, not a G_* constant -- a port reads "
                "layout.h, not this file" % (arg, cur))
    return None                              # an expression or a variable


def main():
    names = named()
    if not names:
        print("check_glyphs: no G_* constants in %s -- the header moved?" % HEADER)
        return 1

    bad = []
    for path in SHARED:
        text = open(path).read()
        local = local_defines(text)
        for n, line in enumerate(text.split("\n"), 1):
            for fn in DRAWERS:
                for m in re.finditer(fn + r"\s*\(", line):
                    arg = nth_arg(line[m.end():], DRAWERS[fn])
                    if arg is None:
                        continue
                    why = judge(arg, names, local)
                    if why:
                        bad.append((path, n, arg, why, line.strip()))

    for path, n, arg, why, text in bad:
        print("check_glyphs: %s:%d draws `%s` -- %s" % (path, n, arg, why))
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
