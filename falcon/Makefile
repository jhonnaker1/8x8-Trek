# EGA Trek for the Atari Falcon030.
#
# Sixth port, and the SECOND WITH NO OVERLAY MACHINERY -- see src/falconovl.c.
# Like the Amiga it has far more memory than the game needs, so there is no
# window, no staging region, no build stamp, no ovl_load and none of the budget
# arithmetic that dominates the four 6502 ports. Unlike the Amiga it also gets
# a real filesystem for the storage seam and boots into its target video mode.
# See NOTES.md, "SCOPE: the ATARI FALCON".
#
# vbcc's Atari TOS target, which was already installed when this port started:
# config/tos plus targets/m68k-atari. `vc +tos` drives vbccm68k, vasmm68k_mot
# and vlink and produces a real .PRG. core/ compiled for it before this
# directory existed.
VBCC ?= $(HOME)/vbcc
export VBCC
# vc shells out to vbccm68k / vasmm68k_mot / vlink BY BARE NAME, so they have
# to be on PATH -- without this the build fails with "vbccm68k: command not
# found" from inside vc, which reads like a missing toolchain rather than a
# missing directory.
export PATH := $(VBCC)/bin:$(PATH)
CC = $(VBCC)/bin/vc

# -DTREK_COLOUR_IS_EGA makes EGA_TO_VDC the identity: the screen's palette
# registers are loaded with EGA's own values in EGA's own order, so a colour
# name IS its index. Same trick as the MEGA65, the X16 and the Amiga.
#
# No -Werror: vbcc's warning set is not gcc's, and the informational
# "statement has no effect" on core/'s (void) casts is not a defect. The root
# `make ports` gate checks the exit status, which is the thing that matters.
# -I TAKES NO SPACE. `vc -I src` silently drops the path and passes a bare
# -I to the compiler; the include then fails much later and blames the header.
CFLAGS = +tos -O2 -DTREK_COLOUR_IS_EGA -Isrc -I../c128/src -I../core

HATARI ?= hatari
TOSIMG ?= $(HOME)/hatari/rom/etos512us.img

# THE GAME. No -DTREK_OVERLAYS: core/overlay.h turns OVL_CODE into nothing and
# src/falconovl.c supplies the empty ovl_load it asks for, so every function
# the 6502 ports page in is simply resident here.
GAME_SRC = ../c128/src/main.c ../c128/src/ui.c ../c128/src/layout.c \
           ../c128/src/strpool.c \
           ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
           src/falconvid.c src/falconinput.c src/falconstorage.c \
           src/falconfarmem.c src/falconsnd.c src/falconovl.c \
           src/music_data.c
GAME_OUT = build/EGATREK.PRG

.PHONY: all game early data music clean run

all: game data

game: $(GAME_OUT)

$(GAME_OUT): $(GAME_SRC) Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(GAME_SRC)
	@ls -l $@

# THE FIRST MEASUREMENT, and the one the scope said to take before writing a
# driver: link the whole game against stubbed seams and read the size. On the
# 6502 ports this is `make early` and it answers "does it fit". Here it answers
# "how much of the machine does it want", which is a different and much easier
# question -- but it is still taken first, and taken the same way.
early: $(GAME_OUT)
	@echo ""
	@echo "  EARLY SIZE -- whole game, seams stubbed"
	@python3 tools/prgsize.py $(GAME_OUT)

data: build/STRINGS.DAT build/BRIEF.TXT build/MUSIC.DAT

# THE POOL'S DATA. gen_strings.py owns the numbering, so every port builds the
# same strdata.h and the same STR_COUNT; only the strings that name the machine
# differ, and those live in src/strings.override.txt.
build/STRINGS.DAT: ../c128/src/strings.txt src/strings.override.txt
	@mkdir -p build
	cd .. && python3 tools/gen_strings.py >/dev/null
	cd .. && python3 tools/gen_strings.py --override falcon/src/strings.override.txt \
	    falcon/build/STRINGS.DAT
	@ls -l $@

# The briefing, streamed through plat_open/plat_read a page at a time. Copied
# rather than generated: it is the port's own prose, committed, and shared with
# every other target.
build/BRIEF.TXT: ../c128/src/briefing.txt
	@mkdir -p build
	cp $< $@

# THE MUSIC IS THIS PORT'S OWN COMPOSITION, from tools/make_music.py -- never
# the note data tools/gen_music.py extracts from EGATREK.EXE, which is
# Anderson's creative work and can never ship. See that file's header.
music src/music_data.c src/music_data.h:
	cd .. && python3 tools/make_music.py && \
	    cp c128/src/music_data.c c128/src/music_data.h falcon/src/

build/MUSIC.DAT: src/music_data.c
	@mkdir -p build
	cp ../c128/build/music.dat $@

# Hatari, with the build directory mounted as GEMDOS drive C: -- so there is no
# floppy image to make and nothing to copy, exactly as Amiberry mounts a host
# directory for the Amiga. USE EmuTOS: Atari's TOS 4.04 double bus-errors on a
# Falcon here, and Hatari says so in as many words.
run: all
	$(HATARI) --machine falcon --tos $(TOSIMG) --monitor vga --vdi off \
	    --gemdos-drive C -d build --auto 'C:\EGATREK.PRG' --sound off

clean:
	rm -f build/*.o $(GAME_OUT)
