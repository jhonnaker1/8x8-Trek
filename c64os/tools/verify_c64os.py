#!/usr/bin/env python3
"""The checks the C64 OS build cannot make for itself. `make verify` runs it.

Each is a way this port has already gone wrong, or nearly:

  THE FILE CEILING. C64 OS loads an app over whatever pages are there -- it
  does not refuse one too big -- and its drivers and libraries live from
  about $6F up, moving between boots. The first game file ran to $79FF, sat
  on the mouse driver, and crashed C64 OS. main.o must end below $5C00
  (NOTES.md, "THE FILE WAS TOO BIG").

  THE WINDOW. Resident code and data end below the overlay window, which
  main() itself runs from -- so nothing resident may reach it, and every
  overlay, with its two-byte stamp, must fit its 4K.

  THE ARCHIVE. egatrek.car must hold exactly the bundle: the EGA Trek folder,
  every file in files.txt with its type and every byte, and a CRC32 that
  matches -- read back by tools/mkcar.py's own reader, proved on three real
  C64 OS app archives.

  THE MUSIC is this project's composition, never note data from the original.
"""
import os
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = os.path.dirname(HERE)
BUILD = os.path.join(PORT, "build")
BUNDLE = os.path.join(BUILD, "bundle")
CEILING = 0x5C00
WINDOW = 0x1000
NM = os.path.expanduser("~/llvm-mos/bin/llvm-nm")

sys.path.insert(0, HERE)
import mkcar  # noqa: E402

bad = 0


def check(ok, what):
    global bad
    print("  %s  %s" % ("ok  " if ok else "FAIL", what))
    if not ok:
        bad += 1


def symbols():
    out = subprocess.check_output([NM, os.path.join(BUILD, "trek.elf")], text=True)
    return {f[2]: int(f[0], 16) for f in (l.split() for l in out.splitlines()) if len(f) == 3}


def main():
    files = [tuple(l.strip().rsplit(",", 1)) for l in open(os.path.join(BUNDLE, "files.txt")) if l.strip()]
    main_o = open(os.path.join(BUNDLE, "main.o"), "rb").read()
    load = main_o[0] | main_o[1] << 8
    end = load + len(main_o) - 2
    check(load == 0x0900, "main.o loads at $%04X, where C64 OS puts an app" % load)
    check(end <= CEILING, "verify: resident file $0900..$%04X, %d spare below $%04X"
          % (end - 1, CEILING - end, CEILING))

    sym = symbols()
    win, top = sym["__ovl_start"], sym["__heap_start"]
    check(top <= win, "verify: resident code and data end at $%04X, window at $%04X, %d spare"
          % (top, win, win - top))
    check(sym["main"] == win, "main() is an overlay, at the window's first byte")
    if "__stack" in sym and "__stack_bottom" in sym:
        print("  ok    verify: soft stack %d bytes at $%04X"
              % (sym["__stack"] - sym["__stack_bottom"], sym["__stack_bottom"]))

    ovls = [f for f, _ in files if f.startswith("ovl")]
    sizes = {f: os.path.getsize(os.path.join(BUNDLE, f)) - 2 for f in ovls}
    big = max(sizes, key=sizes.get)
    check(all(v <= WINDOW for v in sizes.values()),
          "verify: largest overlay %s, %d of %d, %d spare" % (big, sizes[big], WINDOW, WINDOW - sizes[big]))
    check(len(ovls) == 23, "%d overlays in files.txt" % len(ovls))

    car = open(os.path.join(BUILD, "egatrek.car"), "rb").read()
    body, crc = car[:-4], struct.unpack("<I", car[-4:])[0]
    check(zlib.crc32(body) == crc, "egatrek.car CRC32 %08x" % crc)
    check(car[0] == 0 and car[1:11] == mkcar.petscii(mkcar.MAGIC) and car[11] == 3,
          "egatrek.car: a general C64 Archive, version 3")
    p = 48
    folder = mkcar.ascii_of(car[p + 5:p + 21].rstrip(b"\xa0"))
    n = car[p + 2] | car[p + 3] << 8 | car[p + 4] << 16
    check(chr(car[p]) == "D" and folder == "EGA Trek" and n == len(files),
          "egatrek.car holds the folder \"%s\" with %d entries" % (folder, n))
    p += 22
    same = 0
    for name, t in files:
        kind = chr(car[p])
        size = car[p + 2] | car[p + 3] << 8 | car[p + 4] << 16
        rname = mkcar.ascii_of(car[p + 5:p + 21].rstrip(b"\xa0"))
        data = car[p + 22:p + 22 + size]
        if (rname, kind, data) == (name, t.upper(), open(os.path.join(BUNDLE, name), "rb").read()):
            same += 1
        p += 22 + size
    check(same == len(files) and p == len(body),
          "every file in the .car matches the bundle, name, type and byte (%d of %d)" % (same, len(files)))

    music = open(os.path.join(BUNDLE, "music.dat"), "rb").read()
    comp = open(os.path.join(PORT, "..", "c128", "build", "music.dat"), "rb").read()
    check(music == comp, "music.dat is byte-identical to the composition (%d bytes)" % len(music))

    if bad:
        sys.exit("verify_c64os: %d check(s) FAILED" % bad)
    print("verify_c64os: the C64 OS build and its archive are sound")


if __name__ == "__main__":
    main()
