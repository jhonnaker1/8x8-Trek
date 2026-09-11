#!/usr/bin/env python3
"""Where does the 6502 stop when RESTORE is answered?

Pressing RETURN at the restore filename prompt stops the SIMULATOR, not just
the game: AltirraBridge's frame gate never releases, and a gate is released by
frames elapsing regardless of what the CPU is doing. A game spinning in a retry
loop would still let frames pass. A stopped simulator means Altirra's debugger
broke -- which on an illegal instruction is exactly what it does.

So this walks in ONE FRAME AT A TIME with the PC printed each step, and when
the gate finally blocks it CLOSES the dead socket and reconnects, because the
bridge refuses a second client only while the first is still attached. The
debugger answers while the machine is stopped, which is the whole point of it.

    probe_wedge.py            fresh disk, NO save file present
    probe_wedge.py --save     make a save first, then restore it

THE NO-SAVE RUN COMES FIRST AND IS THE CHEAPER HALF OF THE EXPERIMENT. If the
machine stops with no file to find, the fault is in the not-found path and the
whole save/restore question is downstream of it. If it only stops with a real
file, the fault is in reading one. One boot decides which, where the save
version needs two.
"""
import sys
import time
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from session import Session, symbols                   # noqa: E402
from altirra_bridge import AltirraBridge               # noqa: E402
from altirra_bridge.client import BridgeError          # noqa: E402


def nearest(sym, pc):
    """The symbol the PC sits in, so a bare address means something."""
    best, name = -1, "?"
    for n, a in sym.items():
        if a <= pc and a > best:
            best, name = a, n
    return "%s+%d" % (name, pc - best) if best >= 0 else "?"


def as_addr(v):
    """Altirra prints addresses as `$8f87`; int() does not take the dollar."""
    if isinstance(v, int):
        return v
    return int(str(v).lstrip("$").replace("0x", ""), 16)


def pc_of(a):
    return as_addr(a.regs()["PC"])


def main():
    want_save = "--save" in sys.argv
    with Session(scratch="wedge.atr", shots="shots-wedge", deadline=25.0) as s:
        sym = s.sym
        print("wedge: booted, far_used %d" % s.loaded, flush=True)

        if want_save:
            s.keys("RETURN"); s.keys("N,RETURN"); s.keys("N,RETURN")
            s.keys("J,A,M,I,E,RETURN"); s.keys("1,RETURN"); s.keys("T,R,E,K,RETURN")
            s.keys("S,A,V,E,RETURN", 240)
            s.keys("RETURN", 600)
            print("wedge: SAVE transfer $%02X close $%02X"
                  % (s.byte("plat_dbg_status"), s.byte("plat_dbg_close")),
                  flush=True)
            s.boot(slot="rebooted")
            print("wedge: rebooted, far_used %d" % s.loaded, flush=True)

        s.keys("RETURN")
        s.keys("N,RETURN")
        s.keys("Y,RETURN", 300)
        s.shot("0-prompt.png")
        print("wedge: at the filename prompt, PC $%04X  %s"
              % (pc_of(s.a), nearest(sym, pc_of(s.a))), flush=True)

        s.a.key("RETURN")
        stopped_at = None
        for i in range(1, 61):
            try:
                s.a.frame(1)
                pc = pc_of(s.a)
            except BridgeError as e:
                print("wedge: frame %d -- THE GATE BLOCKED (%s)" % (i, e),
                      flush=True)
                stopped_at = i
                break
            print("  frame %2d  PC $%04X  %s" % (i, pc, nearest(sym, pc)),
                  flush=True)
        else:
            print("wedge: 60 frames and it never stopped -- no wedge here",
                  flush=True)
            s.shot("1-no-wedge.png")
            return

        # THE DEAD SOCKET IS THE ONLY THING HOLDING THE SLOT. Drop it, then ask
        # the debugger what it is stopped on.
        try:
            s.a._sock.close()
        except Exception:
            pass
        time.sleep(2)
        token = None
        for ln in (pathlib.Path(__file__).resolve().parents[1]
                   / "build" / "bridge.log").read_text().splitlines():
            if "token-file:" in ln:
                token = ln.split("token-file:")[1].strip()
        print("wedge: reconnecting to ask why it stopped...", flush=True)
        b = AltirraBridge.from_token_file(token)
        b._sock.settimeout(20)
        r = b.regs()
        pc = as_addr(r["PC"])
        print("wedge: STOPPED AT PC $%04X  %s" % (pc, nearest(sym, pc)))
        print("wedge: regs %s" % r)
        try:
            print("wedge: disasm at PC:\n%s" % b.disasm(pc, 8))
        except Exception as e:
            print("wedge: disasm unavailable: %s" % e)

        # THE STACK NAMES THE CALLER. A JAM tells you where execution ended;
        # the return addresses above S tell you how it got there, which is the
        # only part that identifies the bug. 6502 pushes hi then lo, so a
        # return address reads lo,hi at S+1 and points at the byte AFTER the
        # JSR.
        sp = as_addr(r["S"])
        raw = b.peek(0x0100 + sp + 1, 24)
        print("wedge: stack @ $%04X: %s"
              % (0x0100 + sp + 1, " ".join("%02x" % x for x in raw)))
        for i in range(0, len(raw) - 1, 2):
            ret = raw[i] | (raw[i + 1] << 8)
            if 0x3000 <= ret < 0xC000:
                print("       return -> $%04X  %s (caller near $%04X)"
                      % (ret, nearest(sym, ret - 1), ret - 3))

        # WHAT IS ACTUALLY IN THE WINDOW. If it is a valid overlay image this
        # is a rule-4 call into the wrong one; if it matches nothing, something
        # has written over the window itself.
        win = b.peek(0xAE00, 64)
        print("wedge: window head: %s" % " ".join("%02x" % x for x in win[:16]))
        import pathlib as _p
        for f in sorted(_p.Path("build").glob("ovl_*.raw")):
            img = f.read_bytes()
            if img[:64] == bytes(win):
                print("wedge: the window holds %s" % f.stem)
                break
        else:
            print("wedge: the window matches NO overlay image")
        b._sock.close()


main()
