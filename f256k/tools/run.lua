-- Automated F256K run: boot, launch the PGZ from SuperBASIC the way a person
-- does, read the completion marker, snapshot, exit.
--
-- THE MARKER ADDRESS COMES FROM THE ELF, via $TREK_MARKER -- a hard-coded one
-- goes stale on the next build and reports a byte of somebody else's data as
-- the answer.
--
-- AND IT IS READ THROUGH tools/f256.lua, NOT DIRECTLY. FoenixMCP's IRQ remaps
-- the MMU sixty times a second, and since the overlay split put .bss at $0400
-- -- in slot 0, which the kernel's own maps point at a different bank -- a raw
-- read lands in the kernel's memory as often as in ours. That is what made
-- `make check P=frametest` report marker $00 for a program that was running
-- perfectly: the value was real, it just belonged to someone else.
local F = dofile("tools/f256.lua").attach(tonumber(os.getenv("TREK_SIG") or "", 16))
local marker = tonumber(os.getenv("TREK_MARKER") or "0", 16)
local name   = os.getenv("TREK_NAME") or "hello"

local ok, tries = F.launch(name)
if marker == 0 then
    print(string.format("(no marker symbol; %d launch attempt%s)", tries, tries == 1 and "" or "s"))
else
    -- LAUNCH may report false for a probe with no signature; fall through and
    -- let the marker speak, rather than calling a working run a failure.
    for _ = 1, 40 do if F.u8(marker) == 0x5A then break end emu.wait(0.25) end
    local v = F.u8(marker)
    print(string.format("MARKER $%04X = $%02X   (11=started 5A=finished, %d launch attempt%s)",
                        marker, v, tries, tries == 1 and "" or "s"))
    if v ~= 0x11 and v ~= 0x5A then
        print("LAUNCH FAILED -- the marker never reached $11 or $5A.")
    end
end
manager.machine.video:snapshot()
manager.machine:exit()
