# EGA Trek for the colour Macintosh: a Toolbox application built with Retro68
# (https://github.com/autc04/Retro68), gcc for m68k-apple-macos.
#
# COLOUR MACS ONLY, by Jamie's decision (2026-11-26): Color QuickDraw, 32-bit
# QuickDraw for the offscreen GWorld (built into System 7, an INIT on 6.0.x),
# and a screen of at least 640x480 -- the console is 80x25 of 8x16, the
# Falcon's geometry, and needs 640x400 plus a menu bar and a title bar.
#
# THE DATA IS INSIDE THE PROGRAM. STRINGS.DAT, MUSIC.DAT and BRIEF.TXT are
# 'EGAd' resources, named after the files the other ports ship, so the shared
# code still asks for them by name and src/macstorage.c answers from the
# resource fork. Saves and the hall of fame are ordinary files beside the app.
#
# The toolchain lives on the Retro68 sparse bundle (mounted from Crucial Win),
# exactly as commodore-uno/mac/ found it. `hdiutil attach` it after a reboot.
RETRO68  ?= /Volumes/Retro68/Retro68-build/toolchain
CC        = $(RETRO68)/bin/m68k-apple-macos-gcc
SIZE      = $(RETRO68)/bin/m68k-apple-macos-size
REZ       = $(RETRO68)/bin/Rez
RINCLUDES = $(RETRO68)/m68k-apple-macos/RIncludes

# -DTREK_COLOUR_IS_EGA: the offscreen GWorld's colour table holds EGA's own
# sixteen colours in EGA's order, so a colour name IS its pixel value --
# the trick the MEGA65, the X16, the Amiga and the Falcon all use.
CFLAGS  = -O2 -Wall -Wextra -Werror -ffunction-sections \
          -DTREK_COLOUR_IS_EGA -I src -I ../c128/src -I ../core
LDFLAGS = -Wl,-gc-sections -Wl,--mac-strip-macsbug

# Pure C with no machine in it, so linked from the Amiga rather than copied:
# far memory is a plain array and ovl_load is empty, here as there.
SHARED_PLAT = ../amiga/src/amigafarmem.c ../amiga/src/amigaovl.c

MAC_SRC = src/macvid.c src/macinput.c src/macstorage.c

# ---------------------------------------------------------------- first light
# The smoke test: the real console frame through the shared layout.c, the
# sixteen colours, every glyph this port authored, the key echo, a file
# written beside the app, the resources read back, and the Sound Manager
# asked for two square-wave channels. See src/smoke.c.
SMOKE_SRC = src/smoke.c $(MAC_SRC) $(SHARED_PLAT) \
            ../c128/src/strpool.c ../c128/src/layout.c

DATA = build/STRINGS.DAT build/MUSIC.DAT build/BRIEF.TXT

# ---------------------------------------------------------------- the game
# Every shared file, unchanged, and the four Mac ones. No -DTREK_OVERLAYS:
# core/overlay.h turns OVL_CODE into nothing and amigaovl.c's empty ovl_load
# answers, so everything the 8-bit ports page in is resident. Retro68 puts
# all of it in one CODE segment, 'Main', ~72K -- past the classic 32K limit,
# and it loads: measured 2026-11-26.
GAME_SRC = ../c128/src/main.c ../c128/src/ui.c ../c128/src/layout.c \
           ../c128/src/strpool.c \
           ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
           $(MAC_SRC) src/macsnd.c src/midinote.c src/music_data.c \
           $(SHARED_PLAT)

.PHONY: all game dsk verify release smoke data music test clean

all: game dsk test

# WHAT THE COMPILER, THE LINKER AND THE TESTS CANNOT SEE: the console's fit,
# every glyph the shared UI can draw (the Falcon's sweep, imported), the data
# resources byte-identical to the files built beside them, the Finder bundle
# flag, and the partition. See tools/verify_mac.py.
verify: game test
	python3 tools/verify_mac.py

# THE RELEASE ASSET: one zip holding the 800K floppy (the application alone,
# the data inside it), the same application as MacBinary for copying to a
# real Mac's own disk, and README.txt. A zip because a MacBinary file must
# not be touched by a browser or a mail client on the way, and because a
# README must travel beside it. THE RULE CLEARS ITS OWN build/egatrek-*.zip
# FIRST: the root Makefile lists artefacts with a glob, and a stale name left
# here would be offered for upload (the CoCo 3 rename, v0.21.0).
REL = build/release

release: verify dsk
	@rm -rf $(REL) build/egatrek-*.zip && mkdir -p $(REL)/egatrek-mac
	@cp build/EGATREK.dsk $(REL)/egatrek-mac/egatrek.dsk
	@cp build/EGATREK.bin $(REL)/egatrek-mac/egatrek.bin
	@cp README-release.txt $(REL)/egatrek-mac/README.txt
	@cd $(REL) && zip -q -X -r ../egatrek-mac.zip egatrek-mac
	@echo "release: build/egatrek-mac.zip"
	@unzip -l build/egatrek-mac.zip | tail -n +4

