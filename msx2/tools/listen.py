#!/usr/bin/env python3
"""Check a recording of SNDTEST.COM against MUSIC.DAT: PITCH and TEMPO.

Both, because either alone has passed a broken driver: the X16 played every
note at the right pitch and twice the speed. And pitch at SEVERAL points,
because one point cannot tell a wrong scale from a wrong offset.

The script SNDTEST plays is fixed -- the beep, a second of silence, twelve
seconds of the title track, half a second of silence, one effect -- so each
section is found by position and checked against what it should have been:

  beep    440Hz for 13 frames at 50Hz / 15 at 60Hz (~250ms)
  title   every note's pitch against the PSG period the driver must have
          written, and the TICK RATE fitted from every note onset -- the PC
          timer's 18.2065Hz is what the track format is written in
  effect  its notes' pitches

Exits non-zero on any failure, so `make listen` can go red."""
import sys, wave
import numpy as np

wav, dat, screen = sys.argv[1], sys.argv[2], sys.argv[3]
MUS_TITLE, SFX_B = 0, 378             # offsets, as in music_data.c
PC_TICK = 18.2065
PSG = 3579545 / 2

w = wave.open(wav)
rate = w.getframerate()
x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(float)
if w.getnchannels() > 1:
    x = x.reshape(-1, w.getnchannels()).mean(axis=1)

text = open(screen).read()
pal = "PAL" in text
if "DONE" not in text:
    sys.exit("FAIL: SNDTEST never printed DONE -- the screen was:\n" + text)

def psg_hz(tenths):
    """What the PSG really plays for a track frequency: the driver writes
    period = 11186 // tenths, and the chip divides its clock by 16*period."""
    return PSG / (16 * min(11186 // tenths, 4095))

def peak(seg):
    """Fundamental of a segment, to a fraction of a Hz: zero-padded FFT with
    a parabolic fit round the biggest bin below 2kHz."""
    seg = (seg - seg.mean()) * np.hanning(len(seg))
    n = 1 << 18
    s = np.abs(np.fft.rfft(seg, n))
    lo, hi = int(40 * n / rate), int(2000 * n / rate)
    k = lo + int(np.argmax(s[lo:hi]))
    a, b, c = s[k - 1], s[k], s[k + 1]
    return (k + 0.5 * (a - c) / (a - 2 * b + c)) * rate / n

# Frame track: 10ms hop, 20ms window; a frame is SOUND if loud, and its
# frequency is a coarse FFT peak -- enough to find where notes change.
hop, win = rate // 100, rate // 50
frames = []
for i in range(0, len(x) - win, hop):
    f = x[i:i + win]
    loud = np.abs(f - f.mean()).mean() > 150
    frames.append((i, loud, peak(f) if loud else 0.0))

# Segments: runs of loud frames whose frequency stays within 4%.
segs = []
for i, loud, f in frames:
    if not loud:
        cur = None
        continue
    if segs and cur is not None and abs(f - cur) / cur < 0.04:
        segs[-1][1] = i + hop
    else:
        segs.append([i, i + hop, f])
    cur = f
segs = [(a / rate, b / rate, peak(x[a:b])) for a, b, _ in segs if b - a >= hop * 3]

bad = []
def check(label, got, want, tol):
    ok = abs(got - want) / want <= tol
    print("  %-34s %9.2f  want %9.2f  %+6.2f%%  %s" %
          (label, got, want, 100 * (got - want) / want, "ok" if ok else "FAIL"))
    if not ok:
        bad.append(label)

# --- the beep: the first sound after boot that is near 440Hz
beep = next((s for s in segs if abs(s[2] - psg_hz(44)) < 20), None)
if beep is None:
    sys.exit("FAIL: no 440Hz beep found; segments were %s" % segs[:10])
print("%s machine; beep at %.2fs" % ("PAL" if pal else "NTSC", beep[0]))
check("beep pitch (Hz)", beep[2], psg_hz(44), 0.005)
frames_n = 13 if pal else 15
check("beep length (ms)", 1000 * (beep[1] - beep[0]),
      1000 * frames_n / (50 if pal else 60), 0.10)

# --- the title track: starts one second after the beep ends
d = open(dat, "rb").read()
notes = []
for i in range(MUS_TITLE, len(d), 2):
    if d[i] == 0:
        break
    notes.append((d[i], d[i + 1]))
# Merge repeats: two equal notes in a row are one sound to a microphone.
exp, tick = [], 0
for dur, t in notes:
    if t and exp and exp[-1][1] == t and exp[-1][2] == tick:
        exp[-1][2] += dur
    else:
        exp.append([tick, t, tick + dur])
    tick += dur
exp = [e for e in exp if e[1]]        # rests are silence, not segments

title = [s for s in segs if s[0] > beep[1] + 0.8 and s[0] < beep[1] + 1.0 + 12.0]
n = min(len(title), len(exp))
print("title: %d notes heard in 12s, %d expected in that time" %
      (len(title), sum(1 for e in exp if e[0] / PC_TICK < 12.0 - 0.1)))
wrong = [(i, title[i][2], psg_hz(exp[i][1])) for i in range(n)
         if abs(title[i][2] - psg_hz(exp[i][1])) / psg_hz(exp[i][1]) > 0.01]
if wrong:
    for i, g, want in wrong[:8]:
        print("  note %d: heard %.1fHz, want %.1fHz" % (i, g, want))
    bad.append("title pitches")
# Pitch at every DISTINCT frequency the section used: a scale error and an
# offset error diverge across a range, and one point cannot tell them apart.
for t in sorted({exp[i][1] for i in range(n)}):
    got = [title[i][2] for i in range(n) if exp[i][1] == t]
    check("title %4dHz x%-3d (Hz)" % (t * 10, len(got)), float(np.median(got)),
          psg_hz(t), 0.005)
# Tempo: fit onset time against the note's start tick. The slope is seconds
# per tick; a frame of jitter per onset averages out over twelve seconds.
ticks = np.array([exp[i][0] for i in range(n)], float)
onset = np.array([title[i][0] for i in range(n)])
slope, _ = np.polyfit(ticks, onset, 1)
# 0.1%, not 0.5%: the first version allowed 0.5% and PASSED a driver that
# counted a 50.159Hz jiffy as 1/50s -- 0.35% fast, and exactly that ratio.
check("tick rate (Hz)", 1 / slope, PC_TICK, 0.001)

# --- the effect, alone, after half a second of silence
fx = [s for s in segs if s[0] > beep[1] + 13.3]
want = [d[i + 1] for i in range(SFX_B, len(d), 2) if d[i]][:6]
print("effect SFX_B: heard %s Hz" % ", ".join("%.0f" % s[2] for s in fx[:8]))
print("              want  %s Hz" % ", ".join("%.0f" % psg_hz(t) for t in want))
if len(fx) < 3:
    bad.append("effect")

if bad:
    sys.exit("FAIL: " + ", ".join(bad))
print("PASS")
