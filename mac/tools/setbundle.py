#!/usr/bin/env python3
"""Set the Finder's hasBundle flag (and clear 'inited') in a MacBinary II
file, pin its creation/modification dates, and fix up the header CRC.

Rez writes Finder flags of zero, and without hasBundle the Finder ignores
the app's BNDL/ICN# and shows the generic application icon. Clearing
'inited' makes the Finder read the bundle afresh the first time it sees
the file. The dates Rez writes are the build time; pinning them (to
MAC_DATE below, the same date tools/fixdates.py gives the floppy) makes the
release reproducible byte for byte.
usage: setbundle.py <file.bin>   (edits in place)

Taken from commodore-uno/mac/tools/ (same author, MIT), where it was written
for UNO's release; the date is the same so the two projects' Mac assets agree.
"""
import struct, sys

# 2026-11-26 00:00:00 in classic Mac time (seconds since 1904-01-01).
MAC_DATE = 0xE72D2700

def crc16_xmodem(data):
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc

path = sys.argv[1]
d = bytearray(open(path, 'rb').read())
# Rez leaves the MacBinary II version bytes (122/123) at zero but does write
# the CRC, so the CRC is what identifies the header here.
if d[0] != 0 or struct.unpack('>H', d[124:126])[0] != crc16_xmodem(d[:124]):
    sys.exit("%s: not a MacBinary file with a valid header CRC" % path)
d[73] = (d[73] | 0x20) & ~0x01  # high byte of fdFlags: +hasBundle, -inited
struct.pack_into('>II', d, 91, MAC_DATE, MAC_DATE)  # creation, modification
struct.pack_into('>H', d, 124, crc16_xmodem(d[:124]))
open(path, 'wb').write(d)
