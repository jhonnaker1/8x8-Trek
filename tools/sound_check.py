#!/usr/bin/env python3
"""Does the port actually play the right notes? Records VICE and measures.

Sound is the one part of this port that a session on this machine cannot check
by looking, and it fails silently: a wrong frequency constant still plays every
note, just at the wrong pitch, and a wrong region still plays the whole tune.
The bug this was written for made every note 3.8% sharp and nothing else was
different.

So: record the title screen to a WAV, pull the sustained pitches out of it by
autocorrelation, and compare them against the note data the build was made
from. Runs BOTH regions, because getting one right and the other wrong is
exactly what a broken region detector looks like.

    python3 tools/sound_check.py

ALWAYS PASS THE REGION FLAG EXPLICITLY. VICE saves its configuration, so an
earlier `-ntsc` persists into vicerc and silently changes the machine under a
later run -- which is how the original diagnosis went wrong: a recording made
on NTSC was compared against a PAL measurement taken an hour earlier.
"""
import os
import struct
import subprocess
import sys
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_music

ROOT  = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRG   = os.path.join(ROOT, "c128", "build", "trek128.prg")
SHOTS = os.path.join(ROOT, "c128", "build", "sound")
CYCLES = 45000000


def record(region, path):
    subprocess.run(["x128", "-console", "-" + region,
                    "-soundrecdev", "wav", "-soundrecarg", path,
                    "-limitcycles", str(CYCLES), "-autostart", PRG],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def pitches(path):
    w = wave.open(path, "rb")
    sr, ch = w.getframerate(), w.getnchannels()
    raw = w.readframes(w.getnframes())
    s = list(struct.unpack("<%dh" % (len(raw) // 2), raw))
    if ch > 1:
        s = s[0::ch]

    def one(seg):
        m = sum(seg) / len(seg)
        seg = [x - m for x in seg]
        if max(seg) - min(seg) < 2000:
            return None
        best, bv = 0, -1e18
        for lag in range(int(sr / 1800), int(sr / 150)):
            v = sum(seg[i] * seg[i + lag] for i in range(0, len(seg) - lag, 3))
            if v > bv:
                bv, best = v, lag
        return sr / best

    step, win = int(sr * 0.03), int(sr * 0.06)
    runs = []
    for st in range(0, len(s) - win, step):
        f = one(s[st:st + win])
        if f is None:
            runs.append(None)
            continue
        if runs and runs[-1] is not None and abs(runs[-1][0] - f) < f * 0.04:
            runs[-1][1] += 1
            runs[-1][3].append(f)
            continue
        runs.append([f, 1, st / sr, [f]])

    # A note has to hold for at least two windows to count. Single-window
    # readings are the autocorrelator catching a gate transition mid-window and
    # locking onto a subharmonic -- they came out as 150Hz and 1371Hz readings
    # that are not in the track at all.
    # MEDIAN of the run, not its first window. Taking the first made the check
    # flaky -- a window straddling a note boundary autocorrelates against two
    # pitches at once, and one such reading swung a note from +0.4% to -2.0%
    # and tripped the threshold. The middle of a held note is unambiguous.
    out = []
    for r in runs:
        if r is None or r[1] < 2:
            continue
        v = sorted(r[3])
        out.append((v[len(v) // 2], r[2]))
    return out


def collapse(seq):
    """Consecutive equal notes sound like one held note, so a recording can
    never tell them apart. Collapse the expected side the same way before
    comparing, or the two sequences slide apart after the first repeat -- which
    is what made the first version of this script report a 372% error against
    pitches that were in fact correct."""
    out = []
    for x in seq:
        if not out or out[-1] != x:
            out.append(x)
    return out


def expected(track, count):
    """Start times, in the original's 18.2065Hz ticks, of the first `count`
    sounding notes -- collapsing consecutive equal pitches the way a recording
    must, and counting rests, which take time but make no sound."""
    t, out = 0, []
    for dur, ten in track:
        if ten:
            if out and out[-1][1] == ten and out[-1][2] == t:
                out[-1][2] = t + dur
            else:
                out.append([t, ten, t + dur])
        t += dur
    return [(o[0], o[1] * 10) for o in out[:count]]


TICK_HZ = 1193181.0 / 65536.0


def main():
    os.makedirs(SHOTS, exist_ok=True)
    data = open(gen_music.EXE, "rb").read()
    track = gen_music.read_track(data, 0x057E)
    exp  = expected(track, 12)
    want = [hz for _, hz in exp]

    bad = 0
    for region in ("pal", "ntsc"):
        path = os.path.join(SHOTS, "title-%s.wav" % region)
        record(region, path)
        got = pitches(path)

        # Sync on the first expected note before comparing. This checks PITCH,
        # not onset detection, and the recording picks up transients the track
        # does not contain -- a stray 162Hz run led the NTSC take and slid an
        # otherwise perfect sequence by one. Anything before the first real
        # note is discarded rather than compared against.
        head = None
        for i, (f, _t) in enumerate(got):
            if abs(f - want[0]) < want[0] * 0.02:
                head = i
                break
        if head is None:
            print("\n%s -- never found the first note (%d Hz)"
                  % (region.upper(), want[0]))
            bad += 1
            continue
        got = got[head:]

        n = min(len(got), 10)
        print("\n%s -- first %d sustained pitches" % (region.upper(), n))
        worst = 0.0
        for i in range(n):
            err = 100.0 * (got[i][0] - want[i]) / want[i]
            if abs(err) > abs(worst):
                worst = err
            print("   heard %6.0f Hz   want %5d Hz   %+6.2f%%"
                  % (got[i][0], want[i], err))
        print("   worst %+.2f%%" % worst)
        # A semitone is 5.9%; the SID clocks differ by 3.8%. Anything past 2%
        # is a wrong constant, not measurement noise.
        if abs(worst) > 2.0:
            print("   FAIL -- that is a wrong constant, not rounding")
            bad += 1

        # TEMPO, which the pitch check above cannot see at all. It passed for
        # a whole session while PAL music ran 45% slow, because every note was
        # the right note -- just held far too long. The poll rate was too
        # coarse to catch every frame.
        if len(got) >= 11:
            heard = got[10][1] - got[0][1]
            wanted = (exp[10][0] - exp[0][0]) / TICK_HZ
            terr = 100.0 * (heard - wanted) / wanted
            print("   tempo: note 1 to note 11 in %.2fs, want %.2fs (%+.1f%%)"
                  % (heard, wanted, terr))
            # Note onsets are measured in 20ms steps, so about 1% of a 4.4s
            # span is resolution. 5% is comfortably past that and far short of
            # the 45% a missed-frame bug produces.
            if abs(terr) > 5.0:
                print("   FAIL -- the tune is playing at the wrong speed")
                bad += 1
        else:
            print("   FAIL -- too few notes heard to check tempo")
            bad += 1
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
