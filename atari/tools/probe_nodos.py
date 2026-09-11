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

def main():
    disk = ATARI/"build"/"nodos-run.atr"
    shutil.copy(ATARI/"build"/"nodos.atr", disk)
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
            print("  whole file  STRINGS.DAT  status %d  %d bytes  head %s"
                  % (b("t_read_st"), w("t_read_len"),
                     " ".join("%02x"%x for x in a.peek(S["t_read_head"],4))))
            print("  streamed    BRIEF.TXT    status %d  %d bytes"
                  % (b("t_open_st"), w("t_stream_len")))
            print("  write+read  EGATREK.SAV  w=%d r=%d  %d bytes  head %s"
                  % (b("t_write_st"), b("t_back_st"), w("t_back_len"),
                     " ".join("%02x"%x for x in a.peek(S["t_back_head"],8))))
            exp = " ".join("%02x"%(0x5A^i) for i in range(8))
            print("  expected                                        head %s" % exp)
    finally:
        p.terminate()
        try: p.wait(timeout=5)
        except subprocess.TimeoutExpired: p.kill()
main()
