#!/usr/bin/env python3
"""Drive the instrumented MEGA65 build headlessly and screenshot the result.

    make debug
    python3 tools/drive.py out.png RETURN N N J A M I E RETURN 3 RETURN X RETURN

A screenshot answers "what does the player see"; it cannot answer "what is in
`ship`". --peek SYMBOL[:LEN] dumps a symbol out of the running machine just
before the screenshot, looked up in the ELF the way kb_inject is, because these
addresses move for exactly the same reason:

    python3 tools/drive.py out.png --peek ship ... keys ...

WHY A POKE AND NOT A KEYSTROKE. Xemu can run headless and it flushes
-screenshot on SIGTERM, but it has no way to inject a key: $D610 is the ASCII
key register and WRITING it pops the queue rather than filling it. So the
debug build carries `kb_inject` (see src/m65input.c) and this pokes it through
the uart monitor, exactly as the C128 port is driven through VICE's monitor.

The debug build's overlay images are its own -- the release build's would be
linked against different resident addresses -- so this builds its OWN D81 at
build/drive.d81 with OVERLAYS-DEBUG.BIN in it. It no longer disturbs anything
the release build uses, which the old SD-card route did.
"""
import os, re, signal, socket, subprocess, sys, time

def sym(elf, want):
    """Look a symbol up rather than hardcoding it, and return (addr, size).

    IT MOVES. kb_inject was a literal 0x6B for one afternoon, and the moment
    the build changed it pointed at some other variable: the driver poked a
    byte nobody read, every key was 'never consumed', and the failure looked
    like the game hanging. --peek addresses move for the same reason."""
    nm = subprocess.check_output(
        [os.path.expanduser("~/llvm-mos/bin/llvm-nm"), "--print-size", elf]).decode()
    for ln in nm.splitlines():
        f = ln.split()
        if len(f) == 4 and f[3] == want:
            return int(f[0], 16), int(f[1], 16)
        if len(f) == 3 and f[2] == want:
            return int(f[0], 16), 0
    raise SystemExit("drive: no %s in %s -- is this the `make debug` build?"
                     % (want, elf))
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
    argv = sys.argv[1:]
    # --keep-disk leaves build/drive.d81 alone instead of formatting a fresh
    # one. Needed to test a RESTORE: the save the previous run wrote lives on
    # that disk, and rebuilding it is exactly what throws the save away.
    keep = "--keep-disk" in argv
    if keep:
        argv.remove("--keep-disk")
    peeks = []
    while "--peek" in argv:
        i = argv.index("--peek")
        peeks.append(argv[i + 1])
        del argv[i:i + 2]
    out, keys = argv[0], [key(t) for t in argv[1:]]
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(here)
    ELF = "build/egatrek-debug.elf"
    KB_INJECT, _ = sym(ELF, "kb_inject")
    # THE DEBUG D81, built here rather than on the SD card. The release build's
    # overlay images are linked against different resident addresses, so a
    # debug run needs its own OVERLAYS.BIN -- and since this port dropped the
    # Hypervisor for the C65 DOS, "putting a file where the game can find it"
    # is c1541 on a D81 instead of mtools on a FAT32 card. That deleted the one
    # wrinkle putfiles.sh existed for (Xemu's formatter leaves the FAT32 CHS
    # geometry zeroed and mtools refuses the card until it is patched).
    d81 = "build/drive.d81"
    if keep and os.path.exists(d81):
        files = []
    else:
        if os.path.exists(d81):
            os.unlink(d81)
        subprocess.check_call(["c1541", "-format", "ega trek,01", "d81", d81],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        files = [("build/OVERLAYS-DEBUG.BIN", "overlays.bin"),
             ("build/disk/STRINGS.DAT",   "strings.dat"),
             ("build/disk/MUSIC.DAT",     "music.dat"),
             ("build/disk/BRIEF.TXT",     "brief.txt")]
    for src, name in files:
        if not os.path.exists(src):
            raise SystemExit("drive: no %s -- run `make debug disk` first" % src)
        subprocess.check_call(["c1541", "-attach", d81, "-write", src,
                               name + ",s"],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

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
                          "-8", os.path.join(here, d81),
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

    # BEFORE the screenshot: SIGTERM ends the process and the socket with it.
    for spec in peeks:
        name, _, n = spec.partition(":")
        addr, size = sym(ELF, name)
        n = int(n) if n else (size or 16)
        got = b""
        while len(got) < n:
            r = talk("m%08x" % (addr + len(got)))
            row = re.search(r":%08X:((?:[0-9A-F]{2})+)" % (addr + len(got)),
                            r.upper())
            if not row:
                raise SystemExit("drive: peek %s at $%X read nothing" % (name, addr))
            got += bytes.fromhex(row.group(1))
        got = got[:n]
        print("drive: %s $%04X %d bytes" % (name, addr, n))
        for i in range(0, n, 16):
            print("  +%02d  %s" % (i, " ".join("%02x" % b for b in got[i:i + 16])))

    p.send_signal(signal.SIGTERM)
    p.wait()
    print("drive: %d keys -> %s" % (len(keys), out))

main()
