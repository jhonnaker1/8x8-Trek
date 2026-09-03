# EGA Trek — MEGA65 (native C65 mode)

The second port, and the first of the four **text-mode siblings** the target
order puts ahead of the bitmap machines. Status: **first light** — the console
frame draws from the shared layout, in 80 columns, on an exact EGA palette.

```sh
make          # build/smoke.prg
make run      # in Xemu
```

## Why native mode, not C64 mode

A `$0801` binary runs the MEGA65 in **C64 mode**, where 80 columns do not
exist — the C64 never had them, and setting VIC-IV's H640 bit from inside
changes nothing. Colour RAM makes it concrete: 80×25 is 2000 cells and a
C64-mode program reaches only the 1K window at `$D800`.

llvm-mos's `mega65` target links at **`$2001` with a BASIC 65 header**, which
is what actually selects C65/native mode. There H640 is real and mega65-libc
reaches the full colour RAM at `$FF80000` through the 45GS02's 32-bit
addressing, so every one of the 2000 cells gets its own colour.

## The EGA palette, exactly — including brown

The first thing this machine buys over the C128. The VDC is a fixed RGBI chip:
the C128 port maps EGA onto it with a 4-bit rotate, fifteen colours land
exactly, and **EGA's brown comes out olive** — recorded in `core/ega.h` as the
one colour that port cannot have.

The VIC-IV's palette is programmable, so `m65vid.c` writes EGA's own values
into entries 0..15 and the mapping becomes the identity.

Measured off a screenshot rather than assumed: all sixteen land on their EGA
index, brown at `(170, 85, 0)` **exact**. Eight read one LSB low (84 for 85,
254 for 255), which is Xemu's rendering — a wrong palette byte would be off by
a whole nibble step, not by one.

A happy accident makes the palette bytes revision-proof: EGA's two-bits-per-gun
levels are `0x00`, `0x55`, `0xAA`, `0xFF`, every one a duplicated nibble. They
are the same colour whether the core reads the register as eight bits or as the
low nibble of a VIC-III-compatible four.

## What is shared, and what this port has to supply

`core/` and the console layout compile unchanged — the smoke build already
links `../c128/src/layout.c` and uses `layout.h`'s glyph constants as they
stand, because the MEGA65 uses the same C64-family screen codes the C128's VDC
does.

`ui.c` and `main.c` — 4,500 lines of console and command handling — touch the
platform through a seam that is already abstracted at core level:

| seam | calls | MEGA65 |
|---|---|---|
| `scr_*` | 224 | **done** — `m65vid.c` |
| `ovl_load` | 25 | a **no-op**: 40MHz and far more RAM, so nothing needs overlaying |
| `kb_*` | 17 | to write |
| `snd_*` | 15 | the MEGA65 has real SIDs, so `sid.c` should port nearly as-is |
| `vdc_data_*` | 9 | the one genuinely C128-shaped thing: the message log lives in spare **VDC RAM**. Here it becomes plain RAM |
| `plat_*` | 5 | SD-card file I/O via mega65-libc |
| `far_*` | — | plain RAM; the bank-1 seam exists because the C128 has 42K, and this machine does not |

## Overlays are still needed here, and the first version of this file was wrong

That paragraph used to say the three big C128 subsystems — ten 4K code
overlays, bank-1 far memory, byte-at-a-time disk streaming — were "none of them
needed here". **Measured, and it is not true of the first one.**

llvm-mos's mega65 region is `$2001..$CFFF`: **45,055 bytes**. The whole game is
about **66,500 bytes** of code. Unmapping the KERNAL as well would buy 8K and
still leave it short. So the window stays, and `core/overlay.h`'s `OVL_CODE`
had to stop testing `__mos__` — the MEGA65 is built by llvm-mos too, and that
test was asking the wrong question. It is `TREK_OVERLAYS` now, and each
platform's Makefile says whether it wants windows.

**What does change is the cost, and it changes completely.** The C128 reads
each 4K image off a 1541 — hundreds of milliseconds, which is why that port
works so hard to call `ovl_load` rarely and why `check_overlay_calls` exists.
Here the ten images sit in banked RAM at `$50000` and come in by **DMAgic**:
one `lcopy`, microseconds. Same mechanism, and the cost that shaped the C128
port's structure is gone.

