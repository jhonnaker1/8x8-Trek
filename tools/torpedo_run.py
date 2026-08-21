"""Torpedo damage and accuracy runs against the original.

The trick that unstuck open item 9: write a large hit-point value into the
target's record so it SURVIVES the torpedo, and read the damage out of the
enemy table -- the game never prints it.

Four things here were each paid for with a destroyed ship:

  * restore() runs on BOTH sides of every shot. Pinning only beforehand
    leaves the ship carrying the enemy's answer until the next shot.
  * HP stays small. Damage an enemy DEALS is proportional to its hit
    points, so a target pinned to 20000 fires about 14000 and kills you.
  * on_console() checks the VIDEO, not the state. A quit or a death leaves
    every address reading plausibly, and the stardate only goes backwards
    once a NEW game starts.
  * Follow a table SLOT, not a sector. Enemies move after every volley and
    migrate in from other quadrants mid-fight.

See MEASURED.md, "Open item 9" and "How this went wrong, four times".
Addresses are per-run; locate them with probe_original.py first.
"""
import probe, math, time

SD = 181938          # live stardate; 181926 is the START date and never moves
E, SH, SYS, TAB, SEC, TORP = 181956, 181968, 183498, 184162, 182104, 182062
# High enough that the target survives any torpedo -- the ancestor's ceiling
# is 800 -- and no higher, because damage an enemy DEALS is proportional to
# its remaining hit points (MEASURED 2026-08-21). Pinning a target at 20000
# turned it into a ship firing ~14000 a volley and it destroyed us in one
# shot. The finding that makes this experiment possible is the same one that
# makes a large pin lethal.
HP = 700          # see the note above; also keep it LOW -- the target fires
                  # proportional to this, so a big pin arms the thing shooting
                  # back at you

def sec():
    return tuple(int.from_bytes(probe.read(SEC+2*i,2),'little') for i in range(2))

class GameOver(Exception):
    pass

_watch = {"sd": None, "quad": None}

def quad():
    return tuple(int.from_bytes(probe.read(182096+2*i,2),'little') for i in range(2))

def on_console():
    """True if the game is showing the console.

    State-based checks are not enough: quitting or dying leaves a score screen
    while every address still reads plausibly, and the stardate does not move
    backwards until a NEW game starts. The COMMAND panel's magenta is present
    on every console frame, dialog open or not, and absent everywhere else."""
    import io
    from PIL import Image
    im = Image.open(io.BytesIO(probe.call("/video/frame?format=png", raw=True)))
    im = im.convert("RGB")
    return all(im.getpixel(xy) == (170, 0, 170) for xy in ((20, 205), (100, 205)))


def guard():
    """Abort the moment the program is no longer the game we started.

    Losing the ship restarts EGA Trek, and every address here keeps returning
    plausible numbers from the dead process -- which has now wrecked three
    runs, twice without being noticed until the data was analysed. The
    stardate is the tell: it only ever advances inside a game, so a DECREASE
    means a restart. Cheap, and it fails loudly instead of quietly."""
    if not on_console():
        raise GameOver("not on the console -- the game ended or restarted")
    sd = probe.tp_decode(probe.read(SD, 6))
    q = quad()
    if _watch["sd"] is not None and sd < _watch["sd"] - 0.001:
        raise GameOver("stardate went backwards: %.3f -> %.3f (game restarted)"
                       % (_watch["sd"], sd))
    if _watch["quad"] is not None and q != _watch["quad"]:
        raise GameOver("quadrant changed %s -> %s without us moving"
                       % (_watch["quad"], q))
    _watch["sd"], _watch["quad"] = sd, q

def arm():
    _watch["sd"], _watch["quad"] = None, None
    guard()

def restore():
    """Put the ship back to full: shields, energy and all twelve systems.

    Must be called AFTER the turn resolves as well as before it. Pinning only
    beforehand leaves the ship carrying the enemy's answer -- shields down,
    systems wrecked -- for the whole gap until the next shot, and with several
    enemies in the quadrant that is where it dies. Repairing on both sides
    means no damage ever accumulates across shots."""
    probe.write(E,  probe.tp_real(5000.0)*2)
    probe.write(SH, probe.tp_real(2500.0)*2)
    probe.write(SYS, (100).to_bytes(2,'little')*12)
    probe.write(TORP, (9).to_bytes(2,'little')*2)

