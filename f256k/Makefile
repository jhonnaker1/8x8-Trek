# EGA Trek for the Foenix F256K. THE TWELFTH-AND-A-HALF PORT.
#
# llvm-mos, NOT cc65 -- which is the opposite of commodore-uno's F256 port and
# is a decision with reasons, measured 2026-09-17:
#
#   * On a fair full link of core/ the two compilers are WITHIN 1% on size.
#     The often-quoted "13% smaller" and uno's "2.3x" were both unreproduced;
#     llvm-mos's real advantage is the MEMORY MODEL, and against the ~40K this
#     machine gives a program that is the number that decides.
#   * cc65 would put C89 on the SHARED half forever -- ui.c has eight
#     declaration-after-statement sites and the root `c89-check` guards core/
#     only. One port must not constrain every other port's shared sources.
#
# There is no `f256` platform in llvm-mos, so f256k.ld, tools/mkpgz.py and the
# kernel shim are this port's own -- the Plus/4's pattern, which proved a
# platform is a convenience and not a requirement.
CC       = $(HOME)/llvm-mos/bin/mos-common-clang
OBJCOPY  = $(HOME)/llvm-mos/bin/llvm-objcopy
NM       = $(HOME)/llvm-mos/bin/llvm-nm

CPU      = -mcpu=mos65c02
# TREK_COLOUR_IS_EGA makes EGA_TO_VDC the IDENTITY. It is the right answer for
# any target with a programmable palette, and this one has two LUTs to load.
# The Plus/4 shipped for weeks with its colour header never compiled in and the
# gate passed the whole time, so this define is not a detail.
CFLAGS   = $(CPU) -Oz -DTREK_COLOUR_IS_EGA -I src -I ../c128/src -I ../core -T f256k.ld
LDFLAGS  = -Wl,--no-check-sections -linit-stack -lzero-bss -lcopy-data

# THE RIG. MAME's f256k driver boots, Lua scripting works, and a PGZ reaches it
# through a FAT partition on an SD-card image -- so `run` copies onto a WORKING
# COPY and types `/- <name>` at the SuperBASIC prompt, the way a person does.
# commodore-uno's run.sh is where that recipe came from.
MAME_DIR ?= $(HOME)/Downloads/mame-foenix256k
SDCARD   ?= $(MAME_DIR)/sdcard.img
MAME     = $(MAME_DIR)/mame
MAMEOPT  = -rompath "$(MAME_DIR)" -window -skip_gameinfo -nothrottle

# KEEP THE INTERMEDIATES. Chained implicit rules make .elf and .bin
# "intermediate" and make deletes them -- and the .elf is where every probe's
# symbol addresses come from, and the .map is the only place the layout is
# written down. Losing them turns every debugging session into a rebuild.
.PRECIOUS: build/%.elf build/%.bin build/%.low

# ----------------------------------------------------------------- THE GAME
# NOT ../c128/src/storage.c, input.c, sid.c, farmem.c, overlay.c or vdcram.c:
# every one of those is C128 KERNAL or SID code and this port writes its own.
# What IS shared is main, the layout, the whole UI, the string pool and all of
# core/ -- which is the point of the four seams.
SHARED = ../c128/src/main.c ../c128/src/layout.c ../c128/src/ui.c \
         ../c128/src/music_data.c ../c128/src/strpool.c \
         ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c
PLATFORM = src/f256vid.c src/f256key.c src/f256evt.c src/f256stor.c \
           src/f256ovl.c src/f256far.c src/f256snd.c src/f256log.c
GAME_SRC = $(SHARED) $(PLATFORM)
GAME = egatrek

# What goes on the card, and the names are the ones the CODE OPENS -- grepped
# out of the sources rather than remembered. A file whose name does not match
# is a file the game cannot find, and this seam reports that as "no save yet".
DISK = build/disk

.PHONY: all hello frame keys store ovl far snd game disk run check check-ovl \
        check-far check-snd verify clean
