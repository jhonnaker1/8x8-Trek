-- Calibrate the kernel's clock.TICK against MAME's clock. Two reads and a
-- difference, over a known emulated interval.
local sp = manager.machine.devices[":maincpu"].spaces["program"]
local function sym(n) return tonumber(os.getenv(n), 16) end
local RAN, TICKS, KEYS, OTHERS, LAST, TYPE =
      sym("A_RAN"), sym("A_TICKS"), sym("A_KEYS"), sym("A_OTHERS"), sym("A_LAST"), sym("A_TYPE")
local function u16(a) return sp:read_u8(a) + sp:read_u8(a + 1) * 256 end

emu.wait(6)
local tries = 0
repeat
    tries = tries + 1
    manager.machine.natkeyboard:post("/- tickprobe\n")
    for _ = 1, 6 do emu.wait(1) if sp:read_u8(RAN) ~= 0 then break end end
until sp:read_u8(RAN) ~= 0 or tries >= 3
print(string.format("launched in %d attempt(s), marker $%02X", tries, sp:read_u8(RAN)))

local t0, n0 = emu.time(), u16(TICKS)
emu.wait(5)
local t1, n1 = emu.time(), u16(TICKS)
local d, dt = (n1 - n0) % 65536, t1 - t0

print(string.format("kernel frame counter, low byte = %d", sp:read_u8(TYPE)))
print(string.format("FRAMES = %d over %.3f s  ->  %.2f Hz", d, dt, d / dt))
print(string.format("KEYS   = %d   OTHER = %d (last type %d)",
                    u16(KEYS), u16(OTHERS), sp:read_u8(LAST)))
manager.machine.video:snapshot()
manager.machine:exit()
