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

    CLEAR 25,&H6FFF
    LOADM"SPEEDTST"
    EXEC

**THE `CLEAR` IS NOT OPTIONAL AND THIS FILE USED TO OMIT IT.** Without it
BASIC's memory top is the top of RAM, cmoc's runtime puts the program's stack
just under it, and the stack sits directly on the report at `$7F00` -- so the
numbers you read back are partly the program's own return addresses. Jamie ran
it that way on real hardware and got `229` where `161` was meant to be. The
`CLEAR` moves BASIC's ceiling to `$6FFF`, which protects everything above it,
and `LOADM` still places a program at `$7000` quite happily (checked, under
MAME, through real Disk BASIC).

It reads 306 sectors **twice at 0.89 MHz**, then the same 306 **twice at
1.78 MHz**. The slow pass is the CONTROL — a tired drive shows in both columns
and says nothing about the clock, so what matters is the difference. Every
sector is read twice and compared rather than trusted to `DCSTA`, because a
controller that reports success and hands back a wrong byte is exactly what
double speed is suspected of, and a status byte cannot see that.

It returns to BASIC and leaves its report at `$7F00`. **READ THE RESULTS, AND
THIS FILE USED TO NAME THE WRONG BYTES:** `$7F00..$7F03` are a header -- armed,
stage, and the geometry it used -- and contain no measurement whatsoever. The
two passes are at `$7F04` and `$7F08`.

    PRINT PEEK(&H7F00),PEEK(&H7F01),PEEK(&H7F0D)
    PRINT PEEK(&H7F04),PEEK(&H7F05),PEEK(&H7F06),PEEK(&H7F07)
    PRINT PEEK(&H7F08),PEEK(&H7F09),PEEK(&H7F0A),PEEK(&H7F0B)

**Line 1 is whether to believe lines 2 and 3.** It must read `161 179 90` --
armed, both passes finished, report complete. Anything else and the run did
not complete or the report was overwritten; the other two lines mean nothing.

**Line 2 is 0.89 MHz, the CONTROL. Line 3 is 1.78 MHz, the question.** Each is:

    status errors, compare mismatches, first bad track, first bad sector

`0 0 255 255` is a clean pass -- 255 is "no bad sector recorded". **What
answers item 55 is the DIFFERENCE between the two lines**, not either alone: a
clean line 2 and a dirty line 3 means double speed breaks disk I/O on this
machine. Both dirty means the drive or the disk, and says nothing about the
clock.

If it seems to hang, `$7F0E` and `$7F0F` are live crumbs -- the track it is on
and how many tracks it has finished -- so a stuck run says where.

Rebuild with `make -C coco3 speedtest`. Source: `coco3/src/speedtest.c`.
The `.dsk` is still a build product and stays out of git; this directory and
this file are tracked so the path is not something anybody has to rediscover.
