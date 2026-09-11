#!/usr/bin/env python3
"""Does the GAME make a sound? Not: does the driver work.

    make run-probe-sound

THIS EXISTS BECAUSE `make run-sndtest` PASSED TWELVE CHECKS ON A SILENT PORT.
That test links src/sndtest.c against the driver and calls `snd_poll()`
ITSELF -- so it measured POKEY's divisors, both video standards and the tempo,
all correctly, while the shipping game never called `snd_poll()` at any point
and played nothing at all. Jamie found it by putting headphones on, on
2026-09-11, which is the fourth time on this project that a measurement and a
person disagreed and the person was right.

So this one boots the real disk, walks the real title screen, and asks Altirra
what POKEY is actually emitting -- with nothing under test that this script
also drives.

MUS_TITLE plays under the title screen, so the sample is taken there: it is
the one place sound happens before any key is pressed, which keeps the test
free of the input seam it is trying to prove.

AND IT CHECKS THE PITCH IS IN RANGE, which is the half that was missing when
this file was written an hour earlier. It printed 1332, 1998, 2458 and 3196 Hz
against a MUSIC.DAT whose whole range is 90 to 930, said "the title music
PLAYS and CHANGES NOTE", and I let it pass -- I even wrote that the numbers
looked high and did not act on it. Jamie said "the pitch is way too high" and
that was the whole of the diagnosis. A probe that reports a number nobody
bounds is a probe that reports nothing.
"""
import pathlib
import re
import shutil
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve()
ATARI = HERE.parents[1]
SERVER = pathlib.Path.home() / "AltirraBridge-nightly-macos-arm64" / "AltirraBridgeServer"
sys.path.insert(0, str(SERVER.parent / "sdk" / "python"))
from altirra_bridge import AltirraBridge      # noqa: E402


def start(log):
    fh = open(log, "w")
    p = subprocess.Popen([str(SERVER), "--bridge", "--settings=user",
                          "--pacing=unlimited"],
                         stdout=subprocess.DEVNULL, stderr=fh)
    dl = time.time() + 20
    while time.time() < dl:
        m = re.search(r"token-file:\s*(\S+)", pathlib.Path(log).read_text())
        if m:
            return p, m.group(1)
        time.sleep(0.2)
    p.kill()
    sys.exit("probe_sound: no token-file")


# MUSIC.DAT's measured range, the same figures src/atarisnd.c chose POKEY's
# 16-bit mode for. A tone outside it is not a tune played badly, it is a
# different tone: the fault this caught was the OS taking POKEY back on every
# disk read, which left a 290 Hz note sounding at 2,458.
LOW_HZ, HIGH_HZ = 80, 1000


def sing(a):
    """Every channel POKEY is driving right now, as (channel, Hz)."""
    return [(i + 1, c["freq_hz"])
            for i, c in enumerate(a.audio_state()["channels"])
            if c["volume"] and c["freq_hz"]]


def main():
    disk = ATARI / "build" / "sound-run.atr"
    shutil.copy(ATARI / "build" / "egatrek.atr", disk)
    proc, token = start(str(ATARI / "build" / "bridge.log"))
    heard = {}
    try:
        with AltirraBridge.from_token_file(token) as a:
            a._sock.settimeout(180)
            a.mount(0, str(disk))
            a.cold_reset()
            # THE TITLE SCREEN, reached by waiting rather than by typing --
            # a key press would go through the very seam under test.
            for _ in range(60):
                a.frame(30)
                for ch, hz in sing(a):
                    heard.setdefault(ch, set()).add(round(hz))
                if len(heard) and sum(len(v) for v in heard.values()) >= 4:
                    break
            a.screenshot(path=str(ATARI / "build" / "sound.png"))
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    if not heard:
        print("probe_sound: SILENT -- POKEY drove no channel at any sample.")
        print("             That is the bug Jamie heard: the driver is "
              "initialised and")
        print("             asked to play, but nothing calls snd_poll(). See "
              "src/atariinput.c.")
        return 1
    bad = []
    for ch in sorted(heard):
        tones = sorted(heard[ch])
        out = [t for t in tones if not (LOW_HZ <= t <= HIGH_HZ)]
        bad += out
        print("probe_sound: channel %d -- %d distinct tones, %s Hz%s"
              % (ch, len(tones), ", ".join(str(t) for t in tones[:8]),
                 "   OUT OF RANGE: %s" % out if out else ""))
    if bad:
        print("probe_sound: %d tone(s) outside MUSIC.DAT's %d-%d Hz range."
              % (len(bad), LOW_HZ, HIGH_HZ))
        print("             Check AUDCTL: the OS sets it to $28 for every SIO "
              "transfer, which")
        print("             un-joins the music voice and plays channel 2 "
              "three octaves up.")
        return 1
    # ONE TONE IS NOT MUSIC. A driver that gates a voice on and never advances
    # would show exactly one, which is a different fault with the same
    # symptom from outside.
    if max(len(v) for v in heard.values()) < 2:
        print("probe_sound: ONE TONE ONLY -- gated on but never advancing.")
        return 1
    print("probe_sound: the title music PLAYS and CHANGES NOTE.")
    return 0


sys.exit(main())
