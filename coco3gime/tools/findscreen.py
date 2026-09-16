#!/usr/bin/env python3
"""Where is the 80-column text screen, and how is a cell laid out?

NOT READ OFF A DATASHEET AND NOT GUESSED. Super Extended Color BASIC already
programs the GIME correctly for `WIDTH 80`; this prints a string no ROM
contains and then SEARCHES MEMORY FOR IT from MAME's debugger. Whatever
address it turns up at IS the screen, and the bytes around it show whether a
cell is character+attribute interleaved or two separate planes.

The GIME's own registers are no help: coco3/tools and NOTES.md item 30 record
that $FF90 and $FF91 read back $1B (floating bus) on this machine whatever was
written, so PEEK cannot answer any of this.
"""
import os, re, subprocess, sys, tempfile

MAME = os.path.expanduser("~/ample/Ample.app/Contents/MacOS/mame64")
ROMS = os.path.expanduser("~/Library/Application Support/Ample/roms")
NEEDLE = "QXQXQXQX"

LUA = r'''
emu.wait(16)
local kbd = manager.machine.natkeyboard
kbd.in_use = true
kbd:post_coded("WIDTH 80{ENTER}")       emu.wait(3)
kbd:post_coded("CLS{ENTER}")            emu.wait(2)
kbd:post_coded('PRINT "%(needle)s"{ENTER}')  emu.wait(3)

local cpu  = manager.machine.devices[":maincpu"]
local prog = cpu.spaces["program"]
local f = io.open("%(out)s", "w")

-- The needle, as the SCREEN would hold it. The CoCo 3 text screen stores
-- plain ASCII, so search for the literal bytes first; if that misses, the
-- screen is storing something else and the dump below will say what.
local pat = {}
for i = 1, #"%(needle)s" do pat[i] = string.byte("%(needle)s", i) end

local function hit(a, step)
  for i = 1, #pat do
    if prog:read_u8(a + (i-1)*step) ~= pat[i] then return false end
  end
  return true
end

for a = 0x0000, 0xFEFF do
  if hit(a, 1) then f:write(string.format("FOUND step=1 at %%04X\n", a)) end
  if hit(a, 2) then f:write(string.format("FOUND step=2 at %%04X\n", a)) end
end

-- 64 bytes from wherever the first hit was, so the cell layout is visible.
for a = 0x0000, 0xFEFF do
  if hit(a, 2) then
    f:write("DUMP from " .. string.format("%%04X", a - 8) .. ":\n")
    for r = 0, 5 do
      local line = ""
      for c = 0, 15 do
        line = line .. string.format("%%02X ", prog:read_u8(a - 8 + r*16 + c))
      end
      f:write(line .. "\n")
    end
    break
  end
end
f:close()
manager.machine.video:snapshot()
emu.wait(0.5)
manager.machine:exit()
'''


def main():
    tmp = tempfile.mkdtemp()
    outf = os.path.join(tmp, "found.txt")
    luaf = os.path.join(tmp, "p.lua")
    open(luaf, "w").write(LUA % dict(needle=NEEDLE, out=outf))
    p = subprocess.run(
        [MAME, "coco3", "-window", "-skip_gameinfo", "-rompath", ROMS,
         "-autoboot_script", luaf, "-autoboot_delay", "1",
         "-seconds_to_run", "60", "-nothrottle", "-snapshot_directory", tmp],
        capture_output=True, text=True, cwd=tmp, check=False)
    if not os.path.exists(outf):
        print("findscreen: the probe wrote nothing. MAME said:")
        print((p.stdout + p.stderr).strip()[-800:])
        return 1
    txt = open(outf).read().strip()
    print(txt if txt else "findscreen: the needle is NOT IN MEMORY as plain "
          "ASCII at either stride -- the screen stores something else, and "
          "that is the finding")
    import shutil
    shots = [os.path.join(dp, f) for dp, dn, fn in os.walk(tmp)
             for f in fn if f.endswith(".png")]
    if shots:
        os.makedirs("build", exist_ok=True)
        shutil.copy(sorted(shots)[-1], "build/findscreen.png")
        print("  screen: build/findscreen.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
