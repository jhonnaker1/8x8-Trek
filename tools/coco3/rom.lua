local frames, done = 0, false
emu.add_machine_frame_notifier(function()
  frames = frames + 1
  if frames ~= 150 or done then return end
  done = true
  local sp = manager.machine.devices[":maincpu"].spaces["program"]
  local f = io.open("rom.bin","wb")
  for a = 0x8000, 0xFEFF do f:write(string.char(sp:read_u8(a))) end
  f:close()
  print("ROM DUMPED")
end)
