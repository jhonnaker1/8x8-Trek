-- The special keys, tapped through their ioport fields and read back.
-- The F256K's keyboard is a C64 layout: there is no ESC key, there is
-- RUN/STOP, and MAME maps the host's escape onto it.
local F = dofile("tools/f256.lua").attach(tonumber(os.getenv("A_SIG"), 16))
local function sym(n) return tonumber(os.getenv(n), 16) end
local N = sym("A_N")
local T, R, A, FL = sym("A_TYPE"), sym("A_RAW"), sym("A_ASCII"), sym("A_FLAGS")

local ok = F.launch("keyprobe")
if not ok then print("launch failed") manager.machine:exit() return end

local taps = {
    { "ROW0", "DEL",      "DEL"      },
    { "ROW7", "BKSP",     "BKSP"     },
    { "ROW7", "RUN/STOP", "RUN/STOP" },
    { "ROW0", "↑",        "UP"       },
    { "ROW0", "↓",        "DOWN"     },
    { "ROW0", "ENTER",    "ENTER"    },
}
local marks = {}
for _, t in ipairs(taps) do
    local port = manager.machine.ioport.ports[":" .. t[1]]
    local f = port and port.fields[t[2]]
    marks[#marks + 1] = { at = F.u8(N), label = t[3] }
    if f then
        f:set_value(0); emu.wait(0.15); f:set_value(1); emu.wait(0.25)
    else
        print("   -- no field " .. t[1] .. "/" .. t[2])
    end
end
emu.wait(1)

local n = F.u8(N)
local ty, raw, asc, fl = F.read(T, n), F.read(R, n), F.read(A, n), F.read(FL, n)
local function label_at(i)
    local l = ""
    for _, m in ipairs(marks) do if m.at == i then l = l .. "  <== " .. m.label end end
    return l
end
print("   #  type  raw  ascii  char  flags")
for i = 1, n do
    local a = asc[i]
    local ch = (a >= 32 and a < 127) and string.char(a) or "."
    print(string.format("  %2d   %3d  $%02X   $%02X    %s     $%02X%s%s",
          i - 1, ty[i], raw[i], a, ch, fl[i],
          (fl[i] >= 0x80) and "  NO ASCII" or "", label_at(i - 1)))
end
manager.machine:exit()
