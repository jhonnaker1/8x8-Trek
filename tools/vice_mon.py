#!/usr/bin/env python3
"""Drive VICE from a script through its BINARY monitor.

This exists because the obvious route does not work. VICE's *remote* (text)
monitor halts emulation the moment something connects, which wedges the GUI --
that was tried in August, cost about eight tool calls, and got written up as
"VICE cannot be captured from a session here". That conclusion was wrong. The
binary monitor is a separate interface and it does not halt.

Launch the emulator with it enabled:

    x128 -binarymonitor -binarymonitoraddress ip4://127.0.0.1:6502 \
         -autostart build/trek128.prg

or just `make monitor` in c128/. Then:

    python3 tools/vice_mon.py shot:console
    python3 tools/vice_mon.py mem:1c01,16
    python3 tools/vice_mon.py key:M shot:after
    python3 tools/vice_mon.py sym:_ship
    python3 tools/vice_mon.py live          # is the machine actually running?

THE ONE TRAP, AND IT COST A WHOLE SESSION

The binary monitor STOPS the emulated machine on its first command and leaves
it stopped. Nothing tells you: reads keep succeeding, they just all return the
same frozen snapshot, the CPU never advances, and anything you inject is never
processed. It looks exactly like a program ignoring you, and it produced a
string of wrong conclusions on 2026-08-19 -- "input injection does not reach
this port", "the machine is hung", "the port never consumes the byte" -- all
of them artefacts of a stopped machine.

Mon.resume() sends the EXIT command that lets it run again, and mem_get,
mem_set and screenshot all call it. The proof it is real: the KERNAL jiffy
clock at $A0, which the IRQ bumps 60 times a second, does not move between
plain reads and advances 25 ticks per 0.4s once resume() is called between
them.

WHAT WORKS

  * Screenshots of the VDC's 80-column display. The C128 port can be checked
    without asking a human to look at it.
  * Memory read and write, so a game state can be set up directly -- damage a
    system and screenshot SYSTEMS STATUS rather than playing to that state.
  * Scripted keypresses, via `key:`, against a `make monitor` build.

    VICE's own KEYBOARD_FEED fills the KERNAL's keyboard buffer, and
    c128/src/input.c scans the CIA1 matrix directly, so fed keys do not reach
    the port. kb_inject is the way round it: a byte the port tests while it
    waits, compiled in only under -DTREK_DEBUG_INPUT so the release binary has
    no such affordance. Measured at 0.02s from poke to consumption.

There is an MCP server (github.com/axewater/mcp-vice-emu) wrapping this same
protocol. It needs Node and a session restart; this needs neither.
"""
import os
import socket
import struct
import sys
import time

STX, API = 0x02, 0x02

CMD_MEM_GET       = 0x01
CMD_MEM_SET       = 0x02
CMD_REGISTERS_GET = 0x31
CMD_KEYBOARD_FEED = 0x72   # see the warning above
CMD_PING          = 0x81
CMD_DISPLAY_GET   = 0x84
CMD_EXIT          = 0xaa

HOST = os.environ.get("VICE_MON_HOST", "127.0.0.1")
PORT = int(os.environ.get("VICE_MON_PORT", "6502"))
OUT  = os.environ.get("VICE_SHOT_DIR", ".")
KEY_SETTLE = float(os.environ.get("VICE_KEY_SETTLE", "0.35"))

# Where `make` and `make monitor` leave their link maps, newest first. The map
# is the only place a symbol's address is written down.
MAPS = ("c128/build/trek128-debug.map", "build/trek128-debug.map",
        "c128/build/trek128.map", "build/trek128.map")

PALETTES = (
    "/usr/local/share/vice/C128/vdc_deft.vpl",
    "/opt/homebrew/share/vice/C128/vdc_deft.vpl",
    "/usr/share/vice/C128/vdc_deft.vpl",
)


