#!/usr/bin/env python3
"""Run a Plus/4 PRG under VICE's xplus4 and read a probe's report.

Same binary monitor the C64 and C128 ports use -- tools/vice_mon.py -- pointed
at a different emulator. LEAVES VICE RUNNING unless --kill, which is the rule
every rig in this project follows.
"""
import argparse, os, subprocess, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import vice_mon

ap = argparse.ArgumentParser()
ap.add_argument("prg")
ap.add_argument("--addr", default="0x0500")
ap.add_argument("--len", type=int, default=16)
ap.add_argument("--wait", type=float, default=8.0)
ap.add_argument("--kill", action="store_true")
a = ap.parse_args()

vice = subprocess.Popen(
    ["xplus4", "-binarymonitor",
     "-binarymonitoraddress", "ip4://127.0.0.1:6502",
     # DO NOT PERSIST THIS RUN'S OPTIONS -- see run_game.py.
     "+saveres",
     "-autostart", a.prg],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(a.wait)

mon = vice_mon.Mon()
data = mon.mem_get(int(a.addr, 0), a.len)
print("$%04X: %s" % (int(a.addr, 0), " ".join("%02X" % b for b in data)))
mon.resume()
if a.kill:
    vice.terminate()
else:
    print("xplus4 still running (pid %d)" % vice.pid)
