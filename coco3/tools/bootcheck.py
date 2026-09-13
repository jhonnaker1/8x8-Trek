#!/usr/bin/env python3
"""How many bytes does the first-stage loader actually read?

ONE NUMBER, and it separates two very different worlds: if the loader reads
all 42,588 bytes then the image is in memory and the fault is in the game's
own startup; if it stops short, the fault is in the read and the count says
roughly where.

WHY A SEPARATE BUILD. Every earlier attempt read the answer after the game had
crashed and plat_exit() had cold-started BASIC -- which clears low memory, so
the report was gone, and a read above $8000 returned ROM whatever the RAM
beneath held. This builds the loader with -DBOOT_HALT: it reads the image and
then STOPS, leaving the machine in all-RAM mode with nothing to wipe anything.
The report is then simply there to be read.

THE REPORT USES AN ABSOLUTE-ADDRESS MACRO, not a local pointer: cmoc emitted
no stores at all for the pointer version, exactly as coco3bank.h warns. This
tool verifies the stores are in the binary before trusting the run, and looks
for BOTH encodings -- cmoc emits STB (F7), not STA (B7), because it prefers B
for 8-bit values, and checking only for STA reported them missing once.
"""
import os, struct, subprocess, sys, tempfile

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
kbd:post_coded('LOADM"TREKLDR"{ENTER}')
emu.wait(10)
kbd:post_coded("EXEC{ENTER}")
emu.wait(90)                       -- 42K off a floppy, then it halts

local cpu  = manager.machine.devices[":maincpu"]
local prog = cpu.spaces["program"]
local t = {}
for i = 0, 11 do t[#t+1] = string.format("%02X", prog:read_u8(0x2000 + i)) end
local o = io.open(os.getenv("OUTF"), "w")
o:write(table.concat(t, "") .. "\n")
o:write(string.format("%04X\n", cpu.state["PC"].value))
o:close()
'''


def main():
    boot = os.path.join(COCO3, "build", "BOOTCHK.BIN")
    disk = os.path.join(COCO3, "build", "bootchk.dsk")
    raw  = os.path.join(COCO3, "build", "EGATREK.RAW")
    for p in (boot, disk, raw):
        if not os.path.exists(p):
            sys.exit("bootcheck: missing %s -- run `make bootchk` " % p)
    if not os.path.exists(MAME):
        print("bootcheck: SKIPPED -- no MAME at %s" % MAME)
        return 0

    # THE STORES MUST BE IN THE BINARY, or the run measures uninitialised RAM
    # and prints it as a number. That happened twice.
    hexs = open(boot, "rb").read().hex()
    if "f72000" not in hexs and "b72000" not in hexs:
        sys.exit("bootcheck: no store to $2000 in %s -- the report was "
                 "optimised away and any figure below would be RAM noise" % boot)

    tmp = tempfile.mkdtemp(prefix="bootchk")
    lua, outf = (os.path.join(tmp, n) for n in ("run.lua", "out.txt"))
    open(lua, "w").write(LUA)
    subprocess.run([MAME, "coco3", "-window", "-rompath", ROMS,
                    "-ext", "multi", "-ext:multi:slot1", "ssfm",
                    "-ext:multi:slot4", "fdc", "-flop1", disk,
                    "-autoboot_script", lua, "-autoboot_delay", "1",
                    "-seconds_to_run", "130", "-nothrottle"],
                   env=dict(os.environ, OUTF=outf),
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
                   cwd=tmp, check=False)
    if not os.path.exists(outf):
        sys.exit("bootcheck: the run produced no output")
    lines = open(outf).read().strip().split("\n")
    b  = bytes.fromhex(lines[0])
    pc = lines[1]
    img = open(raw, "rb").read()

    print("bootcheck: PC halted at $%s" % pc)
    stage = ("entered main" if b[0] == 0xA1 else None,
             "reached plat_read_all" if b[1] == 0xA2 else None)
    print("  entered main()        : %s" % (b[0] == 0xA1))
    print("  reached plat_read_all : %s" % (b[1] == 0xA2))
    if b[0] != 0xA1:
        print("  -> THE LOADER NEVER RAN. Nothing below is a measurement.")
        return 1
    if b[11] != 0x5A:
        print("  -> plat_read_all NEVER RETURNED (sentinel %02X). The read is "
              "hanging, not failing." % b[11])
        return 1
    rc = {0: "STOR_OK", 1: "STOR_NOTFOUND", 2: "STOR_ERROR"}.get(b[2], "?%02X" % b[2])
    print("  plat_read_all returned: %s" % rc)
    n = b[9] * 256 + b[10]
    print("  BYTES READ            : %d of %d   %s"
          % (n, len(img), "COMPLETE" if n == len(img) else "SHORT by %d" % (len(img) - n)))
    for lbl, addr, off in (("$2800", 0x2800, 0), ("$6000", 0x6000, 0x3800),
                           ("$8000", 0x8000, 0x5800), ("$A000", 0xA000, 0x7800),
                           ("$C000", 0xC000, 0x9800)):
        have = {0x2800: b[3], 0x6000: b[7], 0x8000: b[4],
                0xA000: b[5], 0xC000: b[8]}[addr]
        want = img[off]
        print("  %s read %02X  want %02X   %s"
              % (lbl, have, want, "ok" if have == want else "MISMATCH"))
    return 0 if n == len(img) else 1


sys.exit(main())
