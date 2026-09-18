# EGA Trek on the Foenix F256K — STARTED 2026-09-17

**First light: llvm-mos builds a PGZ that FoenixMCP loads and runs, and it
draws to the text matrix in colour.** That was the risk worth retiring before
anything else, because the fallback — cc65 — would have decided the whole
shape of the port.

| | state |
|---|---|
| toolchain: llvm-mos → PGZ → pexec | **works**, `make hello` |
| rig: MAME `f256k`, Lua, SD-card image | **works**, `make run P=hello` / `make check P=hello` |
| video: `f256vid.c` at 80x30, 8x16 | **works** -- the console draws, `make frame` |
| frame timer: kernel `SetTimer` query | **60.00 Hz measured**, in the driver |
| sound: SN76489 PSG | **calibrated at three points**, no driver yet |
| font: authored box set, screen-code order | **works** -- built at init into font RAM |
| keyboard: `f256key.c` via the event queue | **works** -- nine keys checked against `input.h`, `make keys` |
| storage: `f256stor.c`, all five `plat_` calls | **works** -- ten checks, `make store` |
| far memory, overlays, sound driver | **not started** |

## THE SCREEN IS BIGGER THAN THE GAME, AND THAT TURNED OUT TO BE A GIFT

640x480 at an 8x8 cell is **80x60**, measured with corner markers rather than
read off a spec. The console is 80x25. Drawn straight, the game would sit in
the top third of the screen with 280 of 480 pixels black.

`$D001` bit 2 is Tiny Vicky's **DOUBLE_Y**: the character cell becomes 8x16
and the grid becomes **80x30**. Verified the same way — a frame drawn at
80x30 touches all four edges. Two things follow, and the second is the point:

* 25 rows of 30 is a two-row margin, not a 35-row hole.
* **An 8x16 cell is closer to the original than any other port gets.** EGA
  Trek runs at 640x350 in an 8x14 cell, and every other 8-bit port here draws
  it at 8x8 and loses the vertical detail. This one keeps it.

`DOUBLE_X` (bit 1, 40 columns) is deliberately left off — the eighty columns
are why this machine is worth porting to at all.

The two-row offset lives **in the driver**, not in the layout. `panels[]` is
measured off the original and shared with eleven other ports; this machine
being five rows taller than the console is `f256vid.c`'s problem and nobody
else's.

## THE FONT IS RAM, SO THIS PORT AUTHORS ITS GLYPHS

The machine's font is 2K at `$C000` on I/O page 1, and it is **not CP437** —
`$C0`-`$DF` are dither patterns and symbols, so there is no box-drawing set to
borrow. The ASCII half is correct, which is what matters: `$41` reads
`3C 42 42 7E 42 42 42 00` beside a capital A on screen, and that pairing is
what proves both the page and the read.

So `vdc_init` rebuilds the font in **C64 screen-code order** — the ASCII half
moved down, the box set drawn on top, the reverse half generated — which
leaves the shared `layout.h`'s `G_*` constants working unchanged and gets
reverse video as a rule rather than 128 more glyphs. The box glyphs are this
project's own artwork, shared with `atari/src/vbxevid.c` and
`amiga/src/amigagfx.c`, which drew them first for the same reason.

**The order is what makes it need no scratch buffer.** Codes 1..26 are copied
from ASCII 65..90 — font offsets `$208..$2D7` down to `$08..$D7` — and the box
set then lands at codes 64..127, `$200..$3FF`, *on top of the letters it was
just copied from*. Correct, but only because the copy happened first. Three
passes, none overlapping, no 2K of scratch on a machine that has about forty.

## THE KEYBOARD, AND THE QUEUE IT HAS TO SHARE

One `NextEvent` queue carries keystrokes, every byte of file I/O and the
kernel's timers. **A key wait that drains it during a load eats the file's
data events; a file read that drains it while the player is typing eats the
keys.** So there is exactly one pump in this port — `f256_pump()` in
`f256key.c` — and it sorts: keys into a ring, everything else held for the
storage layer. If a second non-key event arrives before the first is claimed,
`f256_other_lost` counts it, which is the difference between a storage bug
that shows up as a failed load and one that shows up as a file that is subtly
short.

