#!/usr/bin/env python3
"""Cut the overlay images out of the link and pack them into OVERLAYS.BIN.

PACKED BY ACTUAL SIZE, NOT PADDED TO THE WINDOW, which every other port does.
Thirteen windows of 4,608 is 59,904 bytes, and with the string pool and the
music ahead of it in the far store that is 67,600 -- past the 65,535 the
seam's 16-bit offsets can address. It overflowed silently once and the tail of
the images landed on the string pool. Packed, the same thirteen are 36,444.

    [ (OVL_COUNT+1) little-endian offsets from the start of the file ]
    [ image 0 ][ image 1 ] ... [ image N-1 ]
    [ 2-byte build stamp ]

The extra index entry is where the images end, so a LENGTH is available
without storing one. See src/atariovl.c.

    cut_overlays.py ELF OUT WINDOW name0 name1 ...
"""
import pathlib
import struct
import subprocess
import sys

OBJCOPY = str(pathlib.Path.home() / "llvm-mos/bin/llvm-objcopy")
NM = str(pathlib.Path.home() / "llvm-mos/bin/llvm-nm")


def main():
    elf, out, window = sys.argv[1], pathlib.Path(sys.argv[2]), int(sys.argv[3])
    names = sys.argv[4:]
    tmp = out.parent

    images = []
    for n in names:
        raw = tmp / ("ovl_%s.raw" % n)
        subprocess.run([OBJCOPY, "-O", "binary", "--only-section=.ovl_%s" % n,
                        elf, str(raw)], check=True)
        d = raw.read_bytes()
        if len(d) > window:
            sys.exit("cut_overlays: %s is %d bytes, window is %d"
                     % (n, len(d), window))
        if not d:
            sys.exit("cut_overlays: %s is EMPTY -- the section name and the "
                     "OVL_CODE annotation must agree, and an empty overlay is "
                     "a jump into nothing at run time" % n)
        images.append(d)

    head = (len(images) + 1) * 2
    offs, pos = [], head
    for d in images:
        offs.append(pos)
        pos += len(d)
    offs.append(pos)

    blob = b"".join(struct.pack("<H", o) for o in offs) + b"".join(images)

    # THE STAMP, from the link the images were cut from -- see src/atariovl.c.
    nm = subprocess.check_output([NM, elf]).decode()
    anchor = [int(l.split()[0], 16) for l in nm.splitlines()
              if l.split()[-1:] == ["ovl_anchor"]]
    if not anchor:
        sys.exit("cut_overlays: no ovl_anchor in the link -- nothing to stamp "
                 "with, and a stale image file would go unnoticed")
    blob += struct.pack("<H", anchor[0] & 0xFFFF)
    out.write_bytes(blob)

    padded = len(images) * window
    print("atari: OVERLAYS.BIN %d bytes, %d images, largest %d, stamp $%04X"
          % (len(blob), len(images), max(len(d) for d in images), anchor[0] & 0xFFFF))
    print("atari: packing saved %d bytes against %d padded to the window"
          % (padded - (len(blob) - head - 2), padded))


main()
