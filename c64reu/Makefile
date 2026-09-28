# EGA Trek for the Commodore 64 with an REU -- the fifteenth port, and the
# C64's own game with its overlays in an REU instead of on the disk.
#
# THE SAME MACHINE AS ../c64, AND NEARLY THE SAME PROGRAM. What differs is
# where the code lives. The C64 port keeps 40,856 bytes resident and pages
# eleven overlays off the 1541, and the disk rules (core/overlay.h) forbid an
# overlay calling another because a swap costs seconds. From an REU a swap is
# a DMA, so here overlays MAY call each other: seven more groups -- view,
# panel, nav, time, turn, laser, torp -- take the engine and the console
# drawing out, twenty overlays in all, and the resident program is 21,823.
# Twice as fast as the C64 port once started.
#
# THAT IS ALSO WHY IT EXISTS as a port and not a build option: C64 OS gives an
# app 30,976 bytes, and this is the shape of the game that fits in them. See
# NOTES.md, "THE REU OVERLAY MANAGER".
#
# THREE FILES ARE THIS PORT'S OWN, and everything else is the C64's or the
# C128's: src/reuovl.s swaps images through a thunk on every call that
# crosses overlays; tools/reu_thunks.py patches those thunks into the LINKED
# ELF, from the relocations --emit-relocs keeps; and src/c64reu.c loads every
# image into the REU once. ../c64/src/c64mem.c and c64log.c are linked as
# they are.
.DEFAULT_GOAL := all

LLVM_MOS ?= $(HOME)/llvm-mos
CC       = $(LLVM_MOS)/bin/mos-c64-clang
OBJCOPY  = $(LLVM_MOS)/bin/llvm-objcopy
NM       = $(LLVM_MOS)/bin/llvm-nm

X64 ?= x64sc

# The C64's flags, the REU groups on, and this port's link script.
CFLAGS  = -Oz -I ../c64/src -I ../c128/src -I ../core -Wall -Werror \
          -DTREK_OVERLAYS -DTREK_40COL \
          -DTREK_OVL_ENEMY -DTREK_OVL_MOVE -DTREK_OVL_REU -T c64reu.ld
LDFLAGS = -Wl,--no-check-sections -Wl,--emit-relocs

# THE C64'S SOURCE LIST WITH ONE FILE SWAPPED: src/c64reu.c for the disk
# loader in ../c128/src/overlay.c.
SHARED = ../c128/src/main.c ../c128/src/vic.c ../c128/src/layout40.c \
         ../c128/src/ui.c ../c128/src/input.c ../c128/src/sid.c \
         ../c128/src/music_data.c ../c128/src/storage.c \
         ../c128/src/strpool.c \
         ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
         ../c64/src/c64mem.c ../c64/src/c64log.c
PLATFORM = src/c64reu.c src/reuovl.s
SRC = $(SHARED) $(PLATFORM)

OUT       = build/trekreu.prg
ELF       = build/trekreu.elf
D64       = build/egatrek-c64reu.d64
DEBUG_OUT = build/debug/trekreu.prg
DEBUG_ELF = build/debug/trekreu.elf
DEBUG_D64 = build/debug/egatrek-c64reu.d64

# THE ORDER IS THE IDS', core/overlay.h's -- but only the disk cares, because
# src/c64reu.c indexes its names by the id macros and tools/reu_thunks.py
# reads the ids out of the same header.
OVERLAYS = eval hof front info repair msgs planet cmds title events xtra \
           enemy move view panel nav time turn laser torp

OVL_WINDOW := $(shell sed -n 's/.*window (rwx).*LENGTH = \(0x[0-9A-Fa-f]*\).*/\1/p' c64reu.ld)