Measured with `src/keyprobe.c`, which logs raw event bytes rather than
summarising them:

| key | raw | ascii | |
|---|---|---|---|
| letters | unshifted ASCII | the shifted character | so the port folds case |
| ENTER | `$94` | `$0D` | |
| DEL / BKSP | `$92` | `$08` | |
| RUN/STOP | `$BC` | `$03` | **this keyboard is a C64 layout — there is no ESC key** |
| CRSR UP | `$B6` | `$10` | matched on **raw**: the ASCII is a control code |
| CRSR DOWN | `$B7` | `$0E` | other paths could plausibly produce |

`key.PRESSED` (8) and `key.RELEASED` (10) **both** arrive — counting both
doubles every keystroke — and modifiers come through with `flags` bit 7 set
and no ASCII, so SHIFT would register as a keystroke of its own if it were not
dropped.

**Two events are already queued when the game starts**: a `file.CLOSED` from
pexec closing the PGZ it just loaded, and a timer. Neither is a key, so the
Amiga's fault — the RETURN that launched the game dismissing the title screen
— cannot happen here. `kb_init` drains anyway, because "cannot happen" is a
claim about a queue nobody has looked in lately.

`make keys` types nine chosen keys and checks what `kb_waitkey` returns
**against `input.h`'s own constants**, not against my judgement of the output.
And it can fail: with the case fold removed `q` comes back 113 instead of 81,
and with the arrow mapping removed CRSR UP comes back 16 instead of 1 — two
breaks, two mismatches, nothing else moved.

## STORAGE IS ASYNCHRONOUS, WHICH NO OTHER PORT HERE HAS TO DEAL WITH

Every other port opens a file and reads it. On the F256 a call only
**requests** the work; the answer arrives later as an event, on the same queue
as the keyboard. So each `plat_` function is a request and a wait, and the
wait keeps pumping keystrokes into the ring — because the alternative is a
player who loses everything they typed during a load, with nothing anywhere
reporting it. **uno's vendored `kernel.c` waits with `default: continue;`**,
which drops exactly those keys.

Three traps, all in the shape of the API rather than any one call:

1. **A read is two steps.** `File.Read` requests, a `file.DATA` event says how
   many bytes are ready, and `ReadData` then copies them. Stop after the
   event and the buffer holds whatever it held before — *and the byte count is
   still correct*, so nothing looks wrong.
2. **`delivered == 0` means 256, not EOF.** The count is a byte and a full
   read wraps it. This port sidesteps the ambiguity rather than handling it:
   reads are chunked at **255**, so a delivered count of 0 cannot arise. The
   `delivered ? : 256` in the code is belt-and-braces and is **not exercised
   by the test** — said plainly rather than implied to be verified.
3. **One queue for everything.** See `f256evt.c`.

And **every wait has a deadline** — two seconds. The reference spins in
`for(;;)`; on a machine whose ordinary failure is an absent SD card, that is a
game that hangs with no message.

### The kernel never sends `file.NOT_FOUND`

It is in the event enum and nothing in FoenixMCP emits it — the same as
`clock.TICK`, and found the same way: by opening a file that is not there and
reading the event number back. A missing file arrives as `file.ERROR` (`$38`),
which is also what a broken card would send.

So the line this port draws is between **an answer and no answer**, which is a
distinction it can actually make. `file.ERROR` on a read-open is
`STOR_NOTFOUND` — the drive answered, and it said no. Silence until the
deadline is `STOR_ERROR` — the drive is not there. `storage.h` blesses exactly
this. The honest caveat is that a genuine I/O error on a file that *does*
exist comes out as NOTFOUND; `Directory.Read` could tell them apart by
scanning, and no caller in this game distinguishes the two codes, so that is
written down rather than done.

