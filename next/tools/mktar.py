#!/usr/bin/env python3
"""Pack a built EGATrek.app into a tar the NeXT can unpack:

    python3 tools/mktar.py path/to/EGATrek.app OUT.tar

EGATrek.app is built on the NeXT (Makefile.next); this runs on the host. The
archive holds the wrapper, the program and the three data files inside it,
in the old ustar format NeXTSTEP's own tar reads, with fixed dates, owners and
modes and a fixed order, so the same build always packs into the same bytes
-- commodore-uno/next's tools/mktar.py, with this application's files. Unpack
it on the NeXT with `tar xf egatrek-next.tar`.
"""
import io
import os
import sys
import tarfile

app, out = sys.argv[1], sys.argv[2]
STAMP = 1795737600          # 2026-11-27 00:00 UTC, the day the port started
FILES = (("EGATrek", 0o755), ("STRINGS.DAT", 0o644), ("MUSIC.DAT", 0o644),
         ("BRIEF.TXT", 0o644))


def info(name, mode, size=0, kind=tarfile.REGTYPE):
    t = tarfile.TarInfo(name)
    t.type = kind
    t.mode = mode
    t.size = size
    t.mtime = STAMP
    t.uid = t.gid = 0
    t.uname = t.gname = ""
    return t


buf = io.BytesIO()
with tarfile.open(fileobj=buf, mode="w", format=tarfile.USTAR_FORMAT) as tar:
    tar.addfile(info("EGATrek.app", 0o755, kind=tarfile.DIRTYPE))
    for name, mode in FILES:
        path = os.path.join(app, name)
        if not os.path.exists(path):
            sys.exit("mktar: %s is missing from %s" % (name, app))
        data = open(path, "rb").read()
        tar.addfile(info("EGATrek.app/" + name, mode, len(data)), io.BytesIO(data))
with open(out, "wb") as f:
    f.write(buf.getvalue())
