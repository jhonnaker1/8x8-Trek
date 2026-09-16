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
         -DTREK_COLOUR_IS_EGA $(WIDTH_FLAG) -Isrc -I../coco3/src -I../core -I../c128/src

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

clean:
	rm -f build/*.bin build/*.BIN build/*.dsk build/*.o build/*.s

# ---------------------------------------------------------------- overlays
#
# THE CARD PORT'S MACHINERY, UNCHANGED: coco3/tools/build_ovl.py renames an
# overlay source's `code` section and places it at the window, so ONE link
# resolves calls in both directions and an overlay is ordinary C rather than a
# jump-table dialect. Images are cut out as .OVL files and read from disk.
#
# THE WINDOW GOES AS HIGH AS IT CAN, WHICH IS A CHANGE. It used to be packed
# hard under a screen that lived at $E260 -- three tenants stacked below the
# I/O page with the image ending beneath all of them. THE SCREEN IS AT $1000
# NOW, below the load address entirely (src/gimevid.c says why), so the only
# things left up here are the message log at $F200 and the 1K stack under
# $FE00. That leaves $D8EC..$F1FF, and the split is:
#
#     window   $E800..$F1FF   2,560 for the largest overlay image (2,500 today)
#     free     image end..$E5FF     for the resident half to grow into
#
# Putting the window at the TOP rather than the bottom of that gap is the
# whole point: a window pinned just above the image leaves the image no room
# at all, and this one grew 1,314 bytes the day the sound driver landed.
OVL_WINDOW ?= E800

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
          src/gimemem.c src/gimesnd.c src/gimelog.c src/gimeovl.c

overlays:
	@mkdir -p build/img build/int
	python3 ../coco3/tools/build_ovl.py --org 2800 --window $(OVL_WINDOW) \
	    --out build/egatrek-ovl.bin --intdir build/int --imgdir build/img \
	    --cflags "--coco -O2 -fomit-frame-pointer -Isrc -Ibuild/img -I../coco3/src -I../core -I../c128/src -DTREK_COLOUR_IS_EGA $(WIDTH_FLAG)" \
	    --resident $(RES_SRC) --overlay $(OVL_SRC) --split
	@python3 tools/early.py build/egatrek-ovl.bin --window 0x$(OVL_WINDOW)
