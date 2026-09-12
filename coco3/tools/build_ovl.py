#!/usr/bin/env python3
"""Link the CoCo 3 port with overlay code placed in a fixed window, and cut
each overlay out as its own loadable image.

THE PROBLEM THIS SOLVES. cmoc has no per-function section placement, so the
obvious route -- link each overlay separately at the window address -- leaves
the two halves unable to call each other, and would mean writing overlay code
against a hand-built jump table instead of in ordinary C.

THE ANSWER IS ONE LINK. cmoc emits NAMED SECTIONS (start, code, rodata, bss,
...), so an overlay source's `code` section is renamed in its generated
assembly and the link script places that section at the window. Everything is
linked together, so the linker resolves calls IN BOTH DIRECTIONS: resident
code calls overlay functions by name and overlay code calls resident functions
by name. Verified on the machine before this file existed -- a resident main()
calling an overlay function that calls back into a resident helper returns the
right answer, with the overlay's symbols at $6000 and the resident's at $20xx.

lwlink then emits each section as its own DECB block, so cutting the images
out afterwards is just reading the block list.

THE RULE THAT COMES WITH IT is core/overlay.h's rule 4, and it is not enforced
here: only resident code may call into a window, and it must load the right
image first. A function inside an overlay cannot ovl_load, because it would
page itself out mid-call.
"""
import os, re, subprocess, struct, sys

CMOC   = os.environ.get("CMOC",   os.path.expanduser("~/cmoc/bin/cmoc"))
LWASM  = os.environ.get("LWASM",  "lwasm")
LWLINK = os.environ.get("LWLINK", "lwlink")
LIBDIR = os.path.join(os.environ.get("PKGDATADIR",
                      os.path.expanduser("~/cmoc/share/cmoc")), "lib")

# cmoc's own section order, taken from the link script it generates. The
# overlay sections are appended after program_end so the resident image is
# laid out exactly as it would be without them.
BASE_SECTIONS = """define basesympat s_%s
define lensympat l_%s
section start load {org:04x}
section code
section constructors_start
section constructors
section constructors_end
section destructors_start
section destructors
section destructors_end
section initgl_start
section initgl
section initgl_end
section rodata
section rwdata
section bss,bss
section program_end
"""


def run(cmd):
    p = subprocess.run(cmd, capture_output=True, text=True)
    if p.returncode:
        sys.exit("build_ovl: %s\n%s%s" % (" ".join(cmd), p.stdout, p.stderr))
    return p


def compile_to_s(src, intdir, cflags):
    """cmoc -c -i keeps the .s beside the .o; the .s is what gets edited."""
    obj = os.path.join(intdir, os.path.basename(src)[:-2] + ".o")
    run([CMOC] + cflags + ["-i", "--intdir", intdir, "-c", "-o", obj, src])
    return os.path.join(intdir, os.path.basename(src)[:-2] + ".s"), obj


def rename_section(spath, newname):
    """`SECTION code` -> `SECTION <newname>` in one file's assembly. Only the
    code section moves: an overlay's rodata and bss stay resident, because
    paging data out from under a caller is a different and worse problem."""
    s = open(spath).read()
    out = re.sub(r'^(\s*)SECTION(\s+)code$', r'\1SECTION\2' + newname, s, flags=re.M)
    if out == s:
        sys.exit("build_ovl: no `SECTION code` in %s" % spath)
    open(spath, "w").write(out)


def assemble(spath, obj):
    run([LWASM, "-fobj", "--pragma=forwardrefmax", "-D_COCO_BASIC_",
         "--output=" + obj, spath])


def map_top(mappath, ovl_sections):
    """The top of everything RESIDENT, taken from the link map -- which is the
    only place that sees BSS.

    THIS IS THE CHECK THAT WAS MISSING. The first version compared the window
    against the DECB blocks, and BSS is not in a DECB file because it is
    uninitialised. So a window placed at $E100 sat in the MIDDLE of a
    4,633-byte bss and would have loaded an overlay straight over the message
    log, the sector buffer, the FAT and the whole game state -- and the check
    said it was fine. A check that cannot see the thing it is checking is
    worse than no check."""
    top = 0
    for line in open(mappath):
        m = re.match(r'Section: (\S+) \(([^)]*)\) load at ([0-9A-Fa-f]+), '
                     r'length ([0-9A-Fa-f]+)', line.strip())
        if m and m.group(1) not in ovl_sections:
            top = max(top, int(m.group(3), 16) + int(m.group(4), 16))
    return top


