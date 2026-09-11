# EGA Trek for the Commander X16.
#
# Third port. The core compiles for cx16 untouched, and llvm-mos is the same
# compiler family as the C128 and MEGA65 builds, so what was learned about the
# __rc calling convention, overlay sections and -lcopy-data all carries over.
#
# See NOTES.md, "SCOPE: the Commander X16".

LLVM_MOS ?= $(HOME)/llvm-mos
X16CC     = $(LLVM_MOS)/bin/mos-cx16-clang
X16EMU   ?= $(HOME)/x16emu_macos_m1-r48

# TREK_COLOUR_IS_EGA makes EGA_TO_VDC the identity: VERA's palette is
# programmable, so entries 0-15 hold EGA's own values. Same trick as the
# MEGA65, and for the same reason.
CFLAGS = -Wall -Werror -Oz -DTREK_COLOUR_IS_EGA \
         -I src -I ../c128/src -I ../core
OVLFLAGS = $(CFLAGS) -DTREK_OVERLAYS -DOVL_WINDOW=$(OVL_WINDOW)
LDFLAGS  = -T x16.ld -Wl,--no-check-sections
OBJCOPY  = $(LLVM_MOS)/bin/llvm-objcopy
NM       = $(LLVM_MOS)/bin/llvm-nm
OVERLAYS = eval hof front info repair msgs planet cmds title events xtra

# THE WINDOW SIZE, stated once. x16.ld must carry the same number; this is
# what the loader compiles against and what the image cutter enforces.
OVL_WINDOW = 3888

SMOKE_SRC = src/smoke.c src/x16vera.c ../c128/src/layout.c
SMOKE_OUT = build/smoke.prg

.PHONY: sndtest run-sndtest all smoke charset game music data run autoplay clean run-charset run-smoke run-game clean

all: smoke

smoke: $(SMOKE_OUT)

$(SMOKE_OUT): $(SMOKE_SRC) src/x16vera.h Makefile
	@mkdir -p build
	$(X16CC) $(CFLAGS) -o $@ $(SMOKE_SRC)
	@python3 -c "import os; print('x16: smoke %d bytes' % os.path.getsize('$@'))"

CHARSET_SRC = src/charset.c src/x16vera.c ../c128/src/layout.c
CHARSET_OUT = build/charset.prg

charset: $(CHARSET_OUT)

$(CHARSET_OUT): $(CHARSET_SRC) src/x16vera.h Makefile
	@mkdir -p build
	$(X16CC) $(CFLAGS) -o $@ $(CHARSET_SRC)

run-charset: $(CHARSET_OUT)
	cd $(X16EMU) && ./x16emu -prg $(CURDIR)/$(CHARSET_OUT) -run

run-smoke: $(SMOKE_OUT)
	cd $(X16EMU) && ./x16emu -prg $(CURDIR)/$(SMOKE_OUT) -run

clean:
	rm -rf build

# THE EARLY LINK. Not a runnable game -- every seam below the console is a
# stub. It exists to answer one question before any of them is written: does
# the whole thing FIT? See NOTES.md, "SCOPE: the Commander X16", risk 4.
FULL_SRC = ../c128/src/main.c ../c128/src/ui.c ../c128/src/layout.c \
           ../c128/src/strpool.c \
           ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
           src/x16vera.c src/x16storage.c src/x16mem.c src/stubs.c
FULL_OUT = build/full.prg

full: $(FULL_OUT)

$(FULL_OUT): $(FULL_SRC) src/x16vera.h Makefile
	@mkdir -p build
	$(X16CC) $(CFLAGS) -o $@ $(FULL_SRC)
	@python3 -c "import os; \
	    n=os.path.getsize('$@')-2; lim=0x96FF; \
	    print('x16: FULL LINK %d bytes of %d, %d free' % (n, lim, lim-n))"

STORE_SRC = src/storetest.c src/x16storage.c
STORE_OUT = build/storetest.prg

storetest: $(STORE_OUT)

$(STORE_OUT): $(STORE_SRC) Makefile
	@mkdir -p build
	$(X16CC) $(CFLAGS) -o $@ $(STORE_SRC)

# THE BEEP, MEASURED. x16emu can record a WAV -- the only emulator on this
# project that can -- so this runs the SHIPPING sound driver and reads the
# pitch and duration out of the recording rather than out of the source.
#   make run-sndtest
SND_SRC = src/sndtest.c src/x16snd.c src/music_data.c
SND_OUT = build/sndtest.prg

sndtest: $(SND_OUT)

$(SND_OUT): $(SND_SRC) src/x16snd.c Makefile
	@mkdir -p build
	$(X16CC) $(CFLAGS) -DTREK_SND_TEST -o $@ $(SND_SRC)

run-sndtest: $(SND_OUT)
	python3 tools/sndtest.py

