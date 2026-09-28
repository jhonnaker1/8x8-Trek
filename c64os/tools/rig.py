#!/usr/bin/env python3
"""Drive C64 OS in VICE with nobody at the mouse.

    rig.py install BUNDLE APP            copy BUNDLE/{main.o,menu.m,about.t}
                                         to //os/applications/APP on the work
                                         disk and verify every byte
    rig.py boot [--key=HEX] [--wait=S] [--jam] [--human]
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
FS8 = os.path.join(PORT, "build", "fs8")
BANK_RAM = 1
MAGIC = b"EGATREKPROBE!"
CMD_QUIT = 0xBB


TEXTMON = 6510     # VICE's text monitor, for CPU history after a JAM


def vice(keybuf, warp=True, jam=False):
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
        "-binarymonitor", "-binarymonitoraddress",
        "ip4://127.0.0.1:%d" % vice_mon.PORT, "-keybuf", keybuf]
    if jam:
        # A JAM drops into the monitor instead of a dialog on Jamie's screen,
        # and the CPU history stops where the CPU did.
        cmd += ["-jamaction", "2", "-remotemonitor",
                "-remotemonitoraddress", "127.0.0.1:%d" % TEXTMON]
    return subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


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
    for f in ("main.o", "menu.m", "about.t"):
        shutil.copy(os.path.join(bundle, f), os.path.join(FS8, f))
    bas = open(os.path.join(HERE, "install.bas")).read()
    bas = bas.replace("{APP}", app)
    open(os.path.join(FS8, "install.txt"), "w").write(bas)
    subprocess.check_call(["petcat", "-w2", "-o", os.path.join(FS8, "install"),
                           "--", os.path.join(FS8, "install.txt")],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.remove(os.path.join(FS8, "install.txt"))
    want = {f: os.path.getsize(os.path.join(bundle, f))
            for f in ("main.o", "menu.m", "about.t")}

    p = vice('load"install",8\nrun\n')
    try:
        time.sleep(4)
        mon = vice_mon.Mon(check=False)
        t0, lines = time.time(), []
        while time.time() - t0 < 300:
            s = mon.mem_get(0x0400, 1000)
            lines = [text(s[r * 40:r * 40 + 40]).strip() for r in range(25)]
            if any(l == "DONE" for l in lines):
                break
            time.sleep(1)
        quit_clean(p, mon)
    except Exception:
        p.send_signal(signal.SIGKILL)
        p.wait()
        raise
    ok = True
    for f, n in want.items():
        # The installer counts the byte GET# returns at end of file as one
        # more, so a file of n bytes reads back as n+1 -- uno's check does
        # the same, and what matters is that the two agree and nothing differs.
        hit = [l for l in lines if l.startswith(f.upper() + " ")]
        good = bool(hit) and "OK" in hit[0] and not "DIFFERS" in hit[0]
        print("  %-8s %s" % (f, hit[0] if hit else "NOT REPORTED"))
        ok = ok and good
    if not any(l == "DONE" for l in lines):
        print("  the installer never printed DONE; the screen was:")
        for l in lines:
            print("    |%s|" % l)
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
            if key is not None:
                # C64 OS's printable-key event buffer and its count; the
                # machine is stopped between the two writes and the resume.
                for addr, val in ((0x0277, key), (0x00C6, 1)):
                    body = (bytes([0]) + struct.pack("<HH", addr, addr) + bytes([0])
                            + struct.pack("<H", BANK_RAM) + bytes([val]))
                    mon.cmd(vice_mon.CMD_MEM_SET, body)
                mon.resume()
                time.sleep(3)
            r = mon.mem_get(rec, len(MAGIC) + 8, bank=BANK_RAM)[len(MAGIC):]
            print("rig: probe record at $%04X, found after %.0fs" % (rec, took))
            print("  inits %d  draws %d  keys %d  last key $%02X" %
                  (r[0], r[1] | r[2] << 8, r[3], r[4]))
            print("  $01 at init $%02X, in draw $%02X, in kprnt $%02X" % (r[5], r[6], r[7]))
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


def main():
    a = [x for x in sys.argv[1:] if not x.startswith("--")]
    opt = {x.split("=")[0]: (x.split("=", 1) + [""])[1] for x in sys.argv[1:]
           if x.startswith("--")}
    if a[:1] == ["install"] and len(a) == 3:
        return install(a[1], a[2])
    if a[:1] == ["boot"]:
        key = int(opt["--key"], 16) if opt.get("--key") else None
        human = "--human" in opt
        return boot(float(opt.get("--wait") or (600 if human else 90)), key,
                    "--jam" in opt, human)
    sys.exit(__doc__.strip().splitlines()[2])


if __name__ == "__main__":
    sys.exit(main())
