#!/usr/bin/env python3
"""Is the music playing at the RIGHT SPEED? Not the right pitch -- the speed.

THE GAP THIS FILLS. coco3gime/tools/hearit.py reads the PITCH out of a
recording and this project built it after shipping a port an octave flat. It
found nothing wrong with the Plus/4: four bursts, 438.4 / 479.9 / 538.0 /
433.4 Hz, against a calibration of 438.3 Hz for A440 -- while the tune played
at EXACTLY DOUBLE SPEED. Jamie heard it in one listen. A tool that checks pitch
is silent about tempo, and "it makes the right notes" is not "it makes them at
the right time".

src/tedsnd.c counts frames and ticks; this reads both over a wall-clock
interval. PAL wants 50.125 frames and 18.2065 ticks a second -- the latter is
the original's PC timer rate, 18.2065Hz, which is what the music data assumes.

LEAVES xplus4 RUNNING unless --kill, like every rig here.
"""
import argparse, os, subprocess, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import vice_mon

FRAMES_PAL, TICKS = 50.125, 18.2065

ap = argparse.ArgumentParser()
ap.add_argument("--d64", default="build/egatrek-plus4.d64")
ap.add_argument("--elf", default="build/trek4.elf")
ap.add_argument("--nm", default=os.path.expanduser("~/llvm-mos/bin/llvm-nm"))
ap.add_argument("--boot", type=float, default=30.0)
ap.add_argument("--window", type=float, default=10.0)
ap.add_argument("--tol", type=float, default=5.0, help="percent")
ap.add_argument("--kill", action="store_true")
a = ap.parse_args()

out = subprocess.run([a.nm, a.elf], capture_output=True, text=True).stdout
sym = {f[2]: int(f[0], 16) for f in (l.split() for l in out.splitlines()) if len(f) == 3}
for want in ("snd_frames", "snd_ticks"):
    if want not in sym:
        sys.exit("tempo_p4: %s is not in %s -- this gate cannot run" % (want, a.elf))

v = subprocess.Popen(["xplus4", "-binarymonitor",
                      "-binarymonitoraddress", "ip4://127.0.0.1:6502",
                      # +saveres, AND WITHOUT IT THIS GATE MUTES THE USER. VICE writes
                      # command-line options back to vicerc on exit when
                      # SaveResourcesOnExit is set, so `-sounddev dummy`
                      # here would persist and silence every later session.
                      "+saveres", "-basicload", "-sounddev", "dummy",
                      "-8", a.d64, "-autostart", a.d64],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(a.boot)
mon = vice_mon.Mon(check=False)

def rd(n):
    b = mon.mem_get(sym[n], 2)
    return b[0] | (b[1] << 8)

f0, t0, w0 = rd("snd_frames"), rd("snd_ticks"), time.time()
time.sleep(a.window)
f1, t1 = rd("snd_frames"), rd("snd_ticks")
dt = time.time() - w0
mon.resume()
if a.kill:
    v.terminate()
else:
    print("xplus4 still running (pid %d)" % v.pid)

# A STOPPED CLOCK MUST NOT READ AS A PASS. If the title screen never came up,
# or the music never started, both counters sit still -- and 0 is not "slow",
# it is "no measurement". zpprobe.c reported findings off a probe that never
# ran; that is the failure this check exists to prevent.
if f1 == f0:
    print("tempo_p4: the frame counter did not move -- the music never started")
    sys.exit(1)

bad = 0
for name, got, want in (("frames", (f1 - f0) / dt, FRAMES_PAL),
                        ("ticks",  (t1 - t0) / dt, TICKS)):
    err = (got / want - 1.0) * 100.0
    print("  %-7s %7.2f /s   want %7.4f   %+.1f%%" % (name, got, want, err))
    if abs(err) > a.tol:
        bad += 1
print("tempo_p4: %s" % ("PASS" if not bad else "FAIL -- %d rate(s) outside %g%%" % (bad, a.tol)))
sys.exit(1 if bad else 0)
