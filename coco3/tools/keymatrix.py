#!/usr/bin/env python3
"""Derive the CoCo 3 keyboard matrix FROM THE MACHINE.

coco3input.c needs a table mapping (row, column) to a character, and the
tempting thing is to copy one out of a manual. This holds each of MAME's 56
keyboard fields in turn, strobes all eight columns at $FF02 and reads the rows
at $FF00, and prints where each key actually answers.

THE CHECK THAT THE METHOD WORKED is built in: MAME groups the fields into
ports named :row0..:row6, and every key must come back on the row its own port
tag predicts. If a key answers on a different row, the probe is reading the
PIA wrong and nothing it prints can be trusted.

Kept as a tool rather than run once and pasted, because a table nobody can
re-derive is a table nobody can check.
"""
import os, subprocess, sys, tempfile

HERE  = os.path.dirname(os.path.abspath(__file__))
COCO3 = os.path.dirname(HERE)
MAME  = os.path.expanduser("~/ample/Ample.app/Contents/MacOS/mame64")
ROMS  = os.path.expanduser("~/Library/Application Support/Ample/roms")

LUA = r'''
emu.wait(12)
local prog = manager.machine.devices[":maincpu"].spaces["program"]
local out = {}
-- The write and the read happen at the same scheduler point, so BASIC's own
-- IRQ scan cannot get between them and change the strobe.
local function probe()
    for col = 0, 7 do
        prog:write_u8(0xFF02, 0xFF ~ (1 << col))
        local r = prog:read_u8(0xFF00)
        for row = 0, 6 do
            if (r & (1 << row)) == 0 then return row, col end
        end
    end
    return nil, nil
end
for rt = 0, 6 do
    local port = manager.machine.ioport.ports[":row" .. rt]
    if port then
        local names = {}
        for fname, _ in pairs(port.fields) do
            if fname ~= "Keyboard" then names[#names+1] = fname end
        end
        table.sort(names)
        for _, fname in ipairs(names) do
            local f = port.fields[fname]
            f:set_value(1)
            emu.wait(0.02)
            local row, col = probe()
            f:set_value(0)
            emu.wait(0.01)
            out[#out+1] = string.format("%d\t%s\t%s\t%s", rt, fname,
                tostring(row), tostring(col))
        end
    end
end
local o = io.open(os.getenv("OUTF"), "w")
o:write(table.concat(out, "\n") .. "\n")
o:close()
'''


def main():
    if not os.path.exists(MAME):
        print("keymatrix: SKIPPED -- no MAME at %s" % MAME)
        return 0
    tmp = tempfile.mkdtemp(prefix="keymatrix")
    lua, outf = (os.path.join(tmp, n) for n in ("run.lua", "out.txt"))
    open(lua, "w").write(LUA)
    subprocess.run([MAME, "coco3", "-window", "-skip_gameinfo", "-rompath", ROMS,
                    "-ext", "multi", "-ext:multi:slot1", "ssfm",
                    "-ext:multi:slot4", "fdc",
                    "-autoboot_script", lua, "-autoboot_delay", "1",
                    "-seconds_to_run", "40", "-nothrottle"],
                   env=dict(os.environ, OUTF=outf),
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
                   cwd=tmp, check=False)
    if not os.path.exists(outf):
        sys.exit("keymatrix: the run produced no output")

    grid = {}
    bad = []
    n = 0
    for line in open(outf):
        parts = line.rstrip("\n").split("\t")
        if len(parts) != 4:
            continue
        tag, name, row, col = parts
        n += 1
        if row == "nil" or col == "nil":
            bad.append("%s answered on no column at all" % name)
            continue
        if row != tag:
            bad.append("%s is in port :row%s but answered on row %s" % (name, tag, row))
        grid[(int(row), int(col))] = name.split()[0]

    print("keymatrix: %d keys held, %d placed" % (n, len(grid)))
    print("        " + "".join("col%-4d" % c for c in range(8)))
    for r in range(7):
        print("  row%d  " % r + "".join("%-7s" % grid.get((r, c), "-")
                                        for c in range(8)))
    if bad:
        for b in bad:
            print("keymatrix: FAIL -- %s" % b)
        return 1
    if len(grid) != n:
        print("keymatrix: FAIL -- %d keys did not place" % (n - len(grid)))
        return 1
    print("keymatrix: every key answered on the row its own port tag predicts")
    return 0


sys.exit(main())