all: game
hello: build/hello.pgz
game: build/$(GAME).pgz build/$(GAME).ovl
disk: $(DISK)/EGATREK.PGZ
frame: build/frametest.pgz
keys: build/keytest.pgz
store: build/storetest.pgz
ovl: build/ovltest.pgz build/ovltest.ovl
far: build/fartest.pgz build/fartest.ovl build/fartest.dat build/fartest2.dat
snd: build/sndtest.pgz

build:
	@mkdir -p build build/snap

build/%.elf: src/%.c f256k.ld Makefile | build
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@ $<

# kb_waitkey through the real input layer and the real driver.
build/keytest.elf: src/keytest.c src/f256vid.c src/f256key.c src/f256evt.c \
                   src/f256vid.h src/f256kern.h src/f256evt.h f256k.ld Makefile | build
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@ \
	    src/keytest.c src/f256vid.c src/f256key.c src/f256evt.c

build/storetest.elf: src/storetest.c src/f256vid.c src/f256key.c src/f256evt.c \
                     src/f256stor.c \
                     src/f256vid.h src/f256kern.h src/f256evt.h f256k.ld Makefile | build
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@ \
	    src/storetest.c src/f256vid.c src/f256key.c src/f256evt.c src/f256stor.c

# THE OVERLAY BUILD. -DTREK_OVERLAYS turns OVL_CODE from nothing into a
# section attribute; without it every overlay function is inlined into its
# resident caller and every image comes out EMPTY -- which mkovl.py refuses by
# name rather than shipping.
# THE TWO OPT-IN OVERLAYS ARE ON HERE, and this is the port that can afford
# them. overlay.h enables them per target because they page code on the HOT
# PATH; on a 1541 that is ruinous and through the Atari's MEMAC window it
# costs milliseconds. Here a swap is one store to an MMU slot -- about ten
# cycles -- so "the test is FREQUENCY, not size" simply does not bind.
# They are what closed a 1,161-byte resident overflow.
OVLFLAGS = -DTREK_OVERLAYS -DTREK_OVL_ENEMY -DTREK_OVL_MOVE

# Turns off f256evt.c's snd_poll stub so the real driver can define it.
SNDFLAGS = -DTREK_F256_SOUND

build/ovltest.elf: src/ovltest.c src/f256vid.c src/f256key.c src/f256evt.c \
                   src/f256stor.c src/f256ovl.c \
                   src/f256vid.h src/f256kern.h src/f256evt.h f256k.ld Makefile | build
	$(CC) $(CFLAGS) $(OVLFLAGS) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@ \
	    src/ovltest.c src/f256vid.c src/f256key.c src/f256evt.c \
	    src/f256stor.c src/f256ovl.c

# OVERLAYS.BIN RIDES WITH THE PGZ, so `run` and `check` must put BOTH on the
# card. The images are cut from the SAME ELF the program came from, which is
# what makes the stamp meaningful.
build/%.ovl: build/%.elf
	@python3 tools/mkovl.py $< $@ --objcopy $(OBJCOPY) --nm $(NM) --window 8192

build/fartest.elf: src/fartest.c src/f256vid.c src/f256key.c src/f256evt.c \
                   src/f256stor.c src/f256ovl.c src/f256far.c \
                   src/f256vid.h src/f256kern.h src/f256evt.h src/f256ovl.h \
                   f256k.ld Makefile | build
	$(CC) $(CFLAGS) $(OVLFLAGS) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@ \
	    src/fartest.c src/f256vid.c src/f256key.c src/f256evt.c \
	    src/f256stor.c src/f256ovl.c src/f256far.c

build/fartest.dat build/fartest2.dat: tools/mkfar.py | build
	@python3 tools/mkfar.py build/fartest.dat build/fartest2.dat

# sndtest INCLUDES f256snd.c rather than linking it -- see the file. Linking
# it as well would be two definitions of every symbol in it.
build/sndtest.elf: src/sndtest.c src/f256vid.c src/f256evt.c src/f256snd.c \
                   src/f256vid.h src/f256kern.h src/f256evt.h f256k.ld Makefile | build
	$(CC) $(CFLAGS) $(SNDFLAGS) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@ \
	    src/sndtest.c src/f256vid.c src/f256evt.c ../c128/src/music_data.c

