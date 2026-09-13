#!/usr/bin/env python3
"""Render what the V9958 is actually displaying, as a PNG.

MAME DRAWS THE SUPERSPRITE'S SCREEN BLACK no matter what the VDP is doing --
the window shows the CoCo's own VDG output, so a snapshot taken while the game
is running shows Disk BASIC's `EXEC` and nothing else. Every check in this port
has therefore gone through VRAM read-back, which proves bytes and shows
NOBODY THE PICTURE.

That is a real gap and not a small one: six of the seven ports were played and
looked at before release, and on this project a person watching the screen has
beaten the instruments seven times. This does not replace that -- it is not
interactive and it cannot notice that a panel reads wrong -- but it turns "the
bytes are right" into a frame somebody can look at.

HOW: MAME exposes the v9958 device's own `vram` address space, so the frame
comes straight out of the chip rather than through the $FF78 port (a debugger
read of that port would not auto-increment, and driving it from Lua would
fight the running game for the address register).

THE FORMAT IS GRAPHIC6: 512x212, sixteen colours, four bits a pixel, 256 bytes
a line, high nibble leftmost -- 54,272 bytes from VRAM 0.

THE PALETTE IS PARSED OUT OF coco3vid.c so it cannot drift from what the port
actually programs. Two bytes a colour, 0RRR0BBB then 00000GGG, three bits a
channel.
"""
import argparse, os, re, subprocess, sys, tempfile

HERE  = os.path.dirname(os.path.abspath(__file__))
COCO3 = os.path.dirname(HERE)
MAME  = os.path.expanduser("~/ample/Ample.app/Contents/MacOS/mame64")
ROMS  = os.path.expanduser("~/Library/Application Support/Ample/roms")

W, H, STRIDE = 512, 212, 256
VRAM_BYTES = H * STRIDE
CHIP_BYTES = 0x20000        # the whole chip: the logical screen is interleaved


def logical(raw, a):
    """Logical (CPU-visible) VRAM address -> the byte MAME's vram space holds.

    GRAPHIC6 AND GRAPHIC7 INTERLEAVE VRAM ACROSS TWO BANKS: bit 0 of the
    logical address picks the bank and the rest is the offset within it, so
    logical A lives at physical (A >> 1) | ((A & 1) << 16). MAME's `vram`
    space is the PHYSICAL layout.

    THIS TOOL READ IT LINEARLY FIRST AND NEARLY REPORTED A PORT BUG. The
    picture came out as "EGA TREK" drawn twice side by side, half width,
    stopping halfway down -- exactly what a real stride error looks like, and
    the address arithmetic in coco3vid.c was the first place I went looking.
    What gave it away was that the two halves of every line were IDENTICAL
    while content spanned the full 256 bytes, which no single write pass can
    produce. De-interleaved, the same dump is one clean title screen.
    """
    return raw[(a >> 1) | ((a & 1) << 16)]

