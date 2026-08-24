#!/usr/bin/env python3
"""Dump the 8x8 font EGA Trek actually renders with.

NOT one font. Measured 2026-08-23, and the surprise is the point:

  digits, punctuation, space   -> the BIOS ROM CP437 8x8 font
  letters A-Z and a-z          -> a 464-byte table inside EGATREK.EXE

Every glyph on the "Welcome aboard Captain!" screen and the setup prompts was
compared against both sources. Letters matched the executable's table and not
the ROM; digits and punctuation matched the ROM and are not present anywhere in
the executable or in conventional memory. Two glyphs, 'N' and 'W', are
byte-identical in both and so match either.

NOTHING HERE IS HARD-CODED BY OFFSET. Both tables are found by pattern
matching a glyph whose bitmap is known, because both bases are unaligned and
would have been got wrong by assumption:

  ROM 8x8 font   0xC01F5   found via glyph 1, the CP437 smiley
  EXE letters    0x1B149   found via 'A'; the table starts AT 'A' and ends at
                           'z' -- below 0x41 and above 0x7A is x86 code

The ROM half needs a running dosbox-automation (the video BIOS is read through
/memory like any other address). The EXE half needs only reference/ on disk.

Output goes to build/ and is NEVER COMMITTED -- same rule as tools/gen_music.py.
The ROM half is DOSBox's CP437 table and the EXE half is lifted out of
Anderson's binary; both are regenerable from sources every user of this repo
has to fetch for themselves anyway.

    python3 tools/dump_font.py            # both halves, chart, preview PNG
    python3 tools/dump_font.py --exe-only # skips the emulator
"""
import os, sys, urllib.request

BASE  = os.environ.get("DOSBOX_API", "http://localhost:8386/api/v1")
TOKEN = os.environ.get("DOSBOX_API_TOKEN", "")
OUT   = os.environ.get("FONT_OUT", "build")

UNPACKED = "reference/EGATREK_unpacked.exe"

VIDEO_ROM   = 0xC0000
VIDEO_ROM_N = 32768

# Glyph 1 of the CP437 8x8 set, the smiling face. Distinctive, and exactly
# eight bytes, so it cannot collide with the 8x14 set's version of the same
# glyph (which is fourteen bytes with two blank rows top and bottom).
ROM_PROBE_CH  = 1
ROM_PROBE_BITS = bytes([0x7E, 0x81, 0xA5, 0x81, 0xBD, 0x99, 0x81, 0x7E])

# 'A' as EGA Trek draws it. Read off a 640x350 capture of the title screen,
# not taken from any font -- this is the measurement that anchors the table.
EXE_PROBE_CH   = ord('A')
EXE_PROBE_BITS = bytes([0x38, 0x6C, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0x00])

EXE_FIRST, EXE_LAST = ord('A'), ord('z')


def read_mem(off, length):
    req = urllib.request.Request("%s/memory/%d/%d" % (BASE, off, length),
                                 headers={"Authorization": "Bearer " + TOKEN})
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read()


def find_base(blob, probe_bits, probe_ch, what):
    """Locate a table by one known glyph. Refuses an ambiguous answer."""
    hits, i = [], -1
    while True:
        i = blob.find(probe_bits, i + 1)
        if i < 0:
            break
        hits.append(i)
    if not hits:
        sys.exit("%s: probe glyph not found -- the source is not what we think" % what)
    if len(hits) > 1:
        sys.exit("%s: probe glyph found %d times at %s -- ambiguous, pick a "
                 "better probe rather than guessing" % (what, len(hits),
                 [hex(h) for h in hits]))
    return hits[0] - probe_ch * 8


def dump_rom():
    rom = b"".join(read_mem(VIDEO_ROM + o, 4096)
                   for o in range(0, VIDEO_ROM_N, 4096))
    if rom[0] != 0x55 or rom[1] != 0xAA:
        sys.exit("no 55AA signature at C0000 -- that is not a video BIOS")
    base = find_base(rom, ROM_PROBE_BITS, ROM_PROBE_CH, "ROM 8x8 font")
    font = rom[base:base + 256 * 8]
    # Three checks that would each catch a base off by a glyph.
    assert font[0:8] == b"\0" * 8,        "glyph 0 should be blank"
    assert font[0xDB * 8:0xDB * 8 + 8] == b"\xFF" * 8, "0xDB should be a solid block"
    assert font[0xB1 * 8:0xB1 * 8 + 8] == bytes.fromhex("55aa55aa55aa55aa"), \
        "0xB1 should be the 50% dither"
    return base, font