build/$(GAME).elf: $(GAME_SRC) src/f256vid.h src/f256kern.h src/f256evt.h \
                   src/f256ovl.h f256k.ld Makefile | build
	$(CC) $(CFLAGS) $(OVLFLAGS) $(SNDFLAGS) $(LDFLAGS) \
	    -Wl,-Map=$(@:.elf=.map) -o $@ $(GAME_SRC)

# ------------------------------------------------------------- THE CARD
# STRINGS.DAT and MUSIC.DAT come from the shared generators, so this port's
# prose and music are the same files every other port ships -- there is no
# F256K override, because at eighty columns there is nothing to shorten.
# TWICE, and the first pass is not redundant: the bare run regenerates the
# canonical list and strdata.h, and the --override run applies this port's
# names on top. Running only the second would build against whatever list was
# last written by some other port's build.
build/strings.dat: ../c128/src/strings.txt src/strings.override.txt
	@mkdir -p build
	cd .. && python3 tools/gen_strings.py >/dev/null
	cd .. && python3 tools/gen_strings.py --override f256k/src/strings.override.txt \
	    f256k/build/strings.dat

# make_music.py, NOT gen_music.py. The latter extracts Anderson's own note
# data, which is his creative work: gitignored, and it can never ship.
build/music.dat: ../c128/src/music_data.c
	@mkdir -p build
	cd .. && python3 tools/make_music.py >/dev/null
	@cp ../c128/build/music.dat $@

$(DISK)/EGATREK.PGZ: build/$(GAME).pgz build/$(GAME).ovl build/strings.dat \
                     build/music.dat ../c128/src/briefing.txt verify
	@rm -rf $(DISK) && mkdir -p $(DISK)
	@cp build/$(GAME).pgz $(DISK)/EGATREK.PGZ
	@cp build/$(GAME).ovl $(DISK)/OVERLAYS.BIN
	@cp build/strings.dat $(DISK)/STRINGS.DAT
	@cp build/music.dat   $(DISK)/MUSIC.DAT
	@cp ../c128/src/briefing.txt $(DISK)/BRIEF.TXT
	@# THE HALL OF FAME SHIPS EMPTY. The original's TREK.SCR is Anderson's
	@# file; an all-zero table is what a fresh install should have, and the
	@# game writes this one back as people finish games.
	@python3 -c "open('$(DISK)/TREK.SCR','wb').write(bytes(280))"
	@ls -l $(DISK) | tail -n +2 | awk '{printf "  %-14s %7d\n", $$9, $$5}'

# Every file the game opens, as "local:DESTNAME" for sdrun.sh. Derived from
# the disk directory rather than listed twice -- a run that copies a stale
# subset is a run testing something other than the artefact.
DISK_EXTRA = $(DISK)/OVERLAYS.BIN:OVERLAYS.BIN $(DISK)/STRINGS.DAT:STRINGS.DAT \
             $(DISK)/MUSIC.DAT:MUSIC.DAT $(DISK)/BRIEF.TXT:BRIEF.TXT \
             $(DISK)/TREK.SCR:TREK.SCR

# THE GAME, LAUNCHED AND THEN LEFT RUNNING for a person. Throttled, so it plays
# at the speed the machine really runs at, and the script types `/- egatrek` so
# nobody has to. $SD_HUMAN is what keeps the throttle on with a script driving.
play: disk
	@SD_HUMAN=1 SD_EXTRA="$(DISK_EXTRA)" TREK_NAME=egatrek \
	 TREK_SIG=$$($(NM) build/$(GAME).elf | awk '$$3 == "trek_sig" { print $$1 }') \
	 bash tools/sdrun.sh "$(MAME_DIR)" "$(SDCARD)" $(DISK)/EGATREK.PGZ egatrek \
	     tools/playboot.lua

