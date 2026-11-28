# EGA Trek for the NeXT

**Status: released in v0.25.0 (2026-11-27), and played** -- Jamie, in
emulation: *"works great"*, and of the sound, after it was rewritten as one
stream, *"sound is perfect now"*. It runs in BMNeXT's host simulator (Previous inside) as a
NeXTstation Turbo Color under NeXTSTEP 3.3. Not yet tried: OPENSTEP 4.2,
the hall of fame's file being written, and real hardware.

**Colour NeXTs only, by Jamie's decision (2026-11-27), as on every EGA Trek
port.** A NeXT with no colour screen gets an alert panel naming what it
lacks, and the program quits: the Mac's rule. The check looks for a colour
screen anywhere, so a NeXTdimension's screen counts, and the window opens on
it.

A NeXTSTEP application, `EGATrek.app`. Its window holds the 80x25 console,
in EGA's sixteen colours, and it has the NeXT's own menu: Info, Hide, Quit.
STRINGS.DAT, MUSIC.DAT and BRIEF.TXT are inside the wrapper, so copying
EGATrek.app is the whole installation. Saves and the hall of fame go in the
folder that holds EGATrek.app.

## Building

The program is compiled **on the NeXT**, by NeXTSTEP 3.3's own `cc` from the
Developer tools (cc-437.2.6, which is gcc 2.5.8). No compiler on the host
makes 68k Mach-O for NeXTSTEP; commodore-uno/next found the same. The work
is in two halves:

    make verify          on the host: the gate `make ports` runs
                         (tools/verify_next.py)
    make deploy          on the host: builds the data files, and copies
                         them, this folder and the shared sources onto the
                         NeXT's shared folder (NEXT_CARD)
    make -f Makefile.next    on the NeXT, in /Net/nfs/egatrek/next:
                         EGATrek.app
    make release         on the host: build/egatrek-next.zip, holding
                         egatrek-next.tar and README.txt, packed from the
                         EGATrek.app the NeXT built

`make release` packs a program the host cannot compile, so
`tools/checkbuild.py` refuses it unless that program was built from exactly
this tree. Every deployed file must match the repository, the program must
be newer than all of them, and the data inside the wrapper must be
`build/`'s. `tools/mktar.py` packs it as ustar, which NeXTSTEP's tar reads,
with fixed dates and owners. The same build always packs to the same bytes.
Whether NeXTSTEP's cc builds the same program twice has not been measured
here (commodore-uno/next found that it does). The tar has not yet been
unpacked on the NeXT.

`tools/verify_next.py` is what the host can check about a program it cannot
compile. It runs the Falcon's glyph sweep, puts every C file the NeXT
compiles through C89 (the NeXT's gcc 2.5.8 is the strictest compiler any port
has), checks the deploy list against everything `Makefile.next` and the
sources reach, and checks the `<stdint.h>` stand-in and the icon. Each of the
last four was made to fail on purpose.


In Previous or BMNeXT the host's shared folder is `/Net/nfs` on the NeXT.
Here it is BMNeXT's simulated SD card on Crucial Win,
`/Volumes/Crucial Win/EGATrek/next/card`, which holds this port's own copy of
the NeXTSTEP 3.3 disk.

`Makefile.next` has one rule per file, because NeXTSTEP's `make` has no `%`
pattern rules, and every object depends on every header.

## How it works

- **The Mac port, translated.** `src/nextvid.m` is `mac/src/macvid.c`'s
  design: the same seam (`c128/src/vdc.h`), the same box glyphs (the
  Falcon's 8x16 set), and the machine's own font read once at start-up into
  8x16 cells. Here the font is Courier-Bold 13, drawn into an NXImage and
  read back. The image reads back **bottom row first**: the first build
  stood every letter on its head. Setting `EGATREK_FONT="Name size"` in the
  environment tries another face without a rebuild.
- **12 bits a pixel.** The console is a 640x400 bitmap at four bits each of
  red, green and blue, the NeXTstation Color's own depth. EGA's levels
  ($00/$55/$AA/$FF) are exactly 0/5/10/15 in four bits, so nothing is
  rounded. A cell row is twelve bytes, a table lookup per glyph row.
  `NXDrawBitmap` draws the rows of cells that changed, into a buffered
  window that is flushed once a frame.
- **The event loop lives in `wait_vsync()`,** as on the Mac and in
  commodore-uno/next. `main()` is the AppKit's. The delegate's
  `appDidInit:` calls the shared game's `main()`, compiled as `trek_main()`,
  which takes events until the next sixtieth of a second each time it
  waits.
- **Keys.** The view's `keyDown:` queues key-downs. The arrows come in the
  symbol set (173 up, 175 down), and the delete key sends 127. Setting
  `EGATREK_KEYLOG` in the environment lists every key-down on stderr. That
  log found a fault in every port (below).
- **Sound** is two square-wave voices, music and effects, mixed here into
  one stream of 16-bit samples at 44100 Hz stereo. `snd_poll()` keeps three
  46 ms chunks queued in the C sound library (`SNDStartPlaying` with pre-empt
  off queues behind the one playing). Each chunk's end function frees its
  slot, so the sound hardware's clock paces the queue. The music is timed in
  samples inside the renderer, at 2422.2 frames to a PC tick. The pitch is
  the semitone the composer meant: `mac/src/midinote.c` is linked from the
  Mac, and the title tune measures A3 220.0, D4 293.4 and F4 349.3 Hz. Jamie:
  *"sound is perfect now"*.
  **The first version started one sound per note, and Jamie heard it as
  "choppy".** The NeXT's sound output stops and starts again between two
  sounds. My recording missed it because BMNeXT's `/audio` gives out samples
  only while the output is running, so the gaps were cut out. **Measure
  against the wall clock, not note against note**: 8.3 s of samples came
  from about 9 s of music. **And `/audio` is drained by whoever asks**. A
  recorder running while the live page is open takes samples from the page:
  both hear dropouts, and both measure about a third of the rate.
- **The icon** (`EGATrek.tiff`, from `tools/mkicon.py`) is the Mac's and the
  Amiga's ship, at 48x48. It is linked into an `__ICON` segment, as
  NeXTSTEP's own makefiles do it, and shows on the app tile and in the Info
  panel.
- **`<stdint.h>`.** NeXTSTEP 3.3 has none, so `src/compat/stdint.h` stands in
  for it, on the NeXT only.

## What the port found in the shared code

- **The arrow keys never worked, on any port.** `KB_UP` and `KB_DOWN` are 1
  and 2, and `ui_read_command()` drops every code below 32, so `main()`'s
  shields-on-arrows dispatch could never fire. Every port maps its arrows
  to those codes, and every README says the arrows raise and lower shields.
  Now an arrow on an empty command line is a whole order (`c128/src/ui.c`).
- **`ON_PAGE()` was a statement before declarations** in ten functions of
  `ui.c`, which C89 forbids. gcc 2.5.8 rejected all ten. On 80-column
  builds it is now a declaration, which generates no code.
