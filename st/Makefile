# EGA Trek for the Atari ST and STE.
#
# THE NINTH PORT, AND IT IS ONE FILE. The Falcon port already supplies every
# seam this machine needs -- `vc +tos` IS the ST target, the sound is the same
# YM2149 through the same XBIOS Giaccess at the same measured 2 MHz, storage is
# GEMDOS, far memory is a plain array and there are no overlays. What the ST
# does not share is its video chip, so src/stvid.c is the whole port. See
# ../README.md, "What a sub-80 port actually entails".
#
# THIS LINE WAS RULED OUT OF THE PROJECT ON 2026-08-22 and the reason was
# WIDTH: "320x200 in 16 colours (only 40 columns)". The console is forty
# columns now. 320/8 = 40 and 200/8 = 25, so the mode that was never wide
# enough is exactly the right size, and the rule that excluded it no longer
# exists. Nothing about the machine changed.
.DEFAULT_GOAL := all

VBCC ?= $(HOME)/vbcc
export VBCC
# vc shells out to vbccm68k / vasmm68k_mot / vlink BY BARE NAME, so they have
# to be on PATH -- without this the build fails with "vbccm68k: command not
# found" from inside vc, which reads like a missing toolchain rather than a
# missing directory. The Falcon port lost time to this; it is not rediscovered
# here, it is copied.
export PATH := $(VBCC)/bin:$(PATH)
CC = $(VBCC)/bin/vc

# -DTREK_40COL is what makes this a forty-column build: it switches the
# message and viewer geometry in layout.h and the one paging site in ui.c, and
# it selects layout40.c below. -DTREK_COLOUR_IS_EGA makes EGA_TO_VDC the
# identity, because stvid.c loads the palette with EGA's own values in EGA's
# own order.
#
# No -Werror and -dontwarn=153 for the reasons falcon/Makefile gives at
# length: vbcc's warning set is not gcc's, and 153 is "statement has no
# effect", which it emits for every `(void)unused;` in core/.
#
# -I TAKES NO SPACE. `vc -I src` silently drops the path.
CFLAGS = +tos -O2 -dontwarn=153 -DTREK_40COL -DTREK_COLOUR_IS_EGA \
         -Isrc -I../c128/src -I../core

HATARI ?= hatari
# EMUTOS, and that is Jamie's call as well as the Falcon port's default. It is
# free software, so the README can tell a player where to get the exact ROM
# this was developed against; a real TOS 2.06 image is Atari's and is the
# player's to supply. `make run-tos206` uses one if you have it.
TOSIMG ?= $(HOME)/hatari/rom/etos512us.img

# THE SHARED HALF, and it is nearly all of it. Two files are this port's own;
# five more come from the Falcon, unchanged and uncopied -- referenced across
# the directory boundary exactly as the C64 references the C128's.
SHARED = ../c128/src/main.c ../c128/src/ui.c ../c128/src/layout40.c \
         ../c128/src/strpool.c \
         ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c
FROM_FALCON = ../falcon/src/falconinput.c ../falcon/src/falconstorage.c \
              ../falcon/src/falconfarmem.c ../falcon/src/falconsnd.c \
              ../falcon/src/falconovl.c ../falcon/src/music_data.c
PLATFORM = src/stvid.c src/stlog.c
GAME_SRC = $(SHARED) $(FROM_FALCON) $(PLATFORM)
GAME_OUT = build/EGATREK.PRG

.PHONY: all game verify data music release run run-ste run-tos206 sndtest hearit clean

all: game data

game: $(GAME_OUT)

$(GAME_OUT): $(GAME_SRC) Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(GAME_SRC)
	@ls -l $@

# THE GATE. The root `make ports` runs this and checks its exit status.
# This port has none of the pools the 6502 ports run out of, so like the
# Falcon it checks what fails SILENTLY here instead: that the console fits the
# screen, and that every glyph the shared UI can put on screen has something
# to draw.
verify: $(GAME_OUT)
	python3 ../c128/tools/layout40_check.py
	python3 tools/verify_st.py

# THE SOUND PROBE. This port shipped SILENT because every automated check ran
# with `--sound off` -- a driver that writes no registers is indistinguishable
# from a working one in a screenshot. It reports each link of the chain and
# READS THE PSG BACK, which the YM2149 allows and the SID does not.
SND_SRC = src/sndtest.c ../falcon/src/falconsnd.c ../falcon/src/falconfarmem.c \
          ../falcon/src/falconstorage.c ../falcon/src/music_data.c
# AND THE ONE THAT ANSWERS THE REAL QUESTION: does a speaker get anything?
# sndtest proves the driver writes the right values; hearit records Hatari to
# an AVI, extracts the audio and measures the peak. No ear required, and no
# `--sound off` anywhere near it.
hearit: all
	@python3 tools/hearit.py

sndtest: build/SNDTEST.PRG
build/SNDTEST.PRG: $(SND_SRC) Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SND_SRC)
	@ls -l $@

data: build/STRINGS.DAT build/BRIEF.TXT build/MUSIC.DAT

