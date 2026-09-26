#!/usr/bin/env python3
"""Render SCREEN 7 (GRAPHIC6) out of a VRAM dump, independent of any renderer.

openMSX's screenshot needs a renderer, and under throttle off it LAGS VRAM by
tens of seconds (NOTES, the video driver). VRAM is the truth: 512x212, 4 bits a
pixel, high nibble first, 256 bytes a line; the palette is the V9938's own 16
entries of 0RRR0BBB 00000GGG, dumped beside it. Pixels are doubled vertically
so the 512x212 raster keeps the machine's aspect.

Usage: vram2png.py vram.bin palette.bin out.png"""
import struct, sys, zlib

vram, pal, out = open(sys.argv[1], "rb").read(), open(sys.argv[2], "rb").read(), sys.argv[3]
rgb = []
for i in range(16):
    a, b = pal[2 * i], pal[2 * i + 1]
    r, bl, g = (a >> 4) & 7, a & 7, b & 7
    rgb.append(bytes((r * 255 // 7, g * 255 // 7, bl * 255 // 7)))
W, H = 512, 212
rows = []
for y in range(H):
    line = bytearray(b"\x00")
    for x in range(W // 2):
        v = vram[y * 256 + x]
        line += rgb[v >> 4] + rgb[v & 15]
    rows += [bytes(line), bytes(line)]
def chunk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, 2 * H, 8, 2, 0, 0, 0)) \
      + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")
open(out, "wb").write(png)
print(out)
