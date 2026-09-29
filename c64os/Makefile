# EGA Trek as a C64 OS application, the sixteenth port.
#
# THE C64 + REU PORT (../c64reu) MOVED INTO C64 OS. Same C, same overlays
# swapped from the REU by the same manager (../c64reu/src/reuovl.s); what
# is this directory's is where the machine is C64 OS's rather than the
# C64's:
#
#   src/app.s      the application header, the KERNAL link table, the
#                  crossings between C64 OS and the game's C, both key
#                  queues, and quitting from inside the event loop
#   src/osvid.c    the screen, drawn into C64 OS's buffers and font, with
#                  fifteen box-drawing glyphs the font does not have
#   src/osinput.c  keys from C64 OS's queues
#   src/osfile.c   the app's folder, its two REU banks, the far store and
#                  the message log, both in the REU
#   src/osovl.c    c64reu's one-time overlay load, from the app's folder
#
# The measurements every choice here rests on are in NOTES.md, "C64 OS PORT".
#
#   make           build/bundle/ -- everything the app's folder holds
#   make car       build/egatrek.car -- the bundle as C64 OS installs it
#   make release   build/egatrek-c64os.zip -- the .car and README.txt
#   make install   onto a COPY of your C64 OS disk, every byte read back
#   make run       boot C64 OS in VICE; double-click "EGA Trek"
#
# C64 OS, its disk image and the ROMs are yours: C64OS_ROMS (tools/rig.py).
.DEFAULT_GOAL := all

LLVM_MOS ?= $(HOME)/llvm-mos
CC       = $(LLVM_MOS)/bin/mos-c64-clang
OBJCOPY  = $(LLVM_MOS)/bin/llvm-objcopy
NM       = $(LLVM_MOS)/bin/llvm-nm

# The folder the player sees in //os/applications, and the name inside the
# .car that C64 OS's extractor makes it from.
APP = EGA Trek
VERSION  = 0.23.0
CAR_DATE = 2026-09-28
# Up here, not beside its rule: `verify` names it as a prerequisite, and
# make expands a prerequisite list when it reads the rule -- defined below
# `verify`, this was empty there, and verify ran from clean before anything
# was built. It passed until the release cut wiped build/.
CAR = build/egatrek.car

CFLAGS  = -Oz -nostartfiles -mlto-zp=0 -I src -I ../c128/src -I ../core -Wall -Werror \
          -DTREK_OVERLAYS -DTREK_40COL -DTREK_C64OS \
          -DTREK_OVL_ENEMY -DTREK_OVL_MOVE -DTREK_OVL_REU -T c64os.ld \
          -Wa,--defsym,NTHUNKS=80
LDFLAGS = -Wl,--no-check-sections -Wl,--emit-relocs

SHARED = ../c128/src/main.c ../c128/src/layout40.c ../c128/src/ui.c \
         ../c128/src/sid.c ../c128/src/music_data.c ../c128/src/storage.c \
         ../c128/src/strpool.c \
         ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
         ../c64reu/src/reuovl.s
PLATFORM = src/app.s src/osvid.c src/osinput.c src/osfile.c src/osovl.c
SRC = $(SHARED) $(PLATFORM)
HDR = src/c64os.h ../core/overlay.h

ELF    = build/trek.elf
BUNDLE = build/bundle
OVERLAYS = eval hof front info repair msgs planet cmds title events xtra \
           enemy move view panel nav time turn laser torp main dlg io

.PHONY: all car verify release release-test install run clean

all: $(BUNDLE)/files.txt

# Linked, then PATCHED -- every call that crosses overlays pointed at a
# thunk (../c64reu/tools/reu_thunks.py) -- then the relocations the patch
# needed are dropped, or objcopy will not cut the overlays out.
$(ELF): $(SRC) $(HDR) c64os.ld ../c64reu/tools/reu_thunks.py Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=build/trek.map -o $@.tmp $(SRC)
	python3 ../c64reu/tools/reu_thunks.py $@.tmp -DTREK_C64OS
	$(OBJCOPY) -w -R '.rela*' $@.tmp $@
	@rm -f $@.tmp

# main.o: the resident image from $0900, every gap -- .bss, the window, the
# stack -- written as zeros, with the load address in front.
# The overlays: each cut out and stamped with ovl_load's address, which
# src/osovl.c checks before it trusts an image for the whole game.
$(BUNDLE)/main.o: $(ELF)
	@mkdir -p $(BUNDLE)
	$(OBJCOPY) -O binary --remove-section='.ovl_*' $(ELF) build/main.bin
	@python3 -c "d = open('build/main.bin','rb').read(); \
	    open('$@','wb').write(bytes([0x00,0x09]) + d); \
	    print('  main.o: %d bytes, \$$0900..\$$%04X' % (len(d), 0x0900 + len(d) - 1))"
	@stamp=`$(NM) $(ELF) | awk '$$3 == "ovl_load" { print $$1 }'`; \
	win=`$(NM) $(ELF) | awk '$$3 == "__ovl_start" { print $$1 }'`; \
	for o in $(OVERLAYS); do \
	    $(OBJCOPY) --dump-section=.ovl_$$o=build/ovl_$$o.raw $(ELF) /dev/null || exit 1; \
	    python3 -c "import sys; \
	        n = sys.argv[1]; a = int(sys.argv[2], 16); w = int(sys.argv[3], 16); \
	        d = open('build/ovl_%s.raw' % n, 'rb').read() + bytes([a & 0xFF, a >> 8]); \
	        open('$(BUNDLE)/ovl%s' % n, 'wb').write(bytes([w & 0xFF, w >> 8]) + d); \
	        sys.exit('overlay %s does not fit the window' % n) if len(d) > 0x1000 else None" \
	        $$o $$stamp $$win || exit 1; \
	done

