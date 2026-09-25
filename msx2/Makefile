# The MSX2 port, as far as it goes: an MSX-DOS .COM for a V9938 machine.
#
# NOT A PORT YET and not in RELEASE_PORTS or `make ports` -- this directory
# is the SCOPE's working half (NOTES.md, "SCOPE: an MSX2 port"). Tooling
# outside the repository lives in ~/msx-toolchain (Nextor boot files, the
# TPA tool); the compiler is Homebrew's SDCC.
#
#   make hello   build/HELLO.COM -- the video driver's first-light test
#   make disk    build/disk/     -- boot files + HELLO.COM + AUTOEXEC.BAT
#   make probe   link the WHOLE GAME with the real driver and stubs for the
#                rest, and report what is left of the MSX-DOS TPA
#
# FLAGS ARE A MAKE VARIABLE, NOT A SHELL ONE: zsh does not word-split an
# unquoted $FLAGS, and that silently dropped half a measurement once.

SDCC     ?= sdcc
SDASZ80  ?= sdasz80
MAKEBIN  ?= makebin
TOOLCHAIN ?= $(HOME)/msx-toolchain

# --nolospre is NOT optional: core/trek.c hits SDCC 4.6's "Unbalanced stack"
# internal error under every other size-optimised flag set tried.
CFLAGS  = -mz80 --std-c99 --opt-code-size --nolospre -DTREK_COLOUR_IS_EGA \
          -Isrc -I../c128/src -I../core
LDFLAGS = -mz80 --no-std-crt0 --code-loc 0x0103 --data-loc 0

# MSX-DOS TPA on a 128K NMS 8250 under Nextor, MEASURED 2026-09-24 with
# ~/msx-toolchain/tools/tpa.sh: $0100..$DB05. Re-measure, do not trust.
TPA_TOP = 0xDB06

SHARED = ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
         ../c128/src/ui.c ../c128/src/main.c ../c128/src/layout.c \
         ../c128/src/strpool.c ../c128/src/music_data.c
SHARED_REL = $(addprefix build/,$(notdir $(SHARED:.c=.rel)))

.PHONY: all hello disk probe shot clean
all: hello

build:
	mkdir -p build

build/%.rel: src/%.c src/*.h Makefile | build
	$(SDCC) $(CFLAGS) -c $< -o $@
build/%.rel: src/%.c Makefile | build
	$(SDCC) $(CFLAGS) -c $< -o $@
build/%.rel: src/%.s Makefile | build
	$(SDASZ80) -o $@ $<
build/%.rel: ../core/%.c Makefile | build
	$(SDCC) $(CFLAGS) -c $< -o $@
build/%.rel: ../c128/src/%.c Makefile | build
	$(SDCC) $(CFLAGS) -c $< -o $@

# crt0 FIRST: it is what puts the `jp init` at $0100.
BASE_REL = build/crt0.rel build/msxbios.rel build/msx2vid.rel

build/HELLO.COM: $(BASE_REL) build/hello.rel Makefile
	$(SDCC) $(LDFLAGS) $(BASE_REL) build/hello.rel -o build/hello.ihx
	$(MAKEBIN) -s 65536 -o 256 -p build/hello.ihx $@
	@echo "$@: $$(wc -c < $@) bytes"
hello: build/HELLO.COM

disk: build/HELLO.COM
	rm -rf build/disk && mkdir -p build/disk
	cp $(TOOLCHAIN)/nextor-boot/* build/disk/
	cp build/HELLO.COM build/disk/
	printf 'HELLO\r\n' > build/disk/AUTOEXEC.BAT
	@ls build/disk

build/probe.ihx: $(BASE_REL) $(SHARED_REL) build/stubs.rel Makefile
	$(SDCC) $(LDFLAGS) $(BASE_REL) $(SHARED_REL) build/stubs.rel -o $@

probe: build/probe.ihx
	@python3 tools/probe.py build/probe.map build/stubs.rel $(TPA_TOP)

clean:
	rm -rf build

# A throwaway settings file: openMSX SAVES SETTINGS ON EXIT, and a headless
# run against the real one changes the next interactive launch.
shot: disk
	@T=$$(mktemp -d); \
	 cp $(HOME)/.openMSX/share/settings.xml $$T/settings.xml 2>/dev/null || printf '<settings>\n</settings>\n' > $$T/settings.xml; \
	 OUTDIR=$(CURDIR)/build /Applications/openMSX.app/Contents/MacOS/openmsx -setting $$T/settings.xml \
	   -machine Philips_NMS_8250 -ext SunriseIDE_Nextor -diska $(CURDIR)/build/disk \
	   -script $(CURDIR)/tools/shot.tcl >/dev/null 2>&1; \
	 rm -rf $$T; cat build/shot.txt; echo "build/shot.png"
