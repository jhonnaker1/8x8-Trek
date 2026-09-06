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

SMOKE_SRC = src/smoke.c src/x16vera.c ../c128/src/layout.c
SMOKE_OUT = build/smoke.prg

.PHONY: all smoke charset run-charset run-smoke clean

all: smoke

smoke: $(SMOKE_OUT)

$(SMOKE_OUT): $(SMOKE_SRC) src/x16vera.h
	@mkdir -p build
	$(X16CC) $(CFLAGS) -o $@ $(SMOKE_SRC)
	@python3 -c "import os; print('x16: smoke %d bytes' % os.path.getsize('$@'))"

CHARSET_SRC = src/charset.c src/x16vera.c ../c128/src/layout.c
CHARSET_OUT = build/charset.prg

charset: $(CHARSET_OUT)

$(CHARSET_OUT): $(CHARSET_SRC) src/x16vera.h
	@mkdir -p build
	$(X16CC) $(CFLAGS) -o $@ $(CHARSET_SRC)

run-charset: $(CHARSET_OUT)
	cd $(X16EMU) && ./x16emu -prg $(CURDIR)/$(CHARSET_OUT) -run

run-smoke: $(SMOKE_OUT)
	cd $(X16EMU) && ./x16emu -prg $(CURDIR)/$(SMOKE_OUT) -run

clean:
	rm -rf build
