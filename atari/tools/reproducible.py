#!/usr/bin/env python3
"""Link the game twice and say whether the two agree.

THE ATARI LINK IS NOT REPRODUCIBLE and this turns that from an invisible
property into a reported one. Measured 2026-09-10 across twelve identical
links: two distinct binaries, chosen at RANDOM roughly half and half, differing
by exactly 20 bytes in exactly one function -- `ui_draw_position`, 243 or 263
bytes. Everything else, symbol for symbol, is identical.

The other four ports are reproducible: four clean links of the C128 give one
md5, and the X16, MEGA65 and Amiga matched the v0.12.1 tag when it was cut.
This is Atari-only.

WHAT IT IS NOT. Not the data pipeline (this links nothing but sources). Not
`--threads=1`, not `--lto-partitions=1`, not the machine outliner -- all tried,
none changed it. Not the optimisation level: -Os does not fit, so -Oz is
mandatory, and -fno-lto does not fit either, so LTO cannot be dropped. What is
left is non-determinism inside LTO codegen, which on LLVM usually means a pass
iterating a pointer-keyed container -- decided per process, which is exactly
the random half-and-half seen here.

WHY IT MATTERS ENOUGH TO CHECK. A binary you cannot reproduce is a bug report
you cannot pin. It cost an hour on 2026-09-10: a resident figure moved by 20
bytes between builds, was read as a stale link, and a correct finding was
retracted on the strength of it.

    make reproducible
"""
import hashlib
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def sizes(mapfile):
    out = {}
    for line in open(mapfile):
        f = line.split()
        if len(f) == 5 and ":(.text." in f[4]:
            out[f[4].split(":(.text.")[1].rstrip(")")] = int(f[2], 16)
    return out


def link(tag):
    xex = "/tmp/repro-%s.xex" % tag
    mp = "/tmp/repro-%s.map" % tag
    r = subprocess.run(["make", "-C", HERE, "--no-print-directory",
                        "GAME_OUT=" + xex, "-B", "game"],
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit("reproducible: the link failed:\n" + r.stdout[-800:] + r.stderr[-800:])
    if os.path.exists(os.path.join(HERE, "build", "trekatari.map")):
        subprocess.run(["cp", os.path.join(HERE, "build", "trekatari.map"), mp])
    return hashlib.md5(open(xex, "rb").read()).hexdigest(), mp


def main():
    a, ma = link("a")
    b, mb = link("b")
    print("reproducible: link A %s" % a)
    print("reproducible: link B %s" % b)
    if a == b:
        print("reproducible: the two links agree -- reproducible THIS TIME.")
        print("              (it is random, roughly half and half; agreeing "
              "once is not proof)")
        return 0
    print("reproducible: THE TWO LINKS DISAGREE -- open list item 19.")
    try:
        sa, sb = sizes(ma), sizes(mb)
        diff = [(n, sa[n], sb.get(n)) for n in sa if sb.get(n) != sa[n]]
        for n, x, y in diff:
            print("              %-24s %d vs %d (%+d)" % (n, x, y, (y or 0) - x))
        print("              %d function(s), %+d bytes total"
              % (len(diff), sum((y or 0) - x for _, x, y in diff)))
    except Exception as e:
        print("              (could not diff the maps: %s)" % e)
    # NOT a build failure. Both binaries are valid and pass every check; this
    # reports a known property rather than breaking the build over it.
    return 0


sys.exit(main())
