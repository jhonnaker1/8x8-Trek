#!/usr/bin/env python3
"""Build a ProDOS-order (.po) 3.5" disk image for the IIgs.

800K is 1,600 blocks of 512 bytes, stored in block order with no interleave --
which is what ".po" means and why it is the format to write by hand. A 5.25"
".do" image would need DOS 3.3 sector skewing; none of that applies here.

RIGHT NOW THIS WRITES A BOOT BLOCK AND NOTHING ELSE. There is no ProDOS on the
disk and no filesystem: block 0 is ours, the machine runs it, and what it does
next is the boot block's business. See README.md for why the port is going
this way rather than through ProDOS.
"""
import argparse
import sys

BLOCK = 512
BLOCKS_800K = 1600


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--boot", required=True,
                    help="block 0 image, at most 512 bytes")
    ap.add_argument("--payload",
                    help="raw image written from --payload-block onward")
    ap.add_argument("--payload-block", type=int, default=1)
    ap.add_argument("--blocks", type=int, default=BLOCKS_800K)
    a = ap.parse_args(argv[1:])

    boot = bytearray(open(a.boot, "rb").read())
    if len(boot) > BLOCK:
        raise SystemExit(f"{a.boot}: {len(boot)} bytes, block 0 holds {BLOCK}")

    img = bytearray(a.blocks * BLOCK)

    n = 0
    p = b""
    if a.payload:
        p = open(a.payload, "rb").read()
        n = (len(p) + BLOCK - 1) // BLOCK
        off = a.payload_block * BLOCK
        if off + len(p) > len(img):
            raise SystemExit(f"payload does not fit: needs {n} blocks from "
                             f"{a.payload_block}, disk has {a.blocks}")
        img[off:off + len(p)] = p

    # PATCH THE BLOCK COUNT BY SIGNATURE, NOT BY OFFSET. An offset in this file
    # and an offset in src/boot.s are two numbers that have to agree, and the
    # day they stop agreeing this writes a byte into the middle of an
    # instruction and the disk fails to boot for a reason nothing points at.
    sig = b"GSBOOT1"
    i = boot.find(sig)
    if i < 0:
        raise SystemExit(f"{a.boot}: no {sig.decode()} signature -- the boot "
                         f"block cannot be told how much to load")
    if boot.find(sig, i + 1) >= 0:
        raise SystemExit(f"{a.boot}: {sig.decode()} appears more than once")
    if n > 255:
        raise SystemExit(f"payload is {n} blocks; the count is one byte")
    boot[i + len(sig)] = n if n else 1

    img[0:len(boot)] = boot

    open(a.out, "wb").write(img)
    print(f"{a.out}: {a.blocks} blocks, boot {len(boot)} bytes"
          + (f", payload {len(p)} bytes in {n} blocks from block "
             f"{a.payload_block}" if a.payload else ", no payload"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
