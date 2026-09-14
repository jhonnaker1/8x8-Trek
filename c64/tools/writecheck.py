#!/usr/bin/env python3
"""Run the write probe and report what the DRIVE says, not the host image."""
import os, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
C64 = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(os.path.dirname(C64), "tools"))

D64 = os.path.join(C64, "build", "writetest.d64")
STOR = {0: "STOR_OK", 1: "STOR_NOTFOUND", 2: "STOR_ERROR"}


def main():
    kill = "--kill" in sys.argv
    vice = subprocess.Popen(
        ["x64sc", "-binarymonitor",
         "-binarymonitoraddress", "ip4://127.0.0.1:6502",
         "-autostart", D64 + ":writetest"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    rc = 1
    try:
        time.sleep(16)
        import vice_mon
        mon = vice_mon.Mon()
        r = mon.mem_get(0x0340, 32)

        # ASK WHETHER THE INSTRUMENT WAS ARMED, before reading anything it
        # says. $0340 is ordinary RAM and holds whatever was there before.
        if r[31] != 0x5A:
            print("writecheck: THE PROBE DID NOT FINISH (stamp $%02X, want $5A)"
                  % r[31])
            print("            Nothing below this line means anything.")
            return 1

        got = (r[2] << 8) | r[3]
        print("  write  %s" % STOR.get(r[0], "?%d" % r[0]))
        print("  read   %s, %d bytes (wrote 600)" % (STOR.get(r[1], "?%d" % r[1]), got))
        if r[4]:
            print("  compare MATCHES over all 600 bytes")
        else:
            print("  compare DIFFERS, first at byte %d" % ((r[5] << 8) | r[6]))

        ok = r[0] == 0 and r[1] == 0 and got == 600 and r[4] == 1
        print("writecheck: %s" % ("the drive wrote it and read it back"
                                  if ok else "FAILED"))
        rc = 0 if ok else 1
    finally:
        if kill:
            vice.send_signal(signal.SIGKILL)
            vice.wait()
        else:
            print("  VICE IS STILL RUNNING (pid %d)." % vice.pid)
    return rc


if __name__ == "__main__":
    sys.exit(main())
