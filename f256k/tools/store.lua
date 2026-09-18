-- The storage round trip. Types three keys BEFORE the file work so the last
-- check can prove the disk path did not eat them.
local F = dofile("tools/f256.lua").attach(tonumber(os.getenv("A_SIG"), 16))
local function sym(n) return tonumber(os.getenv(n), 16) end
local RES, DET, N = sym("A_RES"), sym("A_DET"), sym("A_N")

local ok = F.launch("storetest")
if not ok then print("LAUNCH FAILED") manager.machine:exit() return end

-- WAIT FOR kb_init BEFORE TYPING. The signature lives in .data, so it appears
-- the moment the PGZ is LOADED -- which is before main() runs, and kb_init
-- drains the queue. Typing on the signature alone meant the keys were
-- sometimes drained and sometimes not: the check passed three runs in a row
-- and then failed on an identical binary. `ran` reaching $5A is set AFTER
-- kb_init, which is the event that actually matters here.
local RAN = tonumber(os.getenv("A_RAN"), 16)
for _ = 1, 200 do if F.u8(RAN) == 0x5A then break end emu.wait(0.05) end
if F.u8(RAN) ~= 0x5A then print("program never became ready") end
manager.machine.natkeyboard:post("ABC")

local labels = {
    "BSS REACHES ABOVE $A000", "WRITE 600 BYTES", "READ IT BACK", "LENGTH IS 600", "EVERY BYTE MATCHES",
    "MISSING FILE IS NOTFOUND", "TOO BIG FOR BUFFER IS ERROR",
    "REWRITE SHORT", "REPLACED NOT APPENDED", "STREAMED 600 IN 140s",
    "KEYS SURVIVED THE DISK",
}

-- Wait for the program to finish its checks rather than guessing how long the
-- SD card takes.
local n = 0
for _ = 1, 30 do
    emu.wait(1)
    n = F.u8(N) or 0
    if n >= #labels then break end
end

local res = F.read(RES, #labels)
local det = F.read(DET, #labels * 2)
local bad = 0
print(string.format("%d of %d checks completed", n, #labels))
for i = 1, #labels do
    local done = i <= n
    local pass = done and res[i] == 1
    if not pass then bad = bad + 1 end
    print(string.format("  %-28s %s   detail $%04X", labels[i],
          done and (pass and "PASS" or "**FAIL**") or "(not reached)",
          det[i*2-1] + det[i*2]*256))
end
print(bad == 0 and "STORAGE: all ok" or string.format("STORAGE: %d PROBLEM(S)", bad))
manager.machine.video:snapshot()
manager.machine:exit()
