#!/usr/bin/env python3
"""Run the OVERLAY build on a coco3 + SuperSprite FM+ under MAME, with the
program and its overlay images on a real Disk BASIC diskette.

IT LOADS THE WAY A CoCo DOES: CLEAR, LOADM, EXEC, typed at the BASIC prompt.

An earlier version poked the 42,620-byte image into RAM from Lua, and that
image ran straight over Disk BASIC's own stack and variables while BASIC was
live. Every failure then looked like a port bug: the CPU wandering in ROM, the
stack landing on BASIC's and climbing into the I/O page (visible on screen as
rainbow colours), not one byte of the startup executing. A screenshot of the
machine still sitting at its OK prompt is what settled it.

AND THE LOAD ADDRESS IS PART OF THE SAME LESSON. The port linked at $1200,
reasoning that it owns the machine. It does -- but something has to LOAD it
first, and `CLEAR 25,&H11FF` comes back ?OM ERROR because $11FF is below
BASIC's own workspace, so the ceiling never moves. At $2800, cmoc's default,
`CLEAR 25,&H6FFF` answers OK and LOADM is safe.
"""
import os, re, subprocess, sys, tempfile

HERE  = os.path.dirname(os.path.abspath(__file__))
COCO3 = os.path.dirname(HERE)
MAME  = os.path.expanduser("~/ample/Ample.app/Contents/MacOS/mame64")
ROMS  = os.path.expanduser("~/Library/Application Support/Ample/roms")

LUA = r'''
local resident_image = tonumber(os.getenv("RESIMG"), 16)
local win  = tonumber(os.getenv("WINDOW"), 16)
local rdy  = tonumber(os.getenv("READYAT"), 16)
local dc   = tonumber(os.getenv("DCOPC"), 16)

emu.wait(14)                       -- Disk BASIC to its OK prompt

-- THE NATURAL KEYBOARD HAS TO BE TURNED ON or posts vanish silently and BASIC
-- sits in its keyboard poll at $A7D5, looking exactly like a program that
-- failed to start.
local kbd = manager.machine.natkeyboard
kbd.in_use = true
-- CLEAR below the stub at $2600, then load the LOADER -- 3K, which BASIC can
-- place -- and let it read the 42K image into position itself.
kbd:post_coded("CLEAR 25,&H6FFF{ENTER}")
emu.wait(3)
kbd:post_coded('LOADM"TREKLDR"{ENTER}')
emu.wait(8)
kbd:post_coded("EXEC{ENTER}")

local cpu  = manager.machine.devices[":maincpu"]
local prog = cpu.spaces["program"]

function probe(a, n)
    local t = {}
    for k = 0, n - 1 do t[#t+1] = string.format("%02X", prog:read_u8(a + k)) end
    return table.concat(t, "")
end

local seen, order, trace, snap = {}, {}, {}, nil
local xy = {}
local imgsnap = nil
local smin, smax = nil, nil
for i = 1, 300 do
    emu.wait(0.25)
    local v  = prog:read_u8(resident_image)
    local sv = cpu.state["S"].value
    if smin == nil or sv < smin then smin = sv end
    if smax == nil or sv > smax then smax = sv end
    if i <= 14 then trace[#trace+1] = string.format("%04X/%04X", cpu.state["PC"].value, sv) end
    -- IS THE LOOP ACTUALLY LOOPING? X is the pointer and Y the counter; if Y
    -- is not falling the loop is not the thing that is stuck.
    -- SAMPLE THE IMAGE WHILE THE GAME IS LIVE. Reading it at the end is
    -- reading after the machine has gone back to ROM mode, which shows ROM at
    -- $A000 and says nothing about whether the loader did its job.
    if imgsnap == nil and prog:read_u8(0x2800) == 0x1A and i > 60 then
        imgsnap = probe(0x2800, 8) .. " " .. probe(0x6000, 8) .. " "
               .. probe(0xA000, 8) .. " " .. probe(0xCE58, 8)
    end
    if i % 40 == 0 then
        xy[#xy+1] = string.format("%.0fs:PC=%04X X=%04X Y=%04X CC=%02X",
            i*0.25, cpu.state["PC"].value, cpu.state["X"].value,
            cpu.state["Y"].value, cpu.state["CC"].value)
    end
    if v ~= 0xFF and not seen[v] then
        seen[v] = true
        order[#order+1] = v
        if snap == nil then
            snap = {}
            for k = 0, 63 do snap[#snap+1] = string.format("%02X", prog:read_u8(win + k)) end
            snap = tostring(v) .. " " .. table.concat(snap, "")
        end
    end
end
local o = io.open(os.getenv("OUTF"), "w")
o:write("img " .. (imgsnap or (probe(0x2800,8).." "..probe(0x6000,8).." "
        ..probe(0xA000,8).." "..probe(0xCE58,8))) .. "\n")
o:write("order " .. table.concat(order, ",") .. "\n")
o:write("snap " .. (snap or "none") .. "\n")
o:write(string.format("pc %04X\n", cpu.state["PC"].value))
o:write(string.format("s %04X %04X\n", smin or 0, smax or 0))
o:write("trace " .. table.concat(trace, " ") .. "\n")
o:write("xy " .. table.concat(xy, " | ") .. "\n")
o:write(string.format("ready %02X\n", prog:read_u8(rdy)))
o:write(string.format("dskcon opc=%02X drv=%02X trk=%02X sec=%02X sta=%02X\n",
  prog:read_u8(dc), prog:read_u8(dc+1), prog:read_u8(dc+2), prog:read_u8(dc+3),
  prog:read_u8(dc+6)))
local tail = {}
for k = 0, 63 do tail[#tail+1] = string.format("%02X", prog:read_u8(win + k)) end
o:write("final " .. prog:read_u8(resident_image) .. " " .. table.concat(tail, "") .. "\n")
o:close()
'''


