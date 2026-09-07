# EGA Trek — Amiga (OCS/ECS, KS2.0+)

Fourth port, **complete 2026-09-06 and played end to end on the machine**:
title, briefing, setup, console, orders, dialogs, save, restore, the hall of
fame, the endgame, play-again, and a clean exit back to the shell.

    make            build the game, the data and run the tempo test
    make release    build/egatrek-amiga.zip
    # then, at the Amiga shell:
    work:egatrek

## Why this target is the cheapest left

**No overlays, no far memory, no banking.** `core/farmem.h` has said "Amiga —
no banking needed, a plain array" since the seam was designed, and this machine
has more memory than the game needs. That deletes the single most expensive
machinery in all three 8-bit ports: the window, ten staging regions, the
resident/overlay split, the build stamp, `ovl_load`, the four overlay rules and
the budget arithmetic. **Three of the four bugs found in the v0.10.0 cycle came
out of that machinery**, and none of them can exist here.

640×200 in four bitplanes is the console exactly — 80 cells of 8 pixels by 25
rows of 8, nothing left over — and four planes at 640 wide is the OCS/ECS hires
maximum, so sixteen colours against the fifteen the console uses.

## The glyph set: fifteen, not eleven

Re-derived from the shared sources rather than counted by eye — a sweep of
every `scr_put`/`hline`/`vline`/`fill_rect` argument in `ui.c`, `layout.c` and
`main.c`. The eleven box-drawing characters are the obvious ones; **the other
four live in panels nothing had drawn yet**, so an eyeball count missed them:

    98   BADGE_DISC_TOP    lower half filled -- rounds the badge disc's top
    226  BADGE_DISC_BOTTOM its reverse, and needs no entry of its own
    160  BADGE_DISC_BODY   solid; also the block cursor, reverse of 32
    228  SYS_BAR_GLYPH     seven rows on an eight-pixel pitch, which is what
                           leaves a hairline under each systems-status bar
    81   the ship's saucer, next to four G_HLINE and a solid block

Reverse video is a **rule, not entries**: codes 128..255 are their base glyph
inverted, so 160, 226 and 228 fall out of 32, 98 and 100. Writing those three
by hand would be three more chances to disagree with the rule.

**A code nobody drew renders as a hollow box, not as nothing.** Every other
port hands these to a charset that has something at every position; here the
set is finite and authored, so a missed code would be invisible — which is
exactly the bug that leaves a panel looking merely empty. `smoke.c` draws one
deliberately undrawn code every run, because a marker that has never fired is
not a marker.

**It has paid for itself twice.** Two of the fifteen were missed on the first
pass. Then it fired on the storage test's own labels, which is how screen
codes 27 and 29 turned up — `ui.c` draws the play-again prompt as `[YES]` and
`[NO]`, and without those the box would have read `□YES□  □NO□` at the end of
somebody's game. topaz has both brackets, so they come from the ROM exactly;
the up and left arrows at 30 and 31 are drawn, since they have no ASCII to
borrow. The pound sign at 28 is deliberately left to the marker: nothing asks
for it, and an invented glyph for a character nobody uses is worse than a
visible gap.

## The glyphs are this port's own artwork

`layout.h` names the box-drawing characters by **C64 screen code** — `G_HLINE`
is 64, `G_VLINE` 93 — and topaz has `@` and `]` at those code points. The
MEGA65 gets away with reusing them because the C65 charset is character-for-
character the C64's; this machine does not.

So `src/amigagfx.c` carries the set as 8×8 bit patterns: a
line sits on rows 3 and 4 and in columns 3 and 4, which is what makes a
vertical meet a horizontal in the middle of a cell and two adjacent cells join.
**What was checked and what was copied.** The C128 chargen ROM was read to find
out what SHAPE each code is meant to be — the same thing
`c128/test/test_panels.c` does to check our screen codes. Three of them are
pure geometry and can only look one way (a half block is a half block, a
seven-row bar is a seven-row bar), so knowing the shape is knowing the bytes.
The rest are drawn here, including the disc, which is this port's own circle
rather than Commodore's. Checking against that ROM is fine; shipping it is not
— the same rule that made `tools/make_music.py` compose the music rather than
extract it.

