# How to run each one

Eleven assets, eight machines. Every port plays the same game from the same
`core/`; what differs is how the machine is asked to start it.

**None of these ship a ROM.** Where an emulator needs one — a Kickstart, a TOS
image, a MEGA65 ROM, a CoCo 3 ROM — it is yours to supply. The one exception is
the Falcon, where free [EmuTOS](https://emutos.sourceforge.io/) works and is in
fact what this port was developed against.

**Four assets are bare disk images, and their READMEs ship beside them** as
`egatrek-<port>.txt`, because a `.d64`, a `.d81` and an `.atr` have nowhere to
put one. The four `.zip` assets carry theirs inside as `README.txt`.

**This file is the source for the release page.** `make running-section` emits
it with the heading demoted, so the GitHub release body is generated from here
rather than retyped — two copies of the same instructions is how other counts
in this project drifted six times.

---

### `egatrek-c128.d64` — Commodore 128

    x128 -autostart egatrek-c128.d64

or, on the machine, put the disk in drive 8 and `RUN "TREK128"`.

**It must be in 80-column mode** — press the 40/80 DISPLAY key before you
start, or set it in your emulator. The console is 80×25 with a colour on every
cell, which is what the VDC gives and the VIC-IIe does not.

**JiffyDOS is assumed but not required** — the port was developed and timed
with it. SAVE writes `EGATREK.SAV` back to this disk and the hall of fame lives
on it too, so **the disk must not be write protected**.

### `egatrek-c64.d64` — Commodore 64

    x64sc -autostart egatrek-c64.d64:trek64

or, on the machine, put the disk in drive 8 and:

    LOAD "TREK64",8,1
    RUN

**Any C64 or 64C, PAL or NTSC.** The port counts raster lines to work out
which it is on and tunes the SID to match — the same frequency word is nearly
half a semitone apart on the two machines.

**The console is in two halves, and `C` swaps between them.** Eighty columns
do not fit, so the tactical page carries the scan, status, lasers, command
line, viewer and messages, and the chart page carries the long range chart,
the badge and the systems. `C` costs no turn.

**A fastloader helps.** There are eleven code overlays on this disk and the
game swaps them in as you change screens; JiffyDOS or an SD2IEC with fastload
makes that noticeably quicker. Not required.

SAVE writes `EGATREK.SAV` back to this disk and the hall of fame lives on it,
so **the disk must not be write protected**.

### `egatrek-mega65.d81` — MEGA65

    xmega65 -8 egatrek-mega65.d81 -prgmode 65

in [Xemu](https://github.com/lgblgblgb/xemu), with a MEGA65 ROM — then at the
prompt:

    MOUNT "EGATREK.D81"
    RUN "EGATREK"

`-prgmode 65` states native C65 mode explicitly. Real hardware works too: put
the `.d81` on the SD card and mount it the same way.

### `egatrek-x16.zip` — Commander X16

    x16emu -prg trekx16.prg -run

Unzip first and run it **from the unzipped folder** — `OVERLAYS.BIN`,
`STRINGS.DAT`, `MUSIC.DAT` and `BRIEF.TXT` are opened by bare name from the
current directory. Needs the emulator's own `rom.bin`. Real hardware works.

### `egatrek-atari.atr` — Atari 800XL **+ VBXE**

Boot it with BASIC disabled — hold **OPTION** on an XL or XE. The disk boots
itself; **there is no DOS on it**, because Atari's DOS is Atari's and a disk
carrying it would not be ours to give away. It has its own boot record,
directory and SIO seam.

**VBXE is required.** The nine-panel console needs 80 columns and sixteen
colours on their own values and a stock 800XL has neither — without VBXE the
program runs and you see nothing.

**In Altirra**, set up a profile for an **XL machine, NTSC, 1088K, with VBXE
enabled**, and select it. Then launch the disk:

    File → Boot Image…   and pick egatrek-atari.atr

Dragging the `.atr` onto the window does the same thing, and on the command
line it is `/disk <path to egatrek-atari.atr>`. **Leave BASIC off** — that is
the default for XL unless you ask for it with `/basic`, and this disk needs it
off the way real hardware does.

**There is no `/vbxe` switch.** Altirra's author: *"The /vbxe switch was a
casualty of when I moved VBXE onto the more general device framework."* VBXE is
a device, so it has to come from the profile — which is why the profile is the
first step rather than an afterthought.

**Be patient with the first screen: about two minutes on a stock 1050.** That
is measured, not estimated — 112 seconds to read 36,474 bytes of overlays,
strings and music into VBXE's memory before the title screen can be drawn. A
fast-SIO drive or an emulator's SIO patch does it in a few seconds.

### `egatrek-amiga.zip` — Commodore Amiga, Kickstart 2.0+

OCS or ECS and about 200K free. Unzip the drawer anywhere and run `egatrek`
from a shell, or double-click it from Workbench — **it finds its files through
`PROGDIR:`, so the current directory does not matter.** It opens its own
640×200 sixteen-colour screen and gives the machine back when you quit.

**Under [Amiberry](https://github.com/BlitterStudio/amiberry)**, which is what
this port was developed and played on. Amiberry mounts a **host directory as an
Amiga volume**, so there is no ADF to build and nothing to copy — point it
straight at the unzipped drawer. Write a `.uae` config:

    config_description=EGA Trek
    cpu_type=68ec020
    chipmem_size=4
    fastmem_size=8
    kickstart_rom_file=<your kicka1200.rom>
    hardfile2=rw,:<your AmigaOS 3.x .hdf>,0,0,0,512,0,,uae
    filesystem2=rw,WORK:WORK:<the unzipped egatrek-amiga folder>,0

and run it:

    amiberry -f egatrek.uae

On macOS the app bundle is not on `PATH`, so that is:

    /Applications/Amiberry.app/Contents/MacOS/Amiberry -f egatrek.uae

Then at the Amiga shell: `work:egatrek`. (Add `-G` to skip Amiberry's GUI —
that is what this project's automation passes; leave it off if you want the
settings window.) You still need **your own Kickstart and a bootable AmigaOS
volume**; neither ships here.

**On real hardware, just copy the drawer across** — `PROGDIR:` makes its
location irrelevant, so it runs from anywhere, Workbench included.

### `egatrek-falcon.zip` — Atari Falcon030

Unzip and **run `EGATREK.PRG` from its own folder** — unlike the Amiga, this
one opens its files by bare name through GEMDOS, so the current directory is
where it looks. In [Hatari](https://hatari.tuxfamily.org/):

    hatari --machine falcon --tos <your tos.img> --monitor vga \
           --vdi off --gemdos-drive C -d <the egatrek-falcon folder> \
           --auto 'C:\EGATREK.PRG'

**Use EmuTOS.** `etos512us.img` is what this port was developed on; Atari's TOS
4.04 double bus-errors on a Falcon here.

The game asks the machine which monitor it has and picks a mode: 640×480 in
sixteen colours on VGA, 640×400 interlaced on RGB and on a television. **A TV
will flicker** — the console needs 400 lines and 640×200 cannot hold it.
**ST monochrome is refused in words** rather than painted unreadably.

### `egatrek-coco3.zip` — Tandy Color Computer 3 **+ SuperSprite FM+**

Unzip and write `TREK.DSK` to a floppy, or mount it on a CoCo SDC. Then:

    CLEAR 25,&H6FFF
    LOADM"TREKLDR"
    EXEC

**Then wait, and keep watching the BASIC screen.** The loader reads 44K with no
progress bar — about sixty seconds on a real floppy, far quicker on an SDC.
When it finishes the monitor switches itself to the card and the title screen
is there.

Under MAME, with a CoCo 3 ROM — the SuperSprite goes in Multi-Pak slot 1 and
the disk controller in slot 4, and that order is not a preference:

    mame coco3 -window -skip_gameinfo \
        -ext multi -ext:multi:slot1 ssfm -ext:multi:slot4 fdc \
        -flop1 TREK.DSK

**The card is not optional and the port has never run on a real one.** The
CoCo 3's own video gives eight colours in 80 columns; this wants sixteen per
pixel and a 512-wide bitmap, and the SuperSprite's V9958 is what provides them.
Everything here was measured under MAME's `ssfm` emulation. **It is the slowest
of the seven** and it is playable.

---

## The original

EGA Trek was written by **Nels Anderson** and released as shareware between
1988 and 1992. The original is his; these are ports of it. If you enjoy one, go
and find [the original](https://archive.org/details/EGATrek) — and if you enjoy
that, register it. That was always the deal.