def sym(mappath, name):
    for line in open(mappath):
        m = re.match(r'Symbol: %s \([^)]*\) = ([0-9A-Fa-f]+)' % re.escape(name), line.strip())
        if m:
            return int(m.group(1), 16)
    sys.exit("ovlrun: no symbol %s in %s" % (name, mappath))


def window_addr(ovlmap_h):
    m = re.search(r'#define\s+OVL_WINDOW\s+0x([0-9A-Fa-f]+)', open(ovlmap_h).read())
    if not m:
        sys.exit("ovlrun: no OVL_WINDOW in %s" % ovlmap_h)
    return int(m.group(1), 16)


def names_from_map(ovlmap_h):
    return dict(enumerate(re.findall(r'"([A-Z0-9]+\.OVL)"', open(ovlmap_h).read())))


def main():
    mappath = os.path.join(COCO3, "build", "egatrek-ovl.bin.map")
    ovlmap  = os.path.join(COCO3, "build", "img", "ovlmap.h")
    disk    = os.path.join(COCO3, "build", "trek.dsk")
    for p in (mappath, ovlmap, disk):
        if not os.path.exists(p):
            sys.exit("ovlrun: missing %s -- run `make disk`" % p)
    if not os.path.exists(MAME):
        print("ovlrun: SKIPPED -- no MAME at %s" % MAME)
        return 0

    win    = window_addr(ovlmap)
    labels = names_from_map(ovlmap)
    tmp = tempfile.mkdtemp(prefix="ovlrun")
    lua, outf = (os.path.join(tmp, n) for n in ("run.lua", "out.txt"))
    open(lua, "w").write(LUA)

    env = dict(os.environ, OUTF=outf, WINDOW="%X" % win,
               RESIMG="%X" % sym(mappath, "_resident_image"),
               DCOPC="%X" % sym(mappath, "_DCOPC"),
               READYAT="%X" % sym(mappath, "_ready"))
    p = subprocess.run([MAME, "coco3", "-window", "-rompath", ROMS,
                    # SLOT ORDER IS LOAD-BEARING AND WAS TESTED, not assumed. Putting the FDC in
                    # slot 1 and the card in slot 4 leaves the machine unable to boot Disk
                    # BASIC at all -- rainbow blocks, no prompt, nothing runs. The FDC's ROM
                    # has to be in the slot the machine looks in.
                    "-ext", "multi", "-ext:multi:slot1", "ssfm",
                    "-ext:multi:slot4", "fdc", "-flop1", disk,
                    "-autoboot_script", lua, "-autoboot_delay", "1",
                    "-seconds_to_run", "160", "-nothrottle"],
                   env=env, capture_output=True, text=True, cwd=tmp, check=False)
    if not os.path.exists(outf):
        # AN INSTRUMENT THAT CANNOT SAY WHY IT FAILED sends you guessing.
        tailout = (p.stdout + p.stderr).strip().splitlines()
        sys.exit("ovlrun: the run produced no output\n  "
                 + "\n  ".join(tailout[-6:] or ["(MAME said nothing)"]))
    got = dict(l.split(" ", 1) for l in open(outf).read().strip().split("\n"))

    order = [int(x) for x in got["order"].strip().split(",") if x]
    print("ovlrun: loaded by CLEAR/LOADM/EXEC from the diskette, window $%04X" % win)
    raw = open(os.path.join(COCO3, "build", "EGATREK.RAW"), "rb").read()
    spots = [(0x2800, "start"), (0x6000, "a third in"),
             (0xA000, "two thirds in"), (0xCE58, "the last bytes")]
    parts = got["img"].strip().split()
    allok = True
    for (addr, lbl), have in zip(spots, parts):
        want = raw[addr - 0x2800: addr - 0x2800 + 8].hex().upper()
        ok = have == want
        allok &= ok
        print("  image %-15s $%04X %s %s %s" % (lbl, addr, have,
              "==" if ok else "!=", want))
    print("  -> the loader placed the image CORRECTLY" if allok
          else "  -> THE IMAGE IS NOT IN MEMORY AS WRITTEN")
    print("  PC at the end   : $%s" % got["pc"].strip())
    for line in got["xy"].strip().split(" | "):
        if line: print("     %s" % line)
    print("  stack S ranged  : $%s..$%s" % tuple(got["s"].strip().split()))
    print("  PC/S trace      : %s" % got["trace"].strip())
    print("  disk ready flag : %s   dskcon %s"
          % (got["ready"].strip(), got["dskcon"].strip()))
    if not order:
        print("ovlrun: FAIL -- no overlay was ever loaded")
        return 1
    stray = [i for i in order if i not in labels]
    print("ovlrun: overlays paged in: "
          + ", ".join("%d=%s" % (i, labels.get(i, "?")) for i in order))
    if stray:
        print("ovlrun: FAIL -- %s is not a valid overlay index" % stray)
        return 1
    idx, hexs = got["snap"].strip().split(" ", 1)
    want = open(os.path.join(COCO3, "build", "img", labels[int(idx)]), "rb").read()[:64]
    if bytes.fromhex(hexs) != want:
        print("ovlrun: FAIL -- the window does not hold %s" % labels[int(idx)])
        return 1
    print("ovlrun: the window holds the real first 64 bytes of %s" % labels[int(idx)])
    return 0


sys.exit(main())