# And the automated one: boot, launch, wait, snapshot, exit.
check-game: disk
	@SD_EXTRA="$(DISK_EXTRA)" TREK_NAME=egatrek \
	 TREK_SIG=$$($(NM) build/$(GAME).elf | awk '$$3 == "trek_sig" { print $$1 }') \
	 bash tools/sdrun.sh "$(MAME_DIR)" "$(SDCARD)" $(DISK)/EGATREK.PGZ egatrek tools/run.lua

# THE ARTEFACT IS SIX LOOSE FILES IN A ZIP, not a disk image. Every other port
# ships something a drive can mount; this machine reads a FAT card, and handing
# someone a .img would make them write a whole card to play a 76K game. The
# Falcon and the Amiga ship folders for the same reason.
REL = build/rel
release: disk
	@rm -rf $(REL) && mkdir -p $(REL)/egatrek-f256k
	@cp $(DISK)/EGATREK.PGZ $(DISK)/OVERLAYS.BIN $(DISK)/STRINGS.DAT \
	    $(DISK)/MUSIC.DAT $(DISK)/BRIEF.TXT $(DISK)/TREK.SCR \
	    $(REL)/egatrek-f256k/
	@cp README-release.txt $(REL)/egatrek-f256k/README.txt
	@cd $(REL) && zip -q -r ../egatrek-f256k.zip egatrek-f256k
	@echo "release: build/egatrek-f256k.zip"
	@unzip -l build/egatrek-f256k.zip | tail -n +4

verify: build/$(GAME).pgz build/$(GAME).ovl build/strings.dat
	@python3 tools/verify_f256.py

build/lowprobe.elf: src/lowprobe.c src/f256vid.c src/f256key.c src/f256evt.c \
                    src/f256stor.c \
                    src/f256vid.h src/f256kern.h src/f256evt.h f256k.ld Makefile | build
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@ \
	    src/lowprobe.c src/f256vid.c src/f256key.c src/f256evt.c src/f256stor.c

# The console frame through the REAL shared layout.c and the REAL driver. A
# pattern rule cannot express this one: it is three sources, not one.
build/frametest.elf: src/frametest.c src/f256vid.c src/f256evt.c \
                     ../c128/src/layout.c \
                     src/f256vid.h src/f256kern.h src/f256evt.h f256k.ld Makefile | build
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@ \
	    src/frametest.c src/f256vid.c src/f256evt.c ../c128/src/layout.c

# TWO SEGMENTS NOW, AND SELECTED BY NAME RATHER THAN BY WHAT IS LEFT OVER.
# The ELF holds three kinds of thing at three very different addresses: the
# resident program at $2000, read-only data at $0400 (the low region), and the
# overlay images at $100000 and up, which no CPU ever sees. A plain
# `objcopy -O binary` spans ALL of them and produced a 1,129,475-byte PGZ --
# the gap between $2000 and $100000 written out as zeros.
build/%.bin: build/%.elf
	$(OBJCOPY) -O binary -j .text -j .data $< $@

build/%.low: build/%.elf
	$(OBJCOPY) -O binary -j .lowrodata $< $@

# THE ENTRY POINT COMES FROM THE BINARY, NOT FROM A CONSTANT. A PGZ carries its
# start address in a zero-size trailing segment, and a wrong one gives a file
# that loads and never runs -- silently.
# The low segment is passed only when there IS one: mkpgz would happily write
# a zero-length segment, and a zero-length segment is how a PGZ says "this is
# the entry point" -- so an empty .lowrodata would terminate the file early
# and the program would never load.
build/%.pgz: build/%.bin build/%.low
	@python3 tools/mkpgz.py $< $@ --load 0x2000 \
	    --exec 0x$$($(NM) $(<:.bin=.elf) | awk '$$3 == "_start" { print $$1 }') \
	    $$(test -s $(<:.bin=.low) && echo --extra 0x0400:$(<:.bin=.low))

# A WORKING COPY, ALWAYS: the SD image is the user's and must not be written.
run: build/$(P).pgz
	@bash tools/sdrun.sh "$(MAME_DIR)" "$(SDCARD)" build/$(P).pgz $(P)

clean:
	rm -rf build

