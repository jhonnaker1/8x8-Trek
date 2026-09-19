-- Drive the game from the title screen into a live console.
--
-- THE KEYS COME FROM THE X16'S AUTOPLAY TABLE, which is the sequence that
-- port already proved reaches a playing game: title, no briefing, no restore,
-- a name, a skill level, accept. Typing them here rather than compiling them
-- in means this exercises the real keyboard seam, not a table.
local F = dofile("tools/f256.lua").attach(tonumber(os.getenv("A_SIG"), 16))
local kb = manager.machine.natkeyboard

if not F.launch("egatrek") then print("LAUNCH FAILED") manager.machine:exit() return end
emu.wait(3)
manager.machine.video:snapshot()      -- the title

local function send(s, wait)
    kb:post(s)
    emu.wait(wait or 1.5)
end

send("\n")                 -- title -> setup
send("n\n")                -- no briefing
send("n\n")                -- no restored game
send("jamie\n")            -- commander
send("3\n")                -- skill level
send("x\n", 4)             -- accept, and the galaxy is generated
manager.machine.video:snapshot()      -- the console, with live state

-- A command with a visible effect: warp to a neighbouring sector.
send("w5\n", 3)
manager.machine.video:snapshot()

-- And the short-range scan redrawn after a turn.
send("c", 2)               -- the galaxy chart; any key returns
manager.machine.video:snapshot()
send(" ", 2)
manager.machine.video:snapshot()
manager.machine:exit()
