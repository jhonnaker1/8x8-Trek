-- Does a Lua write to $FF78/$FF79 actually reach the V9958? Write VRAM and
-- read it back through the same ports. If this round-trips, the ports are
-- fine and any black screen is my register values; if it does not, Lua's
-- accessors are the problem and the probe has to be 6809 code.
local frames, done = 0, false
local sp
local function w(a,v) sp:write_u8(a,v) end
local function r(a) return sp:read_u8(a) end

emu.add_machine_frame_notifier(function()
  frames = frames + 1
  if frames ~= 180 or done then return end
  done = true
  sp = manager.machine.devices[":maincpu"].spaces["program"]

  -- R#14 = 0, write address $0100
  w(0xFF79, 0); w(0xFF79, 0x80 + 14)
  w(0xFF79, 0x00); w(0xFF79, 0x40 + 0x01)
  w(0xFF78, 0xDE); w(0xFF78, 0xAD); w(0xFF78, 0xBE); w(0xFF78, 0xEF)
  -- read address $0100
  w(0xFF79, 0x00); w(0xFF79, 0x01)
  print(string.format("VRAM READBACK %02X %02X %02X %02X",
                      r(0xFF78), r(0xFF78), r(0xFF78), r(0xFF78)))
  print(string.format("STATUS $FF79 = %02X", r(0xFF79)))
end)
