#!/usr/bin/env python3
"""Does the sound seam play what it was told to? Region, pitch and tempo.

Drives src/sndtest.c, which calls the SHIPPING driver rather than a copy, and
asks Altirra what frequency each POKEY voice is actually producing.

Runs the whole thing twice, once in each video standard, because the region is
what decides both the divide constant and the tempo numerator -- and a test
that only ever sees one region cannot catch the fault that made the MEGA65's
music 19% fast.
"""
import pathlib
import re
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve()
ATARI = HERE.parents[1]
SERVER = pathlib.Path.home() / "AltirraBridge-nightly-macos-arm64" / "AltirraBridgeServer"
sys.path.insert(0, str(SERVER.parent / "sdk" / "python"))
from altirra_bridge import AltirraBridge      # noqa: E402

CMD = 0x0600
XEX = ATARI / "build" / "sndtest.xex"
REGION_NAME = {0: "NTSC", 1: "PAL"}
fails = []


def start_server(log):
    fh = open(log, "w")
    proc = subprocess.Popen(
        [str(SERVER), "--bridge", "--settings=user", "--pacing=unlimited"],
        stdout=subprocess.DEVNULL, stderr=fh)
    deadline = time.time() + 20
    while time.time() < deadline:
        if proc.poll() is not None:
            sys.exit("sndtest: server exited -- see %s" % log)
        m = re.search(r"token-file:\s*(\S+)", pathlib.Path(log).read_text())
        if m:
            return proc, m.group(1)
        time.sleep(0.2)
    proc.kill()
    sys.exit("sndtest: no token-file line")


def voice(a, ch):
    """Channel 2 carries the music voice and channel 4 the effects one -- in a
       joined 16-bit pair the output belongs to the even channel."""
    c = a.audio_state()["channels"][ch - 1]
    return (c["freq_hz"] if c["volume"] else None)


def check(label, got, want, tol_pct):
    if got is None:
        print("  %-34s SILENT              FAIL" % label)
        fails.append(label)
        return
    err = abs(got - want) / want * 100.0
    ok = err <= tol_pct
    print("  %-34s %8.2f Hz  want %6d  %5.2f%%  %s"
          % (label, got, want, err, "pass" if ok else "FAIL"))
    if not ok:
        fails.append(label)


def command(a, code, frames=4):
    ack = a.peek(CMD + 1, 1)[0]
    a.poke(CMD, code)
    a.frame(frames)
    return a.peek(CMD + 1, 1)[0] != ack


def run(a, mode):
    a.config("video", mode)
    a.boot(str(XEX))
    a.frame(220)

    region = a.peek(CMD + 2, 1)[0]
    detected = REGION_NAME.get(region, "?%d" % region)
    ok = detected == mode.upper()
    print("\nvideo=%s" % mode)
    print("  %-34s %s  %s" % ("region detected", detected, "pass" if ok else "FAIL"))
    if not ok:
        fails.append("region in %s" % mode)

    # THE EFFECTS VOICE, through snd_beep -- 440Hz, and the one call in this
    # seam that blocks. The command does not ack until the beep has finished,
    # which is also the check that its bounded loop terminates.
    a.poke(CMD, 1)
    a.frame(3)
    check("beep 440Hz on voice 2", voice(a, 4), 440, 1.0)
    a.frame(40)
    if a.peek(CMD + 1, 1)[0] == 0:
        print("  %-34s DID NOT RETURN      FAIL" % "beep terminates")
        fails.append("beep terminates in %s" % mode)
    else:
        print("  %-34s returned            pass" % "beep terminates")

    # THE MUSIC VOICE: the planted track's two notes are the extremes of the
    # real music's range, so this covers the whole span the divide serves.
    command(a, 2)
    check("note 1 on voice 1", voice(a, 2), 930, 1.0)

    # THE TEMPO AND THE LOOP, AS A SEQUENCE RATHER THAN AS TIMED SAMPLES.
    #
    # The first version of this checked the pitch at two fixed frame counts and
    # called the second one FAIL -- wrongly. Nine ticks at 18.2Hz is 29.6 NTSC
    # frames against 24.8 PAL ones, so "45 frames later" lands in a different
    # note in each region, and by the third sample it was a note further on
    # than the test's arithmetic assumed. The driver was right and the
    # expectation was wrong, which is the least useful kind of failing test.
    #
    # Watching the sequence of distinct pitches instead needs no arithmetic at
    # all: 930, 90, 930 can only happen if the track advanced AND the zero pair
    # looped it back. It is also the check that would catch a track that
    # advanced once and stuck.
    seen = []
    for _ in range(40):
        a.frame(4)
        f = voice(a, 2)
        if f is None:
            continue
        near = 930 if abs(f - 930) < 30 else (90 if abs(f - 90) < 5 else round(f))
        if not seen or seen[-1] != near:
            seen.append(near)
    got = " ".join(str(x) for x in seen[:4])
    ok = seen[:3] == [930, 90, 930]
    print("  %-34s %-19s %s" % ("note sequence (loop proves it)", got,
                                "pass" if ok else "FAIL"))
    if not ok:
        fails.append("loop in %s" % mode)

    command(a, 3)
    if voice(a, 2) is None:
        print("  %-34s silent              pass" % "snd_off")
    else:
        print("  %-34s STILL SOUNDING      FAIL" % "snd_off")
        fails.append("snd_off in %s" % mode)


def main():
    (ATARI / "build").mkdir(parents=True, exist_ok=True)
    proc, token = start_server(str(ATARI / "build" / "bridge.log"))
    try:
        with AltirraBridge.from_token_file(token) as a:
            for mode in ("ntsc", "pal"):
                run(a, mode)
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
    print("\n%s" % ("ALL PASS" if not fails else "FAILURES: " + ", ".join(fails)))
    sys.exit(1 if fails else 0)


main()
