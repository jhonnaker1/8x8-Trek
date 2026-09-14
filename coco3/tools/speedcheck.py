#!/usr/bin/env python3
"""Run the double-speed disk probe under MAME -- to check the PROBE, not the claim.

MAME CANNOT ANSWER ITEM 55 and this tool does not pretend to. The hazard is a
real WD1773's timing at 1.78 MHz, which is exactly what an emulator is more
forgiving about. What this checks is that the probe RUNS: that it loads where
BASIC can put it, drives DSKCON at both speeds, fills its report, and returns
to BASIC instead of hanging or wandering.

Handing Jamie a program that has never been executed would be handing him two
questions at once -- is the hardware slow, or is the probe broken -- and on a
machine that needs a person physically present, that is the expensive kind of
mistake.
"""
import os, re, subprocess, sys, tempfile

HERE  = os.path.dirname(os.path.abspath(__file__))
COCO3 = os.path.dirname(HERE)
MAME  = os.path.expanduser("~/ample/Ample.app/Contents/MacOS/mame64")
ROMS  = os.path.expanduser("~/Library/Application Support/Ample/roms")

LUA = r'''
emu.wait(14)                       -- Disk BASIC to its OK prompt
local kbd = manager.machine.natkeyboard
kbd.in_use = true
kbd:post_coded("CLEAR 25,&H6FFF{ENTER}")
emu.wait(3)
kbd:post_coded('LOADM"SPEEDTST"{ENTER}')
emu.wait(8)
kbd:post_coded("EXEC{ENTER}")

local cpu  = manager.machine.devices[":maincpu"]
local prog = cpu.spaces["program"]
-- WAIT FOR THE STAMP, not a fixed time. The probe reads about 1,200 sectors
-- and how long that takes is the thing nobody knows yet.
local done = false
for i = 1, 600 do
    emu.wait(0.5)
    if prog:read_u8(0x7F0D) == 0x5A then done = true break end
end
local out = {}
for i = 0, 15 do out[#out+1] = string.format("%02X", prog:read_u8(0x7F00 + i)) end
out[#out+1] = string.format("PC=%04X", cpu.state["PC"].value)
out[#out+1] = done and "STAMPED" or "NOSTAMP"
local f = io.open(os.getenv("OUTF"), "w"); f:write(table.concat(out, " ")); f:close()
'''


def main():
    disk = os.path.join(COCO3, "build", "speedtst.dsk")
    if not os.path.exists(disk):
        sys.exit("speedcheck: missing %s -- run `make speedtest`" % disk)
    if not os.path.exists(MAME):
        print("speedcheck: SKIPPED -- no MAME at %s" % MAME)
        return 0

    tmp = tempfile.mkdtemp(prefix="speedcheck")
    lua, out = (os.path.join(tmp, n) for n in ("run.lua", "out.txt"))
    open(lua, "w").write(LUA)
    subprocess.run([MAME, "coco3", "-window", "-skip_gameinfo", "-rompath", ROMS,
                    "-ext", "fdc", "-flop1", disk,
                    "-autoboot_script", lua, "-autoboot_delay", "1",
                    "-seconds_to_run", "400", "-nothrottle",
                    "-cfg_directory", tmp, "-snapshot_directory", tmp],
                   env=dict(os.environ, OUTF=out),
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
                   cwd=tmp, check=False)
    if not os.path.exists(out):
        sys.exit("speedcheck: the run produced no output")
    toks = open(out).read().split()
    b = [int(x, 16) for x in toks if re.fullmatch(r"[0-9A-F]{2}", x)]
    tail = [t for t in toks if not re.fullmatch(r"[0-9A-F]{2}", t)]

    stage = {0xA1: "entered main", 0xB1: "slow pass running",
             0xB2: "fast pass running", 0xB3: "both passes done"}
    print("  stage           : %s" % stage.get(b[1], "?%02X" % b[1]))
    print("  stamp           : %s" % ("complete" if b[13] == 0x5A else "MISSING"))
    print("  tracks x sectors: %d x %d   (last track reached %d)" % (b[2], b[3], b[14]))
    print("  0.89 MHz  status errors %3d   mismatches %3d" % (b[4], b[5]))
    print("  1.78 MHz  status errors %3d   mismatches %3d" % (b[8], b[9]))
    print("  cpu             : %s" % " ".join(tail))

    if b[13] != 0x5A:
        print("speedcheck: the probe did not finish -- it is not ready to hand over")
        return 1
    # UNDER MAME BOTH COLUMNS SHOULD BE CLEAN. If the emulator manages to fail
    # this, the probe is wrong -- MAME is the forgiving case.
    if b[4] or b[5] or b[8] or b[9]:
        print("speedcheck: MAME reported errors, which means the PROBE is wrong "
              "-- the emulator is the forgiving case, not the strict one")
        return 1
    print("speedcheck: the probe runs, reads %d sectors at both speeds and "
          "returns to BASIC" % (b[2] * b[3]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
