#!/usr/bin/env python3
"""Build a disk with NO ATARI DOS ON IT -- this port's own format.

Why a format of our own rather than DOS 2.5's: DOS.SYS is Atari's code, so a
disk carrying it is not ours to redistribute, and DOS also occupies
$0700..$1FFF, which is where this port's writable data wants to live. See
NOTES.md, "The two Atari DOS questions are one lever".

The format is as small as it can be and still honour core/storage.h, which is
keyed by NAME -- the save filename is typed by the player, so names cannot
simply be dropped:

    sector 1..3     boot loader (zero-filled until the loader exists)
    sector 4        the directory: 8 entries of 16 bytes
    sector 5..      file data, each file contiguous

    directory entry
        0..10   name, space padded, no dot -- "STRINGSDAT"
        11..12  first sector, low byte first
        13..14  length in BYTES, low byte first
        15      flags: 1 = in use

Files are contiguous on purpose. A chain would need a link byte in every
sector and a free map to allocate from; contiguous needs neither, and the only
file that ever changes size is the save, which is given a fixed reservation
big enough for the largest it can be.

    python3 tools/nodos.py OUT.atr FILE [FILE...]
"""
import os
import sys

SECTOR = 128
DIR_SECTOR = 4
FIRST_DATA = 5
ENTRIES = 8
RESERVED = {"EGATREK.SAV": 8 * SECTOR, "TREK.SCR": 4 * SECTOR}
TOTAL_SECTORS = 1040                 # enhanced density, as the DOS image uses


def dosname(base):
    """`EGATREK.SAV` -> `EGATREKSAV`, space padded to 11.

    The dot carries no information once the field is fixed-width, and dropping
    it means the compare in the seam is one memcmp with no parsing at all.
    """
    name, _, ext = base.upper().partition(".")
    return (name[:8].ljust(8) + ext[:3].ljust(3)).encode("ascii")


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__.strip().splitlines()[-1])
    out, files = sys.argv[1], sys.argv[2:]

    image = bytearray(SECTOR * TOTAL_SECTORS)
    entries = []
    sec = FIRST_DATA
    for path in files:
        data = open(path, "rb").read()
        base = os.path.basename(path)
        room = max(len(data), RESERVED.get(base.upper(), 0))
        n = (room + SECTOR - 1) // SECTOR
        image[(sec - 1) * SECTOR:(sec - 1) * SECTOR + len(data)] = data
        entries.append((dosname(base), sec, len(data)))
        print("  %-14s sector %4d..%-4d  %6d bytes%s"
              % (base, sec, sec + n - 1, len(data),
                 "  (reserved %d)" % room if room > len(data) else ""))
        sec += n
    if sec > TOTAL_SECTORS:
        sys.exit("nodos: %d sectors needed, disk holds %d" % (sec, TOTAL_SECTORS))
    if len(entries) > ENTRIES:
        sys.exit("nodos: %d files, the directory holds %d" % (len(entries), ENTRIES))

    d = bytearray(SECTOR)
    for i, (name, start, length) in enumerate(entries):
        e = i * 16
        d[e:e + 11] = name
        d[e + 11] = start & 0xFF
        d[e + 12] = start >> 8
        d[e + 13] = length & 0xFF
        d[e + 14] = length >> 8
        d[e + 15] = 1
    image[(DIR_SECTOR - 1) * SECTOR:DIR_SECTOR * SECTOR] = d

    # ATR header: magic, size in 16-byte paragraphs, sector size.
    para = len(image) // 16
    hdr = bytes([0x96, 0x02, para & 0xFF, (para >> 8) & 0xFF,
                 SECTOR & 0xFF, SECTOR >> 8,
                 (para >> 16) & 0xFF, 0]) + bytes(8)
    open(out, "wb").write(hdr + image)
    print("nodos: %s -- %d files, %d sectors used of %d, NO DOS ON IT"
          % (out, len(entries), sec - 1, TOTAL_SECTORS))


main()
