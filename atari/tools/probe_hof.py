#!/usr/bin/env python3
"""The hall of fame, and the TREK.SCR write nobody has ever witnessed.

Two separate things, and only one of them is free.

THE SCREEN is one RETURN past the evaluation: main.c runs load_eval() ->
ui_evaluation() -> trek_score() -> load_hof() -> ui_hall_of_fame(). Driving a
self-destruct reaches it.

THE WRITE does not happen at -930. hof_offer() takes a score only if it beats
the slot already there, a fresh table is all zeros, so a qualifying score must
be POSITIVE -- and a self-destruct can never be, because it always books -200
for the ship and the whole crew as casualties. That is why this has gone
unwitnessed on the MEGA65 too (open list item 9), and no amount of driving the
game politely will produce it in one session: beating the -300 incomplete-
mission penalty means killing every Mongol in the galaxy.

SO THE SCORE IS POKED, AND THIS IS A TEST OF THE WRITE, NOT OF SCORING.
`ship.killed` is set so the sheet totals positive; everything downstream --
hof_offer, the serialiser, plat_write_all, the file on the disk -- then runs
for real. Stated plainly because a poked input makes every number on the
evaluation screen meaningless except the ones being tested.

The offsets are CALIBRATED, not trusted: the probe reads `ship` and checks
energy/shields/stardate against what the console shows before poking anything.

    make run-probe-hof
"""
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from session import Session, ATARI             # noqa: E402

OFF_KILLED, OFF_CASUALTIES, OFF_ENERGY, OFF_SHIELDS, OFF_STARDATE = 37, 41, 4, 8, 16


def w(b, i):
    return b[i] | (b[i + 1] << 8)


def main():
    with Session(scratch="hof.atr", shots="shots-hof", deadline=25.0) as s:
        s.keys("RETURN"); s.keys("N,RETURN"); s.keys("N,RETURN")
        s.keys("J,A,M,I,E,RETURN"); s.keys("1,RETURN"); s.keys("T,R,E,K,RETURN")
        base = s.sym["ship"]
        sh = s.a.peek(base, 61)

        # CALIBRATE BEFORE POKING. If these three do not match the console, the
        # offsets are wrong and every poke below would land somewhere else.
        ok = (w(sh, OFF_ENERGY) == 5000 and w(sh, OFF_SHIELDS) == 2500
              and w(sh, OFF_STARDATE) == 35000)
        print("hof: ship energy %d shields %d stardate %d -> offsets %s"
              % (w(sh, OFF_ENERGY), w(sh, OFF_SHIELDS), w(sh, OFF_STARDATE),
                 "CONFIRMED" if ok else "WRONG"), flush=True)
        if not ok:
            sys.exit("hof: refusing to poke against unconfirmed offsets")

        # ---- arm 1: the screen, honestly scored ------------------------
        s.snap("console")
        s.keys("S,RETURN", 300); s.keys("T,R,E,K,RETURN", 400)
        s.shot("1-evaluation.png")
        s.keys("RETURN", 400)
        s.shot("2-hall-of-fame.png")
        print("hof: screen -> 2-hall-of-fame.png (score -930, no write "
              "expected)", flush=True)

        # ---- arm 2: a qualifying score, to exercise the WRITE -----------
        s.rewind("console")
        # 150 KILLS, NOT 60. The first run poked 60 and ALSO poked casualties
        # to zero -- and the score moved from -930 to exactly -330, a
        # difference of 600, which is 60 x SCORE_PER_MONGOL and nothing else.
        # So the kills landed and the casualties did NOT: a self-destruct kills
        # the crew AFTER this poke, so casualties are written over. The floor
        # is therefore -930, and clearing it needs 94+ kills.
        s.a.poke(base + OFF_KILLED, 150)
        s.a.poke(base + OFF_KILLED + 1, 0)
        print("hof: poked ship.killed=150 -- +1500 against a -930 floor",
              flush=True)
        s.keys("S,RETURN", 300); s.keys("T,R,E,K,RETURN", 400)
        s.shot("3-loss-memo.png")
        # memo -> evaluation -> hall of fame. THREE screens, not two.
        s.keys("RETURN", 400); s.shot("4-evaluation-scored.png")
        s.keys("RETURN", 500); s.shot("5-hall-of-fame-scored.png")
        print("hof: write status $%02X, close $%02X"
              % (s.byte("plat_dbg_status"), s.byte("plat_dbg_close")), flush=True)

        # THE ONLY HONEST READ-BACK IS IN THIS SESSION. Altirra's disk writes
        # are VIRTUAL by default -- the emulated drive takes them and the host
        # .ATR never sees them -- which is exactly what made SAVE look broken
        # in the first place. So: play again, reach the hall of fame a second
        # time, and see whether the first run's entry is still in it. The game
        # reads TREK.SCR on the way in, so a name that survives is a file that
        # was written.
        s.keys("RETURN", 500)            # past the hall of fame
        s.keys("N,RETURN", 600)          # play again? no -> back round
        s.rewind("console")
        s.keys("S,RETURN", 300); s.keys("T,R,E,K,RETURN", 400)
        s.keys("RETURN", 400); s.keys("RETURN", 500)
        s.shot("6-hall-of-fame-reread.png")
        print("hof: second visit -> 6-hall-of-fame-reread.png  (JAMIE present "
              "there means TREK.SCR was really written)", flush=True)

    # THE HOST .ATR IS NOT EVIDENCE and this line exists to say so. Altirra
    # writes virtually by default, so the file can be perfectly written inside
    # the emulated drive and absent here. Printed only to show the two
    # disagreeing, which is the point.
    out = subprocess.run([sys.executable, str(ATARI / "tools" / "atr.py"),
                          "list", str(ATARI / "build" / "hof.atr")],
                         capture_output=True, text=True).stdout
    hit = [l for l in out.splitlines() if "TREK" in l.upper()]
    print("\nhof: host .ATR says: %s" % (hit or "NOT PRESENT"))
    print("hof: which proves NOTHING -- Altirra's writes are virtual. The "
          "in-session re-read above is the answer.")


main()
