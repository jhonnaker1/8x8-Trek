"""The controlled enemy-fire run: one enemy, its hit points pinned, a nil
volley for a turn, damage read from the pinned pools and corrected for
death pods by the fall in the enemy's own hit points.

See MEASURED.md, "The controlled fire run", for the results and for the
four instrument faults this shape of code exists to avoid.

The addresses below are from one run and WILL be wrong in the next one.
Locate them with probe_original.py first -- anchor the ship record on the
9999/9999 pair, the enemy table at energy+2206, and the systems array by
finding the run of twelve percentages.
"""
import probe, math, sys, time
E, SH, SYS, TAB, SEC = 181956, 181968, 183498, 184162, 182104
HP = 626

def sec():
    return tuple(int.from_bytes(probe.read(SEC+2*i,2),'little') for i in range(2))

def pin(shields):
    probe.write(TAB+4, HP.to_bytes(2,'little'))
    probe.write(E, probe.tp_real(5000.0)*2)
    probe.write(SYS, (100).to_bytes(2,'little')*12)
    if shields:
        probe.write(SH, probe.tp_real(2500.0)*2)

def turn(shields):
    """One null turn: fire nothing, let the enemy answer. Damage is read as
    the fall in the pools, not off the damage report, so it is exact."""
    live = probe.enemies(TAB)
    if len(live) != 1:
        return None, live
    pin(True)          # always pin the pools; `shields` selects the arm only
    us = sec()
    probe.line('l', pause=1.9)
    probe.line('0', pause=2.7)
    probe.enter(1.5)  # the "Hit enter to continue" after a nil volley
    post = probe.enemies(TAB)
    if len(post) != 1:
        return None, post
    ey, ex, hp_post = post[0]
    # The pools settle AFTER the volley animation. Reading too early gives a
    # zero, and then the late damage lands on the next turn's reading and
    # doubles it -- which is exactly the 0 / 2x pattern the first runs showed.
    # Wait for two identical reads instead of guessing a delay.
    prev = None
    for _ in range(15):
        now = (probe.tp_decode(probe.read(SH,6)), probe.tp_decode(probe.read(E,6)))
        if prev is not None and now == prev:
            break
        prev = now
        time.sleep(0.4)
    total = (2500.0 - prev[0]) + (5000.0 - prev[1])
    # A Vandal Death Pod hits every ship in the quadrant for one figure, ours
    # included, so the fall in the ENEMY's hit points measures the pod exactly
    # -- its hit points were pinned to HP at the top of this turn. Without
    # this correction the pod is indistinguishable from enemy fire, and
    # against a weak enemy it is most of what the pools record.
    pod = max(0.0, HP - hp_post)
    return (us, (ey,ex), math.hypot(us[0]-ey, us[1]-ex), total - pod, pod), post

def block(label, n, shields):
    rows = []
    for _ in range(n):
        r, live = turn(shields)
        if r is None:
            print('  ABORT (%d live: %s)' % (len(live), live)); break
        us, en, d, dmg, pod = r
        rows.append((d, dmg))
        print('  %-7s %-7s d=%.3f  fire=%7.1f  pod=%5.0f  fire/hp=%.3f'
              % (us, en, d, dmg, pod, dmg/HP))
    if rows:
        ds = rows[0][0]
        ks = [dmg/(HP*(1-ds/12.0)) for _, dmg in rows]
        print('  %s: n=%d  d=%.3f  mean dmg=%.1f  k(L=12)=%.3f' %
              (label, len(ks), ds, sum(x[1] for x in rows)/len(rows), sum(ks)/len(ks)))
    return rows
