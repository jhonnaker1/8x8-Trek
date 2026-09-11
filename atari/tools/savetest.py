#!/usr/bin/env python3
"""Save and then restore, against the same disk, in ONE run.

    savetest.py

~~TWO RUNS AND NOT ONE, because a restore needs a cold start and two cold
boots in a single script look exactly like a hang from outside.~~ **That is
what this file used to say and used to do, and it is what made SAVE look
broken**: a restore in a SEPARATE process reads a disk the save never reached,
because Altirra's writes are virtual to the emulated drive. One process, two
cold boots, is the version that works -- see the comment on the second setup().

It still pays for both boots, which is what tools/session.py exists to stop.
This file is kept as the two-boot reference the harness is measured against.

(It also took an argument it never read: `mode = sys.argv[1]`, a leftover of
the two-run interface, which made `savetest.py` with no argument die on an
IndexError rather than run.)
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

DISK = ATARI / "build" / "savetest.atr"
SHOTS = ATARI / "build" / "shots3"


def start_server(log):
    fh = open(log, "w")
    proc = subprocess.Popen(
        [str(SERVER), "--bridge", "--settings=user", "--pacing=unlimited"],
        stdout=subprocess.DEVNULL, stderr=fh)
    deadline = time.time() + 20
    while time.time() < deadline:
        if proc.poll() is not None:
            sys.exit("savetest: server exited -- see %s" % log)
        m = re.search(r"token-file:\s*(\S+)", pathlib.Path(log).read_text())
        if m:
            return proc, m.group(1)
        time.sleep(0.2)
    proc.kill()
    sys.exit("savetest: no token-file line")


def wait_ready(a, sym):
    """Wait for the boot load to finish, rather than guessing a frame count.

    The game streams STRINGS.DAT, MUSIC.DAT and OVERLAYS.BIN into VRAM before
    it draws anything, and how long that takes depends on the file sizes --
    which changed by 40% when the images were packed. Polling far_used until it
    stops moving is both faster than a fixed 4,000 frames and correct when it
    changes again.
    """
    prev, still = -1, 0
    for _ in range(60):
        a.frame(200)
        now = a.peek16(sym["far_used"])
        still = still + 1 if now == prev and now > 40000 else 0
        prev = now
        if still >= 2:
            return now
    return prev


def burst(a, keys, settle=180):
    for k in keys.split(","):
        if k:
            a.key(k)
            a.frame(8)
    a.frame(settle)


def entry():
    out = subprocess.run([sys.executable, str(HERE.parent / "atr.py"), "list",
                          str(DISK)], capture_output=True, text=True).stdout
    for ln in out.splitlines():
        if ln.startswith("EGATREK.SAV"):
            return ln.strip()
    return "EGATREK.SAV is not in the directory"


def symbols():
    nm = subprocess.check_output(
        [str(pathlib.Path.home() / "llvm-mos/bin/llvm-nm"),
         str(ATARI / "build/trekatari.xex.elf")]).decode()
    return {l.split()[-1]: int(l.split()[0], 16)
            for l in nm.splitlines() if len(l.split()) == 3}


def setup(a, sym, restore):
    """Cold boot to the console, or to the restore prompt."""
    a.cold_reset()
    loaded = wait_ready(a, sym)
    burst(a, "RETURN")
    burst(a, "N,RETURN")                              # no briefing
    if restore:
        burst(a, "Y,RETURN", 300)
        return loaded
    burst(a, "N,RETURN")
    burst(a, "J,A,M,I,E,RETURN")
    burst(a, "1,RETURN")
    burst(a, "T,R,E,K,RETURN")
    return loaded


def main():
    SHOTS.mkdir(parents=True, exist_ok=True)
    shutil.copy(ATARI / "build" / "egatrek.atr", DISK)
    print("savetest: fresh disk -- %s" % entry())
    sym = symbols()

    proc, token = start_server(str(ATARI / "build" / "bridge.log"))
    try:
        with AltirraBridge.from_token_file(token) as a:
            a.mount(0, str(DISK))

            loaded = setup(a, sym, restore=False)
            print("savetest: booted, %d bytes in far memory" % loaded, flush=True)
            burst(a, "M,6,COMMA,2,COMMA,3,COMMA,5,RETURN", 300)
            a.screenshot(path=str(SHOTS / "1-before.png"))
            burst(a, "S,A,V,E,RETURN", 240)
            burst(a, "RETURN", 600)
            a.screenshot(path=str(SHOTS / "2-saved.png"))
            print("savetest: saved -- transfer $%02X close $%02X open_live %d"
                  % (a.peek(sym["plat_dbg_status"], 1)[0],
                     a.peek(sym["plat_dbg_close"], 1)[0],
                     a.peek(sym["open_live"], 1)[0]), flush=True)

            # THE SAME SESSION, because Altirra's default disk write mode is
            # VIRTUAL read-write: the emulated drive accepts every write and
            # the host .ATR never sees them. Ejecting proved it -- the save
            # file vanished entirely. So a restore run in a SEPARATE process
            # was reading a disk the save had never reached, and the "hang"
            # was the game asking for a filename it could not find. The rig
            # was blinding the measurement.
            setup(a, sym, restore=True)
            a.screenshot(path=str(SHOTS / "3-restore-prompt.png"))
            burst(a, "RETURN", 900)
            a.screenshot(path=str(SHOTS / "4-restored.png"))
            print("savetest: restored, PC %s" % a.regs()["PC"], flush=True)
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    print("savetest: directory entry -- %s" % entry())


main()
