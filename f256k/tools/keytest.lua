-- Type a chosen sequence and check what kb_waitkey made of it.
--
-- EXPECTED VALUES ARE STATED HERE, not eyeballed from the output. A test that
-- prints numbers and leaves me to judge them is one I will agree with; a test
-- that says PASS or FAIL against the shared header's own constants is one
-- that can tell me I am wrong.
local F = dofile("tools/f256.lua").attach(tonumber(os.getenv("A_SIG"), 16))
local function sym(n) return tonumber(os.getenv(n), 16) end
local GOT, GOTN, ENT, LOST = sym("A_GOT"), sym("A_GOTN"), sym("A_ENT"), sym("A_LOST")

local ok = F.launch("keytest")
if not ok then print("LAUNCH FAILED") manager.machine:exit() return end

local kb = manager.machine.natkeyboard
local function tap(row, field)
    local p = manager.machine.ioport.ports[":" .. row]
    local f = p and p.fields[field]
    if not f then print("   -- missing " .. row .. "/" .. field) return end
    f:set_value(0); emu.wait(0.15); f:set_value(1); emu.wait(0.25)
end

-- name, how to send it, what kb_waitkey must return
local plan = {
    { "q",        function() kb:post("q") end,            81 },  -- case fold
    { "A",        function() kb:post("A") end,            65 },  -- already upper
    { "5",        function() kb:post("5") end,            53 },
    { "space",    function() kb:post(" ") end,            32 },
    { "ENTER",    function() tap("ROW0", "ENTER") end,    13 },
    { "CRSR UP",  function() tap("ROW0", "\u{2191}") end,  1 },
    { "CRSR DN",  function() tap("ROW0", "\u{2193}") end,  2 },
    { "RUN/STOP", function() tap("ROW7", "RUN/STOP") end, 27 },
    { "DEL",      function() tap("ROW0", "DEL") end,      20 },
}
for _, step in ipairs(plan) do step[2]() emu.wait(0.4) end
emu.wait(1)

local n = F.u8(GOTN)
local got = F.read(GOT, math.max(n, 1))
print(string.format("kb_entropy reached %d, unclaimed non-key events lost: %d",
                    F.u16(ENT), F.u8(LOST)))
print(string.format("%d keys expected, %d returned", #plan, n))
local bad = 0
for i = 1, #plan do
    local v = (i <= n) and got[i] or nil
    local okk = (v == plan[i][3])
    if not okk then bad = bad + 1 end
    print(string.format("  %-9s expected %3d  got %s   %s",
          plan[i][1], plan[i][3], v and string.format("%3d", v) or " --",
          okk and "ok" or "**MISMATCH**"))
end
if n ~= #plan then bad = bad + 1 end
print(bad == 0 and "KEYBOARD: all ok" or string.format("KEYBOARD: %d PROBLEM(S)", bad))
manager.machine.video:snapshot()
manager.machine:exit()
