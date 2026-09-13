#!/usr/bin/env python3
"""Every value this port writes to $FF40, because ONE of them would break a CoCo SDC.

WHY THIS EXISTS. This port does not reach the drive through Disk BASIC -- it
runs in all-RAM mode, so there is no ROM to call, and <dskcon-standalone.h>
drives the WD1773 registers directly ($FF40 control latch, $FF48-$FF4B).
That is normally the thing that stops a program working on an SD-card
replacement, and it is NOT the case here: the CoCo SDC emulates the floppy
controller in hardware and says so --

    "The CoCo SDC normally operates in FDC Emulation Mode. This makes it
     appear to the CoCo that a standard floppy disk controller is present."
        -- CoCo SDC User Guide v4, "Low-Level Hardware Interface"

BUT THE SAME PAGE NAMES THE ONE WAY TO LOSE IT:

    "To execute any of the extended commands, the hardware must first be
     placed in Command Mode. To do this you store the value $43 in the
     control latch at $FF40. This value would not normally be used with a
     real floppy controller..."

"Would not normally be used" is an assumption about the driver, and this port
has its own driver. If any sector ever put $43 in $FF40, the SDC would stop
being a floppy controller in the middle of a transfer and every read after it
would fail -- on hardware this tool cannot reach and MAME does not emulate
(MAME 0.281 has no SDC device; `mame coco3 -listslots` lists ssfm and fdc and
nothing else).

So this measures the one thing that is measurable here: the set of values the
port actually writes to $FF40 across a real read and a real write. It taps the
CPU's writes under MAME rather than reading the driver's source, because the
driver is a library this port did not write.

It is not a claim that the port runs on an SDC. It is the removal of one
specific reason it would not.
"""
import os, shutil, struct, subprocess, sys, tempfile

HERE  = os.path.dirname(os.path.abspath(__file__))
COCO3 = os.path.dirname(HERE)
MAME  = os.path.expanduser("~/ample/Ample.app/Contents/MacOS/mame64")
ROMS  = os.path.expanduser("~/Library/Application Support/Ample/roms")

CMD_MODE = 0x43          # the value that means "stop being a floppy controller"

LUA = r'''
local ld, ex
for a, b in io.open(os.getenv("ADDRF")):read("*a"):gmatch("(%d+) (%d+)") do
    ld, ex = tonumber(a), tonumber(b)
end
emu.wait(12)
pcall(function() manager.machine:pause() end)
local cpu  = manager.machine.devices[":maincpu"]
local prog = cpu.spaces["program"]

-- COUNT EVERY WRITE, not just the changes. A change-only log merged two
-- identical notes on this port once and manufactured 38 wrong ones; the same
-- mistake here would hide a $43 that is written twice in a row.
local seen, order = {}, {}
-- HOLD THE HANDLE. install_write_tap returns a tap object and MAME REMOVES THE
-- TAP WHEN IT IS COLLECTED; the first version of this tool dropped it on the
-- floor and caught zero writes on a run that made 60 of them. The guard in the
-- Python half is what caught that, which is the only reason this comment is
-- not a green tick.
TAP = prog:install_write_tap(0xFF40, 0xFF40, "ff40", function(offset, data, mask)
    if seen[data] == nil then seen[data] = 0; order[#order + 1] = data end
    seen[data] = seen[data] + 1
    return data
end)

local f = io.open(os.getenv("RAWF"), "rb"); local img = f:read("*a"); f:close()
for i = 1, #img do prog:write_u8(ld + i - 1, img:byte(i)) end
cpu.state["S"].value  = 0x6F00
cpu.state["PC"].value = ex
pcall(function() manager.machine:resume() end)

for i = 1, 400 do
    emu.wait(0.25)
    if prog:read_u8(0x7F1F) == 0x5A then break end
end

local out = {string.format("stamp %02X", prog:read_u8(0x7F1F))}
for _, v in ipairs(order) do
    out[#out + 1] = string.format("%02X %d", v, seen[v])
end
local o = io.open(os.getenv("OUTF"), "w"); o:write(table.concat(out, "\n")); o:close()
'''


def decb_split(path):
    d = open(path, "rb").read()
    t, ln, ad = struct.unpack(">BHH", d[:5])
    return d[5:5 + ln], ad, struct.unpack(">BHH", d[5 + ln:10 + ln])[2]


def main():
    binp = os.path.join(COCO3, "build", "WRITETEST.BIN")
    disk = os.path.join(COCO3, "build", "trek.dsk")
    for p in (binp, disk):
        if not os.path.exists(p):
            sys.exit("sdccheck: missing %s -- run `make writetest disk`" % p)
    if not os.path.exists(MAME):
        print("sdccheck: SKIPPED -- no MAME at %s" % MAME)
        return 0

    body, ld, ex = decb_split(binp)
    tmp = tempfile.mkdtemp(prefix="sdccheck")
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
        sys.exit("sdccheck: the run produced no output")

    lines = [l.split() for l in open(out).read().strip().splitlines() if l.strip()]
    stamp = lines[0][1] if lines and lines[0][0] == "stamp" else "??"
    vals  = [(int(a, 16), int(n)) for a, n in lines[1:]]

    # ASK THE INSTRUMENT WHETHER IT WAS ARMED. A tap that caught nothing and a
    # driver that touched nothing look identical in the output above, and only
    # one of them is a measurement.
    if not vals:
        print("sdccheck: the tap caught NO writes to $FF40 -- that is an "
              "instrument failure, not a clean result")
        print("   raw output   : %r" % open(out).read()[:200])
        return 1
    if stamp != "5A":
        print("sdccheck: WRITETEST did not finish (stamp %s) -- the values "
              "below are from a partial run" % stamp)
        return 1

    total = sum(n for _, n in vals)
    print("  $FF40 writes : %d, in %d distinct values" % (total, len(vals)))
    for v, n in vals:
        print("     $%02X  x%-5d %s" % (v, n, describe(v)))

    if any(v == CMD_MODE for v, _ in vals):
        print("sdccheck: $43 IS WRITTEN -- this port would knock a CoCo SDC "
              "out of FDC Emulation Mode. It cannot run from one as it stands.")
        return 1
    print("sdccheck: $43 never written -- the one value that would drop a "
          "CoCo SDC out of FDC emulation is not in this port's vocabulary")
    return 0


def describe(v):
    """The FD-502 control latch, so a human can see these are ordinary."""
    if v == CMD_MODE:
        return "<-- SDC COMMAND MODE"
    bits = []
    if v & 0x80: bits.append("halt-enable")
    if v & 0x20: bits.append("single-density")
    if v & 0x10: bits.append("write-precomp")
    drv = [i for i, m in enumerate((0x01, 0x02, 0x04, 0x40)) if v & m]
    bits.append("drive " + ",".join(str(d) for d in drv) if drv else "no drive")
    bits.append("motor on" if v & 0x08 else "motor off")
    return "; ".join(bits)


if __name__ == "__main__":
    sys.exit(main())
