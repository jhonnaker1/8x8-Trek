# EGA Trek — CoCo 3 + SuperSprite FM+

Seventh port, **started 2026-09-12** and **not released**. It links, it fits,
and its storage, font, overlay machinery **and video driver** are real —
`make vidcheck` proves the console reaches VRAM on the card. **Sound and input
are still stubs, and nobody has played it**, so nothing below is a claim that
the game is playable on this machine. It is not yet.

```sh
make           # build/egatrek.bin -- the whole game, resident
make early     # link it and read the overflow: the question this port turns on
make font      # regenerate src/font6x8.h from the authored pictures
make overlays  # cut the overlay images (currently REFUSES ITSELF -- see below)
make vidtest   # build the VRAM-readback program
make vidcheck  # run it on the card under MAME and check what it read back
```

The root `make ports` gate runs `make` here, because there is no overlay budget
for a `make verify` to check yet.

## The machine, and why it needs a card

A stock CoCo 3 gives eight foreground colours per cell at 80 columns. **The
console uses fifteen of EGA's sixteen** — the four Mongol ship types are told
apart by colour alone — so the stock machine was measured, passed its colour
test and was dropped anyway.

A **SuperSprite FM+** carries a **Yamaha V9958** and a **YM2413**, and that
changes the answer. The V9958's **GRAPHIC6** is 512×212 with sixteen colours
per pixel: a 6-pixel font gives 85 columns and 8-pixel rows give 26, so the
console fits with room over. **That is the Amiga's shape — a bitmap with a
software font — not the C128's.**

    $FF78  data        $FF79  address/status
    $FF7A  palette     $FF7B  register indirect

which is the MSX `$98/$99/$9A/$9B` layout moved into the CoCo's slot window.
`R#0=$0A, R#1=$40, R#9=$80, R#8=$08` sets GRAPHIC6: 256 bytes a line, 54,272
bytes a screen, and **the VRAM address counter carries past 16K by itself**, so
nothing has to touch a bank register mid-blit.

**MAME renders the card's screen black no matter what the VDP is doing.** Do
not read that as the driver failing — the CoCo's own VDG output is what shows.
**Verify by reading VRAM back**, not by looking at the window.

## The blit cost was the real risk, and it is measured

**10.625 cycles a byte** to the V9958, timed under MAME with `totalcycles`.
A 6×8 cell blitted on its own costs 512 cycles; the same cell **walking
scanlines across a text line costs 257** — the loop's shape is worth a factor
of two. A 24×3 panel is 10.5 ms and a full 80×25 repaint is **0.287 s** at
1.79 MHz.

**So a dirty-cell scheme is the design, not an optimisation.** One caveat with
teeth: MAME does not model the V9958's VRAM write recovery, so every figure
here is a **lower bound**.

## What is real

**Storage.** `src/coco3storage.c` walks a Disk BASIC directory and FAT on
cmoc's `dskcon-standalone.h`, which drives the WD1773 directly and needs no
Disk BASIC ROM. 35 tracks × 18 sectors × 256 bytes; track 17 is the directory,
sector 2 the FAT; a granule is nine sectors and **track 17 is skipped in
granule numbering**, which is the detail that makes `gran_loc()` look wrong
until you know it.

**The font.** `tools/gen_font.py` — 61 text glyphs and 17 box glyphs in 665
bytes, authored as ASCII-art pictures with `--sheet` and `--box-sheet` proofs.
Ours, like every other port's box glyphs.

**The overlay machinery**, below.

## The video driver, and how it is checked

`src/coco3vid.c` draws the console into GRAPHIC6 and **11 of 11 checks pass on
the card** (`make vidcheck`). A cell is 6 pixels wide because at four bits a
pixel **six is three whole bytes** — no cell ever shares a byte with its
neighbour, so there is no odd-column special case.

**MAME RENDERS THIS CARD BLACK WHATEVER THE VDP IS DOING**, so the check draws
a known pattern and READS VRAM BACK through the card. **The instrument was
checked before the driver was**: write two values at two addresses and read the
first back — a latch returns the second, VRAM returns the first. It returns the
first. Without that, every assertion could have been confirming a write buffer.
The checks were then verified by breaking the thing they protect: with the
reverse-video rule deleted, that check goes red.

**Two bugs it found, and both were mine.**

**A register write goes to `$FF79`, not `$FF7B`.** `$FF7B` is register-*indirect*
access through R#17 — the stub's own comment said so and I read it as "register
write". Sending register writes there set a *sequence* of registers from R#17's
default of zero, which by luck produced something screen-shaped, so the display
looked plausible while R#14 (VRAM A16-A14) took garbage and reads landed in the
wrong 16K bank. The symptom was identical reads giving different answers.

**There must be no 32-bit arithmetic in this file.** The first version held VRAM
addresses in `unsigned long`, and `scr_clear` took over **twelve seconds** of
emulated time — the program was still inside it when the test gave up, which
reads exactly like a hang. Every address this driver touches (54,272 of display
and 2K of log at `$E000`) is under 65,536, so `unsigned int` covers all of it
and R#14 is just bits 14-15. Removing the `long` took **2,000 bytes** off the
test binary as well.

The message log now lives in the card's VRAM rather than in the 6809's address
space, which is what the stub always said should happen — **2,048 bytes bought
back** on a machine that had 2,468 free.

## What is a stub

