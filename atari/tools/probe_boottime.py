#!/usr/bin/env python3
"""How long does the boot load actually take -- and on a REAL drive?

Open list item 5: "packing OVERLAYS.BIN cut it by 40% and nobody has held a
stopwatch to what is left". The catch is that every boot this project has timed
ran with **Altirra's SIO patch on** (`siopatch = 'on'`, `accuratedisk = False`),
which replaces the serial protocol with an instant transfer. That is not a
1050; it is not any drive. So the figure everyone has been living with is not
slow, it is fictional.

This times both:

    patched     what the harness has been doing all day
    accurate    siopatch off, accuratedisk on -- real serial timing

and converts frames to seconds using the region the machine reports, rather
than assuming 60Hz.

    make run-probe-boottime
"""
import pathlib
import re
import subprocess
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from session import ATARI, SERVER, symbols          # noqa: E402
from altirra_bridge import AltirraBridge            # noqa: E402


def start(log):
    fh = open(log, "w")
    p = subprocess.Popen([str(SERVER), "--bridge", "--settings=user",
                          "--pacing=unlimited"],
                         stdout=subprocess.DEVNULL, stderr=fh)
    deadline = time.time() + 20
    while time.time() < deadline:
        if p.poll() is not None:
            sys.exit("boottime: server exited")
        m = re.search(r"token-file:\s*(\S+)", pathlib.Path(log).read_text())
        if m:
            return p, m.group(1)
        time.sleep(0.2)
    p.kill(); sys.exit("boottime: no token-file")


def time_boot(a, sym, label):
    a.cold_reset()
    a.frame(2)
    a.poke16(sym["far_used"], 0)          # same sentinel rule as session.boot
    if a.peek16(sym["far_used"]) != 0:
        sys.exit("boottime: the zero did not take")
    frames, prev, still = 0, -1, 0
    while frames < 40000:
        a.frame(20); frames += 20
        now = a.peek16(sym["far_used"])
        still = still + 1 if now == prev and now > 40000 else 0
        prev = now
        if still >= 3:
            break
    # $D014 is $01 on PAL and $0F on NTSC -- measured by this port, see
    # src/atarisnd.c. The frame rates are the ones its sound driver uses.
    pal = (a.peek(0xD014, 1)[0] & 0x0F) == 0x01
    fps = 49.86 if pal else 59.92
    print("  %-10s %5d frames  %6.2f s   (%s, %.2f fps, far_used %d)"
          % (label, frames, frames / fps, "PAL" if pal else "NTSC", fps, prev))
    return frames / fps


def main():
    sym = symbols()
    disk = ATARI / "build" / "boottime.atr"
    import shutil
    shutil.copy(ATARI / "build" / "egatrek.atr", disk)
    p, token = start(str(ATARI / "build" / "bridge.log"))
    try:
        with AltirraBridge.from_token_file(token) as a:
            a._sock.settimeout(180)
            a.mount(0, str(disk))
            print("boottime: %d bytes of OVERLAYS.BIN + STRINGS.DAT + MUSIC.DAT"
                  % (ATARI / "build" / "data" / "OVERLAYS.BIN").stat().st_size,
                  flush=True)

            patched = time_boot(a, sym, "patched")

            # THE REAL DRIVE. siopatch off puts the serial protocol back;
            # accuratedisk on puts the rotational and seek timing back too.
            a.config("siopatch", "off")
            a.config("accuratedisk", "true")
            print("  (siopatch=%s accuratedisk=%s)"
                  % (a.config().get("siopatch"), a.config().get("accuratedisk")),
                  flush=True)
            accurate = time_boot(a, sym, "accurate")

            print()
            print("boottime: a real drive is %.1fx slower -- %.1fs against %.1fs"
                  % (accurate / patched if patched else 0, accurate, patched))
            print("boottime: THE PATCHED FIGURE IS THE FICTIONAL ONE. Every "
                  "boot this project has timed used it.")
    finally:
        p.terminate()
        try:
            p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            p.kill()


main()
