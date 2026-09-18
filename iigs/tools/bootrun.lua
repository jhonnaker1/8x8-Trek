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
-- GS_DUMP=addr:count[:step], same syntax as run.lua. Added 2026-09-17: this
-- script could boot the disk and print the first sixteen bytes at $0800 and
-- nothing else, so anything the RUNNING GAME computed -- as against a
-- standalone probe binary -- could not be read at all. Region detection is the
-- first thing that needed it.
local DUMP = os.getenv("GS_DUMP")
if DUMP then
  for spec in string.gmatch(DUMP, "[^;]+") do
    local a, n, st = string.match(spec, "([^:]+):([^:]+):?(.*)")
    a, n = tonumber(a), tonumber(n)
    st = (st ~= "" and tonumber(st)) or 1
    local d = {}
    for i = 0, n - 1 do d[#d+1] = string.format("%02X", sp:read_u8(a + i * st)) end
    print(string.format("DUMP %06X+%d*%d %s", a, n, st, table.concat(d, " ")))
  end
end

if os.getenv("GS_SHOT") then manager.machine.video:snapshot() end
manager.machine:exit()
