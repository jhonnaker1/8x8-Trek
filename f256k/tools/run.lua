-- Automated F256K run: boot, launch the PGZ from SuperBASIC the way a person
-- does, read the completion marker, snapshot, exit.
--
-- THE MARKER ADDRESS COMES FROM THE ELF, via $TREK_MARKER -- a hard-coded one
-- goes stale on the next build and then reports a byte of somebody else's data
-- as if it were the answer.
--
-- AND THE LAUNCH IS RETRIED UNTIL IT TAKES. Posting `/- name` at a fixed six
-- seconds worked most of the time and silently did nothing the rest: the
-- keystrokes land before SuperBASIC is accepting, the program never starts,
-- and the marker reads $00 -- which is indistinguishable from a program that
-- started and crashed. It cost two contradictory readings of the same binary
-- before the staged read showed it running perfectly well.
--
-- So don't time the boot, WATCH FOR IT. Poll the marker; if the program has
-- not signed in, type it again. A rig that reports how many attempts it took
-- can go flaky in front of me instead of behind me.
local marker = tonumber(os.getenv("TREK_MARKER") or "0", 16)
local name   = os.getenv("TREK_NAME") or "hello"
local sp

local function mark()
    if marker == 0 then return nil end
    sp = sp or manager.machine.devices[":maincpu"].spaces["program"]
    return sp:read_u8(marker)
end

emu.wait(6)

local tries, started = 0, false
repeat
    tries = tries + 1
    manager.machine.natkeyboard:post("/- " .. name .. "\n")
    for _ = 1, 6 do
        emu.wait(1)
        if marker == 0 or mark() ~= 0 then started = true break end
    end
until started or tries >= 3

emu.wait(3)   -- let it get past whatever it does after signing in

if marker ~= 0 then
    print(string.format("MARKER $%04X = $%02X   (11=started 5A=finished, %d launch attempt%s)",
                        marker, mark(), tries, tries == 1 and "" or "s"))
    if mark() == 0 then
        print("LAUNCH FAILED -- three attempts and the program never signed in.")
    end
else
    print(string.format("(no marker symbol; %d launch attempt%s)", tries, tries == 1 and "" or "s"))
end
manager.machine.video:snapshot()
manager.machine:exit()
