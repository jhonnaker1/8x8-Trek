#!/usr/bin/env python3
"""Copy C64 OS's own documentation and SDK out of a C64 OS disk image, readable.

    c64os_ref.py IMAGE.dmg [OUT]      default OUT: reference/c64os/<version>/

Jamie's C64 OS volumes (~/vice-arm64-gtk3-3.10/C64os/C64OS-1.0x-clean.dmg)
hold the whole system tree as host files. This mounts one read-only, copies
the parts that answer questions -- //os/docs, //os/h, //os/s (with its
subfolders), //os/settings, //os/kernal, //os/library, //os/charsets -- and writes a
decoded .txt beside every PC64 text file, so a grep finds "bkalloc" rather
than PETSCII.

THE OUTPUT IS C64 OS, WHICH IS COMMERCIAL. It goes under reference/, which
.gitignore keeps out of the repository; never move it anywhere tracked.

The files are PC64 wrappers (.P00/.S00: a 26-byte header, "C64File"), and the
text is PETSCII in C64 OS's lowercase set: $41-$5A are lowercase letters,
$C1-$DA capitals, $A4 is the underscore.
"""
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
WANT = ("DOCS", "H", "S", "SETTINGS", "KERNAL", "LIBRARY", "CHARSETS")


def decode(b):
    if b[:7] == b"C64File":
        b = b[26:]
    out = []
    for c in b:
        if c == 13:
            out.append("\n")
        elif 0x41 <= c <= 0x5A:
            out.append(chr(c + 32))
        elif 0xC1 <= c <= 0xDA:
            out.append(chr(c - 0x80))
        elif 0x61 <= c <= 0x7A:
            out.append(chr(c - 32))
        elif c == 0xA4:
            out.append("_")
        elif 32 <= c < 127 or c == 9:
            out.append(chr(c))
        else:
            out.append(".")
    return "".join(out)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__.strip().splitlines()[2])
    image = sys.argv[1]
    mnt = tempfile.mkdtemp(prefix="c64os-")
    subprocess.check_call(["hdiutil", "attach", "-readonly", "-nobrowse",
                           "-mountpoint", mnt, image], stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL)
    try:
        ver = "unknown"
        vt = os.path.join(mnt, "os", "SETTINGS", "version.t.S00")
        if os.path.exists(vt):
            ver = decode(open(vt, "rb").read()).strip().splitlines()[0]
        out = sys.argv[2] if len(sys.argv) > 2 else \
            os.path.join(ROOT, "reference", "c64os", ver)
        n = 0
        for top in WANT:
            src = os.path.join(mnt, "os", top)
            if not os.path.isdir(src):
                continue
            for d, _, files in os.walk(src):
                rel = os.path.relpath(d, os.path.join(mnt, "os"))
                dst = os.path.join(out, rel)
                os.makedirs(dst, exist_ok=True)
                for f in files:
                    shutil.copy(os.path.join(d, f), os.path.join(dst, f))
                    if f.endswith(".S00") or ".t." in f or f.endswith(".t"):
                        text = decode(open(os.path.join(d, f), "rb").read())
                        open(os.path.join(dst, f + ".txt"), "w").write(text)
                    n += 1
        print("c64os_ref: C64 OS %s -> %s, %d files" % (ver, out, n))
    finally:
        subprocess.call(["hdiutil", "detach", mnt], stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL)
        os.rmdir(mnt)


if __name__ == "__main__":
    main()
