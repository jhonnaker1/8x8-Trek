-- The IIgs rig: load a flat bank-0 image, set the PC, let it run, look.
--
-- NO DISK AND NO PRODOS, ON PURPOSE. This decouples the two chains the Plus/4
-- port entangled -- "does our compiled code run and draw" is answered here,
-- and "does the machine load it from a disk" is a separate question with its
-- own failures. A probe that needs both cannot say which one broke.
--
-- Environment: GS_BIN (image), GS_ORG (default $2000), GS_WAIT (seconds to
-- run), GS_SHOT (take a snapshot), GS_DUMP=addr:count[:step] (repeatable,
-- semicolon separated), GS_DONE=addr (completion byte -- see below).

local BIN  = os.getenv("GS_BIN")  or "build/gsprobe.bin"
local ORG  = tonumber(os.getenv("GS_ORG") or "0x2000")
local WAIT = tonumber(os.getenv("GS_WAIT") or "6")

emu.wait(3.0)                       -- let the ROM finish booting

local cpu = manager.machine.devices[":maincpu"]
local sp  = cpu.spaces["program"]

local f = assert(io.open(BIN, "rb"))
local d = f:read("*a"); f:close()
for i = 1, #d do sp:write_u8(ORG + i - 1, d:byte(i)) end

-- READ IT BACK BEFORE JUMPING. A write into a bank that is not mapped the way
-- you think returns silence, not an error, and the symptom is identical to
-- code that ran and did nothing.
local ok = true
for i = 1, #d do
  if sp:read_u8(ORG + i - 1) ~= d:byte(i) then ok = false break end
end
print(string.format("LOADED %d bytes at %06X readback=%s", #d, ORG,
                    ok and "OK" or "MISMATCH"))

cpu.state["PB"].value = 0
cpu.state["PC"].value = ORG
emu.wait(WAIT)

print(string.format("AFTER PB=%02X PC=%04X E=%d P=%02X DB=%02X C029=%02X",
      cpu.state["PB"].value, cpu.state["PC"].value, cpu.state["E"].value,
      cpu.state["P"].value, cpu.state["DB"].value, sp:read_u8(0x00C029)))

-- THE COMPLETION BYTE, CHECKED BEFORE ANY REPORT IS READ. A probe's output
-- array read out of a run that never finished is uninitialised memory
-- presented as a finding; that is exactly how the Plus/4 zero-page probe came
-- to say "32 of 32 clobbered" with the thing under test removed.
local DONE = os.getenv("GS_DONE")
local complete = true
if DONE then
  local v = sp:read_u8(tonumber(DONE))
  complete = (v == 0x5A)
  print(string.format("DONE=%02X %s", v,
        complete and "-- probe completed" or "-- NEVER COMPLETED"))
end

-- READING SHR MEMORY BACK IS ONLY MEANINGFUL WITH SHR OFF. Measured
-- 2026-09-16: the same program that reads back as the identity (0 mismatches
-- of 125 pages) with SHR disabled reads back with every page DOUBLED (124
-- mismatches of 125) once $C029 bit 7 is set. Same writes, one line
-- different. So a dump of $E1/2000.. taken while the display is on is not a
-- picture of what the program wrote -- use the SNAPSHOT for that, and dump
-- only from a build that leaves SHR off.
if complete then
  local DUMP = os.getenv("GS_DUMP")
  if DUMP then
    for spec in string.gmatch(DUMP, "[^;]+") do
      local a, n, st = string.match(spec, "([^:]+):([^:]+):?(.*)")
      a, n = tonumber(a), tonumber(n)
      st = (st ~= "" and tonumber(st)) or 1
      local o = {}
      for i = 0, n - 1 do o[#o+1] = string.format("%02X", sp:read_u8(a + i * st)) end
      print(string.format("DUMP %06X+%d*%d %s", a, n, st, table.concat(o, " ")))
    end
  end
end

if os.getenv("GS_SHOT") then manager.machine.video:snapshot() end
manager.machine:exit()