Letters, digits and punctuation come out of `topaz.font` 8 in ROM, read
straight from `tf_CharData`. No font to author for ASCII.

## Why the planes are written by hand

`graphics.library`'s `Text()` draws in one pen through a RastPort, and every
cell here carries its own colour — a `SetAPen`, `Move` and `Text` per cell,
two thousand times. A cell is 32 bytes of plane data (eight rows, four planes)
and writing it directly is both simpler and exact. The RastPort is still
opened, because the ROM font is read through it.

## The input seam, and the one thing it got wrong

Intuition hands over a character, converted through the **user's own keymap**,
so a non-US keyboard works without this port knowing anything about it. No scan
table, no encoding to probe — the C128 needs a hand-transcribed table of fifty
row/column pairs and two `make verify` checks to guard it, and the X16 needed a
probe to discover `GETIN` returns lower-case ASCII rather than the PETSCII its
own comment claimed.

`IDCMP_VANILLAKEY` carries the characters; the cursor keys have no ASCII, so
they arrive only as `IDCMP_RAWKEY` and exactly two raw codes are taken from
that stream. **Measured, because enabling both classes could have delivered
every letter twice** — it does not.

    m w 5 q   ->  M 077, W 087, 5 053, Q 081     letters fold to upper case
    up, down  ->  001, 002                        KB_UP, KB_DOWN
    ESC       ->  027                             KB_ESC, no mapping needed
    RETURN    ->  013
    backspace ->  008  ... which is WRONG

**Backspace is 8 here and the shared code deletes on 20.** `read_field()` and
`ui_read_command()` both test `KB_DELETE`, which is PETSCII's 20, so backspace
did nothing at all: the commander's name could be typed but not corrected.
Caught by echoing every key's code on screen rather than by assuming ASCII
lines up. Backspace and Del both map to `KB_DELETE` now, verified as 020.

## kb_init was dead code on two ports

`m65input.c` and `x16input.c` each defined a `kb_init()`, and neither
`input.h` nor `main()` ever mentioned it — so neither was ever called. Found
while writing this one. It matters most where the machine **queues**
keystrokes: the game is started by typing `work:egatrek` at a shell, and the
RETURN that launches it is still in Intuition's message port when the title
screen asks for a key, so the title dismisses itself. It is declared, called
once before the title, and implemented on all four ports now — the C128's is
an empty function with a comment saying why (it scans CIA1's matrix; there is
no queue to drain).

## Storage, and the first port where writing works

AmigaDOS `Open`/`Read`/`Write`/`Close` sit straight under the five `plat_*`
functions. No KERNAL channel to open and close in the right order, no
hypervisor trap, no device number, no secondary address, no 8K window and no
512-byte sector to buffer. The MEGA65's `plat_write_all()` is still a stub
returning `STOR_ERROR`; here it is one `Write()`.

**`PROGDIR:` is the whole of the path design.** The game asks for bare names
like `STRINGS.DAT`, because on the other three machines a bare name means "the
disk in the drive". Here it would mean the shell's current directory —
wherever the player happened to be standing — while the data files are in the
program's own drawer. `PROGDIR:` is AmigaDOS's assign for exactly that, so
`work:egatrek` finds its files whether it was started from `WORK:`, from
`SYS:`, or from Workbench.