def blocks(path):
    """The (addr, bytes) blocks of a DECB binary."""
    d = open(path, "rb").read()
    i, out = 0, []
    while i + 5 <= len(d):
        t, ln, ad = struct.unpack(">BHH", d[i:i+5])
        if t == 0xFF:
            break
        out.append((ad, d[i+5:i+5+ln]))
        i += 5 + ln
    return out


def write_map(imgdir, win, names, idx_label=(), slots=1):
    """OVL_WINDOW, the window's capacity, and the image names. GENERATED,
    because a window address written down twice is one that drifts."""
    os.makedirs(imgdir, exist_ok=True)
    with open(os.path.join(imgdir, "ovlmap.h"), "w") as f:
        f.write("/* GENERATED by coco3/tools/build_ovl.py -- do not edit. */\n")
        f.write("#ifndef COCO3_OVLMAP_H\n#define COCO3_OVLMAP_H\n\n")
        f.write("#define OVL_WINDOW  0x%04X\n" % win)
        f.write("#define OVL_SIZE    %d   /* to the I/O page at $FF00 */\n"
                % (0xFF00 - win))
        f.write("#define OVL_IMAGES  %d\n\n" % slots)
        f.write("/* Indexed by the SHARED OVL_* constants, so ovl_load(OVL_HOF)\n"
                "   from main() finds this port's image without the shared code\n"
                "   knowing anything about it. A hole is an overlay this port\n"
                "   does not split out; ovl_load returns for those. */\n")
        f.write("static const char * const ovl_name[OVL_IMAGES] = {\n")
        for i in range(slots):
            lbl = dict(idx_label).get(i)
            f.write('    %-14s /* %d */\n'
                    % (('"%s.OVL",' % lbl) if lbl else "0,", i))
        f.write("};\n\n#endif\n")


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--org", default="1200")
    ap.add_argument("--window", default="6000")
    ap.add_argument("--out", required=True)
    ap.add_argument("--intdir", required=True)
    ap.add_argument("--imgdir", required=True)
    ap.add_argument("--cflags", default="")
    ap.add_argument("--resident", nargs="+", required=True)
    ap.add_argument("--overlay", nargs="*", default=[])
    a = ap.parse_args()

    org, win = int(a.org, 16), int(a.window, 16)
    os.makedirs(a.intdir, exist_ok=True)
    os.makedirs(a.imgdir, exist_ok=True)
    cflags = a.cflags.split()

    # THE WINDOW ADDRESS IS A BUILD PARAMETER, NOT A DERIVED ONE -- the C128's
    # sits at a fixed $AF00 for the same reason. Deriving it from the link
    # created a chicken and egg: the runtime needs the address to compile, and
    # the link needs the runtime compiled. Fixed and CHECKED beats clever.
    # AN OVERLAY IS DECLARED AS INDEX:SOURCE, and the index is the SHARED
    # OVL_* constant from core/overlay.h -- OVL_HOF is 1, OVL_FRONT is 2.
    # That is what makes rule 4 already satisfied: main() has called
    # load_hof() and load_front() before the relevant screens since the C128,
    # so matching the numbering reuses a discipline that is written down and
    # reviewed, instead of inventing a second one beside it.
    names, index = [], []
    for src in a.overlay:
        idx, _, path = src.partition(":")
        index.append(int(idx))
        names.append(("ovl%s" % idx, os.path.basename(path)[:-2].upper()[:8]))
    a.overlay = [s.partition(":")[2] for s in a.overlay]
    idx_label = list(zip(index, [l for _, l in names]))
    slots = max(index) + 1 if index else 1
    write_map(a.imgdir, win, names, idx_label, slots)

    objs = []
    for src in a.resident:
        _, obj = compile_to_s(src, a.intdir, cflags)
        objs.append(obj)

    for (sect, _), src in zip(names, a.overlay):
        spath, obj = compile_to_s(src, a.intdir, cflags)
        rename_section(spath, sect)
        assemble(spath, obj)
        objs.append(obj)

    # TWO PASSES, because the window must not land on resident code. The
    # resident extent does not depend on where the window is -- the overlay
    # sections are separate -- so one probe link is enough to find the top of
    # the resident image, and the window then goes immediately above it. The
    # Atari port places its window the same way, above the code rather than
    # inside it.
    script = os.path.join(a.intdir, "coco3.link")
    def link(window):
        with open(script, "w") as f:
            f.write(BASE_SECTIONS.format(org=org))
            for sect, _ in names:
                # EVERY overlay is placed at the SAME address: they share the
                # one window, which is the whole point.
                f.write("section %s load %04x\n" % (sect, window))
            f.write("entry program_start\n")
        run([LWLINK, "--format=decb", "--output=" + a.out, "--script=" + script,
             "--map=" + a.out + ".map", "-L" + LIBDIR,
             "-lcmoc-crt-ecb", "-lcmoc-std-ecb",
             os.path.join(LIBDIR, "float-ctor.ecb_o"), "-lcmoc-float-ecb"] + objs)

    # TWO PASSES. The window address is a compile-time constant, so changing
    # it does not change any section's SIZE -- one probe link is enough to
    # learn where everything resident ends, bss included, and the real window
    # goes above that. Pass two is then self-consistent, and asserted to be.
    sectnames = set(sect for sect, _ in names)
    if names:
        link(0xFE00)
        top = map_top(a.out + ".map", sectnames)
        want = (top + 0xFF) & ~0xFF
        if want != win:
            print("  window       $%04X is below the top of bss ($%04X); "
                  "moving it to $%04X" % (win, top, want))
            win = want
            write_map(a.imgdir, win, names, idx_label, slots)
            # only the file that includes ovlmap.h needs recompiling, but
            # rebuilding all of it is cheap and cannot go stale
            objs = []
            for src in a.resident:
                _, obj = compile_to_s(src, a.intdir, cflags)
                objs.append(obj)
            for (sect, _), src in zip(names, a.overlay):
                spath, obj = compile_to_s(src, a.intdir, cflags)
                rename_section(spath, sect)
                assemble(spath, obj)
                objs.append(obj)
    link(win)

    top = map_top(a.out + ".map", sectnames)
    if names and win < top:
        sys.exit("build_ovl: THE WINDOW AT $%04X IS BELOW THE TOP OF RESIDENT "
                 "MEMORY ($%04X) -- an image would load over bss" % (win, top))

    # Cut the overlay blocks out. lwlink emits each section as its own DECB
    # block, so a block loading at the window IS an overlay image.
    resident, images = [], []
    for ad, data in blocks(a.out):
        (images if ad == win else resident).append((ad, data))

    for (sect, label), (ad, data) in zip(names, images):
        img = os.path.join(a.imgdir, label + ".OVL")
        open(img, "wb").write(data)
        images and print("  %-12s %5d bytes at $%04X -> %s"
                         % (sect, len(data), ad, img))

    rtot = sum(len(d) for _, d in resident)
    lo = min((ad for ad, _ in resident), default=0)
    hi = max((ad + len(d) for ad, d in resident), default=0)
    print("  resident     %5d bytes, $%04X..$%04X" % (rtot, lo, hi - 1))
    if images:
        print("  window       $%04X, largest image %d bytes"
              % (win, max(len(d) for _, d in images)))
    top = max(hi, (win + max((len(d) for _, d in images), default=0)))
    print("  free below $FF00: %d" % (0xFF00 - top))
    if top > 0xFF00:
        sys.exit("build_ovl: the image overruns the I/O page at $FF00 by %d"
                 % (top - 0xFF00))


if __name__ == "__main__":
    main()
