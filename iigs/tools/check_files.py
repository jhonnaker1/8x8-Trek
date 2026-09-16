#!/usr/bin/env python3
"""Check src/gsfile.c's report, read from stdin as the rig printed it.

SIX BEHAVIOURS, AND HALF OF THEM ARE REFUSALS. "The file was found" means
nothing without "a file that is not there is reported missing", and "the save
was written" means nothing without reading it back -- a save that cannot be
re-read is the failure mode that matters and only the second half of the pair
can see it.
"""
import sys

# core/storage.h
OK, NOTFOUND, ERROR = 0, 1, 2

CHECKS = [
    (0,  OK,       "plat_read_all of a file that IS there"),
    (1,  34,       "...and its length is the directory's, not a block"),
    (2,  1,        "the boot handoff signature was valid"),
    (3,  0,        "the block driver reported no error"),
    (4,  NOTFOUND, "a file that is NOT there is reported missing"),
    (5,  ERROR,    "a write to a name with no slot bit is REFUSED"),
    (6,  OK,       "a write to a new name takes a slot"),
    (7,  OK,       "...and reading it back succeeds"),
    (24, 8,        "...with the length it was written at"),
    (25, 5,        "plat_read returns the 5 bytes asked for"),
    (26, ord("H"), "...starting at the beginning of the file"),
    (27, 5,        "a second plat_read returns 5 more"),
    (28, ord(" "), "...continuing where the first stopped"),
]

TEXT = [("TEXT1", "HELLO FROM A FIL", "the file's contents, not a buffer"),
        ("TEXT2", "SAVED-OK", "the save round-tripped through the disk")]


def main():
    rep, text = None, {}
    for line in sys.stdin:
        if line.startswith("REP "):
            rep = [int(x, 16) for x in line.split()[1:]]
        elif line.startswith("TEXT"):
            k, _, v = line.partition(" ")
            text[k] = v.strip().strip("[]")
        elif "NEVER COMPLETED" in line:
            print("FAIL: the probe never completed -- its report is "
                  "uninitialised memory, not a finding")
            return 1
    if rep is None:
        print("FAIL: no REP line -- the probe produced no report at all")
        return 1

    fails = []
    for off, want, what in CHECKS:
        if off >= len(rep):
            fails.append(f"report is {len(rep)} bytes, needs {off + 1}")
        elif rep[off] != want:
            fails.append(f"{what}: rep[{off}] is ${rep[off]:02X}, "
                         f"expected ${want:02X}")
        else:
            print(f"  {what}")
    for key, want, what in TEXT:
        if text.get(key) != want:
            fails.append(f"{what}: {key} is {text.get(key)!r}, "
                         f"expected {want!r}")
        else:
            print(f"  {what}")

    if fails:
        for f in fails:
            print(f"FAIL: {f}")
        return 1
    print(f"check_files: {len(CHECKS) + len(TEXT)} checks, 0 failures")
    return 0


if __name__ == "__main__":
    sys.exit(main())
