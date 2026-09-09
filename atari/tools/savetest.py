#!/usr/bin/env python3
"""Save in one run, restore in the next, against the same disk.

TWO RUNS AND NOT ONE, because a restore needs a cold start and two cold boots
in a single script look exactly like a hang from outside -- which is what they
were mistaken for once. Each half is one boot.

    savetest.py save      -- fresh disk, play a little, SAVE, report the entry
    savetest.py restore   -- boot the same disk and restore it
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


def main():
    mode = sys.argv[1]
    SHOTS.mkdir(parents=True, exist_ok=True)
    if mode == "save":
        shutil.copy(ATARI / "build" / "egatrek.atr", DISK)
        print("savetest: fresh disk -- %s" % entry())

    proc, token = start_server(str(ATARI / "build" / "bridge.log"))
    try:
        with AltirraBridge.from_token_file(token) as a:
            a.mount(0, str(DISK))
            a.cold_reset()
            a.frame(4000)
            burst(a, "RETURN")
            burst(a, "N,RETURN")                      # no briefing
            if mode == "save":
                burst(a, "N,RETURN")                  # do not restore
                burst(a, "J,A,M,I,E,RETURN")
                burst(a, "1,RETURN")
                burst(a, "T,R,E,K,RETURN")
                burst(a, "M,6,COMMA,2,COMMA,3,COMMA,5,RETURN", 300)
                a.screenshot(path=str(SHOTS / "1-before.png"))
                burst(a, "S,A,V,E,RETURN", 240)
                a.screenshot(path=str(SHOTS / "2-dialog.png"))
                burst(a, "RETURN", 900)               # default name, let DOS finish
                a.screenshot(path=str(SHOTS / "3-saved.png"))
                # THE DRIVER'S OWN VERDICT. plat_write_all reports STOR_OK on
                # the TRANSFER's status and throws the CLOSE's away -- and on a
                # write the close is where the data actually lands. If these
                # disagree with the directory entry, that is the gap.
                print("savetest: plat_dbg_status $%02X, open_live %d"
                      % (a.peek(0xAAB2, 1)[0], a.peek(0xAAB1, 1)[0]))
            else:
                burst(a, "Y,RETURN", 300)             # yes, restore
                burst(a, "RETURN", 900)               # default name
                a.screenshot(path=str(SHOTS / "4-restored.png"))
            print("savetest: %s done, PC %s" % (mode, a.regs()["PC"]))
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    print("savetest: directory entry -- %s" % entry())


main()
