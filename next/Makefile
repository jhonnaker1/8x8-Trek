# EGA Trek for the NeXT: a NeXTSTEP 3.3 application, EGATrek.app.
#
# THIS IS THE HOST'S HALF. The program is compiled ON A NeXT (or in an
# emulator) by NeXTSTEP's own cc -- no compiler here makes 68k Mach-O for
# NeXTSTEP, the same finding as commodore-uno/next -- so this Makefile does
# what needs the host: it builds the three data files (they need Python),
# and copies them, this folder and the shared sources Makefile.next names
# onto the NeXT's shared folder. Then, on the NeXT:
#
#     cd /Net/nfs/egatrek/next
#     make -f Makefile.next
#
# In Previous or BMNeXT the host's shared folder is /Net/nfs on the NeXT;
# NEXT_CARD is that folder here (BMNeXT's simulated SD card).
#
#     make verify     the gate `make ports` runs: tools/verify_next.py
#     make data       build/STRINGS.DAT, build/MUSIC.DAT, build/BRIEF.TXT
#     make deploy     the sources and the data, onto $(NEXT_CARD)/egatrek
#     make release    build/egatrek-next.zip, packed from the EGATrek.app the
#                     NeXT built -- refused unless it is built from this tree
NEXT_CARD ?= /Volumes/Crucial Win/EGATrek/next/card

DATA = build/STRINGS.DAT build/MUSIC.DAT build/BRIEF.TXT

.PHONY: all verify data music deploy icon release clean

all: data

# WHAT THE HOST CAN CHECK about a program it cannot compile: the glyph sweep,
# C89 over every C file the NeXT compiles, the deploy list against what
# Makefile.next and the sources reach, the <stdint.h> stand-in, the icon.
verify: data
	python3 tools/verify_next.py

# ---------------------------------------------------------------- the data
# The same three files every port ships, built the same way; only the
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
	cd .. && python3 tools/gen_strings.py --override next/src/strings.override.txt \
	    next/build/STRINGS.DAT
	@ls -l $@

# THE MUSIC IS THIS PORT'S OWN COMPOSITION, from tools/make_music.py -- never
# the note data tools/gen_music.py extracts from EGATREK.EXE. The track
# table is copied here like every port's (gitignored).
music src/music_data.c src/music_data.h:
	cd .. && python3 tools/make_music.py >/dev/null && \
	    cp c128/src/music_data.c c128/src/music_data.h next/src/

# THE ICON, redrawn only on request: the TIFF is committed, because the NeXT
# that links it has no Python to make it.
icon:
	python3 tools/mkicon.py EGATrek.tiff

# ---------------------------------------------------------------- to the NeXT
# The tree Makefile.next expects, as it is in the repository: next/ beside
# c128/src, core and amiga/src, so its ../ paths hold. Only what it builds
# from goes over. NeXTSTEP's make knows nothing of the host's, so its own
# objects are cleared rather than trusted across a copy.
SHARED = c128/src/main.c c128/src/ui.c c128/src/layout.c c128/src/strpool.c \
         c128/src/vdc.h c128/src/sid.h c128/src/input.h c128/src/ui.h \
         c128/src/layout.h c128/src/layout40.h c128/src/strdata.h \
         c128/src/egavdc.h c128/src/music_data.h \
         core/trek.c core/planet.c core/hof.c core/serial.c \
         core/trek.h core/planet.h core/hof.h core/serial.h core/storage.h \
         core/farmem.h core/overlay.h core/ega.h core/strpool.h core/lowmem.h \
         amiga/src/amigafarmem.c amiga/src/amigaovl.c \
         mac/src/midinote.c mac/src/midinote.h

deploy: $(DATA) src/music_data.c
	@test -d "$(NEXT_CARD)" || { echo "no NeXT card folder at $(NEXT_CARD)"; exit 1; }
	@mkdir -p "$(NEXT_CARD)/egatrek/next/data"
	cd .. && rsync -R $(SHARED) "$(NEXT_CARD)/egatrek/"
	rsync -r --delete Makefile.next EGATrek.iconheader EGATrek.tiff src \
	    "$(NEXT_CARD)/egatrek/next/"
	cp $(DATA) "$(NEXT_CARD)/egatrek/next/data/"
	rm -rf "$(NEXT_CARD)/egatrek/next/obj"
	@echo "deploy: on the NeXT, cd /Net/nfs/egatrek/next; make -f Makefile.next"

# THE RELEASE ASSET: one zip holding egatrek-next.tar (EGATrek.app, as
# NeXTSTEP's tar reads it) and README.txt. A zip because a README must travel
# beside it, as every port's does; a tar inside because the NeXT has tar and
# a zip would lose the program's execute bit. The program is the one the NeXT
# built, from the shared folder, and tools/checkbuild.py refuses it unless it
# was built from exactly this tree. THE RULE CLEARS ITS OWN build/egatrek-*.zip
# FIRST: the root Makefile lists artefacts with a glob.
REL = build/release

release: verify
	python3 tools/checkbuild.py "$(NEXT_CARD)"
	@rm -rf $(REL) build/egatrek-*.zip && mkdir -p $(REL)/egatrek-next
	python3 tools/mktar.py "$(NEXT_CARD)/egatrek/next/EGATrek.app" \
	    $(REL)/egatrek-next/egatrek-next.tar
	@cp README-release.txt $(REL)/egatrek-next/README.txt
	@cd $(REL) && zip -q -X -r ../egatrek-next.zip egatrek-next
	@echo "release: build/egatrek-next.zip"
	@unzip -l build/egatrek-next.zip | tail -n +4

clean:
	rm -rf build
