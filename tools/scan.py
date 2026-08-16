#!/usr/bin/env python3
"""Scan the original EGA Trek's memory for Turbo Pascal reals of known value.

The game is Turbo Pascal and holds its state as `real`: one biased exponent
byte, then a 39-bit mantissa with an implicit leading 1 and the sign in the
top bit of the last byte. dosbox-automation's /memory/search takes an integer
and a width, which cannot express that -- searching for 2500 with shields
visibly at 2500 returns nothing at all. So pull the region down and scan here.

Needs the original running under dosbox-automation; see NOTES.md for the
build and launch. Point DOSBOX_API_TOKEN at the same token the emulator got.

    DOSBOX_API_TOKEN=<64 hex chars> python3 tools/scan.py 5000 2500 500

With no arguments it scans for the values we already know, which is the way
to confirm the instrument is working before trusting it on anything new.
"""
import math
import os
import sys
import urllib.request

BASE = os.environ.get("DOSBOX_API", "http://localhost:8386/api/v1")
TOKEN = os.environ.get("DOSBOX_API_TOKEN", "")

# EGATREK's MCB as reported by GET /dos/internals: segment 483, 338304 bytes.
# Re-check it if the DOS environment changes; everything below assumes it.
LO, HI = 7728, 346032

KNOWN = [("impulse", 500.0), ("energy", 5000.0), ("shields", 2500.0),
         ("stardate", 3500.0)]


def read(off, length):
    req = urllib.request.Request(
        "%s/memory/%d/%d" % (BASE, off, length),
        headers={"Authorization": "Bearer " + TOKEN})
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read()


def tp_real(v):
    """Encode a positive float as Turbo Pascal's 6-byte real."""
    e = math.floor(math.log2(v))
    m = int(round((v / (2.0 ** e) - 1.0) * (1 << 39)))
    if m >> 39:                        # rounding carried into the implicit bit
        m, e = 0, e + 1
    return bytes([e + 129]) + bytes((m >> (8 * i)) & 0xFF for i in range(5))


def decode(b):
    """Inverse of tp_real; None for zero or negative."""
    if b[0] == 0:
        return None
    m = int.from_bytes(b[1:6], "little")
    if m >> 39:                        # sign bit set
        return None
    return (1.0 + m / float(1 << 39)) * (2.0 ** (b[0] - 129))


def dump():
    buf, off = bytearray(), LO
    while off < HI:
        n = min(32768, HI - off)
        buf += read(off, n)
        off += n
    return bytes(buf)


def find(mem, value, limit=16):
    pat, hits, i = tp_real(value), [], 0
    i = mem.find(pat)
    while i != -1 and len(hits) < limit:
        hits.append(LO + i)
        i = mem.find(pat, i + 1)
    return hits


def main():
    if not TOKEN:
        sys.exit("set DOSBOX_API_TOKEN to the emulator's 64-hex-char token")

    mem = dump()
    print("read %d bytes (%d..%d)" % (len(mem), LO, HI))

    wanted = [(a, float(a)) for a in sys.argv[1:]] or KNOWN
    for label, value in wanted:
        hits = find(mem, value)
        print("%-10s %-10g %s -> %d hit(s) %s"
              % (label, value, tp_real(value).hex(), len(hits), hits[:8]))
    return mem


if __name__ == "__main__":
    main()
