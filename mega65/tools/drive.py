#!/usr/bin/env python3
"""Drive the instrumented MEGA65 build headlessly and screenshot the result.

    make debug
    python3 tools/drive.py out.png RETURN N N J A M I E RETURN 3 RETURN X RETURN

WHY A POKE AND NOT A KEYSTROKE. Xemu can run headless and it flushes
-screenshot on SIGTERM, but it has no way to inject a key: $D610 is the ASCII
key register and WRITING it pops the queue rather than filling it. So the
debug build carries `kb_inject` (see src/m65input.c) and this pokes it through
the uart monitor, exactly as the C128 port is driven through VICE's monitor.

The debug build's overlay images are its own -- the release build's would be
linked against different resident addresses -- so this puts OVERLAYS-DEBUG.BIN
on the card as OVERLAYS.BIN and does not put it back. Run `make disk` and
tools/putfiles.sh before testing a release build again.
"""
import os, re, signal, socket, subprocess, sys, time

KB_INJECT = 0x6B          # from `llvm-nm build/egatrek-debug.elf`
NAMED = {"RETURN": 0x0D, "SPACE": 0x20, "ESC": 0x1B, "DEL": 0x14,
         "UP": 0x91, "DOWN": 0x11}

def key(tok):
    if tok in NAMED:
        return NAMED[tok]
    if tok.startswith("$"):
        return int(tok[1:], 16)
    if len(tok) == 1:
        return ord(tok)
    raise SystemExit("drive: don't know key %r" % tok)

def main():
    out, keys = sys.argv[1], [key(t) for t in sys.argv[2:]]
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(here)
    subprocess.check_call(["sh", "tools/putfiles.sh", "build/OVERLAYS-DEBUG.BIN"],
                          stdout=subprocess.DEVNULL)
    # putfiles keeps the source name; the game opens OVERLAYS.BIN.
    subprocess.check_call(["python3", "-c", """
import os,subprocess,tempfile,sys
img=os.path.expanduser('~/Library/Application Support/xemu-lgb/mega65/mega65.img')
rc=tempfile.NamedTemporaryFile('w',suffix='.mtoolsrc',delete=False)
rc.write('drive z: file="%s" offset=1048576\\n' % img); rc.close()
os.environ['MTOOLSRC']=rc.name
subprocess.check_call(['mcopy','-o','build/OVERLAYS-DEBUG.BIN','z:/OVERLAYS.BIN'])
"""])
    rom = os.path.expanduser("~/Library/Application Support/xemu-lgb/mega65/MEGA65.ROM")
    sock = "/tmp/m65drive.sock"
    if os.path.exists(sock):
        os.unlink(sock)
    log = open("/tmp/drive.log", "w")
    for f in (out,):
        if os.path.exists(f):
            os.unlink(f)
    p = subprocess.Popen([os.path.expanduser("~/xemu/bin/xmega65"), "-rom", rom,
                          "-sdimg", "@mega65.img", "-prgmode", "65",
                          "-prg", os.path.join(here, "build/egatrek-debug.prg"),
                          "-besure", "-headless", "-screenshot", out,
                          "-uartmon", sock], stdout=log, stderr=log)
    time.sleep(9)                      # boot, then STRINGS/MUSIC/OVERLAYS
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock)
    s.settimeout(0.4)
    def talk(c):
        s.sendall((c + "\n").encode())
        time.sleep(0.06)
        out = b""
        try:
            while True:
                d = s.recv(65536)
                if not d:
                    break
                out += d
        except socket.timeout:
            pass
        return out.decode("latin1")

    # HANDSHAKE ON THE BYTE, don't guess a delay. kb_inject holds ONE key, so
    # poking again before the game has taken the last one silently loses it --
    # which is what a fixed 0.45s cadence did across the disk load that sits
    # between the briefing answer and the setup screen: six keys in a row
    # vanished and the answers landed on the wrong questions.
    for k in keys:
        talk("s%08x %02x" % (KB_INJECT, k))
        for _ in range(200):
            r = talk("m%08x" % KB_INJECT)
            m = re.search(r":%08X:(..)" % KB_INJECT, r.upper())
            if m and int(m.group(1), 16) == 0:
                break
            time.sleep(0.05)
        else:
            raise SystemExit("drive: key %02x was never consumed" % k)
    time.sleep(1.5)
    p.send_signal(signal.SIGTERM)
    p.wait()
    print("drive: %d keys -> %s" % (len(keys), out))

main()
