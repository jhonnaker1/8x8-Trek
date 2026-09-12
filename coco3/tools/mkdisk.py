#!/usr/bin/env python3
"""Make a blank Disk BASIC diskette image: 35 tracks x 18 sectors x 256 bytes.

Disk BASIC formats with $FF everywhere, and that is also what an empty FAT and
an empty directory look like -- a granule byte of $FF is free, and a directory
entry whose first byte is $FF is unused. So a blank disk really is 161,280
bytes of $FF, and writecocofile fills in the rest.
"""
import sys

TRACKS, SECTORS, SECSIZE = 35, 18, 256
size = TRACKS * SECTORS * SECSIZE

out = sys.argv[1] if len(sys.argv) > 1 else "blank.dsk"
open(out, "wb").write(b"\xFF" * size)
print(f"{out}: {size:,} bytes, {TRACKS} tracks x {SECTORS} sectors x {SECSIZE}")
