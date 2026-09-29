#!/usr/bin/env python3
"""Drive C64 OS in VICE with nobody at the mouse.

    rig.py install BUNDLE APP            copy the files BUNDLE/files.txt lists
                                         ("name,type" per line; the default is
                                         main.o, menu.m, about.t) to
                                         //os/applications/APP on the work
                                         disk and verify every byte
    rig.py watch [--wait=S] [--quiet=S]  boot at 1x for Jamie's double-click and
                                         print every screen the app draws
    rig.py boot [--key=HEX,HEX..] [--wait=S] [--jam] [--human]
                                         boot C64 OS, find the probe's record,
                                         and print what the running system
                                         says; --jam catches a JAM in the
                                         monitor and prints the CPU history;
                                         --human runs at 1x and waits for
                                         Jamie to double-click the app

THE RIG IS uno's: C64 OS on a CMD HD image as device 10, JiffyDOS, a 16MB REU,
a host directory as device 8 (commodore-uno/c64os-llvm/Makefile). The image
and ROMs are commercial and are the user's own: C64OS_ROMS names the directory
(default ~/claude-code/commodore-uno/c64os/rom), and the image is COPIED to
build/work.dhd, never written in place. Delete the copy to start clean.

EVERY VICE COMMAND LINE HERE PASSES +saveres. VICE writes its command line
into ~/.config/vice/vicerc on exit otherwise, and the first `make run` of the
c64reu port did exactly that to Jamie's settings.

THE LAUNCH NEEDS A HUMAN. C64 OS scans the keyboard matrix itself and VICE's
monitor has no mouse, so an app cannot be double-clicked from a script.
//os/settings/homebase.t looked like a way round it and IS NOT: tried
2026-09-27, naming any app the booter does not know crashes the BOOTER --
a JAM at $A98C, reached by the JMP at $14D7 about 24 million cycles in,
before any app runs (a breakpoint on the app's init never fired), while
rewriting the file with its original "App Launcher" boots cleanly. So
--human boots at 1x and waits for Jamie's double-click.
"""
import os
import shutil
import signal
import struct
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = os.path.dirname(HERE)
ROOT = os.path.dirname(PORT)
sys.path.insert(0, os.path.join(ROOT, "tools"))
import vice_mon                                  # noqa: E402

ROMS = os.environ.get("C64OS_ROMS",
                      os.path.expanduser("~/claude-code/commodore-uno/c64os/rom"))
WORK = os.path.join(PORT, "build", "work.dhd")
SOUND_WAV = os.path.join(PORT, "build", "sound.wav")
FS8 = os.path.join(PORT, "build", "fs8")
BANK_RAM = 1
MAGIC = b"EGATREKPROBE!"
CMD_QUIT = 0xBB


TEXTMON = 6510     # VICE's text monitor, for CPU history after a JAM


def vice(keybuf, warp=True, jam=False, monitor=True):
    for f in ("c64os.dhd", "cmd_hd_bootrom.bin", "JiffyDOS_C64.bin"):
        if not os.path.exists(os.path.join(ROMS, f)):
            sys.exit("rig: %s is missing from %s -- C64 OS and its ROMs are "
                     "yours to supply; set C64OS_ROMS" % (f, ROMS))
    if not os.path.exists(WORK):
        os.makedirs(os.path.dirname(WORK), exist_ok=True)
        shutil.copy(os.path.join(ROMS, "c64os.dhd"), WORK)
    os.makedirs(FS8, exist_ok=True)
    cmd = ["x64sc", "-default", "+saveres"] + (["-warp"] if warp else []) + [
        "-kernal", os.path.join(ROMS, "JiffyDOS_C64.bin"),
        "-dosCMDHD", os.path.join(ROMS, "cmd_hd_bootrom.bin"),
        "-controlport1device", "3", "-fslongnames",
        "-reu", "-reusize", "16384", "-keybuf-delay", "50",
        "-busdevice8", "-drive8type", "0", "-drive10type", "4844",
        "-fs8", FS8, "-10", WORK,
        # EVERYTHING VICE PLAYS, TO A FILE: `rig.py loudness` reads it. Sound
        # is the one thing neither a screen dump nor a register read can see
        # -- the SID's registers are write-only -- and "there is no sound"
        # from Jamie is a report about his speakers as much as the game.
        "-soundrecdev", "wav", "-soundrecarg", SOUND_WAV,
        "-keybuf", keybuf]
    if monitor:
        cmd += ["-binarymonitor", "-binarymonitoraddress",
                "ip4://127.0.0.1:%d" % vice_mon.PORT]
    if jam:
        # A JAM drops into the monitor instead of a dialog on Jamie's screen,
        # and the CPU history stops where the CPU did.
        cmd += ["-jamaction", "2", "-remotemonitor",
                "-remotemonitoraddress", "127.0.0.1:%d" % TEXTMON]
    # VICE'S OWN LOG, KEPT: it names every RESET and JAM, and the first runs
    # sent it to /dev/null while memory was coming back as power-on pattern.
    log = open(os.path.join(PORT, "build", "vice.log"), "w")
    return subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)


