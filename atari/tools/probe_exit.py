#!/usr/bin/env python3
"""What happens when the captain answers NO to "Play Again?"

    make run-probe-exit

Jamie, 2026-09-11, first play of this port: "at the end of the game when it
asks if you want to play again and you say no, it does not exit cleanly."

NOBODY HAD EVER LOOKED. tools/probe_hof.py types N at that prompt and rewinds
to a snapshot on the very next line -- its comment says "back round", which is
a guess about a screen the probe never waited to see.

So this one types N and then just WATCHES: the PC, and a screenshot a second
apart, for ten seconds.
"""
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve()
sys.path.insert(0, str(HERE.parent))
from session import Session                  # noqa: E402

ATARI = HERE.parents[1]


def main():
    with Session(shots="shots-exit") as s:
        s.boot()
        print("exit: booted, far_used %d" % s.loaded, flush=True)
        s.keys("RETURN")                     # title
        s.keys("N,RETURN")                   # briefing? no
        s.keys("N,RETURN")                   # restore? no
        s.keys("J,A,M,I,E,RETURN")
        s.keys("1,RETURN")
        s.keys("T,R,E,K,RETURN")             # self-destruct password
        s.shot("0-console.png")

        # Self-destruct: the shortest honest route to the endgame, and the
        # same one the hall-of-fame probe uses.
        s.keys("S,RETURN", 300)
        s.keys("T,R,E,K,RETURN", 400)
        s.shot("1-memo.png")
        s.keys("RETURN", 400)
        s.shot("2-evaluation.png")
        s.keys("RETURN", 500)
        s.shot("3-evaluation-total.png")
        s.keys("RETURN", 500)
        s.shot("4-hall-of-fame.png")
        # FOUR RETURNS AFTER THE PASSWORD, NOT THREE. The endgame is memo,
        # evaluation, total, hall of fame -- and the first run of this probe
        # typed N at the hall of fame's "HIT RETURN TO CONTINUE", where N does
        # nothing, then reported ten seconds of a game that was simply waiting.
        s.keys("RETURN", 500)
        s.shot("5-play-again.png")
        print("exit: at the Play Again prompt, PC %s" % s.a.regs()["PC"],
              flush=True)

        # THE PRECONDITION FOR plat_exit's RESTART, checked after a whole
        # game rather than argued from the linker script: is the boot record
        # still at $0700? Its first six bytes are flags, sector count, load
        # address and init vector -- 00 03 00 07 09 07 -- and nothing in this
        # port should ever have written there.
        hdr = bytes(s.a.peek(0x0700, 6))
        want = bytes(pathlib.Path(ATARI / "build" / "boot.bin").read_bytes()[:6])
        print("exit: boot record at $0700 after a full game: %s (want %s) %s"
              % (hdr.hex(" "), want.hex(" "),
                 "INTACT" if hdr == want else "CLOBBERED"), flush=True)
        if hdr != want:
            print("exit: plat_exit jumps to $0706 -- a clobbered boot record "
                  "would jump into rubble.", flush=True)

        s.keys("N", 60)
        print("exit: typed N -- now watching for ten seconds", flush=True)
        for i in range(10):
            s.a.frame(60)
            r = s.a.regs()
            print("      +%2ds  PC %s  A %s X %s Y %s"
                  % (i + 1, r["PC"], r["A"], r["X"], r["Y"]), flush=True)
            s.shot("6-after-no-%02d.png" % (i + 1))

        # AND THEN THE KEY THE SCREEN ASKS FOR. "HIT A KEY AND THE ATARI
        # RESTARTS" is a promise, and plat_exit closing VBXE's window before
        # the cold start is what makes it keepable -- MEMAC survives a reset,
        # so a reboot with the window still open runs the OS's own init with
        # VRAM mapped over $2000.
        print("exit: hitting a key -- the screen says the Atari restarts",
              flush=True)
        s.keys("RETURN", 60)
        # LONG ENOUGH FOR A WHOLE BOOT LOAD, which is the point that caught
        # the first run of this: 1,440 frames is 24 emulated seconds and a
        # real boot here is thousands. A black screen at 24s is a machine
        # part-way through loading 36,474 bytes, not a machine that failed to
        # restart -- and the two look identical.
        for i in range(6):
            s.a.frame(1000)
            s.shot("7-restart-%02d.png" % (i + 1))
            print("      %5d frames  PC %s  far_used %d"
                  % ((i + 1) * 1000, s.a.regs()["PC"],
                     s.word("far_used")), flush=True)

        # THE DISCRIMINATOR. A black screen with far_used at 0 is either a
        # machine part-way through a reboot or a machine that never started
        # one, and neither the PC nor the picture separates them. So: ask the
        # EMULATOR for a cold reset -- which certainly reboots -- and see
        # whether this machine can still reach a title screen at all. If it
        # can, `jmp $E477` is not doing what plat_exit believes.
        print("exit: forcing a cold reset from the bridge for comparison",
              flush=True)
        s.a.cold_reset()
        for i in range(3):
            s.a.frame(1000)
            s.shot("8-forced-%02d.png" % (i + 1))
            print("      %5d frames  PC %s  far_used %d"
                  % ((i + 1) * 1000, s.a.regs()["PC"],
                     s.word("far_used")), flush=True)
    print("\nexit: read shots-exit/6-after-no-*.png -- a screen that REBOOTS "
          "the game and")
    print("      a screen that hangs look nothing alike, and the PC says "
          "which.")
    return 0


sys.exit(main())
