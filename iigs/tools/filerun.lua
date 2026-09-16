emu.wait(tonumber(os.getenv("GS_WAIT") or "12"))
local cpu = manager.machine.devices[":maincpu"]; local sp = cpu.spaces["program"]
local done = sp:read_u8(tonumber(os.getenv("GS_DONE")))
print(string.format("PC=%04X DONE=%02X", cpu.state["PC"].value, done))
if done ~= 0x5A then print("NEVER COMPLETED"); manager.machine:exit(); return end
local b = tonumber(os.getenv("GS_REP"))
local o={} for i=0,47 do o[#o+1]=string.format("%02X",sp:read_u8(b+i)) end
print("REP " .. table.concat(o," "))
local s="" for i=8,23 do local c=sp:read_u8(b+i); s=s..((c>=32 and c<127) and string.char(c) or ".") end
print("TEXT1 ["..s.."]")
s="" for i=32,39 do local c=sp:read_u8(b+i); s=s..((c>=32 and c<127) and string.char(c) or ".") end
print("TEXT2 ["..s.."]")
manager.machine:exit()
