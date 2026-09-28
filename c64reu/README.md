# EGA Trek — Commodore 64 with an REU

The fifteenth port: **the Commodore 64 port with its overlays in an REU**
instead of on the disk. Same machine as [`../c64`](../c64), same game, same
C. What differs is where the code lives. Because of that, this one plays
**twice as fast once started**.

It is also the shape of the game that fits in C64 OS, which gives an app
30,976 bytes. A C64 OS port would start from here. See `NOTES.md`, *"THE
TRIAL LINK"* and *"THE REU OVERLAY MANAGER"*.

The player's instructions are in [`README-release.txt`](README-release.txt).

```sh
make            # build/trekreu.prg
make d64        # build/egatrek-c64reu.d64 -- runs verify first
make verify     # the static gate `make ports` runs: memory map, ids, thunks
make reucheck   # play one game on this and the C64 port, compare every screen
make run        # x64sc with a 128K REU, +saveres
make release    # the .d64 and egatrek-c64reu.txt beside it
```

## What is this port's own

Almost nothing. `ui.c`, `main.c`, `vic.c`, `layout40.c`, `input.c`, `sid.c`,
`storage.c`, `strpool.c` and `core/` are the C128's, as on the C64. The C64's
`c64mem.c` (the string pool under the KERNAL) and `c64log.c` (the message log)
are linked from `../c64/src`. Four things are this port's:

* **`src/reuovl.s`, the overlay manager.** A thunk is `jsr ovl_far` followed
  by the overlay id and the function's address.
  * If that overlay is already in the window, `ovl_far` jumps straight there.
  * Otherwise it pushes the id that is loaded, DMAs the callee in, calls it,
    DMAs the pushed one back, and returns.
  * A, X and Y survive both ways, and it touches no zero page.
  * **It always puts back what was loaded, even for a resident caller.** A
    resident helper called from overlay A returns into A, so its call into B
    must leave A in the window afterwards.
* **`tools/reu_thunks.py`, which writes the thunks into the linked ELF.** It
  reads the relocations `-Wl,--emit-relocs` keeps, and points every JSR or
  JMP that crosses into another overlay at a thunk. Calls inside one overlay
  stay direct.
  * It works after the link because, under LTO, most callees are static
    functions nothing in the source can rename.
  * It works from relocations, not addresses, because every overlay starts
    at `$C000`, so an address names twenty functions at once.
  * It refuses to patch on a function pointer into an overlay, or on an
    operand that does not hold what its relocation predicts.
  * Switch jump tables are left alone. They sit in resident `.rodata` and
    point into their own function, which is the only code that reads them.
* **`src/c64reu.c`, the one-time load.**
  * The first `ovl_load` loads all twenty images from drive 8, checks each
    one's stamp, and stashes it in the REU at 4K per image. After that
    `ovl_load` does nothing.
  * The shared code's `load_X(); f();` pairs still compile. The thunk on
    `f` does the swap now.
  * It is the only machine-specific part. A C64 OS port would fill the REU
    through C64 OS's file API and keep the rest.
* **Seven overlay groups**: view, panel, nav, time, turn, laser and torp.
  They are marked `OVL_CODE_REU` in the shared sources, a macro that is empty
  on every other port. Twenty overlays in all, each inside the 4K window.

## Measured, 2026-09-26

    resident            21,823 of 46,847   (the C64 port: 40,856)
    largest overlay      3,802 of  4,096   .ovl_enemy
    thunks                  48 of     64   70 calls patched
    reu_xfer + ovl_far     206 bytes       thunks 288, tables 153
    the game, at 1x       24.6 s           the C64 port: 50.0 s
    before the title      47 s             all twenty images, JiffyDOS in VICE

## `make reucheck` is the test

It plays one pinned game on the C64 port's disk and on this one, and compares
every screen in characters and colours. The game covers the chart, two moves,
a laser kill, a torpedo, docking and the modal screens.

* The torpedo is a nested swap: `fire_one_torpedo` in OVL_REPAIR calls
  `trek_fire_torpedo` in OVL_TORP.
* The galaxy is pinned by writing `kb_entropy` and the password's RETURN while
  VICE is stopped. `input.c` returns an injected key before it counts, so both
  builds play the same game with no change to the code.
* The test disk carries the C64's four machine-naming strings, not this
  port's, so the title can be compared too.

All 35 screens match, in warp and at 1x. With the DMA that puts the caller
back replaced by NOPs, the very first console differs.

## Watch out for

**Every VICE command line needs `+saveres`.** The first run of `make run`
wrote `-reu -reusize 128` into Jamie's `vicerc` on exit.

**At 1x the C64 port sits silent for over three seconds while the 1541
loads,** so a harness that waits for three quiet seconds records too early.
`reucheck.py --realtime` waits eight.

**Tested in VICE only.** No real REU has run it yet.