class Mon(object):
    def __init__(self, host=HOST, port=PORT, timeout=15, check=True):
        self.s = socket.create_connection((host, port), timeout=timeout)
        self.rid = 0
        # Checked on connect, deliberately. Saying "remember to check" in a
        # comment is not carrying the lesson forward; every wrong conclusion
        # of 2026-08-19 would have been caught by this in the first minute.
        if check and not self.is_live():
            sys.stderr.write(
                "vice_mon: WARNING -- the emulated machine is NOT running.\n"
                "  The jiffy clock at $A0 did not advance between two reads.\n"
                "  Everything read while stopped is a frozen snapshot, and\n"
                "  injected input is never processed. Usually this means a\n"
                "  monitor command left it stopped: call Mon.resume().\n"
                "  (A paused emulator, or a program that has taken over the\n"
                "  KERNAL IRQ, will also trip this -- see is_live().)\n")

    def _recv(self, n):
        buf = b""
        while len(buf) < n:
            chunk = self.s.recv(n - len(buf))
            if not chunk:
                raise EOFError("VICE closed the monitor connection")
            buf += chunk
        return buf

    def cmd(self, ctype, body=b""):
        """Send one command and return (response_type, error, payload).

        Raw: does NOT resume the machine afterwards. Prefer mem_get, mem_set
        and screenshot, which do. VICE also pushes unsolicited frames --
        notably STOPPED and RESUMED -- interleaved with replies, so matching
        on the request id and discarding the rest is what keeps this from
        desynchronising.
        """
        self.rid += 1
        want = self.rid
        self.s.sendall(bytes([STX, API]) + struct.pack("<II", len(body), want)
                       + bytes([ctype]) + body)
        while True:
            hdr = self._recv(12)
            if hdr[0] != STX:
                raise IOError("bad frame from VICE: %r" % hdr)
            blen, rtype, err, rid = struct.unpack("<IBBI", hdr[2:12])
            payload = self._recv(blen) if blen else b""
            if rid == want:
                return rtype, err, payload

    def is_live(self, gap=0.2, addr=0xA0):
        """True if the emulated CPU is able to execute.

        Reads the KERNAL jiffy clock at $A0 -- incremented 60 times a second
        by the IRQ -- twice, and reports whether it moved.

        NOTE WHAT THIS DOES AND DOES NOT CATCH. mem_get resumes the machine,
        so this cannot detect "a previous command left it stopped": it would
        restart it and then report healthy. That case is designed out
        instead -- every operation in this class resumes -- so it cannot
        arise through this API. What it DOES catch is a machine that cannot
        run at all: paused in the UI, wedged, or a raw self.cmd() call in
        someone else's script that bypassed resume() and then hung.

        Caveat: it measures "the KERNAL IRQ is running", not strictly "the
        CPU is running". A program that took over the IRQ vector would read
        as not-live; nothing in this project does. Pass `addr` to watch a
        location such a program changes itself instead.
        """
        a = self.mem_get(addr, 3)
        time.sleep(gap)
        return self.mem_get(addr, 3) != a

    def resume(self):
        """Let the emulator run again.

        THE trap of this protocol, and it cost a whole session before it was
        found. The binary monitor STOPS the machine on its first command and
        stays stopped. Nothing says so: reads keep succeeding, they just all
        return the same frozen snapshot, and the CPU never advances. The
        symptom looks exactly like a program that is ignoring you.

        Proof it is real: the KERNAL jiffy clock at $A0, which the IRQ bumps
        60 times a second, does not move between plain reads and advances 25
        ticks per 0.4s once this is called between them.
        """
        self.cmd(CMD_EXIT)

    def ping(self):
        return self.cmd(CMD_PING)[1] == 0

    def mem_get(self, start, length, bank=0):
        end = start + length - 1
        body = (bytes([0]) + struct.pack("<HH", start, end)
                + bytes([0]) + struct.pack("<H", bank))
        rtype, err, p = self.cmd(CMD_MEM_GET, body)
        self.resume()
        if err:
            raise IOError("mem_get error %d" % err)
        n = struct.unpack("<H", p[0:2])[0]
        return p[2:2 + n]

    def mem_set(self, start, data, bank=0):
        end = start + len(data) - 1
        body = (bytes([0]) + struct.pack("<HH", start, end)
                + bytes([0]) + struct.pack("<H", bank) + bytes(data))
        rtype, err, p = self.cmd(CMD_MEM_SET, body)
        self.resume()
        if err:
            raise IOError("mem_set error %d" % err)


def symbol(name, maps=MAPS):
    """Address of a linked symbol, from the cl65 map file.

    cc65 lists a symbol under "Exports list" only if another module imports
    it, so a global used in just one translation unit will not appear here
    however global it looks. main.c touches kb_inject once for exactly this
    reason.
    """
    if not name.startswith("_"):
        name = "_" + name
    for path in maps:
        if not os.path.exists(path):
            continue
        for line in open(path):
            for i, tok in enumerate(line.split()):
                if tok == name:
                    parts = line.split()
                    if i + 1 < len(parts):
                        try:
                            return int(parts[i + 1], 16), path
                        except ValueError:
                            pass
    raise KeyError("%s not found in any of %s -- build with `make monitor`?"
                   % (name, ", ".join(maps)))


