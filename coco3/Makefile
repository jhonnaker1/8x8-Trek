# EGA Trek for the CoCo 3 with a SuperSprite FM+.
#
# Seventh port, and the one the scope said turns on a single question: DOES IT
# FIT. A 64K address space, cmoc with no LTO, and a game whose resident half is
# 37,612 bytes on the C128 BEFORE overlays. See NOTES.md, "SCOPE: the CoCo 3 +
# SuperSprite FM+", and take `make early` before writing a driver.
#
# cmoc 0.1.86. PKGDATADIR must be exported or cmoc cannot find its own runtime,
# and src/ carries two shims cmoc does not ship -- stdint.h and string.h. The
# second one matters more than it looks: without it `#include <string.h>`
# resolves to the HOST's, and the build dies fifty lines into Xcode's libc++
# complaining about __BYTE_ORDER__ and an unsupported architecture, never once
# mentioning cmoc.
# `overlays` is defined before `all` below, and make takes the FIRST rule as
# its default -- which silently made `make` mean `make overlays`, and the
# ports gate went red on a target that is not the build.
.DEFAULT_GOAL := all

CMOC       ?= $(HOME)/cmoc/bin/cmoc
PKGDATADIR ?= $(HOME)/cmoc/share/cmoc
export PKGDATADIR

# -fomit-frame-pointer is worth 566 bytes here, measured, and this is a target
# where that is real money. --org=1200 puts the load address below what Disk
# BASIC would leave free: this port owns the machine, the way the Atari one
# does since it dropped DOS.
#
# -DTREK_COLOUR_IS_EGA makes EGA_TO_VDC the identity: the V9958's palette gets
# EGA's own values in EGA's own order, so a colour name IS its index. Same
# trick as the MEGA65, the X16, the Amiga and the Falcon.
# --intdir=build because cmoc writes <basename>.s NEXT TO ITS OUTPUT and
# deletes it on success: a run from the repository root clobbers any file
# there with the same name, which is exactly how four generated .s files got
# committed to the root during scoping and deleted again by the next run.
CFLAGS = --coco --org=1200 -O2 -fomit-frame-pointer --intdir=build \
         -DTREK_COLOUR_IS_EGA -Isrc -Ibuild/img -I../core -I../c128/src

# No -DTREK_OVERLAYS yet, and that is deliberate: the staging rule is to link
# the whole game RESIDENT first and read the overflow. src/coco3ovl.c supplies
# the empty ovl_load core/overlay.h asks for.
GAME_SRC = ../c128/src/main.c ../c128/src/ui.c ../c128/src/layout.c \
           ../c128/src/strpool.c \
           ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
           src/coco3vid.c src/coco3input.c src/coco3storage.c \
           src/coco3farmem.c src/coco3snd.c src/coco3ovl.c \
           src/music_data.c
GAME_OUT = build/egatrek.bin

# AN OVERLAY IMAGE is linked SEPARATELY at the window address and loaded into
# a spare RAM block at startup -- cmoc has no per-function section placement,
# so this is the only shape available. cmoc's own linker step cannot be used
# (its C runtime wants a main() and program_start/program_end), so lwlink is
# driven directly with tools/ovl.link.
LWLINK ?= lwlink

# ---------------------------------------------------------------- overlays
#
# THE C128'S DESIGN, NOT THE GIME'S: a window in ordinary RAM with images read
# into it from disk. Paging an 8K block would make a swap one store instead of
# a seek, but enabling MMUEN breaks standalone DSKCON on this machine -- see
# NOTES.md -- so the port does not enable the MMU at all.
#
# ONE LINK, NOT SEPARATE ONES. tools/build_ovl.py renames an overlay source's
# `code` section and places it at the window, so the linker resolves calls IN
# BOTH DIRECTIONS and an overlay is ordinary C rather than a jump-table
# dialect. It cuts each overlay out as a .OVL image and generates ovlmap.h.
#
# THE WINDOW ADDRESS IS FIXED AND CHECKED, not derived -- the C128's sits at
# $AF00 for the same reason. build_ovl.py refuses a window that would land
# inside the resident image, which it did on the first attempt here.
OVL_WINDOW ?= E100

# WHOLE FILES, because cmoc has no per-function section placement. These three
# are the natural candidates: the hall of fame runs after the game ends, and
# the planet and save code are used at points the console is not drawing.
# INDEX:SOURCE, where the index is the SHARED OVL_* constant from
# core/overlay.h. OVL_HOF is 1 and OVL_FRONT is 2, and main() has called
# load_hof() and load_front() before those screens since the C128 -- so
# matching the numbering means rule 4 is already satisfied by machinery that
# is written down and reviewed.
#
# core/planet.c IS NOT HERE, and the checker is why: it is called from
# core/trek.c during play, not at a phase boundary, so it can never be paged
# out safely. tools/overlay_check.py found that by reading the call graph,
# which is the same way the C128 chooses its splits.
# core/serial.c is NOT here: it carries no OVL_CODE marker, so it is resident
# on the C128 and paging it whole was this port inventing a partition the
# shared design does not have -- which is exactly what overlay_check caught,
# as resident helpers in ui.c calling trek_state_load/save unguarded.
OVL_SRC = 1:../core/hof.c
RES_SRC = ../c128/src/main.c ../c128/src/ui.c ../c128/src/layout.c \
          ../c128/src/strpool.c ../core/trek.c ../core/planet.c ../core/serial.c \
          src/coco3vid.c src/coco3input.c src/coco3storage.c \
          src/coco3farmem.c src/coco3snd.c src/coco3ovl.c src/music_data.c

