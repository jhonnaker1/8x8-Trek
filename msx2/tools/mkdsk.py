#!/usr/bin/env python3
"""Build the release disk image with openMSX's own diskmanipulator.

A 720K FAT12 image with an MSX-DOS 2 BOOT SECTOR -- diskmanipulator's default
format. A plain FAT12 image written by anything else has no MSX boot code
and drops the machine into Disk BASIC. Driven over openMSX's control channel
with the machine never powered on; every reply is checked, and a `nok`
fails the build rather than leaving a half-written image.

Usage: mkdsk.py out.dsk file ..."""
import html, os, re, subprocess, sys, tempfile, time

out, files = os.path.abspath(sys.argv[1]), [os.path.abspath(f) for f in sys.argv[2:]]
if os.path.exists(out):
    os.remove(out)
cmds = ["diskmanipulator create {%s} 720" % out, "virtual_drive {%s}" % out]
cmds += ["diskmanipulator import virtual_drive {%s}" % f for f in files]
cmds += ["diskmanipulator dir virtual_drive", "virtual_drive eject"]

with tempfile.TemporaryDirectory() as t:
    settings = os.path.join(t, "settings.xml")
    open(settings, "w").write("<!DOCTYPE settings SYSTEM 'settings.dtd'>\n<settings/>\n")
    p = subprocess.Popen(["/Applications/openMSX.app/Contents/MacOS/openmsx",
                          "-setting", settings, "-machine", "Philips_NMS_8250",
                          "-control", "stdio"],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True)
    p.stdin.write("<openmsx-control>\n")
    for c in cmds:
        p.stdin.write("<command>%s</command>\n" % html.escape(c))
    p.stdin.write("<command>exit</command>\n")
    p.stdin.flush()
    try:
        text, _ = p.communicate(timeout=60)
    except subprocess.TimeoutExpired:
        p.kill()
        sys.exit("mkdsk: openMSX did not finish")

replies = re.findall(r'<reply result="(\w+)">(.*?)</reply>', text, re.S)
bad = [(c, html.unescape(r)) for c, (ok, r) in zip(cmds, replies) if ok != "ok"]
if len(replies) < len(cmds) or bad:
    sys.exit("mkdsk: FAILED\n" + "\n".join("  %s -> %s" % b for b in bad) + ("" if bad else "\n" + text))
print(html.unescape(replies[len(files) + 2][1]).rstrip())
print("%s: %d bytes" % (out, os.path.getsize(out)))
