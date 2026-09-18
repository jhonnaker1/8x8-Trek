# EGA Trek for the Foenix F256K. THE TWELFTH-AND-A-HALF PORT.
#
# llvm-mos, NOT cc65 -- which is the opposite of commodore-uno's F256 port and
# is a decision with reasons, measured 2026-09-17:
#
#   * On a fair full link of core/ the two compilers are WITHIN 1% on size.
#     The often-quoted "13% smaller" and uno's "2.3x" were both unreproduced;
#     llvm-mos's real advantage is the MEMORY MODEL, and against the ~40K this
#     machine gives a program that is the number that decides.
#   * cc65 would put C89 on the SHARED half forever -- ui.c has eight
#     declaration-after-statement sites and the root `c89-check` guards core/
#     only. One port must not constrain twelve ports' shared sources.
#
# There is no `f256` platform in llvm-mos, so f256k.ld, tools/mkpgz.py and the
# kernel shim are this port's own -- the Plus/4's pattern, which proved a
# platform is a convenience and not a requirement.
CC       = $(HOME)/llvm-mos/bin/mos-common-clang
OBJCOPY  = $(HOME)/llvm-mos/bin/llvm-objcopy
NM       = $(HOME)/llvm-mos/bin/llvm-nm

CPU      = -mcpu=mos65c02
# TREK_COLOUR_IS_EGA makes EGA_TO_VDC the IDENTITY. It is the right answer for
# any target with a programmable palette, and this one has two LUTs to load.
# The Plus/4 shipped for weeks with its colour header never compiled in and the
# gate passed the whole time, so this define is not a detail.
CFLAGS   = $(CPU) -Oz -DTREK_COLOUR_IS_EGA -I src -I ../c128/src -I ../core -T f256k.ld
LDFLAGS  = -Wl,--no-check-sections -linit-stack -lzero-bss -lcopy-data

# THE RIG. MAME's f256k driver boots, Lua scripting works, and a PGZ reaches it
# through a FAT partition on an SD-card image -- so `run` copies onto a WORKING
# COPY and types `/- <name>` at the SuperBASIC prompt, the way a person does.
# commodore-uno's run.sh is where that recipe came from.
MAME_DIR ?= $(HOME)/Downloads/mame-foenix256k
SDCARD   ?= $(MAME_DIR)/sdcard.img
MAME     = $(MAME_DIR)/mame
MAMEOPT  = -rompath "$(MAME_DIR)" -window -skip_gameinfo -nothrottle

# KEEP THE INTERMEDIATES. Chained implicit rules make .elf and .bin
# "intermediate" and make deletes them -- and the .elf is where every probe's
# symbol addresses come from, and the .map is the only place the layout is
# written down. Losing them turns every debugging session into a rebuild.
.PRECIOUS: build/%.elf build/%.bin build/%.low

.PHONY: all hello frame keys store ovl run check check-ovl clean
all: hello
hello: build/hello.pgz
frame: build/frametest.pgz
keys: build/keytest.pgz
store: build/storetest.pgz
ovl: build/ovltest.pgz build/ovltest.ovl

build:
	@mkdir -p build build/snap

build/%.elf: src/%.c f256k.ld Makefile | build
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@ $<

# kb_waitkey through the real input layer and the real driver.
build/keytest.elf: src/keytest.c src/f256vid.c src/f256key.c src/f256evt.c \
                   src/f256vid.h src/f256kern.h src/f256evt.h f256k.ld Makefile | build
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@ \
	    src/keytest.c src/f256vid.c src/f256key.c src/f256evt.c

build/storetest.elf: src/storetest.c src/f256vid.c src/f256key.c src/f256evt.c \
                     src/f256stor.c \
                     src/f256vid.h src/f256kern.h src/f256evt.h f256k.ld Makefile | build
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@ \
	    src/storetest.c src/f256vid.c src/f256key.c src/f256evt.c src/f256stor.c

# THE OVERLAY BUILD. -DTREK_OVERLAYS turns OVL_CODE from nothing into a
# section attribute; without it every overlay function is inlined into its
# resident caller and every image comes out EMPTY -- which mkovl.py refuses by
# name rather than shipping.
OVLFLAGS = -DTREK_OVERLAYS

build/ovltest.elf: src/ovltest.c src/f256vid.c src/f256key.c src/f256evt.c \
                   src/f256stor.c src/f256ovl.c \
                   src/f256vid.h src/f256kern.h src/f256evt.h f256k.ld Makefile | build
	$(CC) $(CFLAGS) $(OVLFLAGS) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@ \
	    src/ovltest.c src/f256vid.c src/f256key.c src/f256evt.c \
	    src/f256stor.c src/f256ovl.c

