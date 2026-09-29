# EGA Trek — C64 OS

The sixteenth port: **the Commodore 64 with an REU port ([`../c64reu`](../c64reu))
made into a C64 OS application.** Same C, same overlay manager, same game;
what is this directory's is where the machine is C64 OS's rather than the
C64's. It ships as `egatrek.car`, the archive C64 OS unpacks into its
Applications.

The player's instructions are in [`README-release.txt`](README-release.txt).

```sh
make                # build/bundle/ -- everything the app's folder holds
make verify         # the checks the link cannot make
make car            # build/egatrek.car
make release        # build/egatrek-c64os.zip: the .car and README.txt
make install        # onto a COPY of your C64 OS disk, every byte read back
make release-test   # the zip's .car on a fresh disk, for C64 OS to unpack
make run            # boot C64 OS in VICE; double-click "EGA Trek"
```

**C64 OS, its disk image and the ROMs are yours to supply** — `C64OS_ROMS`,
see `tools/rig.py`. Nothing of C64 OS is in this repository; its SDK headers
and docs, read out of a C64 OS volume by `tools/c64os_ref.py`, go under the
gitignored `reference/`.

## What is here

* **`src/app.s`** — the application header and C64 OS's KERNAL link table;
  the crossings between C64 OS and the game's C, which swap llvm-mos's
  thirty-two zero-page registers with C64 OS's; both key queues; the REU
  banks' `bkalloc_`/`bkfree_`; and quitting, which has to happen **inside**
  C64 OS's event loop.
* **`src/osvid.c`** — the screen, drawn into C64 OS's buffers at `$0400` and
  `$D800` and copied to the real screen under I/O. C64 OS's font has no
  box-drawing characters, so fifteen of its icon slots are lent the console's
  lines while the game runs and given back when it quits.
* **`src/osinput.c`**, **`src/osfile.c`**, **`src/osovl.c`** — keys; the app's
  folder, read from C64 OS's own record of where the app was launched from;
  the two REU banks, the string and music store and the message log in them;
  and c64reu's one-time overlay load.
* **`OVL_CODE_OS`** in the shared sources, empty on every other port: `main()`
  itself, the dialog helpers and the file code, moved into three overlays
  only this port has. Twenty-three in all.
* **`tools/mkcar.py`** writes and reads C64 OS's `.car` format, measured from
  three app archives on C64 OS 1.08's own volume, CRC32 trailer included.
* **`tools/rig.py`** — VICE with C64 OS: `install`, `watch` (traces REU
  transfers and C64 OS's exceptions, prints each key and the sound driver's
  state), `play` (nothing attached, to be heard) and `loudness`.

## Measured, 2026-09-28 (`make verify`)

    resident file        $0900..$5BFF, 0 spare below $5C00
    resident code, data  end at $4AFC, window at $4B00, 4 spare
    soft stack           256 bytes at $5B00
    largest overlay      ovlfront, 3,912 of 4,096
    overlays             23, in two 64K REU banks
    egatrek.car          30 files, 102,523 bytes

## What C64 OS actually requires

Every one of these was read wrong once, and each is written up in `NOTES.md`
under *"C64 OS PORT"*:

* **It loads an app over its own pages rather than refuse one.** The first
  file ran to `$79FF`, sat on the mouse driver and crashed C64 OS — so the
  file ends below `$5C00` and `verify` holds it there.
* **`bkalloc_` returns a bank counted from `appreubk`**, so 0 is success.
* **RUN/STOP and F1–F7 are command keys** (`readkcmd_`), not printable ones.
* **`quitapp_` only sets the event loop's break vector**, which the loop
  resets each pass — so the game asks from `layer_draw`, inside the loop.
* **Its font is the C64's lowercase screen-code layout**, not ASCII: `@` at 0,
  a backquote at 64.

## Status

Released in v0.23.0. **Played through in VICE** — C64 OS 1.08 on a CMD HD
image, a 16MB REU, eight fast app switching slots: title, play, save and
load, the evaluation, play again and quitting to the File Manager, with
music. Not yet on real hardware. The game takes the whole machine while it
plays; keeping C64 OS's menus alive mid-game would mean yielding to its event
loop between keys, which it does not do yet.
