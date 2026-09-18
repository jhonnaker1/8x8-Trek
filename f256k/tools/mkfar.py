#!/usr/bin/env python3
"""Make the far-memory test's data files.

GENERATED ON THE HOST, and independently of the program that checks them. A
10,000-byte pattern cannot be built inside a program whose whole .bss lives in
7K of low memory -- and computing the expected bytes twice, in two languages,
is the difference between a test that checks the data and one that checks that
the program agrees with itself.
"""
import sys
big = bytes(((i * 7) ^ (i >> 3) ^ 0x5A) & 0xFF for i in range(10000))
small = bytes(((i * 11) ^ 0xC3) & 0xFF for i in range(300))
open(sys.argv[1], "wb").write(big)
open(sys.argv[2], "wb").write(small)
print("  %s: %d bytes   %s: %d bytes" % (sys.argv[1], len(big), sys.argv[2], len(small)))
