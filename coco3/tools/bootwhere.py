#!/usr/bin/env python3
"""WHERE does the loader go when it stops loading?

bootcheck.py established the shape of the fault and corrected two readings of
it at once: the read is not slow (97 sectors in under twenty seconds) and it
does not hang (the CPU ends up at $A7D5, Color BASIC's keyboard poll, with the
ROM mapped back in). A loader running at $E400 in all-RAM mode cannot reach
$A7D5 by any legitimate path -- nothing in it jumps to BASIC, and in all-RAM
mode there is no BASIC at that address to jump to. So it goes somewhere first.

THIS TOOL CATCHES THE TRANSITION. It polls the PC finely once the copy is near
its stopping point and keeps a ring of the last samples, so the report is not
"it ended up in BASIC" but the trail of addresses it took to get there.

A SINGLE END-STATE SAMPLE IS WHAT SENT THE LAST THREE RUNS WRONG -- it cannot
tell a stall from a slow floppy, or a crash from either. Sample over time.
"""
import os, subprocess, sys, tempfile

HERE  = os.path.dirname(os.path.abspath(__file__))
COCO3 = os.path.dirname(HERE)
MAME  = os.path.expanduser("~/ample/Ample.app/Contents/MacOS/mame64")
ROMS  = os.path.expanduser("~/Library/Application Support/Ample/roms")

LO, HI = 0xE400, 0xF200          # the loader's own code

LUA = r'''
emu.wait(14)
local kbd = manager.machine.natkeyboard
kbd.in_use = true
kbd:post_coded("CLEAR 25,&H6FFF{ENTER}")
emu.wait(3)
kbd:post_coded('LOADM"TREKLDR"{ENTER}')
emu.wait(10)
kbd:post_coded("EXEC{ENTER}")

local cpu  = manager.machine.devices[":maincpu"]
local prog = cpu.spaces["program"]
local LO, HI = 0xE400, 0xF200

local function copied() return prog:read_u8(0x2016)*256 + prog:read_u8(0x2017) end

-- ARM ON A CONDITION THAT UNINITIALISED RAM CANNOT SATISFY. The first version
-- armed on `copied >= 24000` and fired instantly: RAM reads $FF, so the
-- counter said 65535 before the loader had run a single instruction, and the
-- tool dutifully sampled BASIC still waiting for the EXEC to be typed. Require
-- the loader's own marker AND a count inside the file, which $FFFF is not.
local armed = false
for i = 1, 1200 do
    emu.wait(0.05)
    local c = copied()
    if prog:read_u8(0x2000) == 0xA1 and c >= 24000 and c <= 42605 then
        armed = true; break
    end
end

local out = {}
out[#out+1] = string.format("armed at copied=%d", copied())

-- ARMING IS NOT THE FREEZE. The first version dumped memory the moment the
-- count passed 24000 and reported "nothing matches the file" -- of course not,
-- the copy had not reached that window yet. Wait for the count to STOP, with
-- the CPU still inside the loader, and dump then: that is the only instant
-- where the machine is still in all-RAM mode and the answer is in memory.
-- A SECTOR TAKES ABOUT A FIFTH OF A SECOND, so `copied` standing still for a
-- hundredth of one is not a freeze -- it is a disk read, and the first version
-- of this gate reported one as the other. The threshold has to be longer than
-- the slowest legitimate pause in the loop, by a margin.
local lastc, still = copied(), 0
for i = 1, 3000 do
    emu.wait(0.01)
    local c = copied()
    if c ~= lastc then lastc, still = c, 0 else still = still + 1 end
    local pc = cpu.state["PC"].value
    if still > 150 and pc >= LO and pc <= HI then break end
    if pc < 0x2000 or (pc > HI and pc < 0xFF00) then
        out[#out+1] = string.format("LEFT ALL-RAM before the count settled: PC=%04X", pc)
        break
    end
end
out[#out+1] = string.format("froze at copied=%d PC=%04X", copied(), cpu.state["PC"].value)

-- READS DO NOT ADVANCE EMULATED TIME, so take the whole picture at the instant
-- of the freeze rather than sampling it later and hoping it held still.
local tr = {}
for i = 0, 19 do tr[#tr+1] = string.format("%02X", prog:read_u8(0x2010 + i)) end
out[#out+1] = "tr " .. table.concat(tr, "")

-- HOW FAR DID THE WRITE ACTUALLY GET? `copied` is the loop's own bookkeeping;
-- this is the memory. A 1K window either side of where it stopped, compared
-- against the file in Python, says which byte was the last one written.
local base = 0x2800 + copied() - 512
if base < 0x2800 then base = 0x2800 end
local win = {}
for i = 0, 2047 do win[#win+1] = string.format("%02X", prog:read_u8(base + i)) end
out[#out+1] = string.format("win %04X ", base) .. table.concat(win, "")

-- THE WHOLE LOOP, not four addresses of it: a histogram over a real slice of
-- time shows its extent and whether it is one loop or two.
local hist = {}
for i = 1, 4000 do
    emu.wait(0.0002)
    local pc = cpu.state["PC"].value
    hist[pc] = (hist[pc] or 0) + 1
end
local hl = {}
for pc, c in pairs(hist) do hl[#hl+1] = string.format("%04X:%d", pc, c) end
table.sort(hl)
out[#out+1] = "hist " .. table.concat(hl, " ")
out[#out+1] = string.format("final PC=%04X S=%04X copied=%d",
    cpu.state["PC"].value, cpu.state["S"].value, copied())
local o = io.open(os.getenv("OUTF"), "w")
o:write(table.concat(out, "\n") .. "\n")
o:close()
'''


