# The MSX2 port, as far as it goes: an MSX-DOS .COM for a V9938 machine,
# with a video driver (msx2vid.c) and a PSG sound driver (msx2snd.c).
#
# NOT A PORT YET and not in RELEASE_PORTS or `make ports` -- this directory
# is the SCOPE's working half (NOTES.md, "SCOPE: an MSX2 port"). Tooling
# outside the repository lives in ~/msx-toolchain (Nextor boot files, the
# TPA tool); the compiler is Homebrew's SDCC.
#
#   make game    build/gdisk/ -- THE GAME: MSXDOS2.SYS, the game installed as
#                COMMAND2.COM, and its data files
#   make dsk     build/egatrek-msx2.dsk -- THE RELEASE DISK IMAGE
#   make memcheck  the memory check REFUSING, run from a COMMAND2 prompt
#   make hello   build/HELLO.COM -- the video driver's first-light test
#   make disk    build/disk/     -- boot files + HELLO.COM + AUTOEXEC.BAT
#   make sound   build/SNDTEST.COM -- the PSG driver's first-sound test, on
#                its own disk (build/sdisk/)
#   make listen  boot that disk, record what it plays, and check PITCH and
#                TEMPO against MUSIC.DAT (tools/listen.py)
#   make gameshot  photograph the running game from VRAM (SHOT_TIMES, SHOT_KEYS)
#   make profile   where the time goes: PC samples mapped to functions
#   make stackrun  the RUNNING game's stack, by sentinel over a full session
#   make dostime what one DOS2 call costs -- why storage reads ahead
#   make kbd     the keyboard driver, typed at through the emulated matrix
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

.PHONY: all game dsk dskshot memcheck gameshot profile stackrun dostime hello disk sound listen kbd shell stack stackbound probe shot clean
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

KB_REL = build/crt0.rel build/msxbios.rel build/msx2vid.rel build/msx2input.rel build/kbtest.rel

build/KBTEST.COM: $(KB_REL) Makefile
	$(SDCC) $(LDFLAGS) $(KB_REL) -o build/kbtest.ihx
	$(MAKEBIN) -s 65536 -o 256 -p build/kbtest.ihx $@

# Typed through openMSX's emulated key matrix, so the BIOS handler queues the
# keys and the driver reads its ring -- the real path, nothing poked. The
# expected line is the input.h values of: (x discarded) a Z 1 , . q SPACE -
# RETURN ESC BS UP DOWN.
KB_WANT = KEYS 65 90 49 44 46 81 32 45 13 27 20 1 2
kbd: build/KBTEST.COM
	rm -rf build/kbdisk && mkdir -p build/kbdisk
	cp $(TOOLCHAIN)/nextor-boot/MSXDOS2.SYS build/kbdisk/
	cp build/KBTEST.COM build/kbdisk/COMMAND2.COM
	$(call openmsx_run,build/kbdisk,tools/kbtest.tcl,build/kbtest.txt,KB_GOT=0x$$(awk '$$2=="_got"{print $$1}' build/kbtest.map) KB_ENT=0x$$(awk '$$2=="_kb_entropy"{print $$1}' build/kbtest.map))
	@grep -v '^ *$$' build/kbtest.txt
	@# The screen is 40 columns and the line WRAPS -- join it before matching.
	@tr '\n' ' ' < build/kbtest.txt | tr -s ' ' | grep -q '$(KB_WANT) ' && echo "kbd: PASS" || { echo "kbd: FAIL, want $(KB_WANT)"; false; }

# What one DOS2 call costs: BRIEF.TXT read at 64 bytes a call and at one.
# The reason msx2storage.c has a read-ahead (~5ms a call, measured).
build/DOSTIME.COM: build/crt0.rel build/dostime.rel Makefile
	$(SDCC) $(LDFLAGS) build/crt0.rel build/dostime.rel -o build/dostime.ihx
	$(MAKEBIN) -s 65536 -o 256 -p build/dostime.ihx $@

