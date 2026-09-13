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
no stores at all for the pointer version, exactly as README.md's "cmoc
traps" warns. This
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

local cpu  = manager.machine.devices[":maincpu"]
local prog = cpu.spaces["program"]

-- ONE SAMPLE CANNOT TELL A STALL FROM A SLOW FLOPPY. That is not a guess about
-- this instrument, it is its history: the first version waited 90 seconds, saw
-- 99 sectors, and printed HANG. Jamie said twice it was still loading, and he
-- was right both times. So poll the breadcrumbs and keep a timeline -- a count
-- still climbing at the end is a short timeout, a count flat for ten minutes is
-- a stall, and the tool no longer has to guess which.
local tl, last = {}, nil
for i = 1, 180 do
    emu.wait(5)
    local line = string.format("%4ds att=%-3d done=%-3d trk=%-2d sec=%-2d stage=%02X copied=%-6d PC=%04X",
        i*5, prog:read_u8(0x2012), prog:read_u8(0x2015),
        prog:read_u8(0x2010), prog:read_u8(0x2011), prog:read_u8(0x2013),
        prog:read_u8(0x2016)*256 + prog:read_u8(0x2017),
        cpu.state["PC"].value)
    local key = string.sub(line, 6)          -- everything but the timestamp
    if key ~= last then tl[#tl+1] = line; last = key end
    if prog:read_u8(0x200B) == 0x5A then
        tl[#tl+1] = string.format("%4ds plat_read_all RETURNED", i*5)
        break
    end
end

local t = {}
for i = 0, 11 do t[#t+1] = string.format("%02X", prog:read_u8(0x2000 + i)) end
for i = 0, 19 do t[#t+1] = string.format("%02X", prog:read_u8(0x2010 + i)) end
local o = io.open(os.getenv("OUTF"), "w")
o:write(table.concat(t, "") .. "\n")

o:write(string.format("%04X\n", cpu.state["PC"].value))
o:write(table.concat(tl, "\n") .. "\n")
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
    subprocess.run([MAME, "coco3", "-window", "-skip_gameinfo", "-rompath", ROMS,
                    "-ext", "multi", "-ext:multi:slot1", "ssfm",
                    "-ext:multi:slot4", "fdc", "-flop1", disk,
                    "-autoboot_script", lua, "-autoboot_delay", "1",
                    "-seconds_to_run", "1000", "-nothrottle"],
                   env=dict(os.environ, OUTF=outf),
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
                   cwd=tmp, check=False)
    if not os.path.exists(outf):
        sys.exit("bootcheck: the run produced no output")
    lines = open(outf).read().strip().split("\n")
    b  = bytes.fromhex(lines[0])
    pc = lines[1]
    timeline = [l for l in lines[2:] if l.strip()]
    img = open(raw, "rb").read()

    print("bootcheck: PC halted at $%s" % pc)
    if timeline:
        print("  --- what the read did over 900 emulated seconds ---")
        for l in timeline:
            print("    %s" % l)
        print("  ---------------------------------------------------")
    stage = ("entered main" if b[0] == 0xA1 else None,
             "reached plat_read_all" if b[1] == 0xA2 else None)
    print("  entered main()        : %s" % (b[0] == 0xA1))
    print("  reached plat_read_all : %s" % (b[1] == 0xA2))
    if b[0] != 0xA1:
        print("  -> THE LOADER NEVER RAN. Nothing below is a measurement.")
        return 1
    print("  last sector asked for : track %d sector %d" % (b[12], b[13]))
    # TWO OF THE ATTEMPTS ARE NOT DATA: read_sec is also how the FAT (track 17
    # sector 2) and the directory (sector 3, where EGATREK.RAW is entry 1) are
    # read. 99 attempts is 97 data sectors, and 97 x 256 is exactly the 24,832
    # bytes copied. THERE WAS NEVER A TWO-SECTOR GAP, and the hunt for an `n`
    # that had gone to zero was chasing arithmetic that was always correct.
    print("  sectors attempted     : %d   completed: %d   (%d of them data, "
          "after the FAT and directory)" % (b[14], b[17], b[14] - 2))
    print("  DSKCON stage          : %s   last status %02X"
          % ({0xB1: "called, NOT returned", 0xB2: "returned"}.get(b[15], "%02X" % b[15]), b[16]))
    cop = b[18] * 256 + b[19]
    print("  bytes copied          : %d   -> dst reached $%04X" % (cop, 0x2800 + cop))
    print("  granule               : %d   last chunk %d bytes" % (b[20], b[21] * 256 + b[22]))
    ln = b[23] * 256 + b[24]
    img = open(raw, "rb").read()
    print("  file_len computed     : %d   actual file %d   %s"
          % (ln, len(img), "ok" if ln == len(img) else "WRONG"))
    print("  lastbytes from dir    : %d" % (b[25] * 256 + b[26]))
    print("  first granule         : %d  chain: fat[%d]=%d fat[12]=%d fat[13]=%d fat[20]=%d"
          % (b[27], b[27], b[28], b[29], b[30], b[31]))
    if b[11] != 0x5A:
        print("  -> plat_read_all did not return in the time allowed. If sectors "
              "are still\n     advancing, that is a SHORT TIMEOUT, not a hang.")
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
