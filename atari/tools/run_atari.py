#!/usr/bin/env python3
"""Boot an XEX on AltirraSDL's headless bridge and take a screenshot.

THE RIG THIS PORT HAS THAT THE MEGA65 DID NOT: a scriptable emulator that
existed before the port did. AltirraBridgeServer is the lean headless build
-- no window, no fonts, no input layer -- speaking the same JSON-over-socket
protocol as `AltirraSDL --bridge`.

VBXE COMES FROM THE USER'S ALTIRRA SETTINGS, NOT FROM A SWITCH. The server
has --machine, --memory and --no-basic but nothing to add a hardware device,
so `--settings=user` is what brings in the Video Board XE that ~/.config/
altirra/settings.ini already carries. Without it the program runs, writes to
$D640, nothing answers, and the screen stays as the OS left it -- which
reads as a driver bug rather than a missing device. The check below says so
out loud instead.

    python3 tools/run_atari.py build/smoke.xex build/smoke.png [frames]
"""
import os
import pathlib
import re
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve()
ATARI = HERE.parents[1]
SERVER = pathlib.Path.home() / "AltirraBridge-nightly-macos-arm64" / "AltirraBridgeServer"
SDK = SERVER.parent / "sdk" / "python"
SETTINGS = pathlib.Path.home() / ".config" / "altirra" / "settings.ini"

sys.path.insert(0, str(SDK))
from altirra_bridge import AltirraBridge      # noqa: E402


def check_vbxe_configured():
    """A device list without vbxe means the whole run measures nothing."""
    if not SETTINGS.exists():
        sys.exit("run_atari: no %s -- configure VBXE in AltirraSDL first" % SETTINGS)
    if "vbxe" not in SETTINGS.read_text():
        sys.exit("run_atari: settings.ini has no vbxe device. Add the Video "
                 "Board XE in AltirraSDL (System > Devices) and save.")


def start_server(log):
    """STDERR GOES TO A FILE, NOT A PIPE, and that is not a style choice.
       Reading the pipe only until the token line appears leaves the server
       writing into a pipe nobody drains; it fills, the server blocks on
       write, and the whole run hangs with the emulator apparently alive.
       Cost one run to find."""
    fh = open(log, "w")
    proc = subprocess.Popen(
        [str(SERVER), "--bridge", "--settings=user", "--pacing=unlimited"],
        stdout=subprocess.DEVNULL, stderr=fh)
    deadline = time.time() + 20
    while time.time() < deadline:
        if proc.poll() is not None:
            sys.exit("run_atari: server exited before it listened -- see %s" % log)
        m = re.search(r"token-file:\s*(\S+)", pathlib.Path(log).read_text())
        if m:
            return proc, m.group(1)
        time.sleep(0.2)
    proc.kill()
    sys.exit("run_atari: no token-file line from the server -- see %s" % log)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    xex = str((ATARI / sys.argv[1]).resolve() if not os.path.isabs(sys.argv[1])
              else sys.argv[1])
    out = ATARI / sys.argv[2]
    frames = int(sys.argv[3]) if len(sys.argv) > 3 else 180

    check_vbxe_configured()
    log = str(ATARI / "build" / "bridge.log")
    (ATARI / "build").mkdir(parents=True, exist_ok=True)
    proc, token = start_server(log)
    try:
        with AltirraBridge.from_token_file(token) as a:
            a.boot(xex)
            a.frame(frames)
            regs = a.regs()
            out.parent.mkdir(parents=True, exist_ok=True)
            # PATH MODE, NOT INLINE. The inline base64 screenshot never
            # returns against this headless build -- the command reaches the
            # server (it is in the bridge log) and the SDK blocks on the
            # reply for ever. Writing the PNG server-side comes back at once,
            # and the server is on this machine anyway.
            a.screenshot(path=str(out))
            print("run_atari: PC=%s A=%s X=%s Y=%s" %
                  (regs.get("PC"), regs.get("A"), regs.get("X"), regs.get("Y")))
            print("run_atari: %s" % out)
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


main()
