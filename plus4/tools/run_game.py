#!/usr/bin/env python3
"""Boot the Plus/4 disk in xplus4 and decode the screen. LEAVES IT RUNNING."""
import os, subprocess, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import vice_mon

d64 = sys.argv[1] if len(sys.argv) > 1 else "build/egatrek-plus4.d64"
wait = float(sys.argv[2]) if len(sys.argv) > 2 else 25.0
kill = "--kill" in sys.argv

v = subprocess.Popen(["xplus4", "-binarymonitor",
                      "-binarymonitoraddress", "ip4://127.0.0.1:6502",
                      # -basicload, AND IT IS NOT A PREFERENCE. Autostart
                      # defaults to LOAD"*",8,1 -- an ABSOLUTE load, which does
                      # not relink BASIC's pointers, so RUN walks a program
                      # BASIC does not think it has and answers ?SYNTAX ERROR
                      # IN 10. The line is this port's own BASIC header and it
                      # is byte-perfect; the loader was wrong, not the file.
                      # A player types LOAD"TREK4",8 -- no ,1 -- for the same
                      # reason, and README-release says so.
                      "-basicload",
                      # +saveres: DO NOT WRITE THIS RUN'S OPTIONS BACK TO
                      # vicerc. Jamie's config has SaveResourcesOnExit=1, so
                      # VICE PERSISTS COMMAND-LINE OPTIONS ON EXIT -- which is
                      # how one calibration run with `-sounddev wav` left
                      # SoundDeviceName="wav" in [PLUS4] and silently muted
                      # every later session, until the port was reported as
                      # having no sound while it had been making sound all
                      # along. A rig must not edit the user's settings as a
                      # side effect of measuring something.
                      "+saveres",
                      # SOUND DEVICE, STATED EXPLICITLY, AND IT IS NOT A
                      # PREFERENCE. VICE PERSISTS -sounddev INTO vicerc, so one
                      # calibration run with `-sounddev wav` leaves EVERY later
                      # session writing audio to a file instead of the speakers
                      # -- silently, for every emulated machine, until someone
                      # notices they have not heard anything in weeks. That is
                      # exactly what happened here: SoundDeviceName="wav" was
                      # still in ~/.config/vice/vicerc from the tedsnd.c
                      # calibration, and the Plus/4 port was reported as having
                      # no sound when it had been making sound all along.
                      # A rig that wants a human to HEAR something must say so.
                      "-sounddev", "coreaudio",
                      "-8", d64, "-autostart", d64],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(wait)
mon = vice_mon.Mon()
scr = mon.mem_get(0x0C00, 1000)
col = mon.mem_get(0x0800, 1000)

def ch(c):
    if c < 32:  return chr(64 + c)
    if c < 64:  return chr(c)
    return "#"
live = sum(1 for c in scr if c != 32)
hues = sorted({b & 0x0F for b, c in zip(col, scr) if c != 32})
print("%d non-space cells, hues %s" % (live, hues))
for r in range(25):
    print("%2d|%s|" % (r, "".join(ch(c) for c in scr[r*40:(r+1)*40])))
mon.resume()
if kill:
    v.terminate()
else:
    print("xplus4 still running (pid %d)" % v.pid)
