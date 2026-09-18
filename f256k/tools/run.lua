emu.wait(6)
manager.machine.natkeyboard:post("/- hello\n")
emu.wait(10)
local sp = manager.machine.devices[":maincpu"].spaces["program"]
print(string.format("MARKER $2158 = $%02X  (11=started 5A=finished)", sp:read_u8(0x2158)))
manager.machine.video:snapshot()
manager.machine:exit()
