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

CFLAGS  = -Wall -Werror -Oz -DTREK_COLOUR_IS_EGA -DTREK_OVERLAYS \
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
EARLY_DEFS = -DATARI_HAVE_VIDEO -DATARI_HAVE_INPUT -DATARI_HAVE_FARMEM -DATARI_HAVE_OVERLAY -DATARI_HAVE_SOUND
EARLY_SRC = ../c128/src/main.c ../c128/src/ui.c ../c128/src/layout.c \
            ../c128/src/strpool.c \
            ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
            src/vbxevid.c src/atariinput.c src/atarimem.c src/atariovl.c src/atarisnd.c src/music_data.c src/stubs.c

.PHONY: all early smoke run-smoke keyprobe keytable keyecho run-keyecho memtest run-memtest sndprobe pokey sndtest run-sndtest clean

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
