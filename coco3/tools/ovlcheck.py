#!/usr/bin/env python3
"""Load overlay images off a real Disk BASIC diskette on the machine.

THE OVERLAY SEAM ON ITS OWN. build_ovl.py links eleven images and
overlay_check.py agrees they obey the rules, but both are arithmetic about
files. This is a 6809 finding a file in a directory, walking a FAT chain
through the standalone WD1773 driver, and landing the bytes at the window --
compared against the file they claim to be.

It is separate from tools/ovlrun.py on purpose. ovlrun boots the WHOLE GAME,
which reaches ovl_load through a title screen, a stubbed keyboard and the
video driver, so a failure there can be any of a dozen things. This one
narrows it to the seam, and it is the reason the seam is known to work while
the game's own boot is still being chased.
"""
import os, struct, subprocess, sys, tempfile

HERE  = os.path.dirname(os.path.abspath(__file__))
COCO3 = os.path.dirname(HERE)
MAME  = os.path.expanduser("~/ample/Ample.app/Contents/MacOS/mame64")
ROMS  = os.path.expanduser("~/Library/Application Support/Ample/roms")

LUA = r'''
local ld, ex
for a, b in io.open(os.getenv("ADDRF")):read("*a"):gmatch("(%d+) (%d+)") do
    ld, ex = tonumber(a), tonumber(b)
end
emu.wait(12)                       -- Disk BASIC needs twelve seconds to boot
local cpu  = manager.machine.devices[":maincpu"]
local prog = cpu.spaces["program"]
local f = io.open(os.getenv("RAWF"), "rb"); local img = f:read("*a"); f:close()
for i = 1, #img do prog:write_u8(ld + i - 1, img:byte(i)) end
cpu.state["S"].value  = 0x3F00
cpu.state["PC"].value = ex
emu.wait(20)
local t = {}
for i = 0, 47 do t[#t + 1] = string.format("%02X", prog:read_u8(0x2F00 + i)) end
local o = io.open(os.getenv("OUTF"), "w"); o:write(table.concat(t, " ")); o:close()
'''


def main():
    binp = os.path.join(COCO3, "build", "OVLTEST.BIN")
    disk = os.path.join(COCO3, "build", "trek.dsk")
    for p in (binp, disk):
        if not os.path.exists(p):
            sys.exit("ovlcheck: missing %s -- run `make ovltest` and `make disk`" % p)
    if not os.path.exists(MAME):
        print("ovlcheck: SKIPPED -- no MAME at %s" % MAME)
        return 0

    d = open(binp, "rb").read()
    _, ln, ld = struct.unpack(">BHH", d[:5])
    body = d[5:5 + ln]
    ex = struct.unpack(">H", d[5 + ln + 3:5 + ln + 5])[0]

    tmp = tempfile.mkdtemp(prefix="ovlcheck")
    raw, adr, lua, out = (os.path.join(tmp, n) for n in
                          ("t.raw", "t.addr", "t.lua", "t.txt"))
    open(raw, "wb").write(body)
    open(adr, "w").write("%d %d" % (ld, ex))
    open(lua, "w").write(LUA)

    subprocess.run([MAME, "coco3", "-window", "-rompath", ROMS,
                    "-ext", "multi", "-ext:multi:slot1", "ssfm",
                    "-ext:multi:slot4", "fdc", "-flop1", disk,
                    "-autoboot_script", lua, "-autoboot_delay", "1",
                    "-seconds_to_run", "40", "-nothrottle"],
                   env=dict(os.environ, ADDRF=adr, RAWF=raw, OUTF=out),
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
                   cwd=tmp, check=False)
    if not os.path.exists(out):
        sys.exit("ovlcheck: the run produced no output")
    b = [int(x, 16) for x in open(out).read().split()]
    if b[47] != 0x5A:
        sys.exit("ovlcheck: OVLTEST did not run to completion (sentinel %02X, "
                 "not 5A) -- the results below would be meaningless" % b[47])

    img = os.path.join(COCO3, "build", "img")
    bad = 0
    for label, name, rc_at, got_at, win_at in (
            ("first image",  "HOF.OVL",   1,  2,  4),
            ("second image", "TITLE.OVL", 20, 21, 24)):
        want = open(os.path.join(img, name), "rb").read()
        rc, got = b[rc_at], b[got_at] * 256 + b[got_at + 1]
        head_ok = bytes(b[win_at:win_at + 16]) == want[:16]
        ok = rc == 0 and got == len(want) and head_ok
        bad += 0 if ok else 1
        print("  %-4s %-13s %-11s rc=%d got=%d of %d  window %s"
              % ("ok" if ok else "FAIL", label, name, rc, got, len(want),
                 "matches" if head_ok else "DOES NOT MATCH"))
    # NEGATIVE CONTROL: a file that is not on the disk must come back
    # STOR_NOTFOUND, or the two successes prove nothing.
    if b[40] != 1:
        print("  FAIL absent file      rc=%d, expected 1 (STOR_NOTFOUND)" % b[40])
        bad += 1
    else:
        print("  ok   absent file      rc=1 (STOR_NOTFOUND)")
    print("ovlcheck: %d of 3 ok" % (3 - bad))
    return 1 if bad else 0


sys.exit(main())
