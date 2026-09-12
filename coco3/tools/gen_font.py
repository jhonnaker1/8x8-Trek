#!/usr/bin/env python3
"""Author the CoCo 3 port's 6x8 font and emit it as C.

WHY THIS PORT HAS TO DRAW ITS OWN. Every other target borrows the machine's
glyphs at runtime -- topaz on the Amiga, Line-A's 8x16 on the Falcon, the OS
ROM font on the Atari, the chargen ROM on the three Commodores. The CoCo 3 has
nothing to borrow: $8000..$FEFF rendered as a bitmap is all code, because the
GIME's character generator is internal silicon rather than a table the CPU can
read. And 80 columns in the V9958's 512 pixels forces SIX-pixel cells, which
no stock 8-wide font would fit into anyway.

SO THE SHAPE IS 5x7 IN A 6x8 CELL -- five columns of glyph plus one of
spacing, seven rows plus one of leading. That is the classic 8-bit font size,
including the CoCo's own text mode, so legibility is a solved problem here;
what was needed was the labour.

INDEXED BY SCREEN CODE, NOT ASCII, and that is what keeps it to 64 entries:
`scr_puts` folds lowercase onto the same screen codes as upper case, so no
lowercase glyph is ever drawn. Codes 64..127 are the box-drawing set, which
lives in the video driver beside the other ports' versions, and 128..255 are
reverse video, which is a rule rather than data.

Authored as pictures because that is the only form in which artwork can be
reviewed. `--sheet` renders a PNG to look at before trusting any of it.
"""
import sys

# Screen code -> 7 rows of 5 columns. '#' is ink.
G = {}

def g(code, *rows):
    assert len(rows) == 7, f"code {code}: {len(rows)} rows"
    for r in rows:
        assert len(r) == 5, f"code {code}: row {r!r} is {len(r)} wide"
    G[code] = rows

# 0 = '@'
g(0,  ".###.", "#...#", "#.###", "#.#.#", "#.###", "#....", ".###.")

# 1..26 = A..Z
g(1,  ".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#")
g(2,  "####.", "#...#", "#...#", "####.", "#...#", "#...#", "####.")
g(3,  ".###.", "#...#", "#....", "#....", "#....", "#...#", ".###.")
g(4,  "###..", "#..#.", "#...#", "#...#", "#...#", "#..#.", "###..")
g(5,  "#####", "#....", "#....", "####.", "#....", "#....", "#####")
g(6,  "#####", "#....", "#....", "####.", "#....", "#....", "#....")
g(7,  ".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####")
g(8,  "#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#")
g(9,  ".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###.")
g(10, "....#", "....#", "....#", "....#", "#...#", "#...#", ".###.")
g(11, "#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#")
g(12, "#....", "#....", "#....", "#....", "#....", "#....", "#####")
g(13, "#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#")
g(14, "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#")
g(15, ".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###.")
g(16, "####.", "#...#", "#...#", "####.", "#....", "#....", "#....")
g(17, ".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#")
g(18, "####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#")
g(19, ".####", "#....", "#....", ".###.", "....#", "....#", "####.")
g(20, "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#..")
g(21, "#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###.")
g(22, "#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#..")
g(23, "#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#")
g(24, "#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#")
g(25, "#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#..")
g(26, "#####", "....#", "...#.", "..#..", ".#...", "#....", "#####")

# 27 = '[', 29 = ']' -- the play-again box draws "[YES]" and "[NO]", which is
# how the Amiga found these two were needed at all.
g(27, ".###.", ".#...", ".#...", ".#...", ".#...", ".#...", ".###.")
g(29, ".###.", "...#.", "...#.", "...#.", "...#.", "...#.", ".###.")
# 28 is POUND and 30/31 are the arrows -- 28 is deliberately absent, as on the
# Amiga: an invented glyph for a character nobody asks for is worse than the
# missing-glyph marker. 30 and 31 are drawn because a code with a glyph cannot
# come out as a hollow box later.
g(30, "..#..", ".###.", "#.#.#", "..#..", "..#..", "..#..", ".....")
g(31, ".....", "..#..", ".#...", "#####", ".#...", "..#..", ".....")