def _palette():
    for path in PALETTES:
        if os.path.exists(path):
            out = []
            for line in open(path):
                line = line.split("#")[0].strip()
                if not line:
                    continue
                r, g, b = (int(x, 16) for x in line.split()[:3])
                out.append((r, g, b))
            return out
    # VDC default, in case VICE's data directory is somewhere unexpected.
    return [(0, 0, 0), (85, 85, 85), (0, 0, 170), (85, 85, 255),
            (0, 170, 0), (85, 255, 85), (0, 170, 170), (85, 255, 255),
            (170, 0, 0), (255, 85, 85), (170, 0, 170), (255, 85, 255),
            (170, 85, 0), (255, 255, 85), (170, 170, 170), (255, 255, 255)]


def screenshot(mon, path, use_vicii=0, scale=1):
    """Grab the current display as a PNG; returns (width, height).

    use_vicii=0 is the VDC's 80-column screen, which is what this port draws
    on. use_vicii=1 gives the 40-column VIC-II screen instead.
    """
    from PIL import Image

    rtype, err, p = mon.cmd(CMD_DISPLAY_GET, bytes([use_vicii, 0x00]))
    mon.resume()
    if err:
        raise IOError("display_get error %d" % err)

    flen = struct.unpack("<I", p[0:4])[0]
    dw, dh, xo, yo, iw, ih = struct.unpack("<HHHHHH", p[4:16])

    # THE TRAP: the image data follows the field block immediately. Parsing a
    # separate uint32 data-length after the fields -- which is what the field
    # layout suggests -- leaves the buffer exactly four bytes short and PIL
    # then refuses the frame. len(payload) - (4 + flen) is the pixel count.
    data = p[4 + flen:]
    if len(data) < dw * dh:
        raise IOError("short frame: got %d bytes, need %d" % (len(data), dw * dh))

    im = Image.frombytes("P", (dw, dh), data[:dw * dh])
    flat = []
    for c in _palette():
        flat += list(c)
    flat += [0, 0, 0] * (256 - len(flat) // 3)
    im.putpalette(flat)
    im = im.convert("RGB")

    if iw and ih and (iw != dw or ih != dh):
        im = im.crop((xo, yo, xo + iw, yo + ih))
    if scale > 1:
        im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)

    im.save(path)
    return im.size


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__.strip().splitlines()[0] + "\n\nSee --help / the "
                 "docstring at the top of this file.")
    if args[0] in ("-h", "--help"):
        print(__doc__)
        return

    mon = Mon()
    for arg in args:
        if arg.startswith("shot:"):
            name = arg[5:]
            path = os.path.join(OUT, name + ".png")
            print("%s %dx%d" % ((path,) + screenshot(mon, path)))
        elif arg.startswith("mem:"):
            spec = arg[4:].split(",")
            start = int(spec[0], 16)
            length = int(spec[1]) if len(spec) > 1 else 16
            data = mon.mem_get(start, length)
            for off in range(0, len(data), 16):
                row = data[off:off + 16]
                text = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
                print("%04X  %-47s  %s"
                      % (start + off, " ".join("%02X" % b for b in row), text))
        elif arg.startswith("poke:"):
            addr, _, vals = arg[5:].partition("=")
            mon.mem_set(int(addr, 16), bytes(int(v, 16) for v in vals.split(",")))
            print("poked %s" % addr)
        elif arg.startswith("key:"):
            text = arg[4:]
            addr, path = symbol("kb_inject")
            if "debug" not in path:
                sys.exit("kb_inject came from %s, which is a release map. "
                         "Scripted input needs `make monitor`." % path)
            for ch in text:
                mon.mem_set(addr, bytes([ord(ch)]))
                # A fixed settle, NOT a read-back handshake. The port does
                # clear the byte after consuming it -- keys demonstrably
                # arrive one per write -- but a MEM_GET afterwards keeps
                # returning the old value, so the port's own store is not
                # visible through the monitor. Rather than trust a read that
                # has been observed to lie, give the port time to poll. At
                # 2MHz it is around the loop far faster than this.
                time.sleep(KEY_SETTLE)
            print("typed %r" % text)
        elif arg.startswith("sym:"):
            addr, path = symbol(arg[4:])
            print("%s = $%04X  (%s)" % (arg[4:], addr, path))
        elif arg == "live":
            print("live:", "yes -- machine is running"
                           if mon.is_live() else "NO -- machine is stopped")
        elif arg == "ping":
            print("ping:", "ok" if mon.ping() else "FAILED")
        else:
            sys.exit("unknown action: %s" % arg)


if __name__ == "__main__":
    main()
