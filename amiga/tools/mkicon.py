#!/usr/bin/env python3
"""Write egatrek.info -- the Workbench icon -- from artwork authored here.

Jamie ran the port on a real Amiga by unzipping it to the hard drive and
starting it from the directory. Without a `.info` file Workbench does not show
a program at all unless you turn on Show All Files, so the game was there and
invisible. This is the file that makes it an icon you can double-click.

THE ARTWORK IS THIS PROJECT'S OWN, drawn below, and that is the same rule the
box-drawing glyphs and the Atari font follow: nothing is lifted out of anyone's
Workbench. `--sheet` prints it back, because artwork that cannot be reviewed
cannot be corrected.

THE FORMAT is a DiskObject followed by one Image and its planes, all
big-endian: 78 bytes of DiskObject, 20 of Image, then two bitplanes of
ceil(width/16) words per row. Workbench 1.x reads it and so does every later
one. `--dump` reads the file back and prints what it finds, so a structure
that is subtly wrong is caught here rather than by a machine that simply does
not show the icon.

Colours are the Workbench four: 0 grey, 1 black, 2 white, 3 blue.
"""
import argparse
import struct
import sys

# '.' grey   '#' black   'o' white   '*' blue
#
# DRAWN RATHER THAN TYPED. The first version was hand-laid ASCII and read as a
# blob at icon size -- forty-eight pixels is not enough for a shape that is
# only approximately right, and rows of hand-counted dots drift. These are
# ellipses with an exact centre and radius, so the saucer is symmetrical
# because it is computed to be.
W, H = 48, 23


def _art():
    g = [["o"] * W for _ in range(H)]                   # white ground

    def ellipse(cx, cy, rx, ry, pen):
        for y in range(H):
            for x in range(W):
                if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0:
                    g[y][x] = pen

    ellipse(24, 7.0, 15.0, 3.2, "#")                    # the saucer
    ellipse(24, 15.0, 9.0, 2.6, "#")                    # the hull
    for y in range(9, 14):                              # the neck
        for x in range(22, 26):
            g[y][x] = "#"
    ellipse(24, 7.0, 11.0, 1.0, "*")                    # a band of windows

    for y in range(H):                                  # the frame
        g[y][0] = g[y][W - 1] = "#"
    for x in range(W):
        g[0][x] = g[H - 1][x] = "#"
    for y in (1, H - 2):
        for x in range(1, W - 1):
            if g[y][x] == "o":
                g[y][x] = "."
    for y in range(1, H - 1):
        for x in (1, W - 2):
            if g[y][x] == "o":
                g[y][x] = "."

    for (sx, sy) in ((5, 4), (42, 4), (5, 18), (42, 18)):   # four stars
        g[sy][sx] = "*"
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            if g[sy + dy][sx + dx] == "o":
                g[sy + dy][sx + dx] = "*"
    return ["".join(r) for r in g]


ART = _art()
PEN = {".": 0, "#": 1, "o": 2, "*": 3}

MAGIC, VERSION = 0xE310, 1
WBDRAWER = 2
WBTOOL = 3
NO_ICON_POSITION = 0x80000000
GFLG_GADGIMAGE = 0x0004
GACT_RELVERIFY_IMMEDIATE = 0x0003
GTYP_BOOLGADGET = 0x0001


def planes(art, width, height, depth):
    """Planar bitmap: one bit per pixel per plane, rows padded to 16 bits."""
    words = (width + 15) // 16
    out = bytearray()
    for p in range(depth):
        for y in range(height):
            row = art[y] if y < len(art) else ""
            for w in range(words):
                v = 0
                for b in range(16):
                    x = w * 16 + b
                    pen = PEN.get(row[x], 0) if x < len(row) else 0
                    if pen >> p & 1:
                        v |= 1 << (15 - b)
                out += struct.pack(">H", v)
    return bytes(out)


