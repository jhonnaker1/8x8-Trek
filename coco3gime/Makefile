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
         -DTREK_COLOUR_IS_EGA -Isrc -I../coco3/src -I../core -I../c128/src

# src/gimevid.c FIRST, the same rule coco3/Makefile records: vdc_init() runs
# while the ROM is still mapped and has to be reachable below $8000.
GAME_SRC = src/gimevid.c ../c128/src/main.c ../c128/src/ui.c \
           ../c128/src/layout.c ../c128/src/strpool.c \
           ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
           ../coco3/src/coco3input.c ../coco3/src/coco3storage.c \
           src/gimeovl.c ../coco3/src/music_data.c \
           src/gimemem.c src/gimesnd.c src/gimelog.c
GAME_OUT = build/egatrek.bin

.PHONY: early contest clean

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

build/CONTEST.BIN: src/contest.c src/gimevid.c ../c128/src/layout.c Makefile
	@mkdir -p build
	$(CMOC) --coco --org=3000 -O2 -fomit-frame-pointer --intdir=build \
	    -Isrc -I../coco3/src -I../core -I../c128/src \
	    -o $@ src/contest.c src/gimevid.c ../c128/src/layout.c

build/contest.dsk: build/CONTEST.BIN
	@python3 ../coco3/tools/mkdisk.py $@ $<

clean:
	rm -f build/*.bin build/*.BIN build/*.dsk build/*.o build/*.s
