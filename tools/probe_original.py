#!/usr/bin/env python3
"""Drive the original EGA Trek and record enemy motion against ship state.

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


def enemies(table):
    """Read the current quadrant's enemy table until the zero terminator."""
    b = read(table, 6 * 12)
    out = []
    for k in range(12):
        y, x, hp = (int.from_bytes(b[k*6+j*2:k*6+j*2+2], "little")
                    for j in range(3))
        if y == 0 and x == 0 and hp == 0:
            break
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