# gen_strings.py owns the numbering, so every port builds the same strdata.h
# and the same STR_COUNT; only the strings that name the machine differ.
build/STRINGS.DAT: ../c128/src/strings.txt src/strings.override.txt
	@mkdir -p build
	cd .. && python3 tools/gen_strings.py >/dev/null
	cd .. && python3 tools/gen_strings.py --override st/src/strings.override.txt \
	    st/build/STRINGS.DAT
	@ls -l $@

# THE FORTY-COLUMN BRIEFING, not the eighty-column one. briefing40.txt is
# generated from briefing40-console.txt by tools/reflow_briefing.py -- 24
# pages, every line inside 38 columns. Handing this port the Falcon's
# briefing.txt would put 78-column prose on a 40-column screen, which the
# streamer would not complain about at all.
build/BRIEF.TXT: ../c128/src/briefing40.txt
	@mkdir -p build
	cp $< $@

# THE MUSIC IS THE PROJECT'S OWN COMPOSITION, from tools/make_music.py, never
# the note data gen_music.py extracts from Anderson's binary. Named as a
# prerequisite and compared, because a stale copy of this file shipped a wrong
# tune on the C64 for the whole of its first day.
music:
	cd .. && python3 tools/make_music.py >/dev/null

build/MUSIC.DAT: ../c128/build/music.dat
	@mkdir -p build
	cp $< $@
	@cmp $< $@

../c128/build/music.dat:
	cd .. && python3 tools/make_music.py >/dev/null

# THE RELEASE ARTEFACT IS A ZIP OF A FOLDER, like the Falcon, the Amiga and the
# X16: GEMDOS opens the data files by BARE NAME, so they sit beside the .PRG in
# whatever directory it runs from. No .ST floppy image -- it would have to
# carry a bootable filesystem and cap the port at 720K for no gain, and Hatari
# mounts a host directory.
REL = build/release

release: game data verify
	@rm -rf $(REL) && mkdir -p $(REL)/egatrek-st
	@cp $(GAME_OUT) build/STRINGS.DAT build/MUSIC.DAT build/BRIEF.TXT \
	    $(REL)/egatrek-st/
	@cp README-release.txt $(REL)/egatrek-st/README.txt
	@cd $(REL) && zip -q -r ../egatrek-st.zip egatrek-st
	@echo "release: build/egatrek-st.zip"
	@unzip -l build/egatrek-st.zip | tail -n +4

# Hatari with build/ mounted as GEMDOS drive C:, so there is no floppy image to
# make and nothing to copy. `--machine st` is the BASELINE on purpose: the
# palette is written in STE four-bit encoding whose extra bit a plain ST
# ignores, so if it looks right here it looks better on an STE.
#
# THE PATH IS QUOTED. `--auto C:\EGATREK.PRG` unquoted has the backslash eaten
# by the shell and Hatari is handed C:EGATREK.PRG, which it cannot open.
# --memsize 1 is a 1040ST. Hatari's default is FOURTEEN MEGABYTES, a machine
# no ST ever was, and its TOS memory test takes long enough to be mistaken for
# a hang. --fast-boot patches the test out.
#
# TOSIMG defaults to EmuTOS, which is free and is what this port was developed
# against. A real TOS is the player's to supply:
#     make run-tos206
#
# --sound 44100 is written out rather than left to a default. THE DEFAULT ALSO
# WORKS -- that was measured, by Jamie listening to three runs: no flag and
# `--sound 44100` both played, `--sound off` did not, which is all three
# behaving correctly. So this flag is explicitness, NOT a fix.
#
# I BRIEFLY WROTE THE OPPOSITE HERE. The release came back reported as silent,
# the next run I made passed a frequency, Jamie heard it, and I put "the one
# run he heard was the one that named a frequency" into this comment as a
# cause. It was a correlation across two runs with one difference I had
# noticed and several I had not, and his A/B killed it an hour later.
#
# WHAT IS ESTABLISHED: the driver is correct. `make sndtest` reads the PSG
# BACK through Giaccess -- the YM2149 allows that where the SID does not --
# and finds a 150Hz note, mixer bit 0 clear, volume 12. What made the release
# session silent is NOT KNOWN. tools/shot.py passes `--sound off`, which is
# why no automated check here could ever have caught it either way.
run: all
	$(HATARI) --machine st --tos $(TOSIMG) --vdi off --memsize 1 --sound 44100 \
	    --fast-boot on --gemdos-drive C -d build --auto 'C:\EGATREK.PRG'

run-ste: all
	$(HATARI) --machine ste --tos $(TOSIMG) --vdi off --memsize 1 --sound 44100 \
	    --fast-boot on --gemdos-drive C -d build --auto 'C:\EGATREK.PRG'

# Real TOS 2.06 instead of EmuTOS, for checking the port is not leaning on one
# ROM -- the Line-A font table is the part that could differ.
run-tos206: all
	$(HATARI) --machine st --tos $(HOME)/hatari/rom/tos206us.img --vdi off \
	    --memsize 1 --sound 44100 --fast-boot on --gemdos-drive C -d build \
	    --auto 'C:\EGATREK.PRG'

clean:
	rm -f build/*.o $(GAME_OUT) build/*.DAT build/*.TXT build/*.zip
	rm -rf $(REL)
