#!/usr/bin/env python3
"""Measure the Atari keyboard: which CH code does each key produce?

CH ($02FC) holds a MATRIX POSITION, not a character. Rather than copy a table
out of a manual, this injects every key EGA Trek's input seam needs and reads
back what the OS actually put there. See src/keyprobe.c.

Prints a C table ready to paste, and names anything that came back empty --
a key the emulator does not know is a key the driver would silently lose.
"""
import pathlib
import re
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve()
ATARI = HERE.parents[1]
SERVER = pathlib.Path.home() / "AltirraBridge-nightly-macos-arm64" / "AltirraBridgeServer"
sys.path.insert(0, str(SERVER.parent / "sdk" / "python"))
from altirra_bridge import AltirraBridge      # noqa: E402

BUF = 0x0600

# Every key c128/src/input.h names, by the bridge's own key identifier. The
# ASCII column is what kb_waitkey must return -- KB_* in that header.
KEYS = (
    [(c, ord(c)) for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ"]
    + [(c, ord(c)) for c in "0123456789"]
    + [("SPACE", 32), ("RETURN", 13), ("ESC", 27),
       ("COMMA", 44), ("PERIOD", 46), ("SLASH", 47), ("MINUS", 45),
       ("EQUALS", 61), ("SEMICOLON", 59), ("PLUS", 43), ("ASTERISK", 42),
       ("COLON", 58), ("AT", 64), ("BACKSPACE", 20),
       ("UP", 1), ("DOWN", 2)]
)


def start_server(log):
    fh = open(log, "w")
    proc = subprocess.Popen(
        [str(SERVER), "--bridge", "--settings=user", "--pacing=unlimited"],
        stdout=subprocess.DEVNULL, stderr=fh)
    deadline = time.time() + 20
    while time.time() < deadline:
        if proc.poll() is not None:
            sys.exit("keytable: server exited -- see %s" % log)
        m = re.search(r"token-file:\s*(\S+)", pathlib.Path(log).read_text())
        if m:
            return proc, m.group(1)
        time.sleep(0.2)
    proc.kill()
    sys.exit("keytable: no token-file line")


def main():
    (ATARI / "build").mkdir(parents=True, exist_ok=True)
    proc, token = start_server(str(ATARI / "build" / "bridge.log"))
    found, missing = [], []
    try:
        with AltirraBridge.from_token_file(token) as a:
            a.boot(str(ATARI / "build" / "keyprobe.xex"))
            a.frame(180)
            for name, ascii_val in KEYS:
                before = a.peek(BUF, 1)[0]
                try:
                    a.key(name)
                except Exception as exc:              # unknown key identifier
                    missing.append((name, "rejected: %s" % exc))
                    continue
                a.frame(6)
                after = a.peek(BUF, 1)[0]
                if after == before:
                    missing.append((name, "no code recorded"))
                    continue
                code = a.peek(BUF + after, 1)[0]
                found.append((name, ascii_val, code))
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    print("/* MEASURED, not copied: tools/keytable.py, %d keys. */" % len(found))
    print("static const unsigned char keymap[] = {")
    for name, ascii_val, code in found:
        print("    0x%02X, %3d,   /* %-10s */" % (code, ascii_val, name))
    print("};")
    if missing:
        print("\nNOT MEASURED -- the driver would lose these:")
        for name, why in missing:
            print("    %-12s %s" % (name, why))


main()
