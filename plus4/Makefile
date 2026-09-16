# EGA Trek for the Commodore Plus/4.
#
# THE TENTH-AND-A-HALF PORT: the survey costed this one as "video, a new sound
# driver, a link script", and that has held. What it did NOT cost, because the
# survey said "cc65 rather than llvm-mos", is a whole platform package --
# llvm-mos has no `plus4` target, so plus4.ld, the BASIC header and the KERNAL
# jump table are this port's own. The COMPILER was never the problem; see
# README.md.
#
# mos-commodore-clang, not mos-plus4-clang, because the latter does not exist.
# The generic Commodore layer supplies crt0, imag-regs.ld and commodore.ld,
# and commodore.ld has zero C64-specific references in it.
CC       = $(HOME)/llvm-mos/bin/mos-commodore-clang

# -DTREK_40COL picks layout40.c's 40x25 console, the same one the C64 draws.
# -DTREK_OVERLAYS cuts the eleven-way split; without it the game links
# all-resident and overflows by 16,497, which is the staging measurement and
# is kept reproducible as `make early`.
CFLAGS   = -Oz -DTREK_40COL -I../c128/src -T plus4.ld
LDFLAGS  = -Wl,--no-check-sections

SHARED   = ../c128/src/main.c ../c128/src/layout40.c ../c128/src/ui.c \
           ../c128/src/input.c ../c128/src/music_data.c ../c128/src/storage.c \
           ../c128/src/overlay.c ../c128/src/strpool.c \
           ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c
# THIS PORT'S OWN. Four files where the C64 wrote two, and the survey said so:
# TED is neither a VIC-II nor a SID, so vic.c and sid.c cannot be linked here
# the way the C64 links them.
PLATFORM = src/ted.c src/p4log.c src/p4mem.c src/tedsnd.c src/basichdr.c
SRC      = $(SHARED) $(PLATFORM)

OUT = build/trek4.prg
MAP = build/trek4.map

.PHONY: all verify early frametest clean
all: $(OUT)

$(OUT): $(SRC) plus4.ld Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) -DTREK_OVERLAYS $(LDFLAGS) -Wl,-Map=$(MAP) -o $@ $(SRC)
	@ls -l $@

# Everything checkable without a machine. A port without one of these makes
# `make ports` true and meaningless.
verify: $(OUT)
	@python3 tools/verify_p4.py

# The staging measurement, kept reproducible: link RESIDENT and read the
# overflow. It is EXPECTED TO FAIL and the number is the point.
early:
	@mkdir -p build
	-@$(CC) $(CFLAGS) $(LDFLAGS) -o build/early.prg $(SRC) 2>&1 | grep overflow

# The console frame through the real layout40.c, for looking at.
frametest:
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -o build/frametest.prg \
	    src/frametest.c src/ted.c ../c128/src/layout40.c src/basichdr.c
	@python3 tools/screen_p4.py build/frametest.prg

clean:
	rm -f build/*.prg build/*.map
