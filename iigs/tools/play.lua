-- Drive the game: boot, wait, press keys, snapshot after each.
--
-- THE KEYBOARD IS AN ADB KEYBOARD, at :macadb:KEY0..KEY7, and that is why the
-- obvious routes do not work. natkeyboard:post_coded() is ACCEPTED AND DOES
-- NOTHING here -- the first attempt posted RETURN, reported no error, and the
-- title screen sat there with $C000 still reading $00, which is the hardware
-- latch saying no key ever arrived. And $C000 cannot be poked: it is a
-- READ-ONLY soft switch whose write side means something else entirely.
--
-- So a key is pressed the way a key is pressed: find the ioport field by name
-- and hold it down for a moment.
local WAIT = tonumber(os.getenv("GS_WAIT") or "22")
local KEYS = os.getenv("GS_KEYS") or ""
local STEP = tonumber(os.getenv("GS_STEP") or "6")

emu.wait(WAIT)
local cpu = manager.machine.devices[":maincpu"]
local sp  = cpu.spaces["program"]

-- Field names are what the ports call them: "Return", "Space", "a  A", "1  !".
local function find_field(want)
  for tag, port in pairs(manager.machine.ioport.ports) do
    if tag:find("KEY") then
      for fname, f in pairs(port.fields) do
        if fname == want or fname:match("^(%S+)") == want then return f, tag end
      end
    end
  end
  return nil
end

local function press(want, hold)
  local f, tag = find_field(want)
  if not f then print("NO SUCH KEY: " .. want) return false end
  f:set_value(1)
  emu.wait(hold or 0.25)
  f:set_value(0)
  emu.wait(0.15)
  return true
end

print(string.format("TITLE PB=%02X PC=%04X C029=%02X",
      cpu.state["PB"].value, cpu.state["PC"].value, sp:read_u8(0xC029)))
manager.machine.video:snapshot()

for key in string.gmatch(KEYS, "[^,]+") do
  if press(key) then
    emu.wait(STEP)
    -- $C000 bit 7 is the keyboard latch: it says whether the key reached the
    -- HARDWARE, which is a different question from whether the game noticed.
    -- Only the first is the rig's business, and the first is what failed.
    print(string.format("AFTER %-8s PC=%04X C000=%02X", key,
          cpu.state["PC"].value, sp:read_u8(0xC000)))
    manager.machine.video:snapshot()
  end
end
manager.machine:exit()
