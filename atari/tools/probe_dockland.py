#!/usr/bin/env python3
"""DOCK and LAND -- the last two things this port has never done.

Open list item 4's remainder. Combat and the evaluation were exercised on
2026-09-10; docking and landing were not, for a plain reason: neither the
starting quadrant nor the combat one put the ship next to a base, and no
planet had been visited. Both need the ship placed deliberately, not hopefully.

SO IT READS THE GALAXY AND NAVIGATES, rather than guessing coordinates. The
base map is `gal_base[64]` and the planets are `planets[19]` (5 bytes each:
quad, sec, cls, find, flags) -- both in the core's own state. The probe picks a
target, works out an EMPTY cell adjacent to it from the sector map, and moves
there. Adjacency is the whole point of both commands: DOCK and ORBIT refuse at
any other range, and a run that never got next to the thing would report a
refusal that looks identical to a broken command.

Every arm re-reads `sector[]` and says what is actually there. The first
version of probe_play.py fired lasers into an empty quadrant and reported no
wedge; that is the failure this guards against.

    make run-probe-dockland
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from session import Session                    # noqa: E402
from altirra_bridge.client import BridgeError  # noqa: E402

SEC = {0: ".", 1: "*", 2: "E", 3: "B", 4: "P", 5: "b", 6: "C", 7: "s",
       8: "u", 9: "R", 10: " ", 11: "N"}


def grid(s):
    return list(s.a.peek(s.sym["sector"], 64))


def show(s, tag):
    c = grid(s)
    rows = ["".join(SEC.get(v, "?") for v in c[r * 8:r * 8 + 8]) for r in range(8)]
    print("  %-16s %s" % (tag, " ".join(rows)), flush=True)
    return c


def find(cells, code):
    for i, v in enumerate(cells):
        if v == code:
            return i
    return None


def adjacent_empty(cells, at):
    """An empty cell touching `at` -- where DOCK and ORBIT actually work."""
    ay, ax = at // 8, at % 8
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if not dy and not dx:
                continue
            y, x = ay + dy, ax + dx
            if 0 <= y < 8 and 0 <= x < 8 and cells[y * 8 + x] == 0:
                return y * 8 + x
    return None


def move_to(s, quad, sec, tag):
    """M quad_y,quad_x,sec_y,sec_x -- inline, so no dialog to drive."""
    qy, qx = quad // 8 + 1, quad % 8 + 1
    sy, sx = sec // 8 + 1, sec % 8 + 1
    print("  %-16s M %d,%d,%d,%d" % (tag, qy, qx, sy, sx), flush=True)
    s.keys("M,%d,COMMA,%d,COMMA,%d,COMMA,%d,RETURN" % (qy, qx, sy, sx), 400)
    return find(grid(s), 2)          # where the ship ACTUALLY ended up


def move_beside(s, quad, target, tag):
    """Get NEXT TO `target`, trying each free neighbour until one is reached.

    MOVEMENT IS A STRAIGHT LINE AND OBJECTS BLOCK IT. The first version picked
    the first empty neighbour, issued one M, and carried straight on to DOCK --
    and the ship had not moved at all, because a star sat between. The screen
    then showed the ship being shot at instead of docking, which reads exactly
    like a broken DOCK command. So: try the candidates, and VERIFY the ship
    arrived before believing any of them.
    """
    ay, ax = target // 8, target % 8
    cands = []
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if dy or dx:
                y, x = ay + dy, ax + dx
                if 0 <= y < 8 and 0 <= x < 8 and grid(s)[y * 8 + x] == 0:
                    cands.append(y * 8 + x)
    for spot in cands:
        at = move_to(s, quad, spot, tag)
        if at == spot:
            print("  %-16s ARRIVED at sector %d,%d (adjacent)"
                  % ("", spot // 8 + 1, spot % 8 + 1), flush=True)
            return spot
        print("  %-16s blocked -- ship is at %s, wanted %d"
              % ("", at, spot), flush=True)
    return None


def main():
    with Session(scratch="dl.atr", shots="shots-dockland", deadline=25.0) as s:
        print("dockland: booted, far_used %d" % s.loaded, flush=True)
        s.keys("RETURN"); s.keys("N,RETURN"); s.keys("N,RETURN")
        s.keys("J,A,M,I,E,RETURN"); s.keys("1,RETURN"); s.keys("T,R,E,K,RETURN")
        s.snap("console")

        gal_base = list(s.a.peek(s.sym["gal_base"], 64))
        n_pl = s.a.peek(s.sym["planet_count"], 1)[0]
        praw = list(s.a.peek(s.sym["planets"], 5 * 19))
        planets = [(praw[i * 5], praw[i * 5 + 1]) for i in range(n_pl)]
        bases = [(i, b) for i, b in enumerate(gal_base) if b]
        print("dockland: %d bases, %d planets" % (len(bases), n_pl), flush=True)
        print("  bases   : %s" % ", ".join("%d,%d type %d" % (q // 8 + 1, q % 8 + 1, b)
                                           for q, b in bases[:6]), flush=True)
        print("  planets : %s" % ", ".join("q%d,%d s%d,%d" % (q // 8 + 1, q % 8 + 1,
                                                              c // 8 + 1, c % 8 + 1)
                                           for q, c in planets[:6]), flush=True)

        # ---- DOCK ------------------------------------------------------
        if bases:
            bq = bases[0][0]
            move_to(s, bq, 27, "to the base quad")     # land mid-quadrant first
            cells = show(s, "arrived")
            b_at = find(cells, 3)
            if b_at is None:
                print("  dock             NO BASE in sector[] -- cannot test", flush=True)
            else:
                spot = move_beside(s, bq, b_at, "next to the base")
                if spot is None:
                    print("  dock             could not get adjacent -- NOT a "
                          "test of DOCK", flush=True)
                else:
                    show(s, "in position")
                    s.keys("D,RETURN", 400)
                    s.shot("1-dock.png")
                    print("  dock             -> 1-dock.png", flush=True)

        # ---- LAND ------------------------------------------------------
        s.rewind("console")
        if planets:
            pq, pc = planets[0]
            move_to(s, pq, 27, "to the planet quad")
            cells = show(s, "arrived")
            p_at = find(cells, 4)
            if p_at is None:
                print("  land             NO PLANET in sector[] -- cannot test", flush=True)
            else:
                spot = move_beside(s, pq, p_at, "next to the planet")
                if spot is None:
                    print("  land             could not get adjacent -- NOT a "
                          "test of LAND", flush=True)
                else:
                    show(s, "in position")
                    s.keys("O,RETURN", 400)
                    s.shot("2-orbit.png")
                    print("  orbit            -> 2-orbit.png", flush=True)
                    s.keys("L,A,N,D,RETURN", 500)
                    s.shot("3-land.png")
                    print("  land             -> 3-land.png", flush=True)

        print("\ndockland: read the screenshots -- a command that refuses is "
              "not the same as a command that worked.")


main()
