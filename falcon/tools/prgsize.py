#!/usr/bin/env python3
"""Read the sizes out of a TOS .PRG header.

A GEMDOS executable starts with a 28-byte header: magic $601A, then the text,
data and bss lengths as big-endian longs. That is the whole measurement -- no
map file needed, and it is the same question `make early` asks on every other
port, which is what the game costs before any seam is real.
"""
import struct, sys

for path in sys.argv[1:]:
    h = open(path, 'rb').read(28)
    magic, text, data, bss, syms = struct.unpack('>HIIII', h[:18])
    if magic != 0x601A:
        sys.exit(f"{path}: not a TOS .PRG (magic {magic:#06x})")
    print(f"    {path}")
    print(f"      text {text:>8,}   data {data:>8,}   bss {bss:>8,}")
    print(f"      file {text+data:>8,} bytes of program, "
          f"{text+data+bss:,} resident with bss")
