#!/usr/bin/env python3
"""What the compiler, the linker and the test suite structurally cannot see.

THIS IS THE FALCON'S CHECKER, POINTED AT THIS PORT -- imported, not copied.
falcon/tools/verify_prg.py already sweeps every glyph the shared UI can put on
screen and checks the console's geometry against the driver's own constants,
and both checks are about the SEAM rather than about the Falcon: a screen code
with no `box[]` entry draws the missing-glyph marker on any port that has to
supply its own font, which is this one.

WHAT IS OVERRIDDEN, and it is only paths and numbers:

    SHARED        layout40.c instead of layout.c -- this is a 40-column build,
                  and the junction tables it must cover are different ones
    driver        src/stvid.c instead of src/falconvid.c
    the geometry  40x25 of 8x8 in 320x200, out of stvid.c's own #defines,
                  because vdc.h's VDC_COLS is 80 and means the OTHER driver

**IMPORTING RATHER THAN COPYING IS THE POINT.** A copy would pass for months
after the original grew a check, and this project has spent two sweeps
deleting duplicated facts that drifted. If the Falcon's sweep gains a rule,
this port gets it on the next build or fails loudly trying.
"""
import importlib.util
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(HERE)
FALCON = os.path.join(ROOT, "falcon", "tools", "verify_prg.py")


def load_falcon_checker():
    if not os.path.exists(FALCON):
        sys.exit("verify_st: %s is missing -- this port's gate IS that file, "
                 "so a missing Falcon checker is a failure here and not a "
                 "skip" % FALCON)
    spec = importlib.util.spec_from_file_location("falcon_verify", FALCON)
    mod = importlib.util.module_from_spec(spec)
    # IF IT EXITS DURING IMPORT, SAY SO RATHER THAN DYING QUIETLY. The first
    # version of this file had no such catch and the Falcon's had a bare
    # `sys.exit(main())` at its foot, so importing it ran the FALCON'S checks
    # against the FALCON'S driver and killed the process on the way out. This
    # gate printed "console 80x25 cells of 8x16 = 640x400" and exited 0 --
    # green, and about the other machine. A check that cannot run is worse
    # than a check that fails.
    try:
        spec.loader.exec_module(mod)
    except SystemExit:
        sys.exit("verify_st: importing %s EXITED. It must be guarded with "
                 "`if __name__ == \"__main__\":` or this gate runs the "
                 "Falcon's checks instead of the ST's." % FALCON)
    return mod


def main():
    vp = load_falcon_checker()

    # Point it at this port. vp.ROOT is already the repository root.
    vp.HERE = HERE
    vp.SHARED = [os.path.join(ROOT, "c128", "src", f)
                 for f in ("ui.c", "layout40.c", "main.c")]

    src_path = os.path.join(HERE, "src", "stvid.c")
    src = open(src_path).read()

    # The driver's own facts, the same way vp.driver_facts() reads the
    # Falcon's -- an assertion written in terms of the constant it tests
    # cannot fail, so both come out of the file.
    f = {}
    for name in ("SCR_W", "SCR_H", "CELL_W", "CELL_H", "STRIDE"):
        m = re.search(r'^#define\s+%s\s+(.+?)\s*(?:/\*|$)' % name, src, re.M)
        if not m:
            print("verify_st: stvid.c has no #define %s" % name)
            return 1
        f[name] = m.group(1).strip()
    box_codes = set(int(m) for m in re.findall(r'^\s*\{\s*(\d+),\s*\{', src, re.M))
    if not box_codes:
        print("verify_st: read ZERO box[] entries out of stvid.c. An empty "
              "set makes every glyph look missing OR every glyph look "
              "present depending on the rule -- either way the parse is the "
              "bug, not the driver.")
        return 1

    consts = vp.constants()
    # vdc.h says 80; this driver says 40, and the driver is the authority for
    # its own screen. Read from stvid.c rather than hardcoded here.
    for name, key in (("ST_COLS", "VDC_COLS"), ("ST_ROWS", "VDC_ROWS")):
        m = re.search(r'^#define\s+%s\s+(\d+)' % name, src, re.M)
        if not m:
            print("verify_st: stvid.c has no #define %s" % name)
            return 1
        consts[key] = int(m.group(1))

    vp.fail = []
    if consts["VDC_COLS"] != 40 or int(f["CELL_H"]) != 8:
        print("verify_st: geometry came out %sx%s of %sx%s -- that is not this "
              "machine, so the override did not take"
              % (consts["VDC_COLS"], consts["VDC_ROWS"], f["CELL_W"], f["CELL_H"]))
        return 1
    vp.check_geometry(f, consts)
    vp.check_glyphs(box_codes, consts)

    # Size, and the machine it has to fit. A 520ST leaves about 450K free
    # under TOS; this is printed rather than gated because the port is nowhere
    # near it -- but a resource nobody reports is a resource nobody manages.
    prg = os.path.join(HERE, "build", "EGATREK.PRG")
    if os.path.exists(prg):
        d = open(prg, "rb").read()
        magic, text, data, bss = struct.unpack(">HLLL", d[:14])
        if magic != 0x601A:
            vp.fail.append("PRG magic is $%04X, not $601A -- TOS will not "
                           "run this" % magic)
        else:
            total = text + data + bss
            print("verify: PRG %d bytes on disk; text %d + data %d + bss %d "
                  "= %d in RAM, of ~450K free on a 512K ST"
                  % (len(d), text, data, bss, total))
    else:
        vp.fail.append("no build/EGATREK.PRG -- run `make game` first")

    if vp.fail:
        for m in vp.fail:
            print("verify_st: " + m)
        return 1
    print("verify_st: the console fits, every glyph draws, and TOS will load it")
    return 0


if __name__ == "__main__":
    sys.exit(main())
