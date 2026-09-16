# EGA Trek for a CoCo 3 with NO SuperSprite FM+ -- the GIME's own 80x25 text.
#
# THE MACHINE JAMIE ACTUALLY HAS. coco3/ needs a SuperSprite for its video,
# its sound and its far memory, and he has a CoCo 3 with an SDC and no card --
# which is why item 55 has never been answerable. This port needs nothing but
# the machine and a disk controller.
#
# AND IT IS AN 80-COLUMN PORT, measured rather than assumed: the GIME does
# 80x25 text with eight per-cell foreground colours on a bare machine, so this
# links layout.c and the console Anderson drew. See NOTES.md, "THE CARD-LESS
# COCO 3 IS AN 80-COLUMN PORT".
.DEFAULT_GOAL := early

# EIGHTY OR FORTY COLUMNS, BOTH BUILDABLE, because the budget question is not
# settled and a switch nobody can reverse is not a measurement. `make
# WIDTH=40` selects the 40-column layout and a 2,000-byte screen.
#
# MEASURED 2026-09-16, and the result is counter-intuitive: forty columns
# saves 2,000 bytes of screen and SPENDS 779 of them back, because layout40.c
# and ui.c's TREK_40COL path are bigger than the 80-column ones. Net 1,186.
#
#     80 columns   over by 1,608
#     40 columns   over by   422
WIDTH ?= 80
ifeq ($(WIDTH),40)
WIDTH_FLAG = -DTREK_40COL
LAYOUT     = ../c128/src/layout40.c
else
WIDTH_FLAG =
LAYOUT     = ../c128/src/layout.c
endif

CMOC      ?= $(HOME)/cmoc/bin/cmoc
# PKGDATADIR must be exported or cmoc cannot find its own runtime and the link
# fails with "Can't open library: -lcmoc-crt-ecb", which reads like a missing
# toolchain rather than a missing variable.
PKGDATADIR ?= $(HOME)/cmoc/share/cmoc
export PKGDATADIR

# -I../coco3/src for that port's stdint.h and string.h shims. WITHOUT THEM
# cmoc hands the host cc a <stdint.h> include and on an arm64 Mac that fails
# with "architecture not supported" -- a host error for a 6809 compile.
CFLAGS = --coco --org=2800 -O2 -fomit-frame-pointer --intdir=build \
         -DTREK_COLOUR_IS_EGA -DSTOR_TR_AT=0x1FB0 $(WIDTH_FLAG) -Isrc -I../coco3/src -I../core -I../c128/src

# src/gimevid.c FIRST, the same rule coco3/Makefile records: vdc_init() runs
# while the ROM is still mapped and has to be reachable below $8000.
GAME_SRC = src/gimevid.c ../c128/src/main.c ../c128/src/ui.c \
           $(LAYOUT) ../c128/src/strpool.c \
           ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
           ../coco3/src/coco3input.c ../coco3/src/coco3storage.c \
           src/gimeovl.c ../coco3/src/music_data.c \
           src/gimemem.c src/gimesnd.c src/gimelog.c
GAME_OUT = build/egatrek.bin

.PHONY: early overlays contest clean

# THE FIRST MEASUREMENT, AND THE ONE THIS PORT TURNS ON. Link the whole game
# resident, with the real string-pool array in it, and read the overflow. The
# question is whether 64K holds a 45K program, a 4,000-byte screen at $E000
# and an 8K pool -- and no amount of driver writing changes the answer.
early: $(GAME_OUT)
	@python3 tools/early.py $(GAME_OUT)

$(GAME_OUT): $(GAME_SRC) Makefile
	@mkdir -p build
	$(CMOC) $(CFLAGS) -o $@ $(GAME_SRC)
	@ls -l $@

# The console frame through the real layout.c, for looking at.
contest: build/contest.dsk

build/CONTEST.BIN: src/contest.c src/gimevid.c $(LAYOUT) Makefile
	@mkdir -p build
	$(CMOC) --coco --org=3000 -O2 -fomit-frame-pointer --intdir=build \
	    -Isrc -I../coco3/src -I../core -I../c128/src \
	    -o $@ src/contest.c src/gimevid.c $(LAYOUT)

build/contest.dsk: build/CONTEST.BIN
	@python3 ../coco3/tools/mkdisk.py $@ $<

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
	cd .. && python3 tools/gen_strings.py --override coco3gime/src/strings.override.txt \
	    coco3gime/build/STRINGS.DAT
	@ls -l $@

build/BRIEF.TXT: ../c128/src/briefing.txt
	@mkdir -p build
	cp $< $@

build/MUSIC.DAT: ../c128/src/music_data.c
	@mkdir -p build
	cd .. && python3 tools/make_music.py >/dev/null
	cp ../c128/build/music.dat $@
	@ls -l $@