build/strings.dat: ../c128/src/strings.txt src/strings.override.txt ../tools/gen_strings.py
	@mkdir -p build
	cd .. && python3 tools/gen_strings.py >/dev/null
	cd .. && python3 tools/gen_strings.py --override c64os/src/strings.override.txt \
	    c64os/build/strings.dat

../c128/build/music.dat:
	cd .. && python3 tools/make_music.py >/dev/null

# THE BUNDLE, and the list the installer copies from. main.o and the
# overlays are PRG: the KERNAL's LOAD reads them. STRINGS.DAT and MUSIC.DAT
# are SEQ, where the C64's disk has PRG -- storage.c opens everything as SEQ
# and osfile.c streams them into the REU through it.
$(BUNDLE)/files.txt: $(BUNDLE)/main.o build/strings.dat ../c128/build/music.dat \
                     ../c128/src/briefing40.txt src/menu.json tools/c64os_meta.py
	cp build/strings.dat $(BUNDLE)/strings.dat
	cp ../c128/build/music.dat $(BUNDLE)/music.dat
	cp ../c128/src/briefing40.txt $(BUNDLE)/brief.txt
	@python3 -c "import sys; sys.stdout.buffer.write(b''.join(b'.'*25 + b'\r\n0\r\n' for _ in range(10)))" > $(BUNDLE)/trek.scr
	python3 tools/c64os_meta.py menu src/menu.json > $(BUNDLE)/menu.m
	python3 tools/c64os_meta.py about "EGA Trek" $(VERSION) 2026 "Nels Anderson" > $(BUNDLE)/about.t
	@( echo "main.o,p"; echo "menu.m,s"; echo "about.t,s"; \
	   echo "strings.dat,s"; echo "music.dat,s"; echo "brief.txt,s"; echo "trek.scr,s"; \
	   for o in $(OVERLAYS); do echo "ovl$$o,p"; done ) > $@
	@echo "bundle: $(BUNDLE), `wc -l < $@ | tr -d ' '` files"

# The checks the link cannot make -- the file ceiling, the window, the
# archive against the bundle, the music. See tools/verify_c64os.py.
verify: $(CAR)
	python3 tools/verify_c64os.py

# THE RELEASE IS A .car, as C64 OS apps ship: copied to //os/applications
# and double-clicked, C64 OS's extractor makes the EGA Trek folder there.
# tools/mkcar.py writes it the way the third-party app archives on C64 OS
# 1.08's own volume are built, and reads it back -- every record, and the
# CRC32 trailer -- before anything is allowed to ship it. CAR_DATE is fixed
# so the same bundle makes the same archive byte for byte.
car: $(CAR)

$(CAR): $(BUNDLE)/files.txt tools/mkcar.py
	python3 tools/mkcar.py write $(BUNDLE) "$(APP)" "EGA Trek for C64 OS v$(VERSION)" $(CAR_DATE) $@
	python3 tools/mkcar.py read $@ > /dev/null

# The zip carries the .car and README.txt in an egatrek-c64os folder, as the
# other zip ports do. The .car keeps a lowercase name, as all three sample
# archives have: it is copied onto a C64 filesystem, often off an SD card.
# Clears its own build/egatrek-*.zip first: the root Makefile lists
# artefacts with a glob.
REL = build/release

release: verify README-release.txt
	@rm -f build/egatrek-*.zip
	@rm -rf $(REL) && mkdir -p $(REL)/egatrek-c64os
	@cp $(CAR) $(REL)/egatrek-c64os/egatrek.car
	@cp README-release.txt $(REL)/egatrek-c64os/README.txt
	@cd $(REL) && zip -q -r ../egatrek-c64os.zip egatrek-c64os
	@echo "release: build/egatrek-c64os.zip"
	@unzip -l build/egatrek-c64os.zip | tail -n +4

# THE RELEASE, TESTED THE WAY A PLAYER INSTALLS IT: egatrek.car taken out
# of the ZIP (not build/), put into //os/applications on a FRESH copy of the
# C64 OS disk, and C64 OS left to extract it -- double-click egatrek.car,
# then EGA Trek. `make release-test ZIP=path` tests a zip from anywhere.
ZIP ?= build/egatrek-c64os.zip
release-test:
	rm -rf build/cartest && mkdir -p build/cartest
	unzip -o -j -q "$(ZIP)" egatrek-c64os/egatrek.car -d build/cartest
	echo "egatrek.car,p" > build/cartest/files.txt
	rm -f build/work.dhd
	python3 tools/rig.py install build/cartest ""
	python3 tools/rig.py play

install: $(BUNDLE)/files.txt
	python3 tools/rig.py install $(BUNDLE) "$(APP)"

run:
	python3 tools/rig.py boot --human

clean:
	rm -rf build/bundle build/*.elf build/*.map build/*.bin build/*.raw build/*.dat \
	    build/*.car build/release build/egatrek-*.zip
