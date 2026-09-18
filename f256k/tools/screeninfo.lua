-- Minimal and DEFENSIVE. The first version of this script printed nothing and
-- left MAME running: an autoboot script that throws dies silently, so every
-- line here is guarded and says so when it fails. An instrument that cannot
-- report its own failure is the thing this project keeps a list of.
local function try(label, f)
    local ok, v = pcall(f)
    print(string.format("%-14s %s", label, ok and tostring(v) or ("ERR " .. tostring(v))))
end

local scr
for _, s in pairs(manager.machine.screens) do scr = s end
try("screen?",   function() return scr ~= nil end)
try("width",     function() return scr.width end)
try("height",    function() return scr.height end)
try("refresh",   function() return scr.refresh end)
try("frame_per", function() return scr.frame_period end)

emu.wait(6)
manager.machine.natkeyboard:post("/- vsyncprobe\n")
emu.wait(9)

local sp = manager.machine.devices[":maincpu"].spaces["program"]
print("  MAME vpos | $D01B:$D01A = combined")
for i = 1, 10 do
    local ok, msg = pcall(function()
        local v = scr:vpos()
        local lo, hi = sp:read_u8(0xD01A), sp:read_u8(0xD01B)
        return string.format("     %4d   |    %d:%02X   = %4d", v, hi, lo, hi * 256 + lo)
    end)
    print(ok and msg or ("ERR " .. tostring(msg)))
    emu.wait(0.004)
end
manager.machine:exit()
