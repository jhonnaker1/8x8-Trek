-- Measure a running program's own frame counter against MAME's clock.
local sp = manager.machine.devices[":maincpu"].spaces["program"]
local RAN, FRAMES = tonumber(os.getenv("A_RAN"),16), tonumber(os.getenv("A_FRAMES"),16)
local function u16(a) return sp:read_u8(a) + sp:read_u8(a+1)*256 end
-- $11 or $5A, never merely non-zero: see run.lua.
local function signed_in() local v = sp:read_u8(RAN) return v == 0x11 or v == 0x5A end
emu.wait(6)
local tries = 0
repeat
    tries = tries + 1
    manager.machine.natkeyboard:post("/- " .. os.getenv("A_NAME") .. "\n")
    for _ = 1, 6 do emu.wait(1) if signed_in() then break end end
until signed_in() or tries >= 3
emu.wait(2)
local t0, n0 = emu.time(), u16(FRAMES)
emu.wait(5)
local t1, n1 = emu.time(), u16(FRAMES)
print(string.format("marker $%02X, %d launch attempt(s)", sp:read_u8(RAN), tries))
print(string.format("wait_vsync IN THE DRIVER: %d frames / %.3f s = %.2f Hz",
                    (n1-n0) % 65536, t1-t0, ((n1-n0) % 65536) / (t1-t0)))
manager.machine.video:snapshot()
manager.machine:exit()
