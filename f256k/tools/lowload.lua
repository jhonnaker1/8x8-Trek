local F = dofile("tools/f256.lua").attach(tonumber(os.getenv("A_SIG"), 16))
local function sym(n) return tonumber(os.getenv(n), 16) end
local RAN, BAD, FB, FBH, WB, NB = sym("A_RAN"), sym("A_BAD"), sym("A_FB"), sym("A_FBH"), sym("A_WB"), sym("A_NB")
local ok = F.launch("lowload")
if not ok then
    print("PEXEC DID NOT SURVIVE a segment landing in its own workspace at $0400.")
    print("  -> low memory must be COPIED DOWN at startup, not loaded into.")
    manager.machine:exit() return
end
for _ = 1, 40 do if F.u8(RAN) == 0x5A then break end emu.wait(0.1) end
local bad = F.u16(BAD)
print(string.format("marker $%02X", F.u8(RAN)))
if bad == 0 then
    print("A PGZ SEGMENT REACHES $0400-$1FFF INTACT -- all 7168 bytes.")
else
    print(string.format("%d of 7168 bytes wrong; first at $%04X (wanted $%02X, got $%02X)",
          bad, 0x0400 + F.u8(FBH)*256 + F.u8(FB), F.u8(WB), F.u8(NB)))
end
manager.machine.video:snapshot()
manager.machine:exit()