# 32..63 are ASCII already
g(32, ".....", ".....", ".....", ".....", ".....", ".....", ".....")  # space
g(33, "..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#..")  # !
g(34, ".#.#.", ".#.#.", ".....", ".....", ".....", ".....", ".....")  # "
g(35, ".#.#.", ".#.#.", "#####", ".#.#.", "#####", ".#.#.", ".#.#.")  # #
g(36, "..#..", ".####", "#.#..", ".###.", "..#.#", "####.", "..#..")  # $
g(37, "##...", "##..#", "...#.", "..#..", ".#...", "#..##", "...##")  # %
g(38, ".##..", "#..#.", "#.#..", ".#...", "#.#.#", "#..#.", ".##.#")  # &
g(39, "..#..", "..#..", ".....", ".....", ".....", ".....", ".....")  # '
g(40, "...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#.")  # (
g(41, ".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#...")  # )
g(42, ".....", "#.#.#", ".###.", "#####", ".###.", "#.#.#", ".....")  # *
g(43, ".....", "..#..", "..#..", "#####", "..#..", "..#..", ".....")  # +
g(44, ".....", ".....", ".....", ".....", "..##.", "..##.", ".#...")  # ,
g(45, ".....", ".....", ".....", "#####", ".....", ".....", ".....")  # -
g(46, ".....", ".....", ".....", ".....", ".....", "..##.", "..##.")  # .
g(47, "....#", "...#.", "..#..", "..#..", ".#...", "#....", "#....")  # /
g(48, ".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###.")  # 0
g(49, "..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###.")  # 1
g(50, ".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####")  # 2
g(51, "#####", "...#.", "..##.", "....#", "....#", "#...#", ".###.")  # 3
g(52, "...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#.")  # 4
g(53, "#####", "#....", "####.", "....#", "....#", "#...#", ".###.")  # 5
g(54, "..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###.")  # 6
g(55, "#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#...")  # 7
g(56, ".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###.")  # 8
g(57, ".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##..")  # 9
g(58, ".....", "..##.", "..##.", ".....", "..##.", "..##.", ".....")  # :
g(59, ".....", "..##.", "..##.", ".....", "..##.", "..##.", ".#...")  # ;
g(60, "...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#.")  # <
g(61, ".....", ".....", "#####", ".....", "#####", ".....", ".....")  # =
g(62, ".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#...")  # >
g(63, ".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#..")  # ?


# ---------------------------------------------------------------- box set
#
# SIX WIDE AND EIGHT TALL -- the FULL cell, unlike the 5x7 text glyphs, because
# a rule has to reach the cell edges or adjacent cells will not join. The set
# and its codes are the Amiga's, re-derived there by sweeping every
# scr_put/hline/vline/fill_rect argument in the shared UI; that sweep found
# FIFTEEN where an eyeball count gives eleven. Taking the same LIST is
# deliberate. Taking the same BITMAPS is not possible -- these cells are two
# pixels narrower.
#
# RESTATED FOR 6x8, NOT SQUEEZED FROM 8x8: the rule is that a line sits on rows
# 3 and 4 and in columns 2 and 3, which is what puts a junction in the middle
# of the cell and makes two neighbours meet.

B = {}

def b(code, *rows):
    assert len(rows) == 8, f"box {code}: {len(rows)} rows"
    for r in rows:
        assert len(r) == 6, f"box {code}: row {r!r} is {len(r)} wide"
    B[code] = rows

HL, VL = "######", "..##.."
BL_    = "..####"      # left half of a horizontal, from the centre out
BR_    = "####.."

b(64,  "......", "......", "......", HL,     HL,     "......", "......", "......")  # G_HLINE
b(93,  VL,      VL,      VL,      VL,     VL,     VL,      VL,      VL)             # G_VLINE
b(112, "......", "......", "......", BL_,    BL_,    VL,      VL,      VL)           # G_TL  ,-
b(110, "......", "......", "......", BR_,    BR_,    VL,      VL,      VL)           # G_TR  -.
b(109, VL,      VL,      VL,      BL_,    BL_,    "......", "......", "......")      # G_BL  `-
b(125, VL,      VL,      VL,      BR_,    BR_,    "......", "......", "......")      # G_BR  -'
b(107, VL,      VL,      VL,      BL_,    BL_,    VL,      VL,      VL)              # G_TEE_L |-
b(115, VL,      VL,      VL,      BR_,    BR_,    VL,      VL,      VL)              # G_TEE_R -|
b(114, "......", "......", "......", HL,     HL,     VL,      VL,      VL)           # G_TEE_D T
b(113, VL,      VL,      VL,      HL,     HL,     "......", "......", "......")      # G_TEE_U _|_
b(91,  VL,      VL,      VL,      HL,     HL,     VL,      VL,      VL)              # G_CROSS +

# 98 BADGE_DISC_TOP -- the lower half filled, which rounds the top of the
# badge's disc. Its reverse, 226, is the bottom half and needs no entry.
b(98,  "......", "......", "......", "......", HL, HL, HL, HL)

# 100 the bottom row alone, and the ONLY reason it exists is that its REVERSE
# is 228, the systems-status bar: filled except for one row, so adjacent bars
# keep a hairline instead of merging. ui.c measured that off the original as
# "7px bars on an 8px pitch", which at this cell height is 7 on 8 exactly.
b(100, "......", "......", "......", "......", "......", "......", "......", HL)

# 81 the ship's saucer -- drawn, not copied. The badge and the info panel both
# put this beside four cells of G_HLINE and a solid block, so what it has to be
# is a round body that reads as a hull. Wider than tall, clear of the top and
# bottom rows so it does not merge with the cell above or below.
b(81,  "......", "......", ".####.", "######", "######", ".####.", "......", "......")


