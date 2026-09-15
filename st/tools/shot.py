#!/usr/bin/env python3
"""Boot the ST build in Hatari, photograph it, AND LEAVE IT RUNNING.

NEVER SIGTERM HATARI. It catches the signal, tries to shut down cleanly, and
on this rig it hangs instead -- the emulator stays up with no window and the
next run cannot bind its fifo. The supported way out is `hatari-shortcut quit`
down the command fifo, and `kill -9` only if that does not take. That is a
standing rule of this project, learned the hard way on the Falcon.

AND IT LEAVES THE MACHINE RUNNING BY DEFAULT, which is Jamie's correction on
the first tool that got a machine somewhere interesting and then killed it:
"your script kills it too fast." --kill is for automation.

Hatari's fifo takes one command per line:
    hatari-shortcut screenshot      write a PNG into --screenshot-dir
    hatari-shortcut quit            shut down cleanly
    hatari-event keypress <name>    one key, by SDL name
"""
import argparse, os, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
ST = os.path.dirname(HERE)

# EMUTOS, JAMIE'S CALL 2026-09-14. It is also what the Falcon port runs and
# what this project can redistribute a pointer to -- a real TOS ROM is Atari's.
DEFAULT_TOS = os.path.expanduser("~/hatari/rom/etos512us.img")


# HATARI SAYS WHAT IT TAKES, IN ITS OWN ERROR TEXT: "<key> can be either a
# single ASCII character or an ST scancode (e.g. space has scancode of 57 and
# enter 28)." I sent `keypress Return` and `text n` instead -- one a multi-
# character name it cannot parse and one a command that does not exist -- and
# then read the SCREENSHOTS to decide it had not worked. Hatari had been
# printing "unrecognized event" to stderr the whole time, into a pipe I was
# discarding to /dev/null. THE EMULATOR WAS ANSWERING AND I WAS NOT LISTENING.
NAMED = {"RETURN": "28", "ENTER": "28", "SPACE": "57", "ESC": "1",
         "UP": "72", "DOWN": "80", "LEFT": "75", "RIGHT": "77",
         "BACKSPACE": "14", "DELETE": "83"}


def scancode(k):
    """One key for `hatari-event keypress`. A single character goes through
       as itself; anything longer must be a name we know, because Hatari
       silently reads an unparseable name as scancode 0."""
    if len(k) == 1:
        return k
    u = k.upper()
    if u not in NAMED:
        sys.exit("shot: %r is not a single character and not one of %s"
                 % (k, ", ".join(sorted(NAMED))))
    return NAMED[u]