# $(1) = the directory. The link is PATCHED before the target exists, so no
# unpatched ELF ever sits under its name; the relocations were kept for the
# patch alone, and objcopy will not cut the .ovl_* sections while they pin
# them.
define LINK
	@mkdir -p $(dir $(2))
	$(CC) $(CFLAGS) $(1) $(LDFLAGS) -Wl,-Map=$(2:.elf=.map) -o $(2).tmp $(SRC)
	python3 tools/reu_thunks.py $(2).tmp
	$(OBJCOPY) -w -R '.rela*' $(2).tmp $(2)
	@rm -f $(2).tmp
endef

# $(1) = the ELF, $(2) = the directory. The C64's image rule, stamp and all:
# the low sixteen bits of ovl_load's address, which src/c64reu.c checks on
# every image before it trusts it for the whole game.
define OVL_IMAGES
	@stamp=`$(NM) $(1) | awk '$$3 == "ovl_load" { print $$1 }'`; \
	for o in $(OVERLAYS); do \
	    $(OBJCOPY) --dump-section=.ovl_$$o=$(2)/ovl_$$o.raw $(1) /dev/null || exit 1; \
	    python3 -c "import sys; \
	        n = sys.argv[1]; a = int(sys.argv[2], 16); \
	        d = open('$(2)/ovl_%s.raw' % n, 'rb').read(); \
	        w = int('$(OVL_WINDOW)', 16); \
	        d = d + bytes([a & 0xFF, (a >> 8) & 0xFF]); \
	        open('$(2)/ovl%s.prg' % n, 'wb').write(bytes([0x00,0xC0]) + d); \
	        sys.exit('overlay %s does not fit the window' % n) if len(d) > w else None" $$o $$stamp || exit 1; \
	done
endef

define MAKE_PRG
	$(OBJCOPY) -O binary --remove-section=.ovl_* $(1) $(2).res
	@python3 -c "d = open('$(2).res','rb').read(); \
	    open('$(2)','wb').write(bytes([0x01,0x08]) + d)"
endef

# $(1) = the .d64, $(2) = the PRG, $(3) = the ELF, $(4) = the directory,
# $(5) = its strings.dat. Formatted from empty every time: `c1541 -write` over
# an existing name keeps the OLD file -- see ../c64/Makefile.
define MAKE_D64
	$(call OVL_IMAGES,$(3),$(4))
	@rm -f $(1)
	c1541 -format "ega trek reu,01" d64 $(1) >/dev/null
	c1541 -attach $(1) -write $(2) trekreu >/dev/null
	@python3 -c "import sys; sys.stdout.buffer.write(b''.join(b'.'*25 + b'\r\n0\r\n' for _ in range(10)))" > build/trek.scr
	c1541 -attach $(1) -write build/trek.scr trek.scr,s >/dev/null
	c1541 -attach $(1) -write ../c128/src/briefing40.txt brief.txt,s >/dev/null
	@python3 -c "d = open('$(5)','rb').read(); \
	    open('$(5:.dat=.pdat)','wb').write(bytes([0x00,0xE0]) + d)"
	c1541 -attach $(1) -write $(5:.dat=.pdat) strings.dat >/dev/null
	@python3 -c "d = open('build/music.dat','rb').read(); \
	    open('build/music.pdat','wb').write(bytes([0x00,0xE0]) + d)"
	c1541 -attach $(1) -write build/music.pdat music.dat >/dev/null
	@for o in $(OVERLAYS); do \
	    c1541 -attach $(1) -write $(4)/ovl$$o.prg ovl$$o >/dev/null; \
	done
	@echo "disk: $(1)"
endef

.PHONY: all game debug d64 d64-debug release run verify reucheck clean

all: $(OUT)
game: $(OUT)

$(ELF): $(SRC) c64reu.ld tools/reu_thunks.py ../core/overlay.h Makefile
	$(call LINK,,$@)

$(OUT): $(ELF)
	$(call MAKE_PRG,$(ELF),$(OUT))
	@python3 ../c64/tools/report_size.py $(ELF) $(OUT)

debug: $(DEBUG_OUT)
$(DEBUG_ELF): $(SRC) c64reu.ld tools/reu_thunks.py ../core/overlay.h Makefile
	$(call LINK,-DTREK_DEBUG_INPUT,$@)
