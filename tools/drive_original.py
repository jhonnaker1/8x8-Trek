#!/usr/bin/env python3
"""Drive the original EGA Trek through dosbox-automation's REST API.

See NOTES.md for building and launching the emulator. Point
DOSBOX_API_TOKEN at the same token it was given.

Two traps this wraps up, both learned the hard way:

  * /input/type types characters but does NOT submit. Enter has to go through
    /input/sequence as an explicit KBD_enter press and release, which is why
    `line()` exists rather than just appending a carriage return.
  * /video/text is useless here. EGA Trek runs in BIOS mode 16 (640x350 EGA
    graphics), so the screen has no character buffer to read -- every reading
    comes from a captured frame.

Example, from a fresh launch to a level-3 game:

    python3 tools/drive_original.py line:  line:n line:n line:3 line:3 \\
                                   line:abc shot:console
"""
import json
import os
import sys
import time
import urllib.request

BASE = os.environ.get("DOSBOX_API", "http://localhost:8386/api/v1")
TOKEN = os.environ.get("DOSBOX_API_TOKEN", "")
OUT = os.environ.get("DOSBOX_SHOT_DIR", ".")


def call(path, data=None, raw=False):
    req = urllib.request.Request(
        BASE + path,
        data=json.dumps(data).encode() if data is not None else None,
        headers={"Authorization": "Bearer " + TOKEN,
                 "Content-Type": "application/json"},
        method="POST" if data is not None else "GET")
    with urllib.request.urlopen(req, timeout=30) as r:
        body = r.read()
    return body if raw else json.loads(body or b"{}")


def type_(text, pause=0.4):
    call("/input/type", {"text": text, "cps": 20})
    time.sleep(pause)


def enter(pause=2.0):
    call("/input/sequence", {"events": [
        {"t": 0, "type": "key", "key": "KBD_enter", "pressed": True},
        {"t": 60, "type": "key", "key": "KBD_enter", "pressed": False}]})
    time.sleep(pause)


def line(text="", pause=2.0):
    """Type text, then submit it. The submit is the part /input/type omits."""
    if text:
        type_(text)
    enter(pause)


def shot(name):
    path = os.path.join(OUT, name + ".png")
    with open(path, "wb") as f:
        f.write(call("/video/frame?format=png", raw=True))
    return path


def main():
    if not TOKEN:
        sys.exit("set DOSBOX_API_TOKEN to the emulator's 64-hex-char token")
    for arg in sys.argv[1:]:
        if arg.startswith("line:"):
            line(arg[5:])
        elif arg.startswith("type:"):
            type_(arg[5:])
        elif arg.startswith("shot:"):
            print(shot(arg[5:]))
        elif arg == "enter":
            enter()
        elif arg.startswith("wait:"):
            time.sleep(float(arg[5:]))
        else:
            sys.exit("unknown action: %s" % arg)


if __name__ == "__main__":
    main()
