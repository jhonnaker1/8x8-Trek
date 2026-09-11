#!/usr/bin/env python3
"""How deep does the soft stack actually go, and where does it start?

_start sets the soft stack to MEMTOP+1 ($02E5/$02E6) and it grows DOWN. This
port clears SDMCTL, so MEMTOP is high -- and the overlay window runs to $C000,
so the stack descends INTO it. That is the restore wedge: read_field writes a
filename onto a stack slot that is also live overlay code.

MEASURED BY SAMPLING $80/$81 (__rc0/__rc1), not by a sentinel fill. The C128
used a sentinel because its stack region was otherwise idle; here the stack
shares its address range with the overlay window, so any pattern laid down is
erased by the next ovl_load and the low-water mark would be a reading about
ovl_load rather than about the stack. Sampling per frame is coarser -- it can
miss a transient deeper than any sample -- and that limitation is stated rather
than hidden. It is a LOWER BOUND on the depth.

    make run-probe-stack
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from session import Session                    # noqa: E402


def main():
    with Session(scratch="stack.atr", shots="shots-stack", deadline=25.0) as s:
        memtop = s.a.peek16(0x02E5)
        sp0 = s.a.peek16(0x0080)
        print("probe: MEMTOP $%04X, soft stack starts $%04X" % (memtop, sp0))
        print("probe: overlay window $AE00..$C000 -- the stack starts INSIDE it"
              if sp0 > 0xAE00 else "probe: stack starts below the window")

        lo = sp0
        def sample(tag, n=1):
            nonlocal lo
            for _ in range(n):
                s.a.frame(1)
                v = s.a.peek16(0x0080)
                if v and v < lo:
                    lo = v
            print("  %-26s SP $%04X   deepest so far $%04X  (%d bytes)"
                  % (tag, s.a.peek16(0x0080), lo, sp0 - lo), flush=True)

        sample("title", 30)
        s.keys("RETURN");        sample("past title", 20)
        s.keys("N,RETURN");      sample("briefing declined", 20)
        s.keys("N,RETURN");      sample("new game", 20)
        s.keys("J,A,M,I,E,RETURN"); sample("captain named", 20)
        s.keys("1,RETURN");      sample("level", 20)
        s.keys("T,R,E,K,RETURN"); sample("password -> console", 40)
        s.keys("S,A,V,E,RETURN", 240); sample("SAVE prompt", 40)
        s.keys("RETURN", 600);   sample("saved", 60)
        s.keys("C", 120);        sample("chart", 30)
        s.keys("M,6,COMMA,2,COMMA,3,COMMA,5,RETURN", 300); sample("warped", 40)

        print()
        print("probe: stack top $%04X, deepest $%04X" % (sp0, lo))
        print("probe: DEPTH USED >= %d bytes (a lower bound -- see the header)"
              % (sp0 - lo))
        print("probe: resident ends about $AB4B; window base $AE00")
        print("probe: a stack that deep needs $%04X..$%04X to itself"
              % (lo, sp0))


main()