game: build/EGATREK.bin

build/egatrek.code.bin: $(GAME_SRC) $(wildcard src/*.h) Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(GAME_SRC)
	@$(SIZE) $@.gdb | tail -1

build/EGATREK.bin: build/egatrek.code.bin src/egatrek.r build/icon.r $(DATA) \
                   tools/setbundle.py
	$(REZ) src/egatrek.r -I$(RINCLUDES) -Ibuild \
	  --copy build/egatrek.code.bin -o $@ -t APPL -c EGAt
	python3 tools/setbundle.py $@
	@ls -l $@

build/icon.r: tools/mkicon.py
	@mkdir -p build
	python3 tools/mkicon.py $@

# THE RELEASE FLOPPY: an 800K HFS disk holding nothing but the application
# -- the data is inside it -- built from the MacBinary with Retro68's
# hfsutils so it carries the Finder flags setbundle.py set. hmount keeps its
# state in $$HOME/.hcwd, so HOME points into build/. fixdates.py pins the
# volume's dates so the image reproduces byte for byte. commodore-uno/mac's
# recipe, unchanged.
dsk: build/EGATREK.dsk

build/EGATREK.dsk: build/EGATREK.bin tools/fixdates.py
	rm -f $@
	dd if=/dev/zero of=$@ bs=1024 count=800 2>/dev/null
	HOME=$(CURDIR)/build $(RETRO68)/bin/hformat -l "EGA Trek" $@ >/dev/null
	HOME=$(CURDIR)/build $(RETRO68)/bin/hmount $@ >/dev/null
	HOME=$(CURDIR)/build $(RETRO68)/bin/hcopy -m build/EGATREK.bin ":EGA Trek"
	HOME=$(CURDIR)/build $(RETRO68)/bin/humount >/dev/null
	python3 tools/fixdates.py $@

# THE MUSIC IS THIS PORT'S OWN COMPOSITION, from tools/make_music.py -- never
# the note data tools/gen_music.py extracts from EGATREK.EXE, which is
# Anderson's creative work and can never ship. The track table is copied
# here like every port's (gitignored).
music src/music_data.c src/music_data.h:
	cd .. && python3 tools/make_music.py >/dev/null && \
	    cp c128/src/music_data.c c128/src/music_data.h mac/src/

# The track format's Hz/10 -> MIDI note table, checked on the HOST against
# floating point for every byte, and against every composed note whose byte
# is its own. `make test BREAK=1` shifts the table a semitone and must fail --
# built to its OWN file, or an up-to-date green binary would answer for it.
TEST_MIDI = build/test_midi$(if $(BREAK),_broken)

test: $(TEST_MIDI)
	./$(TEST_MIDI)

$(TEST_MIDI): test/test_midi.c src/midinote.c src/midinote.h
	@mkdir -p build
	cc -O2 -Wall -Wextra -Werror $(if $(BREAK),-DBREAK_IT) -o $@ \
	    test/test_midi.c src/midinote.c -lm

smoke: build/SMOKE.bin

build/smoke.code.bin: $(SMOKE_SRC) $(wildcard src/*.h) Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SMOKE_SRC)
	@$(SIZE) $@.gdb | tail -1

# Rez reads the data files by path relative to THIS directory -- see
# src/egatrek.r -- so it must run here, after the data is built.
build/SMOKE.bin: build/smoke.code.bin src/egatrek.r build/icon.r $(DATA)
	$(REZ) src/egatrek.r -I$(RINCLUDES) -Ibuild \
	  --copy build/smoke.code.bin -o $@ -t APPL -c EGAt
	@ls -l $@

# ---------------------------------------------------------------- the data
# The same three files every port ships, built the same way; only the two
# strings that name the machine differ (src/strings.override.txt).
data: $(DATA)

build/MUSIC.DAT: src/music_data.c
	@mkdir -p build
	cp ../c128/build/music.dat $@

build/BRIEF.TXT: ../c128/src/briefing.txt
	@mkdir -p build
	cp $< $@

build/STRINGS.DAT: ../c128/src/strings.txt src/strings.override.txt
	@mkdir -p build
	cd .. && python3 tools/gen_strings.py >/dev/null
	cd .. && python3 tools/gen_strings.py --override mac/src/strings.override.txt \
	    mac/build/STRINGS.DAT
	@ls -l $@

clean:
	rm -f build/*.bin build/*.gdb build/*.DAT build/*.TXT build/*.dsk build/test_midi* build/icon.r build/.hcwd build/*.zip
	rm -rf build/release
