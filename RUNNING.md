# How to run each one

Eighteen assets, thirteen machines. Every port plays the same game from the same
`core/`; what differs is how the machine is asked to start it.

**None of these ship a ROM.** Where an emulator needs one — a Kickstart, a TOS
image, a MEGA65 ROM, a CoCo 3 ROM — it is yours to supply. The one exception is
the Falcon, where free [EmuTOS](https://emutos.sourceforge.io/) works and is in
fact what this port was developed against.

**Five assets are bare disk images, and their READMEs ship beside them** as
`egatrek-<port>.txt`, because a `.d64`, a `.d81` and an `.atr` have nowhere to
put one. The eight `.zip` assets carry theirs inside as `README.txt`.

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

#### The console is in two halves — press `C` for the other one

EGA Trek's console is **eighty columns wide and this machine has forty**, so it
is the same console seen a half at a time. You spend the game on the **tactical
page**; `C` at the `CMD:` prompt shows the **chart page**, and **any key brings
you back**. `C` costs no turn, and neither does looking.

**Tactical — where you give orders, and where messages arrive:**

```
+-SHORT RANGE SCAN-+-STATUS------------+
|   1 2 3 4 5 6 7 8|                   |
| 1 E . . . . . . .| STARDATE 3500.0   |
| 2 . * . . . . . .| ENERGY    5000    |
| 3 . # . . . . . .| IMPULSE    500    |
| 4 . * . . . . . *| SHIELDS   2500    |
| 5 . . . * . . . .| TORPS        9    |
| 6 . . . . . . . .| WARP       1.0    |
| 7 . . . . . . . .| MONGOLS     18    |
| 8 . . . . . . . .|                   |
+------------------+-------------------+
+-LASERS------------++-MAIN VIEWER-----+
|EFF  ########  100 ||                 |
|TEMP           0   ||                 |
+-COMMAND-----------+|   NO CONTACT    |
| CMD: _            ||                 |
| QUAD 5,7  SEC 1,1 ||                 |
+-------------------++-----------------+
+-------------------------------3500.0-+
|HELM: AWAITING ORDERS CAPTAIN         |
+--------------------------------------+
```

**Chart — press `C`; any key returns you to the orders above:**

```
+-CHART OF KNOWN GALAXY----------------+
|  1   2   3   4   5   6   7   8       |
|1... ... ... ... ... ... ... ...      |
|2... ... ... ... ... ... ... ...      |
|3... ... ... ... ... ... ... ...      |
|4... ... ... ... ... 002 002 002      |
|5... ... ... ... ... 002 014 002      |
|6... ... ... ... ... 004 001 032      |
|7... ... ... ... ... ... ... ...      |
|8... ... ... ... ... ... ... ...      |
+-LEXINGTON IN QUAD 5,7----------------+
+-SYSTEMS STATUS----+------------------+
|CNV #####IMP ##### | U.S.S. LEXINGTON |
|SHD #####SRS ##### |      RCB-92      |
|LIF #####LRS ##### |                  |
|LAS #####CMP ##### |      #.##*##     |
|TUB #####TRN ##### |                  |
|WRP #####SHT ##### |  DEPT. OF SPACE  |
+-------------------+------------------+
```

**Nothing is lost, only moved.** Every panel of the 80-column console is on one
page or the other — the tactical page carries the short range scan, status,
lasers, the command line, the main viewer and the message log; the chart page
carries the long range chart, the systems' state of repair and the ship's
badge.

**Messages arrive on the tactical page**, which is where `C` puts you back, so
you will not miss one by looking at the chart.

At the `CMD:` prompt, **type `HELP` for the full list of orders**. Every prompt
in this game is a line editor — type your answer and press RETURN, including
the ones that ask Y or N. **RUN/STOP is ESC**, which is what the self-destruct
prompt wants when it offers you a way out.

**A fastloader helps.** There are eleven code overlays on this disk and the
game swaps them in as you change screens; JiffyDOS or an SD2IEC with fastload
makes that noticeably quicker. Not required.

SAVE writes `EGATREK.SAV` back to this disk and the hall of fame lives on it,
so **the disk must not be write protected**.

### `egatrek-plus4.d64` — Commodore Plus/4

    xplus4 -autostart egatrek-plus4.d64:trek4

or, on the machine, put the disk in drive 8 and:

    LOAD "TREK4",8
    RUN

Plain `LOAD`, not `LOAD ...,8,1` — the program carries its own BASIC line and
is started with `RUN`, so it wants the ordinary BASIC load.

**Any Plus/4, 50Hz or 60Hz.** The port reads TED's raster counter — 312 lines
against 262 — and sets the music's tempo to match. **A Commodore 16 will not
run it**: the game needs the Plus/4's full 64K.

The console is in two halves exactly as on the C64 above — press `C` at the
`CMD:` prompt for the chart page, any key to come back.

**Quitting resets the machine.** The game runs with RAM mapped over both ROMs,
so there is no BASIC underneath to return to; `Q` hands the machine back by
rebooting it, and says so first.

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


**Two Workbench icons ship with it, and you want both**: `egatrek-amiga.info`
sits *beside* the drawer and makes the drawer visible; `egatrek.info` sits
*inside* it and makes the game double-clickable. Copy the drawer and the
`.info` next to it together — AmigaDOS pairs an icon with the thing of the same
name beside it. **Without them Workbench shows nothing at all** unless Show All
Files is on, which is what an earlier release looked like on a real machine. A
Shell needs neither.

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

### `egatrek-st.zip` — Atari ST / STE

    hatari --machine st --memsize 1 --gemdos-drive C -d egatrek-st \
           --auto 'C:\EGATREK.PRG'

or on the machine: unpack the folder anywhere and double-click `EGATREK.PRG`.
**Quote the path** — an unquoted `C:\EGATREK.PRG` has the backslash eaten by
the shell and Hatari is handed `C:EGATREK.PRG`, which it cannot open.

**Any ST, Mega ST, STE or Mega STE with 1MB and a COLOUR display.** The game
needs about 110K. It switches to low resolution itself and puts your desktop
back when it quits; **in ST high resolution it says so and exits** rather than
paint an unreadable screen, because the console needs sixteen colours and mono
has none.

**An STE gets a better picture for free.** The palette is written in the STE's
four-bits-a-gun encoding, whose extra bit a plain ST ignores — the nearest of
512 colours on an ST, of 4096 on an STE, from one code path.

**No ROM is shipped.** Developed against free
[EmuTOS](https://emutos.sourceforge.io/); a real TOS 2.06 works too and is
yours to supply.

**The console is in two halves** — `C` at the `CMD:` prompt shows the chart
page and any key returns you. Both pages are drawn out in full under the
Commodore 64 entry above; the layout is the same.

The four files must stay together — the program opens them by name from the
directory it runs from. SAVE writes `EGATREK.SAV` beside them, so the folder
must not be read-only.

### `egatrek-coco-ssfm.zip` — Tandy Color Computer 3 **+ SuperSprite FM+**

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
Everything here was measured under MAME's `ssfm` emulation. **A full console
repaint takes about four seconds** — playable, and not brisk.

### `egatrek-iigs.zip` — Apple IIgs

**Any IIgs, any ROM, any memory size, no card and no system software.** Unzip
and put `EGATREK.PO` in a 3.5" drive, then switch the machine on.

That is all there is to it. **There is no ProDOS on this disk and nothing to
type**: the machine reads block 0, which is this port's own loader, and that
reads the game. About fifteen seconds to the title screen.

Under **MAME**, with a IIgs romset:

    mame apple2gs -flop3 EGATREK.PO

**Super Hi-Res, 320×200 — which is exactly 40×25 cells of 8×8**, no margin and
no rounding. The palette holds **EGA's own sixteen colours**, so what you see is
what the original's EGA card put on a monitor, brown included — the one colour
the C128's fixed RGBI chip renders as olive.

Sound is the **Ensoniq 5503**: two oscillators, music on one and effects on the
other, so a hit during the title tune does not chop it. The original had one PC
speaker and could not.

**The console is in two halves** — `C` at the `CMD:` prompt shows the chart
page and any key brings you back, exactly as on the C64. Four saved games fit
on the disk, and they go on the disk the game came from, so do not write
protect it.

**The disk is entirely this port's own** — its own boot block, its own
directory, its own reader over the drive's firmware. Nothing on it belongs to
anyone but Nels Anderson and this project. And the font is this project's own
too, because a IIgs's character generator is **not readable by the CPU**: unlike
the Amiga and ST builds, not one glyph here comes out of the machine's ROM.

---

### `egatrek-coco3gime.zip` — Tandy Color Computer 3, **stock**

**This is the one a plain CoCo 3 runs.** No card, no expansion: 128K, a disk
drive, and nothing else. `egatrek-coco-ssfm.zip` above needs a SuperSprite FM+;
this one uses the machine's own GIME for 80x25 text in eight colours and its
own 6-bit DAC for sound.

Unzip and write `TREK.DSK` to a floppy, or mount it on a CoCo SDC. Then:

    CLEAR 25,&H6FFF
    LOADM"TREKLDR"
    EXEC

**Then wait.** The loader reads 45K with no progress bar — about a minute on a
real floppy, much quicker on an SDC — and the title screen appears when it is
done.

Under **XRoar**, with a CoCo 3 ROM:

    xroar -machine coco3 -ram 128 -load-fd0 TREK.DSK

**XRoar and not MAME, and that is measured rather than preferred.** MAME's
CoCo 3 floppy model wedges this port partway through startup — the WD1773 goes
busy on a sector whose track and sector registers are both correct and never
raises DRQ — where XRoar plays it. Every probe behind this port was run under
both; only the whole game tells them apart.

**What is different to look at.** The GIME's text font has no box-drawing
characters, no block and no reverse video, so panel rules are underlines (which
join across cells), solid cells are spaces in a background colour, and vertical
rules are dotted. The badge is a filled rectangle rather than a rounded disc.
Sound is ONE voice — the CoCo has a DAC, not a sound chip, so an effect
interrupts the music exactly as the original's PC speaker did. And the string
pool lives on the diskette, so the drive works while the console draws.

---

### `egatrek-f256k.zip` — Foenix F256K

**An F256K and an SD card. Nothing else** — no SID chips, no expansion. Unzip
and copy all six files to the **root** of the card, then at the SuperBASIC
prompt:

    /- egatrek

(`/-` hands off to pexec, which loads and runs `EGATREK.PGZ`.)

`OVERLAYS.BIN` **is not optional and must be the one that came with this
`EGATREK.PGZ`** — the two are cut from a single build and the game checks a
stamp in the file. Mixing them stops the game with a message rather than
running something that would be very hard to explain.

Under **MAME**, with a `f256k` driver and an SD-card image:

    mame f256k -window -harddisk sdcard.img

**640×480 in 8×16 character cells, which is 80×30 — and the console is 80×25**,
so it fills the screen with a two-row margin. The original ran at 640×350 in an
8×14 cell, so **this is the closest any of these ports gets to its
proportions**; every other 8-bit port draws it at 8×8. The palette holds EGA's
own sixteen colours, brown included.

Sound is the **SN76489 PSG**, two of its three tone channels — music on one and
effects on the other. The PSGs are inside the machine's FPGA, so this works on
every F256K; the SID sockets are not used.

**The music keeps playing while the card is read.** On the other 8-bit ports a
load stops the tune and the last note drones until it finishes; this machine's
storage is asynchronous, so the game advances the music while it waits.

**This keyboard has no ESC key.** It is a C64 layout: RUN/STOP is what the game
means by escape, and DEL is backspace.

---

## The original

EGA Trek was written by **Nels Anderson** and released as shareware between
1988 and 1992. The original is his; these are ports of it. If you enjoy one, go
and find [the original](https://archive.org/details/EGATrek) — and if you enjoy
that, register it. That was always the deal.
