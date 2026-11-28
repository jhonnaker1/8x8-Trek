#!/usr/bin/env python3
"""What the host can check about a port whose compiler is on another machine.

The NeXT port is compiled ON THE NeXT (Makefile.next), so nothing here links
it. What fails there fails a hundred seconds into a build inside an emulator,
or silently on screen. This is the part of that the host can check in a second.

  * THE GLYPH SWEEP IS THE FALCON'S CHECKER, IMPORTED, as mac/tools/verify_mac.py
    and st/tools/verify_st.py import it. This port draws the Falcon's 8x16 box
    glyphs, so a screen code the shared UI can draw with no box[] entry and no
    ASCII mapping is the missing-glyph marker here exactly as there.
  * C89. The NeXT's cc is gcc 2.5.8, from 1993, the strictest compiler any port
    has. It rejected ui.c's ON_PAGE(), a statement ahead of declarations,
    in ten functions on the first build. Every C file the NeXT compiles is
    put through the host's cc as -std=c89 with declaration-after-statement an
    error -- the same check `make c89-check` makes of core/, over the wider set
    this port needs.
  * THE DEPLOY CARRIES EVERYTHING. `make deploy` copies a LIST of shared files
    to the NeXT. A file Makefile.next names, or a header one of them includes,
    that is missing from the list is a build that fails on the NeXT and
    nowhere else.
  * <stdint.h>. NeXTSTEP 3.3 has none; src/compat/stdint.h stands in, and
    must define every fixed-width name the compiled files use.
  * THE ICON. EGATrek.tiff is committed, because the NeXT that links it has no
    Python; it must be what tools/mkicon.py draws today.
"""
import importlib.util
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(HERE)
FALCON = os.path.join(ROOT, "falcon", "tools", "verify_prg.py")

fail = []


def load_falcon_checker():
    if not os.path.exists(FALCON):
        sys.exit("verify_next: %s is missing -- the glyph sweep IS that file"
                 % FALCON)
    spec = importlib.util.spec_from_file_location("falcon_verify", FALCON)
    mod = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(mod)
    except SystemExit:
        sys.exit("verify_next: importing %s EXITED -- it must be guarded with "
                 "`if __name__ == \"__main__\":`" % FALCON)
    return mod


def makefile_next():
    return open(os.path.join(HERE, "Makefile.next")).read()


def deploy_list():
    """The SHARED list out of the host Makefile, paths from the repo root."""
    mk = open(os.path.join(HERE, "Makefile")).read()
    m = re.search(r"^SHARED\s*=\s*((?:.*\\\n)*.*)$", mk, re.M)
    if not m:
        fail.append("no SHARED list in next/Makefile")
        return set()
    return set(m.group(1).replace("\\\n", " ").split())


def compiled_sources(mk):
    """Every source Makefile.next compiles, as paths from the repo root."""
    out = []
    for src in re.findall(r"-c (\S+\.[cm]) -o", mk):
        out.append(os.path.normpath(os.path.join("next", src)))
    return out


def check_deploy(mk, shared):
    """Everything Makefile.next or an included header reaches outside next/
    must be on the deploy list, and must exist."""
    need = set()
    for ref in re.findall(r"\.\./[\w./]+\.[a-z]+\b", mk):     # files, not -I dirs
        need.add(os.path.normpath(os.path.join("next", ref)))
    # Headers, transitively, from every compiled source.
    todo = list(compiled_sources(mk)) + [p for p in need if p.endswith(".h")]
    seen = set()
    while todo:
        p = todo.pop()
        if p in seen:
            continue
        seen.add(p)
        full = os.path.join(ROOT, p)
        if not os.path.exists(full):
            fail.append("%s is named but does not exist" % p)
            continue
        if not p.startswith("next/"):
            need.add(p)
        for inc in re.findall(r'^\s*#\s*include\s+"([^"]+)"', open(full).read(), re.M):
            cand = os.path.normpath(os.path.join(os.path.dirname(p), inc))
            if not os.path.exists(os.path.join(ROOT, cand)):
                # -I paths, as Makefile.next gives them: src, ../c128/src, ../core
                for d in ("next/src", "c128/src", "core"):
                    alt = os.path.normpath(os.path.join(d, inc))
                    if os.path.exists(os.path.join(ROOT, alt)):
                        cand = alt
                        break
            if os.path.exists(os.path.join(ROOT, cand)):
                todo.append(cand)
    missing = sorted(p for p in need if not p.startswith("next/") and p not in shared)
    for p in missing:
        fail.append("%s is needed on the NeXT but `make deploy` does not copy it"
                    % p)
    print("verify: deploy carries %d shared files; %d needed, %d missing"
          % (len(shared), len([p for p in need if not p.startswith("next/")]),
             len(missing)))


