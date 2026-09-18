-- Type a known set of keys at the probe and read the log back.
--
-- THE CURSOR KEYS ARE DRIVEN THROUGH THE IOPORT, NOT THE NATURAL KEYBOARD.
-- post_coded took {ENTER} and typed {ESC} and {UP} as literal characters --
-- the log came back holding '{', 'E', 'S'... which reads as a machine that
-- has no escape key rather than as a host that did not send one. MAME's own
-- ROW0 has the two arrows as fields; setting them is unambiguous.
--
-- They are the two keys the port CANNOT get from ASCII: input.h gives KB_UP
-- and KB_DOWN the values 1 and 2 precisely because no ASCII code exists for
-- them, and the game needs both -- shields up and down, and scrolling the
-- message log.
local F = dofile("tools/f256.lua").attach(tonumber(os.getenv("A_SIG"), 16))
local function sym(n) return tonumber(os.getenv(n), 16) end
local PRE, TOTAL, N = sym("A_PRE"), sym("A_TOTAL"), sym("A_N")
local T, R, A, FL = sym("A_TYPE"), sym("A_RAW"), sym("A_ASCII"), sym("A_FLAGS")

local ok, tries = F.launch("keyprobe")
print(string.format("launched: %s, %d attempt(s)", tostring(ok), tries))
if not ok then manager.machine:exit() return end
print(string.format("PRE-QUEUED AT STARTUP: %d", F.u8(PRE)))

local row0 = manager.machine.ioport.ports[":ROW0"]
print("ROW0 fields:")
for name, fld in pairs(row0.fields) do
    print(string.format("   mask $%03X  %q", fld.mask, name))
end

local function tap(field, label)
    local f = row0.fields[field]
    if not f then print("   -- no field " .. label) return end
    f:set_value(0)          -- IP_ACTIVE_LOW: pressed
    emu.wait(0.15)
    f:set_value(1)
    emu.wait(0.15)
end

manager.machine.natkeyboard:post("Q")
emu.wait(0.5)
local before = F.u8(N)
tap(os.getenv("A_UPNAME"), "UP")
tap(os.getenv("A_DOWNNAME"), "DOWN")
emu.wait(1)

local n = F.u8(N)
print(string.format("total events: %d, logged %d (arrows begin at %d)",
                    F.u16(TOTAL), n, before))
print("   #  type  raw  ascii  char  flags")
local ty, raw, asc, fl = F.read(T, n), F.read(R, n), F.read(A, n), F.read(FL, n)
for i = 1, n do
    local a = asc[i]
    local ch = (a >= 32 and a < 127) and string.char(a) or "."
    print(string.format("  %2d   %3d  $%02X   $%02X    %s     $%02X%s%s",
          i - 1, ty[i], raw[i], a, ch, fl[i],
          (fl[i] >= 0x80) and "  NO ASCII" or "",
          (i - 1 >= before) and "   <-- ARROW" or ""))
end
manager.machine.video:snapshot()
manager.machine:exit()
