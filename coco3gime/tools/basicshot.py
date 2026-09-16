#!/usr/bin/env python3
"""Type a BASIC program into a BARE CoCo 3 under MAME and photograph the screen.

THE FIRST MEASUREMENT OF THIS PORT, and it is deliberately not a driver. The
question -- can the GIME give 80x25 text with eight per-cell foreground
colours -- has a datasheet answer I could recite and a machine answer I can
read. Super Extended Color BASIC already programs every one of those registers
correctly for `WIDTH 80`, so the ROM is the ground truth: let it set the mode
up, print a ruler, and COUNT.

BARE MACHINE ON PURPOSE. No `-ext multi`, no `ssfm` -- that card is the whole
reason [[coco3-port]] cannot run on Jamie's own CoCo 3, and this port exists to
need nothing but the machine and its disk controller.

THE NATURAL KEYBOARD HAS TO BE TURNED ON or posts vanish silently and BASIC
sits in its keyboard poll looking exactly like a program that failed to start
-- learned by coco3/tools/ovlrun.py, not rediscovered here.
"""
import os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
MAME = os.path.expanduser("~/ample/Ample.app/Contents/MacOS/mame64")
ROMS = os.path.expanduser("~/Library/Application Support/Ample/roms")

LUA = r'''
emu.wait(%(boot)s)
local kbd = manager.machine.natkeyboard
kbd.in_use = true
%(posts)s
emu.wait(%(settle)s)
manager.machine.video:snapshot()
emu.wait(0.5)
manager.machine:exit()
'''


def main():
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "build", "gime.png")
    boot = float(os.environ.get("BOOT", "14"))
    settle = float(os.environ.get("SETTLE", "4"))

    lines = [l.rstrip("\n") for l in open(src) if l.strip()]
    posts = "\n".join(
        'kbd:post_coded(%s)\nemu.wait(1.2)' % lua_str(l + "{ENTER}") for l in lines)

    tmp = tempfile.mkdtemp()
    luaf = os.path.join(tmp, "p.lua")
    open(luaf, "w").write(LUA % dict(boot=boot, posts=posts, settle=settle))

    p = subprocess.run(
        [MAME, "coco3", "-window", "-skip_gameinfo", "-rompath", ROMS,
         "-autoboot_script", luaf, "-autoboot_delay", "1",
         "-seconds_to_run", str(int(boot + settle + 6 * len(lines) + 10)),
         "-nothrottle", "-snapshot_directory", tmp],
        capture_output=True, text=True, cwd=tmp, check=False)

    shots = []
    for dp, dn, fn in os.walk(tmp):
        shots += [os.path.join(dp, f) for f in fn if f.endswith(".png")]
    if not shots:
        print("basicshot: MAME wrote no snapshot. Its output:")
        print((p.stdout + p.stderr).strip()[-900:])
        return 1
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    import shutil
    shutil.copy(sorted(shots)[-1], out)
    print("  %s  %d bytes" % (out, os.path.getsize(out)))
    return 0


def lua_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


if __name__ == "__main__":
    sys.exit(main())