def quit_clean(p, mon):
    """Quit through the monitor so VICE flushes the disk image, then make sure."""
    try:
        mon.cmd(CMD_QUIT)
    except Exception:
        pass
    try:
        p.wait(timeout=10)
    except subprocess.TimeoutExpired:
        p.send_signal(signal.SIGKILL)
        p.wait()


def text(cells, width=40):
    """C64 screen codes (the ROM set, as BASIC shows them) to ASCII."""
    out = []
    for c in cells:
        c &= 0x7F
        out.append(chr(c + 64) if 1 <= c <= 26 else chr(c) if 32 <= c < 64 else
                   "@" if c == 0 else ".")
    return "".join(out)


def install(bundle, app):
    for f in os.listdir(FS8) if os.path.isdir(FS8) else []:
        os.remove(os.path.join(FS8, f))
    os.makedirs(FS8, exist_ok=True)
    listing = os.path.join(bundle, "files.txt")
    files = ([tuple(l.strip().split(",")) for l in open(listing) if l.strip()]
             if os.path.exists(listing) else
             [("main.o", "p"), ("menu.m", "s"), ("about.t", "s")])
    for f, _ in files:
        shutil.copy(os.path.join(bundle, f), os.path.join(FS8, f))
    # THE COPIER (tools/copier.s): whole blocks where BASIC's GET#/PRINT# moved
    # a byte at a time -- 18 minutes for the game's 27 files before it.
    subprocess.check_call([os.path.expanduser("~/llvm-mos/bin/mos-common-clang"),
                           "-mcpu=mos6502", "-nostartfiles",
                           "-T", os.path.join(HERE, "copier.ld"),
                           "-o", os.path.join(FS8, "copier"),
                           os.path.join(HERE, "copier.s")],
                          stderr=subprocess.DEVNULL)
    bas = open(os.path.join(HERE, "install.bas")).read()
    # THE LIST AS DATA LINES OF 60 CHARACTERS OR SO: one line would be over
    # BASIC's 255, and petcat would take it without saying so.
    def expect(f):
        d = open(os.path.join(bundle, f), "rb").read()
        return "%d,%d" % (len(d), sum(d) & 0xFFFF)
    items = [str(len(files))] + ["%s,%s,%s" % (f, t, expect(f)) for f, t in files]
    lines, cur = [], []
    for it in items:
        if cur and len(",".join(cur + [it])) > 60:
            lines.append(cur)
            cur = []
        cur.append(it)
    lines.append(cur)
    data = "\n".join("%d data %s" % (350 + i, ",".join(l)) for i, l in enumerate(lines))
    if not app:
        # NO FOLDER: the files go into //os/applications itself, which is
        # where a player puts a .car before double-clicking it -- the one
        # test of a release that C64 OS's own extractor gets to take.
        for line in ('80 print#15,"md:{APP}"', '90 print#15,"cd//os/applications/{APP}"'):
            if line not in bas:
                sys.exit("rig: install.bas no longer has %r" % line)
            bas = bas.replace(line, line[:2] + " rem")
    bas = bas.replace("{APP}", app).replace("350 data {FILES}", data)
    open(os.path.join(FS8, "install.txt"), "w").write(bas)
    subprocess.check_call(["petcat", "-w2", "-o", os.path.join(FS8, "install"),
                           "--", os.path.join(FS8, "install.txt")],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.remove(os.path.join(FS8, "install.txt"))
    want = {f: os.path.getsize(os.path.join(bundle, f)) for f, _ in files}

    p = vice('load"install",8\nrun\n')
    try:
        time.sleep(4)
        mon = vice_mon.Mon(check=False)
        # THE RESULTS COME BACK AS A FILE on device 8 -- this Mac's folder --
        # because with the copier, lines scroll off the C64's screen between
        # polls. BUT VICE DOES NOT WRITE THAT FILE TO THE MAC UNTIL IT QUITS,
        # CLOSE or no CLOSE: a finished install's result read back empty. So
        # the end is taken from the screen -- "DONE" is printed last and
        # never scrolls away -- and the file is read after a clean quit.
        t0, done = time.time(), False
        while time.time() - t0 < 1800 and not done:
            scr = mon.mem_get(0x0400, 1000)
            done = any(text(scr[r * 40:r * 40 + 40]).strip() == "DONE" for r in range(25))
            time.sleep(1)
        print("  installed in %.0fs" % (time.time() - t0))
        quit_clean(p, mon)
    except Exception:
        p.send_signal(signal.SIGKILL)
        p.wait()
        raise
    res = os.path.join(FS8, "result")
    lines = ([l.strip().upper() for l in
              open(res, "rb").read().decode("latin-1").split("\r") if l.strip()]
             if os.path.exists(res) else [])
    ok = True
    for f, n in want.items():
        # LENGTH AND SUM, read back off the CMD HD and compared with the file
        # on the Mac -- see tools/copier.s for why not byte by byte.
        d = open(os.path.join(bundle, f), "rb").read()
        hit = [l for l in lines if l.startswith(f.upper() + " ")]
        got = hit[0].split() if hit else []
        good = (len(got) >= 7 and got[2] == str(len(d)) and got[4] == str(sum(d) & 0xFFFF)
                and got[6] == "0")
        tries = got[8] if len(got) >= 9 else "?"
        print("  %-12s %6d bytes  %s%s" % (f, len(d), "ok" if good else
              ("MISMATCH: " + hit[0]) if hit else "NOT REPORTED",
              "" if tries in ("1", "?") else "  (%s tries)" % tries))
        ok = ok and good
    if "DONE" not in lines:
        print("  the installer never wrote DONE; its result file said:")
        for l in lines:
            print("    %s" % l)
        ok = False
    print("rig: install %s" % ("verified" if ok else "FAILED"))
    return 0 if ok else 1


def textmon(cmds, quiet=2.0):
    """Send VICE's text monitor some commands; return everything it said."""
    import socket
    s = socket.create_connection(("127.0.0.1", TEXTMON), timeout=10)
    out = b""
    for c in cmds:
        s.sendall(c.encode() + b"\n")
        t = time.time()
        s.settimeout(0.3)
        while time.time() - t < quiet:
            try:
                d = s.recv(65536)
                if d:
                    out += d
                    t = time.time()
            except socket.timeout:
                pass
    s.close()
    return out.decode("latin-1")


def boot(wait, key, jam=False, human=False):
    p = vice('load"c64os",10\nrun\n', warp=not human, jam=jam)
    if human:
        print("rig: C64 OS is booting in the VICE window. Double-click the app;"
              " this waits %.0f seconds for it." % wait)
    try:
        time.sleep(4)
        mon = vice_mon.Mon(check=False)
        t0, at = time.time(), -1
        while time.time() - t0 < wait:
            try:
                ram = mon.mem_get(0x0900, 0xA500 - 0x0900, bank=BANK_RAM)
            except (TimeoutError, OSError):
                # A JAM WITH --jam PUTS VICE IN ITS TEXT MONITOR, and the
                # binary monitor stops answering: the timeout IS the report.
                if not jam:
                    raise
                print("rig: the machine stopped answering after %.0fs -- a JAM"
                      % (time.time() - t0))
                print(textmon(["r", "chis 120", "d a970 a9a0"]))
                p.send_signal(signal.SIGKILL)
                p.wait()
                return 1
            at = ram.find(MAGIC)
            if at >= 0 and ram[at + len(MAGIC) + 1] | ram[at + len(MAGIC) + 2]:
                break                     # found, and drawn at least once
            time.sleep(2)
        took = time.time() - t0
        if at < 0:
            print("rig: no probe record in $0900-$A4FF after %.0fs" % took)
            if jam:
                print(textmon(["r", "chis 120", "d a970 a9a0"]))
        else:
            rec = 0x0900 + at
            if key:
                # C64 OS's printable-key event buffer and its count; the
                # machine is stopped between the writes and the resume.
                writes = [(0x0277 + i, k) for i, k in enumerate(key)]
                for addr, val in writes + [(0x00C6, len(key))]:
                    body = (bytes([0]) + struct.pack("<HH", addr, addr) + bytes([0])
                            + struct.pack("<H", BANK_RAM) + bytes([val]))
                    mon.cmd(vice_mon.CMD_MEM_SET, body)
                mon.resume()
                time.sleep(3)
            r = mon.mem_get(rec, len(MAGIC) + 8 + 20, bank=BANK_RAM)[len(MAGIC):]
            print("rig: probe record at $%04X, found after %.0fs" % (rec, took))
            print("  inits %d  draws %d  key events %d" % (r[0], r[1] | r[2] << 8, r[3]))
            print("  $01 at init $%02X, in draw $%02X, in kprnt $%02X" % (r[5], r[6], r[7]))
            if key:
                print("  injected: %s" % " ".join("$%02X" % k for k in key))
            for i in range(min(r[3], 4)):
                e = r[8 + i * 5:13 + i * 5]
                print("  event %d: callback A=$%02X  readkprnt A=$%02X X=$%02X Y=$%02X%s"
                      % (i + 1, e[0], e[1], e[2], e[3], "  (queue EMPTY)" if e[4] else ""))
        pm = mon.mem_get(0x0809, 0x98, bank=BANK_RAM)
        runs, start = [], 0
        for i in range(1, len(pm) + 1):
            if i == len(pm) or pm[i] != pm[start]:
                runs.append((0x09 + start, 0x09 + i - 1, pm[start]))
                start = i
        names = {0: "free", 1: "system", 2: "utility", 0xFF: "app"}
        print("  page map:")
        for a, z, v in runs:
            print("    $%02X00-$%02XFF  %-7s %3d pages" % (a, z, names.get(v, hex(v)), z - a + 1))
        rv = mon.mem_get(0x0281, 6, bank=BANK_RAM)
        print("  REU: banks $%02X, first app bank %d, app freeze bank %d"
              % (rv[0], rv[1], rv[5]))
        scr = mon.mem_get(0xDC00, 1000, bank=BANK_RAM)
        for row in (0, 24):
            codes = sorted(set(scr[row * 40:row * 40 + 40]))
            print("  row %2d uses screen codes %s" % (row, " ".join("%02X" % c for c in codes)))
        open(os.path.join(PORT, "build", "screen.bin"), "wb").write(scr)
        quit_clean(p, mon)
    except Exception:
        p.send_signal(signal.SIGKILL)
        p.wait()
        raise
    return 0 if at >= 0 else 1


# C64 OS's screen codes, as osvid.c draws them, to text: capitals at 65-90,
# lowercase at 1-26, the fifteen borrowed box glyphs at their slots.
BOX = {102: "-", 103: "|", 104: "+", 105: "+", 106: "+", 107: "+", 108: "+",
       109: "+", 110: "+", 111: "+", 112: "+", 114: "#", 115: "o", 116: "_",
       117: "^"}


def ostext(cells):
    out = []
    for c in cells:
        b = c & 0x7F
        out.append(BOX.get(b) or (chr(b) if 32 <= b <= 95 else
                   chr(b + 96) if 1 <= b <= 26 else "@" if b == 0 else "."))
    return "".join(out)


class TextMon(object):
    """VICE's text monitor, connected at boot so a JAM has somewhere to print
    (a socket opened after the JAM gets nothing). -jamaction 2 sends a JAM
    here instead of into a dialog on Jamie's screen."""
    def __init__(self):
        import socket
        self.sock = socket.create_connection(("127.0.0.1", TEXTMON), timeout=10)
        self.sock.settimeout(0.3)
        self.read(1.0)
        self.sock.sendall(b"x\n")          # connecting stopped it; carry on
        self.read(1.0)

    def read(self, quiet=1.5):
        import socket
        out, end = b"", time.time() + quiet
        while time.time() < end:
            try:
                d = self.sock.recv(65536)
                if d:
                    out += d
                    end = time.time() + quiet
            except socket.timeout:
                pass
        return out.decode("latin-1")

    def ask(self, cmd, quiet=2.0):
        self.sock.sendall(cmd.encode() + b"\n")
        return self.read(quiet)


def play():
    """C64 OS in VICE with NOTHING attached: no monitor, no polling -- to be
    heard. Every monitor call stops the emulator for a moment, and at the
    watch's ten a second the music came out choppy on Jamie's speakers while
    the recording, made in emulated time, was smooth. Still records
    build/sound.wav. Returns when VICE closes."""
    p = vice('load"c64os",10\nrun\n', warp=False, monitor=False)
    print("rig: C64 OS is booting with no monitor attached; double-click the "
          "app, and close VICE when done.")
    return p.wait()


def loudness():
    """Peak level of build/sound.wav, second by second, beside the watch
    log's clock. Reads the samples raw after the 44-byte header, because a
    VICE that was killed never went back to fill in the header's length."""
    raw = open(SOUND_WAV, "rb").read()
    rate = struct.unpack("<I", raw[24:28])[0] or 44100
    chans = struct.unpack("<H", raw[22:24])[0] or 1
    data = raw[44:]
    n = len(data) // 2
    s = struct.unpack("<%dh" % n, data[:n * 2])[::chans]
    per = rate
    loud = 0
    for i in range(0, len(s), per):
        seg = s[i:i + per]
        peak = max(abs(x) for x in seg) if seg else 0
        if peak > 500:
            loud += 1
        print("  %4ds  peak %5d  %s" % (i // per, peak, "#" * (peak // 1000)))
    print("rig: %d of %d seconds carry sound (peak > 500)" % (loud, (len(s) + per - 1) // per))
    return 0 if loud else 1


def sound_report(mon, sym):
    """The game is waiting for a key: is the sound driver running, and has it
    written the SID? c128/src/sid.c's statics, read by name. The SID's own
    registers are write-only ($D400-$D418), so a read of them says nothing;
    the driver's state is the instrument. mus_ok = MUSIC.DAT reached the far
    store; mus_on = a track is playing; enabled = the player's sound toggle."""
    names = ("enabled", "mus_ok", "mus_on", "sfx_on", "snd_region")
    got = ["%s %d" % (n, mon.mem_get(sym[n], 1, bank=BANK_RAM)[0])
           for n in names if n in sym]
    for n in ("mus_base", "mus", "sfx"):
        if n in sym:
            v = mon.mem_get(sym[n], 2, bank=BANK_RAM)
            got.append("%s $%04X" % (n, v[0] | v[1] << 8))
    port = mon.mem_get(0x0001, 1, bank=0)[0]      # the CPU's view: the port
    print("--- sound: %s; $01 $%02X" % (", ".join(got), port))


def bank_report(mon, sym):
    """The game halted (stage 50): if for want of REU banks, what C64 OS
    had. io_rec.t names $0281 reubanks, $0282 appreubk, $0286 reufrzbk;
    bkalloc_ (memory.lib) fetches the bank map from bank 0 $1B00 into its
    own page + $214 and scans it from appreubk -- $FE free, $FF not there
    (it stops), $FD/$FC fast-app-switch slots, $01-$20 owned."""
    v = mon.mem_get(0x0281, 7, bank=BANK_RAM)
    print("--- REU: reubanks $%02X  appreubk $%02X  reufrzbk $%02X  ($0281-7: %s)"
          % (v[0], v[1], v[5], v.hex(" ")))
    page = mon.mem_get(sym["c64os_libpage"], 1, bank=BANK_RAM)[0] \
        if "c64os_libpage" in sym else 0
    if not page:
        print("--- no memory.lib page recorded")
        return
    bm = mon.mem_get(page * 256 + 0x214, 256, bank=BANK_RAM)
    print("--- bank map as bkalloc_ saw it (memory.lib at $%02X00):" % page)
    for r in range(0, 256, 32):
        print("  %02X: %s" % (r, bm[r:r + 32].hex(" ")))


def watch(wait, quiet):
    """Boot at 1x, wait for Jamie to launch the app, print every new screen,
    and when the app goes still, ask the machine why.

    THE APP IS 'LAUNCHED' WHEN ITS OWN BYTES ARE AT $0900 -- main.o's first
    ten, read from the bundle. Stillness only counts from then: the first
    version counted it from C64 OS's own screen, and a slow double-click
    would have ended the watch before the game started."""
    head = open(os.path.join(PORT, "build", "bundle", "main.o"), "rb").read()[2:12]
    p = vice('load"c64os",10\nrun\n', warp=False, jam=True)
    print("rig watch v9 (traces REU transfers, C64 OS exceptions, and its page map)")
    print("rig: C64 OS is booting in the VICE window. Double-click the app;"
          " this watches for %.0f seconds." % wait)
    try:
        time.sleep(3)
        tm = TextMon()
        mon = vice_mon.Mon(check=False)
        t0, last, since, shown, launched = time.time(), None, time.time(), 0, None
        parked, dmas = 0, 0
        while time.time() - t0 < wait:
            said = tm.read(0.05)
            if launched and "STOP ON EXEC" in said.upper().replace("  ", " ") and excs < 4:
                excs += 1
                print("--- EXCEPTION %d at %.1fs (C64 OS's BRK handler entered)"
                      % (excs, time.time() - t0))
                print(tm.ask("r").strip())
                print(tm.ask("chis 60", quiet=2.0).strip())
                print("C64 OS system page (drivers, page map, libraries):")
                print(tm.ask("m 0800 08cf").strip())
                print("exception index $C7 and table $024A-$0258:")
                print(tm.ask("m 00c7 00c7").strip())
                print(tm.ask("m 024a 0258").strip())
                print(tm.ask("m 01c0 01ff").strip())
                if excs >= 4:
                    print(tm.ask("delete"))
                tm.sock.sendall(b"x\n")
                sys.stdout.flush()
                continue
            if launched and "STOP ON STORE" in said.upper().replace("  ", " "):
                dmas += 1
                print("--- REU command write %d at %.1fs" % (dmas, time.time() - t0))
                print(tm.ask("r").strip())
                print(tm.ask("chis 300", quiet=3.0).strip())
                # THE CALL CHAIN: every return address back to main() is here.
                print(tm.ask("m 0100 01ff").strip())
                print(tm.ask("bank io") + tm.ask("m df00 df0a").strip() + tm.ask("bank cpu"))
                # FOUR IS ENOUGH. Each stop costs ~14s of wall clock for a
                # transfer that takes microseconds: the 23 overlays' loads
                # took 11 minutes under the tracer, half a second each by the
                # cycle counter. The first four cover memory.lib and the first
                # overlay; after that only the exception break stays armed.
                if dmas >= 4:
                    tm.ask("delete")
                    tm.ask("break exec 03df")
                    print("--- REU tracing off after %d transfers; exceptions still watched" % dmas)
                tm.sock.sendall(b"x\n")
                sys.stdout.flush()
                continue
            if "JAM" in said.upper():
                jam_report(tm, said, time.time() - t0)
                p.send_signal(signal.SIGKILL)
                p.wait()
                return 1
            try:
                scr = mon.mem_get(0xDC00, 1000, bank=BANK_RAM)
                if launched is None and mon.mem_get(0x0900, 10, bank=BANK_RAM) == head:
                    launched = time.time()
                    print("--- %.0fs: the app is loaded at $0900" % (launched - t0))
                    # FROM HERE, STOP ON EVERY REU TRANSFER: a store to $DF01
                    # with bit 7 set starts one. The first captured run found
                    # RAM overwritten with untouched REU memory right after
                    # stage 2, and before the game makes a transfer of its
                    # own -- so whose transfer, and with what registers.
                    # C64 OS'S SYSTEM PAGE AS THE APP ARRIVES: drivers ($0801-
                    # $0808), the page map ($0809-$08A0) and the shared-library
                    # table ($08A2-$08C9). The first traced exception was C64
                    # OS finding no four free pages for memory.lib, and the
                    # mouse driver's page ($6F) was INSIDE the game's file.
                    print("--- C64 OS system page at launch:")
                    print(tm.ask("m 0800 08cf").strip())
                    tm.ask("bank io")
                    tm.ask("watch store df01")
                    tm.ask("bank cpu")
                    # AND C64 OS'S EXCEPTION ENTRY. The first traced wipe was
                    # the END of a cascade: a BRK with no catch registered
                    # sends C64 OS's handler to a garbage catch point, which
                    # BRKs again. The first BRK is the cause.
                    tm.ask("break exec 03df")
                    tm.sock.sendall(b"x\n")
                    dmas = excs = 0
                    print("--- tracer armed: REU transfers ($DF01) and exceptions ($03DF)")
            except (TimeoutError, OSError):
                jam_report(tm, tm.read(1.0), time.time() - t0)
                p.send_signal(signal.SIGKILL)
                p.wait()
                return 1
            if launched:
                rt, err, pl = mon.cmd(CMD_REGISTERS_GET, bytes([0]))
                mon.resume()
                # BY REGISTER ID, not position: VICE's reply is a list of
                # (size, id, value) items, and PC (id 3) is not always 4th.
                # The first version read a fixed slot, saw $0000 three
                # seconds running while C64 OS was loading the app, and
                # called that a JAM.
                pc = -1
                for i in range(struct.unpack("<H", pl[:2])[0]):
                    it = pl[2 + i * 4:6 + i * 4]
                    if len(it) == 4 and it[1] == 3:
                        pc = it[2] | it[3] << 8
                parked = parked + 1 if pc == getattr(watch, "lastpc", -1) else 0
                watch.lastpc = pc
                if parked >= 30 and pc >= 0:        # 3 seconds at 0.1s
                    print("rig: the CPU has sat at $%04X for 3 seconds -- a JAM" % pc)
                    jam_report(tm, "", time.time() - t0)
                    p.send_signal(signal.SIGKILL)
                    p.wait()
                    return 1
                sym = sym if "sym" in dir() else symbols()
                if "c64os_stage" in sym:
                    st = mon.mem_get(sym["c64os_stage"], 1, bank=BANK_RAM)[0]
                    hd = mon.mem_get(0x0900, 10, bank=BANK_RAM) == head
                    if (st, hd) != getattr(watch, "prev", None):
                        watch.prev = (st, hd)
                        print("  %5.1fs  stage %d%s" % (time.time() - t0, st,
                              "" if hd else "   main.o IS GONE FROM $0900"))
                        if st == 50 and not getattr(watch, "banks_shown", False):
                            watch.banks_shown = True
                            bank_report(mon, sym)
                        if st == 50:
                            sound_report(mon, sym)
                        sys.stdout.flush()
                if "c64os_keylog" in sym:
                    kl = mon.mem_get(sym["c64os_keylog"], 2, bank=BANK_RAM)
                    if kl != getattr(watch, "prevkey", kl):
                        print("  %5.1fs  key: $%02X (last command key's modifiers $%02X)"
                              % (time.time() - t0, kl[0], kl[1]))
                        sound_report(mon, sym)
                        sys.stdout.flush()
                    watch.prevkey = kl
            if scr != last:
                last, since = scr, time.time()
            elif shown != hash(scr) and time.time() - since > 2:
                shown = hash(scr)
                print("--- %.0fs" % (time.time() - t0))
                for r in range(25):
                    print("|%s|" % ostext(scr[r * 40:r * 40 + 40]))
            elif launched and time.time() - max(since, launched) > quiet:
                print("--- %.0fs: still for %.0fs since the app loaded"
                      % (time.time() - t0, quiet))
                break
            # Fast once the app is loaded: the stage just before the wipe was
            # missed at half a second.
            time.sleep(0.1 if launched else 0.5)
        open(os.path.join(PORT, "build", "screen.bin"), "wb").write(last or b"")
        try:
            diagnose(mon)
        except (TimeoutError, OSError):
            print("rig: no answer to the diagnosis either")
        quit_clean(p, mon)
    except Exception:
        p.send_signal(signal.SIGKILL)
        p.wait()
        raise
    return 0


def jam_report(tm, said, when):
    """A JAM, caught in VICE's monitor: where, how it got there, and what the
    game's own state was. Everything goes to build/watch.log too."""
    print("rig: JAM after %.0fs" % when)
    print(said.strip())
    sym = symbols()
    print(tm.ask("r"))
    print(tm.ask("chis 200", quiet=3.0))
    print(tm.ask("m 0100 01ff"))
    print(tm.ask("m 004e 006d"))
    # THE REU'S OWN REGISTERS: the first captured run found RAM overwritten
    # with untouched REU memory, and whose transfer did it is written here.
    print("REU registers $DF00-$DF0A:")
    print(tm.ask("bank io") + tm.ask("m df00 df0a") + tm.ask("bank cpu"))
    if "c64os_stage" in sym:
        print("c64os_stage:")
        print(tm.ask("m %04x %04x" % (sym["c64os_stage"], sym["c64os_stage"])))
    for v in ("c64os_bank", "exit_sp", "app_zp", "os_zp"):
        if v in sym and sym[v] < 0xFFE0:
            print("%s:" % v)
            print(tm.ask("m %04x %04x" % (sym[v], sym[v] + 31)))


CMD_REGISTERS_GET = 0x31


def symbols():
    """name -> address, from the game's link map."""
    out = {}
    mp = os.path.join(PORT, "build", "trek.map")
    if os.path.exists(mp):
        for line in open(mp):
            f = line.split()
            if len(f) == 5 and not f[4].startswith("."):
                try:
                    out[f[4]] = int(f[0], 16)
                except ValueError:
                    pass
    return out


def diagnose(mon):
    """ASK THE MACHINE WHAT IT IS WAITING FOR -- every question at once,
    because each run costs Jamie a double-click. Whose code is running, what
    the KERNAL was doing, whether main.o landed, how far the game got."""
    print("rig: diagnosis")
    pcs = []
    for _ in range(12):
        rt, err, pl = mon.cmd(CMD_REGISTERS_GET, bytes([0]))
        mon.resume()
        n = struct.unpack("<H", pl[:2])[0]
        regs = {}
        for i in range(n):
            rid, val = pl[2 + i * 4 + 1], struct.unpack("<H", pl[2 + i * 4 + 2:2 + i * 4 + 4])[0]
            regs[rid] = val
        pcs.append(regs.get(3, 0))          # register id 3 is PC in VICE's list
        time.sleep(0.2)
    sym = symbols()
    inv = sorted((a, n) for n, a in sym.items())

    def where(a):
        best = None
        for sa, sn in inv:
            if sa <= a:
                best = (sa, sn)
            else:
                break
        return ("%s+$%X" % (best[1], a - best[0])) if best and a - best[0] < 0x1000 else "?"
    print("  PC samples: " + ", ".join("$%04X %s" % (pc, where(pc) if 0x0900 <= pc < 0x7A00 else "")
                                       for pc in pcs))
    hdr = mon.mem_get(0x0900, 10, bank=BANK_RAM)
    print("  $0900 header: %s" % hdr.hex(" "))
    k = mon.mem_get(0x0090, 0x30, bank=BANK_RAM)
    st, fnl, lfn, sa, dv = k[0], k[0xB7 - 0x90], k[0xB8 - 0x90], k[0xB9 - 0x90], k[0xBA - 0x90]
    fp = k[0xBB - 0x90] | k[0xBC - 0x90] << 8
    name = (mon.mem_get(fp, fnl, bank=BANK_RAM)
            if fnl and fp + fnl <= 0x10000 else b"")
    print("  KERNAL: ST $%02X  LFN %d SA %d DEV %d  name %r" % (st, lfn, sa, dv, name))
    for v, n in (("c64os_bank", 1), ("c64os_dev", 1), ("prefix", 40), ("far_len", 2),
                 ("ovl_live", 1), ("ready", 1), ("kb_entropy", 2), ("__ovl_start", 2)):
        # Overlay symbols carry their staging address above 64K in the map;
        # only what the C64 can see is read.
        if v in sym and sym[v] + n <= 0x10000:
            b = mon.mem_get(sym[v], n, bank=BANK_RAM)
            shown = b.split(b"\0")[0] if n > 2 else (b[0] if n == 1 else b[0] | b[1] << 8)
            print("  %-12s $%04X = %r" % (v, sym[v], shown))
    pm = mon.mem_get(0x0809, 0x98, bank=BANK_RAM)
    print("  page map $09-$A0: %s" % pm.hex())
    rv = mon.mem_get(0x0281, 6, bank=BANK_RAM)
    print("  REU: first app bank %d, app freeze bank %d" % (rv[1], rv[5]))


class Tee(object):
    """Everything printed also goes to build/watch.log, so a run's findings
    survive the terminal it ran in -- Jamie runs these, and the first JAM at
    $0106 was reported in words because the output was not kept."""
    def __init__(self, path):
        self.f = open(path, "w")
        self.out = sys.stdout

    def write(self, t):
        self.out.write(t)
        self.f.write(t)
        self.f.flush()

    def flush(self):
        self.out.flush()
        self.f.flush()


def main():
    os.makedirs(os.path.join(PORT, "build"), exist_ok=True)
    if sys.argv[1:2] == ["loudness"]:       # beside watch.log, not over it
        return loudness()
    sys.stdout = Tee(os.path.join(PORT, "build", "watch.log"))
    a = [x for x in sys.argv[1:] if not x.startswith("--")]
    opt = {x.split("=")[0]: (x.split("=", 1) + [""])[1] for x in sys.argv[1:]
           if x.startswith("--")}
    if a[:1] == ["install"] and len(a) == 3:
        return install(a[1], a[2])
    if a[:1] == ["play"]:
        return play()
    if a[:1] == ["watch"]:
        return watch(float(opt.get("--wait") or 900), float(opt.get("--quiet") or 30))
    if a[:1] == ["boot"]:
        key = [int(k, 16) for k in opt["--key"].split(",")] if opt.get("--key") else []
        human = "--human" in opt
        return boot(float(opt.get("--wait") or (600 if human else 90)), key,
                    "--jam" in opt, human)
    sys.exit(__doc__.strip().splitlines()[2])


if __name__ == "__main__":
    sys.exit(main())
