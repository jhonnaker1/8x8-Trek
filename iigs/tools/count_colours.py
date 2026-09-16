#!/usr/bin/env python3
"""Count the distinct colours in a MAME snapshot, and say where the content is.

THE INSTRUMENT THE IIgs WAS RULED OUT WITH, and now the one it is being
measured back in with. The 2026-09-05 exercise counted colours off a PNG to
establish that 640 mode gives four freely-placeable colours; this is the same
question asked of 320 mode, so it had better be the same question ASKED THE
SAME WAY rather than a fresh script that happens to agree.

No Pillow -- it decodes the PNG itself, because a dependency that is not
installed turns a measurement into an error message three weeks from now.
"""
import collections
import struct
import sys
import zlib


def png_pixels(path):
    d = open(path, "rb").read()
    assert d[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    i, idat, w, h, bd, ct = 8, b"", None, None, None, None
    while i < len(d):
        ln = struct.unpack(">I", d[i:i + 4])[0]
        typ, data = d[i + 4:i + 8], d[i + 8:i + 8 + ln]
        if typ == b"IHDR":
            w, h, bd, ct = struct.unpack(">IIBB", data[:10])
        elif typ == b"IDAT":
            idat += data
        elif typ == b"IEND":
            break
        i += 12 + ln
    if (ct, bd) != (2, 8):
        raise SystemExit(f"{path}: expected 8-bit truecolour, got colour type "
                         f"{ct} depth {bd}")
    raw, bpp = zlib.decompress(idat), 3
    stride, out, prev, p = w * bpp, [], bytearray(w * bpp), 0
    for _ in range(h):
        f = raw[p]; p += 1
        line = bytearray(raw[p:p + stride]); p += stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 255
        out.append(bytes(line))
        prev = line
    return w, h, out


def main(argv):
    if len(argv) < 2:
        raise SystemExit("usage: count_colours.py SNAPSHOT.png [--expect N]")
    path = argv[1]
    expect = None
    if "--expect" in argv:
        expect = int(argv[argv.index("--expect") + 1])

    w, h, rows = png_pixels(path)

    # The border is a solid colour all the way round, so the content box is
    # whatever is NOT it along the middle row and column. Measuring the box
    # rather than assuming 640x200 is what caught the 2026-09-05 capture being
    # 704 wide with the content inset.
    def px(x, y):
        return rows[y][x * 3:x * 3 + 3]

    border = px(0, 0)
    xs = [x for x in range(w) if px(x, h // 2) != border]
    ys = [y for y in range(h) if px(w // 2, y) != border]
    if not xs or not ys:
        print(f"{path}: ENTIRE IMAGE IS ONE COLOUR #{border.hex()} -- "
              f"nothing was drawn")
        return 1

    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    cnt = collections.Counter()
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            cnt[px(x, y)] += 1

    print(f"image     {w}x{h}")
    print(f"border    #{border.hex()}")
    print(f"content   x={x0}..{x1} ({x1 - x0 + 1} px)  "
          f"y={y0}..{y1} ({y1 - y0 + 1} px)")
    print(f"distinct  {len(cnt)}")
    for c, n in cnt.most_common():
        print(f"    #{c.hex()}  {n}")

    if expect is not None and len(cnt) != expect:
        print(f"FAIL: expected {expect} distinct colours, found {len(cnt)}")
        return 1
    if expect is not None:
        print(f"OK: {expect} distinct colours as expected")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