MEM_SRC = src/memtest.c src/x16mem.c src/x16storage.c
MEM_OUT = build/memtest.prg

memtest: $(MEM_OUT)

$(MEM_OUT): $(MEM_SRC) Makefile
	@mkdir -p build
	$(X16CC) $(CFLAGS) -o $@ $(MEM_SRC)

# THE GAME WITH OVERLAYS. One link produces the ELF; objcopy then cuts the
# resident half (everything except .ovl_*) into the PRG and each window image
# out separately, concatenated into OVERLAYS.BIN. Both existing ports do this
# and for the same reason -- an overlay is linked WITH the resident half, so
# the two halves are only valid together.
# GENERATED, not committed -- tools/make_music.py composes this port's own
# music. See .gitignore.
music src/music_data.c src/music_data.h:
	cd .. && python3 tools/make_music.py && cp c128/src/music_data.c c128/src/music_data.h x16/src/

GAME_SRC = ../c128/src/main.c ../c128/src/ui.c ../c128/src/layout.c \
           ../c128/src/strpool.c \
           ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
           src/x16vera.c src/x16storage.c src/x16mem.c src/x16ovl.c src/x16input.c src/x16snd.c src/music_data.c
GAME_OUT = build/trekx16.prg

game: $(GAME_OUT)

$(GAME_OUT): $(GAME_SRC) src/x16vera.h src/x16mem.h x16.ld Makefile
	@mkdir -p build
	$(X16CC) $(OVLFLAGS) $(LDFLAGS) -o build/trekx16.elf $(GAME_SRC)
	$(OBJCOPY) -O binary "--remove-section=.ovl_*" build/trekx16.elf build/trekx16.res
	@python3 -c "d=open('build/trekx16.res','rb').read(); \
	    open('$(GAME_OUT)','wb').write(bytes([0x01,0x08]) + d)"
	@rm -f build/OVERLAYS.BIN
	@for o in $(OVERLAYS); do \
	    $(OBJCOPY) -O binary --only-section=.ovl_$$o build/trekx16.elf build/ovl$$o.bin; \
	    python3 -c "import sys; d=open('build/ovl$$o.bin','rb').read(); \
	        sys.exit('overlay $$o is %d bytes, window is $(OVL_WINDOW)' % len(d)) if len(d) > $(OVL_WINDOW) else None; \
	        open('build/OVERLAYS.BIN','ab').write(d + b'\\0'*($(OVL_WINDOW)-len(d)))" || exit 1; \
	done
	@# THE STAMP, into the last two bytes -- see src/x16ovl.c.
	@python3 -c "import subprocess; \
	    nm=subprocess.check_output(['$(NM)','build/trekx16.elf']).decode(); \
	    a=[int(l.split()[0],16) for l in nm.splitlines() if l.split()[-1:]==['ovl_anchor']][0]; \
	    f=open('build/OVERLAYS.BIN','r+b'); f.seek(-2,2); \
	    f.write(bytes([a & 0xFF, (a >> 8) & 0xFF])); f.close(); \
	    print('x16: overlay stamp', hex(a))"
	@python3 -c "import os; n=os.path.getsize('$(GAME_OUT)')-2; lim=0x96ff-$(OVL_WINDOW);   # tracks the window, not a copy of it \
	    print('x16: resident %d of %d, %d free' % (n, lim, lim-n)); \
	    print('x16: OVERLAYS.BIN %d bytes' % os.path.getsize('build/OVERLAYS.BIN'))"

# THE DATA FILES, in a directory x16emu serves as device 8 through HostFS --
# so no disk image is needed and the same code path works against an SD card
# image later. mega65/Makefile assembles the identical set.
DATA ?= build/data

data: $(GAME_OUT)
	@mkdir -p $(DATA)
	@cd .. && python3 tools/gen_strings.py >/dev/null && python3 tools/make_music.py >/dev/null
	@# THE POOL IS SHARED; THE TWO STRINGS THAT NAME THE MACHINE ARE NOT.
	@# Same ids, same count, same strdata.h -- see src/strings.override.txt.
	@cd .. && python3 tools/gen_strings.py --override x16/src/strings.override.txt \
	    x16/build/strings.dat
	@cp build/strings.dat        $(DATA)/STRINGS.DAT
	@cp ../c128/build/music.dat  $(DATA)/MUSIC.DAT
	@cp ../c128/src/briefing.txt $(DATA)/BRIEF.TXT
	@cp build/OVERLAYS.BIN       $(DATA)/OVERLAYS.BIN
	@cp $(GAME_OUT)              $(DATA)/
	@ln -sf $(X16EMU)/rom.bin    $(DATA)/rom.bin
	@ls -l $(DATA) | grep -v total

run: data
	cd $(DATA) && $(X16EMU)/x16emu -prg trekx16.prg -run

