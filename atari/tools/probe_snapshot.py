#!/usr/bin/env python3
"""Does rewinding a snapshot also rewind the emulated DISK?

THE HARNESS CANNOT BE TRUSTED UNTIL THIS IS ANSWERED. Altirra's default disk
mode is VIRTUAL read-write -- the emulated drive accepts writes the host .ATR
never sees, which is what once made this port's SAVE look broken when it was
not. So when tools/session.py rewinds to a snapshot, one of two things is true
and they mean opposite things for every save test built on it:

  * the disk state is INSIDE the snapshot -- a rewind un-writes the save file,
    and a restore after a rewind must fail, proving nothing about SAVE; or
  * the disk lives beside the snapshot -- a rewind keeps the file, and a
    restore after a rewind is a real test.

Measured here, not assumed, and by a discriminator rather than a confirmation:
the same restore attempt is made twice, once WITHOUT a rewind and once WITH,
and the two answers together say which world we are in.

    make run-probe-snapshot
"""
import sys
sys.path.insert(0, __file__.rsplit("/", 1)[0])
from session import Session                   # noqa: E402


def setup_new_game(s):
    """Title, no briefing, new game, a captain, level 1, a password."""
    s.keys("RETURN")
    s.keys("N,RETURN")
    s.keys("N,RETURN")
    s.keys("J,A,M,I,E,RETURN")
    s.keys("1,RETURN")
    s.keys("T,R,E,K,RETURN")


def to_restore_prompt(s, tag):
    """Title, no briefing, YES to restore, then accept the default name.

    SHOT AT EVERY STEP, WITH SHORT SETTLES. The first version pressed the
    last RETURN and waited 900 frames, and the simulator stopped advancing
    inside that wait -- which is indistinguishable from a slow disk read from
    outside. "The rig is ignoring me" has meant five different things on this
    project and the game has said which IN WORDS every time, so the screen is
    read before anything is concluded from a stalled frame gate.
    """
    s.keys("RETURN");            s.shot("%s-a-title.png" % tag)
    s.keys("N,RETURN");          s.shot("%s-b-nobrief.png" % tag)
    s.keys("Y,RETURN", 300);     s.shot("%s-c-restore-prompt.png" % tag)
    s.a.key("RETURN")
    # SMALL STEPS from here: the wedge lives in this wait, so walk into it.
    for i, n in enumerate((30, 120, 300, 600), 1):
        s.a.frame(n)
        s.shot("%s-d%d-after-%d.png" % (tag, i, n))
        print("       %s +%-4d frames  PC %s" % (tag, n, s.a.regs()["PC"]),
              flush=True)


def main():
    with Session(scratch="probe.atr", shots="shots-probe") as s:
        print("probe: booted, %d bytes in far memory" % s.loaded, flush=True)

        setup_new_game(s)
        s.keys("M,6,COMMA,2,COMMA,3,COMMA,5,RETURN", 300)
        s.shot("0-before-save.png")

        s.keys("S,A,V,E,RETURN", 240)
        s.keys("RETURN", 600)
        s.shot("1-saved.png")
        print("probe: SAVE  SIO status $%02X  open_live %d"
              % (s.byte("sio_dbg_status"),
                 s.byte("open_live")), flush=True)
        s.snap("saved")

        # ARM A: restore with NO rewind -- a warm start from the saved state.
        # If this fails, SAVE itself is broken and arm B says nothing.
        s.a.cold_reset()
        s.boot(slot="rebooted")
        to_restore_prompt(s, "2-armA")
        a_shot = s.shot("2-armA-final.png")
        print("probe: ARM A (no rewind)  PC %s  -> %s"
              % (s.a.regs()["PC"], a_shot.name), flush=True)

        # ARM B: rewind to the pre-save snapshot, then try the same restore.
        # Same keys, same game, the only difference is the state_load.
        s.rewind("booted")
        to_restore_prompt(s, "3-armB")
        b_shot = s.shot("3-armB-final.png")
        print("probe: ARM B (rewound)    PC %s  -> %s"
              % (s.a.regs()["PC"], b_shot.name), flush=True)

        print()
        print("probe: compare the two screenshots. Both restored -> the disk")
        print("       survives a rewind and the harness is sound for save")
        print("       tests. A only -> a rewind un-writes the disk, and every")
        print("       save test must re-boot rather than rewind. Neither ->")
        print("       SAVE is the problem, not the harness.")


main()
