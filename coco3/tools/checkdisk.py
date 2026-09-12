#!/usr/bin/env python3
"""Read a disk image back with src/coco3storage.c's OWN algorithm and compare
every file byte for byte against the originals.

A WRITER CHECKED ONLY BY THE THING THAT WROTE IT CHECKS NOTHING. This is
transcribed from the C the machine will actually run -- find_file, file_len
and the granule walk -- so a disagreement between mkdisk.py and the port shows
up here instead of as an overlay that will not load.

It caught the first version of mkdisk.py immediately: assigning a 256-byte
value into a shorter bytearray slice GROWS the array rather than padding,
which shifted every sector after it, relocated the directory track, and left
ten of eleven files unfindable while the eleventh read back perfectly.
"""
import os, sys

SEC, GS, NG, ENT, EPS = 256, 9, 68, 32, 8
DIRT, FATS, DF, DL = 17, 2, 3, 11


def off(t, s):
    return (t * 18 + (s - 1)) * SEC


def gran_loc(g):
    t = g >> 1
    if t >= DIRT:
        t += 1
    return t, (10 if (g & 1) else 1)


def normalise(name):
    b, _, e = name.partition(".")
    return (b.upper().ljust(8) + e.upper().ljust(3)).encode()


class Reader:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        self.fat = self.d[off(DIRT, FATS):off(DIRT, FATS) + NG]

    def find(self, name):
        want = normalise(name)
        for s in range(DF, DL + 1):
            for e in range(EPS):
                p = off(DIRT, s) + e * ENT
                ent = self.d[p:p + ENT]
                if ent[0] in (0xFF, 0x00):
                    continue
                if ent[0:11] == want:
                    return ent[13], ent[14] * 256 + ent[15]
        return None, None

    def length(self, g, lastbytes):
        n, guard = 0, 0
        while guard < NG + 1:
            guard += 1
            v = self.fat[g]
            if (v & 0xC0) == 0xC0:
                return n + ((v & 0x3F) - 1) * SEC + lastbytes
            n += GS * SEC
            g = v
            if g >= NG:
                break
        return 0

    def read(self, name):
        g, lb = self.find(name)
        if g is None:
            return None
        ln = self.length(g, lb)
        out = bytearray()
        while g < NG:
            v = self.fat[g]
            nsec = (v & 0x3F) if (v & 0xC0) == 0xC0 else GS
            t, s = gran_loc(g)
            for i in range(nsec):
                o = off(t, s + i)
                out += self.d[o:o + min(SEC, ln - len(out))]
                if len(out) >= ln:
                    break
            if (v & 0xC0) == 0xC0:
                break
            g = v
        return bytes(out)


def main():
    if len(sys.argv) < 3:
        sys.exit("usage: checkdisk.py DISK.DSK FILE [FILE ...]")
    r = Reader(sys.argv[1])
    bad = 0
    for p in sys.argv[2:]:
        want = open(p, "rb").read()
        got = r.read(os.path.basename(p))
        ok = got == want
        bad += 0 if ok else 1
        print("  %-4s %-14s %5d bytes%s"
              % ("ok" if ok else "FAIL", os.path.basename(p), len(want),
                 "" if ok else "   read back %s" % (len(got) if got is not None else "NOTHING")))
    # NEGATIVE CONTROL: a name that is not on the disk must not be found, or
    # every "ok" above is meaningless.
    if r.find("NOSUCH.OVL")[0] is not None:
        sys.exit("checkdisk: a file that is not there was FOUND -- the reader "
                 "matches anything and proves nothing")
    print("checkdisk: %d file(s) round-tripped%s"
          % (len(sys.argv) - 2, "" if not bad else ", %d MISMATCH" % bad))
    return 1 if bad else 0


sys.exit(main())