# The NeXT's <sound/sound.h> exists only on a NeXT. For a SYNTAX check the host
# needs the names nextsnd.c uses and nothing else, so it gets these, with the
# shapes NeXTSTEP 3.3's soundstruct.h, utilsound.h and performsound.h give
# them. A stand-in can only let a wrong call through, never fail a right one;
# the NeXT's own compiler remains the judge of the calls.
SOUND_STUB = """
typedef struct { int magic, dataLocation, dataSize, dataFormat,
                 samplingRate, channelCount; char info[4]; } SNDSoundStruct;
typedef int (*SNDNotificationFun)(SNDSoundStruct *s, int tag, int err);
#define SND_NULL_FUN ((SNDNotificationFun)0)
#define SND_ERR_NONE 0
#define SND_FORMAT_LINEAR_16 3
int SNDAlloc(SNDSoundStruct **s, int dataSize, int dataFormat,
             int samplingRate, int channelCount, int infoSize);
int SNDStartPlaying(SNDSoundStruct *s, int tag, int priority, int preempt,
                    SNDNotificationFun beginFun, SNDNotificationFun endFun);
int SNDStop(int tag);
char *SNDSoundError(int err);
"""


def check_c89(mk):
    """The C files only: the .m is Objective-C, which the host's cc cannot
    judge as the NeXT's would."""
    srcs = [p for p in compiled_sources(mk) if p.endswith(".c")]
    bad = 0
    stub = tempfile.mkdtemp()
    os.mkdir(os.path.join(stub, "sound"))
    with open(os.path.join(stub, "sound", "sound.h"), "w") as f:
        f.write(SOUND_STUB)
    for p in srcs:
        r = subprocess.run(["cc", "-std=c89", "-fsyntax-only", "-I" + stub,
                            "-Werror=declaration-after-statement",
                            "-DTREK_COLOUR_IS_EGA",
                            "-I" + os.path.join(HERE, "src"),
                            "-I" + os.path.join(ROOT, "c128", "src"),
                            "-I" + os.path.join(ROOT, "core"),
                            os.path.join(ROOT, p)],
                           capture_output=True, text=True)
        if r.returncode:
            bad += 1
            first = [l for l in r.stderr.splitlines() if "error" in l][:3]
            fail.append("%s is not C89, and the NeXT's gcc 2.5.8 will reject "
                        "it:\n        %s" % (p, "\n        ".join(first)))
    print("verify: %d C files the NeXT compiles, %d of them not C89"
          % (len(srcs), bad))


def check_stdint(mk):
    shim = open(os.path.join(HERE, "src", "compat", "stdint.h")).read()
    defined = set(re.findall(r"\b(u?int(?:8|16|32)_t|uintptr_t|U?INT\d+_(?:C|MAX|MIN))\b", shim))
    used = set()
    for p in compiled_sources(mk):
        used |= set(re.findall(r"\b(u?int(?:8|16|32|64)_t|u?intptr_t|U?INT\d+_(?:C|MAX|MIN))\b",
                               open(os.path.join(ROOT, p)).read()))
    # Headers too: the same names in a header count wherever it is included.
    for d in ("c128/src", "core", "next/src", "mac/src"):
        for f in os.listdir(os.path.join(ROOT, d)):
            if f.endswith(".h"):
                used |= set(re.findall(r"\b(u?int(?:8|16|32)_t|U?INT\d+_(?:C|MAX))\b",
                                       open(os.path.join(ROOT, d, f)).read()))
    missing = sorted(used - defined)
    for n in missing:
        fail.append("%s is used but src/compat/stdint.h does not define it -- "
                    "NeXTSTEP 3.3 has no <stdint.h> of its own" % n)
    print("verify: <stdint.h> stand-in defines %d names, the sources use %d, "
          "%d missing" % (len(defined), len(used), len(missing)))


def check_icon():
    tiff = os.path.join(HERE, "EGATrek.tiff")
    with tempfile.TemporaryDirectory() as t:
        out = os.path.join(t, "icon.tiff")
        subprocess.run([sys.executable, os.path.join(HERE, "tools", "mkicon.py"),
                        out], check=True)
        if not os.path.exists(tiff) or open(out, "rb").read() != open(tiff, "rb").read():
            fail.append("EGATrek.tiff is not what tools/mkicon.py draws -- "
                        "run `make icon`")
    print("verify: icon EGATrek.tiff, %d bytes, as tools/mkicon.py draws it"
          % os.path.getsize(tiff))


def main():
    vp = load_falcon_checker()
    vp.HERE = HERE
    vp.SHARED = [os.path.join(ROOT, "c128", "src", f)
                 for f in ("ui.c", "layout.c", "main.c")]
    src = open(os.path.join(HERE, "src", "nextvid.m")).read()
    box_codes = set(int(m) for m in
                    re.findall(r'^\s*\{\s*(\d+),\s*\{', src, re.M))
    if not box_codes:
        print("verify_next: read ZERO box[] entries out of nextvid.m -- the "
              "parse is the bug")
        return 1

    consts = vp.constants()
    vp.fail = fail
    vp.check_glyphs(box_codes, consts)
    mk = makefile_next()
    check_c89(mk)
    check_deploy(mk, deploy_list())
    check_stdint(mk)
    check_icon()

    if fail:
        for m in fail:
            print("verify_next: FAIL -- " + m)
        return 1
    print("verify_next: every glyph draws, the NeXT's sources are C89, the "
          "deploy carries them all, and the icon is current")
    return 0


if __name__ == "__main__":
    sys.exit(main())