def pin():
    probe.write(TORP, (9).to_bytes(2,'little')*2)   # refill the rack
    probe.write(TAB+4, HP.to_bytes(2,'little'))
    probe.write(E,  probe.tp_real(5000.0)*2)
    probe.write(SH, probe.tp_real(2500.0)*2)
    probe.write(SYS, (100).to_bytes(2,'little')*12)

def hp_now():
    b = probe.read(TAB+4, 2)
    return int.from_bytes(b, 'little')

def table(slots=8):
    """Every slot, raw. Enemies migrate INTO a quadrant mid-fight and killed
    ships leave holes, so neither the count nor the packing can be assumed."""
    b = probe.read(TAB, 6*slots)
    return [tuple(int.from_bytes(b[k*6+j*2:k*6+j*2+2],'little') for j in range(3))
            for k in range(slots)]

def find(sec_yx, slots=8):
    """Slot index of the live enemy at a sector, or None."""
    for k, (y, x, hp) in enumerate(table(slots)):
        if (y, x) == sec_yx and hp:
            return k
    return None

def aimed_shot(target_yx):
    """Fire one torpedo at a sector and report what actually changed.

    Diffs the whole table rather than one record: a torpedo that misses its
    mark can still hit another ship, and a shot at a sector whose occupant has
    moved away hits nothing at all -- five of those in a row read as five
    misses before this was written."""
    guard()
    k = find(target_yx)
    if k is None:
        raise GameOver("nothing alive at %s to aim at" % (target_yx,))
    probe.write(TORP, (9).to_bytes(2,'little')*2)
    probe.write(TAB + 6*k + 4, HP.to_bytes(2,'little'))
    probe.write(E,  probe.tp_real(5000.0)*2)
    probe.write(SH, probe.tp_real(2500.0)*2)
    probe.write(SYS, (100).to_bytes(2,'little')*12)
    pre = table()
    probe.line('t', pause=1.4); probe.line('1', pause=1.4)
    probe.line('%d%d' % target_yx, pause=3.4)
    prev = None
    for _ in range(15):
        now = table()
        if prev is not None and now == prev:
            break
        prev = now
        time.sleep(0.4)
    probe.enter(1.2)
    restore()                     # repair immediately, before anything else fires
    guard()
    hits = [(pre[i][:2], pre[i][2] - prev[i][2])
            for i in range(len(pre))
            if pre[i][2] and prev[i][2] < pre[i][2]]
    moved = [(pre[i][:2], prev[i][:2]) for i in range(len(pre))
             if pre[i][2] and prev[i][2] and pre[i][:2] != prev[i][:2]]
    return hits, moved

def enemy_pos():
    b = probe.read(TAB, 4)
    return (int.from_bytes(b[0:2],'little'), int.from_bytes(b[2:4],'little'))

def shot(target):
    """One torpedo at `target` ("83" style). Returns damage, 0 for a miss."""
    guard()
    pin()
    probe.line('t', pause=1.4)
    probe.line('1', pause=1.4)
    probe.line(target, pause=3.2)
    # settle: the damage lands after the tracking animation
    prev = None
    for _ in range(15):
        now = hp_now()
        if prev is not None and now == prev:
            break
        prev = now
        time.sleep(0.4)
    probe.enter(1.2)
    restore()
    guard()
    return HP - prev

def block(target, n, label):
    us = sec()
    ty, tx = int(target[0]), int(target[1])
    d = math.hypot(us[0]-ty, us[1]-tx)
    hits = []
    for i in range(n):
        p0 = enemy_pos()
        dmg = shot(target)
        p1 = enemy_pos()
        hits.append(dmg)
        print('  %s  us=%s  enemy %s->%s  d=%.3f  damage=%5d%s'
              % (label, us, p0, p1, d, dmg, '' if dmg else '   MISS'))
    live = [h for h in hits if h > 0]
    print('  %s: %d/%d hit, mean %.1f, range %d..%d' %
          (label, len(live), n, (sum(live)/len(live)) if live else 0,
           min(live) if live else 0, max(live) if live else 0))
    return d, hits
