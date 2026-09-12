#!/usr/bin/env python3
"""What the compiler, the linker and the test suite structurally cannot see.

This port has none of the pools the 6502 ports run out of -- no overlay window,
no staging regions, no banked far memory, no soft-stack reserve -- so the
checks those ports need have nothing to bite on here. What it DOES have that
they do not is a screen it has to fit and a font it has to supply, and both of
those fail silently:

  * GEOMETRY. The console is 80x25 cells of 8x16 inside 640x480. Change the
    cell height, the row count or the margin and nothing complains -- the
    bottom rows just leave the screen, which on a 6502 port would be a linker
    error and here is a quiet crop.

  * GLYPH COVERAGE, and this is the check worth having. A screen code with no
    box[] entry and no ASCII mapping draws the missing-glyph marker, which is
    only loud if somebody is looking at that panel. The Amiga found its set by
    SWEEPING every scr_put/hline/vline/fill_rect argument in the shared UI by
    hand, and that sweep turned up FIFTEEN where an eyeball count gives eleven
    -- the two extra live in panels nothing had drawn yet. A hand sweep is
    exactly the thing that goes stale the next time someone adds a panel. This
    does it on every build.

It also prints the size, because a resource nobody reports is a resource
nobody manages -- even one this port has no shortage of. The number is here so
that the day it starts mattering, it is already on screen rather than being
re-derived.
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(HERE)
SHARED = [os.path.join(ROOT, "c128", "src", f)
          for f in ("ui.c", "layout.c", "main.c")]

fail = []


def die(msg):
    fail.append(msg)


# ---------------------------------------------------------------- constants

def defines(path):
    """Every `#define NAME <int>` in a file, as a dict. Deliberately ignores
       macros with arguments (SC_LETTER) -- those are call sites, not codes."""
    out = {}
    for m in re.finditer(r'^#define\s+([A-Za-z_]\w*)\s+\(?(\d+)\)?\s*(?:/\*|//|$)',
                         open(path).read(), re.M):
        out[m.group(1)] = int(m.group(2))
    return out


def constants():
    c = {}
    for p in ("c128/src/layout.h", "c128/src/vdc.h", "c128/src/ui.c",
              "core/ega.h"):
        full = os.path.join(ROOT, p)
        if os.path.exists(full):
            c.update(defines(full))
    return c


# ------------------------------------------------------------- the driver

def driver_facts():
    """The cell size, screen size and box[] codes, read out of the driver
       itself rather than restated here. An assertion written in terms of the
       constant it tests cannot fail."""
    src = open(os.path.join(HERE, "src", "falconvid.c")).read()
    f = {}
    for name in ("SCR_W", "SCR_H", "CELL_W", "CELL_H", "STRIDE"):
        m = re.search(r'^#define\s+%s\s+(.+?)\s*(?:/\*|$)' % name, src, re.M)
        if not m:
            die("falconvid.c has no #define %s" % name)
            return None, None
        f[name] = m.group(1).strip()
    codes = set(int(m) for m in re.findall(r'^\s*\{\s*(\d+),\s*\{', src, re.M))
    return f, codes


def check_geometry(f, consts):
    cols, rows = consts.get("VDC_COLS"), consts.get("VDC_ROWS")
    if cols is None or rows is None:
        die("vdc.h has no VDC_COLS/VDC_ROWS")
        return
    scr_w, scr_h = int(f["SCR_W"]), int(f["SCR_H"])
    cw, ch = int(f["CELL_W"]), int(f["CELL_H"])

    if cols * cw != scr_w:
        die("%d columns of %dpx is %dpx, but the screen is %d"
            % (cols, cw, cols * cw, scr_w))
    con_h = rows * ch
    if con_h > scr_h:
        die("%d rows of %dpx is %dpx, which does not fit in %d -- the bottom "
            "%d pixels would be off screen" % (rows, ch, con_h, scr_h,
                                               con_h - scr_h))
    # STRIDE is written as an expression; check the value it must come to.
    want = scr_w // 2
    if str(want) not in f["STRIDE"] and eval_stride(f) != want:
        die("STRIDE is %r but 4 planes at %d pixels is %d bytes"
            % (f["STRIDE"], scr_w, want))
    print("verify: console %dx%d cells of %dx%d = %dx%d in %dx%d, "
          "margin %d top and bottom"
          % (cols, rows, cw, ch, cols * cw, con_h, scr_w, scr_h,
             (scr_h - con_h) // 2))


def eval_stride(f):
    try:
        return eval(f["STRIDE"], {}, {"SCR_W": int(f["SCR_W"])})
    except Exception:
        return None


# --------------------------------------------------------- the glyph sweep

CALLS = {"scr_put": 2, "scr_fill_rect": 4, "scr_hline": 3, "scr_vline": 3}


def args_at(src, i):
    """Split one call's arguments at the top level, balancing parens and
       brackets. A regex cannot do this: the real call sites nest casts three
       deep and run across lines."""
    depth, start, out = 0, i, []
    while i < len(src):
        c = src[i]
        if c in "([":
            depth += 1
            if depth == 1:
                start = i + 1
        elif c in ")]":
            depth -= 1
            if depth == 0:
                out.append(src[start:i])
                return out, i
        elif c == "," and depth == 1:
            out.append(src[start:i])
            start = i + 1
        i += 1
    return None, i


def sweep():
    """Every glyph the shared UI can put on screen, with where it came from.

    TWO SOURCES, AND THE SECOND IS THE ONE THAT MATTERS. Call arguments are
    the obvious half. The other half is glyph constants used in DATA TABLES --
    `junctions[]` in layout.c holds G_CROSS and four G_TEE_*, and every one of
    them reaches the screen through `scr_put(junctions[i], ..., junctions[i+2],
    ...)`, an argument that is an array subscript and resolves to nothing.

    The first version of this swept call arguments only. It passed, and it
    went on passing when G_CROSS was deleted from the driver's box[] -- a
    check that could not fail, reporting coverage it did not have. The box
    junctions are exactly the glyphs a port is most likely to get wrong and
    they are exactly the ones that live in tables.
    """
    used = {}
    for path in SHARED:
        src = open(path).read()
        base = os.path.basename(path)

        for name, argn in CALLS.items():
            for m in re.finditer(r'\b%s\s*\(' % name, src):
                args, end = args_at(src, m.end() - 1)
                if not args or len(args) <= argn:
                    continue
                a = " ".join(args[argn].split())
                line = src.count("\n", 0, m.start()) + 1
                used.setdefault(a, []).append("%s:%d" % (base, line))

        # Glyph constants ANYWHERE in the file, tables included.
        for m in re.finditer(r'\b(G_[A-Z0-9_]+|SC_[A-Z0-9_]+)\b', src):
            name = m.group(1)
            if name.endswith("("):
                continue
            line = src.count("\n", 0, m.start()) + 1
            used.setdefault(name, []).append("%s:%d" % (base, line))
    return used


def resolve(expr, consts):
    """A glyph argument to a screen code, or None if it is decided at run
       time. Returning None is not a failure -- the runtime marker covers
       those, and pretending to know is worse than saying we do not."""
    e = expr.strip()
    if re.fullmatch(r'\d+', e):
        return int(e)
    if e in consts:
        return consts[e]
    m = re.fullmatch(r"'(.)'", e)
    if m:
        return ord(m.group(1))
    return None


def drawable(code, box_codes):
    """Mirrors falconvid.c's glyph_rows: box[] first, then screen code to
       ASCII. Kept in step by reading box[] out of the driver, not by
       restating it."""
    base = code & 0x7F
    if base in box_codes:
        return True
    if base == 0 or base <= 26 or base in (27, 29) or 32 <= base <= 63:
        return True
    return False


def check_glyphs(box_codes, consts):
    used = sweep()
    if not used:
        die("the glyph sweep found NO call sites -- the parser is broken, "
            "which is worse than a missing glyph because it reports success")
    resolved, runtime, missing = 0, 0, []
    for expr, sites in sorted(used.items()):
        code = resolve(expr, consts)
        if code is None:
            runtime += 1
            continue
        resolved += 1
        if not drawable(code, box_codes):
            missing.append((expr, code, sites[0]))
    for expr, code, site in missing:
        die("no glyph for screen code %d (%s) drawn at %s -- it would render "
            "as the missing-glyph marker" % (code, expr, site))
    print("verify: glyphs %d call sites, %d codes resolved, %d decided at "
          "run time, %d box entries" % (sum(len(v) for v in used.values()),
                                        resolved, runtime, len(box_codes)))


# ------------------------------------------------------------------- size

def check_size():
    prg = os.path.join(HERE, "build", "EGATREK.PRG")
    if not os.path.exists(prg):
        die("build/EGATREK.PRG is missing -- run make first")
        return
    h = open(prg, "rb").read(28)
    magic, text, data, bss = struct.unpack(">HIII", h[:14])
    if magic != 0x601A:
        die("build/EGATREK.PRG is not a TOS executable (magic %#06x)" % magic)
        return
    print("verify: resident %s bytes (text %s + data %s + bss %s), "
          "free is the machine's -- 1MB at worst"
          % ("{:,}".format(text + data + bss), "{:,}".format(text),
             "{:,}".format(data), "{:,}".format(bss)))


def main():
    consts = constants()
    f, box_codes = driver_facts()
    if f:
        check_geometry(f, consts)
        check_glyphs(box_codes, consts)
    check_size()
    if fail:
        print()
        for m in fail:
            print("verify: FAIL -- " + m)
        return 1
    return 0


sys.exit(main())