LUA = r'''
local at   = tonumber(os.getenv("AT"))
local keys = tonumber(os.getenv("KEYS"))
emu.wait(14)
local kbd = manager.machine.natkeyboard
kbd.in_use = true
kbd:post_coded("CLEAR 25,&H6FFF{ENTER}")
emu.wait(3)
kbd:post_coded('LOADM"TREKLDR"{ENTER}')
emu.wait(8)
kbd:post_coded("EXEC{ENTER}")
emu.wait(at)

-- Dismiss the title and walk forward. The game waits for a key now.
-- FIND A KEY BY ITS LABEL rather than carrying a table: MAME's field names
-- start with the unshifted character ("a  A", "0", "ENTER"), so the first
-- token IS the key. A table here would be a third copy of the matrix.
local function keyfield(want)
    for r = 0, 6 do
        local port = manager.machine.ioport.ports[":row" .. r]
        for name, f in pairs(port.fields) do
            if name ~= "Keyboard" then
                local tok = name:match("^(%S+)")
                if tok and tok:lower() == want:lower() then return f end
            end
        end
    end
    error("vramshot: no key named " .. want)
end

local vram = manager.machine.devices[":ext:multi:slot1:ssfm:v9958"].spaces["vram"]
local film = os.getenv("FILM")
local shot = 0

-- ONLY THE BYTES THE SCREEN USES. GRAPHIC6 interleaves, so logical 0..54271
-- lives at physical 0..27135 and 65536..92671 -- half the reads of dumping
-- the whole chip, which matters when this runs once per keystroke.
local function grab(tag)
    if film == nil or film == "" then return end
    shot = shot + 1
    local f = io.open(string.format("%s/%03d-%s.bin", film, shot, tag), "wb")
    local c = {}
    for a = 0, 27135 do
        c[#c+1] = string.char(vram:read_u8(a))
        if #c == 8192 then f:write(table.concat(c)); c = {} end
    end
    for a = 65536, 92671 do
        c[#c+1] = string.char(vram:read_u8(a))
        if #c == 8192 then f:write(table.concat(c)); c = {} end
    end
    if #c > 0 then f:write(table.concat(c)) end
    f:close()
end

local function press(f, hold)
    f:set_value(1); emu.wait(0.15)
    f:set_value(0); emu.wait(hold)
end

local enter = keyfield("ENTER")
grab("start")
for i = 1, keys do press(enter, 2.0); grab("enter") end
-- `+N` IS A WAIT, AND IT IS NOT A CONVENIENCE. This port has no keyboard
-- buffer: kb_waitkey() polls the PIA matrix, so a key pressed while the game
-- is busy is GONE. The console redraw is a 54K blit at 10.6 cycles a byte and
-- takes over ten seconds of emulated time, and a "save" typed into it
-- vanishes letter by letter with nothing on screen to show for it -- which is
-- precisely what Jamie reported seeing, and what cost item 49 and 50.
for want in string.gmatch(os.getenv("SEQ") or "", "[^,]+") do
    local secs = want:match("^%+(%d+)$")
    if secs then
        emu.wait(tonumber(secs))
        grab("wait" .. secs)
    else
        press(keyfield(want), 2.5)
        grab(want)
    end
end
-- LET THE SCREEN FINISH. An overlay load off a floppy plus a full redraw is
-- seconds, and catching it half-drawn reports a black frame as a dead port.
emu.wait(tonumber(os.getenv("SETTLE")))

local vdp = manager.machine.devices[":ext:multi:slot1:ssfm:v9958"]
local vram = vdp.spaces["vram"]
local f = io.open(os.getenv("RAWF"), "wb")
local chunk = {}
for a = 0, tonumber(os.getenv("NBYTES")) - 1 do
    chunk[#chunk+1] = string.char(vram:read_u8(a))
    if #chunk == 4096 then f:write(table.concat(chunk)); chunk = {} end
end
if #chunk > 0 then f:write(table.concat(chunk)) end
f:close()
'''