def main():
    disk = os.path.join(COCO3, "build", "bootchk.dsk")
    raw  = os.path.join(COCO3, "build", "EGATREK.RAW")
    for p in (disk, raw):
        if not os.path.exists(p):
            sys.exit("bootwhere: missing %s -- run `make bootchk`" % p)
    if not os.path.exists(MAME):
        print("bootwhere: SKIPPED -- no MAME at %s" % MAME)
        return 0

    tmp = tempfile.mkdtemp(prefix="bootwhere")
    lua, outf = (os.path.join(tmp, n) for n in ("run.lua", "out.txt"))
    open(lua, "w").write(LUA)
    subprocess.run([MAME, "coco3", "-window", "-rompath", ROMS,
                    "-ext", "multi", "-ext:multi:slot1", "ssfm",
                    "-ext:multi:slot4", "fdc", "-flop1", disk,
                    "-autoboot_script", lua, "-autoboot_delay", "1",
                    "-seconds_to_run", "120", "-nothrottle"],
                   env=dict(os.environ, OUTF=outf),
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
                   cwd=tmp, check=False)
    if not os.path.exists(outf):
        sys.exit("bootwhere: the run produced no output")
    img = open(raw, "rb").read()
    for line in open(outf).read().strip().split("\n"):
        if line.startswith("win "):
            _, basehex, hexs = line.split(" ", 2)
            base = int(basehex, 16)
            ram = bytes.fromhex(hexs)
            want = img[base - 0x2800: base - 0x2800 + len(ram)]
            last = None
            for i in range(len(ram)):
                if ram[i] == want[i]:
                    last = i
                else:
                    break
            if last is None:
                print("  memory: the file does not match RAM even at $%04X" % base)
            else:
                end = base + last + 1
                print("  LAST BYTE ACTUALLY WRITTEN: $%04X  (RAM matches the file "
                      "from $%04X up to there, and diverges at $%04X)"
                      % (end - 1, base, end))
                print("     that is %d bytes into the image = %.2f sectors"
                      % (end - 0x2800, (end - 0x2800) / 256.0))
        elif line.startswith("tr "):
            b = bytes.fromhex(line[3:])
            print("  at the freeze: trk=%d sec=%d attempted=%d stage=%02X "
                  "status=%02X completed=%d copied=%d gran=%d n=%d"
                  % (b[0], b[1], b[2], b[3], b[4], b[5],
                     b[6] * 256 + b[7], b[8], b[9] * 256 + b[10]))
        elif line.startswith("hist "):
            e = line[5:].split()
            print("  the loop, %d distinct addresses over 0.8 emulated seconds:" % len(e))
            for i in range(0, len(e), 8):
                print("     %s" % " ".join(e[i:i + 8]))
        else:
            print("  %s" % line)
    return 0


sys.exit(main())
