-- Pitch and tempo in one run. The wav is MAME's business; this half counts
-- frames and ticks against emu.time(), which is the clock the music has to
-- agree with.
local F = dofile("tools/f256.lua").attach(tonumber(os.getenv("A_SIG"), 16))
local function sym(n) return tonumber(os.getenv(n), 16) end
local RAN, RUNNING, FRAMES, TICKS = sym("A_RAN"), sym("A_RUNNING"), sym("A_FRAMES"), sym("A_TICKS")

if not F.launch("sndtest") then print("LAUNCH FAILED") manager.machine:exit() return end

-- The four tone bursts run first; wait for the driver to reach the tempo
-- phase rather than timing it, so a change to the burst lengths cannot
-- silently move the measurement into the middle of them.
for _ = 1, 120 do if F.u8(RUNNING) == 1 then break end emu.wait(0.25) end
if F.u8(RUNNING) ~= 1 then print("never reached the tempo phase") manager.machine:exit() return end

emu.wait(1)                       -- let it settle past the phase boundary
local t0, f0, k0 = emu.time(), F.u16(FRAMES), F.u16(TICKS)
emu.wait(10)
local t1, f1, k1 = emu.time(), F.u16(FRAMES), F.u16(TICKS)
local dt = t1 - t0
local df, dk = (f1 - f0) % 65536, (k1 - k0) % 65536

print(string.format("TEMPO over %.3f emulated seconds:", dt))
print(string.format("  frames %5d  ->  %6.2f Hz   (want 60.00)", df, df / dt))
print(string.format("  ticks  %5d  ->  %6.3f Hz   (want 18.207)", dk, dk / dt))
local fok = math.abs(df / dt - 60.0) < 0.6          -- 1%
local kok = math.abs(dk / dt - 18.2065) < 0.19      -- 1%
print(fok and kok and "TEMPO: ok" or "TEMPO: **OUT OF RANGE**")
manager.machine:exit()
