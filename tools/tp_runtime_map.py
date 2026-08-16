#!/usr/bin/env python3
"""Label the Turbo Pascal runtime inside a DOS executable.

Most of EGATREK.EXE is Borland's Turbo Pascal 5 runtime rather than the game.
Knowing which is which is the single biggest reduction in search space for
reverse engineering it, and nothing in Ghidra identifies Turbo Pascal library
routines.

dcc (github.com/nemerle/dcc) ships that knowledge as signature files, but dcc
itself cannot get through this binary -- it walks into the game's string pool,
mistakes prose for 80386 opcodes, and then segfaults. The signatures are
perfectly good on their own, so this reads them directly and scans.

The .sig format, from dcc's src/chklib.cpp:

    "dccs"                     magic
    numKeys, numVert           uint16 each
    PatLen (23), SymLen (16)   uint16 each
    "T1" len <len bytes>       perfect-hash table
    "T2" len <len bytes>       perfect-hash table
    "gg" len <len bytes>       perfect-hash graph
    "ht" len <entries>         numKeys * (SymLen name + PatLen pattern)

Only the "ht" section matters here: the hash tables exist to make lookup O(1)
for a known entry point, and we are scanning instead. 0xF4 is the wild byte,
standing in for operands that vary between programs -- call targets and the
like.

    python3 tools/tp_runtime_map.py <sigfile> <exe> [--dump]
"""
import struct
import sys

WILD = 0xF4
MZ_HEADER_PARAS = 8            # offset of the header-size field in an MZ header


def parse_sig(path):
    """Return [(name, pattern_bytes)] from a dcc signature file."""
    d = open(path, "rb").read()
    if d[:4] != b"dccs":
        raise SystemExit("%s is not a dcc signature file" % path)
    num_keys, num_vert, pat_len, sym_len = struct.unpack("<4H", d[4:12])

    pos = 12
    for tag in (b"T1", b"T2", b"gg", b"ht"):
        if d[pos:pos + 2] != tag:
            raise SystemExit("expected %r at offset %d, found %r"
                             % (tag, pos, d[pos:pos + 2]))
        seg_len = struct.unpack("<H", d[pos + 2:pos + 4])[0]
        pos += 4
        if tag == b"ht":
            break
        pos += seg_len

    # The "ht" length field is numKeys * (SymLen + PatLen + 2), but the
    # entries are actually SymLen + PatLen. dcc gets away with it because it
    # only ever uses the field in a sanity check and then reads 39 bytes at a
    # time. Deriving the stride from that field misaligns every record after
    # the first, so take it from the bytes that are really there.
    stride = sym_len + pat_len
    remaining = len(d) - pos
    if remaining // num_keys != stride:
        raise SystemExit("ht stride is %d, not the expected %d"
                         % (remaining // num_keys, stride))

    out = []
    for i in range(num_keys):
        rec = d[pos + i * stride: pos + i * stride + stride]
        name = rec[:sym_len].split(b"\0")[0].decode("ascii", "replace").strip()
        pat = rec[sym_len:sym_len + pat_len]
        if name:
            out.append((name, pat))
    return out, pat_len


def load_module(path):
    """Strip the MZ header; return (image, load_base_paragraph_offset)."""
    d = open(path, "rb").read()
    if d[:2] not in (b"MZ", b"ZM"):
        raise SystemExit("%s is not an MZ executable" % path)
    hdr_paras = struct.unpack("<H", d[MZ_HEADER_PARAS:MZ_HEADER_PARAS + 2])[0]
    return d[hdr_paras * 16:]


def scan(image, sigs, pat_len, min_concrete=8):
    """Match every signature at every offset. Indexed by first concrete byte
    so this stays one pass rather than len(sigs) passes.

    Signatures that are mostly wild carry too little information to identify
    anything -- one of them matched 669 consecutive offsets before this guard
    existed -- so they are skipped and counted rather than reported."""
    by_first = {}
    anywhere = []
    skipped = 0
    for name, pat in sigs:
        concrete = [b for b in pat if b != WILD]
        # Too little information, or a placeholder slot: one entry is 23 zero
        # bytes, which matches every run of zeros in the image.
        if len(concrete) < min_concrete or len(set(concrete)) < 2:
            skipped += 1
            continue
        if pat[0] == WILD:
            anywhere.append((name, pat))
        else:
            by_first.setdefault(pat[0], []).append((name, pat))
    if skipped:
        print("skipped %d signature(s) with fewer than %d concrete bytes"
              % (skipped, min_concrete))

    def matches(pat, off):
        for j in range(pat_len):
            b = pat[j]
            if b != WILD and b != image[off + j]:
                return False
        return True

    hits = []
    end = len(image) - pat_len
    for off in range(end):
        cands = by_first.get(image[off])
        if cands:
            for name, pat in cands:
                if matches(pat, off):
                    hits.append((off, name))
    for name, pat in anywhere:
        for off in range(end):
            if matches(pat, off):
                hits.append((off, name))
    return sorted(hits)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) != 2:
        raise SystemExit(__doc__.strip().splitlines()[-1])
    sig_path, exe_path = args

    sigs, pat_len = parse_sig(sig_path)
    print("%d signatures, %d-byte patterns" % (len(sigs), pat_len))
    if "--dump" in sys.argv:
        for name, pat in sigs:
            print("  %-16s %s" % (name, pat.hex()))
        return

    image = load_module(exe_path)
    print("load module: %d bytes" % len(image))

    hits = scan(image, sigs, pat_len)
    names = {}
    for off, name in hits:
        names.setdefault(name, []).append(off)

    print("%d match(es), %d distinct routine(s)\n" % (len(hits), len(names)))
    for name in sorted(names):
        offs = names[name]
        shown = " ".join("%06X" % o for o in offs[:6])
        more = "" if len(offs) <= 6 else "  (+%d more)" % (len(offs) - 6)
        print("  %-16s x%-3d  %s%s" % (name, len(offs), shown, more))


if __name__ == "__main__":
    main()