def palette():
    """Straight out of the driver, so the picture cannot drift from the port."""
    src = open(os.path.join(COCO3, "src", "coco3vid.c")).read()
    m = re.search(r"ega_pal\[16\]\[2\]\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        sys.exit("vramshot: no ega_pal[16][2] in coco3vid.c")
    vals = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1))]
    if len(vals) != 32:
        sys.exit("vramshot: ega_pal has %d bytes, expected 32" % len(vals))
    out = []
    for i in range(16):
        b0, b1 = vals[2 * i], vals[2 * i + 1]
        r, b, g = (b0 >> 4) & 7, b0 & 7, b1 & 7
        out.append((r * 255 // 7, g * 255 // 7, b * 255 // 7))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", nargs="?", default=os.path.join(COCO3, "build", "screen.png"))
    ap.add_argument("--at", type=float, default=40.0,
                    help="emulated seconds to wait after EXEC before shooting")
    ap.add_argument("--keys", type=int, default=0,
                    help="ENTER presses before the shot, to walk past the title")
    ap.add_argument("--seq", default="",
                    help="comma-separated keys to type first, e.g. ENTER,n,ENTER -- "
                         "a letter or digit, or a named key (ENTER SPACE UP DOWN)")
    ap.add_argument("--film", default="",
                    help="directory to write a FRAME AFTER EVERY KEY into -- "
                         "a black screen at the end says nothing about which "
                         "key caused it")
    ap.add_argument("--settle", type=float, default=6.0,
                    help="emulated seconds to let the screen finish drawing")
    a = ap.parse_args()

    disk = os.path.join(COCO3, "build", "trek.dsk")
    if not os.path.exists(disk):
        sys.exit("vramshot: missing %s -- run `make disk`" % disk)
    if not os.path.exists(MAME):
        print("vramshot: SKIPPED -- no MAME at %s" % MAME)
        return 0

    tmp = tempfile.mkdtemp(prefix="vramshot")
    lua, raw = (os.path.join(tmp, n) for n in ("run.lua", "vram.bin"))
    open(lua, "w").write(LUA)
    secs = int(30 + a.at + a.keys * 3 + len(a.seq.split(',')) * 4 + a.settle + 40)
    subprocess.run([MAME, "coco3", "-window", "-skip_gameinfo", "-rompath", ROMS,
                    "-ext", "multi", "-ext:multi:slot1", "ssfm",
                    "-ext:multi:slot4", "fdc", "-flop1", disk,
                    "-autoboot_script", lua, "-autoboot_delay", "1",
                    "-seconds_to_run", str(secs), "-nothrottle",
                    "-cfg_directory", tmp, "-snapshot_directory", tmp],
                   env=dict(os.environ, RAWF=raw, AT=str(a.at), FILM=a.film,
                            KEYS=str(a.keys), SETTLE=str(a.settle), SEQ=a.seq,
                            NBYTES=str(CHIP_BYTES)),
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
                   cwd=tmp, check=False)
    if not os.path.exists(raw):
        sys.exit("vramshot: the run produced no VRAM dump")
    # KEEP THE BYTES. A picture that looks wrong is a question about the
    # bytes, and re-running MAME to ask it costs a minute every time.
    keep = os.path.splitext(a.out)[0] + ".bin"
    open(keep, "wb").write(open(raw, "rb").read())
    d = open(raw, "rb").read()
    if len(d) != CHIP_BYTES:
        sys.exit("vramshot: got %d bytes, expected %d" % (len(d), CHIP_BYTES))
    d = bytes(logical(d, a) for a in range(VRAM_BYTES))

    pal = palette()
    from PIL import Image

    if a.film:
        import glob
        # THE TWO HALVES MUST BE INTERLEAVED BACK, not concatenated. The Lua
        # reads physical 0..27135 (every EVEN logical byte) and 65536..92671
        # (every ODD one); writing them one after the other and rendering that
        # gives the console drawn twice and shifted -- the same false picture
        # the whole-chip linear read gave before, in a new place.
        n = 0
        for fn in sorted(glob.glob(os.path.join(a.film, "*.bin"))):
            raw_f = open(fn, "rb").read()
            if len(raw_f) != VRAM_BYTES:
                continue
            half = VRAM_BYTES // 2
            fd = bytearray(VRAM_BYTES)
            fd[0::2] = raw_f[:half]
            fd[1::2] = raw_f[half:]
            fd = bytes(fd)
            im = Image.new("RGB", (W, H)); q = im.load()
            live = 0
            for y in range(H):
                row = fd[y * STRIDE:(y + 1) * STRIDE]
                for i, byte in enumerate(row):
                    if byte:
                        live += 1
                    q[i * 2, y] = pal[byte >> 4]
                    q[i * 2 + 1, y] = pal[byte & 15]
            png = fn[:-4] + ".png"
            im.save(png)
            n += 1
            print("   %-40s %5.1f%% ink" % (os.path.basename(png),
                                            100.0 * live / (H * STRIDE)))
        print("vramshot: %d film frames in %s" % (n, a.film))
    img = Image.new("RGB", (W, H))
    px = img.load()
    hist = [0] * 16
    for y in range(H):
        row = d[y * STRIDE:(y + 1) * STRIDE]
        for i, byte in enumerate(row):
            hi, lo = byte >> 4, byte & 15
            hist[hi] += 1
            hist[lo] += 1
            px[i * 2, y] = pal[hi]
            px[i * 2 + 1, y] = pal[lo]
    img.save(a.out)

    # SAY WHETHER THERE IS A PICTURE AT ALL. An all-zero frame is what a dead
    # driver and a black screen look like alike, and the whole point of this
    # tool is to stop reporting one as the other.
    # THE GUARD FOR THE MISTAKE ABOVE. Reading the wrong layout produces
    # lines whose two 128-byte halves are byte-identical even though content
    # spans the whole line -- a signature no drawing code makes. Checked
    # against non-empty lines only, because two empty halves match trivially.
    live = [y for y in range(H) if any(d[y * STRIDE:(y + 1) * STRIDE])]
    twins = sum(1 for y in live
                if d[y * STRIDE:y * STRIDE + 128] == d[y * STRIDE + 128:(y + 1) * STRIDE])
    if live and twins * 2 > len(live):
        print("vramshot: %d of %d non-empty lines are two identical halves -- "
              "THE VRAM LAYOUT IS BEING READ WRONG, not the port drawing wrong"
              % (twins, len(live)))
        return 1

    used = sum(1 for c in hist if c)
    print("vramshot: wrote %s  (%dx%d, %d of 16 colours used)" % (a.out, W, H, used))
    top = sorted(range(16), key=lambda i: -hist[i])[:4]
    for i in top:
        if hist[i]:
            print("   colour %2d rgb%-14s %6.2f%% of the frame"
                  % (i, str(pal[i]), 100.0 * hist[i] / (W * H)))
    if used <= 1:
        print("vramshot: the frame is ONE COLOUR -- that is not a picture")
        return 1
    return 0


sys.exit(main())
