#!/usr/bin/env python3
"""Drive the original EGA Trek and measure it through its own memory.

Four traps, each of which produced confident wrong numbers before it was
found. Read them before designing a run:

  1. The enemy table is NOT zero-terminated -- see enemies() below.
  2. Damage read as a fall in the shield/energy pools is NOT enemy fire.
     A Vandal Death Pod hits every ship in the quadrant, ours included, and
     against a weak enemy the pods are most of what the pools record. The
     pod hits the enemy for the same figure, so pinning the enemy's hit
     points and reading them back after the turn measures the pod exactly;
     subtract it. The damage-report TEXT names the firer and needs no such
     correction, which makes it the better instrument when it is legible.
  3. The pools settle after the volley animation, not when the command
     returns. Read too early and the turn reads zero, then the late damage
     lands on the next reading and doubles it. Poll until two reads agree.
  4. Firing zero at a target still opens a "Hit enter to continue" prompt,
     and the weapons dialog asks once PER LIVE ENEMY. Miss either and the
     next command is swallowed answering the stale prompt, which shows up as
     alternating live/dead turns.

Reads the enemy table (6-byte records: y, x, hit points) and the ship record
directly out of memory. Both move between runs -- the ship record shifted 16
bytes between two launches of the same binary -- so nothing is hard-coded;
the ship record is located by scanning for known Turbo Pascal reals and the
enemy table is found at a fixed delta from it.
"""
import json, math, os, sys, time, urllib.request

BASE  = os.environ.get("DOSBOX_API", "http://localhost:8386/api/v1")
TOKEN = os.environ.get("DOSBOX_API_TOKEN", "")
OUT   = os.environ.get("DOSBOX_SHOT_DIR", ".")
LO, HI = 7728, 346032

# Enemy table sits this far past the `energy` field of the ship record.
# Confirmed across two launches whose absolute addresses differed by 16.
ENEMY_DELTA = 2206


def call(path, data=None, raw=False, method=None):
    req = urllib.request.Request(
        BASE + path,
        data=json.dumps(data).encode() if data is not None else None,
        headers={"Authorization": "Bearer " + TOKEN,
                 "Content-Type": "application/json"},
        method=method or ("POST" if data is not None else "GET"))
    with urllib.request.urlopen(req, timeout=60) as r:
        body = r.read()
    return body if raw else json.loads(body or b"{}")


def read(off, length):
    req = urllib.request.Request("%s/memory/%d/%d" % (BASE, off, length),
        headers={"Authorization": "Bearer " + TOKEN})
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read()


def tp_real(v):
    e = math.floor(math.log2(v))
    m = int(round((v / (2.0 ** e) - 1.0) * (1 << 39)))
    if m >> 39:
        m, e = 0, e + 1
    return bytes([e + 129]) + bytes((m >> (8 * i)) & 0xFF for i in range(5))


def tp_decode(b):
    if b[0] == 0:
        return 0.0
    m = int.from_bytes(b[1:6], "little")
    neg = m >> 39
    m &= (1 << 39) - 1
    v = (1.0 + m / float(1 << 39)) * (2.0 ** (b[0] - 129))
    return -v if neg else v


def dump():
    buf, off = bytearray(), LO
    while off < HI:
        n = min(32768, HI - off)
        buf += read(off, n); off += n
    return bytes(buf)


def find_ship_record(mem):
    """Locate `energy` by the 5000/2500 pair the ship starts with."""
    pat = tp_real(5000.0)
    i = mem.find(pat)
    while i != -1:
        # energy(cur,max) then shields(cur,max) six bytes apart
        if mem[i+6:i+12] == pat and mem[i+12:i+18] == tp_real(2500.0):
            return LO + i
        i = mem.find(pat, i + 1)
    return None


def enemies(table, slots=12):
    """Read the live enemies out of the current quadrant's table.

    The table is NOT zero-terminated, which cost an experiment to find out.
    Killing a ship zeroes its record in place and leaves it there, so a
    quadrant that started with four and lost three reads as
    [live, 0, 0, 0, live] -- stopping at the first zero silently hides every
    enemy past the hole, including ones that wander in later. Scan all the
    slots and filter.

    The word the game keeps beside this (`nenhere` in our notes) counts slots
    in use, not ships alive, so it is not a terminator either."""
    b = read(table, 6 * slots)
    out = []
    for k in range(slots):
        y, x, hp = (int.from_bytes(b[k*6+j*2:k*6+j*2+2], "little")
                    for j in range(3))
        if hp and 1 <= y <= 8 and 1 <= x <= 8:
            out.append((y, x, hp))
    return out


def type_(text, pause=0.35):
    call("/input/type", {"text": text, "cps": 30}); time.sleep(pause)


def enter(pause=1.5):
    call("/input/sequence", {"events": [
        {"t": 0, "type": "key", "key": "KBD_enter", "pressed": True},
        {"t": 60, "type": "key", "key": "KBD_enter", "pressed": False}]})
    time.sleep(pause)


def line(text="", pause=1.5):
    if text:
        type_(text)
    enter(pause)


def shot(name):
    p = os.path.join(OUT, name + ".png")
    with open(p, "wb") as f:
        f.write(call("/video/frame?format=png", raw=True))
    return p


def new_game(level=3, name="KIRK", pw="ABC"):
    """Title screen through to the console."""
    line()                 # dismiss title
    line("n")              # no briefing
    line("n")              # no saved game
    line(name)
    line(str(level))
    line(pw, pause=3.0)


def write(off, data):
    import base64
    req = urllib.request.Request(
        "%s/memory/%d" % (BASE, off),
        data=json.dumps({"data": base64.b64encode(data).decode()}).encode(),
        headers={"Authorization": "Bearer " + TOKEN,
                 "Content-Type": "application/json"},
        method="PUT")
    with urllib.request.urlopen(req, timeout=30) as r:
        return r.read()


# Addresses from one particular launch, kept only as a worked example.
# THEY MOVE BETWEEN RUNS AND THEY DO NOT ALL MOVE TOGETHER -- between two
# launches of the same binary the ship record shifted +16 bytes while the
# twelve system percentages did not shift at all. Re-locate every structure
# every session; see MEASURED.md for how to anchor each one.
A_ENERGY, A_SHIELD, A_SECTOR, A_NENHERE = 181956, 181968, 182104, 182108
A_TABLE = 184162


def restore(energy=5000.0, shields=2500.0):
    """Pin the ship's energy and shields so `forces` inputs stay fixed.

    Both words of each pair are written: the second is not a maximum, it
    tracks the current value (a display latch), and leaving it stale makes
    the gauges disagree with the state the game actually uses.
    """
    write(A_ENERGY, tp_real(energy) * 2)
    write(A_SHIELD, tp_real(shields) * 2)


def snap():
    return {"sec": tuple(int.from_bytes(read(A_SECTOR + 2*i, 2), "little")
                         for i in range(2)),
            "energy": round(tp_decode(read(A_ENERGY, 6))),
            "shields": round(tp_decode(read(A_SHIELD, 6)), 1),
            "n": int.from_bytes(read(A_NENHERE, 2), "little"),
            "enemies": enemies(A_TABLE)}
