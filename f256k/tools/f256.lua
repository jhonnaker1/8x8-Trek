-- Shared helpers for reading a running F256K program from MAME's Lua.
--
-- THE ADDRESS SPACE MOVES UNDER THE READ, and that is the reason this file
-- exists. FoenixMCP's IRQ runs sixty times a second and remaps the MMU to
-- reach its own code; a debugger read that lands inside it sees a different
-- memory map, and every one of the program's variables reads as $00. Watched
-- once a second, a probe's counters flickered between correct values and
-- zeros -- and a single read taken at the wrong instant is a zero that looks
-- exactly like a program that never ran.
--
-- SO EVERY READ IS FENCED BY A SIGNATURE. The program carries four known
-- bytes in .data; a read is trusted only when the signature matches BEFORE
-- and AFTER it, which is the same guard a nine-bit raster read needs and for
-- the same reason -- the thing being read does not hold still.
--
-- It also settles launch detection, which two weaker versions got wrong: the
-- marker byte alone was garbage before the PGZ loaded (one run read $04 and
-- called it a launch), and restricting it to $11/$5A was better but still a
-- single byte that leftovers from a PREVIOUS probe can imitate -- which is
-- exactly what happened next. Four bytes in a fixed order cannot.
local M = {}

M.SIG = { 0xE6, 0xA7, 0x5C, 0x13 }

function M.attach(sig_addr)
    M.sp = manager.machine.devices[":maincpu"].spaces["program"]
    M.sig_addr = sig_addr
    return M
end

function M.sig_ok()
    for i = 1, #M.SIG do
        if M.sp:read_u8(M.sig_addr + i - 1) ~= M.SIG[i] then return false end
    end
    return true
end

-- Read `n` bytes from `addr`, retrying until the signature brackets the read.
-- Returns nil if the program never presents a consistent view.
function M.read(addr, n, tries)
    for _ = 1, tries or 40 do
        if M.sig_ok() then
            local out = {}
            for i = 0, (n or 1) - 1 do out[i + 1] = M.sp:read_u8(addr + i) end
            if M.sig_ok() then return out end
        end
        emu.wait(0.002)
    end
    return nil
end

function M.u8(addr)  local v = M.read(addr, 1); return v and v[1] end
function M.u16(addr) local v = M.read(addr, 2); return v and (v[1] + v[2] * 256) end

-- Boot, launch by name, and wait for the SIGNATURE -- not for a marker byte.
function M.launch(name, boot_wait)
    emu.wait(boot_wait or 6)
    local tries = 0
    repeat
        tries = tries + 1
        manager.machine.natkeyboard:post("/- " .. name .. "\n")
        for _ = 1, 6 do emu.wait(1) if M.sig_ok() then break end end
    until M.sig_ok() or tries >= 3
    return M.sig_ok(), tries
end

return M