# THE FIRST-STAGE LOADER, AND IT IS THE CARD PORT'S, UNCHANGED. Disk BASIC
# cannot place a 45K image spanning $2800..$D8EC -- that is under the BASIC and
# Disk BASIC ROMs and CLEAR cannot move the ceiling there -- so BASIC loads a
# small loader and the loader reads the image. See NOTES.md item 35.
#
# --org=E400 IS THE CARD PORT'S ADDRESS AND IT SUITS THIS MAP TOO, which is
# luck worth checking rather than assuming: the body is 4,665 bytes, so it
# occupies $E400..$F658 -- inside this port's BSS ($D8FC..$E5BA), its overlay
# window ($E800) and the bottom of its message log ($F200). Every one of those
# is DEAD AT LOAD TIME and none is read before the loader has jumped. What
# matters is that it clears the image itself, which ends at $D8EC.
BOOT_SRC = ../coco3/src/coco3boot.c ../coco3/src/coco3storage.c
BOOT_OUT = build/BOOT.BIN

boot: $(BOOT_OUT)

$(BOOT_OUT): $(BOOT_SRC) Makefile
	@mkdir -p build
	$(CMOC) --coco --org=E400 -O2 -fomit-frame-pointer --intdir=build \
	    -DBOOTR_AT=0x1FA0 -Isrc -I../coco3/src -I../core -o $@ $(BOOT_SRC)
	@ls -l $@

# THE DISKETTE, and the round-trip that proves it. mkdisk.py writes the Disk
# BASIC filesystem (ToolShed is not installed here); checkdisk.py reads it back
# with coco3storage.c's OWN algorithm and compares byte for byte, because a
# writer checked only by the thing that wrote it checks nothing.
disk: overlays data boot
	python3 ../coco3/tools/mkboot.py build/BOOT.BIN build/egatrek-ovl.bin build/
	python3 ../coco3/tools/mkdisk.py build/trek.dsk build/TREKLDR.BIN build/EGATREK.RAW \
	    build/img/*.OVL build/STRINGS.DAT build/MUSIC.DAT build/BRIEF.TXT
	python3 ../coco3/tools/checkdisk.py build/trek.dsk build/TREKLDR.BIN build/EGATREK.RAW \
	    build/img/*.OVL build/STRINGS.DAT build/MUSIC.DAT build/BRIEF.TXT

clean:
	rm -f build/*.bin build/*.BIN build/*.dsk build/*.o build/*.s

# ---------------------------------------------------------------- overlays
#
# THE CARD PORT'S MACHINERY, UNCHANGED: coco3/tools/build_ovl.py renames an
# overlay source's `code` section and places it at the window, so ONE link
# resolves calls in both directions and an overlay is ordinary C rather than a
# jump-table dialect. Images are cut out as .OVL files and read from disk.
#
# THE WINDOW IS PLACED BY build_ovl, NOT BY THIS NUMBER, and that is the right
# way round: it links once, reads the top of bss out of the map and pins the
# window to the first page above it, so the window follows the image instead of
# having to be edited every time the image grows. This value is only the
# starting guess. (I set it to $E800 by hand to leave the image room and the
# tool moved it back to $E600 -- correctly, and then said so in a sentence that
# had the direction backwards. Both are fixed.)
#
# THE CEILING IS THIS PORT'S OWN. With the screen down at $1000 the only things
# above the window are the 2K message log at $F200 and the 1K stack under
# $FE00. OVL_SIZE is the bound handed to plat_read_all, so left at its default
# -- "everything up to the stack" -- an overlay that outgrew the gap would be
# read straight through the log with no symptom but corrupted messages.
OVL_WINDOW  ?= E600
OVL_CEILING ?= FA00   # the stack; the log is in low RAM now

# Same split as the card port's, for the same reason its Makefile gives: whole
# files, because cmoc has no per-function section placement, and the index is
# the SHARED OVL_* constant from core/overlay.h so that main()'s existing
# load_hof()/load_front() calls already satisfy rule 4.
OVL_SRC = 1:../core/hof.c

# gimevid.c FIRST, and it is not cosmetic -- the card port lost a day to this.
# vdc_init() takes the machine out of ROM/RAM mode and every other line
# depends on that having happened, so the switch itself must live below $8000
# or main() calls into Color BASIC ROM trying to reach it.
RES_SRC = src/gimevid.c ../c128/src/main.c ../c128/src/ui.c \
          $(LAYOUT) ../c128/src/strpool.c \
          ../core/trek.c ../core/planet.c ../core/serial.c \
          ../coco3/src/coco3input.c ../coco3/src/coco3storage.c \
          ../coco3/src/music_data.c \
          src/gimemem.c src/gimesnd.c src/gimelog.c ../coco3/src/coco3ovl.c

overlays:
	@mkdir -p build/img build/int
	python3 ../coco3/tools/build_ovl.py --org 2800 --window $(OVL_WINDOW) \
	    --ceiling $(OVL_CEILING) \
	    --out build/egatrek-ovl.bin --intdir build/int --imgdir build/img \
	    --cflags "--coco -O2 -fomit-frame-pointer -Isrc -Ibuild/img -I../coco3/src -I../core -I../c128/src -DTREK_COLOUR_IS_EGA -DSTOR_TR_AT=0x1FB0 $(WIDTH_FLAG)" \
	    --resident $(RES_SRC) --overlay $(OVL_SRC) --split
	@python3 tools/early.py build/egatrek-ovl.bin --window 0x$(OVL_WINDOW)
