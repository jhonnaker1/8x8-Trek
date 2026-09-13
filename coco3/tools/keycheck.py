#!/usr/bin/env python3
"""Press keys on the machine and check what the driver made of them.

coco3input.c carries a 56-entry table. A table can be plausibly wrong in ONE
cell and the only symptom is a player unable to type one letter of a name --
which is exactly how the C128's input bugs were found, late and by a person.
So this presses twenty-four keys through MAME's own keyboard fields and
compares kb_waitkey()'s answer against the ASCII c128/src/input.h names.

IT DRIVES THE IOPORT FIELDS, NOT THE NATURAL KEYBOARD. natkeyboard would test
MAME's ASCII-to-matrix translation as much as the driver's matrix-to-ASCII;
holding the field is the machine's side of the keyboard and nothing else.

THE SHIFTED THREE ARE THE POINT OF THE EXERCISE. On this keyboard ':' ';' and
'-' are one key each with '*' '+' and '=' above them, which is the opposite of
the C128, where all eight punctuation keys are unshifted. Those three are the
cells most likely to be wrong and the least likely to be noticed.
"""
import os, struct, subprocess, sys, tempfile

HERE  = os.path.dirname(os.path.abspath(__file__))
COCO3 = os.path.dirname(HERE)
MAME  = os.path.expanduser("~/ample/Ample.app/Contents/MacOS/mame64")
ROMS  = os.path.expanduser("~/Library/Application Support/Ample/roms")

# (row port, field name, shift held, expected ASCII, what it is)
KEYS = [
    (0, "@",            0, 64, "@"),
    (0, "a  A",         0, 65, "A"),
    (0, "g  G",         0, 71, "G"),
    (1, "h  H",         0, 72, "H"),
    (2, "p  P",         0, 80, "P"),
    (3, "z  Z",         0, 90, "Z"),
    (3, "UP",           0,  1, "KB_UP"),
    (3, "DOWN",         0,  2, "KB_DOWN"),
    (3, "LEFT",         0, 20, "KB_DELETE (this machine's backspace)"),
    (3, "SPACE",        0, 32, "space"),
    (4, "0",            0, 48, "0"),
    (4, "7  '  ^",      0, 55, "7"),
    (5, "8  (  [",      0, 56, "8"),
    (5, "9  )  ]",      0, 57, "9"),
    (5, ",  <  {",      0, 44, ","),
    (5, "-  =  _",      0, 45, "-"),
    (5, ".  >  }",      0, 46, "."),
    (5, "/  ?  \\",     0, 47, "/"),
    (5, ":  *",         0, 58, ":"),
    (5, ";  +",         0, 59, ";"),
    (6, "ENTER",        0, 13, "KB_RETURN"),
    (6, "BREAK",        0, 27, "KB_ESC"),
    (5, ":  *",         1, 42, "SHIFT+: is *"),
    (5, ";  +",         1, 43, "SHIFT+; is +"),
    (5, "-  =  _",      1, 61, "SHIFT+- is ="),
    (0, "b  B",         0, 66, "B"),
]

