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

# THE MEASUREMENT THE SCOPE ASKS FOR: link the whole game against stubbed
# seams and see what the GAME costs, before any driver is written. See
# src/stubs.c for why every stub writes through a volatile.
# THE VIDEO SEAM IS REAL NOW, and its stubs drop out with it -- see
# src/stubs.c. Every driver that lands moves one line from the stub column to
# the source column, so the budget converges on the truth rather than being
# corrected by hand at the end.
EARLY_DEFS = -DATARI_HAVE_VIDEO
EARLY_SRC = ../c128/src/main.c ../c128/src/ui.c ../c128/src/layout.c \
            ../c128/src/strpool.c \
            ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
            src/vbxevid.c src/stubs.c

.PHONY: all early smoke run-smoke clean

all: early

# EXPECTED TO FAIL AT THIS STAGE, and budget.py reports by how much rather
# than stopping. The number is the deliverable; a link that succeeds is a
# later milestone.
early:
	@python3 tools/budget.py

build/early.elf: $(EARLY_SRC) atari.ld
	@mkdir -p build
	$(ATARICC) $(CFLAGS) $(EARLY_DEFS) $(LDFLAGS) -Wl,-Map=build/early.map -o $@ $(EARLY_SRC)

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