`make store` runs ten checks and they can fail. Skipping step two of the read
leaves **LENGTH IS 600 passing while EVERY BYTE MATCHES fails at byte 0** —
the pair discriminates. Dropping non-file events the way the reference does
fails only the keystroke check. A short write over a long file is verified to
**replace** rather than append, which would otherwise give a save that loads
and is wrong.

## THE FRAME TIMER IS A KERNEL CALL, NOT A RASTER READ

`SetTimer` at `$FFF0` with the QUERY bit set queues nothing and returns the
kernel's own frame counter. **60.00 Hz, measured twice** — 300 frames in 5.000
emulated seconds, once in a probe and again through `wait_vsync` in the built
driver, because the two are the same eight lines compiled in different
translation units and this project has an llvm-mos miscompile on record.

Not the raster, for a reason worth writing down: **MAME's f256k returns
`m_screen->hpos()` — the horizontal dot position — from the scan-line
registers at `$D01A`/`$D01B`**, and a hard-coded `0` from the column registers
at `$D018`/`$D019`. The variable is even named `line` in the source. Two
attempts to pace off it returned -5944 Hz and 5457 Hz; both were sampling
noise. On real hardware that register is the scan line, so this is a fact
about the rig rather than the machine — and the kernel call is the better
answer either way.

Not an event, either. `clock.TICK` is in the kernel's event enum and **nothing
emits it**. And a frame wait built on events would pull from the one queue
that also carries every keystroke and every byte of file data. The tick probe
counts keys alongside frames to prove the query takes none: zero.

## THE RIG: MAME, AND WHY THERE IS NOTHING BETTER TO AUTOMATE

Three F256 emulators exist. **MAME's `f256k` driver (dtremblay's fork) is the
only one this machine can run, and it is also the only one with a scripting
interface** — so the choice `commodore-uno` made is the choice there is:

| | | |
|---|---|---|
| **MAME `f256k`** (dtremblay) | macOS/Linux/Windows | Lua: memory, registers, breakpoints, key injection, snapshots, `-autoboot_script` |
| **FoenixIDE** (Trinity-11) | **Windows only** — C#/WinForms `.msi` | GUI debugger; nothing to drive from a script |
| **Foenijs** (wf2) | browser | `?url=&run=` launches a program, but its debugger is for a human at the keyboard |

MAME is not the compromise here — it is the one with the automation. The
friction was never MAME; it was **getting the PGZ onto the SD image.**
`commodore-uno`'s recipe (and this port's first one) attached the image as a
device and asked `diskutil` to mount it: seconds of disk arbitration per run,
and a leaked device node whenever a run died mid-way. **`mcopy` writes the FAT
partition in place instead — 7ms, no mounting, no privileges** — and FoenixMCP
still reads the same filesystem and pexec still loads the same file. The
partition offset is read out of the MBR rather than assumed, because 2048 is
right for this image and would be wrong for the next one.

`make check` is the automated half: it boots, types `/- <name>` at the
SuperBASIC prompt the way a person does, reads the completion marker, takes a
snapshot and exits. **The marker's address comes from the ELF**, not a
constant — a stale one reports somebody else's byte as the answer. Whole cycle,
including the boot: **3.6 seconds.**

The control matters more than the pass. Launching a name pexec cannot find
reads `$00` where a real run reads `$5A`, so the rig can tell "ran" from
"never started" — which is the thing a green light is worthless without.

`make run` leaves the machine up at the prompt, throttled, for a person.
`make check` is the one that passes `-nothrottle`, and it is never for a human.

## WHY llvm-mos AND NOT cc65, WHICH IS THE PROVEN ROUTE HERE