dostime: build/DOSTIME.COM
	rm -rf build/tdisk && mkdir -p build/tdisk
	cp $(TOOLCHAIN)/nextor-boot/MSXDOS2.SYS build/tdisk/
	cp build/DOSTIME.COM build/tdisk/COMMAND2.COM
	cp ../c128/src/briefing.txt build/tdisk/BRIEF.TXT
	$(call openmsx_run,build/tdisk,tools/dostime.tcl,build/dostime.txt)
	@grep -v '^ *$$' build/dostime.txt

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
            build/msx2snd.asm build/msx2input.asm build/msx2storage.asm \
            build/msx2farmem.asm build/stubs.asm
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
# $(1) disk dir, $(2) Tcl script, $(3) the file the script writes when done,
# $(4) optional environment for the script.
define openmsx_run
	@T=$$(mktemp -d); \
	 cp $(HOME)/.openMSX/share/settings.xml $$T/settings.xml 2>/dev/null || printf "<!DOCTYPE settings SYSTEM 'settings.dtd'>\n<settings/>\n" > $$T/settings.xml; \
	 rm -f $(3); \
	 ( printf '<openmsx-control>\n'; sleep 2; \
	   printf '<command>source $(CURDIR)/$(2)</command>\n'; \
	   printf '<command>set power on</command>\n'; \
	   for i in $$(seq 600); do [ -f $(3) ] && break; sleep 1; done ) | \
	 $(4) OUTDIR=$(CURDIR)/build /Applications/openMSX.app/Contents/MacOS/openmsx -setting $$T/settings.xml \
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

DRV_REL = build/msx2snd.rel build/msx2input.rel build/msx2storage.rel build/msx2farmem.rel

build/probe.ihx: $(BASE_REL) $(SHARED_REL) $(DRV_REL) build/stubs.rel Makefile
	$(SDCC) $(LDFLAGS) $(BASE_REL) $(SHARED_REL) $(DRV_REL) build/stubs.rel -o $@

# THE GAME. The probe link IS the game now -- every seam is real but the
# overlay stub, which nothing calls -- so this is that image as a .COM.
build/EGATREK.COM: build/probe.ihx
	$(MAKEBIN) -s 65536 -o 256 -p build/probe.ihx $@
	@echo "$@: $$(wc -c < $@) bytes"

# STRINGS.DAT carries this port's machine-naming overrides. The generator
# runs plain first, because it also writes c128/src/strdata.h, which ui.c
# compiles against -- the ids and STR_COUNT must be the ones the binary has.
build/strings.dat: ../c128/src/strings.txt src/strings.override.txt ../tools/gen_strings.py | build
	cd .. && python3 tools/gen_strings.py >/dev/null
	cd .. && python3 tools/gen_strings.py --override msx2/src/strings.override.txt msx2/build/strings.dat

# THE GAME DISK. The game is the SHELL (decided 2026-09-25): MSXDOS2.SYS
# loads COMMAND2.COM, and this is it. The data files sit beside it on the
# boot drive, where bare names find them.
game: build/EGATREK.COM build/strings.dat ../c128/build/music.dat
	rm -rf build/gdisk && mkdir -p build/gdisk
	cp $(TOOLCHAIN)/nextor-boot/MSXDOS2.SYS build/gdisk/
	cp build/EGATREK.COM build/gdisk/COMMAND2.COM
	cp build/strings.dat build/gdisk/STRINGS.DAT
	cp ../c128/build/music.dat build/gdisk/MUSIC.DAT
	cp ../c128/src/briefing.txt build/gdisk/BRIEF.TXT
	@[ -z "$(GAME_SAVE)" ] || cp $(GAME_SAVE) build/gdisk/egatrek.sav
	@ls -l build/gdisk

# THE RELEASE DISK: a 720K image with an MSX-DOS 2 boot sector, the game as
# its COMMAND2.COM, and the data -- the same files `game` puts in build/gdisk.
# `make dskshot` boots the IMAGE, not the directory, and photographs it.
DSK = build/egatrek-msx2.dsk
dsk: game
	python3 tools/mkdsk.py $(DSK) build/gdisk/MSXDOS2.SYS build/gdisk/COMMAND2.COM \
	    build/gdisk/STRINGS.DAT build/gdisk/MUSIC.DAT build/gdisk/BRIEF.TXT

