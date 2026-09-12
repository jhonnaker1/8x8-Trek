-- Draw a bracket into VRAM, then READ IT BACK and write it to a file. The
-- host reconstructs the bitmap and looks at it. MAME will not show the card's
-- screen, but the driver's job is to put the right bytes in VRAM and that is
-- checkable directly -- the same move as confirming the Falcon's geometry by
-- drawing rather than trusting a byte count.
local frames, done = 0, false
local W, H = 256, 212        -- GRAPHIC6: 256 bytes a line, 2 pixels a byte
emu.add_machine_frame_notifier(function()
  frames = frames + 1
  if frames ~= 150 or done then return end
  done = true
  local sp = manager.machine.devices[":maincpu"].spaces["program"]
  local function reg(r,v) sp:write_u8(0xFF79,v); sp:write_u8(0xFF79,0x80+r) end
  reg(0,0x0A); reg(1,0x40); reg(9,0x80); reg(8,0x08); reg(7,0x00)

  reg(14,0); sp:write_u8(0xFF79,0x00); sp:write_u8(0xFF79,0x40)
  for y = 0, H-1 do
    for x = 0, W-1 do
      local v = 0x44
      if y < 8 or y >= H-8 then v = 0x11 end
      if x < 4 then v = 0x11 end
      sp:write_u8(0xFF78, v)
    end
  end

  -- read it all back
  reg(14,0); sp:write_u8(0xFF79,0x00); sp:write_u8(0xFF79,0x00)
  local f = io.open("vram.bin","wb")
  local n = W*H
  for i = 1, n do f:write(string.char(sp:read_u8(0xFF78))) end
  f:close()
  print("VRAM DUMPED " .. n .. " bytes")
end)
