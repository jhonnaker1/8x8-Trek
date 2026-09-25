# The MSX2 port, as far as it goes: an MSX-DOS .COM for a V9938 machine,
# with a video driver (msx2vid.c) and a PSG sound driver (msx2snd.c).
#
# NOT A PORT YET and not in RELEASE_PORTS or `make ports` -- this directory
# is the SCOPE's working half (NOTES.md, "SCOPE: an MSX2 port"). Tooling
# outside the repository lives in ~/msx-toolchain (Nextor boot files, the
# TPA tool); the compiler is Homebrew's SDCC.
#
#   make hello   build/HELLO.COM -- the video driver's first-light test
#   make disk    build/disk/     -- boot files + HELLO.COM + AUTOEXEC.BAT
#   make sound   build/SNDTEST.COM -- the PSG driver's first-sound test, on
#                its own disk (build/sdisk/)
#   make listen  boot that disk, record what it plays, and check PITCH and
#                TEMPO against MUSIC.DAT (tools/listen.py)
#   make shell   prove a program installed AS COMMAND2.COM owns the whole TPA
#                (src/shelltest.c) -- under COMMAND2 it gets 1,280 bytes less
#   make stackbound  a STATIC bound on the game's stack (tools/stackdepth.py)
#   make stack   what the BIOS, its interrupt handler and BDOS add to it,
#                measured by sentinel in STKTEST.COM
#   make probe   link the WHOLE GAME with the real drivers and stubs for the
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
# THAT IS THE SHELL'S TPA: a program COMMAND2 runs gets $0100..$D605, 1,280
# less (measured 2026-09-25), so the game has to be installed as the shell.
# `make shell` proves that route; `make probe` counts against it.
TPA_TOP = 0xDB06

SHARED = ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
         ../c128/src/ui.c ../c128/src/main.c ../c128/src/layout.c \
         ../c128/src/strpool.c ../c128/src/music_data.c
SHARED_REL = $(addprefix build/,$(notdir $(SHARED:.c=.rel)))

.PHONY: all hello disk sound listen shell stack stackbound probe shot clean
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

# MUSIC.DAT is this port's own composition, from tools/make_music.py -- never
# the note data tools/gen_music.py extracts from EGATREK.EXE.
../c128/build/music.dat:
	cd .. && python3 tools/make_music.py >/dev/null

# The test links the file in as an array; the game will read it from far
# memory. build/musicdat.c is a TEST fixture and nothing ships it.
build/musicdat.c: ../c128/build/music.dat | build
	python3 -c 'import sys; b=open(sys.argv[1],"rb").read(); print("const unsigned char music_dat[%d] = {%s};" % (len(b), ",".join(map(str,b))))' $< > $@
build/musicdat.rel: build/musicdat.c
	$(SDCC) $(CFLAGS) -c $< -o $@

SND_REL = build/crt0.rel build/msx2snd.rel build/music_data.rel build/musicdat.rel build/sndtest.rel

build/SNDTEST.COM: $(SND_REL) Makefile
	$(SDCC) $(LDFLAGS) $(SND_REL) -o build/sndtest.ihx
	$(MAKEBIN) -s 65536 -o 256 -p build/sndtest.ihx $@
	@echo "$@: $$(wc -c < $@) bytes"

