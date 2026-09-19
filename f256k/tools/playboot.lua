-- Boot, launch the game, and then GET OUT OF THE WAY.
--
-- `make play` used to print "at the SuperBASIC prompt: /- egatrek" and leave a
-- person to type it. Jamie ran it and got the prompt, which is exactly what
-- that design produces when the instruction is in a terminal the player is not
-- reading. A player should never have to type a loader incantation -- and the
-- launch logic already exists, retries and all, for the automated runs.
--
-- IT DOES NOT EXIT. Every other script here ends with machine:exit() because
-- it has an answer to report; this one's whole job is to hand over a running
-- machine. See the standing rule: a tool that gets a machine somewhere
-- interesting must LEAVE IT THERE.
local F = dofile("tools/f256.lua").attach(tonumber(os.getenv("TREK_SIG") or "", 16))
local ok, tries = F.launch(os.getenv("TREK_NAME") or "egatrek")
if ok then
    print(string.format(">>> EGA TREK is running (%d launch attempt%s). The keyboard is yours.",
                        tries, tries == 1 and "" or "s"))
    print(">>> RUN/STOP is the ESC key on this machine; arrows are shields.")
else
    print(">>> THE LAUNCH DID NOT TAKE. At the prompt, type:  /- egatrek")
end
