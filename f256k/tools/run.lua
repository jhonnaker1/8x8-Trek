-- Automated F256K run: boot, launch the PGZ from SuperBASIC the way a person
-- does, read the completion marker, snapshot, exit.
--
-- THE MARKER ADDRESS COMES FROM THE ELF, via $TREK_MARKER -- a hard-coded one
-- goes stale on the next build and then reports a byte of somebody else's data
-- as if it were the answer.
local marker = tonumber(os.getenv("TREK_MARKER") or "0", 16)
local name   = os.getenv("TREK_NAME") or "hello"

emu.wait(6)
manager.machine.natkeyboard:post("/- " .. name .. "\n")
emu.wait(10)

if marker ~= 0 then
    local sp = manager.machine.devices[":maincpu"].spaces["program"]
    print(string.format("MARKER $%04X = $%02X   (11=started 5A=finished)",
                        marker, sp:read_u8(marker)))
end
manager.machine.video:snapshot()
manager.machine:exit()
