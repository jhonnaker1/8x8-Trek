# EGA Trek — Commodore 128 (VDC)

First port, **released in v0.9.0** and shipping in
[v0.13.0](../../../releases/latest). The machine the project started on, and
the one every other port is a diff against: `main.c`, `ui.c`, `layout.c` and
`strpool.c` live here and are compiled unchanged by all five.

The VDC gives 80×25 text with a foreground and a background colour on every
cell, which is exactly what the nine-panel console is — so the console renders
as designed, with one compromise: the VDC's palette is fixed and its dark
yellow stands in for EGA's brown.

```sh
make            # build/trek128.prg + the overlay images
make verify     # geometry, dialogs, the briefing, the key table, the budget
make test       # the host-side suite over core/ and the shared UI
make d64        # build/trek128.d64 -- the release artefact
make run        # boot it in VICE
```

**`make` does not run `verify`, and `make d64` does not either** — unlike the
MEGA65, where `make d81` does. Run it yourself. It is what catches an overlay
calling out of its own window, a dialog line wider than its box, or a command
word the keyboard cannot type, and `make ports` from the repository root runs
it here along with every other port's gate and checks the exit status.

## Live figures

Run `make verify`; it is the only authority, and every number below moves.

```
lowram (writable data) 2172 of 2304 used, 132 free
resident $1c01..$a94d, 1459 bytes free below the window at $af00
11 overlays at $af00, distinct load addresses, largest 3892 of 4094 bytes
```

Three pools, and they are not interchangeable:

- **Resident code**, `$1C01` upward. The scarce one. Everything not paged.
- **The overlay window**, 4,094 bytes at `$AF00`. Eleven images, one link, each
  loaded from disk on demand.
- **`lowram`**, `$1300–$1BFF`. Writable data only — the KERNAL's work areas end
  below it and BASIC text starts above. It does not compete with code at all,
  which is why `io_buf` lives there.

The C128 is the roomiest 6502 target in the project. That is worth saying
plainly because it was not always true on paper: the figure sat at 211 bytes in
three documents for four days while the real number was 1,589, because **this
was the one port whose `make verify` did not print it.** A resource nobody
reports is a resource nobody manages; it prints now.

## Bank 1, and why it is not mapped in

Bulk data — the string pool, the music, the overlay index — lives in RAM bank
1, reached through the KERNAL's `FETCH` and `STASH`.

The MMU could map bank 1 in directly and let ordinary loads reach it, but the
code doing the mapping would have to sit in common RAM at the bottom of memory,
because switching banks changes what is visible at every other address. `FETCH`
and `STASH` need no relocation, at the cost of a subroutine call per byte —
fine for a store written once at startup and read a few times a second.

**Getting data there is a different problem from reading it back**, and the
first answer was wrong. Loading the pool a byte at a time through `STASH` was
replaced by **one KERNAL `LOAD` straight into bank 1 via `SETBNK`** — about
eight times faster, and the difference between a visible pause at startup and
none.

What can never go there: anything a caller wants as one contiguous blob.
`io_buf` cannot move to bank 1, and that has been proposed and refused more
than once.

## Overlays

Eleven windows, cut from one link, each an image on the disk:

    ovleval  ovlhof  ovlfront  ovlinfo  ovlrepair  ovlmsgs
    ovlplanet  ovlcmds  ovltitle  ovlevents  ovlxtra

The rules `make verify` enforces are in `core/overlay.h`. The one that matters
most is **rule 4**: only `main()` or a declared pair may call into a window. A
function inside an overlay cannot call `ovl_load`, because it would page itself
out mid-call — so when an overlay fills up, the work goes back to the resident
caller as a request rather than being called directly. `do_use` returns one.

**Measuring the size of a split is not measuring the split.** The call graph
decides: a shared callee has to stay resident, and per-function sizes have to
come from `make nolto`, because with LTO on, a function that looks like 850
bytes as written links as part of a 6,917-byte whole.

This port does **not** take the two opt-in overlays the Atari does
(`TREK_OVL_ENEMY`, `TREK_OVL_MOVE`). Both page code on the hot path, which is
affordable where a swap is a copy out of video RAM and ruinous where it is a
1541 seek.

## The disk

One D64, device 8, `0:NAME,S,R`. `make d64` builds it:

```
trek128        the program
strings.dat    every word on screen
music.dat      the music
brief.txt      the briefing, streamed a page at a time
ovl*           eleven overlay images
trek.scr       the hall of fame
```

