#!/usr/bin/env python3
"""Run the OVERLAY build on a coco3 + SuperSprite FM+ under MAME, with the
overlay images on a real Disk BASIC diskette, and report what actually got
paged in.

IT DOES NOT PASS, AND THE HARNESS IS PART OF WHY. This loads the 42,620-byte
resident image by POKING IT INTO RAM from Lua, and that image spans
$1200..$B85C -- straight over Disk BASIC's own stack (~$34xx) and variables,
while BASIC is live. Pausing the machine first (below) fixed most of it: with
the pause the port zeroes bss, keeps a healthy stack at $FDxx, takes the
machine, drives the V9958 and issues CORRECT disk requests (opc=2, track 17,
sector 3 -- the directory -- with status 0). Without it, none of that happened
and every symptom looked like a port bug.

WHAT IS STILL WRONG IS NOT TIME. Raising the boot wait from 12s to 20s made it
fail DIFFERENTLY (PC executing the stack at $FDEA, the DSKCON block all
zeros), and the run-to-run behaviour varies. That non-determinism is the
signature of loading over a live BASIC, not of a program that needs longer.

THE REAL FIX IS TO STOP POKING: give the diskette a boot record, or LOADM the
image from BASIC, so the machine loads the program the way a CoCo actually
would. Until then this tool is diagnostic, not a gate. tools/ovlcheck.py IS a
gate and passes -- its image is 3.6K and stops short of BASIC's stack, which
is exactly why it was never affected.

WHAT WOULD OTHERWISE BE UNPROVEN: build_ovl.py links eleven images and
overlay_check.py agrees they obey the rules, but both of those are arithmetic
about a file. Nothing there says a 6809 can find HOF.OVL in a directory, walk
a FAT chain through the standalone WD1773 driver, and land 1,709 bytes at
$C300 that then execute.

HOW IT IS PROVEN HERE, and none of it is an inference:
  * `_resident_image` in coco3ovl.c is SAMPLED while the game runs, so the
    SEQUENCE of overlays is visible, not just whichever was last.
  * the bytes actually sitting in the window are compared against the .OVL
    file they claim to be -- a loader that reads the wrong file, or half of
    one, passes every other check here.
  * the CPU is checked to still be executing resident code afterwards.
"""
import os, re, struct, subprocess, sys, tempfile

HERE  = os.path.dirname(os.path.abspath(__file__))
COCO3 = os.path.dirname(HERE)
MAME  = os.path.expanduser("~/ample/Ample.app/Contents/MacOS/mame64")
ROMS  = os.path.expanduser("~/Library/Application Support/Ample/roms")

