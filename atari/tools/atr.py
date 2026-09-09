#!/usr/bin/env python3
"""Read and write files inside a DOS 2 ATR disk image.

THE PORT NEEDS THIS TWICE. To test the storage seam at all -- the bridge can
mount an ATR but not a host directory, so there is no way to put a file where
the D: handler can see it without writing the filesystem -- and later to build
whatever the release ships.

DOS 2 single/enhanced density, 128-byte sectors, allocating only out of the
primary VTOC's range (sectors 1..719). DOS 2.5's second VTOC at sector 1024
covers 720..1023 and is deliberately not touched: this tool would have to keep
two bitmaps in step to use it, and 707 sectors is 88,375 bytes, which is more
than anything it is asked to carry.

    atr.py list      DISK
    atr.py add       DISK NAME.EXT FILE
    atr.py extract   DISK NAME.EXT FILE
    atr.py delete    DISK NAME.EXT
"""
import pathlib
import sys

HDR = 16
SEC = 128
VTOC = 360
DIR0, DIRN = 361, 368
FIRST_DATA, LAST_DATA = 4, 719      # 1-3 are the boot sectors
DATA_PER_SEC = SEC - 3


class Atr:
    def __init__(self, path):
        self.path = pathlib.Path(path)
        self.d = bytearray(self.path.read_bytes())

    def off(self, n):
        return HDR + (n - 1) * SEC

    def sec(self, n):
        o = self.off(n)
        return self.d[o:o + SEC]

    def put(self, n, data):
        o = self.off(n)
        self.d[o:o + SEC] = bytes(data).ljust(SEC, b"\0")

    # ---- the free-sector bitmap ----------------------------------------
    def _bit(self, n):
        return 10 + (n >> 3), 7 - (n & 7)

    def is_free(self, n):
        b, s = self._bit(n)
        return bool(self.sec(VTOC)[b] & (1 << s))

    def set_free(self, n, free):
        v = bytearray(self.sec(VTOC))
        b, s = self._bit(n)
        if free:
            v[b] |= (1 << s)
        else:
            v[b] &= ~(1 << s) & 0xFF
        self.put(VTOC, v)

    def free_count(self):
        v = self.sec(VTOC)
        return v[3] | (v[4] << 8)

    def set_free_count(self, n):
        v = bytearray(self.sec(VTOC))
        v[3], v[4] = n & 0xFF, n >> 8
        self.put(VTOC, v)

    # ---- the directory --------------------------------------------------
    def entries(self):
        for i in range(64):
            s = self.sec(DIR0 + i // 8)
            yield i, bytes(s[(i % 8) * 16:(i % 8) * 16 + 16])

    def write_entry(self, i, ent):
        n = DIR0 + i // 8
        s = bytearray(self.sec(n))
        s[(i % 8) * 16:(i % 8) * 16 + 16] = ent
        self.put(n, s)

    @staticmethod
    def name_of(ent):
        base = ent[5:13].decode("ascii", "replace").rstrip()
        ext = ent[13:16].decode("ascii", "replace").rstrip()
        return base + ("." + ext if ext else "")

    @staticmethod
    def encode_name(name):
        base, _, ext = name.upper().partition(".")
        if len(base) > 8 or len(ext) > 3:
            sys.exit("atr: %r is not an 8.3 name" % name)
        return base.ljust(8).encode() + ext.ljust(3).encode()

    def find(self, name):
        want = self.encode_name(name)
        for i, ent in self.entries():
            if ent[0] & 0x40 and not ent[0] & 0x80 and ent[5:16] == want:
                return i, ent
        return None, None

    # ---- operations ------------------------------------------------------
    def list(self):
        print("%-14s %6s %6s   %s" % ("name", "sects", "start", "flag"))
        for i, ent in self.entries():
            if ent[0] and not ent[0] & 0x80:
                print("%-14s %6d %6d   $%02X"
                      % (self.name_of(ent), ent[1] | (ent[2] << 8),
                         ent[3] | (ent[4] << 8), ent[0]))
        print("\n%d sectors free in the primary VTOC (%d bytes)"
              % (self.free_count(), self.free_count() * DATA_PER_SEC))

    def delete(self, name):
        i, ent = self.find(name)
        if i is None:
            sys.exit("atr: %s is not on %s" % (name, self.path.name))
        n = ent[3] | (ent[4] << 8)
        count = 0
        while n:
            s = self.sec(n)
            nxt = ((s[125] & 0x03) << 8) | s[126]
            self.set_free(n, True)
            count += 1
            n = nxt
        self.set_free_count(self.free_count() + count)
        self.write_entry(i, bytes([0x80]) + ent[1:])     # flag as deleted
        self.save()
        print("atr: deleted %s (%d sectors)" % (name, count))

    def add(self, name, data):
        if self.find(name)[0] is not None:
            self.delete(name)
        slot = None
        for i, ent in self.entries():
            if ent[0] == 0 or ent[0] & 0x80:
                slot = i
                break
        if slot is None:
            sys.exit("atr: the directory is full")

        need = max(1, (len(data) + DATA_PER_SEC - 1) // DATA_PER_SEC)
        free = [n for n in range(FIRST_DATA, LAST_DATA + 1)
                if n != VTOC and not (DIR0 <= n <= DIRN) and self.is_free(n)]
        if len(free) < need:
            sys.exit("atr: %s needs %d sectors and %d are free"
                     % (name, need, len(free)))
        chain = free[:need]

        for k, n in enumerate(chain):
            chunk = data[k * DATA_PER_SEC:(k + 1) * DATA_PER_SEC]
            nxt = chain[k + 1] if k + 1 < len(chain) else 0
            s = bytearray(SEC)
            s[:len(chunk)] = chunk
            # byte 125: file number in bits 7-2, next sector's high bits in 1-0
            s[125] = ((slot & 0x3F) << 2) | ((nxt >> 8) & 0x03)
            s[126] = nxt & 0xFF
            s[127] = len(chunk)          # bit7 clear: a full-length data sector
            self.put(n, s)
            self.set_free(n, False)

        self.set_free_count(self.free_count() - need)
        ent = bytearray(16)
        ent[0] = 0x42                    # in use, DOS 2 file, unlocked
        ent[1], ent[2] = need & 0xFF, need >> 8
        ent[3], ent[4] = chain[0] & 0xFF, chain[0] >> 8
        ent[5:16] = self.encode_name(name)
        self.write_entry(slot, bytes(ent))
        self.save()
        print("atr: wrote %s, %d bytes in %d sectors from %d"
              % (name, len(data), need, chain[0]))

    def extract(self, name):
        i, ent = self.find(name)
        if i is None:
            sys.exit("atr: %s is not on %s" % (name, self.path.name))
        out = bytearray()
        n = ent[3] | (ent[4] << 8)
        while n:
            s = self.sec(n)
            out += s[:s[127] & 0x7F]
            n = ((s[125] & 0x03) << 8) | s[126]
        return bytes(out)

    def save(self):
        self.path.write_bytes(bytes(self.d))


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    op, disk = sys.argv[1], sys.argv[2]
    a = Atr(disk)
    if op == "list":
        a.list()
    elif op == "add":
        a.add(sys.argv[3], pathlib.Path(sys.argv[4]).read_bytes())
    elif op == "extract":
        pathlib.Path(sys.argv[4]).write_bytes(a.extract(sys.argv[3]))
        print("atr: extracted %s" % sys.argv[3])
    elif op == "delete":
        a.delete(sys.argv[3])
    else:
        sys.exit(__doc__)


main()