def box_bytes(rows):
    """Eight picture rows of six columns -> eight bytes, bit 5 leftmost."""
    out = []
    for r in rows:
        v = 0
        for i, c in enumerate(r):
            if c == '#':
                v |= 1 << (5 - i)
        out.append(v)
    return out


def rows_to_bytes(rows):
    """Seven picture rows -> eight bytes, bit 5 leftmost, an eighth blank row
    for leading. Six significant bits so a cell is six pixels wide."""
    out = []
    for r in rows:
        v = 0
        for i, c in enumerate(r):          # five columns, then one of spacing
            if c == '#':
                v |= 1 << (5 - i)
        out.append(v)
    out.append(0)
    return out


def emit_c():
    lines = []
    lines.append("/* GENERATED by coco3/tools/gen_font.py -- do not edit here.")
    lines.append(" * Author the pictures in that file and regenerate; it renders a proof")
    lines.append(" * sheet with --sheet, which is the only way artwork can be reviewed. */")
    lines.append("#ifndef COCO3_FONT_H")
    lines.append("#define COCO3_FONT_H")
    lines.append("")
    lines.append("/* Screen codes 0..63, eight rows each, SIX significant bits with bit 5")
    lines.append("   leftmost. Codes 64..127 are the box set in coco3vid.c; 128..255 are")
    lines.append("   reverse video, which is a rule and not data. */")
    lines.append("#define FONT_CELL_W 6")
    lines.append("#define FONT_CELL_H 8")
    lines.append("#define FONT_CODES  64")
    lines.append("")
    lines.append("static const unsigned char font6x8[FONT_CODES][FONT_CELL_H] = {")
    for c in range(64):
        rows = G.get(c)
        if rows is None:
            body = "0,0,0,0,0,0,0,0"
            note = "  /* %d -- deliberately absent, see gen_font.py */" % c
        else:
            body = ",".join("0x%02X" % b for b in rows_to_bytes(rows))
            note = "  /* %d */" % c
        lines.append("    {%s},%s" % (body, note))
    lines.append("};")
    lines.append("")
    lines.append("/* The box and badge set: SIX WIDE AND EIGHT TALL, the full cell, because a")
    lines.append("   rule has to reach the cell edges or neighbours will not join. Codes are")
    lines.append("   C128 screen codes. Reverse video is a RULE, not entries -- 160, 226 and")
    lines.append("   228 fall out of 32, 98 and 100 rather than being written again. */")
    lines.append("typedef struct { unsigned char code; unsigned char row[FONT_CELL_H]; } BoxGlyph;")
    lines.append("")
    lines.append("static const BoxGlyph font_box[] = {")
    for c in sorted(B):
        body = ",".join("0x%02X" % v for v in box_bytes(B[c]))
        lines.append("    {%3d, {%s}}," % (c, body))
    lines.append("};")
    lines.append("")
    lines.append("#define FONT_BOX_COUNT ((int)(sizeof font_box / sizeof font_box[0]))")
    lines.append("")
    lines.append("#endif")
    return "\n".join(lines) + "\n"


def box_sheet(path):
    from PIL import Image
    scale, cols = 6, 9
    W = cols * 8 * scale
    H = 2 * 12 * scale
    im = Image.new("RGB", (W, H), (16, 16, 24))
    px = im.load()
    for i, code in enumerate(sorted(B)):
        bx = (i % cols) * 8 * scale
        by = (i // cols) * 12 * scale
        for y, row in enumerate(B[code]):
            for x, ch in enumerate(row):
                if ch == '#':
                    for dy in range(scale):
                        for dx in range(scale):
                            px[bx + x*scale + dx, by + y*scale + dy] = (150, 220, 255)
    im.save(path)
    return im.size


def sheet(path):
    from PIL import Image
    cols, cell = 16, (6, 8)
    scale = 6
    W = cols * (cell[0] + 2) * scale
    H = 4 * (cell[1] + 4) * scale
    im = Image.new("RGB", (W, H), (16, 16, 24))
    px = im.load()
    for c in range(64):
        rows = G.get(c)
        if rows is None:
            continue
        bx = (c % cols) * (cell[0] + 2) * scale
        by = (c // cols) * (cell[1] + 4) * scale
        for y, b in enumerate(rows_to_bytes(rows)):
            for x in range(6):
                if b & (1 << (5 - x)):
                    for dy in range(scale):
                        for dx in range(scale):
                            px[bx + x*scale + dx, by + y*scale + dy] = (230, 230, 210)
    im.save(path)
    return im.size


if __name__ == "__main__":
    missing = [c for c in range(64) if c not in G and c != 28]
    if missing:
        sys.exit("gen_font: no glyph for screen codes %s" % missing)
    if "--box-sheet" in sys.argv:
        print("box sheet:", box_sheet(sys.argv[sys.argv.index("--box-sheet") + 1]))
    elif "--sheet" in sys.argv:
        print("proof sheet:", sheet(sys.argv[sys.argv.index("--sheet") + 1]))
    else:
        sys.stdout.write(emit_c())
