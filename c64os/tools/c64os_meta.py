#!/usr/bin/env python3
"""Writes a C64 OS application's menu.m and about.t.

COPIED FROM commodore-uno/c64os-llvm/tools/c64os_meta.py (Jamie's sibling
project), unchanged but for this paragraph; its notes below refer to uno.

The same two files ../c64os/tools builds, in the same format (see
../c64os/tools/schema/app.py), without its cbmcodecs2 dependency: every
string here is plain ASCII, and C64 OS's lowercase PETSCII for ASCII is a
case swap.

    c64os_meta.py menu  src/menu.json      > out/menu.m
    c64os_meta.py about NAME VERSION YEAR AUTHOR > out/about.t

The year is an argument rather than today's date so a rebuild reproduces
the same bytes.
"""
import collections
import json
import sys

CR = b"\x0d"


def petscii(s):
    out = bytearray()
    for ch in s:
        c = ord(ch)
        if c > 0x7E or (c < 0x20):
            sys.exit(f"c64os_meta: {ch!r} has no plain PETSCII mapping")
        if 0x61 <= c <= 0x7A:      # a-z -> $41-$5A, lowercase glyphs
            c -= 0x20
        elif 0x41 <= c <= 0x5A:    # A-Z -> $C1-$DA, capitals
            c += 0x80
        out.append(c)
    return bytes(out)


def menu(items, out):
    for key, val in items.items():
        if isinstance(val, dict):
            out.write(petscii(f"{key};{chr(ord('a') - 1 + len(val))}") + CR)
            menu(val, out)
        else:
            out.write(petscii(f"{key}:{val}") + CR)


def main(argv):
    out = sys.stdout.buffer
    if len(argv) == 2 and argv[0] == "menu":
        with open(argv[1]) as f:
            items = json.load(f, object_pairs_hook=collections.OrderedDict)
        menu(items, out)
        out.write(CR)
    elif len(argv) == 5 and argv[0] == "about":
        name, version, year, author = argv[1:]
        out.write(petscii(name) + CR)
        out.write(b" " + petscii(version) + CR)
        out.write(petscii(year) + CR)
        out.write(petscii(author) + CR)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv[1:])
