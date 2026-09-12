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
# src/coco3vid.c FIRST here too -- see the note above RES_SRC. vdc_init()
# takes the machine out of ROM mode and has to be reachable while ROM is still
# mapped, which means below $8000.
GAME_SRC = src/coco3vid.c ../c128/src/main.c ../c128/src/ui.c \
           ../c128/src/layout.c ../c128/src/strpool.c \
           ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
           src/coco3input.c src/coco3storage.c \
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
# src/coco3vid.c IS DELIBERATELY FIRST, and this is not cosmetic.
#
# vdc_init() takes the machine out of ROM/RAM mode ($FFDF, all RAM) and every
# other line of this port depends on that having happened: the CoCo 3 boots
# with ROM over $8000-$FEFF, and this image reaches $B82B resident with its
# overlay window above that. So the SWITCH ITSELF has to live below $8000, or
# main() calls into Color BASIC ROM trying to reach it and the 6809 never
# comes back. It did exactly that: _vdc_init landed at $A2D8 and the program
# wandered off into ROM at $A03F.
#
# Link order is what places it, so the order is load-bearing and tools/
# binsize.py asserts it rather than trusting this comment.
RES_SRC = src/coco3vid.c ../c128/src/main.c ../c128/src/ui.c \
          ../c128/src/layout.c ../c128/src/strpool.c \
          ../core/trek.c ../core/planet.c ../core/serial.c \
          src/coco3input.c src/coco3storage.c \
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

.PHONY: vidtest vidcheck ovltest ovlcheck data disk ovlrun all game early ovl overlays font data music clean

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

# THE OVERLAY SEAM ON ITS OWN: take the machine, read one image off the
# diskette into the window, report what landed. The full game reaches ovl_load
# through a title screen and a stubbed keyboard, so a failure there could be
# any of a dozen things.
OVLT_SRC = src/ovltest.c src/coco3ovl.c src/coco3storage.c
OVLT_OUT = build/OVLTEST.BIN

ovltest: $(OVLT_OUT)

$(OVLT_OUT): $(OVLT_SRC) build/img/ovlmap.h Makefile
	@mkdir -p build
	$(CMOC) $(CFLAGS) -o $@ $(OVLT_SRC)
	@ls -l $@

vidtest: $(VID_OUT)

# THE VIDEO GATE. Runs VIDTEST on the card under MAME and checks the bytes it
# read back out of VRAM. Skips itself with a note if MAME is not installed, so
# a fresh clone stays green -- the same rule tools/check_ports.py follows.
# THE DATA FILES the game reads at startup. gen_strings.py owns the numbering,
# so every port builds the same strdata.h and the same STR_COUNT; only the
# strings that name the machine differ, and those live in
# src/strings.override.txt.
#
# THE MUSIC IS THIS PORT'S OWN COMPOSITION, from tools/make_music.py -- never
# the note data tools/gen_music.py extracts from EGATREK.EXE, which is
# Anderson's creative work and can never ship.
data: build/STRINGS.DAT build/BRIEF.TXT build/MUSIC.DAT

build/STRINGS.DAT: ../c128/src/strings.txt src/strings.override.txt
	@mkdir -p build
	cd .. && python3 tools/gen_strings.py >/dev/null
	cd .. && python3 tools/gen_strings.py --override coco3/src/strings.override.txt \
	    coco3/build/STRINGS.DAT
	@ls -l $@

build/BRIEF.TXT: ../c128/src/briefing.txt
	@mkdir -p build
	cp $< $@

build/MUSIC.DAT: src/music_data.c
	@mkdir -p build
	cd .. && python3 tools/make_music.py >/dev/null
	cp ../c128/build/music.dat $@
	@ls -l $@

# THE DISKETTE, and the round-trip that proves it. mkdisk.py writes the Disk
# BASIC filesystem (ToolShed is not installed here); checkdisk.py reads it back
# with src/coco3storage.c's OWN algorithm and compares byte for byte, because a
# writer checked only by the thing that wrote it checks nothing. It caught the
# first version immediately.
disk: overlays data
	python3 tools/mkdisk.py build/trek.dsk build/img/*.OVL \
	    build/STRINGS.DAT build/MUSIC.DAT build/BRIEF.TXT
	python3 tools/checkdisk.py build/trek.dsk build/img/*.OVL \
	    build/STRINGS.DAT build/MUSIC.DAT build/BRIEF.TXT

# THE OVERLAY BUILD, ON THE MACHINE. Everything else about overlays here is
# arithmetic about files; this boots the resident image with the images on a
# real diskette and watches which ones get paged in.
# THE SEAM, ON THE MACHINE: two images loaded off a real diskette into the
# window and compared against the files. This is the one that passes.
ovlcheck: ovltest disk
	@python3 tools/ovlcheck.py

# THE WHOLE GAME, on the machine. DOES NOT PASS YET -- see "What is open".
ovlrun: disk
	@python3 tools/ovlrun.py

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
