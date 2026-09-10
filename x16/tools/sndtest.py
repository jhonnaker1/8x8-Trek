#!/usr/bin/env python3
"""Measure the X16's refusal beep out of x16emu's own audio recording.

WHY A RECORDING AND NOT A REGISTER READ. The MEGA65's beep was settled with a
probe counting driver calls, because SID registers are write-only and there was
nothing else. Here there is something else: x16emu records a WAV of its audio
output -- the only emulator on this project that does -- so the beep can be
measured as SOUND. That answers a question a probe cannot: not "what did the
driver intend" but "what came out".

It matters here more than it did there. `snd_beep()` plays voice_note(V_SFX,20)
for six frame ticks, and BOTH halves of that are claims rather than facts: the
pitch is a claim about what VERA does with a frequency word, and "six frame
ticks" is a claim about a clock on a machine whose clocks have already been
wrong twice (RDTIM returns zero forever, VSYNC never sets).

Reads pitch by counting zero crossings, which is exact for the 50% pulse this
driver selects, and duration from the burst's own length in samples.
"""
import os, struct, subprocess, sys, time, wave

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EMU  = os.path.expanduser("~/x16emu_macos_m1-r48/x16emu")
WAV  = "/tmp/x16beep.wav"

# What the original was MEASURED at, and what three other ports implement.
WANT_HZ, WANT_MS = 440.0, 250.6

# The tones sndtest.c plays before the beeps, and what the driver believes each
# one is. Two inside the analyser's trustworthy range so the reading is not one
# sample, and one deliberately above it -- see pitch() -- so every run shows
# what this instrument cannot do.
REFS = ((0, 440.0), (1, 300.0), (2, 880.0))

# Frame-tick holds, in ticks. THREE, so the result is a slope: any fixed
# overhead per hold lands in the intercept where it can be seen, instead of
# being averaged into the rate where it cannot. That intercept is what found
# the free first tick: -15.4ms against a 16.77ms slope.
HOLDS = (6, 15, 30)


