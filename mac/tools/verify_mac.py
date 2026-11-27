#!/usr/bin/env python3
"""What the compiler, the linker and the test suite structurally cannot see.

THE GLYPH SWEEP IS THE FALCON'S CHECKER, IMPORTED -- as st/tools/verify_st.py
does it, and for the same reason: a copy would pass for months after the
original grew a rule. This port draws the Falcon's 8x16 box glyphs, so a
screen code the shared UI can put on screen with no box[] entry and no ASCII
mapping is the missing-glyph marker here exactly as there.

WHAT IS THIS PORT'S OWN, and each one fails silently otherwise:

  * GEOMETRY. The console is a WINDOW: 80x25 of 8x16 = 640x400, below a 20-line
    menu bar and a 19-line title bar, on a screen that must be 640x480. Read
    out of macvid.c's own #defines -- an assertion written in terms of the
    constant it tests cannot fail.
  * THE DATA IS INSIDE THE PROGRAM. STRINGS.DAT, MUSIC.DAT and BRIEF.TXT are
    'EGAd' resources in the MacBinary's resource fork, and each must be
    BYTE-IDENTICAL to the file built beside it. A stale resource is the
    STR_COUNT coupling (NOTES item 60): the binary refuses the pool and plays
    with every label blank, no error anywhere.
  * THE FINDER BUNDLE. Without the hasBundle flag the Finder ignores the icon;
    Rez writes it clear and tools/setbundle.py sets it.
  * THE PARTITION and the code segment, printed: 'Main' is ~72K, past the
    classic 32K limit, and it loads (measured); the SIZE resource is what the
    Finder gives the program, set from a MEMPROBE measurement.
"""
import hashlib
import importlib.util
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(HERE)
FALCON = os.path.join(ROOT, "falcon", "tools", "verify_prg.py")
APP = os.path.join(HERE, "build", "EGATREK.bin")
DATA = ("STRINGS.DAT", "MUSIC.DAT", "BRIEF.TXT")

fail = []


def load_falcon_checker():
    if not os.path.exists(FALCON):
        sys.exit("verify_mac: %s is missing -- the glyph sweep IS that file"
                 % FALCON)
    spec = importlib.util.spec_from_file_location("falcon_verify", FALCON)
    mod = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(mod)
    except SystemExit:
        sys.exit("verify_mac: importing %s EXITED -- it must be guarded with "
                 "`if __name__ == \"__main__\":`" % FALCON)
    return mod


def resources(path):
    """The resource fork of a MacBinary file: {(type, id): (name, bytes)}."""
    d = open(path, "rb").read()
    if len(d) < 128 or d[0] != 0:
        fail.append("%s is not MacBinary" % path)
        return {}, d
    dl, rl = struct.unpack(">II", d[83:91])
    roff = 128 + ((dl + 127) // 128) * 128
    r = d[roff:roff + rl]
    doff, moff, _, mlen = struct.unpack(">IIII", r[:16])
    m = r[moff:moff + mlen]
    tlo, nlo = struct.unpack(">HH", m[24:28])
    out = {}
    for i in range(struct.unpack(">H", m[28:30])[0] + 1):
        t, n, ro = struct.unpack(">4sHH", m[30 + 8 * i:38 + 8 * i])
        for j in range(n + 1):
            e = tlo + ro + 12 * j
            rid, no, a3 = struct.unpack(">hHI", m[e:e + 8])
            off = doff + (a3 & 0xFFFFFF)
            size = struct.unpack(">I", r[off:off + 4])[0]
            name = ""
            if no != 0xFFFF:
                ln = m[nlo + no]
                name = m[nlo + no + 1:nlo + no + 1 + ln].decode("mac_roman")
            out[(t.decode("mac_roman"), rid)] = (name, r[off + 4:off + 4 + size])
    return out, d


def check_geometry(src, consts):
    f = {}
    for name in ("CELL_W", "CELL_H", "MENU_H", "TITLE_H"):
        mm = re.search(r'^#define\s+%s\s+(\d+)' % name, src, re.M)
        if not mm:
            fail.append("macvid.c has no #define %s" % name)
            return
        f[name] = int(mm.group(1))
    cols, rows = consts["VDC_COLS"], consts["VDC_ROWS"]
    w, h = cols * f["CELL_W"], rows * f["CELL_H"]
    need = f["MENU_H"] + f["TITLE_H"] + h
    if w > 640 or need > 480:
        fail.append("the console window is %dx%d and needs %d lines with the "
                    "menu and title bars -- it does not fit 640x480" % (w, h, need))
    print("verify: console %dx%d cells of %dx%d = %dx%d window, %d of 480 "
          "lines with menu and title bars" % (cols, rows, f["CELL_W"],
                                              f["CELL_H"], w, h, need))


def check_app():
    if not os.path.exists(APP):
        fail.append("no build/EGATREK.bin -- run `make game` first")
        return
    res, d = resources(APP)
    if not res:
        return

    by_name = {name: body for (t, _), (name, body) in res.items() if t == "EGAd"}
    for f in DATA:
        built = os.path.join(HERE, "build", f)
        if f not in by_name:
            fail.append("no 'EGAd' resource named %s -- the game would play "
                        "without it" % f)
            continue
        if not os.path.exists(built):
            fail.append("build/%s is missing to compare against" % f)
            continue
        if by_name[f] != open(built, "rb").read():
            fail.append("the %s resource is NOT the file built beside it -- a "
                        "stale resource is a blank-label game" % f)
    print("verify: data resources %s, each byte-identical to build/"
          % ", ".join("%s %s" % (f, "{:,}".format(len(by_name.get(f, b""))))
                      for f in DATA))

    music = by_name.get("MUSIC.DAT", b"")
    print("verify: MUSIC.DAT sha1 %s (tools/make_music.py's own composition)"
          % hashlib.sha1(music).hexdigest()[:16])

    if not d[73] & 0x20:
        fail.append("the MacBinary's hasBundle flag is clear -- the Finder "
                    "will ignore the icon (tools/setbundle.py)")

    main = [body for (t, _), (name, body) in res.items()
            if t == "CODE" and name == "Main"]
    size = res.get(("SIZE", -1))
    if not main:
        fail.append("no CODE 'Main' segment")
    if not size or len(size[1]) != 10:
        fail.append("no SIZE -1 resource -- the Finder would use Retro68's "
                    "megabyte default")
    if main and size:
        pref, minimum = struct.unpack(">II", size[1][2:10])
        print("verify: resident CODE 'Main' %s bytes; partition %dK "
              "preferred, %dK minimum (lowest free measured: 113,184 in 640K)"
              % ("{:,}".format(len(main[0])), pref // 1024, minimum // 1024))


def main():
    vp = load_falcon_checker()
    vp.HERE = HERE
    vp.SHARED = [os.path.join(ROOT, "c128", "src", f)
                 for f in ("ui.c", "layout.c", "main.c")]
    src = open(os.path.join(HERE, "src", "macvid.c")).read()
    box_codes = set(int(m) for m in
                    re.findall(r'^\s*\{\s*(\d+),\s*\{', src, re.M))
    if not box_codes:
        print("verify_mac: read ZERO box[] entries out of macvid.c -- the "
              "parse is the bug")
        return 1

    consts = vp.constants()
    vp.fail = fail
    check_geometry(src, consts)
    vp.check_glyphs(box_codes, consts)
    check_app()

    if fail:
        for m in fail:
            print("verify_mac: FAIL -- " + m)
        return 1
    print("verify_mac: the console fits, every glyph draws, and the data "
          "inside the program is the data built")
    return 0


if __name__ == "__main__":
    sys.exit(main())
