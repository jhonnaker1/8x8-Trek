-- The rig, rebuilt: emu.wait() instead of a frame notifier, and NO debugger
-- window -- which is what made MAME freeze until the mouse moved.
local function loadbin(path, sp)
  local f = assert(io.open(path,"rb")); local d = f:read("*a"); f:close()
  local i, exec = 1, nil
  while i + 4 <= #d do
    local t = d:byte(i)
    local ln = d:byte(i+1)*256 + d:byte(i+2)
    local ad = d:byte(i+3)*256 + d:byte(i+4)
    if t == 0xFF then exec = ad break end
    for k = 1, ln do sp:write_u8(ad + k - 1, d:byte(i+4+k)) end
    i = i + 5 + ln
  end
  return exec
end
emu.wait(3.0)
local sp = manager.machine.devices[":maincpu"].spaces["program"]
local exec = loadbin("banktest.bin", sp)
manager.machine.devices[":maincpu"].state["PC"].value = exec
emu.wait(2.0)
local o = {}
for i = 0, 6 do o[#o+1] = string.format("%02X", sp:read_u8(0x2F00 + i)) end
print("BANKTEST " .. table.concat(o, " "))
