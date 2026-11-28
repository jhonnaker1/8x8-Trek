#!/usr/bin/env python3
"""Draw EGA Trek's application icon and write it as a NeXTSTEP icon TIFF.

    python3 tools/mkicon.py EGATrek.tiff

THE ARTWORK IS THIS PROJECT'S OWN, and the same design as the Mac's Finder
icon and the Amiga's Workbench icon (mac/tools/mkicon.py): the ship as the
game's own badge draws it -- a saucer, a neck and a hull -- on a white disc,
computed as ellipses with exact centres and radii so it is symmetrical
because it is computed to be. Here it is 48x48, NeXTSTEP's icon size, the
Mac's 32-pixel geometry scaled by 1.5, drawn four times over in each
direction and averaged down so the edges are smooth.

THE FILE IS LAID OUT AS NeXTSTEP 3.3's OWN ICONS ARE -- commodore-uno/next
read the TIFF in Edit.app's __ICON segment to get it right, and this writer
is that one's, with this artwork:
  1. 2-bit grey with a 2-bit alpha plane, planar: for greyscale NeXTs;
  2. 4-bit RGBA, one strip: for colour NeXTs.
Big-endian, uncompressed, premultiplied alpha, marked with NeXT's Matteing
tag (32995) rather than ExtraSamples. The build runs on the NeXT, which has
no Python, so the TIFF this writes is kept in the repository beside this
script; Makefile.next links it into the program.
"""
import struct
import sys

N = 48
SS = 4                      # supersampling
W = N * SS
K = 1.5                     # the Mac's 32-pixel geometry, scaled

BLACK = (0.0, 0.0, 0.0)
WHITE = (1.0, 1.0, 1.0)
BLUE = (0.0, 0.0, 10 / 15)  # EGA's blue, $0000AA

img = [[(0.0, 0.0, 0.0, 0.0) for _ in range(W)] for _ in range(W)]


def ellipse(cx, cy, rx, ry, colour):
    """Fill an ellipse given in the Mac's 32-pixel coordinates."""
    cx, cy, rx, ry = cx * K * SS, cy * K * SS, rx * K * SS, ry * K * SS
    for y in range(W):
        for x in range(W):
            if ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1.0:
                img[y][x] = colour + (1.0,)


def rect(l, t, r, b, colour):
    for y in range(int(t * K * SS), int(b * K * SS)):
        for x in range(int(l * K * SS), int(r * K * SS)):
            img[y][x] = colour + (1.0,)


ellipse(16, 16, 15.5, 15.5, BLACK)      # the disc's rim
ellipse(16, 16, 14.5, 14.5, WHITE)      # the disc, so it reads on any backdrop
ellipse(16, 11.5, 11.0, 3.0, BLACK)     # the saucer
ellipse(16, 21.0, 6.5, 2.4, BLACK)      # the hull
rect(15, 13, 17, 20, BLACK)             # the neck
ellipse(16, 11.5, 8.0, 0.9, BLUE)       # a band of windows

# average down to 48x48, premultiplying the colour by its coverage
pix = []
for y in range(N):
    row = []
    for x in range(N):
        r = g = b = a = 0.0
        for yy in range(y * SS, (y + 1) * SS):
            for xx in range(x * SS, (x + 1) * SS):
                pr, pg, pb, pa = img[yy][xx]
                r += pr * pa
                g += pg * pa
                b += pb * pa
                a += pa
        k = SS * SS
        row.append((r / k, g / k, b / k, a / k))
    pix.append(row)


def q(v, levels):
    return min(levels - 1, int(v * (levels - 1) + 0.5))


# image 1: 2-bit grey + 2-bit alpha, planar
def plane(values):
    out = bytearray()
    for row in values:
        byte = 0
        for i, v in enumerate(row):
            byte = (byte << 2) | v
            if i % 4 == 3:
                out.append(byte)
                byte = 0
    return bytes(out)


grey = [[q(min(0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2], p[3]), 4) for p in row] for row in pix]
alpha = [[q(p[3], 4) for p in row] for row in pix]
g_plane, a_plane = plane(grey), plane(alpha)

# image 2: 4-bit RGBA, one strip, two samples a byte
rgba = bytearray()
for row in pix:
    nib = []
    for p in row:
        a = q(p[3], 16)
        nib += [min(q(p[0], 16), a), min(q(p[1], 16), a), min(q(p[2], 16), a), a]
    for i in range(0, len(nib), 2):
        rgba.append((nib[i] << 4) | nib[i + 1])
rgba = bytes(rgba)

# --- write the TIFF ---
SHORT, LONG, RATIONAL = 3, 4, 5


def ifd(entries, next_off):
    """entries: (tag, type, count, value-or-offset), sorted by tag."""
    out = struct.pack(">H", len(entries))
    for tag, typ, count, val in sorted(entries):
        if typ == SHORT and count == 1:
            out += struct.pack(">HHIHH", tag, typ, count, val, 0)
        elif typ == SHORT and count == 2 and isinstance(val, tuple):
            out += struct.pack(">HHIHH", tag, typ, count, val[0], val[1])
        else:
            out += struct.pack(">HHII", tag, typ, count, val)
    return out + struct.pack(">I", next_off)


def ifd_size(n):
    return 2 + 12 * n + 4


data = bytearray(b"MM\x00\x2a\x00\x00\x00\x00")
off_g = len(data); data += g_plane
off_a = len(data); data += a_plane
off_rgba = len(data); data += rgba
if len(data) % 2:
    data += b"\x00"

off1_vals = len(data)
data += struct.pack(">II", off_g, off_a)                # +0 strip offsets
data += struct.pack(">II", len(g_plane), len(a_plane))   # +8 strip byte counts
data += struct.pack(">II", 72, 1)                        # +16 x resolution
data += struct.pack(">II", 72, 1)                        # +24 y resolution
off2_vals = len(data)
data += struct.pack(">HHHH", 4, 4, 4, 4)                 # +0 bits per sample
data += struct.pack(">II", 72, 1)                        # +8 x resolution
data += struct.pack(">II", 72, 1)                        # +16 y resolution

img1 = [
    (256, SHORT, 1, N), (257, SHORT, 1, N),
    (258, SHORT, 2, (2, 2)), (259, SHORT, 1, 1), (262, SHORT, 1, 1),
    (273, LONG, 2, off1_vals), (277, SHORT, 1, 2), (278, SHORT, 1, N),
    (279, LONG, 2, off1_vals + 8), (282, RATIONAL, 1, off1_vals + 16),
    (283, RATIONAL, 1, off1_vals + 24), (284, SHORT, 1, 2),
    (296, SHORT, 1, 2), (32995, SHORT, 1, 1),
]
img2 = [
    (256, SHORT, 1, N), (257, SHORT, 1, N),
    (258, SHORT, 4, off2_vals), (259, SHORT, 1, 1), (262, SHORT, 1, 2),
    (273, LONG, 1, off_rgba), (277, SHORT, 1, 4), (278, SHORT, 1, N),
    (279, LONG, 1, len(rgba)), (282, RATIONAL, 1, off2_vals + 8),
    (283, RATIONAL, 1, off2_vals + 16), (284, SHORT, 1, 1),
    (296, SHORT, 1, 2), (32995, SHORT, 1, 1),
]
ifd1_off = len(data)
ifd2_off = ifd1_off + ifd_size(len(img1))
data += ifd(img1, ifd2_off)
data += ifd(img2, 0)
data[4:8] = struct.pack(">I", ifd1_off)

with open(sys.argv[1], "wb") as f:
    f.write(data)