`src/coco3snd.c` and `src/coco3input.c` say so in their first line. Two
consequences worth stating: the input stub never advances the entropy source,
**so a stubbed build plays the same game every time**; and
`plat_write_all` returns `STOR_ERROR`, so **SAVE reports "COULD NOT SAVE."
rather than pretending.**

**The message log's backing store never was one of those stubs.** `ui.c` calls
`vdc_set_address` / `vdc_data_write` / `vdc_data_read` every time it files a
message — on the C128 the scrollback lives in spare VDC video RAM — and
stubbing them is exactly how the Falcon shipped a broken message panel under a
comment I had invented. It is in the card's VRAM now, round-tripped by three of
the eleven checks, including one proving it survives a screen clear.

## Overlays: one link, a fixed window, and two checks that were blind

One link with the overlay source's `code` section **renamed**, so the linker
resolves calls in both directions; lwlink emits each section as its own DECB
block and `tools/build_ovl.py` cuts the images out. The overlay index is the
**shared `OVL_*` constant**, so `ovl_load(OVL_HOF)` finds this port's `HOF.OVL`
with `core/` knowing nothing about it.

**The first collision check compared the window against the DECB blocks, and
BSS is not in a DECB file.** The window sat at `$E100` inside a 4,633-byte bss
and the check said it was fine — it would have loaded an image straight over
the message log, the sector buffer, the FAT and the game state. **It reads the
link map now.**

**`tools/overlay_check.py` rewrote the overlay set on its first run.** Reading
the map against the generated assembly, it found eighteen undeclared call
sites — four of them `core/trek.c` calling `core/planet.c` **during play**,
not at a phase boundary. planet.c went back resident. *The call graph decides a
split, not its size.*

**With the set correct, the arithmetic refuses it:**

```
  window       $FA00, largest image 2540 bytes
  resident     54518 bytes, $1200..$E6F5
  free below $FF00: -1260
build_ovl: the image overruns the I/O page at $FF00 by 1260
```

That is the check working. **The next step is not mechanism, it is
candidates** — more phase-boundary code has to move before the window pays for
itself. `make` stays all-resident and green in the meantime: **60,097 bytes spanning
`$1200..$FCC0`, with 575 left below the I/O page** — the video driver cost
about 4,400 bytes of address space and handed 2,048 back by moving the message
log to VRAM. **575 bytes is the whole remaining budget for sound and input**,
which is why the overlay candidates below stopped being a tidy-up and became
the thing that decides whether this port finishes.

## The GIME MMU is proven and abandoned

8K blocks at `$FFA0-$FFA7`; a 128K machine has `$30-$3F` and the address space
uses `$38-$3F`, leaving eight spare — 64K of far memory. `bank_map` and friends
are **macros**, so there is no calling convention to get wrong and nothing to
page out mid-call.

**Setting MMUEN alone — no remapping, nothing else — permanently breaks
standalone DSKCON access.** Isolated by bisection to that one bit. Restoring
the registers does not recover it, restoring TR does not, re-initialising the
driver does not. Unexplained, and routed around by not using the MMU;
`src/coco3bank.c` is kept and is not linked into the game.

Two traps from that work worth keeping. **The GIME registers do not read back
what was written** here — `$FF90` and `$FF91` both read `$1B` — so ask the
machine before trusting a reference. And **the stack must not live in the
window**: moving `S` is not enough, because `U` is the frame pointer and a
local lives at `-2,U`. A local read back as `$FF` for hours and the `$FF` was
uninitialised RAM; **pre-painting the result area with `$00` is what separated
"wrong value" from "never written".**

## Building

cmoc 0.1.86, the only 6809 compiler, and the fifth toolchain on this project.
`PKGDATADIR` must be exported, and `src/` carries two shims cmoc does not
ship — `stdint.h` and `string.h`. The second matters more than it looks:
without it `#include <string.h>` resolves to the **host's**, and the build dies
fifty lines into Xcode's libc++ never once mentioning cmoc.

**cmoc rejects things every other compiler here accepts**, and the fixes went
into the shared sources rather than being worked around in this port: no
`continue` inside a `switch`, and a ternary cannot size an array. It also has
**no `volatile`** — three plain stores to one hardware address with no read
between them are dead-store-eliminated down to the last, which reads exactly
like the hardware failing. Drive memory-mapped registers through inline `asm`.

**It writes `<basename>.s` beside its output**, which is how four generated
`.s` files once got committed to the repository root. `--intdir=build`.

## Running

MAME, with Ample's romsets — `coco3` verifies good there, `coco_fdc` included,
plus the `ym2413` the card's OPLL needs.

```sh
mame64 coco3 -window -ext multi -ext:multi:slot1 ssfm -ext:multi:slot4 fdc
```

**Always `-window`.** And do not open the debugger: on a windowed run it
freezes MAME until the mouse moves. Drive it from an autoboot Lua script using
`emu.wait()` instead — `emu.add_machine_frame_notifier` stops firing after the
first callback that does real work, which looks exactly like the program
hanging. **Give Disk BASIC twelve seconds to boot**; three is not enough and a
short wait looks exactly like the disk hardware failing.

## What is open

See THE OPEN LIST in [`NOTES.md`](../NOTES.md) — items 27 through 31, and all
five of them are this port:

1. ~~Video~~ **BUILT and checked on the card** — sound and input are still stubs.
2. **`plat_write_all` is unimplemented**, so SAVE cannot work.
3. **The overlay set is too small to pay**, and needs candidates.
4. **MMUEN breaks DSKCON**, unexplained and parked.
5. **Nobody has played it** — which on this project is the item that finds what
   the instruments cannot.
