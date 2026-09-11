# EGA Trek for the Atari 800XL + VBXE.
#
# Fifth port and, on the numbers, the TIGHTEST. VBXE's MEMAC window is mapped
# INTO the 6502 address space -- vbxevid.c opens it at $2000, 8K -- so the
# program gets $4000..$BFFF = 32,768 bytes, about 4.8K LESS than the C128,
# which holds the whole game with 211 bytes spare. See NOTES.md, "SCOPE:
# Atari 800XL + VBXE".
#
# --start-addr 0x4000 IS NOT OPTIONAL. Without it the link loads at $2000,
# vbxe_init() maps the window over the program's own code, and execution falls
# into VRAM. Uno found that by crashing into it: PC=$25ED illegal=1.
LLVM_MOS ?= $(HOME)/llvm-mos
ATARICC   = $(LLVM_MOS)/bin/mos-atari8-dos-clang

# THE TWO OPT-IN OVERLAYS, and this is the only port that turns them on.
# Both page code on the HOT PATH -- the enemy turn runs on essentially every
# command and MOVE is the commonest thing a player types -- which is ruinous
# where a swap is a disk read and costs milliseconds where it is a copy out of
# VBXE's VRAM. Measured 2026-09-09: 3,910 and 1,578 bytes, which is what takes
# this target from 5,161 over to linking. See core/overlay.h.
OVL_OPT = -DTREK_OVL_ENEMY -DTREK_OVL_MOVE -DTREK_LOWMEM

CFLAGS  = -Wall -Werror -Oz -DTREK_COLOUR_IS_EGA -DTREK_OVERLAYS $(OVL_OPT) \
          -I src -I ../c128/src -I ../core
LDFLAGS = -T atari.ld -Wl,--no-check-sections

# THE WINDOW SIZE, READ OUT OF atari.ld RATHER THAN COPIED. The X16 keeps the
# same number in its Makefile and its linker script and relies on nobody
# changing one without the other; there is no reason to repeat that here when
# the linker script is the only file that can actually enforce it.
OVL_WINDOW = $(shell python3 -c "import re;print(int(re.search(r'__ovl_size\s*=\s*(0x[0-9a-f]+)',open('atari.ld').read()).group(1),16))")

# THE MEASUREMENT THE SCOPE ASKS FOR: link the whole game against stubbed
# seams and see what the GAME costs, before any driver is written. See
# src/stubs.c for why every stub writes through a volatile.
# THE VIDEO SEAM IS REAL NOW, and its stubs drop out with it -- see
# src/stubs.c. Every driver that lands moves one line from the stub column to
# the source column, so the budget converges on the truth rather than being
# corrected by hand at the end.
EARLY_DEFS = -DATARI_HAVE_VIDEO -DATARI_HAVE_INPUT -DATARI_HAVE_FARMEM -DATARI_HAVE_OVERLAY -DATARI_HAVE_SOUND -DATARI_HAVE_STORAGE
EARLY_SRC = ../c128/src/main.c ../c128/src/ui.c ../c128/src/layout.c \
            ../c128/src/strpool.c \
            ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
            src/vbxevid.c src/atariinput.c src/atarimem.c src/atariovl.c src/atarisnd.c src/ataristorage.c src/music_data.c src/stubs.c

.PHONY: all early smoke run-smoke keyprobe keytable keyecho run-keyecho memtest run-memtest sndprobe pokey sndtest run-sndtest storetest run-storetest nolto verify game data atr run clean

all: early

# EXPECTED TO FAIL AT THIS STAGE, and budget.py reports by how much rather
# than stopping. The number is the deliverable; a link that succeeds is a
# later milestone.
early:
	@python3 tools/budget.py

# GENERATED, not committed -- tools/make_music.py composes this port's own
# music. tools/gen_music.py extracts Anderson's note data instead and its
# output must never be committed or released; see the root .gitignore.
music src/music_data.c src/music_data.h:
	cd .. && python3 tools/make_music.py && cp c128/src/music_data.c c128/src/music_data.h atari/src/

