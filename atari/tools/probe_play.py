#!/usr/bin/env python3
"""Exercise the paths this port has never run: combat, docking, the endgame.

Open list item 4 -- "it has never fought, docked, landed on a planet or reached
the hall of fame". A turn is not a game, and every fault worth having on this
project was found by putting the game through something rather than by reading
it.

THIS IS WHAT THE SNAPSHOT HARNESS WAS FOR. One boot, one setup, a snapshot at
the console, and then each arm rewinds to it. Before tools/session.py the same
three arms cost three cold boots and looked like a hang from outside.

It also READS THE QUADRANT before each arm rather than assuming: a combat arm
run in a quadrant with no Mongols in it proves nothing, and would look exactly
like combat that works. What is actually there is printed either way.

    make run-probe-play
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from session import Session                    # noqa: E402
from altirra_bridge.client import BridgeError  # noqa: E402

SEC = {0: ".", 1: "*", 2: "E", 3: "B", 4: "P", 5: "b", 6: "C", 7: "s",
       8: "u", 9: "R", 10: " ", 11: "N"}
MONGOL = (5, 6, 7, 8)          # battleship, commander, scout, supply


def scene(s, tag):
    """What is actually in this quadrant, read out of the core's own state."""
    cells = s.a.peek(s.sym["sector"], 64)
    rows = ["".join(SEC.get(c, "%d" % c) for c in cells[r * 8:r * 8 + 8])
            for r in range(8)]
    kinds = {}
    for c in cells:
        if c:
            kinds[SEC.get(c, str(c))] = kinds.get(SEC.get(c, str(c)), 0) + 1
    print("  %-14s quadrant: %s" % (tag, " ".join(rows)), flush=True)
    print("  %-14s contents: %s" % ("", kinds or "empty"), flush=True)
    return kinds


def find_mongols(s):
    """A quadrant that actually HAS Mongols, read out of gal_enemies.

    The first run of this probe fired lasers and torpedoes in a quadrant
    holding one ship and two stars, and reported that nothing wedged. Nothing
    wedged because nothing happened. A combat test needs something to shoot.
    """
    gal = s.a.peek(s.sym["gal_enemies"], 64)
    best = [(n, i) for i, n in enumerate(gal) if n]
    best.sort(reverse=True)
    return [(n, (i // 8) + 1, (i % 8) + 1) for n, i in best[:5]]


def arm(s, name, keys, settle=240):
    """Rewind to the console, drive one thing, screenshot it.

    EVERY COMMAND ENDS IN RETURN. The first version typed the command letter
    and waited -- so `S`, `L`, `T` and `D` sat in the command field, none of
    them ran, and four arms came back "no wedge" having done nothing at all.
    A test that cannot act looks exactly like a test that passed.
    """
    s.rewind("console")
    kinds = scene(s, name)
    try:
        for k in keys:
            s.keys(k, settle)
    except BridgeError as e:
        print("  %-14s WEDGED: %s" % (name, e), flush=True)
        return kinds, False
    p = s.shot("%s.png" % name)
    print("  %-14s -> %s   PC %s" % (name, p.name, s.a.regs()["PC"]), flush=True)
    return kinds, True


def main():
    with Session(scratch="play.atr", shots="shots-play", deadline=25.0) as s:
        print("play: booted, far_used %d" % s.loaded, flush=True)
        s.keys("RETURN"); s.keys("N,RETURN"); s.keys("N,RETURN")
        s.keys("J,A,M,I,E,RETURN"); s.keys("1,RETURN"); s.keys("T,R,E,K,RETURN")
        s.snap("console")
        s.shot("0-console.png")
        scene(s, "at start")
        print(flush=True)

        targets = find_mongols(s)
        print("play: quadrants with Mongols: %s"
              % ", ".join("%d at %d,%d" % t for t in targets), flush=True)

        # 1. GO AND FIND A FIGHT. Move to the fullest quadrant, then look.
        if targets:
            n, qy, qx = targets[0]
            arm(s, "1-warp-to-mongols",
                ["M,%d,COMMA,%d,COMMA,4,COMMA,4,RETURN" % (qy, qx)], 400)
            s.snap("in-combat")
            k = scene(s, "arrived")
            if any(SEC[c] in "bCsu" for c in MONGOL if SEC[c] in k):
                arm2 = True
            else:
                arm2 = False
                print("play: no Mongols visible after the move -- combat arms "
                      "would prove nothing, skipping", flush=True)
        else:
            arm2 = False

        # 2. LASERS, at something. The dialog asks how much energy.
        if arm2:
            s.rewind("in-combat"); scene(s, "2-lasers")
            s.keys("L,RETURN", 240); s.keys("5,0,0,RETURN", 300)
            s.shot("2-lasers.png")
            print("  2-lasers       fired", flush=True)
            scene(s, "after lasers")

        # 3. DOCK. Says so plainly when there is no base -- worth seeing.
        arm(s, "3-dock", ["D,RETURN"])

        # 4. THE ENDGAME. Self-destruct needs the password, then runs the
        #    score sheet (.ovl_eval) and the hall of fame (.ovl_hof) -- three
        #    overlays that have never executed on this machine.
        arm(s, "4-selfdestruct",
            ["S,RETURN", "T,R,E,K,RETURN", "RETURN", "RETURN"], 400)

        print("\nplay: screenshots in build/shots-play -- read them, do not "
              "assume a run that did not wedge did the right thing.")


main()
