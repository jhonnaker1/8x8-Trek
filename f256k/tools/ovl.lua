-- Eleven overlays through eleven RAM banks, checked by VALUE.
local F = dofile("tools/f256.lua").attach(tonumber(os.getenv("A_SIG"), 16))
local function sym(n) return tonumber(os.getenv(n), 16) end
local RAN, GOT, BACK, MIX, PASS = sym("A_RAN"), sym("A_GOT"), sym("A_BACK"), sym("A_MIX"), sym("A_PASS")

if not F.launch("ovltest") then print("LAUNCH FAILED") manager.machine:exit() return end
for _ = 1, 80 do if F.u8(RAN) == 0x5A then break end emu.wait(0.1) end
emu.wait(2)

local fwd, back, mix = F.read(GOT, 11), F.read(BACK, 11), F.read(MIX, 8)
local bad = 0
print("overlay  forwards  backwards   (each returns its own number)")
for i = 1, 11 do
    local want = 0xE0 + i - 1
    local okf, okb = fwd[i] == want, back[i] == want
    if not (okf and okb) then bad = bad + 1 end
    print(string.format("   %2d      $%02X %-3s   $%02X %-3s   want $%02X",
          i - 1, fwd[i], okf and "ok" or "BAD", back[i], okb and "ok" or "BAD", want))
end
local mixwant = {0xE8, 0xE8, 0xE0, 0xEA, 0xE0, 0xE1, 0xEA, 0xE8}
local mixok = true
for i = 1, 8 do if mix[i] ~= mixwant[i] then mixok = false end end
print(string.format("interleaved (incl. the same overlay twice): %s",
      mixok and "ok" or "**MISMATCH**"))
if not mixok then
    local a, b = {}, {}
    for i = 1, 8 do a[i] = string.format("$%02X", mix[i]); b[i] = string.format("$%02X", mixwant[i]) end
    print("   got  " .. table.concat(a, " ")); print("   want " .. table.concat(b, " "))
end
print((bad == 0 and mixok and F.u8(PASS) == 1) and "OVERLAYS: all ok"
      or string.format("OVERLAYS: %d PROBLEM(S)", bad + (mixok and 0 or 1)))
manager.machine.video:snapshot()
manager.machine:exit()
