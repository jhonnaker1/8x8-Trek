#!/usr/bin/env python3
"""Build an 800K ProDOS-order (.po) disk for the IIgs, with our own directory.

800K is 1,600 blocks of 512 bytes stored in block order with no interleave --
which is what ".po" means and why it is the format to write by hand.

THERE IS NO PRODOS ON THIS DISK. Every block is ours:

    block 0        the loader (src/boot.S), which the firmware runs
    block 1        directory, 32 entries of 16 bytes
    block 2..      the game image, loaded by block 0
    then           file data, each file CONTIGUOUS

An entry is name[11] (space padded, "NAME    EXT"), start block (16-bit LE),
length in bytes (16-bit LE), flags. Flags bit 0 = used, bit 1 = writable slot.

SLOTS EXIST BECAUSE THE SAVE'S NAME IS TYPED BY THE PLAYER and cannot be known
when the disk is built. A slot is an entry with an extent already assigned and
no name; the port's dir_claim() takes one on the first write to a name that is
not there. That is the whole allocator -- it cannot fragment, because nothing
is ever freed and every slot is the same size. The flag also makes a write to
STRINGS.DAT refused BY THE FORMAT rather than by nobody having tried it.
"""
import argparse
import sys

BLOCK = 512
BLOCKS_800K = 1600
DIR_BLOCK = 1
PAYLOAD_BLOCK = 2
ENTRIES = BLOCK // 16
SIG = b"GSBOOT1"


def entry_name(name):
    base, _, ext = name.partition(".")
    return (base[:8].ljust(8) + ext[:3].ljust(3)).upper().encode("ascii")


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--boot", required=True, help="block 0, at most 512 bytes")
    ap.add_argument("--payload", help="the game image, loaded from block 2")
    ap.add_argument("--file", action="append", default=[],
                    metavar="NAME=PATH", help="a data file, repeatable")
    ap.add_argument("--slots", type=int, default=0,
                    help="writable slots to lay down")
    ap.add_argument("--slot-blocks", type=int, default=4,
                    help="blocks per slot (default 4 = 2,048 bytes)")
    ap.add_argument("--blocks", type=int, default=BLOCKS_800K)
    a = ap.parse_args(argv[1:])

    boot = bytearray(open(a.boot, "rb").read())
    if len(boot) > BLOCK:
        raise SystemExit(f"{a.boot}: {len(boot)} bytes, block 0 holds {BLOCK}")

    img = bytearray(a.blocks * BLOCK)
    dirent = bytearray(BLOCK)
    used = 0
    next_block = PAYLOAD_BLOCK
    report = []

    payload_blocks = 0
    if a.payload:
        p = open(a.payload, "rb").read()
        payload_blocks = (len(p) + BLOCK - 1) // BLOCK
        img[PAYLOAD_BLOCK * BLOCK:PAYLOAD_BLOCK * BLOCK + len(p)] = p
        next_block += payload_blocks
        report.append(f"image {len(p)} bytes in {payload_blocks} blocks "
                      f"from {PAYLOAD_BLOCK}")

    def add_entry(name, start, length, flags):
        nonlocal used
        if used >= ENTRIES:
            raise SystemExit(f"directory is full at {ENTRIES} entries")
        off = used * 16
        dirent[off:off + 11] = entry_name(name) if name else b" " * 11
        dirent[off + 11] = start & 0xFF
        dirent[off + 12] = start >> 8
        dirent[off + 13] = length & 0xFF
        dirent[off + 14] = length >> 8
        dirent[off + 15] = flags
        used += 1

    for spec in a.file:
        name, _, path = spec.partition("=")
        if not path:
            raise SystemExit(f"--file wants NAME=PATH, got {spec!r}")
        data = open(path, "rb").read()
        if len(data) > 0xFFFF:
            raise SystemExit(f"{path}: {len(data)} bytes; the length field is "
                             f"16 bits")
        n = (len(data) + BLOCK - 1) // BLOCK
        if (next_block + n) > a.blocks:
            raise SystemExit(f"{name} does not fit: needs {n} blocks from "
                             f"{next_block}, disk has {a.blocks}")
        img[next_block * BLOCK:next_block * BLOCK + len(data)] = data
        add_entry(name, next_block, len(data), 1)
        report.append(f"{name} {len(data)} bytes in {n} blocks from "
                      f"{next_block}")
        next_block += n

    for _ in range(a.slots):
        if (next_block + a.slot_blocks) > a.blocks:
            raise SystemExit("no room left for a slot")
        add_entry(None, next_block, 0, 2)          # slot, not yet used
        next_block += a.slot_blocks
    if a.slots:
        report.append(f"{a.slots} slots of {a.slot_blocks * BLOCK} bytes")

    img[DIR_BLOCK * BLOCK:(DIR_BLOCK + 1) * BLOCK] = dirent

    # PATCH THE BLOCK COUNT BY SIGNATURE, NOT BY OFFSET. An offset here and an
    # offset in src/boot.S are two numbers that have to agree, and the day they
    # stop agreeing this writes a byte into the middle of an instruction.
    i = boot.find(SIG)
    if i < 0:
        raise SystemExit(f"{a.boot}: no {SIG.decode()} signature -- the boot "
                         f"block cannot be told how much to load")
    if boot.find(SIG, i + 1) >= 0:
        raise SystemExit(f"{a.boot}: {SIG.decode()} appears more than once")
    if payload_blocks > 255:
        raise SystemExit(f"image is {payload_blocks} blocks; the count is one "
                         f"byte")
    boot[i + len(SIG)] = payload_blocks if payload_blocks else 1

    img[0:len(boot)] = boot
    open(a.out, "wb").write(img)
    print(f"{a.out}: {a.blocks} blocks, {used} directory entries, "
          f"{a.blocks - next_block} blocks free")
    for r in report:
        print(f"  {r}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
