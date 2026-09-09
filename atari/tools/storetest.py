#!/usr/bin/env python3
"""Does the disk seam agree with DOS? Build a disk, boot it, check both ways.

    1. copy a DOS 2.5 image, strip the utilities to make room
    2. plant the files src/storetest.c reads, and the test itself as
       AUTORUN.SYS -- which is the only way to have D: exist at all, since
       booting a bare XEX leaves the machine with no DOS
    3. boot it, screenshot the verdicts
    4. extract the file the program WROTE and check it on the host

Step 4 is the half that matters most: tools/atr.py writing sector chains and
then reading back its own is self-consistency, not evidence. DOS reading what
atr.py wrote, and atr.py reading what DOS wrote, are two independent checks
that only pass together if the format is right.
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

sys.path.insert(0, str(HERE.parent))
DISK = ATARI / "build" / "storetest.atr"
OUT_BYTES = 700
BIG_BYTES = 3000


def want(j):
    return (j * 7 + 11) & 0xFF


def atr(*args):
    subprocess.run([sys.executable, str(HERE.parent / "atr.py")] + [str(a) for a in args],
                   check=True, capture_output=True)


def build_disk(base):
    shutil.copy(base, DISK)
    # ROOM. The stock 2.5 disk keeps 436 sectors free and the utilities are
    # not wanted here; DOS.SYS is, because it is what boots.
    for junk in ("DUP.SYS", "RAMDISK.COM", "SETUP.COM", "COPY32.COM", "DISKFIX.COM"):
        try:
            atr("delete", DISK, junk)
        except subprocess.CalledProcessError:
            pass
    tmp = ATARI / "build" / "_plant.bin"
    tmp.write_bytes(bytes(want(i) for i in range(BIG_BYTES)))
    atr("add", DISK, "TREKDATA.BIN", tmp)
    tmp.write_bytes(b"THE QUICK BROWN FOX")
    atr("add", DISK, "SHORT.TXT", tmp)
    atr("add", DISK, "AUTORUN.SYS", ATARI / "build" / "storetest.xex")
    tmp.unlink()


def start_server(log):
    fh = open(log, "w")
    proc = subprocess.Popen(
        [str(SERVER), "--bridge", "--settings=user", "--pacing=unlimited"],
        stdout=subprocess.DEVNULL, stderr=fh)
    deadline = time.time() + 20
    while time.time() < deadline:
        if proc.poll() is not None:
            sys.exit("storetest: server exited -- see %s" % log)
        m = re.search(r"token-file:\s*(\S+)", pathlib.Path(log).read_text())
        if m:
            return proc, m.group(1)
        time.sleep(0.2)
    proc.kill()
    sys.exit("storetest: no token-file line")


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    build_disk(sys.argv[1])

    proc, token = start_server(str(ATARI / "build" / "bridge.log"))
    try:
        with AltirraBridge.from_token_file(token) as a:
            a.mount(0, str(DISK))
            a.cold_reset()
            a.frame(700)              # DOS boots, then loads AUTORUN.SYS
            a.screenshot(path=str(ATARI / "build" / "storetest.png"))
            print("storetest: regs %s" % a.regs()["PC"])
            print("storetest: build/storetest.png")
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    # THE HOST HALF. The program wrote WROTE.DAT from a rule, not from a blob,
    # so nothing had to be shipped to compare against.
    # BOTH FILES, and the directory FLAGS, because the fault being chased is
    # a file that was written and never closed -- which reads as a perfectly
    # good file until DOS is asked for it again.
    subprocess.run([sys.executable, str(HERE.parent / "atr.py"), "list", str(DISK)])

    out = ATARI / "build" / "_wrote.bin"
    try:
        atr("extract", DISK, "WROTE.DAT", out)
    except subprocess.CalledProcessError:
        print("HOST CHECK: WROTE.DAT is not on the disk            FAIL")
        sys.exit(1)
    got = out.read_bytes()
    out.unlink()
    exp = bytes(want(i) for i in range(OUT_BYTES))
    if got == exp:
        print("HOST CHECK: WROTE.DAT is %d bytes and every one correct  pass"
              % len(got))
    else:
        bad = sum(1 for x, y in zip(got, exp) if x != y)
        print("HOST CHECK: WROTE.DAT is %d bytes (want %d), %d wrong    FAIL"
              % (len(got), OUT_BYTES, bad))
        sys.exit(1)


main()
