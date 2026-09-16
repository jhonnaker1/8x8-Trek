-- Boot a disk we wrote and look at what happens. No image poking, no PC
-- setting: the machine does the whole thing itself, which is the point.
local WAIT = tonumber(os.getenv("GS_WAIT") or "12")
emu.wait(WAIT)
local cpu = manager.machine.devices[":maincpu"]
local sp  = cpu.spaces["program"]
print(string.format("PB=%02X PC=%04X E=%d C029=%02X FLAG0300=%02X",
      cpu.state["PB"].value, cpu.state["PC"].value, cpu.state["E"].value,
      sp:read_u8(0x00C029), sp:read_u8(0x000300)))
local o = {}
for i = 0, 15 do o[#o+1] = string.format("%02X", sp:read_u8(0x0800 + i)) end
print("AT0800 " .. table.concat(o, " "))
if os.getenv("GS_SHOT") then manager.machine.video:snapshot() end
manager.machine:exit()
