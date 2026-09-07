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

SMOKE_SRC = src/smoke.c src/amigagfx.c src/amigainput.c ../c128/src/layout.c
SMOKE_OUT = build/smoke

.PHONY: all smoke clean

all: smoke

smoke: $(SMOKE_OUT)

$(SMOKE_OUT): $(SMOKE_SRC) src/../../c128/src/vdc.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SMOKE_SRC)
	@ls -l $@

clean:
	rm -f build/smoke build/*.o
