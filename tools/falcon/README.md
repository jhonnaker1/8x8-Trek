# Falcon scope probes

**The Atari Falcon target was re-opened on 2026-09-11 and `falcon/` is now a
working port** -- see `NOTES.md`, "SCOPE: the ATARI FALCON". These programs are
the measurements the port was built from, kept so the numbers can be
re-derived rather than recited.

    vidprobe.c   VgetSize() over every VIDEL mode word -- finds that the VGA
                 boot mode $001A is 640x480 in 16 colours (153,600 bytes).
    geom.c       Draws a bracket assuming a 320-byte stride, which confirms
                 640x480 and word-interleaved planes. A byte count alone
                 cannot tell 640x480 from 320x960; this can.
    bench.c      Full-screen clear timed on the 200Hz tick: 19 ms.
    fontprobe.c  Line-A's three ROM font headers. Finds 6x6, 8x8 and 8x16 on
                 EmuTOS 1.3.0, all covering codes 0..255 with form_width 256 --
                 which is why falcon/src/falconvid.c can treat a glyph row as
                 dat_table[r * 256 + c]. It picks its font by measuring
                 form_height, not by taking index 2.

Build with the vbcc TOS target, which is already installed:

    export VBCC=$HOME/vbcc PATH=$HOME/vbcc/bin:$PATH
    vc +tos -O2 -o GEOM.PRG geom.c

Run under Hatari, with the program auto-started off a GEMDOS drive and the
console echoed to the host:

    hatari --machine falcon --tos ~/hatari/rom/etos512us.img --monitor vga \
           --vdi off --gemdos-drive C -d <dir> --auto C:\GEOM.PRG \
           --conout 2 --sound off --fast-forward on --run-vbls 3000

**Use EmuTOS, not TOS 4.04** -- the latter double bus-errors on a Falcon here.
For screenshots, Hatari connects to a control socket you listen on first, and
`hatari-shortcut screenshot` triggers one; the socket path must be short
(AF_UNIX is capped near 104 characters).
