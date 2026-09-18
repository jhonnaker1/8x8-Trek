#!/usr/bin/env python3
"""Check the F256K's four calibration tones, and only those.

WHY THIS EXISTS RATHER THAN CALLING hearit.py DIRECTLY. A recording of this
machine holds three kinds of burst:

    ~820 ms   FoenixMCP's OWN boot sound, before the program runs at all
   ~1020 ms   the four calibration tones this test plays
 220-260 ms   the tempo track's A440, which runs for the rest of the session

Passing hearit.py a list of four expected values compares every burst against
the NEAREST expected one, so the boot sound shifted the whole list by one
position and reported four failures on a driver that was correct to 0.1%.

SELECTED BY DURATION, NOT BY POSITION. "Skip the first burst" would work today
and break the first time FoenixMCP's startup changes or a tempo burst lands
early. The calibration tones are a second each and nothing else on this
machine is, so the length is what identifies them.

hearit.py does the signal work -- zero crossings over the middle 60% of each
burst -- and is imported rather than copied.
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "coco3gime", "tools"))
import hearit

TONE_MS = 900            # the calibration tones are ~1020; nothing else is
TOL     = 0.01           # 1%, against a semitone of 5.9%

def main():
    path, want = sys.argv[1], [float(x) for x in sys.argv[2:]]
    a, rate = hearit.load(path)
    found = []
    for s, t in hearit.bursts(a, rate):
        ms = (t - s) * 1000.0 / rate
        if ms >= TONE_MS:
            found.append((s / rate, ms, hearit.pitch(a, rate, s, t)))

    print("pitch: %d calibration tone(s) of %d ms or longer" % (len(found), TONE_MS))
    bad = 0
    for i, (at, ms, hz) in enumerate(found):
        if i < len(want):
            err = (hz - want[i]) / want[i]
            ok = abs(err) <= TOL
            if not ok:
                bad += 1
            print("  %6.2fs  %4.0f ms  %8.1f Hz   want %7.1f   %+6.2f%%  %s"
                  % (at, ms, hz, want[i], err * 100, "ok" if ok else "OUT"))
        else:
            print("  %6.2fs  %4.0f ms  %8.1f Hz   (unexpected extra)" % (at, ms, hz))
            bad += 1
    if len(found) != len(want):
        print("pitch: expected %d tones, found %d" % (len(want), len(found)))
        bad += 1
    print("PITCH: ok" if bad == 0 else "PITCH: %d PROBLEM(S)" % bad)
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main())
