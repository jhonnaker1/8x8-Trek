#!/usr/bin/env python3
"""Drive the game on a booted disk and screenshot each screen.

    play.py OUTDIR frames-to-boot "KEY,KEY,..." ["KEY,..." ...]

Each argument after the boot frame count is one BURST of keys; a screenshot is
taken after each burst, so a run leaves a numbered sequence of what the player
would have seen. That is the only way this port gets looked at -- nothing here
runs unless a human or this script presses the keys.
"""
import pathlib
import re
import shutil
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve()
ATARI = HERE.parents[1]
SERVER = pathlib.Path.home() / "AltirraBridge-nightly-macos-arm64" / "AltirraBridgeServer"
sys.path.insert(0, str(SERVER.parent / "sdk" / "python"))
from altirra_bridge import AltirraBridge      # noqa: E402


def start_server(log):
    fh = open(log, "w")
    proc = subprocess.Popen(
        [str(SERVER), "--bridge", "--settings=user", "--pacing=unlimited"],
        stdout=subprocess.DEVNULL, stderr=fh)
    deadline = time.time() + 20
    while time.time() < deadline:
        if proc.poll() is not None:
            sys.exit("play: server exited -- see %s" % log)
        m = re.search(r"token-file:\s*(\S+)", pathlib.Path(log).read_text())
        if m:
            return proc, m.group(1)
        time.sleep(0.2)
    proc.kill()
    sys.exit("play: no token-file line")


def main():
    outdir = ATARI / sys.argv[1]
    boot = int(sys.argv[2])
    bursts = sys.argv[3:]
    outdir.mkdir(parents=True, exist_ok=True)

    proc, token = start_server(str(ATARI / "build" / "bridge.log"))
    try:
        with AltirraBridge.from_token_file(token) as a:
            # A COPY, NEVER THE MASTER. The emulator mounts an ATR
            # read-write, so a run that saves a game writes back into the
            # image -- and a later "fresh disk" then is not one. That cost
            # a round of diagnosing a save-file flag that had been left by
            # the PREVIOUS run.
            scratch = ATARI / "build" / "play.atr"
            shutil.copy(ATARI / "build" / "egatrek.atr", scratch)
            a.mount(0, str(scratch))
            a.cold_reset()
            a.frame(boot)
            a.screenshot(path=str(outdir / "00-title.png"))
            print("play: 00-title  PC %s" % a.regs()["PC"])
            for i, burst in enumerate(bursts, 1):
                for k in [k for k in burst.split(",") if k]:
                    a.key(k)
                    a.frame(8)
                a.frame(180)          # let a load or a redraw finish
                name = "%02d-%s.png" % (i, burst.replace(",", "")[:16] or "keys")
                a.screenshot(path=str(outdir / name))
                print("play: %-22s PC %s" % (name, a.regs()["PC"]))
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


main()
