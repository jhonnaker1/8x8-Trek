#!/usr/bin/env python3
"""Cut the overlay images out of the ELF and write OVERLAYS.BIN.

THE IMAGES ARE PACKED AT THEIR TRUE SIZES, not padded to the window. The
window is 8K here because an MMU slot is 8K, and the eleven images average
2,754 bytes -- padding every one of them to the slot would make a 90,112-byte
file where 30,322 will do, and every byte of it is read off an SD card at
startup. The per-image length in the header is what makes that safe.

THE STAMP IS NOT OPTIONAL. An overlay is linked WITH the resident half, so
every call it makes into resident code is a fixed address from that one link.
Yesterday's images beside today's program jump into the middle of some other
function, with no error and nothing naming either file. It is the low sixteen
bits of `ovl_anchor`, which the loader compares against its own.

    'TOVL'  count:u16  stamp:u16  size[count]:u16  then the images
"""
import argparse, struct, subprocess, sys, tempfile, os

# The order IS the overlay id -- these are indices into the file. Keep them in
# step with core/overlay.h, which is where the numbers are defined.
SECTIONS = ["eval", "hof", "front", "info", "repair", "msgs",
            "planet", "cmds", "title", "events", "xtra"]

ap = argparse.ArgumentParser()
ap.add_argument("elf")
ap.add_argument("out")
ap.add_argument("--objcopy", required=True)
ap.add_argument("--nm", required=True)
ap.add_argument("--window", type=int, required=True)
a = ap.parse_args()

stamp = None
for line in subprocess.run([a.nm, a.elf], capture_output=True, text=True).stdout.splitlines():
    f = line.split()
    if len(f) == 3 and f[2] == "ovl_anchor":
        stamp = int(f[0], 16) & 0xFFFF
if stamp is None:
    sys.exit("mkovl: no ovl_anchor in %s -- LTO dropped the stamp's anchor, and "
             "a stamp of zero would match a file from any build" % a.elf)

images = []
with tempfile.TemporaryDirectory() as tmp:
    for name in SECTIONS:
        raw = os.path.join(tmp, name + ".bin")
        subprocess.run([a.objcopy, "-O", "binary",
                        "--only-section=.ovl_" + name, a.elf, raw], check=True)
        data = open(raw, "rb").read()
        if not data:
            sys.exit("mkovl: .ovl_%s is EMPTY. Either nothing is annotated "
                     "OVL_CODE(\"%s\") or the compiler inlined it all into a "
                     "resident caller -- overlay.h's rule 1." % (name, name))
        if len(data) > a.window:
            sys.exit("mkovl: .ovl_%s is %d bytes, the window is %d"
                     % (name, len(data), a.window))
        images.append((name, data))

with open(a.out, "wb") as f:
    f.write(b"TOVL")
    f.write(struct.pack("<HH", len(images), stamp))
    for _, d in images:
        f.write(struct.pack("<H", len(d)))
    for _, d in images:
        f.write(d)

total = sum(len(d) for _, d in images)
print("  %s: %d images, %d bytes, stamp $%04X (window %d, largest %s %d)"
      % (a.out, len(images), total, stamp, a.window,
         *max(((n, len(d)) for n, d in images), key=lambda x: x[1])))