def dump_exe():
    if not os.path.exists(UNPACKED):
        sys.exit("%s missing -- see NOTES.md 'Reference material'" % UNPACKED)
    data = open(UNPACKED, "rb").read()
    base = find_base(data, EXE_PROBE_BITS, EXE_PROBE_CH, "EGATREK letter table")
    lo, hi = base + EXE_FIRST * 8, base + (EXE_LAST + 1) * 8
    # The table starts at 'A' and ends at 'z': what sits just outside is x86
    # code, not glyphs, so a wider dump would ship instructions as bitmaps.
    return base, data[lo:hi]


def chart(font, first=0, last=255, cols=8):
    rows = []
    for start in range(first, last + 1, cols):
        block = [c for c in range(start, min(start + cols, last + 1))]
        head = []
        for c in block:
            name = chr(c) if 32 <= c < 127 else " "
            head.append(("0x%02X %s" % (c, name)).ljust(8))
        rows.append("      " + "  ".join(head))
        for y in range(8):
            line = []
            for c in block:
                b = font[c * 8 + y]
                line.append("".join("#" if b & (0x80 >> k) else "." for k in range(8)))
            rows.append("      " + "  ".join(s.ljust(8) for s in line))
        rows.append("")
    return "\n".join(rows)


def preview(rendered, path):
    try:
        from PIL import Image
    except ImportError:
        return None
    im = Image.new("RGB", (16 * 9 + 1, 16 * 9 + 1), (0, 0, 170))
    px = im.load()
    for c in range(256):
        ox, oy = (c % 16) * 9 + 1, (c // 16) * 9 + 1
        for y in range(8):
            b = rendered[c * 8 + y]
            for k in range(8):
                if b & (0x80 >> k):
                    px[ox + k, oy + y] = (170, 170, 170)
    im.resize((im.width * 4, im.height * 4), Image.NEAREST).save(path)
    return path


def main():
    os.makedirs(OUT, exist_ok=True)
    exe_only = "--exe-only" in sys.argv

    eb, letters = dump_exe()
    print("EGATREK letter table: base 0x%05X in %s" % (eb, UNPACKED))
    print("  covers 0x%02X..0x%02X ('%s'..'%s'), %d glyphs, %d bytes"
          % (EXE_FIRST, EXE_LAST, chr(EXE_FIRST), chr(EXE_LAST),
             EXE_LAST - EXE_FIRST + 1, len(letters)))
    open(os.path.join(OUT, "egatrek_letters.bin"), "wb").write(letters)

    if exe_only:
        print("\n--exe-only: skipping the ROM half")
        return

    rb, rom = dump_rom()
    print("ROM CP437 8x8 font:   base 0x%05X (offset 0x%04X into the video BIOS)"
          % (VIDEO_ROM + rb, rb))
    print("  256 glyphs, 2048 bytes, all three sanity checks pass")
    open(os.path.join(OUT, "cp437_8x8.bin"), "wb").write(rom)

    # The font as EGA Trek renders it: CP437 with the letters overlaid.
    rendered = bytearray(rom)
    rendered[EXE_FIRST * 8:(EXE_LAST + 1) * 8] = letters
    open(os.path.join(OUT, "egatrek_8x8.bin"), "wb").write(bytes(rendered))

    same = sum(1 for c in range(EXE_FIRST, EXE_LAST + 1)
               if rom[c * 8:c * 8 + 8] == rendered[c * 8:c * 8 + 8])
    print("\nOf the %d letter slots, %d are byte-identical in both sources:"
          % (EXE_LAST - EXE_FIRST + 1, same))
    print("  " + " ".join(chr(c) for c in range(EXE_FIRST, EXE_LAST + 1)
                          if rom[c * 8:c * 8 + 8] == rendered[c * 8:c * 8 + 8]))

    txt = os.path.join(OUT, "egatrek_8x8.txt")
    with open(txt, "w") as f:
        f.write("EGA Trek's 8x8 font: CP437 from the video BIOS, letters from "
                "EGATREK.EXE\n\n")
        f.write(chart(rendered))
    print("\nwrote %s/cp437_8x8.bin, %s/egatrek_letters.bin, "
          "%s/egatrek_8x8.bin, %s" % (OUT, OUT, OUT, txt))
    p = preview(rendered, os.path.join(OUT, "egatrek_8x8.png"))
    print("wrote %s" % p if p else "(no PIL -- skipped the preview PNG)")


if __name__ == "__main__":
    main()
