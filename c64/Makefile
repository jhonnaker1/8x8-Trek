# EGA Trek for the Commodore 64 -- the eighth port, and the one that is almost
# entirely somebody else's code.
#
# THE 40-COLUMN LINE. c128/src/vic.c was written as the C128's 40-column
# driver and is linked here UNCHANGED: a VIC-IIe in 40-column mode and a VIC-II
# are the same chip for everything this port does. ui.c, main.c, layout40.c,
# input.c, sid.c, storage.c, overlay.c and strpool.c are shared the same way,
# which is why `make -C c128 game40` has to keep working -- it is this port's
# regression test as much as its own.
#
# WHAT IS ACTUALLY THE C64'S OWN is two files. src/c64mem.c puts the string
# pool in the RAM under the KERNAL, and src/c64log.c puts the message log in a
# plain array. Both replace a piece of C128 hardware: bank 1, and the VDC's
# spare 16K. See NOTES.md item 57, which said far memory was the one new seam
# and was wrong -- it is two.
#
# .DEFAULT_GOAL stated because this file grew a probe harness before it grew a
# game, and make's default goal is whatever target it reads first. The MEGA65
# built its smoke test for weeks on a bare `make` for exactly this reason.
.DEFAULT_GOAL := all

LLVM_MOS ?= $(HOME)/llvm-mos
CC       = $(LLVM_MOS)/bin/mos-c64-clang
OBJCOPY  = $(LLVM_MOS)/bin/llvm-objcopy
NM       = $(LLVM_MOS)/bin/llvm-nm

X64 ?= x64sc

# -Oz for the same measured reason the C128 uses it: 3,200 bytes smaller than
# -Os on this codebase.
#
# NO -lcopy-data, AND THAT IS A DELIBERATE DIFFERENCE FROM trek128.ld. The
# C128 moves c_writeable to low RAM, which splits .data's run address from its
# load address and makes the copy routine mandatory -- leaving it out there
# cost the port its sound for a day. c64.ld aliases BOTH to `ram`, so VMA
# equals LMA and there is nothing to copy. `verify` checks that this is still
# true rather than trusting the comment.
CFLAGS  = -Oz -I src -I ../c128/src -I ../core -Wall -Werror \
          -DTREK_OVERLAYS -DTREK_40COL -T c64.ld
LDFLAGS = -Wl,--no-check-sections

# THE SHARED HALF, and it is nearly all of it.
SHARED = ../c128/src/main.c ../c128/src/vic.c ../c128/src/layout40.c \
         ../c128/src/ui.c ../c128/src/input.c ../c128/src/sid.c \
         ../c128/src/music_data.c ../c128/src/storage.c \
         ../c128/src/overlay.c ../c128/src/strpool.c \
         ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c
# THE C64'S OWN. Two files, both of them answers to "the C128 had a chip for
# this and you do not".
PLATFORM = src/c64mem.c src/c64log.c
SRC = $(SHARED) $(PLATFORM)

OUT       = build/trek64.prg
ELF       = build/trek64.elf
MAP       = build/trek64.map
DEBUG_OUT = build/trek64-debug.prg
DEBUG_ELF = build/trek64-debug.elf
D64       = build/egatrek-c64.d64
DEBUG_D64 = build/trek64-debug.d64

OVERLAYS = eval hof front info repair msgs planet cmds title events xtra

# Read out of the linker script, not repeated here -- one number, one home.
OVL_WINDOW := $(shell sed -n 's/.*window (rwx).*LENGTH = \(0x[0-9A-Fa-f]*\).*/\1/p' c64.ld)

