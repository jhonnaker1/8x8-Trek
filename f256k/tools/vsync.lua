-- Calibrate the F256K's frame timer against MAME's own clock.
--
-- emu.wait() counts EMULATED seconds, which is the right clock here: the
-- question is how many times the line counter wraps per second of machine
-- time, and -nothrottle changes only how long I wait for the answer.
--
-- TWO READS AND A DIFFERENCE, not one read and a division. The probe spends
-- its first second finding the top of the count, so a single total divided by
-- elapsed time would be low by however long that took -- and this project has
-- a memory entry about calibrating with one point.
local sp = manager.machine.devices[":maincpu"].spaces["program"]
local VSYNCS   = tonumber(os.getenv("A_VSYNCS"), 16)
local LINE_MAX = tonumber(os.getenv("A_LINEMAX"), 16)
local RAN      = tonumber(os.getenv("A_RAN"), 16)
local SPINS    = tonumber(os.getenv("A_SPINS"), 16)

local function u16(a) return sp:read_u8(a) + sp:read_u8(a + 1) * 256 end

emu.wait(6)
-- Launch until it takes, for the reason run.lua now spells out: a post at a
-- fixed time silently misses and every number below then reads zero.
local tries = 0
repeat
    tries = tries + 1
    manager.machine.natkeyboard:post("/- vsyncprobe\n")
    for _ = 1, 6 do emu.wait(1) if sp:read_u8(RAN) ~= 0 then break end end
until sp:read_u8(RAN) ~= 0 or tries >= 3
print(string.format("launched in %d attempt(s)", tries))
emu.wait(4)

local t0, n0 = emu.time(), u16(VSYNCS)
emu.wait(4)
local t1, n1 = emu.time(), u16(VSYNCS)

print(string.format("MARKER  = $%02X   (5A = at least one wrap seen)", sp:read_u8(RAN)))
print(string.format("LINEMAX = %d  ($%04X)", u16(LINE_MAX), u16(LINE_MAX)))
local d = (n1 - n0) % 65536
print(string.format("CROSS   = %d over %.3f emulated seconds", d, t1 - t0))
print(string.format("RATE    = %.2f Hz   (MAME reports the screen at 60.00)", d / (t1 - t0)))
local s0 = sp:read_u8(SPINS) + sp:read_u8(SPINS+1)*256 + sp:read_u8(SPINS+2)*65536
print(string.format("SAMPLES = %d total -- the loop's own rate, so aliasing is visible", s0))
manager.machine.video:snapshot()
manager.machine:exit()
