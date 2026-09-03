#!/usr/bin/env python3
"""Build a MEGA65 SD-card image the Hypervisor will actually accept.

`mformat` alone is not enough, and the failure is loud once you look at the
SCREEN rather than the exit status: the Hypervisor prints COULD NOT CHDIR TO /
and COULD NOT FIND ROM MEGA65.ROM, because it wants a PARTITION TABLE with a
FAT32 partition -- not a bare filesystem written at sector 0.

So: an MBR with one type-0x0C partition at sector 2048, FAT32 inside it, the
ROM at the root because the Hypervisor loads it from there, and the game's data
files alongside it.
"""
import subprocess
import sys

SECTOR   = 512
PART_LBA = 2048
SIZE_MB  = 4200      # SDHC: the Hypervisor prints "LOOKING FOR SDHC CARD >=4GB"
                     # and refuses anything smaller. Sparse on APFS, so it
                     # costs the few hundred KB actually written.


def main(img, rom, files):
    total = SIZE_MB * 1024 * 1024
    with open(img, "wb") as f:
        f.truncate(total)

    part_sectors = total // SECTOR - PART_LBA
    mbr = bytearray(SECTOR)
    e = 0x1BE
    mbr[e + 0] = 0x80                       # bootable
    mbr[e + 1:e + 4] = b"\x00\x02\x00"      # CHS start, ignored by anything modern
    mbr[e + 4] = 0x0C                       # FAT32 LBA
    mbr[e + 5:e + 8] = b"\xFE\xFF\xFF"      # CHS end, ditto
    mbr[e + 8:e + 12]  = PART_LBA.to_bytes(4, "little")
    mbr[e + 12:e + 16] = part_sectors.to_bytes(4, "little")
    mbr[510:512] = b"\x55\xAA"
    with open(img, "r+b") as f:
        f.write(bytes(mbr))

    at = "%s@@%d" % (img, PART_LBA * SECTOR)
    subprocess.run(["mformat", "-i", at, "-F", "-v", "MEGA65", "::"], check=True)
    for src in [rom] + files:
        subprocess.run(["mcopy", "-i", at, "-o", src, "::/"], check=True)
    subprocess.run(["mdir", "-i", at, "::"], check=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2], sys.argv[3:]))