# $(1) = the ELF to cut every overlay out of. Lifted from c128/Makefile
# including its two scars: a shell loop rather than $(foreach), because
# foreach joins with a space and silently loses one objcopy; and a STAMP on
# every image -- the low sixteen bits of ovl_load's address in the link it was
# cut from -- because an overlay is linked WITH the resident half and yesterday's
# images beside today's program jump into the middle of some other function.
define OVL_IMAGES
	@stamp=`$(NM) $(1) | awk '$$3 == "ovl_load" { print $$1 }'`; \
	for o in $(OVERLAYS); do \
	    $(OBJCOPY) --dump-section=.ovl_$$o=build/ovl_$$o.raw $(1) /dev/null || exit 1; \
	    python3 -c "import sys; \
	        n = sys.argv[1]; a = int(sys.argv[2], 16); \
	        d = open('build/ovl_%s.raw' % n, 'rb').read(); \
	        w = int('$(OVL_WINDOW)', 16); \
	        d = d + bytes([a & 0xFF, (a >> 8) & 0xFF]); \
	        open('build/ovl%s.prg' % n, 'wb').write(bytes([0x00,0xC0]) + d); \
	        print('  overlay %s: %d bytes + stamp, window is %d' % (n, len(d) - 2, w)); \
	        sys.exit('overlay %s does not fit the window' % n) if len(d) > w else None" $$o $$stamp || exit 1; \
	done
endef

# $(1) = the ELF, $(2) = the PRG. $0801 in front, and the overlay sections cut
# out -- they travel as their own files.
define MAKE_PRG
	$(OBJCOPY) -O binary --remove-section=.ovl_* $(1) $(2).res
	@python3 -c "d = open('$(2).res','rb').read(); \
	    open('$(2)','wb').write(bytes([0x01,0x08]) + d)"
endef

.PHONY: all game debug d64 d64-debug run rund verify probe logcheck clean

all: $(OUT)
game: $(OUT)

$(ELF): $(SRC) c64.ld Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=$(MAP) -o $@ $(SRC)

$(OUT): $(ELF)
	$(call MAKE_PRG,$(ELF),$(OUT))
	$(call OVL_IMAGES,$(ELF))
	@python3 tools/report_size.py $(ELF) $(OUT)

# A SEPARATE BINARY, as on every other port: TREK_DEBUG_INPUT compiles in the
# byte a script pokes to fake a keypress, and the release build must not have
# it. Its overlay images differ from the release build's, so `d64-debug` cuts
# them from its own ELF.
debug: $(DEBUG_OUT)

$(DEBUG_ELF): $(SRC) c64.ld Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) -DTREK_DEBUG_INPUT $(LDFLAGS) \
	    -Wl,-Map=build/trek64-debug.map -o $@ $(SRC)

$(DEBUG_OUT): $(DEBUG_ELF)
	$(call MAKE_PRG,$(DEBUG_ELF),$(DEBUG_OUT))

# THE DATA FILES. strings.dat carries this port's four overrides (see
# src/strings.override.txt); music.dat is the C128's, because the composition
# is the project's and not the machine's.
build/strings.dat: ../c128/src/strings.txt src/strings.override.txt ../tools/gen_strings.py
	@mkdir -p build
	cd .. && python3 tools/gen_strings.py >/dev/null
	cd .. && python3 tools/gen_strings.py --override c64/src/strings.override.txt \
	    c64/build/strings.dat

build/music.dat: ../tools/make_music.py
	@mkdir -p build
	cd .. && python3 tools/make_music.py >/dev/null
	cp ../c128/build/music.dat build/music.dat