LUA = r'''
local ld, ex
for a, b in io.open(os.getenv("ADDRF")):read("*a"):gmatch("(%d+) (%d+)") do
    ld, ex = tonumber(a), tonumber(b)
end
emu.wait(12)
-- PAUSE BEFORE POKING: Disk BASIC is live and this image lands at $2800.
pcall(function() manager.machine:pause() end)
local cpu  = manager.machine.devices[":maincpu"]
local prog = cpu.spaces["program"]
local f = io.open(os.getenv("RAWF"), "rb"); local img = f:read("*a"); f:close()
for i = 1, #img do prog:write_u8(ld + i - 1, img:byte(i)) end
cpu.state["S"].value  = 0x3F00
cpu.state["PC"].value = ex
pcall(function() manager.machine:resume() end)
emu.wait(1)

local shift = manager.machine.ioport.ports[":row6"].fields["SHIFT"]
for line in io.lines(os.getenv("KEYSF")) do
    local rt, sh, name = line:match("^(%d) (%d) (.*)$")
    local fld = manager.machine.ioport.ports[":row" .. rt].fields[name]
    -- SHIFT GOES DOWN FIRST AND COMES UP LAST, which is both what a human
    -- does and the only way this can test what it means to. Pressing SHIFT
    -- and the key in the SAME instant races: the driver samples the matrix
    -- once per scan and reports what it finds, so whichever of the two
    -- MAME's port update reaches first decides the answer, and the result
    -- moved with the hold time. That is not a driver bug -- a real keyboard
    -- where SHIFT arrives after the key gives the unshifted character too --
    -- it is a harness that was pressing two keys at once and calling the
    -- outcome a table error.
    if sh == "1" then shift:set_value(1); emu.wait(0.10) end
    fld:set_value(1)
    emu.wait(0.25)
    fld:set_value(0)
    emu.wait(0.05)
    if sh == "1" then shift:set_value(0) end
    emu.wait(0.25)
end

emu.wait(1)
local out = {}
for i = 0, 31 do out[#out + 1] = string.format("%02X", prog:read_u8(0x7F00 + i)) end
local o = io.open(os.getenv("OUTF"), "w"); o:write(table.concat(out, " ")); o:close()
'''


def decb_split(path):
    d = open(path, "rb").read()
    t, ln, ad = struct.unpack(">BHH", d[:5])
    body = d[5:5 + ln]
    i = 5 + ln
    ex = struct.unpack(">BHH", d[i:i + 5])[2]
    return body, ad, ex


def main():
    binp = os.path.join(COCO3, "build", "KEYTEST.BIN")
    if not os.path.exists(binp):
        sys.exit("keycheck: missing %s -- run `make keytest`" % binp)
    if not os.path.exists(MAME):
        print("keycheck: SKIPPED -- no MAME at %s" % MAME)
        return 0

    body, ld, ex = decb_split(binp)
    tmp = tempfile.mkdtemp(prefix="keycheck")
    raw, adr, keysf, lua, out = (os.path.join(tmp, n) for n in
        ("keytest.raw", "addr.txt", "keys.txt", "run.lua", "out.txt"))
    open(raw, "wb").write(body)
    open(adr, "w").write("%d %d" % (ld, ex))
    open(keysf, "w").write("".join("%d %d %s\n" % (r, sh, nm)
                                   for r, nm, sh, _, _ in KEYS))
    open(lua, "w").write(LUA)

    subprocess.run([MAME, "coco3", "-window", "-skip_gameinfo", "-rompath", ROMS,
                    "-ext", "multi", "-ext:multi:slot1", "ssfm",
                    "-ext:multi:slot4", "fdc",
                    "-autoboot_script", lua, "-autoboot_delay", "1",
                    "-seconds_to_run", "40", "-nothrottle"],
                   env=dict(os.environ, ADDRF=adr, RAWF=raw, KEYSF=keysf, OUTF=out),
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
                   cwd=tmp, check=False)
    if not os.path.exists(out):
        sys.exit("keycheck: the run produced no output -- MAME never reached the dump")
    b = [int(x, 16) for x in open(out).read().split()]

    # ASK THE INSTRUMENT WHETHER IT IS ARMED before believing a single value.
    if b[31] != 0x5A:
        print("keycheck: KEYTEST did not run to completion (stamp %02X) -- "
              "nothing below is a measurement" % b[31])
        return 1

    bad = 0
    for i, (rt, name, sh, want, what) in enumerate(KEYS):
        got = b[i]
        ok = got == want
        if not ok:
            bad += 1
        print("  %-24s want %3d  got %3d   %s"
              % (what, want, got, "ok" if ok else "*** WRONG ***"))
    print("keycheck: %d of %d keys correct" % (len(KEYS) - bad, len(KEYS)))
    return 0 if bad == 0 else 1


sys.exit(main())