# An AUTOMATED run: same disk, same pexec launch, but scripted -- the marker is
# read out of the machine and a snapshot is taken. `run` is for a person and
# leaves the machine up; `check` is for me and exits.
# The overlay swap, end to end: thirteen images into thirteen RAM banks,
# then every one loaded forwards, backwards and interleaved, checked BY VALUE.
check-ovl: build/ovltest.pgz build/ovltest.ovl
	@SD_EXTRA="build/ovltest.ovl:OVERLAYS.BIN" \
	 A_SIG=$$($(NM) build/ovltest.elf | awk '$$3=="trek_sig"{print $$1}') \
	 A_RAN=$$($(NM) build/ovltest.elf | awk '$$3=="ran"{print $$1}') \
	 A_GOT=$$($(NM) build/ovltest.elf | awk '$$3=="got"{print $$1}') \
	 A_BACK=$$($(NM) build/ovltest.elf | awk '$$3=="got_back"{print $$1}') \
	 A_MIX=$$($(NM) build/ovltest.elf | awk '$$3=="got_mixed"{print $$1}') \
	 A_PASS=$$($(NM) build/ovltest.elf | awk '$$3=="pass"{print $$1}') \
	 A_N=13 \
	 bash tools/sdrun.sh "$(MAME_DIR)" "$(SDCARD)" build/ovltest.pgz ovltest tools/ovl.lua

# Far memory: two tenants, bank straddles, and a read issued from inside an
# overlay -- the hazard that comes from having no slot to spare.
# Sound: four calibration tones recorded and measured, then the tempo counted
# against MAME's own clock. TWO TOOLS BECAUSE ONE CANNOT SEE BOTH -- hearit.py
# reads pitch and is silent about rate, which is how a tune once played at
# double speed with every note in tune.
check-snd: snd
	@SD_WAV=build/snd.wav \
	 A_SIG=$$($(NM) build/sndtest.elf | awk '$$3=="trek_sig"{print $$1}') \
	 A_RAN=$$($(NM) build/sndtest.elf | awk '$$3=="ran"{print $$1}') \
	 A_RUNNING=$$($(NM) build/sndtest.elf | awk '$$3=="tempo_running"{print $$1}') \
	 A_FRAMES=$$($(NM) build/sndtest.elf | awk '$$3=="snd_frames"{print $$1}') \
	 A_TICKS=$$($(NM) build/sndtest.elf | awk '$$3=="snd_ticks"{print $$1}') \
	 bash tools/sdrun.sh "$(MAME_DIR)" "$(SDCARD)" build/sndtest.pgz sndtest tools/snd.lua
	@echo "PITCH:"
	@python3 tools/pitch.py build/snd.wav 440 1000 200 440

check-far: far
	@SD_EXTRA="build/fartest.ovl:OVERLAYS.BIN build/fartest.dat:FARTEST.DAT build/fartest2.dat:FARTEST2.DAT" \
	 A_SIG=$$($(NM) build/fartest.elf | awk '$$3=="trek_sig"{print $$1}') \
	 A_RAN=$$($(NM) build/fartest.elf | awk '$$3=="ran"{print $$1}') \
	 A_RES=$$($(NM) build/fartest.elf | awk '$$3=="res"{print $$1}') \
	 A_DET=$$($(NM) build/fartest.elf | awk '$$3=="detail"{print $$1}') \
	 A_N=$$($(NM) build/fartest.elf | awk '$$3=="done_n"{print $$1}') \
	 bash tools/sdrun.sh "$(MAME_DIR)" "$(SDCARD)" build/fartest.pgz fartest tools/far.lua

check: build/$(P).pgz
	@TREK_NAME=$(P) \
	 TREK_MARKER=$$($(NM) build/$(P).elf | awk '$$3 == "ran" { print $$1 }') \
	 TREK_SIG=$$($(NM) build/$(P).elf | awk '$$3 == "trek_sig" { print $$1 }') \
	 bash tools/sdrun.sh "$(MAME_DIR)" "$(SDCARD)" build/$(P).pgz $(P) tools/run.lua
