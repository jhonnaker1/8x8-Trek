#!/usr/bin/env python3
"""The no-DOS storage seam, tested against a disk with no DOS on it."""
import pathlib, re, shutil, subprocess, sys, time
HERE = pathlib.Path(__file__).resolve(); ATARI = HERE.parents[1]
SERVER = pathlib.Path.home()/"AltirraBridge-nightly-macos-arm64"/"AltirraBridgeServer"
sys.path.insert(0, str(SERVER.parent/"sdk"/"python"))
from altirra_bridge import AltirraBridge

def syms():
    nm = subprocess.check_output([str(pathlib.Path.home()/"llvm-mos/bin/llvm-nm"),
         "--print-size", str(ATARI/"build"/"nodostest.xex.elf")]).decode()
    out = {}
    for l in nm.splitlines():
        f = l.split()
        if len(f) >= 3: out[f[-1]] = int(f[0], 16)
    return out

def tally(data):
    """The same rotate-and-add src/nodostest.c computes, so a file that came
    back REORDERED fails -- a plain byte sum could not tell."""
    s = 0
    for ch in data:
        s = ((s << 1 | s >> 15) & 0xFFFF)
        s = (s + ch) & 0xFFFF
    return s


def main():
    disk = ATARI/"build"/"nodos-run.atr"
    shutil.copy(ATARI/"build"/"egatrek.atr", disk)
    log = ATARI/"build"/"bridge.log"; fh = open(log,"w")
    p = subprocess.Popen([str(SERVER),"--bridge","--settings=user","--pacing=unlimited"],
                         stdout=subprocess.DEVNULL, stderr=fh)
    tok=None; dl=time.time()+20
    while time.time()<dl:
        m=re.search(r"token-file:\s*(\S+)", log.read_text())
        if m: tok=m.group(1); break
        time.sleep(0.2)
    if not tok: p.kill(); sys.exit("probe_nodos: no token-file")
    S = syms()
    try:
        with AltirraBridge.from_token_file(tok) as a:
            a._sock.settimeout(180)
            a.mount(0, str(disk))
            a.boot(str(ATARI/"build"/"nodostest.xex"))
            for _ in range(90):
                a.frame(60)
                if a.peek(S["t_done"],1)[0]: break
            else: sys.exit("probe_nodos: never finished")
            w = lambda n: a.peek16(S[n])
            b = lambda n: a.peek(S[n],1)[0]
            bad = 0
            for label, name, st, ln, sm, want in (
                ("whole file", "MUSIC.DAT", "t_read_st", "t_read_len",
                 "t_read_sum", (ATARI/"build"/"data"/"MUSIC.DAT").read_bytes()),
                ("streamed",   "BRIEF.TXT",   "t_open_st", "t_stream_len",
                 "t_stream_sum", (ATARI/"build"/"data"/"BRIEF.TXT").read_bytes()),
                ("write+read", "PROBE.DAT",   "t_back_st", "t_back_len",
                 "t_back_sum", bytes((0x5A ^ (i*7)) & 0xFF for i in range(300))),
            ):
                glen, gsum = w(ln), w(sm)
                elen, esum = len(want), tally(want)
                ok = b(st) == 0 and glen == elen and gsum == esum
                bad += not ok
                print("  %-11s %-12s status %d  %5d bytes (want %5d)  "
                      "sum %04x (want %04x)  %s"
                      % (label, name, b(st), glen, elen, gsum, esum,
                         "ok" if ok else "MISMATCH"))
            if b("t_write_st"):
                print("  write status %d -- the slot was not claimed" % b("t_write_st"))
                bad += 1
            if bad:
                sys.exit("probe_nodos: %d of 3 paths wrong" % bad)
    finally:
        p.terminate()
        try: p.wait(timeout=5)
        except subprocess.TimeoutExpired: p.kill()
main()
