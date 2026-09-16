#!/usr/bin/env python3
"""Cut the eleven overlay images out of the ELF into one OVERLAYS.BIN.

FIXED 4,096-BYTE SLOTS, so ovl_load indexes with a multiply and reads eight
blocks. The Atari packs its images by actual size because thirteen padded
windows overflow the 16-bit offsets of its far store; this port reads from the
disk, where the only cost of padding is blocks nobody is short of.

AND EVERY SLOT CARRIES A STAMP -- the low sixteen bits of ovl_load's address in
the link the image was cut from, at offset 4094. An overlay is linked WITH the
resident half, so yesterday's images beside today's program jump into the
middle of some other function. On the MEGA65 a stale OVERLAYS.BIN reset the
machine to BASIC and could mimic any bug you cared to name; on the C128 a
missing eleventh image opened the game on an uninitialised galaxy, having
first shown the SAVE GAME dialog, because that is what lived at that address
in the image that was actually in the window.
"""
import os
import subprocess
import sys

NM = os.path.expanduser("~/llvm-mos/bin/llvm-nm")
OBJCOPY = os.path.expanduser("~/llvm-mos/bin/llvm-objcopy")
SLOT = 4096
STAMP_AT = SLOT - 2
NAMES = ["eval", "hof", "front", "info", "repair", "msgs", "planet", "cmds",
         "title", "events", "xtra"]


def main(argv):
    if len(argv) != 3:
        raise SystemExit("usage: cut_overlays.py IMAGE.elf OUT.bin")
    elf, out = argv[1], argv[2]

    syms = subprocess.run([NM, elf], capture_output=True, text=True).stdout
    stamp = None
    for line in syms.splitlines():
        p = line.split()
        if len(p) == 3 and p[2] == "ovl_load":
            stamp = int(p[0], 16) & 0xFFFF
    if stamp is None:
        raise SystemExit(f"{elf}: no ovl_load symbol to stamp the images with")

    blob = bytearray()
    for i, n in enumerate(NAMES):
        raw = f"/tmp/ovl_{n}.raw"
        r = subprocess.run([OBJCOPY, f"--dump-section=.ovl_{n}={raw}", elf,
                            "/dev/null"], capture_output=True, text=True)
        if r.returncode or not os.path.exists(raw):
            raise SystemExit(f"{elf}: no .ovl_{n} section -- was it built with "
                             f"-DTREK_OVERLAYS?")
        d = open(raw, "rb").read()
        if len(d) > STAMP_AT:
            raise SystemExit(f"overlay {n} is {len(d)} bytes; the slot holds "
                             f"{STAMP_AT} plus a two-byte stamp")
        slot = bytearray(SLOT)
        slot[0:len(d)] = d
        slot[STAMP_AT] = stamp & 0xFF
        slot[STAMP_AT + 1] = (stamp >> 8) & 0xFF
        blob += slot
        print(f"  overlay {i:2d} {n:<7} {len(d):5d} bytes of {STAMP_AT}")

    open(out, "wb").write(bytes(blob))
    print(f"{out}: {len(NAMES)} slots of {SLOT}, {len(blob)} bytes, "
          f"stamp ${stamp:04X}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
