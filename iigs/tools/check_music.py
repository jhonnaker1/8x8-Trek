#!/usr/bin/env python3
"""Check that the title tune PLAYS A TUNE, not one note held down.

`make sound` already settles whether the pitches are RIGHT -- four tones
measured against 440, 1000, 200 and 440 on the second voice. What it cannot
see is whether the music ever advances, and that is a separate failure with
its own cause: the driver was correct and measured, and the first build played
nothing at all because kb_waitkey() did not call snd_poll(). sid.h says in as
many words that it must -- "that is where this port spends every second it is
not drawing" -- and the thing that never called it was thirty-nine bytes away.

hearit.py reports ONE burst for a tune with no rests in it, and reports its
middle pitch, so "441.0 Hz for 14.6 seconds" is exactly what a stuck note and
a working tune both look like from there. This slices the burst instead.
"""
import sys

sys.path.insert(0, __file__.rsplit("/", 1)[0] + "/../../coco3gime/tools")
import hearit                                              # noqa: E402

SLICE_S = 0.25
MIN_DISTINCT = 6          # a tune, not a note
TOL = 0.03                # 3%, against a semitone of 5.9%


def main(argv):
    if len(argv) < 2:
        raise SystemExit("usage: check_music.py SESSION.wav")
    a, rate = hearit.load(argv[1])
    bs = hearit.bursts(a, rate)
    if not bs:
        print("FAIL: no sound at all in the recording")
        return 1

    s, t = max(bs, key=lambda b: b[1] - b[0])
    dur = (t - s) / rate
    print(f"  longest burst {s/rate:.2f}s..{t/rate:.2f}s ({dur:.1f}s)")
    if dur < 3.0:
        print(f"FAIL: the longest burst is {dur:.1f}s -- too short to be the "
              f"title tune")
        return 1

    w = int(rate * SLICE_S)
    pitches = [hearit.pitch(a, rate, s + k * w, s + (k + 1) * w)
               for k in range((t - s) // w)]
    pitches = [p for p in pitches if p > 50]

    # Group pitches that are within TOL of each other: two slices of the same
    # held note must not count as two notes.
    levels = []
    for p in sorted(pitches):
        if not levels or abs(p - levels[-1]) / levels[-1] > TOL:
            levels.append(p)
    print(f"  {len(pitches)} slices, {len(levels)} distinct pitch levels")
    print("  " + " ".join(f"{p:.0f}" for p in levels))

    if len(levels) < MIN_DISTINCT:
        print(f"FAIL: {len(levels)} distinct pitches -- a tune needs at least "
              f"{MIN_DISTINCT}; one level means the music is not advancing")
        return 1
    print(f"check_music: the tune advances -- {len(levels)} distinct pitches "
          f"over {dur:.1f}s")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
