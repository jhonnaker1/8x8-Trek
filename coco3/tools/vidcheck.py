#!/usr/bin/env python3
"""Run VIDTEST.BIN on a coco3 + SuperSprite FM+ under MAME and check the
answers it leaves in CPU RAM.

WHY A READBACK AND NOT A SCREENSHOT: MAME renders this card's screen BLACK
whatever the VDP is doing -- the window shows the CoCo's own VDG output -- so
looking at it proves nothing either way. VIDTEST draws a known pattern, reads
VRAM back through the card and parks the bytes at $2F00.

THE INSTRUMENT WAS CHECKED BEFORE THE DRIVER WAS. Write two values at two
addresses, read the first back: a latch returns the second, real VRAM returns
the first. It returns the first. Without that check every assertion below
could have been confirming a write buffer.
"""
import os, struct, subprocess, sys, tempfile

HERE  = os.path.dirname(os.path.abspath(__file__))
COCO3 = os.path.dirname(HERE)
MAME  = os.path.expanduser("~/ample/Ample.app/Contents/MacOS/mame64")
ROMS  = os.path.expanduser("~/Library/Application Support/Ample/roms")

# (label, offset, expected, mode) -- mode "ne" means "must not be this"
CHECKS = [
    ("'A' row 0, white",        0,  [0x0F, 0xFF, 0x00], "eq"),
    ("'A' row 3, white",        3,  [0xFF, 0xFF, 0xF0], "eq"),
    ("far corner (79,24)",      6,  [0x20, 0x00, 0x20], "eq"),
    ("reverse video inverts",   9,  [0xF0, 0x00, 0xFF], "eq"),
    ("box glyph 64",           12,  [0x11, 0x11, 0x11], "eq"),
    ("log round-trip",         15,  [0x11, 0x22, 0x33], "eq"),
    ("log at offset 100",      18,  [0xA5],             "eq"),
    ("log slot 0 undisturbed", 19,  [0x11],             "eq"),
    ("cell drawn before clear",20,  [0x00],             "ne"),
    ("cell gone after clear",  21,  [0x00],             "eq"),
    ("log survives scr_clear", 22,  [0x11],             "eq"),
]

LUA = r'''
local ld, ex
for a, b in io.open(os.getenv("ADDRF")):read("*a"):gmatch("(%d+) (%d+)") do
    ld, ex = tonumber(a), tonumber(b)
end
-- emu.wait() in the autoboot coroutine, NOT the debugger (its console freezes
-- MAME until the mouse moves) and NOT a frame notifier (which stops firing
-- after the first callback that does real work).
emu.wait(12)                  -- Disk BASIC needs twelve seconds, not three
local cpu  = manager.machine.devices[":maincpu"]
local prog = cpu.spaces["program"]
local f = io.open(os.getenv("RAWF"), "rb"); local img = f:read("*a"); f:close()
for i = 1, #img do prog:write_u8(ld + i - 1, img:byte(i)) end
cpu.state["S"].value  = 0x3F00
cpu.state["PC"].value = ex
emu.wait(15)
local out = {}
for i = 0, 31 do out[#out + 1] = string.format("%02X", prog:read_u8(0x2F00 + i)) end
local o = io.open(os.getenv("OUTF"), "w"); o:write(table.concat(out, " ")); o:close()
'''


def decb_split(path):
    """A DECB binary is a 5-byte header, the body, then a 5-byte tail whose
    last two bytes are the exec address."""
    d = open(path, "rb").read()
    if d[0] != 0:
        sys.exit("vidcheck: %s is not a DECB data block" % path)
    n  = (d[1] << 8) | d[2]
    ld = (d[3] << 8) | d[4]
    ex = (d[5 + n + 3] << 8) | d[5 + n + 4]
    return d[5:5 + n], ld, ex


def main():
    binp = os.path.join(COCO3, "build", "VIDTEST.BIN")
    if not os.path.exists(binp):
        sys.exit("vidcheck: no build/VIDTEST.BIN -- run `make vidtest` first")
    if not os.path.exists(MAME):
        print("vidcheck: SKIPPED -- no MAME at %s" % MAME)
        return 0

    body, ld, ex = decb_split(binp)
    tmp = tempfile.mkdtemp(prefix="vidcheck")
    raw, adr, lua, out = (os.path.join(tmp, n) for n in
                          ("vidtest.raw", "addr.txt", "run.lua", "out.txt"))
    open(raw, "wb").write(body)
    open(adr, "w").write("%d %d" % (ld, ex))
    open(lua, "w").write(LUA)

    env = dict(os.environ, ADDRF=adr, RAWF=raw, OUTF=out)
    # -window ALWAYS: without it MAME takes the whole screen away from whoever
    # is at the keyboard, and a runaway headless run is then a fight.
    subprocess.run([MAME, "coco3", "-window", "-rompath", ROMS,
                    "-ext", "multi", "-ext:multi:slot1", "ssfm",
                    "-ext:multi:slot4", "fdc",
                    "-autoboot_script", lua, "-autoboot_delay", "1",
                    "-seconds_to_run", "35", "-nothrottle"],
                   env=env, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
                   cwd=tmp, check=False)

    if not os.path.exists(out):
        sys.exit("vidcheck: the run produced no output -- MAME did not reach the dump")
    b = [int(x, 16) for x in open(out).read().split()]

    if b[31] != 0x5A:
        sys.exit("vidcheck: VIDTEST did not run to completion "
                 "(sentinel $2F1F = %02X, not 5A) -- results below are meaningless" % b[31])

    bad = 0
    for label, off, exp, mode in CHECKS:
        got = b[off:off + len(exp)]
        ok  = (got == exp) if mode == "eq" else (got != exp)
        bad += 0 if ok else 1
        print("  %-4s %-26s %-11s %s %s" % (
            "ok" if ok else "FAIL", label,
            " ".join("%02X" % v for v in got),
            "==" if mode == "eq" else "!=",
            " ".join("%02X" % v for v in exp)))

    print("vidcheck: %d of %d ok" % (len(CHECKS) - bad, len(CHECKS)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
