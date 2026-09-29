#!/usr/bin/env python3
"""C64 OS's archive format, .car: write one from the bundle, or read one back.

    mkcar.py write BUNDLE "FOLDER" "NOTE" YYYY-MM-DD OUT.car
    mkcar.py read  FILE.car

C64 OS APPS SHIP AS A .car. A player copies it to //os/applications and
double-clicks it, and C64 OS's own extractor makes the app's folder there --
or extracts it somewhere else and the folder is moved in. This writes one the
way the three third-party app releases on Jamie's C64 OS 1.08 volume are
built (Fart Machine, Desktop Designer, Eliza -- read with `read`, which is
how the layout below was measured, CRC included):

  HEADER, 48 bytes (//os/s/c64archive.t names the offsets)
    0      archive type: 0 general -- "the folder goes where the .car is";
           2 is an install that overlays the system directory
    1-10   "C64Archive", in C64 OS's PETSCII
    11     version: 3 in all three
    12-16  date: year-1900, month, day, hour, minute
    17-47  a 30-character note and its NUL
  RECORDS, each a 22-byte header and then the file's bytes
    0      'D' $44 directory, 'S' $53 SEQ, 'P' $50 PRG
    1      lock
    2-4    size, little-endian -- a DIRECTORY's is its number of children
    5-20   name, PETSCII, padded with $A0
    21     compression, 0 none
  TRAILER, 4 bytes: CRC32 of everything before it, little-endian -- the
  standard polynomial, what checksum.lib's updc32 computes. All three
  samples match.

The date is an argument, not the clock, so the same bundle makes the same
archive byte for byte -- the rule every other release artefact here keeps.

NAMES ARE C64 OS'S PETSCII, the lowercase set: a-z are $41-$5A, A-Z are
$C1-$DA. "EGA Trek" is the folder the player sees.
"""
import os
import struct
import sys
import zlib

MAGIC = "C64Archive"
VERSION = 3
NAME_LEN = 16


def petscii(s):
    out = bytearray()
    for ch in s:
        c = ord(ch)
        if 0x61 <= c <= 0x7A:
            out.append(c - 0x20)            # a-z -> $41-$5A
        elif 0x41 <= c <= 0x5A:
            out.append(c + 0x80)            # A-Z -> $C1-$DA
        elif 0x20 <= c <= 0x3F:
            out.append(c)
        else:
            sys.exit("mkcar: %r has no PETSCII here" % ch)
    return bytes(out)


def ascii_of(b):
    return "".join(chr(c + 0x20) if 0x41 <= c <= 0x5A else
                   chr(c - 0x80) if 0xC1 <= c <= 0xDA else
                   chr(c) if 0x20 <= c < 0x7F else "." for c in b)


def record(kind, name, size):
    n = petscii(name)
    if len(n) > NAME_LEN:
        sys.exit("mkcar: %r is longer than %d" % (name, NAME_LEN))
    return (bytes([ord(kind), 0]) + struct.pack("<I", size)[:3]
            + n.ljust(NAME_LEN, b"\xa0") + bytes([0]))


def write(bundle, folder, note, date, out):
    y, m, d = (int(x) for x in date.split("-"))
    nb = petscii(note)
    if len(nb) > 30:
        sys.exit("mkcar: the note is %d characters; 30 fit" % len(nb))
    head = (bytes([0]) + petscii(MAGIC) + bytes([VERSION])
            + bytes([y - 1900, m, d, 0, 0]) + nb.ljust(31, b"\x00"))
    assert len(head) == 48
    files = []
    for line in open(os.path.join(bundle, "files.txt")):
        line = line.strip()
        if line:
            name, t = line.rsplit(",", 1)
            files.append((name, {"p": "P", "s": "S"}[t.lower()]))
    body = record("D", folder, len(files))
    for name, kind in files:
        data = open(os.path.join(bundle, name), "rb").read()
        body += record(kind, name, len(data)) + data
    blob = head + body
    blob += struct.pack("<I", zlib.crc32(blob))
    open(out, "wb").write(blob)
    print("mkcar: %s -- \"%s\", %d files, %d bytes" % (out, folder, len(files), len(blob)))


def read(path):
    """Decode, print the tree, and check the CRC. Exit 1 if anything is off."""
    b = open(path, "rb").read()
    if b[1:11] != petscii(MAGIC):
        sys.exit("mkcar: %s is not a C64 Archive" % path)
    y, mo, d, h, mi = b[12:17]
    print("%s: type %d, version %d, %04d-%02d-%02d %02d:%02d, note %r"
          % (os.path.basename(path), b[0], b[11], 1900 + y, mo, d, h, mi,
             ascii_of(b[17:48].split(b"\x00")[0])))
    pos = [48]

    def rec(depth):
        p = pos[0]
        kind, size = chr(b[p]), b[p + 2] | b[p + 3] << 8 | b[p + 4] << 16
        name = ascii_of(b[p + 5:p + 21].rstrip(b"\xa0"))
        print("  " * depth + "%s %-16s %6d%s" % (kind, name, size,
              "" if b[p + 21] == 0 else "  compression %d" % b[p + 21]))
        pos[0] = p + 22
        if kind == "D":
            for _ in range(size):
                rec(depth + 1)
        elif kind in "PS":
            pos[0] += size
        else:
            sys.exit("mkcar: unknown record type %r at %d" % (kind, p))

    rec(1)
    end = pos[0]
    crc = zlib.crc32(b[:end])
    stored = struct.unpack("<I", b[end:end + 4])[0] if len(b) >= end + 4 else None
    ok = stored == crc and len(b) == end + 4
    print("  CRC32 %08x, stored %s, %d bytes after it -- %s"
          % (crc, "%08x" % stored if stored is not None else "none",
             len(b) - end - 4, "ok" if ok else "BAD"))
    return 0 if ok else 1


def main():
    a = sys.argv[1:]
    if a[:1] == ["write"] and len(a) == 6:
        return write(*a[1:])
    if a[:1] == ["read"] and len(a) == 2:
        return read(a[1])
    sys.exit(__doc__.strip().splitlines()[2] + "\n" + __doc__.strip().splitlines()[3])


if __name__ == "__main__":
    sys.exit(main())