# NOT YET THE DEFAULT BUILD. The images build and the link resolves both ways,
# but nothing yet calls ovl_load before entering the window -- core/overlay.h's
# rule 4 is the remaining work, and until it is done this binary would jump
# into whatever the window happens to hold. `make game` stays all-resident.
overlays:
	@mkdir -p build/img build/int
	python3 tools/build_ovl.py --org 1200 --window $(OVL_WINDOW) \
	    --out build/egatrek-ovl.bin --intdir build/int --imgdir build/img \
	    --cflags "--coco -O2 -fomit-frame-pointer -Isrc -Ibuild/img -I../core -I../c128/src -DTREK_COLOUR_IS_EGA" \
	    --resident $(RES_SRC) --overlay $(OVL_SRC) --split
	python3 tools/overlay_check.py build/egatrek-ovl.bin.map build/int \
	    ../core/hof.c ../core/serial.c


# THE FONT IS THIS PORT'S OWN ARTWORK, and it has to be: the CoCo 3 is the one
# target with no glyphs to borrow -- the GIME's character generator is internal
# silicon, not a table the CPU can read. Authored as pictures in
# tools/gen_font.py, which also renders proof sheets:
#
#     python3 tools/gen_font.py --sheet text.png
#     python3 tools/gen_font.py --box-sheet box.png
#
# Regenerate rather than editing src/font6x8.h.
src/font6x8.h: tools/gen_font.py
	python3 tools/gen_font.py > $@
	@ls -l $@

font: src/font6x8.h

build/OVLDEMO.BIN: src/ovldemo.c tools/ovl.link
	@mkdir -p build
	$(CMOC) --coco -O2 -fomit-frame-pointer --intdir=build -c -o build/ovldemo.o src/ovldemo.c
	$(LWLINK) --format=raw --script=tools/ovl.link -o $@ build/ovldemo.o
	@ls -l $@

.PHONY: vidtest vidcheck all game early ovl overlays font data music clean

ovl: build/OVLDEMO.BIN

all: game

game: $(GAME_OUT)

# THE ALL-RESIDENT BUILD still compiles src/coco3ovl.c, which includes the
# GENERATED ovlmap.h -- so it needs one saying "no images", which is exactly
# what an all-resident build means: ovl_load finds nothing and returns.
build/img/ovlmap.h:
	@mkdir -p build/img
	@printf '/* GENERATED: the all-resident build has no overlay images. */\n#ifndef COCO3_OVLMAP_H\n#define COCO3_OVLMAP_H\n#define OVL_WINDOW 0x0000\n#define OVL_SIZE   0\n#define OVL_IMAGES 1\nstatic const char * const ovl_name[OVL_IMAGES] = { 0 };\n#endif\n' > $@

$(GAME_OUT): $(GAME_SRC) build/img/ovlmap.h Makefile
	@mkdir -p build
	$(CMOC) $(CFLAGS) -o $@ $(GAME_SRC)
	@ls -l $@

# THE FIRST MEASUREMENT AND THE ONE THAT DECIDES THIS PORT. Link the whole
# game against stubbed seams and read what is left below the I/O page at
# $FF00. Every 6502 port here opened exactly this way.
# THE VIDEO DRIVER, CHECKED ON THE CARD. MAME renders this card's screen black
# whatever the VDP is doing, so `vidtest` draws a known pattern, reads VRAM
# back THROUGH the card and leaves the answers in CPU RAM for a Lua dump.
# Looking is not a check here; reading back is.
VID_SRC = src/vidtest.c src/coco3vid.c
VID_OUT = build/VIDTEST.BIN

vidtest: $(VID_OUT)

# THE VIDEO GATE. Runs VIDTEST on the card under MAME and checks the bytes it
# read back out of VRAM. Skips itself with a note if MAME is not installed, so
# a fresh clone stays green -- the same rule tools/check_ports.py follows.
vidcheck: $(VID_OUT)
	@python3 tools/vidcheck.py

$(VID_OUT): $(VID_SRC) src/font6x8.h Makefile
	@mkdir -p build
	$(CMOC) $(CFLAGS) -o $@ $(VID_SRC)
	@ls -l $@

early: $(GAME_OUT)
	@echo ""
	@echo "  EARLY SIZE -- whole game resident, seams stubbed, no overlays"
	@python3 tools/binsize.py $(GAME_OUT)

# THE MUSIC IS THIS PORT'S OWN COMPOSITION, from tools/make_music.py -- never
# the note data tools/gen_music.py extracts from EGATREK.EXE, which is
# Anderson's creative work and can never ship.
music src/music_data.c src/music_data.h:
	cd .. && python3 tools/make_music.py && \
	    cp c128/src/music_data.c c128/src/music_data.h coco3/src/

clean:
	rm -f build/*.bin build/*.o