$(DEBUG_OUT): $(DEBUG_ELF)
	$(call MAKE_PRG,$(DEBUG_ELF),$(DEBUG_OUT))

# THIS PORT'S TEXT: the C64's overrides with the REU named.
build/strings.dat: ../c128/src/strings.txt src/strings.override.txt ../tools/gen_strings.py
	@mkdir -p build
	cd .. && python3 tools/gen_strings.py >/dev/null
	cd .. && python3 tools/gen_strings.py --override c64reu/src/strings.override.txt \
	    c64reu/build/strings.dat

# THE TEST DISK CARRIES THE C64'S TEXT, not this port's, and on purpose:
# `reucheck` compares every screen against the C64 port's, and the title is
# one of them. The code is what is under test; the four strings that name the
# machine are not.
build/debug/strings.dat: ../c128/src/strings.txt ../c64/src/strings.override.txt ../tools/gen_strings.py
	@mkdir -p build/debug
	cd .. && python3 tools/gen_strings.py >/dev/null
	cd .. && python3 tools/gen_strings.py --override c64/src/strings.override.txt \
	    c64reu/build/debug/strings.dat

# The C128's composition, compared on the way in -- see ../c64/Makefile for
# the wrong tune a stale copy once shipped.
build/music.dat: ../c128/build/music.dat ../tools/make_music.py
	@mkdir -p build
	cd .. && python3 tools/make_music.py >/dev/null
	cp ../c128/build/music.dat build/music.dat
	@cmp ../c128/build/music.dat build/music.dat

../c128/build/music.dat:
	cd .. && python3 tools/make_music.py >/dev/null

d64: $(D64)
$(D64): $(OUT) build/strings.dat build/music.dat ../c128/src/briefing40.txt verify Makefile
	$(call MAKE_D64,$(D64),$(OUT),$(ELF),build,build/strings.dat)

d64-debug: $(DEBUG_D64)
$(DEBUG_D64): $(DEBUG_OUT) build/debug/strings.dat build/music.dat ../c128/src/briefing40.txt Makefile
	$(call MAKE_D64,$(DEBUG_D64),$(DEBUG_OUT),$(DEBUG_ELF),build/debug,build/debug/strings.dat)

# THE RELEASE ARTEFACT IS THE .d64, with the README beside it, as on the C64.
release: $(D64)
	@cp README-release.txt build/egatrek-c64reu.txt
	@ls -l build/egatrek-c64reu.d64 build/egatrek-c64reu.txt

verify: $(OUT) build/strings.dat build/music.dat
	python3 ../c128/tools/layout40_check.py
	python3 tools/verify_c64reu.py

# A 128K 1700, the smallest REU made: twenty 4K images need 80K.
#
# +saveres, OR VICE WRITES THIS COMMAND LINE INTO ~/.config/vice/vicerc on
# exit. The first run of this target did exactly that: Jamie's C64 came back
# up with a 128K REU switched on where his own settings had a 16MB one off.
run: $(D64)
	$(X64) +saveres -reu -reusize 128 -autostart $(CURDIR)/$(D64):trekreu

# THE TEST THAT CAN SEE THE THUNKS WORK. One pinned game on the C64 port's
# disk and on this one, every screen compared in characters and colours.
# Sabotaged -- the DMA that puts the caller's image back removed -- it fails
# at the first console. Headless, in warp, about four minutes.
reucheck: $(DEBUG_D64)
	$(MAKE) -C ../c64 d64-debug
	python3 tools/reucheck.py ../c64/build/trek64-debug.d64 \
	    ../c64/build/trek64-debug.map build/reucheck-c64.json
	python3 tools/reucheck.py $(DEBUG_D64) build/debug/trekreu.map \
	    build/reucheck-reu.json --reu --prg=trekreu
	python3 tools/reucheck.py --compare build/reucheck-c64.json build/reucheck-reu.json

clean:
	rm -rf build