def record():
    if os.path.exists(WAV):
        os.unlink(WAV)
    p = subprocess.Popen(
        [EMU, "-rom", os.path.expanduser("~/x16emu_macos_m1-r48/rom.bin"),
         "-prg", os.path.join(HERE, "build/sndtest.prg"), "-run",
         "-wav", WAV + ",auto", "-warp"],
        cwd=HERE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(12)
    p.terminate()
    p.wait()
    if not os.path.exists(WAV):
        sys.exit("sndtest: x16emu wrote no WAV -- is -wav supported in r48?")


def samples():
    w = wave.open(WAV, "rb")
    ch, width, rate, n = (w.getnchannels(), w.getsampwidth(),
                          w.getframerate(), w.getnframes())
    if width != 2:
        sys.exit("sndtest: expected 16-bit samples, got %d-bit" % (width * 8))
    raw = w.readframes(n)
    w.close()
    vals = struct.unpack("<%dh" % (len(raw) // 2), raw)
    left = list(vals[::ch])          # one channel is enough for a mono beep
    return rate, left


def bursts(rate, s, win=64, floor=200):
    """Windows whose peak clears `floor` are sound; the rest are silence."""
    out, run = [], None
    for i in range(0, len(s) - win, win):
        loud = max(abs(v) for v in s[i:i + win]) > floor
        if loud and run is None:
            run = i
        elif not loud and run is not None:
            if i - run > rate // 100:        # ignore anything under 10ms
                out.append((run, i))
            run = None
    if run is not None:
        out.append((run, len(s)))
    return out


def duty(s, a, b):
    """Fraction of samples above the burst's own midpoint.

    This is how WAVE 0x00 was caught: the comment said "pulse, 50%" and the
    recording said 0.9%. A waveform's shape is as measurable as its pitch and
    nothing here was looking at it."""
    seg = s[a + (b - a) // 3:a + (b - a) // 3 + min(20000, (b - a) // 3)]
    if not seg:
        return 0.0
    mid = (min(seg) + max(seg)) / 2.0
    return 100.0 * sum(1 for v in seg if v > mid) / len(seg)


def pitch(s, a, b, rate):
    """Hz from the MIDDLE of the burst, by the median interval between rising
    crossings of the segment's own mean.

    Three things this gets right that the first version did not. It reads the
    middle 60%, because a window that includes the attack or the tail averages
    silence into the crossing RATE and reports a pitch far too low -- the first
    run of this said 108Hz for a tone the arithmetic puts at 200. It counts
    RISING crossings only, one per period rather than two. And it takes the
    MEDIAN interval, so a stray crossing from the emulator's resampling moves
    nothing, where a count over the whole segment would be inflated by it."""
    # WHERE THIS ANALYSIS BREAKS, and it is a property of the SIGNAL rather
    # than of the code below. While the driver selected VERA's narrowest pulse
    # (0.9% duty, measured) a note above ~600Hz was on for under one sample at
    # 48828Hz, so periods went unsampled and the median interval landed on two
    # of them -- a reading exactly an octave low. That was diagnosed here and
    # then FIXED IN THE DRIVER: with a 50% square the 880Hz reference reads
    # 887.8. Kept as a comment because the failure will return the moment
    # anything plays a narrow pulse again.
    lo = a + (b - a) // 5
    hi = b - (b - a) // 5
    seg = s[lo:hi]
    if len(seg) < 64:
        return 0.0
    dc = sum(seg) / float(len(seg))
    ups = [i for i in range(1, len(seg))
           if seg[i - 1] - dc < 0 <= seg[i] - dc]
    if len(ups) < 3:
        return 0.0
    gaps = sorted(ups[i] - ups[i - 1] for i in range(1, len(ups)))
    period = gaps[len(gaps) // 2]
    return rate / float(period) if period else 0.0


def main():
    record()
    rate, s = samples()
    found = bursts(rate, s)
    print("sndtest: %s, %d Hz, %.2fs recorded, %d burst(s)"
          % (WAV, rate, len(s) / float(rate), len(found)))
    if not found:
        sys.exit("sndtest: no sound in the recording at all")

    # BURSTS 1 AND 2 ARE REFERENCES the driver believes are 440 and 880.
    # Two of them, not one: a single wrong reading cannot say whether the error
    # is a SCALE or an OFFSET, and that is the whole question here.
    for n, want in REFS:
        ra, rb = found[n]
        got = pitch(s, ra, rb, rate)
        # FLAG ON THE READING, NOT ON THE FREQUENCY. This used to say "above
        # the instrument's limit" for anything over 600Hz, which was true only
        # while the driver played a sub-sample-wide pulse: with a 50% square
        # the 880 reference reads 887.8. A hardcoded limit outlived its cause
        # by one commit.
        off = abs(got - want) / want if want else 1.0
        flag = "" if off < 0.05 else "   [OFF -- see pitch() on narrow pulses]"
        print("  reference: driver says %5.0f Hz, VERA gives %6.1f Hz  (x%.3f)%s"
              % (want, got, got / want if want else 0, flag))

    # THE FRAME-TICK CALIBRATION: three holds of a known tick count.
    holds = found[len(REFS):len(REFS) + len(HOLDS)]
    if len(holds) == len(HOLDS):
        pts = []
        for (a, b), n in zip(holds, HOLDS):
            d = (b - a) * 1000.0 / rate
            pts.append((n, d))
            print("  hold %2d ticks: %7.1f ms   (%5.2f ms/tick)" % (n, d, d / n))
        (n0, d0), (n1, d1) = pts[0], pts[-1]
        slope = (d1 - d0) / float(n1 - n0)
        icept = d0 - slope * n0
        print("  -> %.2f ms per tick (%.1f Hz), intercept %+.1f ms"
              % (slope, 1000.0 / slope if slope else 0, icept))

    hz, ms = [], []
    for n, (a, b) in enumerate(found[len(REFS) + len(HOLDS):], 1):
        f = pitch(s, a, b, rate)
        d = (b - a) * 1000.0 / rate
        hz.append(f); ms.append(d)
        print("  beep %d: %6.1f Hz  %6.1f ms  duty %4.1f%%"
              % (n, f, d, duty(s, a, b)))
    if not hz:
        sys.exit("sndtest: reference only -- no beeps in the recording")

    mh, mm = sum(hz) / len(hz), sum(ms) / len(ms)
    print("  mean:   %6.1f Hz  %6.1f ms" % (mh, mm))
    print("  spec:   %6.1f Hz  %6.1f ms   (measured off the original)"
          % (WANT_HZ, WANT_MS))
    print("  ratio:  %6.2fx pitch, %6.2fx length" % (mh / WANT_HZ, mm / WANT_MS))


main()