Written against the raw `cbm_k_*` primitives, because llvm-mos's `<cbm.h>` has
the entire high-level file API inside an `#if 0`. That suits the port: it wants
the status channel under its own control anyway.

**The briefing is streamed and drawn as it arrives** — there is no page buffer,
because `lowram` has no room for one.

**JiffyDOS is the assumed environment**, at both ends: `JiffyDOS_C128_6.01` as
the kernal and `JiffyDOS_1541-II_6.00` in the drive. Every disk timing this
project has ever taken was JiffyDOS-accelerated, and that is Jamie's call
rather than an accident. It is also why VICE prints `C128MEM: Warning - Unknown
kernal image`.

## Three traps this port found first, and every other port inherited

**`.data` is never initialised unless you link `-lcopy-data`.** Moving
`c_writeable` to `lowram` gives `.data` a run address that differs from its load
address, and llvm-mos only bridges the two if `copy-data.c.obj` is linked in.
The Commodore targets never link it, because with both aliases on one region
their `.data` needs no copy. So `.data` was simply never initialised, and the
sound driver read whatever was in low RAM. **The Atari hit exactly this on
2026-09-11** when its writable half moved into the space Atari DOS vacated — the
second machine to meet the same debt, and it was caught before it shipped only
because this file records it.

**Inline asm containing a `jsr` must declare the `"p"` clobber.** Without it the
compiler emits a compare before the call and branches on the flags after.
Latent in the released C128 and X16 builds until 2026-09-08.

**The soft stack has no guard of its own.** llvm-mos points it at the top of
free memory and grows it down; the stock script will let a build succeed and
then corrupt itself with the linker none the wiser. This port reserves for it
explicitly — and the reserve was **measured**, not guessed: the first guess of
64 bytes was overrun by 79. The Atari repeated the same fault on 2026-09-11,
with its stack inside the overlay window, corrupting the running image under
its own feet.

## Quitting resets the machine, and the note that said otherwise was wrong

Answering NO at "Play Again?" used to drop into the C128's machine-language
monitor — deterministically, `PC=$005B`. The cause is llvm-mos's exit path
banking ROM over this program's own code, and the fix is `plat_exit()`: reset
the machine, which is the only way to hand back a C128 whose entire BASIC text
area this program is sitting in.

**Why it stood for so long is the useful part.** A note in `NOTES.md` claimed
there was "no way to observe the machine from a session on this host" — which
was simply wrong; VICE's binary monitor was there all along. The wedge was
never diagnosed, only assumed, and the assumption cost every player an exit.

The trap underneath is real and caught the session that fixed it: **a program
still blocked in `kb_waitkey()` is indistinguishable from a wedged machine.**
Both ignore the keyboard, and the C128 still shows `READY` because BASIC printed
it before the program ran. Deciding needs a question only a live BASIC can
answer — hence asking it for arithmetic.

## What play found, and it is the most productive thing in the project

Four sessions at the keyboard have found four bugs no build check could see:

- `FIX` was missing half its command.
- Nine letters of the alphabet could not be typed — and the self-destruct
  password `JAMIE` has a J and an I in it.
- A yes/no question was drawn in the wrong panel.
- The last page of the briefing never waited for a key.

Each lived exactly where an automated check does not go, and each fix shipped
the check that closes its class.

## What is open

**Play.** Whether the game this adds up to is survivable, readable and fair is
not a question any build check reaches, and it is item 11 on THE OPEN LIST in
[`NOTES.md`](../NOTES.md) — one of only two still open, both wanting a person
at a keyboard.

Colour per message is built (2026-09-10) and ships here. The MAIN VIEWER's other
nine instrument pages are deferred on every port.

## Verifying the glyphs against the machine, not against a guess

VICE ships the C128 character ROM, and `make test` reads the bitmaps out of it
— `test/test_panels.c`, looking for `chargen-390059-01.bin` in the usual VICE
install paths. It is how you find out whether the screen codes the port picked
actually draw the shapes it thinks they do, rather than trusting a table.

**On success it prints nothing of its own** — it is folded into
`console panels: all checks passed` — and if the ROM is not installed it prints
`(no C128 chargen ROM found -- bitmap check skipped)` and carries on. So the
two outcomes that look most alike from a distance are "ran and passed" and
"never ran": one of them says so and the other is silent. If you have VICE,
check that the skip line is absent rather than assuming.

The box-drawing set the console uses is authored here, not lifted from
Commodore's ROM. Checking it against the ROM is about what the machine *draws*,
not about where the design came from.