# $(1) = the .d64, $(2) = the program PRG, $(3) = the ELF the overlays come
# from. THE IMAGES ARE CUT HERE, in the disk rule, and not left to the link
# rule: build/ovl*.prg is shared between the release and debug builds and
# whichever linked last wins, so a disk that merely depends on the PRG can
# pick up the OTHER build's overlays when the PRG is already up to date.
#
# THE DISK IS FORMATTED FROM EMPTY EVERY TIME. `c1541 -write` over an existing
# name answers ERR = 63, FILE EXISTS and LEAVES THE OLD FILE -- which is how a
# deliberately-broken probe reported a PASS in this very directory on
# 2026-09-14. Formatting is the only way -write can be trusted.
define MAKE_D64
	$(call OVL_IMAGES,$(3))
	@rm -f $(1)
	c1541 -format "ega trek 64,01" d64 $(1) >/dev/null
	c1541 -attach $(1) -write $(2) trek64 >/dev/null
	@# The hall of fame ships EMPTY. The original's TREK.SCR is Anderson's
	@# file, and an all-zero table is what a fresh install should have.
	@python3 -c "import sys; sys.stdout.buffer.write(b''.join(b'.'*25 + b'\r\n0\r\n' for _ in range(10)))" > build/trek.scr
	c1541 -attach $(1) -write build/trek.scr trek.scr,s >/dev/null
	@# The briefing, STREAMED a byte at a time through plat_open/plat_read --
	@# twenty-four pages will not fit in memory on this machine either.
	c1541 -attach $(1) -write ../c128/src/briefing40.txt brief.txt,s >/dev/null
	@# WRITTEN AS PRG, NOT SEQ, and the two-byte header is the reason:
	@# far_load() uses the KERNAL's LOAD with secondary address 0, which
	@# reads and DISCARDS a PRG's load address and puts the rest where it is
	@# told. The value written is ignored; $E000 is used so a human LOADing
	@# one by hand sees where it really belongs.
	@python3 -c "d = open('build/strings.dat','rb').read(); \
	    open('build/strings.pdat','wb').write(bytes([0x00,0xE0]) + d)"
	c1541 -attach $(1) -write build/strings.pdat strings.dat >/dev/null
	@python3 -c "d = open('build/music.dat','rb').read(); \
	    open('build/music.pdat','wb').write(bytes([0x00,0xE0]) + d)"
	c1541 -attach $(1) -write build/music.pdat music.dat >/dev/null
	@for o in $(OVERLAYS); do \
	    c1541 -attach $(1) -write build/ovl$$o.prg ovl$$o >/dev/null; \
	done
	@echo "disk: $(1)"
endef

d64: $(D64)
$(D64): $(OUT) build/strings.dat build/music.dat ../c128/src/briefing40.txt verify Makefile
	$(call MAKE_D64,$(D64),$(OUT),$(ELF))

d64-debug: $(DEBUG_D64)
$(DEBUG_D64): $(DEBUG_OUT) build/strings.dat build/music.dat ../c128/src/briefing40.txt Makefile
	$(call MAKE_D64,$(DEBUG_D64),$(DEBUG_OUT),$(DEBUG_ELF))

# THE CHECKS THE COMPILER STRUCTURALLY CANNOT MAKE. `d64` depends on this so a
# binary that cannot run can never reach a disk -- the same wiring the MEGA65
# uses and for the same reason.
verify: $(OUT) build/strings.dat build/music.dat
	python3 ../c128/tools/layout40_check.py
	python3 tools/verify_c64.py

run: $(D64)
	$(X64) -autostart $(CURDIR)/$(D64):trek64

rund: $(DEBUG_D64)
	$(X64) -autostart $(CURDIR)/$(DEBUG_D64):trek64

# The item 59 probe that started this port: can 2K under the KERNAL survive a
# KERNAL LOAD. Kept because it is the evidence behind c64mem.c.
probe: build/probe.d64

build/logprobe.prg: src/logprobe.c Makefile
	@mkdir -p build
	$(CC) -Oz -Wall -Werror -o $@ $<
	@ls -l $@

build/probe.d64: build/logprobe.prg build/music.dat
	@mkdir -p build
	@rm -f $@
	@c1541 -format "logprobe,01" d64 $@ >/dev/null
	@c1541 -attach $@ -write build/logprobe.prg logprobe >/dev/null
	@python3 -c "d = open('build/music.dat','rb').read(); \
	    open('build/music.pdat','wb').write(bytes([0x00,0x40]) + d)"
	@c1541 -attach $@ -write build/music.pdat music.dat >/dev/null
	@echo "probe disk: $@"

logcheck: build/probe.d64
	@python3 tools/logcheck.py

clean:
	rm -f build/*.prg build/*.elf build/*.d64 build/*.dat build/*.pdat \
	      build/*.res build/*.raw build/*.map build/*.scr
