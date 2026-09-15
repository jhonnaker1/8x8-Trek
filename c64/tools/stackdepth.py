#!/usr/bin/env python3
"""How deep does the soft stack actually go on this machine?

THE LINKER CANNOT SEE A RUNTIME STACK. c64.ld reserves $BF00..$BFFF -- 256
bytes below __stack at $C000 -- and says in as many words "re-measure here
rather than assume the C128's number transfers". Nobody had, and this port is
RELEASED.

It is not a theoretical worry. The C128 shipped a 64-byte guard in v0.9.0 on
the stated assumption that "the soft stack is for recursion and alloca,
neither of which this code uses". The soft stack is in constant use: the
measured deepest path was 143 bytes, so that guard was overrun by 79 -- into
the overlay window, whose next load wrote 4K of code over the live return
addresses. Quitting BRKed into the monitor. Reported by Jamie against a
release.

THE METHOD: fill the unused RAM BELOW the current stack pointer with a
sentinel, drive the game to the deepest path anyone has found (the hall of
fame, through the evaluation), then find the lowest byte that is no longer the
sentinel. llvm-mos keeps the soft stack pointer in __rc0/__rc1, which this
build places at $0002/$0003 -- read, never assumed.
"""
import signal, subprocess, sys, time

C64 = "/Users/jhonnaker/claude-code/egatrek/c64"
sys.path.insert(0, "/Users/jhonnaker/claude-code/egatrek/tools")
sys.path.insert(0, C64 + "/tools")

D64 = C64 + "/build/trek64-debug.d64"
MAP = C64 + "/build/trek64-debug.map"
SENTINEL = 0xA5
FILL_LO = 0xA800          # above the program's top ($A720), below everything
STACK_TOP = 0xC000        # __stack, from c64.ld


def sym(n):
    for l in open(MAP):
        f = l.split()
        if len(f) == 5 and f[4] == n:
            return int(f[0], 16)
    sys.exit("stackdepth: %s not in the map" % n)


def main():
    kill = "--kill" in sys.argv
    kb = sym("kb_inject")
    v = subprocess.Popen(
        ["x64sc", "-binarymonitor", "-binarymonitoraddress", "ip4://127.0.0.1:6502",
         "-autostart", D64 + ":trek64"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(18)
        import vice_mon
        mon = vice_mon.Mon()

        def live():
            return sum(1 for b in mon.mem_get(0x0400, 1000) if b not in (32, 0))

        def quiet(t=30):
            last, st, t0 = -1, 0, time.time()
            while time.time() - t0 < t:
                n = live()
                st = st + 1 if n == last else 0
                last = n
                if st >= 13:
                    return n
                time.sleep(0.4)
            return last

        quiet()
        sp = mon.mem_get(0x0002, 2)
        sp = sp[0] | (sp[1] << 8)
        print("  soft stack pointer ($0002) at the title: $%04X" % sp)
        print("  __stack is $%04X, so %d bytes are in use right now"
              % (STACK_TOP, STACK_TOP - sp))
        if not (FILL_LO < sp <= STACK_TOP):
            sys.exit("stackdepth: SP $%04X is outside the expected range -- "
                     "the fill would be meaningless" % sp)

        n = sp - FILL_LO
        mon.mem_set(FILL_LO, bytes([SENTINEL]) * n)
        print("  filled $%04X..$%04X with $%02X (%d bytes)"
              % (FILL_LO, sp - 1, SENTINEL, n))

        # THE DEEPEST PATH ANYONE HAS FOUND, which on the C128 was the
        # evaluation and the hall of fame rather than the console.
        for keys in ["\r", "n\r", "n\r", "\r", "1\r", "ABC\r",
                     "Q\r", "Y", "\r", "\r"]:
            for ch in keys:
                mon.mem_set(kb, bytes([ord(ch)]))
                time.sleep(0.45)
            quiet()

        block = mon.mem_get(FILL_LO, STACK_TOP - FILL_LO)
        low = None
        for i, b in enumerate(block):
            if b != SENTINEL:
                low = FILL_LO + i
                break
        if low is None:
            print("  NOTHING below $%04X was touched -- the fill is intact, "
                  "which means the drive never got deep. Suspect the run, not "
                  "the stack." % STACK_TOP)
            return 1
        print("  deepest byte written: $%04X" % low)
        print("  MAX DEPTH %d bytes below __stack" % (STACK_TOP - low))
        guard = 256
        print("  guard is %d bytes -> %d spare (%s)"
              % (guard, guard - (STACK_TOP - low),
                 "SAFE" if STACK_TOP - low < guard else "OVERRUN"))
        return 0 if STACK_TOP - low < guard else 1
    finally:
        if kill:
            v.send_signal(signal.SIGKILL); v.wait()
        else:
            print("  VICE still running (pid %d)" % v.pid)


if __name__ == "__main__":
    sys.exit(main())