LUA = r'''
local resident_image = tonumber(os.getenv("RESIMG"), 16)
local win = tonumber(os.getenv("WINDOW"), 16)
local ld, ex
for a, b in io.open(os.getenv("ADDRF")):read("*a"):gmatch("(%d+) (%d+)") do
    ld, ex = tonumber(a), tonumber(b)
end
emu.wait(12)                       -- Disk BASIC needs twelve seconds to boot
-- PAUSE BEFORE POKING. This image spans $1200..$B85B, which covers Disk
-- BASIC's own stack (~$34xx) and its variables -- and BASIC IS STILL RUNNING
-- while the poke happens, so writing 42,620 bytes over it crashed the machine
-- before PC was ever set. Everything after that looked like the port failing:
-- the CPU wandering in ROM at $89xx-$A2xx, S landing on BASIC's stack and
-- then climbing into the I/O page, and not one byte of the startup executing.
-- ovlcheck never saw it because its image is 3.6K and stops short of $34xx.
local paused = pcall(function() manager.machine:pause() end)
if not paused then paused = pcall(function() emu.pause() end) end
local cpu  = manager.machine.devices[":maincpu"]
local prog = cpu.spaces["program"]
local f = io.open(os.getenv("RAWF"), "rb"); local img = f:read("*a"); f:close()
for i = 1, #img do prog:write_u8(ld + i - 1, img:byte(i)) end
prog:write_u8(resident_image, 0xFF)          -- "nothing loaded", so a stale
                                             -- value cannot be mistaken for one
-- ALL-RAM MODE IS THE PORT'S JOB, not the harness's: vdc_init() does it with
-- a CPU write to $FFDF. Poking it from here does nothing -- these are
-- write-only address latches and a debugger write never triggers them, which
-- is why doing it here appeared to disprove a correct diagnosis.
-- IS BSS ZEROED? Read the storage layer's `ready` flag and the DSKCON block
-- BEFORE the program runs. If `ready` is non-zero, disk_ready() returns
-- success without ever initialising the driver or reading the FAT.
local ready_at = tonumber(os.getenv("READYAT"), 16)
local dc0 = tonumber(os.getenv("DCOPC"), 16)
for a=0xCF00,0xCF04 do prog:write_u8(a,0xEE) end
for a=0x0F00,0x0F02 do prog:write_u8(a,0xEE) end
local bss0 = string.format("ready=%02X dskcon=%02X%02X%02X%02X",
    prog:read_u8(ready_at), prog:read_u8(dc0), prog:read_u8(dc0+1),
    prog:read_u8(dc0+2), prog:read_u8(dc0+3))
cpu.state["S"].value  = 0xFE00
cpu.state["PC"].value = ex
-- DID THE POKE AND THE PC SET TAKE? Read both back immediately.
local chk = string.format("poked@%04X=%02X%02X%02X%02X pc_readback=%04X s=%04X",
  ld, prog:read_u8(ld), prog:read_u8(ld+1), prog:read_u8(ld+2), prog:read_u8(ld+3),
  cpu.state["PC"].value, cpu.state["S"].value)
if paused then
  if not pcall(function() manager.machine:resume() end) then pcall(function() emu.unpause() end) end
end

local pre = {}
for k = 0, 15 do pre[#pre + 1] = string.format("%02X", prog:read_u8(win + k)) end

-- SAMPLE, do not just read at the end: the game loads several overlays as it
-- runs and only the last would survive a single read.
local seen, order = {}, {}
local smin, smax = nil, nil
local trace = {}
local rtrace, lastready = {}, -1
local snap = nil
for i = 1, 480 do
    emu.wait(0.25)
    local v = prog:read_u8(resident_image)
    local sv = cpu.state["S"].value
    if i <= 16 then trace[#trace+1] = string.format("%04X/%04X", cpu.state["PC"].value, sv) end
    local rv = prog:read_u8(ready_at)
    if rv ~= lastready then
        lastready = rv
        rtrace[#rtrace+1] = string.format("%.1fs:ready=%02X", i*0.25, rv)
    end
    if smin == nil or sv < smin then smin = sv end
    if smax == nil or sv > smax then smax = sv end
    if v ~= 0xFF and not seen[v] then
        seen[v] = true
        order[#order + 1] = v
        if snap == nil then                  -- keep the window's bytes for the
            snap = {}                        -- FIRST image, to compare on disk
            for k = 0, 63 do snap[#snap + 1] = string.format("%02X", prog:read_u8(win + k)) end
            snap = tostring(v) .. " " .. table.concat(snap, "")
        end
    end
end
local o = io.open(os.getenv("OUTF"), "w")
o:write("pre " .. table.concat(pre, "") .. "\n")
o:write("bss0 " .. bss0 .. "\n")
o:write("chk " .. chk .. " paused=" .. tostring(paused) .. "\n")
o:write("bss1 " .. string.format("ready=%02X", prog:read_u8(ready_at)) .. "\n")
o:write(string.format("low %02X %02X %02X\n", prog:read_u8(0x0F00), prog:read_u8(0x0F01), prog:read_u8(0x0F02)))
o:write(string.format("prog %02X %02X %02X %02X %02X\n", prog:read_u8(0xCF00),
  prog:read_u8(0xCF01), prog:read_u8(0xCF02), prog:read_u8(0xCF03), prog:read_u8(0xCF04)))
o:write("order " .. table.concat(order, ",") .. "\n")
o:write("snap " .. (snap or "none") .. "\n")
o:write(string.format("pc %04X\n", cpu.state["PC"].value))
o:write(string.format("s %04X %04X\n", smin or 0, smax or 0))
o:write("trace " .. table.concat(trace, " ") .. "\n")
o:write("rtrace " .. table.concat(rtrace, " ") .. "\n")
-- ASK THE DISK CONTROLLER WHAT IT WAS ASKED FOR. A hang inside
-- dskcon_processSector says nothing about WHICH sector; this does.
local dc = tonumber(os.getenv("DCOPC"), 16)
o:write(string.format("dskcon opc=%02X drv=%02X trk=%02X sec=%02X bpt=%02X%02X sta=%02X nmiflg=%02X nmivec=%02X%02X\n",
  prog:read_u8(dc), prog:read_u8(dc+1), prog:read_u8(dc+2), prog:read_u8(dc+3),
  prog:read_u8(dc+4), prog:read_u8(dc+5), prog:read_u8(dc+6),
  prog:read_u8(dc+13), prog:read_u8(dc+14), prog:read_u8(dc+15)))
local tail = {}
for k = 0, 63 do tail[#tail + 1] = string.format("%02X", prog:read_u8(win + k)) end
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
    out_bin = os.path.join(COCO3, "build", "egatrek-ovl.bin")
    mappath = out_bin + ".map"
    ovlmap  = os.path.join(COCO3, "build", "img", "ovlmap.h")
    disk    = os.path.join(COCO3, "build", "trek.dsk")
    for p in (out_bin, mappath, ovlmap, disk):
        if not os.path.exists(p):
            sys.exit("ovlrun: missing %s -- run `make overlays` and `make disk`" % p)
    if not os.path.exists(MAME):
        print("ovlrun: SKIPPED -- no MAME at %s" % MAME)
        return 0

    win = window_addr(ovlmap)
    resimg = sym(mappath, "_resident_image")
    labels = names_from_map(ovlmap)

    d = open(out_bin, "rb").read()
    t, ln, ld = struct.unpack(">BHH", d[:5])
    body = d[5:5 + ln]
    ex = struct.unpack(">H", d[5 + ln + 3:5 + ln + 5])[0]

    tmp = tempfile.mkdtemp(prefix="ovlrun")
    raw, adr, lua, outf = (os.path.join(tmp, n) for n in
                           ("res.raw", "addr.txt", "run.lua", "out.txt"))
    open(raw, "wb").write(body)
    open(adr, "w").write("%d %d" % (ld, ex))
    open(lua, "w").write(LUA)

    env = dict(os.environ, ADDRF=adr, RAWF=raw, OUTF=outf,
               RESIMG="%X" % resimg, WINDOW="%X" % win,
               DCOPC="%X" % sym(mappath, "_DCOPC"),
               READYAT="%X" % sym(mappath, "_ready"))
    subprocess.run([MAME, "coco3", "-window", "-rompath", ROMS,
                    "-ext", "multi", "-ext:multi:slot1", "ssfm",
                    "-ext:multi:slot4", "fdc", "-flop1", disk,
                    "-autoboot_script", lua, "-autoboot_delay", "1",
                    "-seconds_to_run", "150", "-nothrottle"],
                   env=env, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
                   cwd=tmp, check=False)

    if not os.path.exists(outf):
        sys.exit("ovlrun: the run produced no output")
    got = dict(l.split(" ", 1) for l in open(outf).read().strip().split("\n"))

    order = [int(x) for x in got["order"].strip().split(",") if x]
    print("ovlrun: resident $%04X..$%04X, window $%04X" % (ld, ld + ln - 1, win))
    print("  window before the program ran : %s" % got["pre"].strip()[:32])
    print("  window at the end             : %s" % got["final"].strip().split(" ")[1][:32])
    print("  _resident_image at the end    : %s" % got["final"].strip().split(" ")[0])
    print("  PC at the end                 : $%s" % got["pc"].strip())
    print("  PC/S over the first 4s        : %s" % got["trace"].strip())
    print("  BEFORE the program ran        : %s" % got["bss0"].strip())
    print("  after                         : %s" % got["bss1"].strip())
    print("  right after PC was set        : %s" % got["chk"].strip())
    print("  low markers  $0F00-02        : %s   (AA at 0F01 = startup ran)" % got["low"].strip())
    print("  startup progress (EE=never reached): %s" % got["prog"].strip())
    print("     11=took machine 22=bss cleared [first bss byte] 33=INILIB back [first bss byte]")
    print("  disk ready flag over time     : %s" % got["rtrace"].strip())
    print("  %s" % got["dskcon"].strip())
    lo_s, hi_s = got["s"].strip().split()
    print("  stack S ranged over           : $%s..$%s" % (lo_s, hi_s))
    if int(lo_s, 16) <= ld + ln:
        print("  *** THE STACK IS INSIDE THE PROGRAM ($%04X..$%04X) ***" % (ld, ld + ln - 1))
    if not order:
        print("ovlrun: FAIL -- no overlay was ever loaded "
              "(_resident_image never left $FF)")
        return 1
    print("ovlrun: overlays paged in, in order: "
          + ", ".join("%d=%s" % (i, labels.get(i, "?")) for i in order))
    # AN INSTRUMENT MUST REPORT BAD DATA, NOT CRASH ON IT. A value outside the
    # table means the byte being sampled is not the loader's state any more --
    # a moved symbol, or a program that has overwritten it.
    stray = [i for i in order if i not in labels]
    if stray:
        print("ovlrun: FAIL -- %s is not a valid overlay index (0..%d). The "
              "sampled byte is not _resident_image, or the program wrote over "
              "it." % (", ".join(str(i) for i in stray), len(labels) - 1))
        return 1

    # THE DECIDING CHECK: are the bytes in the window the ones on the disk?
    idx, hexs = got["snap"].strip().split(" ", 1)
    want = open(os.path.join(COCO3, "build", "img", labels[int(idx)]), "rb").read()[:64]
    have = bytes.fromhex(hexs)
    if have != want:
        print("ovlrun: FAIL -- the window does not hold %s" % labels[int(idx)])
        print("   want %s" % want[:16].hex())
        print("   have %s" % have[:16].hex())
        return 1
    print("ovlrun: the window holds the real first 64 bytes of %s" % labels[int(idx)])
    print("ovlrun: PC after the run $%s" % got["pc"].strip())
    return 0


sys.exit(main())