def drawer_data():
    """A drawer's own 56 bytes: a NewWindow describing the window Workbench
    opens for it, then the scroll position. Written out because a TOOL icon
    alone is useless -- Workbench will not show the DRAWER either without one,
    so the game would still be invisible and the icon unreachable."""
    new_window = struct.pack(
        ">hhhh BB I I I I I I I hhhh H",
        40, 40, 320, 100,        # LeftEdge, TopEdge, Width, Height
        0, 1,                    # DetailPen, BlockPen
        0, 0,                    # IDCMPFlags, Flags
        0, 0, 0, 0, 0,           # FirstGadget, CheckMark, Title, Screen, BitMap
        90, 40, 640, 200,        # Min/Max width and height
        1)                       # Type -- WBENCHSCREEN
    assert len(new_window) == 48, len(new_window)
    return new_window + struct.pack(">ii", 0, 0)      # CurrentX, CurrentY


def build(stack=8192, drawer=False):
    height = len(ART)
    width = max(len(r) for r in ART)
    if any(len(r) != width for r in ART):
        raise SystemExit("every row of ART must be the same width; "
                         f"got {sorted({len(r) for r in ART})}")
    bad = sorted({c for r in ART for c in r} - set(PEN))
    if bad:
        raise SystemExit(f"ART uses characters with no pen: {bad}")
    depth = 2

    gadget = struct.pack(
        ">I hh hh HHH I I I i I H I",
        0,                       # NextGadget
        0, 0,                    # LeftEdge, TopEdge
        width, height,
        GFLG_GADGIMAGE, GACT_RELVERIFY_IMMEDIATE, GTYP_BOOLGADGET,
        1,                       # GadgetRender -- non-NULL: an image follows
        0,                       # SelectRender -- none, so no second image
        0, 0, 0,                 # GadgetText, MutualExclude, SpecialInfo
        0,                       # GadgetID
        0)                       # UserData
    assert len(gadget) == 44, len(gadget)

    disk = struct.pack(">HH", MAGIC, VERSION) + gadget + struct.pack(
        ">BB I I II I I i",   # CurrentX/Y are UNSIGNED: NO_ICON_POSITION
                              # is $80000000 and does not fit a signed long
        WBDRAWER if drawer else WBTOOL, 0,
        0,                       # DefaultTool -- a tool runs itself
        0,                       # ToolTypes
        NO_ICON_POSITION, NO_ICON_POSITION,
        1 if drawer else 0,      # DrawerData -- non-NULL means 56 bytes follow
        0,                       # ToolWindow
        stack)
    assert len(disk) == 78, len(disk)
    if drawer:
        disk += drawer_data()

    image = struct.pack(">hh hhh I BB I",
                        0, 0, width, height, depth,
                        1,           # ImageData -- non-NULL, data follows
                        (1 << depth) - 1, 0,
                        0)           # NextImage
    assert len(image) == 20, len(image)

    return disk + image + planes(ART, width, height, depth), width, height


