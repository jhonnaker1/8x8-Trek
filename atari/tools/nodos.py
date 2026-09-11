#!/usr/bin/env python3
"""Build a disk with NO ATARI DOS ON IT -- this port's own format.

Why a format of our own rather than DOS 2.5's: DOS.SYS is Atari's code, so a
disk carrying it is not ours to redistribute, and DOS also occupies
$0700..$1FFF, which is where this port's writable data wants to live. See
NOTES.md, "The two Atari DOS questions are one lever".

The format is as small as it can be and still honour core/storage.h, which is
keyed by NAME -- the save filename is typed by the player, so names cannot
simply be dropped:

    sector 1..3     the boot record -- src/boot.s, which finds EGATREK.XEX in
                    the directory below and loads it. Zero-filled if no --boot
                    is given, which makes an unbootable data disk.
    sector 4..5     the directory: 16 entries of 16 bytes
    sector 6..      file data, each file contiguous

    directory entry
        0..10   name, space padded, no dot -- "STRINGSDAT"
        11..12  first sector, low byte first
        13..14  length in BYTES, low byte first
        15      flags: bit 0 in use, bit 1 writable slot

Files are contiguous on purpose. A chain would need a link byte in every
sector and a free map to allocate from; contiguous needs neither.

WHICH LEAVES THE SAVE, WHICH IS THE ONLY FILE THAT IS NOT KNOWN AT BUILD TIME.
The player types its name (ui.c offers EGATREK.SAV and takes anything), and
TREK.SCR appears the first time a game reaches the hall of fame. So the disk
is built with SLOTS: entries whose extent is already assigned and whose name
is still empty, flagged writable. src/atarisio.c claims one by name on the
first write to a name it cannot find. That is the whole allocator -- no free
map, no compaction, and nothing that can fragment.

The writable bit is not decoration either: it is what makes "you cannot
overwrite STRINGS.DAT" a property of the format rather than of nobody having
tried.

    python3 tools/nodos.py OUT.atr [--boot BOOT.BIN] [--slots N] FILE [FILE...]
    python3 tools/nodos.py --list DISK.atr
"""
import os
import sys

SECTOR = 128
BOOT_SECTORS = 3
DIR_SECTOR = 4
DIR_SECTORS = 2
FIRST_DATA = DIR_SECTOR + DIR_SECTORS
PER_SECTOR = SECTOR // 16
ENTRIES = DIR_SECTORS * PER_SECTOR

# EIGHT SECTORS A SLOT, and the number is in src/atarisio.c as well because
# both halves have to agree on how much a claimed slot may hold. The save is
# 625 bytes today (SAVE_HDR + TREK_SAVE_SIZE) and the hall of fame 384, so
# 1,024 is room for both and for a save record that grows again -- it has.
SLOT_SECTORS = 8
DEFAULT_SLOTS = 11                   # 16 entries less the five game files
TOTAL_SECTORS = 1040                 # enhanced density, as the DOS image uses


def dosname(base):
    """`EGATREK.SAV` -> `EGATREK SAV`, space padded to 11.

    The dot carries no information once the field is fixed-width, and dropping
    it means the compare in the seam is one memcmp with no parsing at all.
    """
    name, _, ext = base.upper().partition(".")
    return (name[:8].ljust(8) + ext[:3].ljust(3)).encode("ascii")


def show(path):
    """Read a directory back. THE BUILDER OWNS THE FORMAT, so the reader lives
    here rather than in whichever probe needed it first -- a second copy of
    these offsets is a second place for them to be wrong."""
    d = open(path, "rb").read()[16:]              # past the ATR header
    dirbytes = d[(DIR_SECTOR - 1) * SECTOR:(DIR_SECTOR - 1 + DIR_SECTORS) * SECTOR]
    for i in range(ENTRIES):
        e = dirbytes[i * 16:i * 16 + 16]
        if not e[15]:
            continue
        start = e[11] | e[12] << 8
        length = e[13] | e[14] << 8
        what = ("file" if e[15] == 1 else
                "slot, unclaimed" if e[15] == 2 else "slot, claimed")
        print("%-11s  sector %4d  %6d bytes  flags %d  (%s)"
              % (e[:11].decode("ascii", "replace"), start, length, e[15], what))
    return 0


def main():
    argv = sys.argv[1:]
    if argv and argv[0] == "--list":
        sys.exit(show(argv[1]))
    boot, slots = None, DEFAULT_SLOTS
    for opt in ("--boot", "--slots"):
        if opt in argv:
            i = argv.index(opt)
            value = argv[i + 1]
            del argv[i:i + 2]
            if opt == "--boot":
                boot = value
            else:
                slots = int(value)
    if len(argv) < 2:
        sys.exit(__doc__.strip().splitlines()[-1])
    out, files = argv[0], argv[1:]

    image = bytearray(SECTOR * TOTAL_SECTORS)

    # THE BOOT RECORD IS NOT LENGTH-CHECKED HERE. boot.ld asserts both of its
    # bounds at link time, where the number is a symbol rather than a file
    # size -- and a check in two places is a check that can disagree.
    if boot:
        code = open(boot, "rb").read()
        image[0:len(code)] = code
        print("  %-14s sector    1..%-4d  %6d bytes  (boot record; the OS "
              "reads %d sectors)"
              % (os.path.basename(boot), BOOT_SECTORS, len(code), code[1]))

    entries = []
    sec = FIRST_DATA
    for path in files:
        data = open(path, "rb").read()
        base = os.path.basename(path)
        n = (len(data) + SECTOR - 1) // SECTOR
        image[(sec - 1) * SECTOR:(sec - 1) * SECTOR + len(data)] = data
        entries.append((dosname(base), sec, len(data), 1))
        print("  %-14s sector %4d..%-4d  %6d bytes"
              % (base, sec, sec + n - 1, len(data)))
        sec += n

    for _ in range(slots):
        entries.append((b" " * 11, sec, 0, 2))
        sec += SLOT_SECTORS
    if slots:
        print("  %-14s sector %4d..%-4d  %6d bytes each, unclaimed"
              % ("%d slots" % slots, entries[-slots][1], sec - 1,
                 SLOT_SECTORS * SECTOR))

    if sec > TOTAL_SECTORS + 1:
        sys.exit("nodos: %d sectors needed, disk holds %d" % (sec - 1, TOTAL_SECTORS))
    if len(entries) > ENTRIES:
        sys.exit("nodos: %d entries, the directory holds %d" % (len(entries), ENTRIES))

    d = bytearray(SECTOR * DIR_SECTORS)
    for i, (name, start, length, flags) in enumerate(entries):
        e = i * 16
        d[e:e + 11] = name
        d[e + 11] = start & 0xFF
        d[e + 12] = start >> 8
        d[e + 13] = length & 0xFF
        d[e + 14] = length >> 8
        d[e + 15] = flags
    image[(DIR_SECTOR - 1) * SECTOR:(DIR_SECTOR - 1 + DIR_SECTORS) * SECTOR] = d

    # ATR header: magic, size in 16-byte paragraphs, sector size.
    para = len(image) // 16
    hdr = bytes([0x96, 0x02, para & 0xFF, (para >> 8) & 0xFF,
                 SECTOR & 0xFF, SECTOR >> 8,
                 (para >> 16) & 0xFF, 0]) + bytes(8)
    open(out, "wb").write(hdr + image)
    print("nodos: %s -- %d files + %d slots, %d sectors used of %d, %s, NO DOS"
          % (out, len(files), slots, sec - 1, TOTAL_SECTORS,
             "BOOTABLE" if boot else "data only, NOT bootable"))


main()