`make storetest` builds a test that runs on the machine and prints twelve
PASS/FAIL lines: the round trip, the byte count, that a file longer than `max`
is an **error and not a truncation**, that a missing file is `NOTFOUND` and not
`ERROR` (the setup screen has to tell "there is no save" from "the disk went
wrong"), that the streaming path returns the same bytes and 0 at EOF, and that
`plat_open` twice in a row is fine — which is the rule the X16 broke in a way
that made every load but the first fail.

## The string pool, and the first big-endian machine on the project

`core/farmem.h` has said "Amiga — no banking needed, a plain array" since the
seam was designed, and that is the whole of `amigafarmem.c`: a far offset is an
index, `far_read` is a `memcpy`. The C128 reaches bank 1 through a KERNAL call
per byte, the MEGA65 through DMA, the X16 by paging 8K windows and splitting
every read that straddles one. With that in place `c128/src/strpool.c` is
**shared unchanged** — the panel titles on screen come out of `STRINGS.DAT`.

**Except that it did not work, and the reason is this machine's byte order.**
`STRINGS.DAT` is written little-endian by `tools/gen_strings.py`, and
`strpool.c` read its 16-bit count and offsets straight into a `uint16_t`.
That is correct on three 6502 ports and wrong on a 68000: the count came back
byte-swapped, failed its own guard, and the game would have run wordless with
nothing on screen to say why. Both reads are composed from bytes now, which is
right on either byte order and costs nothing on the 8-bit ports.

**Checked by breaking it**: with the byte-wise read reverted, the smoke test
says `NO POOL — PANEL TITLES WILL BE BLANK`; with it restored, `STRINGS.DAT
LOADED, TITLES ARE POOLED`. That guard in `str_load()` — refusing a pool whose
count disagrees — is what turned a silent garbage-on-screen failure into a
legible one, and it was written for a stale disk rather than for endianness.

## The game runs

`make game` links `main.c`, `ui.c`, `layout.c`, `strpool.c` and all of `core/`
against the five Amiga files — 81K, and **no overlays to arrange**. Played on
the machine to: the title screen, the twelve-page briefing streaming out of
`BRIEF.TXT`, the setup screen, the nine-panel console with a live game, orders
typed at `CMD:`, and a modal dialog.

    make game data
    # then, at the Amiga shell:
    work:egatrek

Three seams proved themselves in the real game rather than in a test: the
briefing pages come through `plat_open`/`plat_read`, every word on screen comes
from `STRINGS.DAT`, and the title screen **did not dismiss itself** — which is
`kb_init()` throwing away the RETURN that launched the program.

Sound is stubbed, not broken: `snd_enabled()` answers "off" and stays off,
which is honest. `main.c` already treats missing music as a luxury it can do
without.

## Sound: Paula, and why the tempo is a counting problem

Paula has no tone generator. A channel plays back a buffer in CHIP RAM on a
loop, so "play a note" means having a waveform and choosing a **period** — the
clock ticks between samples — rather than writing a frequency. The waveform is
a 16-step square wave, because that is what the original made: EGA Trek on a PC
is one square wave out of the speaker, and `sid.c` says "50% pulse, the square
wave the original actually made".

Sixteen steps rather than two so the period stays in Paula's usable range. Its
floor is 124, and with a 16-sample wave the highest note reachable is
`clock/(124*16)`, about 1.8 kHz; the music's highest is 1480 Hz. The clamp in
`voice_note()` is the guard, not the plan.

Two channels, mirroring the SID port's split: 0 is music, 1 is effects, so a
laser does not cut the music off. The region is **read** from
`graphics.library` rather than assumed, because it decides both the period for
a given note and how many frames a quarter-second beep is.

**The tempo is measured, not listened to.** The tracks are written in ticks of
the PC's 18.2065 Hz timer. It would be easy to tick once per `snd_poll()` —
the key loop calls it about fifty times a second — but `snd_poll()` is also
called from places that run at no fixed rate, so the tempo would wander with
whatever the game was doing. That is the MEGA65's bug in a different costume.
This one converts from `DateStamp()`, which counts fiftieths of a second.

`make -C amiga test` runs that conversion on the host: 1092 ticks in 60
seconds, 18.2000 Hz against 18.2065 wanted, and the same count when the
fiftieths arrive 25 at a time (a disk load blocks, and the accumulator must not
drop the remainder). Checked by breaking it — doubling the rate fails the
build. **Both previous ports shipped a tempo bug and neither was caught by
listening**, because "a bit fast" is not obvious in music nobody has heard
before. Both were caught by counting.

### The bug this seam is shaped to catch, and I made it anyway

`snd_poll()` has to be called from inside the keyboard wait — that loop is
where the program spends its idle time and it is the driver's only chance to
run. The stub version of `amigasnd.c` said exactly that, in a comment, and
`amigainput.c` was written without the call. Paula came up with master DMA on
and every channel silent. Found by asking the emulator for its audio state
rather than by listening, which is the same reason the tempo is counted.

## The screen is as tall as the display, and the console is centred in it

The console is 80×25 cells of 8×8 — 640×200, which is **exactly an NTSC
screen**, and that is where the number came from. PAL gives 256 non-interlaced
lines, so a 200-line screen left the bottom 56 empty: a fifth of the display
doing nothing, with the game jammed against the top. Jamie spotted it.

**The X16 had the same shape of fault** — VERA's text mode is 80×60 while the
console is 80×25 — and it was fixed there by doubling the row height. That
trick is not available here: these are 8×8 glyphs blitted into bitplanes, and
stretching them would mean a second set at another height, or interlace, which
flickers on a real display. So the screen opens at the display's own height and
the console is placed in the middle, 28 lines above and below on PAL. On NTSC
the offset is zero and nothing changes. 640×256 in four planes is 81,920 bytes
of chip RAM against 64,000; both are comfortable.

**It uncovered a second bug that had been invisible.** `ShowTitle(scr, FALSE)`
hides a screen's drag bar *behind backdrop windows*, so it does nothing until
there is one — and it was being called before `OpenWindow`. Nobody could see
that while the console filled the screen from row 0, because `scr_put` writes
the bitplanes directly and simply painted over the bar. Centring the console
left a margin, and the bar appeared in it. `scr_clear` now blanks the whole
bitmap by plane rather than the console's 25 rows, which is both correct for
the margins and much quicker than two thousand `scr_put` calls.

## Played through, and what that proved

Driven on the machine rather than reasoned about:

* **SAVE** writes `EGATREK.SAV`, 625 bytes, into the program's drawer.
* **Restore** brings it back — warp 5.0, the same quadrant, the same chart and
  the same scan, where a fresh game would be warp 1.0 and a different galaxy.
* **The hall of fame** reads `TREK.SCR`, and with a table it can beat it writes
  the file again with the commander inserted **into the right rank band** —
  a level-3 captain lands in the captain slots, not at the top.
* **The endgame** — self destruct, loss memo, evaluation, hall of fame — and
  the play-again loop back to the title with setup asked again.
* **Quitting returns to the AmigaDOS shell cleanly**: prompt back, no hang, no
  crash. Worth stating, because the C128 BRKed into its machine-language
  monitor here and shipped that way in v0.9.0.

A −930 score does **not** qualify for an empty table, so the write path had to
be exercised against a seeded one. That is correct behaviour, not a bug — worth
writing down, because "the file was not written" looks identical to a broken
seam until you read `hof_offer`.

## Running it

Amiberry mounts a **host directory** as an Amiga volume, so there is no ADF to
build and nothing to copy — `tools/egatrek.uae` points `WORK:` straight at
`amiga/build`. Build on the host, type `work:smoke` on the Amiga.

    /Applications/Amiberry.app/Contents/MacOS/Amiberry -G -f amiga/tools/egatrek.uae

`tools/amiga_type.py` types at the shell and takes screenshots through
Amiberry's IPC socket. **Two things nothing documents**: the protocol is
TAB-separated — a space-separated command answers `ERROR Unknown command`,
which reads exactly like an unsupported feature — and `SEND_KEY` takes raw
Amiga keycodes with separate press and release, not characters.

## What is built, and what is next

    DONE   video seam: vdc_init, scr_put/puts/clear/hline/vline/fill_rect,
           wait_vsync, vdc_shutdown, plat_exit, the EGA palette
           glyphs: all fifteen, verified on the machine, with a marker for
           any sixteenth nobody has noticed yet
           input seam: kb_init, kb_waitkey, kb_entropy
           storage seam: all five plat_* functions, twelve checks passing
           on the machine -- and WRITING WORKS, which it does not on the
           MEGA65
           far memory: a plain array, so c128/src/strpool.c is shared
           unchanged and the panel titles come off the disk
           the game: main.c, ui.c and all of core/, linked and PLAYED --
           title, briefing, setup, console, orders, dialogs
           sound: a Paula driver on two channels, title track confirmed
           playing and stopping on the machine
           played through: SAVE writes EGATREK.SAV and the setup screen
           restores it (warp 5 and the same chart come back); the hall of
           fame reads TREK.SCR, inserts into the right rank band and writes
           it again; the endgame, play-again and the quit all work
    NEXT   a human playing it. Everything above was driven by a script.
The smoke build links the shared `layout.c` and supplies a **stub `S()`** for
the seven panel titles, because the string pool needs the file seam that is not
built yet. It is replaced by `c128/src/strpool.c` when storage lands.
