#!/usr/bin/env python3
"""Does SIO read and write sectors with no DOS involved?

The first step in dropping Atari DOS from this port. SIO is the layer under
CIO: fill the Device Control Block at $0300, JSR $E459, and the drive answers.
No `D:` handler, no DOS.SYS.

Sector 1 of the game disk is DOS's own boot record and begins 00 03 00 07 --
a known answer, which is what makes this a test. The write half uses sector
800: inside an enhanced-density image and outside anything the game or DOS
maps, so a stray write cannot damage the disk under test. It is a scratch
copy either way.

    make run-siotest
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


def sym(name):
    nm = subprocess.check_output(
        [str(pathlib.Path.home() / "llvm-mos/bin/llvm-nm"), "--print-size",
         str(ATARI / "build" / "siotest.xex.elf")]).decode()
    for ln in nm.splitlines():
        f = ln.split()
        if len(f) >= 3 and f[-1] == name:
            return int(f[0], 16)
    sys.exit("probe_sio: no %s in siotest.xex.elf" % name)


def main():
    disk = ATARI / "build" / "sio.atr"
    shutil.copy(ATARI / "build" / "egatrek.atr", disk)
    log = ATARI / "build" / "bridge.log"
    fh = open(log, "w")
    p = subprocess.Popen([str(SERVER), "--bridge", "--settings=user",
                          "--pacing=unlimited"],
                         stdout=subprocess.DEVNULL, stderr=fh)
    token = None
    deadline = time.time() + 20
    while time.time() < deadline:
        m = re.search(r"token-file:\s*(\S+)", log.read_text())
        if m:
            token = m.group(1)
            break
        time.sleep(0.2)
    if not token:
        p.kill(); sys.exit("probe_sio: no token-file")
    try:
        with AltirraBridge.from_token_file(token) as a:
            a._sock.settimeout(120)
            a.mount(0, str(disk))
            a.boot(str(ATARI / "build" / "siotest.xex"))
            for _ in range(40):
                a.frame(60)
                if a.peek(sym("sio_done"), 1)[0]:
                    break
            else:
                sys.exit("probe_sio: siotest never finished")

            st = a.peek(sym("sio_status"), 1)[0]
            got = list(a.peek(sym("sio_bytes"), 8))
            wst = a.peek(sym("sio_wstatus"), 1)[0]
            rb = list(a.peek(sym("sio_rb"), 8))

            want = [0x00, 0x03, 0x00, 0x07]
            print("  read  sector 1 : status $%02X  %s" % (st, " ".join("%02x" % b for b in got)))
            print("        expected : ... %s (DOS boot record)" % " ".join("%02x" % b for b in want))
            ok_r = st == 1 and got[:4] == want
            print("        -> %s" % ("READ WORKS with no DOS" if ok_r else "READ FAILED"))

            exp = [(0xA5 ^ i) & 0xFF for i in range(8)]
            ok_w = wst == 1 and rb == exp
            print("  write sector 800: status $%02X  read back %s"
                  % (wst, " ".join("%02x" % b for b in rb)))
            print("        expected  : %s" % " ".join("%02x" % b for b in exp))
            print("        -> %s" % ("WRITE WORKS with no DOS" if ok_w else "WRITE FAILED"))
    finally:
        p.terminate()
        try:
            p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            p.kill()


main()