build/early.elf: $(EARLY_SRC) src/music_data.h atari.ld
	@mkdir -p build
	$(ATARICC) $(CFLAGS) -DOVL_WINDOW=$(OVL_WINDOW) $(EARLY_DEFS) $(LDFLAGS) -Wl,-Map=build/early.map -o $@ $(EARLY_SRC)

clean:
	rm -f build/*.elf build/*.map

# FIRST LIGHT. The video seam and nothing else: sixteen colours, the authored
# box-drawing set, and row 24 -- because uno's VBXE driver is 24 rows and this
# console needs 25. Linked with atari.ld like the game, so the load address and
# the window are the same ones the real build uses.
SMOKE_SRC = src/smoke.c src/vbxevid.c
SMOKE_OUT = build/smoke.xex

smoke: $(SMOKE_OUT)

$(SMOKE_OUT): $(SMOKE_SRC) src/vbxevid.h atari.ld
	@mkdir -p build
	$(ATARICC) $(CFLAGS) -I ../c128/src $(LDFLAGS) -o $@ $(SMOKE_SRC)
	@python3 -c "import os; print('atari: smoke %d bytes' % os.path.getsize('$(SMOKE_OUT)'))"

# Drive AltirraSDL's headless bridge: boot the XEX, run frames, screenshot.
# The VBXE device itself comes from the user's Altirra settings (--settings=user)
# because the standalone server has no switch to add hardware devices.
run-smoke: $(SMOKE_OUT)
	python3 tools/run_atari.py $(SMOKE_OUT) build/smoke.png

# THE KEY TABLE, MEASURED. CH holds a matrix position, not a character, and
# the mapping is the kind of thing a header comment gets wrong -- see the X16.
# `make keytable` injects every key the game needs and prints what came back.
keyprobe: src/keyprobe.c
	@mkdir -p build
	$(ATARICC) -Wall -Werror -Oz $(LDFLAGS) -o build/keyprobe.xex src/keyprobe.c

keytable: keyprobe
	python3 tools/keytable.py

# THE INPUT SEAM, CHECKED THROUGH THE VIDEO ONE. Injects keys over the bridge
# and screenshots what came back -- the fold from ATASCII to input.h's ASCII
# is the part that can be wrong, so each key prints its decimal value.
keyecho: src/keyecho.c src/vbxevid.c src/atariinput.c src/vbxevid.h atari.ld
	@mkdir -p build
	$(ATARICC) $(CFLAGS) $(LDFLAGS) -o build/keyecho.xex \
	    src/keyecho.c src/vbxevid.c src/atariinput.c

run-keyecho: keyecho
	python3 tools/run_atari.py build/keyecho.xex build/keyecho.png 120 \
	    --keys T,R,E,K,1,9,COMMA,PERIOD,MINUS,SPACE,RETURN,ESCAPE,BACKSPACE

# FAR MEMORY AND THE OVERLAY-SIZED COPY, checked across bank boundaries -- the
# case the C128's flat 64K bank cannot have and this port's 4K window meets on
# every overlay swap.
memtest: src/memtest.c src/vbxevid.c src/atarimem.c src/vbxevid.h src/atarimem.h atari.ld
	@mkdir -p build
	$(ATARICC) $(CFLAGS) -DOVL_WINDOW=$(OVL_WINDOW) $(LDFLAGS) -o build/memtest.xex \
	    src/memtest.c src/vbxevid.c src/atarimem.c

run-memtest: memtest
	python3 tools/run_atari.py build/memtest.xex build/memtest.png 240

# WHAT DOES POKEY ACTUALLY DO? -- the divisor formula differs per mode and the
# frame clock is the thing every port here has got wrong once. Driven live from
# the harness: no rebuild per configuration.
sndprobe: src/sndprobe.c
	@mkdir -p build
	$(ATARICC) -Wall -Werror -Oz $(LDFLAGS) -o build/sndprobe.xex src/sndprobe.c

pokey: sndprobe
	python3 tools/pokey.py

# THE SOUND SEAM, checked in BOTH video standards -- the region decides the
# divide constant and the tempo numerator, and a test that only ever sees one
# region cannot catch the fault that made the MEGA65's music 19% fast.
sndtest: src/sndtest.c src/vbxevid.c src/atarisnd.c src/atarimem.c src/music_data.c
	@mkdir -p build
	$(ATARICC) $(CFLAGS) $(LDFLAGS) -o build/sndtest.xex \
	    src/sndtest.c src/vbxevid.c src/atarisnd.c src/atarimem.c src/music_data.c

run-sndtest: sndtest
	python3 tools/sndtest.py

# THE DISK SEAM. Needs a booted DOS for D: to exist at all, so the test runs as
# AUTORUN.SYS off a DOS 2.5 disk -- which is what a release would do anyway.
# tools/atr.py plants the files it reads and extracts the file it writes, so
# each half checks the other's work.
DOS_ATR ?= $(HOME)/Atari400mini/os/Dos 2.5.atr

storetest: src/storetest.c src/vbxevid.c src/ataristorage.c
	@mkdir -p build
	$(ATARICC) $(CFLAGS) $(LDFLAGS) -o build/storetest.xex \
	    src/storetest.c src/vbxevid.c src/ataristorage.c

run-storetest: storetest
	python3 tools/storetest.py "$(DOS_ATR)"

# OBJECTS WITHOUT LTO, built only so verify can read the call graph AS
# WRITTEN. LTO inlines a resident function into main(), at which point a call
# that only works with one particular overlay loaded is indistinguishable from
# the correct ones -- which is exactly how the hall-of-fame crash got out on
# three ports at once. See tools/overlay_check.py, rule 4.
# THESE THREE ARE DEFINED HERE, ABOVE THE RULES THAT NAME THEM, AND THAT IS
# NOT A STYLE CHOICE. They used to live with the game rule two hundred lines
# below -- and make expands a rule's prerequisites AS IT PARSES THE LINE, so
# `verify: $(GAME_OUT)` expanded to `verify:` and `nolto: $(GAME_SRC)` to
# `nolto:`. Neither depended on anything. It showed up on 2026-09-10 as a
# `make verify` that failed on a clean tree because nothing had built the game,
# and the quieter half is worse: nolto's objects are what `make verify` reads
# to check overlay rule 4, and they were never rebuilt when a source changed.
# Same bug as c128/Makefile's, found by tools/check_makefiles.py.
# ORDER MATTERS AND IS NOT ALPHABETICAL -- the loader indexes OVERLAYS.BIN by
# the OVL_* id, so this list must read in the same order as core/overlay.h.
# `enemy` and `move` are last because they are the two opt-in windows.
OVERLAYS = eval hof front info repair msgs planet cmds title events xtra \
           enemy move

GAME_SRC = ../c128/src/main.c ../c128/src/ui.c ../c128/src/layout.c \
           ../c128/src/strpool.c \
           ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
           src/vbxevid.c src/atariinput.c src/atarimem.c src/atariovl.c \
           src/atarisnd.c src/ataristorage.c src/music_data.c
GAME_OUT = build/trekatari.xex

NOLTO = build/nolto

nolto: $(GAME_SRC) src/music_data.h
	@mkdir -p $(NOLTO)
	@for f in $(GAME_SRC); do \
	    $(ATARICC) -c -fno-lto $(CFLAGS) -DOVL_WINDOW=$(OVL_WINDOW) \
	        -o $(NOLTO)/`basename $$f .c`.o $$f || exit 1; \
	done

verify: $(GAME_OUT) nolto
	python3 tools/verify_xex.py

# ---------------------------------------------------------------------------
# THE GAME. One link produces the resident XEX and every overlay image, which
# is not tidiness: an overlay is linked WITH the resident half, so the two are
# only valid together. See src/atariovl.c on the build stamp that enforces it.
#
game: $(GAME_OUT)

$(GAME_OUT): $(GAME_SRC) src/vbxevid.h src/atarimem.h src/music_data.h atari.ld
	@mkdir -p build
	$(ATARICC) $(CFLAGS) -DOVL_WINDOW=$(OVL_WINDOW) $(LDFLAGS) \
	    -Wl,-Map=build/trekatari.map -o $(GAME_OUT) $(GAME_SRC)
	@python3 tools/cut_overlays.py $(GAME_OUT).elf build/OVERLAYS.BIN \
	    $(OVL_WINDOW) $(OVERLAYS)
	@python3 -c "import os; print('atari: XEX %d bytes' % os.path.getsize('$(GAME_OUT)'))"

OBJCOPY = $(LLVM_MOS)/bin/llvm-objcopy
NM      = $(LLVM_MOS)/bin/llvm-nm

# THE DATA FILES. Same set as every other port assembles.
DATA = build/data

data: $(GAME_OUT)
	@mkdir -p $(DATA)
	@cd .. && python3 tools/gen_strings.py >/dev/null && python3 tools/make_music.py >/dev/null
	@# THE POOL IS SHARED; THE THREE STRINGS THAT NAME THE MACHINE ARE NOT.
	@# Same ids, same count, same strdata.h -- see src/strings.override.txt.
	@cd .. && python3 tools/gen_strings.py --override atari/src/strings.override.txt \
	    atari/build/strings.dat
	@cp build/strings.dat        $(DATA)/STRINGS.DAT
	@cp ../c128/build/music.dat  $(DATA)/MUSIC.DAT
	@cp ../c128/src/briefing.txt $(DATA)/BRIEF.TXT
	@cp build/OVERLAYS.BIN       $(DATA)/OVERLAYS.BIN
	@ls -l $(DATA) | grep -v total

# THE DISK. A DOS 2.5 image with the game as AUTORUN.SYS, so booting it runs
# the game -- which is what a player does and what the test harness does.
# DOS.SYS stays; the utilities go, because the game wants the room.
GAME_ATR = build/egatrek.atr

atr: data
	@cp "$(DOS_ATR)" $(GAME_ATR)
	@for f in DUP.SYS RAMDISK.COM SETUP.COM COPY32.COM DISKFIX.COM; do \
	    python3 tools/atr.py delete $(GAME_ATR) $$f >/dev/null 2>&1 || true; \
	done
	@python3 tools/atr.py add $(GAME_ATR) AUTORUN.SYS $(GAME_OUT)
	@for f in OVERLAYS.BIN STRINGS.DAT MUSIC.DAT BRIEF.TXT; do \
	    python3 tools/atr.py add $(GAME_ATR) $$f $(DATA)/$$f || exit 1; \
	done
	@python3 tools/atr.py list $(GAME_ATR) | tail -8

run: atr
	python3 tools/run_atari.py --disk $(GAME_ATR) build/run.png 900

# IS LOW RAM FREE WHILE DOS IS RESIDENT? A memory map is not evidence about a
# running machine, and the buffer this port wants to put down there is 626
# bytes of the tightest budget in the project.
lowprobe: src/lowprobe.c src/vbxevid.c src/ataristorage.c
	@mkdir -p build
	$(ATARICC) $(CFLAGS) $(LDFLAGS) -o build/lowprobe.xex \
	    src/lowprobe.c src/vbxevid.c src/ataristorage.c

run-lowprobe: lowprobe atr
	@cp build/egatrek.atr build/lowprobe.atr
	@python3 tools/atr.py delete build/lowprobe.atr AUTORUN.SYS >/dev/null
	@python3 tools/atr.py add build/lowprobe.atr AUTORUN.SYS build/lowprobe.xex
	python3 tools/run_atari.py --disk build/lowprobe.atr build/lowprobe.png 1200
