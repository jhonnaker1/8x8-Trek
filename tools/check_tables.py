#!/usr/bin/env python3
"""Check the port's fixed tables against the ORIGINAL BINARY, not against
reference/strings.txt.

This exists because of one bug. core/planet.c's eight planet names were taken
from strings.txt, which showed SEVEN -- `strings` had dropped Vega -- and the
port shipped a name table that could never produce a planet the original
produces. The same block of strings.txt joins two entries into "Life support
suppliesRaw energium" with no separator, which is the same failure in a form
you can see.

strings.txt is fine for finding a message. It is wrong for counting a LIST.
The binary stores these as fixed-stride, length-prefixed Pascal strings, and
that structure is what this reads.

SKIPPED with a note when reference/ is absent, which it is on any clone: the
shareware licence requires the package be distributed complete and unmodified,
so it is gitignored. See NOTES.md.
"""
import os
import re
import sys

EXE = "reference/EGATREK_unpacked.exe"


def table_at(data, first, count, stride=13):
    """Read `count` length-prefixed strings at `stride` bytes apart."""
    i = data.find(bytes([len(first)]) + first.encode())
    if i < 0:
        return None
    out = []
    for k in range(count):
        rec = data[i + stride * k:i + stride * (k + 1)]
        out.append(rec[1:1 + rec[0]].decode("latin-1"))
    return out


def c_array(path, name):
    """The string literals of a `const char *const NAME[] = {...}` array."""
    src = open(path).read()
    m = re.search(re.escape(name) + r"\s*\[[^\]]*\]\s*=\s*\{(.*?)\}", src, re.S)
    if not m:
        return None
    return re.findall(r'"([^"]*)"', m.group(1))


def main():
    if not os.path.exists(EXE):
        print("check-tables: SKIPPED -- no reference/ in this tree")
        return 0

    data = open(EXE, "rb").read()
    bad = []

    # The stride is per table: it is the longest name in that table plus its
    # length byte, rounded up. Planets top out at "Gamma Regula" (12) and
    # items at "Life support supplies" (21), so 13 and 22.
    for cname, first, count, stride, src in (
            ("planet_name", "Andromeda", 8, 13, "core/planet.c"),
            ("item_name",   "Mongol energium", 5, 22, "core/planet.c"),
    ):
        want = table_at(data, first, count, stride)
        got = c_array(src, cname)
        if want is None:
            bad.append("%s: could not find %r in the binary" % (cname, first))
            continue
        if got is None:
            bad.append("%s: not found in %s" % (cname, src))
            continue
        if len(got) != len(want):
            bad.append("%s: %d entries, binary has %d -- %s"
                       % (cname, len(got), len(want), want))
            continue
        for a, b in zip(got, want):
            if a.replace(" ", "") .upper() != b.replace(" ", "").upper():
                bad.append("%s: %r should be %r" % (cname, a, b))

    if bad:
        for b in bad:
            print("check-tables: " + b, file=sys.stderr)
        return 1
    print("check-tables: planet and item names match the binary -- ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