# THE RELEASE BUNDLE. The X16 has no disk image the way a 1541 machine does:
# the emulator's HostFS and a real SD card both want a DIRECTORY of files, so
# what ships is a zip of exactly what `make data` assembles, unpacked wherever
# the player keeps their programs.
#
# MUSIC.DAT IS THIS PORT'S OWN COMPOSITION, from tools/make_music.py. The
# extracted note data that tools/gen_music.py pulls out of EGATREK.EXE is
# Anderson's creative work and must never reach a release; make_music.py exists
# so that a release can have sound at all. See its header.
REL = build/release
VERSION ?= dev

release: data
	@rm -rf $(REL) && mkdir -p $(REL)/egatrek-x16
	@cp $(DATA)/trekx16.prg $(DATA)/OVERLAYS.BIN $(DATA)/STRINGS.DAT \
	    $(DATA)/MUSIC.DAT $(DATA)/BRIEF.TXT $(REL)/egatrek-x16/
	@cp README-release.txt $(REL)/egatrek-x16/README.txt
	@cd $(REL) && zip -q -r ../egatrek-x16.zip egatrek-x16
	@echo "release: build/egatrek-x16.zip"
	@unzip -l build/egatrek-x16.zip | tail -n +4

# OBJECTS WITHOUT LTO, built only so verify can read the call graph AS
# WRITTEN. LTO inlines a resident function into main(), at which point a call
# that only works with one particular overlay loaded is indistinguishable from
# the thirteen that are correct -- which is exactly how the hall-of-fame crash
# got out. See tools/overlay_check.py, rule 4.
NOLTO = build/nolto

nolto: $(GAME_SRC) Makefile
	@mkdir -p $(NOLTO)
	@for f in $(GAME_SRC); do \
	    $(X16CC) -c -fno-lto $(OVLFLAGS) -o $(NOLTO)/`basename $$f .c`.o $$f \
	        || exit 1; \
	done

# `data` FIRST, because verify checks OVERLAYS.BIN against the ELF and on a
# clean tree that file does not exist yet. Without this it reported "the image
# file is stale" and then "OVERLAYS.BIN missing" -- two failures that read like
# real faults and are a missing build step. Cost two rounds on 2026-09-07.
verify: $(GAME_OUT) data nolto
	python3 tools/verify_prg.py

# WHAT DOES A KEY ACTUALLY RETURN? -- prints the raw byte GETIN hands back,
# in hex, for every key pressed. Run it, type, read the screen. This is the
# instrument that caught the case bug: the header comment in src/x16input.c
# had REASONED that GETIN speaks PETSCII, and it does not.
keyprobe: src/keyprobe.c Makefile
	@mkdir -p build
	$(X16CC) -Wall -Oz -o build/keyprobe.prg src/keyprobe.c
	@echo "run it, type keys, read the hex off the screen"

# A BUILD THAT PLAYS ITSELF to the console, so this port can be screenshotted
# without a human. x16emu has no key-injection and no memory poke, so the keys
# are compiled in rather than driven from outside -- see TREK_AUTOPLAY in
# src/x16input.c. Never part of the game build.
AUTO_OUT = build/autoplay.prg

autoplay: $(GAME_SRC) src/x16vera.h src/x16mem.h x16.ld
	@mkdir -p build $(DATA)
	$(X16CC) $(OVLFLAGS) -DTREK_AUTOPLAY $(STACKPROBE) $(LDFLAGS) -o build/auto.elf $(GAME_SRC)
	$(OBJCOPY) -O binary "--remove-section=.ovl_*" build/auto.elf build/auto.res
	@python3 -c "d=open('build/auto.res','rb').read(); \
	    open('$(AUTO_OUT)','wb').write(bytes([0x01,0x08]) + d)"
	@rm -f build/AUTO_OVL.BIN
	@for o in $(OVERLAYS); do \
	    $(OBJCOPY) -O binary --only-section=.ovl_$$o build/auto.elf build/a_$$o.bin; \
	    python3 -c "import sys; d=open('build/a_$$o.bin','rb').read(); \
	        open('build/AUTO_OVL.BIN','ab').write(d + b'\\0'*($(OVL_WINDOW)-len(d)))"; \
	done
	@python3 -c "import subprocess; \
	    nm=subprocess.check_output(['$(NM)','build/auto.elf']).decode(); \
	    a=[int(l.split()[0],16) for l in nm.splitlines() if l.split()[-1:]==['ovl_anchor']][0]; \
	    f=open('build/AUTO_OVL.BIN','r+b'); f.seek(-2,2); \
	    f.write(bytes([a & 0xFF, (a >> 8) & 0xFF])); f.close()"
	@cp $(AUTO_OUT) $(DATA)/ && cp build/AUTO_OVL.BIN $(DATA)/OVERLAYS.BIN
	@echo "autoplay built; OVERLAYS.BIN in $(DATA) is now the AUTOPLAY set"
