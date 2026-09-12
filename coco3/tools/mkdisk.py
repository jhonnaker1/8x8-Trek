#!/usr/bin/env python3
"""Build a Disk BASIC diskette image and put files on it.

35 tracks x 18 sectors x 256 bytes. Disk BASIC formats with $FF everywhere,
and that is also what an empty FAT and an empty directory look like -- a
granule byte of $FF is free, and a directory entry whose first byte is $FF is
unused -- so a blank disk really is 161,280 bytes of $FF.

THE LAYOUT HERE IS READ OUT OF src/coco3storage.c, not out of a reference.
That file is the thing that will have to find these files on the machine, so
its constants are the ones that matter:

    track 17          the directory track, and it is SKIPPED in granule
                      numbering: granule g lives on track g>>1, bumped by one
                      once that reaches 17
    (17, 2)           the FAT: 68 bytes, $FF free, $C0|n the last granule of a
                      chain with n sectors used, anything else the next granule
    (17, 3..11)       directory entries, 8 per sector, 32 bytes each:
                      [0..7] name, [8..10] extension, [11] type, [12] ASCII
                      flag, [13] first granule, [14..15] bytes used in the
                      file's last sector, big-endian
    granule           9 sectors; sector 1 of the track for an even granule,
                      sector 10 for an odd one

ToolShed is not installed on this machine, which is why this exists rather
than shelling out to writecocofile.
"""
import os, sys

TRACKS, SECTORS, SECSIZE = 35, 18, 256
DIR_TRACK, FAT_SECTOR, DIR_FIRST, DIR_LAST = 17, 2, 3, 11
GRAN_SECS, NUM_GRAN, ENT_SIZE, ENT_PER_SEC = 9, 68, 32, 8
SIZE = TRACKS * SECTORS * SECSIZE


def off(track, sector):
    """Byte offset of a sector. SECTORS ARE 1-BASED on this filesystem."""
    return (track * SECTORS + (sector - 1)) * SECSIZE


def gran_loc(g):
    """Where granule g lives -- the same arithmetic as coco3storage.c's
    gran_loc(), including the skipped directory track."""
    t = g >> 1
    if t >= DIR_TRACK:
        t += 1
    return t, (10 if (g & 1) else 1)


def normalise(name):
    """"STRINGS.DAT" -> eleven bytes of name and extension, space padded and
    upper-cased, which is how a directory entry stores it."""
    base, _, ext = name.partition(".")
    if len(base) > 8 or len(ext) > 3:
        sys.exit("mkdisk: %r does not fit 8.3" % name)
    return (base.upper().ljust(8) + ext.upper().ljust(3)).encode("ascii")


class Disk:
    def __init__(self):
        self.d = bytearray(b"\xFF" * SIZE)
        self.next_gran = 0

    def add(self, name, data, ftype=2, ascii_flag=0):
        """ftype 2 = machine-language, which is what every file here is; the
        reader ignores the field, but a real Disk BASIC would not."""
        if not data:
            sys.exit("mkdisk: %s is empty" % name)
        ngran = (len(data) + GRAN_SECS * SECSIZE - 1) // (GRAN_SECS * SECSIZE)
        first = self.next_gran
        if first + ngran > NUM_GRAN:
            sys.exit("mkdisk: out of granules adding %s" % name)

        pos = 0
        for i in range(ngran):
            g = first + i
            trk, sec = gran_loc(g)
            chunk = data[pos:pos + GRAN_SECS * SECSIZE]
            for s in range(GRAN_SECS):
                part = chunk[s * SECSIZE:(s + 1) * SECSIZE]
                if not part:
                    break
                o = off(trk, sec + s)
                # THE SLICE MUST BE SECSIZE WIDE ON BOTH SIDES. Assigning a
                # 256-byte value into a shorter slice does not pad -- it
                # GROWS the bytearray and shifts every sector after it, which
                # silently relocated the directory track and left ten of
                # eleven files unfindable while the eleventh read back fine.
                self.d[o:o + SECSIZE] = part.ljust(SECSIZE, b"\x00")
            pos += len(chunk)
            fo = off(DIR_TRACK, FAT_SECTOR) + g
            if i == ngran - 1:
                nsec = (len(chunk) + SECSIZE - 1) // SECSIZE
                self.d[fo] = 0xC0 | nsec
            else:
                self.d[fo] = g + 1
        self.next_gran = first + ngran

        last = len(data) % SECSIZE or SECSIZE
        ent = bytearray(b"\x00" * ENT_SIZE)
        ent[0:11] = normalise(name)
        ent[11] = ftype
        ent[12] = ascii_flag
        ent[13] = first
        ent[14] = last >> 8
        ent[15] = last & 0xFF
        for s in range(DIR_FIRST, DIR_LAST + 1):
            for e in range(ENT_PER_SEC):
                o = off(DIR_TRACK, s) + e * ENT_SIZE
                if self.d[o] in (0xFF, 0x00):
                    self.d[o:o + ENT_SIZE] = ent
                    return first, ngran
        sys.exit("mkdisk: the directory is full adding %s" % name)


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: mkdisk.py OUT.DSK [FILE ...]")
    out, files = sys.argv[1], sys.argv[2:]
    dk = Disk()
    for f in files:
        data = open(f, "rb").read()
        name = os.path.basename(f)
        g, n = dk.add(name, data)
        print("  %-14s %6d bytes  %2d granule(s) from %d" % (name, len(data), n, g))
    # The image is a FIXED SIZE. If it ever is not, a slice assignment has
    # resized it and every offset past that point is wrong.
    if len(dk.d) != SIZE:
        sys.exit("mkdisk: the image grew to %d bytes -- a slice assignment "
                 "resized it and the layout is corrupt" % len(dk.d))
    open(out, "wb").write(dk.d)
    print("%s: %d bytes, %d file(s), %d of %d granules used"
          % (out, SIZE, len(files), dk.next_gran, NUM_GRAN))


if __name__ == "__main__":
    main()