The other two really did evaporate. The string pool and music live at `$40000`
and the message log at `$44000`, reached with `lcopy`/`lpeek`/`lpoke` instead
of an MMU dance, and `far_read` is a DMA burst.

    resident   33,566 bytes of 40,959    ($2001..$BFFF)
    window      4,096 bytes              ($C000..$CFFF)
    images     40,960 bytes              banked at $50000, from OVERLAYS.BIN

## Running it, and why that is still awkward

Xemu's `-virtsd` presents a plain directory as the card, which is the only
provisioning that has worked here. But the MEGA65 ROM runs its **ONBOARDing
utility** on a card without Xemu's signature, and that needs a human to press
RETURN and approve an FPGA-reconfiguration dialog -- which then RESETS the
machine and runs onboarding again. So `-virtsd` cannot be driven headlessly,
and `-screenshot`/`-dumpscreen` (which flush on SIGTERM and work fine for the
smoke build) capture the onboarding screen instead of the game.

Three attempts at a persistent image the Hypervisor would accept all failed: a
bare FAT32 with no partition table; an MBR plus FAT32, which it could not
`CHDIR /` into; and a blank card offered to its own FDISK+FORMAT utility, which
cannot work because that utility is a file **on** the card.

**This is the thing to solve before any more MEGA65 work.** Until it is,
verification means asking a human what is on the screen, which is slow and --
as below -- unreliable in a way that wasted a whole debugging session.

## What is verified, and what is not

**Verified:** the toolchain, the video layer and the palette — the smoke build
draws the console frame and a screenshot measures all sixteen colours onto
their EGA index. The full game **links** at the sizes above with `-Wall
-Werror`. The shared `ui.c`, `main.c`, `strpool.c` and `layout.c` compile for
this target unchanged, which is the real portability result.

**NOT verified: the game running.** Xemu will not mount a fresh SD image until
it has written its own system files onto it, and that is a GUI action —
`Disks -> SD-card -> Update files` — with no command line behind it. So startup,
file loading, the DMA overlay path, input and sound have not been seen working.

`make sd-setup` opens Xemu on the built image for that one-time step; after it,
`make run` should work. Until someone does it, treat this port as **compiled
and unproven**.

## It found a bug in the C128 port

Compiling the shared code for a second target immediately flagged
`-Wreturn-type` on `do_use()`, which had gained a `uint8_t` return the day
before and still fell off the end of its energium path. The caller tests
`do_use() == USE_WANT_BOLT`, so a garbage return could have fired the plasma
bolt dialog after mining a crystal. The C128 build had been printing that
warning too; the grep used to check builds filtered warnings out. Both ports
now build with `-Werror`.

## Verifying it

Xemu flushes `-screenshot` **and** `-dumpscreen` on **SIGTERM**, which is what
makes a program with no exit path checkable — `-prgexit` never fires, because
llvm-mos binaries do not reliably return to BASIC.

`-dumpscreen` writes the character matrix as text, so a layout can be asserted
by reading it rather than by looking at a picture.


## A debugging session spent on a phantom

Worth recording because the lesson is not about the MEGA65.

The game reached its **title screen** once, drawn from the shared `ui.c` with
the overlay DMA path working. The only fault was silence. The music base fix
went in, and the next three runs came back "purple screen" -- so the fix looked
like a regression, and two rounds of analysis went into finding what the sound
driver could possibly have broken. A linker-script collision between the
overlay window and the soft stack was found and fixed on the strength of it.

**The purple screen was the ONBOARDING SCREEN**, dimmed behind one of Xemu's
own modal dialogs. It was never the game. The runs had never got as far as our
code.

Two things went wrong, and only one of them was the emulator's fault:

  * **A screenshot would have settled it in seconds** and I had no way to take
    one, so I reasoned from a three-word description instead. "Screenshot
    before theorising" is written down in this project already.
  * **I asked for a colour, not for a picture.** "Still purple" is consistent
    with a dozen states; the actual screen was unambiguous the moment it was
    seen.

The window/stack fix stands on its own -- `__stack` at $D000 really did grow
down through a window at $C000, and `far_read` really does DMA into a stack
local -- but it was found by accident while chasing something that was not
happening.