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
         -DTREK_COLOUR_IS_EGA -Isrc -I../core -I../c128/src

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

.PHONY: all game early data music clean

all: game

game: $(GAME_OUT)

$(GAME_OUT): $(GAME_SRC) Makefile
	@mkdir -p build
	$(CMOC) $(CFLAGS) -o $@ $(GAME_SRC)
	@ls -l $@

# THE FIRST MEASUREMENT AND THE ONE THAT DECIDES THIS PORT. Link the whole
# game against stubbed seams and read what is left below the I/O page at
# $FF00. Every 6502 port here opened exactly this way.
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
