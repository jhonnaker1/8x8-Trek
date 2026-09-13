#!/usr/bin/env python3
"""Write a file from the CoCo and read it back on the host.

plat_write_all was a stub returning STOR_ERROR for the life of this port, so
SAVE could not work. This is the check that it does now, and it deliberately
does NOT trust the machine's own answer: the port writing a file and then
reading it back with its own reader proves only that it is consistent with
itself, and the same wrong idea about the format would sit on both sides.

So the machine writes SAVETEST.DAT and reads it back, AND this tool then opens
the .dsk and walks the directory and the FAT itself, with arithmetic that was
written for mkdisk/checkdisk before any of the write path existed. A file both
can read is a file on a Disk BASIC diskette.

It runs against a COPY of the disk, because it is a write test and the game
disk is a build product.
"""
import os, shutil, struct, subprocess, sys, tempfile

HERE  = os.path.dirname(os.path.abspath(__file__))
COCO3 = os.path.dirname(HERE)
MAME  = os.path.expanduser("~/ample/Ample.app/Contents/MacOS/mame64")
ROMS  = os.path.expanduser("~/Library/Application Support/Ample/roms")

SECSIZE, GRAN_SECS, NUM_GRAN = 256, 9, 68
DIR_TRACK, FAT_SECTOR, DIR_FIRST, DIR_LAST, ENT_SIZE = 17, 2, 3, 11, 32
NBYTES = 600

LUA = r'''
local ld, ex
for a, b in io.open(os.getenv("ADDRF")):read("*a"):gmatch("(%d+) (%d+)") do
    ld, ex = tonumber(a), tonumber(b)
end
emu.wait(12)
pcall(function() manager.machine:pause() end)
local cpu  = manager.machine.devices[":maincpu"]
local prog = cpu.spaces["program"]
local f = io.open(os.getenv("RAWF"), "rb"); local img = f:read("*a"); f:close()
for i = 1, #img do prog:write_u8(ld + i - 1, img:byte(i)) end
-- THE STACK MUST CLEAR bss, AND $3F00 DOES NOT. This program's bss runs to
-- about $3E60 -- 600 bytes of pattern, 600 of read-back, plus the sector
-- buffer and the FAT -- and cmoc's crt ZEROES bss before main() runs, on
-- whatever stack it was given. At $3F00 it wiped its own return addresses
-- and came back to $8006 in ROM with four bytes popped. main()'s own
-- `lds` is far too late to help: the damage is done before it executes.
cpu.state["S"].value  = 0x6F00
cpu.state["PC"].value = ex
pcall(function() manager.machine:resume() end)

-- Wait for the stamp rather than a fixed time: a write is slower than a read
-- and guessing the number is how three earlier tools reported a timeout as a
-- hang on this port.
for i = 1, 400 do
    emu.wait(0.25)
    if prog:read_u8(0x7F1F) == 0x5A then break end
end
local out = {}
for i = 0, 31 do out[#out + 1] = string.format("%02X", prog:read_u8(0x7F00 + i)) end
out[#out+1] = string.format("PC=%04X", cpu.state["PC"].value)
out[#out+1] = string.format("S=%04X", cpu.state["S"].value)
local o = io.open(os.getenv("OUTF"), "w"); o:write(table.concat(out, " ")); o:close()
'''


def decb_split(path):
    d = open(path, "rb").read()
    t, ln, ad = struct.unpack(">BHH", d[:5])
    return d[5:5 + ln], ad, struct.unpack(">BHH", d[5 + ln:10 + ln])[2]


def gran_loc(g):
    t = g >> 1
    if t >= DIR_TRACK:
        t += 1
    return t, (10 if (g & 1) else 1)


