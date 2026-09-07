# EGA Trek for the Amiga (OCS/ECS, KS2.0+).
#
# Fourth port, and the FIRST WITH NO OVERLAY MACHINERY. `core/farmem.h` has
# said "Amiga -- no banking needed, a plain array" since the seam was designed,
# and this target has more memory than the game needs -- so there is no window,
# no staging region, no build stamp, no ovl_load and none of the budget
# arithmetic that dominates the three 8-bit ports. See NOTES.md, "SCOPE: the
# Amiga".
#
# bebbo's m68k-amigaos-gcc. The root `make port-check` has compiled the core
# for 68000 on every build since before any port existed, so the shared half
# was known to be portable here long before this directory appeared.
AMIGA_TOOLCHAIN ?= $(HOME)/amiga-toolchain
CC = $(AMIGA_TOOLCHAIN)/bin/m68k-amigaos-gcc

# -DTREK_COLOUR_IS_EGA makes EGA_TO_VDC the identity: the screen's palette
# registers are loaded with EGA's own values in EGA's own order, so a colour
# name IS its index. Same trick as the MEGA65 and the X16.
CFLAGS = -O2 -Wall -Wextra -Werror -noixemul \
         -DTREK_COLOUR_IS_EGA -I src -I ../c128/src -I ../core

# Amiberry mounts a HOST DIRECTORY as an Amiga volume, so there is no ADF to
# build and nothing to copy: point it at build/ and the binaries are there.
AMIBERRY ?= $(HOME)/Amiberry/Amiberry

SMOKE_SRC = src/smoke.c src/amigagfx.c src/amigainput.c src/amigastorage.c \
            src/amigafarmem.c ../c128/src/strpool.c ../c128/src/layout.c
SMOKE_OUT = build/smoke

# THE GAME. No -DTREK_OVERLAYS: core/overlay.h turns OVL_CODE into nothing and
# src/amigaovl.c supplies the empty ovl_load it asks for, so every function the
# 8-bit ports page in is simply resident here.
GAME_SRC = ../c128/src/main.c ../c128/src/ui.c ../c128/src/layout.c \
           ../c128/src/strpool.c \
           ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
           src/amigagfx.c src/amigainput.c src/amigastorage.c \
           src/amigafarmem.c src/amigasnd.c src/amigaovl.c
GAME_OUT = build/egatrek

STORE_SRC = src/storetest.c src/amigagfx.c src/amigainput.c src/amigastorage.c
STORE_OUT = build/storetest

# THE POOL'S DATA. gen_strings.py owns the numbering, so every port builds the
# same strdata.h and the same STR_COUNT; only the two strings that name the
# machine differ, and those live in src/strings.override.txt.
data: build/STRINGS.DAT build/BRIEF.TXT

# The briefing, STREAMED through plat_open/plat_read a page at a time. It is
# copied rather than generated: it is the port's own prose, committed, and
# shared with every other target. See NOTES.md on why none of Anderson's is
# in it.
build/BRIEF.TXT: ../c128/src/briefing.txt
	@mkdir -p build
	cp $< $@

build/STRINGS.DAT: ../c128/src/strings.txt src/strings.override.txt
	@mkdir -p build
	cd .. && python3 tools/gen_strings.py >/dev/null
	cd .. && python3 tools/gen_strings.py --override amiga/src/strings.override.txt \
	    amiga/build/STRINGS.DAT
	@ls -l $@

.PHONY: all game smoke storetest data clean

all: game data

game: $(GAME_OUT)

$(GAME_OUT): $(GAME_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(GAME_SRC)
	@ls -l $@

smoke: $(SMOKE_OUT)

storetest: $(STORE_OUT)

$(STORE_OUT): $(STORE_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(STORE_SRC)
	@ls -l $@

$(SMOKE_OUT): $(SMOKE_SRC) src/../../c128/src/vdc.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SMOKE_SRC)
	@ls -l $@

clean:
	rm -f build/smoke build/storetest build/*.o
