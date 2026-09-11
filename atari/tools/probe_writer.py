#!/usr/bin/env python3
"""WHO writes EGATREK.SAV into the overlay window?

The window at $AE00..$BF00 loads `ovl_front` intact -- its first 64 bytes match
the image byte for byte -- and then something writes the save filename over the
middle of it. The executing code is corrupted underneath itself, runs into the
string, and hits $52 (kil). That is the restore wedge.

Everything up to here was inference from where execution ENDED. This asks the
machine directly: a WRITE watchpoint on the first byte of the string. Altirra
breaks on the store, and the PC is the instruction that did it.

Per-byte, not a range -- Altirra's access-breakpoint API has no range form and
the bridge rejects `len=`. One address is enough: the string is written in
order, so the first byte is hit first.

    make run-probe-writer
"""
import pathlib
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from session import Session                            # noqa: E402
from altirra_bridge import AltirraBridge               # noqa: E402
from altirra_bridge.client import BridgeError          # noqa: E402

TARGET = 0xBBE8          # where "EGATREK.SAV" starts in the corrupted window


def as_addr(v):
    return v if isinstance(v, int) else int(str(v).lstrip("$").replace("0x", ""), 16)


def nearest(sym, pc):
    best, name = -1, "?"
    for n, a in sym.items():
        if a <= pc and a > best:
            best, name = a, n
    return "%s+%d" % (name, pc - best) if best >= 0 else "?"


def main():
    with Session(scratch="writer.atr", shots="shots-writer", deadline=25.0) as s:
        sym = s.sym
        print("writer: booted, far_used %d" % s.loaded, flush=True)

        # A real save first -- the wedge needs a file to read back.
        s.keys("RETURN"); s.keys("N,RETURN"); s.keys("N,RETURN")
        s.keys("J,A,M,I,E,RETURN"); s.keys("1,RETURN"); s.keys("T,R,E,K,RETURN")
        s.keys("S,A,V,E,RETURN", 240)
        s.keys("RETURN", 600)
        print("writer: SAVE transfer $%02X close $%02X"
              % (s.byte("plat_dbg_status"), s.byte("plat_dbg_close")), flush=True)

        s.boot(slot="rebooted")
        s.keys("RETURN"); s.keys("N,RETURN")
        s.keys("Y,RETURN", 300)
        print("writer: at the filename prompt", flush=True)

        ids = s.a.watch_set(TARGET, mode="w")
        print("writer: write watch on $%04X -- ids %s" % (TARGET, ids), flush=True)

        s.a.key("RETURN")
        for i in range(1, 61):
            try:
                s.a.frame(1)
                pc = as_addr(s.a.regs()["PC"])
            except BridgeError:
                print("writer: frame %d -- BROKE (the watch fired, or a JAM)" % i,
                      flush=True)
                break
            print("  frame %2d  PC $%04X  %s" % (i, pc, nearest(sym, pc)), flush=True)
        else:
            print("writer: never broke -- the watch did not fire in 60 frames")
            return

        try:
            s.a._sock.close()
        except Exception:
            pass
        time.sleep(2)
        tok = None
        for ln in (pathlib.Path(__file__).resolve().parents[1]
                   / "build" / "bridge.log").read_text().splitlines():
            if "token-file:" in ln:
                tok = ln.split("token-file:")[1].strip()
        b = AltirraBridge.from_token_file(tok)
        b._sock.settimeout(20)
        r = b.regs()
        pc = as_addr(r["PC"])
        print()
        print("writer: BROKE AT PC $%04X  %s" % (pc, nearest(sym, pc)))
        print("writer: regs %s" % r)
        print("writer: $%04X now holds $%02X" % (TARGET, b.peek(TARGET, 1)[0]))
        # The store is the instruction BEFORE the PC on a break-after write.
        for back in (0, 2, 3):
            try:
                d = b.disasm(pc - back, 3)
                print("writer: disasm from $%04X: %s"
                      % (pc - back, [x["text"].strip() for x in d]))
            except Exception:
                pass
        sp = as_addr(r["S"])
        raw = b.peek(0x0100 + sp + 1, 16)
        print("writer: stack: %s" % " ".join("%02x" % x for x in raw))
        for i in range(len(raw) - 1):
            ret = raw[i] | (raw[i + 1] << 8)
            if 0x3000 <= ret < 0xC000:
                print("        return -> $%04X  %s" % (ret, nearest(sym, ret - 1)))
        b._sock.close()


main()