def host_read(dsk, name):
    """The host's own walk of the directory and the FAT -- no code shared with
    the port, which is the whole reason this tool exists."""
    d = open(dsk, "rb").read()
    def sec(t, s):
        o = (t * 18 + (s - 1)) * SECSIZE
        return d[o:o + SECSIZE]
    fat = sec(DIR_TRACK, FAT_SECTOR)
    base, ext = (name.split(".") + [""])[:2]
    want = base.ljust(8)[:8] + ext.ljust(3)[:3]
    ent = None
    for s in range(DIR_FIRST, DIR_LAST + 1):
        b = sec(DIR_TRACK, s)
        for e in range(8):
            p = b[e * ENT_SIZE:(e + 1) * ENT_SIZE]
            if p[0] in (0x00, 0xFF):
                continue
            if p[0:11].decode("latin1") == want:
                ent = p
    if ent is None:
        return None, "no directory entry"
    first, lastb = ent[13], ent[14] * 256 + ent[15]
    out, g, seen = bytearray(), first, 0
    while True:
        seen += 1
        if seen > NUM_GRAN:
            return None, "FAT chain loops"
        if g >= NUM_GRAN:
            return None, "chain runs off the end at granule %d" % g
        v = fat[g]
        last = (v & 0xC0) == 0xC0
        n = (v & 0x3F) if last else GRAN_SECS
        t, s0 = gran_loc(g)
        for k in range(n):
            out += sec(t, s0 + k)
        if last:
            out = out[:len(out) - SECSIZE + lastb]
            break
        g = v
    return bytes(out), None


def main():
    binp = os.path.join(COCO3, "build", "WRITETEST.BIN")
    disk = os.path.join(COCO3, "build", "trek.dsk")
    for p in (binp, disk):
        if not os.path.exists(p):
            sys.exit("writecheck: missing %s -- run `make writetest disk`" % p)
    if not os.path.exists(MAME):
        print("writecheck: SKIPPED -- no MAME at %s" % MAME)
        return 0

    body, ld, ex = decb_split(binp)
    tmp = tempfile.mkdtemp(prefix="writecheck")
    raw, adr, lua, out = (os.path.join(tmp, n) for n in
                          ("wt.raw", "addr.txt", "run.lua", "out.txt"))
    scratch = os.path.join(tmp, "scratch.dsk")
    shutil.copyfile(disk, scratch)
    open(raw, "wb").write(body)
    open(adr, "w").write("%d %d" % (ld, ex))
    open(lua, "w").write(LUA)

    subprocess.run([MAME, "coco3", "-window", "-skip_gameinfo", "-rompath", ROMS,
                    "-ext", "multi", "-ext:multi:slot1", "ssfm",
                    "-ext:multi:slot4", "fdc", "-flop1", scratch,
                    "-autoboot_script", lua, "-autoboot_delay", "1",
                    "-seconds_to_run", "130", "-nothrottle",
                    "-cfg_directory", tmp, "-snapshot_directory", tmp],
                   env=dict(os.environ, ADDRF=adr, RAWF=raw, OUTF=out),
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
                   cwd=tmp, check=False)
    if not os.path.exists(out):
        sys.exit("writecheck: the run produced no output")
    toks = open(out).read().split()
    tail = [t for t in toks if "=" in t]
    b = [int(x, 16) for x in toks if "=" not in x]

    if b[31] != 0x5A:
        print("writecheck: WRITETEST did not finish (stamp %02X) -- nothing "
              "below is a measurement" % b[31])
        print("   report bytes : %s" % " ".join("%02X" % x for x in b[:8]))
        print("   cpu          : %s" % " ".join(tail))
        return 1

    rc = {0: "STOR_OK", 1: "STOR_NOTFOUND", 2: "STOR_ERROR"}
    got = b[2] * 256 + b[3]
    print("  plat_write_all returned : %s" % rc.get(b[0], "?%02X" % b[0]))
    print("  plat_read_all  returned : %s   %d of %d bytes"
          % (rc.get(b[1], "?%02X" % b[1]), got, NBYTES))
    print("  the machine's own compare: %s%s"
          % ("MATCHES" if b[4] else "DIFFERS",
             "" if b[4] else " -- first difference at %d" % (b[5] * 256 + b[6])))

    want = bytes(((i * 7 + (i >> 3)) & 0xFF) ^ 0x5A for i in range(NBYTES))
    data, err = host_read(scratch, "SAVETEST.DAT")
    if err:
        print("  the HOST reading the disk: FAILED -- %s" % err)
        return 1
    ok = data == want
    print("  the HOST reading the disk: %d bytes, %s"
          % (len(data), "byte-for-byte correct" if ok else "WRONG CONTENT"))
    if not ok and len(data) == len(want):
        bad = next(i for i in range(len(want)) if data[i] != want[i])
        print("     first difference at %d: disk %02X, expected %02X"
              % (bad, data[bad], want[bad]))

    good = (b[0] == 0 and b[1] == 0 and got == NBYTES and b[4] and ok)
    print("writecheck: %s" % ("both readers agree -- the file is on the disk"
                              if good else "FAIL"))
    return 0 if good else 1


sys.exit(main())
