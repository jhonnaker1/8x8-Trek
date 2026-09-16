#!/usr/bin/env python3
"""Read the PITCH out of a recorded emulator session, not the loudness.

`-wavwrite` gives a whole session; this finds the tone bursts in it and says
what frequency each one actually was. "It makes a noise" is not a frequency,
and this project shipped a port an OCTAVE FLAT for four months because one
check passed by ear.

ZERO CROSSINGS, NOT AN FFT, and that is a fit to the signal rather than an
economy: the CoCo's audio here is a DAC toggled between two levels, so the
waveform is a square and its crossings are exactly two per cycle. No numpy on
this machine either, and a stdlib FFT over five million frames is not worth
writing when the signal hands you the answer.

    hearit.py FILE.wav [expected_hz ...]

With expected values it checks each burst against the nearest one and fails if
any is out by more than 1% -- a fifth of a semitone.

Bursts are found by RMS over 20 ms windows, and the middle 60% of each is
measured so the attack and release cannot drag the count.
"""
import array, sys, wave

WIN_MS   = 20
TOL      = 0.01          # 1%, against a semitone of 5.9%
MIN_MS   = 120           # shorter than this is a click, not a note


def load(path):
    w = wave.open(path, "rb")
    n, ch, sw, rate = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
    if sw != 2:
        sys.exit("hearit: expected 16-bit samples, got %d-byte" % sw)
    a = array.array("h")
    a.frombytes(w.readframes(n))
    w.close()
    if ch > 1:                                  # fold to mono
        a = array.array("h", [sum(a[i:i + ch]) // ch for i in range(0, len(a), ch)])
    return a, rate


def bursts(a, rate):
    """Contiguous runs of windows whose RMS is above a tenth of the loudest."""
    win = max(1, (rate * WIN_MS) // 1000)
    energies = []
    for s in range(0, len(a) - win, win):
        # VARIANCE, NOT RAW POWER. Silence in these recordings is not zero --
        # it is a flat DC level of -8192 -- so summing squares makes the quiet
        # parts the loudest thing in the file and the whole session reads as
        # one continuous burst. It did exactly that the first time.
        sl = a[s:s + win:4]                     # every 4th sample is plenty
        m = sum(sl) // len(sl)
        energies.append((s, sum((x - m) * (x - m) for x in sl)))
    if not energies:
        return []
    peak = max(e for _, e in energies)
    if peak == 0:
        return []
    out, run = [], None
    for s, e in energies:
        if e > peak // 50:
            if run is None:
                run = [s, s + win]
            else:
                run[1] = s + win
        elif run is not None:
            out.append(run)
            run = None
    if run is not None:
        out.append(run)
    return [(s, t) for s, t in out if (t - s) * 1000 // rate >= MIN_MS]


def pitch(a, rate, s, t):
    """Crossings of the segment's own mean, over its middle 60%."""
    lo = s + (t - s) * 20 // 100
    hi = s + (t - s) * 80 // 100
    mean = sum(a[lo:hi]) // max(1, hi - lo)
    crossings, prev = 0, a[lo] - mean
    for i in range(lo + 1, hi):
        cur = a[i] - mean
        if (prev < 0) != (cur < 0):
            crossings += 1
        if cur != 0:
            prev = cur
    secs = (hi - lo) / float(rate)
    return crossings / 2.0 / secs if secs else 0.0


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__.strip().splitlines()[-6].strip())
    a, rate = load(sys.argv[1])
    want = [float(x) for x in sys.argv[2:]]

    found = bursts(a, rate)
    if not found:
        print("hearit: NO TONE AT ALL in %s" % sys.argv[1])
        return 1
    print("hearit: %d burst(s) in %.1fs at %d Hz" %
          (len(found), len(a) / float(rate), rate))

    # IN ORDER, not nearest-match. Nearest-match would pass a recording whose
    # tones came out in the wrong order, or one where two of three notes were
    # the same pitch -- and the expected values are written next to the code
    # that plays them, so their order is known.
    bad = 0
    for i, (s, t) in enumerate(found):
        f = pitch(a, rate, s, t)
        line = "  %6.2fs  %5.0f ms  %8.1f Hz" % (s / float(rate),
                                                 (t - s) * 1000 // rate, f)
        if i < len(want):
            err = abs(f - want[i]) / want[i]
            line += "   want %7.1f   %+.2f%%  %s" % (
                want[i], 100.0 * (f - want[i]) / want[i],
                "ok" if err <= TOL else "OUT")
            if err > TOL:
                bad += 1
        print(line)

    if len(found) < len(want):
        print("hearit: only %d burst(s), %d expected" % (len(found), len(want)))
        return 1
    if bad:
        print("hearit: %d burst(s) off by more than %.0f%%" % (bad, TOL * 100))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
