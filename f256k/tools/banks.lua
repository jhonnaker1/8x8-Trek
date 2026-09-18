local F = dofile("tools/f256.lua").attach(tonumber(os.getenv("A_SIG"), 16))
local function sym(n) return tonumber(os.getenv(n), 16) end
local RAN, MLUT, FL, FH, Z, W, L5, CTL =
  sym("A_RAN"), sym("A_MLUT"), sym("A_FPLO"), sym("A_FPHI"), sym("A_ZERO"),
  sym("A_WROTE"), sym("A_L5"), sym("A_CTL")

if not F.launch("bankprobe") then print("LAUNCH FAILED") manager.machine:exit() return end
for _ = 1, 60 do if F.u8(RAN) == 0x5A then break end emu.wait(0.1) end

local ctl = F.u8(CTL)
print(string.format("MMU_MEM_CTRL = $%02X  ->  ACTIVE MLUT %d  (this is the map the program runs in)",
      ctl, ctl % 4))
local m = F.read(MLUT, 32)
print("MLUTs as FoenixMCP left them (slot: $0000 $2000 $4000 $6000 $8000 $A000 $C000 $E000):")
for l = 0, 3 do
    local row = {}
    for s = 0, 7 do row[#row+1] = string.format("$%02X", m[l*8+s+1]) end
    print(string.format("  MLUT %d:  %s%s", l, table.concat(row, "  "),
          (l == ctl % 4) and "   <== ACTIVE" or ""))
end
print(string.format("slot 5 ($A000-$BFFF) writable as left by MCP: %s",
      F.u8(L5) == 1 and "YES" or "NO"))

local lo, hi, z, w = F.read(FL, 64), F.read(FH, 64), F.read(Z, 64), F.read(W, 64)
local free, used, alias, untested = 0, 0, 0, 0
print("bank  first-256 sum  content   write test")
for b = 0, 63 do
    local sum = lo[b+1] + hi[b+1]*256
    local content = (z[b+1] == 1) and "zero" or "IN USE"
    local wt
    if w[b+1] == 2 then wt = "(not attempted)" untested = untested + 1
    elseif w[b+1] == 1 then wt = "held its own signature" free = free + 1
    else wt = "**ALIASED OR DEAD**" alias = alias + 1 end
    if z[b+1] ~= 1 then used = used + 1 end
    print(string.format("  $%02X   $%04X          %-7s  %s", b, sum, content, wt))
end
print(string.format("\n%d banks free and distinct (%d KB), %d in use, %d aliased/dead",
      free, free * 8, used, alias))
manager.machine:exit()
