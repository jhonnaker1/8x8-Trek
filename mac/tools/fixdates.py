#!/usr/bin/env python3
"""Pin the build-time dates hfsutils writes into an HFS floppy image.

hformat and hcopy stamp the volume (its creation, modification and backup
dates, in the Master Directory Block and its alternate copy at the end of
the volume) and the root folder's catalog records with the current time.
This rewrites every such date to MAC_DATE -- the same date setbundle.py
gives the application file -- so the image reproduces byte for byte.

A date counts as build-time if it lies within an hour of the MDB's own
creation date; the app's file dates are already MAC_DATE and are left
alone. Only the two MDBs and the catalog file's extents are touched.
usage: fixdates.py <image.dsk>   (edits in place)

Taken from commodore-uno/mac/tools/ (same author, MIT), where it was written
for UNO's release; the date is the same so the two projects' Mac assets agree.
"""
import struct, sys

MAC_DATE = 0xE72D2700  # 2026-11-26 00:00:00, seconds since 1904-01-01

path = sys.argv[1]
d = bytearray(open(path, 'rb').read())
MDB = 1024
if d[MDB:MDB + 2] != b'BD':
    sys.exit("%s: no HFS volume (MDB signature missing)" % path)
built = struct.unpack('>I', d[MDB + 2:MDB + 6])[0]

alblk_size = struct.unpack('>I', d[MDB + 0x14:MDB + 0x18])[0]
alblk_start = struct.unpack('>H', d[MDB + 0x1C:MDB + 0x1E])[0]
regions = [(MDB, MDB + 512), (len(d) - 1024, len(d) - 512)]  # MDB + alternate MDB
for i in range(3):  # drCTExtRec: the catalog file's three extents
    start, count = struct.unpack('>HH', d[MDB + 0x96 + 4 * i:MDB + 0x9A + 4 * i])
    if count:
        a = alblk_start * 512 + start * alblk_size
        regions.append((a, a + count * alblk_size))

changed = 0
for lo, hi in regions:
    for off in range(lo, hi - 3, 2):
        v = struct.unpack('>I', d[off:off + 4])[0]
        if abs(v - built) <= 3600:
            struct.pack_into('>I', d, off, MAC_DATE)
            changed += 1
open(path, 'wb').write(d)
print("%s: pinned %d dates" % (path, changed))
