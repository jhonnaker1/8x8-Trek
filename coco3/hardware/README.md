# Disks for real hardware

**`make release-clean` does not reach this directory, and that is why it
exists.** Everything in `build/` is wiped before a release is cut — rightly,
because ALWAYS REBUILD is what stops a stale artefact shipping — but a disk
written for a machine on someone's desk is not that kind of artefact. This one
was deleted twice by a release, and the second time the 6809 toolchain was
itself unavailable for a morning, because an Xcode update had revoked the
licence agreement every compiler on the machine depends on.

## `speedtst.dsk` — item 55

The one open item on this project, and the only one nothing in this repository
can settle. `vdc_init()` writes `$FFD9` and the CoCo 3 runs the whole game at
1.78 MHz — **including every overlay load and every SAVE** — and CoCo 3 disk
I/O at double speed is a known hazard on real hardware that MAME is more
forgiving about than a WD1773 is.

It touches **no video at all**: no `$FF7E`, no V9958, no YM2149. So it runs on
a bare CoCo 3 with any controller, a CoCo SDC included — which is what matters
here, because the machine that exists has no SuperSprite FM+ and cannot run
the game itself.

Mount as drive 0, then:

    LOADM"SPEEDTST"
    EXEC

It reads 306 sectors **twice at 0.89 MHz**, then the same 306 **twice at
1.78 MHz**. The slow pass is the CONTROL — a tired drive shows in both columns
and says nothing about the clock, so what matters is the difference. Every
sector is read twice and compared rather than trusted to `DCSTA`, because a
controller that reports success and hands back a wrong byte is exactly what
double speed is suspected of, and a status byte cannot see that.

It returns to BASIC and leaves its report at `$7F00`:

    PRINT PEEK(&H7F00),PEEK(&H7F01),PEEK(&H7F02),PEEK(&H7F03)

Rebuild with `make -C coco3 speedtest`. Source: `coco3/src/speedtest.c`.
The `.dsk` is still a build product and stays out of git; this directory and
this file are tracked so the path is not something anybody has to rediscover.
