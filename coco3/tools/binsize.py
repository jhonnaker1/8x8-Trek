#!/usr/bin/env python3
"""Read the load map out of a CoCo Disk BASIC .BIN.

DECB's format is a chain of blocks: a type byte (0x00 data, 0xFF end), a
big-endian length, a big-endian load address, then that many bytes. The final
block carries the exec address. The FILE size is not the program size -- five
bytes of header per block plus a five-byte tail -- so this reports what is
actually loaded and where, which is the number `make early` is asking for.
"""
import struct, sys

for path in sys.argv[1:]:
    d = open(path, 'rb').read()
    i, total, lo, hi, execaddr, blocks = 0, 0, 0xFFFF, 0, None, 0
    while i + 5 <= len(d):
        t, ln, addr = struct.unpack('>BHH', d[i:i+5])
        if t == 0xFF:
            execaddr = addr
            break
        blocks += 1
        total += ln
        lo = min(lo, addr)
        hi = max(hi, addr + ln)
        i += 5 + ln
    print(f"    {path}")
    print(f"      {blocks} block(s), {total:,} bytes loaded")
    print(f"      spans ${lo:04X}..${hi-1:04X}   exec ${execaddr:04X}"
          if execaddr is not None else f"      spans ${lo:04X}..${hi-1:04X}")
    print(f"      file on disk {len(d):,} bytes (DECB headers included)")
    free = 0xFF00 - hi
    print(f"      {free:,} bytes between the end and $FF00 (the I/O page)")