sound: build/SNDTEST.COM
	rm -rf build/sdisk && mkdir -p build/sdisk
	cp $(TOOLCHAIN)/nextor-boot/* build/sdisk/
	cp build/SNDTEST.COM build/sdisk/
	printf 'SNDTEST\r\n' > build/sdisk/AUTOEXEC.BAT

STK_REL = build/crt0.rel build/msxbios.rel build/msx2vid.rel build/stktest.rel

build/STKTEST.COM: $(STK_REL) Makefile
	$(SDCC) $(LDFLAGS) $(STK_REL) -o build/stktest.ihx
	$(MAKEBIN) -s 65536 -o 256 -p build/stktest.ihx $@

build/SHELLTEST.COM: build/crt0.rel build/shelltest.rel Makefile
	$(SDCC) $(LDFLAGS) build/crt0.rel build/shelltest.rel -o build/shelltest.ihx
	$(MAKEBIN) -s 65536 -o 256 -p build/shelltest.ihx $@

shell: build/SHELLTEST.COM
	rm -rf build/shdisk && mkdir -p build/shdisk
	cp $(TOOLCHAIN)/nextor-boot/MSXDOS2.SYS build/shdisk/
	cp build/SHELLTEST.COM build/shdisk/COMMAND2.COM
	$(call openmsx_run,build/shdisk,tools/shelltest.tcl,build/shelltest.txt)
	@grep -v '^ *$$' build/shelltest.txt; grep -q 'SHELL OK' build/shelltest.txt

# THE STACK, in two halves. `stackbound` is static: every frame SDCC emitted
# and the call graph from main(), over the probe link's own .asm. `stack` is
# dynamic: what the BIOS, its interrupt handler and BDOS push onto OUR stack,
# which no reading of our code can see. The budget needs the sum.
PROBE_ASM = src/msxbios.s build/msx2vid.asm $(SHARED_REL:.rel=.asm) \
            build/msx2snd.asm build/stubs.asm
stackbound: build/probe.ihx
	@python3 tools/stackdepth.py _main $(PROBE_ASM)

# AS THE SHELL, because that is how the game has to run (src/shelltest.c):
# under COMMAND2 the TPA is 54,534 bytes, not 55,814.
stack: build/STKTEST.COM
	rm -rf build/kdisk && mkdir -p build/kdisk
	cp $(TOOLCHAIN)/nextor-boot/MSXDOS2.SYS build/kdisk/
	cp build/STKTEST.COM build/kdisk/COMMAND2.COM
	$(call openmsx_run,build/kdisk,tools/stktest.tcl,build/stktest.txt)
	@cat build/stktest.txt

# OVER THE CONTROL CHANNEL, NOT -script. On 2026-09-24 a plain `-script` run
# stopped emulating (1% CPU, the script's own realtime timer never fired)
# while the same machine under `-control stdio` + `set power on` ran
# normally. The control channel starts the machine POWERED OFF; the script
# powers it on after arming itself.
# $(1) disk dir, $(2) Tcl script, $(3) the file the script writes when done.
define openmsx_run
	@T=$$(mktemp -d); \
	 cp $(HOME)/.openMSX/share/settings.xml $$T/settings.xml 2>/dev/null || printf "<!DOCTYPE settings SYSTEM 'settings.dtd'>\n<settings/>\n" > $$T/settings.xml; \
	 rm -f $(3); \
	 ( printf '<openmsx-control>\n'; sleep 2; \
	   printf '<command>source $(CURDIR)/$(2)</command>\n'; \
	   printf '<command>set power on</command>\n'; \
	   for i in $$(seq 240); do [ -f $(3) ] && break; sleep 1; done ) | \
	 OUTDIR=$(CURDIR)/build /Applications/openMSX.app/Contents/MacOS/openmsx -setting $$T/settings.xml \
	   -machine $(MACHINE) $(EXT) -ext SunriseIDE_Nextor -diska $(CURDIR)/$(1) -control stdio >/dev/null 2>&1; \
	 rm -rf $$T
endef

# MACHINE=Sony_HB-F1XD EXT='-ext ram512k' reaches the NTSC branch: a 60Hz
# Japanese MSX2, which has 64K and no mapper, so Nextor needs the cartridge.
MACHINE ?= Philips_NMS_8250
EXT ?=
listen: sound ../c128/build/music.dat
	$(call openmsx_run,build/sdisk,tools/listen.tcl,build/listen.txt)
	python3 tools/listen.py build/listen.wav ../c128/build/music.dat build/listen.txt

build/probe.ihx: $(BASE_REL) $(SHARED_REL) build/msx2snd.rel build/stubs.rel Makefile
	$(SDCC) $(LDFLAGS) $(BASE_REL) $(SHARED_REL) build/msx2snd.rel build/stubs.rel -o $@

probe: build/probe.ihx
	@python3 tools/probe.py build/probe.map build/stubs.rel $(TPA_TOP)

clean:
	rm -rf build

# A throwaway settings file: openMSX SAVES SETTINGS ON EXIT, and a headless
# run against the real one changes the next interactive launch.
shot: disk
	@T=$$(mktemp -d); \
	 cp $(HOME)/.openMSX/share/settings.xml $$T/settings.xml 2>/dev/null || printf "<!DOCTYPE settings SYSTEM 'settings.dtd'>\n<settings/>\n" > $$T/settings.xml; \
	 OUTDIR=$(CURDIR)/build /Applications/openMSX.app/Contents/MacOS/openmsx -setting $$T/settings.xml \
	   -machine Philips_NMS_8250 -ext SunriseIDE_Nextor -diska $(CURDIR)/build/disk \
	   -script $(CURDIR)/tools/shot.tcl >/dev/null 2>&1; \
	 rm -rf $$T; cat build/shot.txt; echo "build/shot.png"
