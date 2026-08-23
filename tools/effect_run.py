#!/usr/bin/env python3
"""Which of EGA Trek's sound effects belongs to which action?

tools/extract_music.py found five short tracks and where each is started in the
code, but not what any of them MEANS. Guessing is exactly what the notes exist
to prevent, so this asks the game.

The trick is that the player's current-track pointer at DGROUP+0x1cb6 is never
cleared -- when a track ends only the flag at +0x1cc9 drops. So the pointer
still names the LAST effect played, and identifying one needs no fast polling:
do a thing, read the pointer, see whether it changed.

    DOSBOX_API_TOKEN=... python3 tools/effect_run.py

WHAT ACTUALLY SETTLED IT was not this script but reading the strings each
calling routine prints -- see tools/dis16.py and MEASURED.md. This found one
effect of five before the static route found all five in a single pass. Kept
because the technique is right for questions the strings cannot answer (WHEN
something fires, as opposed to what routine owns it), and because both traps it
documents cost a run each.

TRAP ONE: the pointer is never cleared, so an effect REPLAYING looks identical
to nothing happening. The first version reported "silent" for ten actions in a
row against a game that was visibly firing lasers and taking hits. Poke the
pointer to a sentinel before each action -- safe only while the flag is down,
because the ISR never dereferences it then.

TRAP TWO: this drives the setup screen at startup, so running it against an
emulator that ALREADY has a game going types the answers into the game instead.
Relaunch the emulator for every run.
"""
import os
import struct
import sys
import time
import urllib.request

BASE  = os.environ.get("DOSBOX_API", "http://localhost:8386/api/v1")
TOKEN = os.environ.get("DOSBOX_API_TOKEN", "")

TRACK_PTR = 0x1cb6
PLAYING   = 0x1cc9
KNOWN = {0x057E: "TITLE", 0x071A: "END", 0x099A: "SFX_A", 0x09A0: "SFX_B",
         0x09AE: "SFX_C", 0x09B4: "SFX_D", 0x09BA: "SFX_E"}


def call(path, data=None, raw=False):
    import json
    req = urllib.request.Request(
        BASE + path,
        data=json.dumps(data).encode() if data is not None else None,
        headers={"Authorization": "Bearer " + TOKEN,
                 "Content-Type": "application/json"},
        method="POST" if data is not None else "GET")
    with urllib.request.urlopen(req, timeout=30) as r:
        body = r.read()
    return body if raw else json.loads(body or b"{}")


def mem(off, length):
    req = urllib.request.Request("%s/memory/%d/%d" % (BASE, off, length),
                                 headers={"Authorization": "Bearer " + TOKEN})
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read()


def find_dgroup():
    exe = open("reference/EGATREK_unpacked.exe", "rb").read()
    needle = exe[0x2D820 + 0x57E:0x2D820 + 0x57E + 20]
    for start in range(0x10000, 0xA0000, 0x8000):
        i = mem(start, 0x8000).find(needle)
        if i >= 0:
            return start + i - 0x57E
    raise SystemExit("could not locate the data segment")


def poke(off, data):
    import base64, json
    req = urllib.request.Request(
        "%s/memory/%d" % (BASE, off),
        data=json.dumps({"data": base64.b64encode(data).decode()}).encode(),
        headers={"Authorization": "Bearer " + TOKEN,
                 "Content-Type": "application/json"},
        method="PUT")
    urllib.request.urlopen(req, timeout=30).read()


def type_(text):
    call("/input/type", {"text": text, "cps": 20})
    time.sleep(0.4)


def enter():
    call("/input/sequence", {"events": [
        {"t": 0,  "type": "key", "key": "KBD_enter", "pressed": True},
        {"t": 60, "type": "key", "key": "KBD_enter", "pressed": False}]})
    time.sleep(1.2)


def line(text=""):
    if text:
        type_(text)
    enter()


def main():
    if not TOKEN:
        sys.exit("set DOSBOX_API_TOKEN")
    dg = find_dgroup()
    print("DGROUP at %06X\n" % dg)

    def state():
        t = struct.unpack("<H", mem(dg + TRACK_PTR, 2))[0]
        f = mem(dg + PLAYING, 1)[0]
        return t, f

    # Into a level 3 game: briefing no, restore no, name, level, password.
    for k in ("", "n", "n", "KIRK", "3", "abc"):
        line(k)
    time.sleep(1.5)

    last, _ = state()
    print("baseline after entering the game: %s\n" % KNOWN.get(last, "%04X" % last))
    print("%-28s %-8s %s" % ("action", "track", "changed?"))
    print("-" * 52)

    # Each entry is a list of lines to send. Kept to actions that need no
    # follow-up dialog, plus the ones whose dialogs are known.
    actions = [
        ("enter quadrant (warp)", ["m", "3,3,4,4"]),
        ("enter another quadrant", ["m", "6,6,4,4"]),
        ("shields up (SHUP)",   ["shup"]),
        ("shields down (SHDN)", ["shdn"]),
        ("max energy (MAX)",    ["max"]),
        ("set warp 3 (W)",      ["w", "3"]),
        ("impulse move",        ["m", "4,4"]),
        ("fire lasers (L)",     ["l", "", "", ""]),
        ("fire torpedo (T)",    ["t", "", "", ""]),
        ("dock (D)",            ["d"]),
        ("energy xfer (E)",     ["e", "100"]),
        ("warp to 4,4",         ["m", "4,4,4,4"]),
    ]

    for what, keys in actions:
        # Clear the pointer to a sentinel first, so an effect REPLAYING is as
        # visible as a different one starting. Without this the first run of
        # this rig reported "no change" for ten actions in a row -- the pointer
        # already held the track and nothing could move it.
        #
        # Safe only while the flag is down: the ISR returns immediately when
        # [0x1cc9] is zero and never dereferences the pointer, and StartMusic
        # sets both together.
        while state()[1]:
            time.sleep(0.2)
        poke(dg + TRACK_PTR, b"\x00\x00")

        for k in keys:
            line(k)
        time.sleep(1.2)
        t, f = state()
        print("%-28s %s" % (what, "-- silent" if t == 0
                            else "<-- %s" % KNOWN.get(t, "%04X" % t)))


if __name__ == "__main__":
    main()
