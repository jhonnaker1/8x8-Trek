local F = dofile("tools/f256.lua").attach(tonumber(os.getenv("A_SIG"), 16))
local function sym(n) return tonumber(os.getenv(n), 16) end
local RAN, RES, DET, N = sym("A_RAN"), sym("A_RES"), sym("A_DET"), sym("A_N")
local labels = {
    "LOAD 10000 BYTES", "FAR_SIZE IS 10000", "READ AT THE START",
    "READ ACROSS BANK 1", "READ THE LAST 50", "SECOND TENANT APPENDS",
    "SECOND TENANT READS BACK", "far_read FROM AN OVERLAY",
    "THE OVERLAY SURVIVED IT",
}
if not F.launch("fartest") then print("LAUNCH FAILED") manager.machine:exit() return end
local n = 0
for _ = 1, 40 do emu.wait(1) n = F.u8(N) or 0 if n >= #labels then break end end
local res, det = F.read(RES, #labels), F.read(DET, #labels * 2)
local bad = 0
print(string.format("%d of %d checks completed", n, #labels))
for i = 1, #labels do
    local done, pass = i <= n, (i <= n and res[i] == 1)
    if not pass then bad = bad + 1 end
    print(string.format("  %-26s %s   detail $%04X", labels[i],
          done and (pass and "PASS" or "**FAIL**") or "(not reached)",
          det[i*2-1] + det[i*2]*256))
end
print(bad == 0 and "FAR MEMORY: all ok" or string.format("FAR MEMORY: %d PROBLEM(S)", bad))
manager.machine.video:snapshot()
manager.machine:exit()