def dump(path):
    """Read it back. A structure that is subtly wrong is caught HERE, not by a
    machine that simply declines to show an icon and says nothing."""
    d = open(path, "rb").read()
    # OFFSETS, AND THE FIRST VERSION OF THIS READER HAD THEM WRONG -- it read
    # Width at 10 and do_Type at 46, reported "NOT a tool" for a correct file,
    # and the disagreement was the READER's. Laid out here so the next reading
    # is against the structure rather than against arithmetic done twice:
    #   0 magic  2 version  4 Gadget[44]  48 do_Type  49 pad
    #   50 DefaultTool  54 ToolTypes  58 CurrentX  62 CurrentY
    #   66 DrawerData  70 ToolWindow  74 StackSize  78 Image[20]  98 planes
    # and inside the Gadget: 8 LeftEdge 10 TopEdge 12 Width 14 Height.
    magic, ver = struct.unpack(">HH", d[:4])
    w, h = struct.unpack(">hh", d[12:16])
    typ = d[48]
    stack = struct.unpack(">i", d[74:78])[0]
    off = 78 + (56 if typ == WBDRAWER else 0)        # a drawer inserts 56
    iw, ih, idep = struct.unpack(">hhh", d[off + 4:off + 10])
    words = (iw + 15) // 16
    need = off + 20 + words * 2 * ih * idep
    print(f"  magic     ${magic:04X} {'ok' if magic == MAGIC else 'WRONG'}"
          f"   version {ver}")
    kind = ("(WBTOOL)" if typ == WBTOOL else
            "(WBDRAWER)" if typ == WBDRAWER else "NEITHER tool nor drawer")
    print(f"  type      {typ} {kind}")
    print(f"  gadget    {w}x{h}     image {iw}x{ih} depth {idep}")
    print(f"  stack     {stack}")
    print(f"  size      {len(d)} bytes, structure needs {need}"
          f"   {'ok' if len(d) == need else 'MISMATCH'}")
    return 0 if (magic == MAGIC and typ in (WBTOOL, WBDRAWER) and len(d) == need
                 and (w, h) == (iw, ih)) else 1


# The Workbench four, as 1.x draws them.
WB_RGB = [(0xAA, 0xAA, 0xAA), (0x00, 0x00, 0x00),
          (0xFF, 0xFF, 0xFF), (0x66, 0x88, 0xBB)]


def render(path, out, scale=6):
    """Decode the PLANES back to a picture, which is the only way to check the
    encoder rather than the ASCII it was made from. Artwork that cannot be
    reviewed cannot be corrected -- coco3/tools/gen_font.py's rule."""
    import zlib
    d = open(path, "rb").read()
    typ = d[48]
    off = 78 + (56 if typ == WBDRAWER else 0)
    w, h, depth = struct.unpack(">hhh", d[off + 4:off + 10])
    words = (w + 15) // 16
    base = off + 20
    px = [[0] * w for _ in range(h)]
    for pl in range(depth):
        for y in range(h):
            for wd in range(words):
                off = base + ((pl * h + y) * words + wd) * 2
                v = struct.unpack(">H", d[off:off + 2])[0]
                for b in range(16):
                    x = wd * 16 + b
                    if x < w and (v >> (15 - b)) & 1:
                        px[y][x] |= 1 << pl
    rows = b""
    for y in range(h):
        for _ in range(scale):
            line = b"\x00"
            for x in range(w):
                line += bytes(WB_RGB[px[y][x]]) * scale
            rows += line
    def chunk(t, b):
        c = t + b
        return struct.pack(">I", len(b)) + c + struct.pack(">I", zlib.crc32(c))
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w * scale, h * scale, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(rows))
           + chunk(b"IEND", b""))
    open(out, "wb").write(png)
    print(f"{out}: {w*scale}x{h*scale}, decoded from the PLANES")
    return 0


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("out", nargs="?", default="build/egatrek.info")
    ap.add_argument("--sheet", action="store_true",
                    help="print the artwork back for review")
    ap.add_argument("--dump", metavar="FILE",
                    help="read an .info back and check its structure")
    ap.add_argument("--stack", type=int, default=8192)
    ap.add_argument("--drawer", action="store_true",
                    help="a DRAWER icon rather than a tool")
    ap.add_argument("--png", metavar="OUT",
                    help="decode an .info's planes back to a picture")
    a = ap.parse_args(argv[1:])

    if a.png:
        return render(a.out, a.png)
    if a.dump:
        return dump(a.dump)
    if a.sheet:
        for r in ART:
            print("  " + r)
        print(f"  {max(len(r) for r in ART)} x {len(ART)}, pens "
              f"{' '.join(sorted(PEN))}")
        return 0

    data, w, h = build(a.stack, a.drawer)
    open(a.out, "wb").write(data)
    print(f"{a.out}: {len(data)} bytes, {w}x{h}, 2 planes, stack {a.stack}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