`commodore-uno/f256` is a complete working F256 port built with cc65, and its
toolchain directory is vendored from the FoenixMCP kernel repo. Taking it would
have been the cheap start. **Two measurements decided against it**, neither
about the thing everyone quotes:

  * **Code size is NOT the discriminator.** On a fair full link of `core/` —
    the same 78 functions pinned into both, because cc65 links whole modules
    while llvm-mos runs LTO and `--gc-sections` — cc65 came out at **31,035
    bytes against llvm-mos's 31,222. Within 1%.** The "13% smaller" this
    project has quoted, and uno's "2.3× tighter", are both unreproduced.
  * **What actually differs is the memory model.** cc65 carves its stack out
    of the program's own space; llvm-mos puts `__stack` outside it. Against
    the ~40K this machine gives a program, that is the number that decides.
  * **And cc65 would put C89 on the SHARED half forever.** `c128/src/ui.c` has
    eight `declaration-after-statement` sites; every other shared file is
    already C89 and so is `core/`. One port must not constrain twelve ports'
    shared sources in perpetuity — though **extending the root `c89-check` to
    the shared half is worth doing anyway**, since it guards `core/` only and
    the shared UI drifted to C99 with nothing noticing.

## WHAT THE MACHINE IS LIKE

**Its MMU is the opposite of the Plus/4's.** Eight slots of 8K, four MLUTs,
`$0000` the control register: you choose what is in each slot, so **nothing has
to be hidden to reach RAM and no kernel call needs a banking shim**. That one
cost is what made the Plus/4 a fourth seam bigger than the other three.

**But the kernel does take something away.** FoenixMCP's IRQ handler expects
I/O page 0, so an interrupt landing while the page is 2 or 3 reads matrix RAM
instead of its registers. Every screen write is bracketed `sei`/`cli` and
leaves the page at 0 — uno documents this and it is not optional.

    $0000 MMU_MEM_CTRL   $0001 MMU_IO_CTRL (0=regs 2=char 3=colour)
    $0002-$003F MCP's    $0040-$00EF free zp   $00F0-$00FF kernel args
    $2000-$9FFF program  $C000 matrix (paged)  $D800/$D840 colour LUTs
    $D608 PSG (page 0)   $FF00 kernel jump table

**80×60 cells of 8×8**, so the console's native **80 columns** fit with rows to
spare — `layout.c`, not `layout40.c`. **6.29 MHz**, three to six times every
other 8-bit port here.

## SOUND IS THE PSG, AND THAT IS A DELIBERATE CHOICE

MAME emulates two SIDs, two SN76489 PSGs and an OPL3, so the emulator cannot
decide it. **The PSGs are inside the Beatrix FPGA — every owner has them.**
The SIDs are the only audio anyone describes as optional (the jr's are
sockets, the K2's are FPGA, the K's are undocumented either way). Jamie's call,
and the right one: *we have to have sound, and neither of us owns the machine.*

Measured on the hardware, three points, because this project shipped a port an
octave flat off one check:

    wanted   predicted      MEASURED    error
      440    440.4 (N=254)  440.8 Hz   +0.09%
     1000    998.8 (N=112)  998.4 Hz   -0.04%
      200    200.1 (N=559)  200.2 Hz   +0.05%

`f = 111,860.78 / N` — **the same numerator `plus4/src/tedsnd.c` already
carries**, without TED's `1024 - N` inversion.

## THE ONE SEAM WITH A GENUINELY NEW SHAPE

**Storage is event-driven.** A jump table at `$FF00`, `error` is the carry
flag, and every operation is asynchronous: issue the call, then pump
`NextEvent` until your event arrives. Three traps, all visible in uno's loops:

  1. **Read is two steps** — `File.Read` requests, and on `file.DATA` you must
     call `ReadData` to copy the payload. The data is not in the event.
  2. **`delivered == 0` means 256, not EOF** — `buflen` is a `uint8_t`, so a
     full read wraps to zero. A reader that treats 0 as end-of-file truncates
     every file at its first full block, silently.
  3. **One queue for everything.** Keyboard, mouse, clock and file events all
     arrive through `NextEvent`, and a file loop that `continue`s past what is
     not its own **discards keystrokes**. On this machine input and storage are
     coupled in a way no other port here has.

## NOT YET TRUE

No `verify`, so this port is in neither `RELEASE_PORTS` nor
`tools/check_ports.py` — **a port with no gate makes `make ports` true and
meaningless**, and it joins when it has one. Nothing but `hello.c` exists: no
driver, no storage, no keyboard, no overlays. The `$A000-$BFFF` region may be
reachable and is **not assumed** until measured.
