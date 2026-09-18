#!/usr/bin/env python3
"""Wrap a raw binary as a PGZ, the container FoenixMCP's pexec loads.

THE FORMAT, read out of commodore-uno's `toolchain/pgz.s` rather than guessed:

    'Z'                 signature -- the 24-bit variant ('z' is the 16-bit one)
    addr24  size24      one segment header, little-endian, 3 bytes each
    <size bytes>        the segment
    addr24  0           a ZERO-SIZE segment: its address is the ENTRY POINT

So the terminator is not a marker, it is a segment whose size happens to be
zero -- which is how the start address is carried. Getting that wrong gives a
file that loads and never runs.
"""
import argparse, sys

ap = argparse.ArgumentParser()
ap.add_argument("binary")
ap.add_argument("out")
ap.add_argument("--load", required=True, help="load address, e.g. 0x2000")
ap.add_argument("--exec", required=True, help="entry point, e.g. 0x2000")
a = ap.parse_args()

data = open(a.binary, "rb").read()
load, entry = int(a.load, 0), int(a.exec, 0)
if not data:
    sys.exit("mkpgz: %s is empty -- a link that produced nothing is not a PGZ" % a.binary)

def u24(v):
    return bytes((v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF))

with open(a.out, "wb") as f:
    f.write(b"Z")
    f.write(u24(load)); f.write(u24(len(data))); f.write(data)
    f.write(u24(entry)); f.write(u24(0))

print("  %s: %d bytes at $%04X, entry $%04X" % (a.out, len(data), load, entry))
