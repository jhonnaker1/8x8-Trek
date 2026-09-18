local F = dofile("tools/f256.lua").attach(tonumber(os.getenv("A_SIG"), 16))
local function sym(n) return tonumber(os.getenv(n), 16) end
local RAN, CH, N, LO, HI, WAS, NOW =
  sym("A_RAN"), sym("A_CHANGED"), sym("A_CHN"), sym("A_CHLO"), sym("A_CHHI"),
  sym("A_CHWAS"), sym("A_CHNOW")
local OKW, OKR, OKB, OKF, OKS, FR =
  sym("A_OKW"), sym("A_OKR"), sym("A_OKB"), sym("A_OKF"), sym("A_OKS"), sym("A_FR")

if not F.launch("lowprobe") then print("LAUNCH FAILED") manager.machine:exit() return end

-- The stage marker: $11 started, $20 filled, $30 exercised, $5A scanned.
-- A machine that dies mid-fill is the honest failure here, so watch the stage
-- rather than waiting a fixed time and reading whatever is there.
local stage, names = 0, {[0x11]="started", [0x20]="filled", [0x30]="exercised", [0x5A]="scanned"}
for _ = 1, 120 do
    emu.wait(0.25)
    local v = F.u8(RAN)
    if v and v ~= stage then stage = v print("  stage: " .. (names[v] or string.format("$%02X", v))) end
    if stage == 0x5A then break end
end
if stage ~= 0x5A then
    print(string.format("STOPPED AT $%02X -- the kernel did not survive the fill", stage))
    manager.machine:exit() return
end

print("")
print("DID THE KERNEL STILL WORK WITH $0400-$1FFF FULL OF GARBAGE?")
local function say(l, v) print(string.format("  %-34s %s", l, v == 1 and "yes" or "**NO**")) end
say("file write",              F.u8(OKW))
say("file read back",          F.u8(OKR))
say("every byte correct",      F.u8(OKB))
say("streaming read",          F.u8(OKS))
say("frame timer advancing",   F.u8(OKF))
print(string.format("  frames counted during the run:     %d", F.u16(FR)))

local changed = F.u16(CH)
print("")
print(string.format("BYTES THE KERNEL WROTE INTO $0400-$1FFF: %d of %d", changed, 0x1C00))
local n = F.u8(N)
if n > 0 then
    local lo, hi, was, now = F.read(LO, n), F.read(HI, n), F.read(WAS, n), F.read(NOW, n)
    print("  first few:")
    for i = 1, n do
        print(string.format("    $%02X%02X  was $%02X  now $%02X", hi[i], lo[i], was[i], now[i]))
    end
end
manager.machine.video:snapshot()
manager.machine:exit()