def send(fifo, line, timeout=10.0):
    """One command down the fifo, WITHOUT EVER BLOCKING FOREVER.

    Opening a fifo for writing blocks until a reader has it open, so the first
    version of this hung the whole session: it pre-created the fifo with
    mkfifo, Hatari then found the path already there, and nothing ever opened
    the read end. **Hatari CREATES the fifo itself** -- `--cmd-fifo <file>`
    says so -- so the path must not exist beforehand, and the write has to be
    non-blocking with a deadline rather than a plain open().

    O_WRONLY|O_NONBLOCK raises ENXIO while there is no reader, which is the
    signal to wait rather than an error.
    """
    t0 = time.time()
    while time.time() - t0 < timeout:
        if not os.path.exists(fifo):
            time.sleep(0.25)
            continue
        try:
            fd = os.open(fifo, os.O_WRONLY | os.O_NONBLOCK)
        except OSError:
            time.sleep(0.25)
            continue
        try:
            os.write(fd, (line + "\n").encode())
        finally:
            os.close(fd)
        time.sleep(0.4)
        return True
    print("  fifo: no reader on %s after %.0fs -- Hatari did not open it"
          % (fifo, timeout))
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--machine", default="st", choices=["st", "ste"])
    ap.add_argument("--tos", default=DEFAULT_TOS)
    ap.add_argument("--wait", type=float, default=12.0)
    ap.add_argument("--keys", default="",
                    help="comma-separated keys: one ASCII character each, or "
                         "RETURN/SPACE/UP/DOWN for the named ones")
    ap.add_argument("--settle", type=float, default=2.5,
                    help="seconds after each key before the next")
    ap.add_argument("--out", default=os.path.join(ST, "build", "shots"))
    ap.add_argument("--tail", type=float, default=0.0,
                    help="extra seconds after the last key, then one more shot "
                         "-- a screen that is still being drawn photographs as "
                         "a screen that is missing things")
    ap.add_argument("--kill", action="store_true")
    a = ap.parse_args()

    prg = os.path.join(ST, "build", "EGATREK.PRG")
    if not os.path.exists(prg):
        sys.exit("shot: no %s -- run `make` first" % prg)
    if not os.path.exists(a.tos):
        sys.exit("shot: no TOS image at %s" % a.tos)
    os.makedirs(a.out, exist_ok=True)

    # HATARI MAKES THE FIFO. The path must NOT exist when it starts.
    fifo = "/tmp/hatari-st-%d.fifo" % os.getpid()
    if os.path.exists(fifo):
        os.unlink(fifo)

    # THE PATH IS QUOTED IN THE SHELL AND A LIST HERE, so the backslash
    # survives. `--auto C:\EGATREK.PRG` unquoted has it eaten and Hatari is
    # handed C:EGATREK.PRG, which it cannot open.
    # --memsize 1 IS A 1040ST, and Hatari's default is FOURTEEN MEGABYTES --
    # a machine no ST ever was, whose TOS memory test was still running
    # sixteen seconds in and filled the first screenshot. 1MB is the realistic
    # target and the port wants about 110K of it.
    #
    # --fast-boot patches out that test. --confirm-quit off is not a
    # convenience: without it `hatari-shortcut quit` POPS A DIALOG AND WAITS,
    # so an automated shutdown leaves a window on the user's screen asking
    # permission to close. Jamie saw exactly that and asked why it was asking
    # to quit; a tool that cleans up after itself must not need an answer.
    cmd = ["hatari", "--machine", a.machine, "--tos", a.tos, "--vdi", "off",
           "--memsize", "1", "--fast-boot", "on", "--confirm-quit", "off",
           "--gemdos-drive", "C", "-d", os.path.join(ST, "build"),
           "--screenshot-dir", a.out, "--cmd-fifo", fifo,
           "--sound", "off", "--auto", "C:\\EGATREK.PRG"]
    print("  " + " ".join(cmd))
    # HATARI'S OUTPUT IS KEPT, not discarded. Every fifo command it cannot
    # parse is reported here and nowhere else, and throwing it away cost a
    # whole debugging round: twelve identical screenshots read as "the port
    # ignores the keyboard" when the emulator had said "unrecognized event"
    # twelve times.
    logpath = os.path.join(a.out, "hatari.log")
    log = open(logpath, "w")
    h = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)

    try:
        time.sleep(a.wait)
        send(fifo, "hatari-shortcut screenshot")
        n = 1
        for k in [k for k in a.keys.split(",") if k]:
            send(fifo, "hatari-event keypress %s" % scancode(k))
            time.sleep(a.settle)
            send(fifo, "hatari-shortcut screenshot")
            n += 1
        if a.tail:
            time.sleep(a.tail)
            send(fifo, "hatari-shortcut screenshot")
            n += 1
        time.sleep(1.0)
        log.flush()
        errs = [l.rstrip() for l in open(logpath)
                if "ERROR" in l or "unrecognized" in l]
        if errs:
            print("  HATARI REPORTED %d ERROR(S) -- read these before the "
                  "pictures:" % len(errs))
            for e in errs[:6]:
                print("     " + e)
        shots = sorted(f for f in os.listdir(a.out) if f.endswith(".png"))
        print("  %d screenshot(s) in %s" % (len(shots), a.out))
        for s in shots[-n:]:
            p = os.path.join(a.out, s)
            print("     %-28s %d bytes" % (s, os.path.getsize(p)))
        if not shots:
            print("  NO SCREENSHOTS. Hatari took the fifo but wrote nothing --")
            print("  suspect the fifo command name or --screenshot-dir, not")
            print("  the port, and check the emulator window before concluding.")
            return 1
    finally:
        if a.kill:
            # THE SUPPORTED WAY OUT, then force. Never SIGTERM.
            try:
                send(fifo, "hatari-shortcut quit")
                h.wait(timeout=8)
            except Exception:
                h.send_signal(signal.SIGKILL)
                h.wait()
        else:
            print("  HATARI IS STILL RUNNING (pid %d) -- play it, then close"
                  " the window." % h.pid)
            print("  To stop it from here:  echo 'hatari-shortcut quit' > %s"
                  % fifo)
    return 0


if __name__ == "__main__":
    sys.exit(main())