# OVERLAYS.BIN RIDES WITH THE PGZ, so `run` and `check` must put BOTH on the
# card. The images are cut from the SAME ELF the program came from, which is
# what makes the stamp meaningful.
build/%.ovl: build/%.elf
	@python3 tools/mkovl.py $< $@ --objcopy $(OBJCOPY) --nm $(NM) --window 8192

build/lowprobe.elf: src/lowprobe.c src/f256vid.c src/f256key.c src/f256evt.c \
                    src/f256stor.c \
                    src/f256vid.h src/f256kern.h src/f256evt.h f256k.ld Makefile | build
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@ \
	    src/lowprobe.c src/f256vid.c src/f256key.c src/f256evt.c src/f256stor.c

# The console frame through the REAL shared layout.c and the REAL driver. A
# pattern rule cannot express this one: it is three sources, not one.
build/frametest.elf: src/frametest.c src/f256vid.c src/f256evt.c \
                     ../c128/src/layout.c \
                     src/f256vid.h src/f256kern.h src/f256evt.h f256k.ld Makefile | build
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@ \
	    src/frametest.c src/f256vid.c src/f256evt.c ../c128/src/layout.c

# TWO SEGMENTS NOW, AND SELECTED BY NAME RATHER THAN BY WHAT IS LEFT OVER.
# The ELF holds three kinds of thing at three very different addresses: the
# resident program at $2000, read-only data at $0400 (the low region), and the
# overlay images at $100000 and up, which no CPU ever sees. A plain
# `objcopy -O binary` spans ALL of them and produced a 1,129,475-byte PGZ --
# the gap between $2000 and $100000 written out as zeros.
build/%.bin: build/%.elf
	$(OBJCOPY) -O binary -j .text -j .data $< $@

build/%.low: build/%.elf
	$(OBJCOPY) -O binary -j .lowrodata $< $@

# THE ENTRY POINT COMES FROM THE BINARY, NOT FROM A CONSTANT. A PGZ carries its
# start address in a zero-size trailing segment, and a wrong one gives a file
# that loads and never runs -- silently.
# The low segment is passed only when there IS one: mkpgz would happily write
# a zero-length segment, and a zero-length segment is how a PGZ says "this is
# the entry point" -- so an empty .lowrodata would terminate the file early
# and the program would never load.
build/%.pgz: build/%.bin build/%.low
	@python3 tools/mkpgz.py $< $@ --load 0x2000 \
	    --exec 0x$$($(NM) $(<:.bin=.elf) | awk '$$3 == "_start" { print $$1 }') \
	    $$(test -s $(<:.bin=.low) && echo --extra 0x0400:$(<:.bin=.low))

# A WORKING COPY, ALWAYS: the SD image is the user's and must not be written.
run: build/$(P).pgz
	@bash tools/sdrun.sh "$(MAME_DIR)" "$(SDCARD)" build/$(P).pgz $(P)

clean:
	rm -rf build

# An AUTOMATED run: same disk, same pexec launch, but scripted -- the marker is
# read out of the machine and a snapshot is taken. `run` is for a person and
# leaves the machine up; `check` is for me and exits.
# The overlay swap, end to end: eleven images into eleven RAM banks, then
# every one loaded forwards, backwards and interleaved, checked BY VALUE.
check-ovl: build/ovltest.pgz build/ovltest.ovl
	@SD_EXTRA="build/ovltest.ovl:OVERLAYS.BIN" \
	 A_SIG=$$($(NM) build/ovltest.elf | awk '$$3=="trek_sig"{print $$1}') \
	 A_RAN=$$($(NM) build/ovltest.elf | awk '$$3=="ran"{print $$1}') \
	 A_GOT=$$($(NM) build/ovltest.elf | awk '$$3=="got"{print $$1}') \
	 A_BACK=$$($(NM) build/ovltest.elf | awk '$$3=="got_back"{print $$1}') \
	 A_MIX=$$($(NM) build/ovltest.elf | awk '$$3=="got_mixed"{print $$1}') \
	 A_PASS=$$($(NM) build/ovltest.elf | awk '$$3=="pass"{print $$1}') \
	 bash tools/sdrun.sh "$(MAME_DIR)" "$(SDCARD)" build/ovltest.pgz ovltest tools/ovl.lua

check: build/$(P).pgz
	@TREK_NAME=$(P) \
	 TREK_MARKER=$$($(NM) build/$(P).elf | awk '$$3 == "ran" { print $$1 }') \
	 TREK_SIG=$$($(NM) build/$(P).elf | awk '$$3 == "trek_sig" { print $$1 }') \
	 bash tools/sdrun.sh "$(MAME_DIR)" "$(SDCARD)" build/$(P).pgz $(P) tools/run.lua