memcheck: build/EGATREK.COM
	rm -rf build/pdisk && mkdir -p build/pdisk
	cp $(TOOLCHAIN)/nextor-boot/* build/pdisk/
	cp build/EGATREK.COM build/pdisk/
	$(call openmsx_run,build/pdisk,tools/memcheck.tcl,build/memcheck_done)
	@grep -v '^ *$$' build/memcheck.txt
	@grep -q 'NOT ENOUGH MEMORY' build/memcheck.txt && grep -q 'EGATREK .COM' build/memcheck.txt && echo "memcheck: PASS (refused, and DOS survived)"

# BOOT THE IMAGE, not the directory it was made from: the staged artefact is
# what gets played. Title, then setup and the console.
dskshot: dsk
	$(MAKE) gameshot SHOT_DISK=$(DSK) SHOT_TIMES=24,62 SHOT_KEYS='25:\r,35:n\r,38:n\r,41:jamie\r,44:1\r,47:trek\r'

# Photograph the running game from VRAM (tools/vram2png.py) at SHOT_TIMES,
# typing SHOT_KEYS ("t:text,...") on the way. build/game_<t>.png.
SHOT_TIMES ?= 30
SHOT_KEYS ?=
# The disk to boot: the directory by default, or the release image --
# `make dskshot` -- which is the artefact a player gets.
SHOT_DISK ?= build/gdisk
# STACK=1 also measures the stack: sentinel from the image end up, read at
# the end of the run (tools/gameshot.tcl).
STACK_FROM = $(if $(STACK),$(shell python3 tools/probe.py --end build/probe.map))
# THE SESSION the running game's stack was measured over (2026-09-25): boot,
# title and music, setup, a move INTO FIRE, three laser volleys and the
# Mongols' turns, SAVE, quit, evaluation, hall of fame, play again, RESTORE.
# The galaxy is seeded from key timing, so these times are the galaxy.
STACK_SESSION = 25:\r,35:n\r,38:n\r,41:jamie\r,44:1\r,47:trek\r,54:m\r,60:2.5 4.4\r,84:l\r,99:500\r,103:500\r,107:500\r,123:\r,136:save\r,152:\r,160:q\r,165:y,197:\r,216:\r,224:y\r,240:\r,250:y\r,256:\r
# WHERE THE TIME GOES: PC samples every PROF_STEP between PROF_FROM and
# PROF_TO, mapped to functions through relocated listings (-Wl-u). The
# listing link must produce the SAME image as the game, and is checked.
PROF_FROM ?= 47.5
PROF_TO   ?= 64
PROF_STEP ?= 0.002
PROF_KEYS ?= 25:\r,35:n\r,38:n\r,41:jamie\r,44:1\r,47:trek\r
profile: game
	sdasz80 -l -o build/crt0_l.rel src/crt0.s && sdasz80 -l -o build/msxbios_l.rel src/msxbios.s
	cd build && $(SDCC) $(LDFLAGS) -Wl-u crt0_l.rel msxbios_l.rel $(notdir $(filter-out build/crt0.rel build/msxbios.rel,$(BASE_REL)) $(SHARED_REL) $(DRV_REL)) stubs.rel -o prof.ihx
	cmp build/prof.ihx build/probe.ihx
	$(call openmsx_run,build/gdisk,tools/profile.tcl,build/profile_done,SHOT_KEYS='$(PROF_KEYS)' PROF_FROM=$(PROF_FROM) PROF_TO=$(PROF_TO) PROF_STEP=$(PROF_STEP))
	python3 tools/profile.py build/profile.txt build/probe.map build/*.rst

stackrun:
	$(MAKE) gameshot STACK=1 SHOT_TIMES=82,122,150,174,215,280 SHOT_KEYS='$(STACK_SESSION)'

gameshot: game
	$(call openmsx_run,$(SHOT_DISK),tools/gameshot.tcl,build/gameshot_done,SHOT_TIMES='$(SHOT_TIMES)' SHOT_KEYS='$(SHOT_KEYS)' STACK_FROM='$(STACK_FROM)')
	@[ -z "$(STACK_FROM)" ] || cat build/stack.txt
	@for t in $$(echo $(SHOT_TIMES) | tr ',' ' '); do python3 tools/vram2png.py build/vram_$$t.bin build/pal_$$t.bin build/game_$$t.png; done

probe: build/probe.ihx
	@python3 tools/probe.py build/probe.map build/stubs.rel $(TPA_TOP)

clean:
	rm -rf build

# A throwaway settings file: openMSX SAVES SETTINGS ON EXIT, and a headless
# run against the real one changes the next interactive launch.
shot: disk
	@T=$$(mktemp -d); \
	 cp $(HOME)/.openMSX/share/settings.xml $$T/settings.xml 2>/dev/null || printf "<!DOCTYPE settings SYSTEM 'settings.dtd'>\n<settings/>\n" > $$T/settings.xml; \
	 $(4) OUTDIR=$(CURDIR)/build /Applications/openMSX.app/Contents/MacOS/openmsx -setting $$T/settings.xml \
	   -machine Philips_NMS_8250 -ext SunriseIDE_Nextor -diska $(CURDIR)/build/disk \
	   -script $(CURDIR)/tools/shot.tcl >/dev/null 2>&1; \
	 rm -rf $$T; cat build/shot.txt; echo "build/shot.png"
