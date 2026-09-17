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
# -linit-stack, AND IT WAS NOT HERE. `__do_init_stack` was absent from the
# binary entirely: the soft stack pointer __rc0/__rc1 was never loaded with
# __stack, so it held whatever BASIC had left at $02/$03 -- measured on the
# live machine as $FFCD against a __stack of $CC00. Every C call then built
# its frame in the middle of the far store and the I/O page.
#
# The commodore platform links zero-bss on its own and does NOT link this. The
# Apple IIgs port met the same thing on the `common` platform the same day and
# its Makefile says so: nothing in a map tells you an init library is missing,
# because what is absent is an absence.
LDFLAGS  = -Wl,--no-check-sections -linit-stack

SHARED   = ../c128/src/main.c ../c128/src/layout40.c ../c128/src/ui.c \
           ../c128/src/music_data.c ../c128/src/storage.c \
           ../c128/src/overlay.c ../c128/src/strpool.c \
           ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c
# THIS PORT'S OWN. Four files where the C64 wrote two, and the survey said so:
# TED is neither a VIC-II nor a SID, so vic.c and sid.c cannot be linked here
# the way the C64 links them.
# src/p4key.c REPLACES the shared ../c128/src/input.c, which is CIA1 code:
# it reads $DC00/$DC01, and a Plus/4 has no CIA -- that address is plain RAM
# inside this program's own image. It also picks its cursor-key handling with
# `#ifdef __C128__ ... #else __C64__`, and mos-commodore-clang defines NEITHER,
# so this port was silently compiling the C64 branch. See src/p4key.c.
PLATFORM = src/ted.c src/p4log.c src/p4mem.c src/tedsnd.c src/basichdr.c \
           src/p4bank.c src/p4key.c
SRC      = $(SHARED) $(PLATFORM)

OUT = build/trek4.prg
MAP = build/trek4.map

OBJCOPY  = $(HOME)/llvm-mos/bin/llvm-objcopy
NM       = $(HOME)/llvm-mos/bin/llvm-nm
ELF      = build/trek4.elf
D64      = build/egatrek-plus4.d64

OVERLAYS = eval hof front info repair msgs planet cmds title events xtra

# Read out of the linker script, not repeated here -- one number, one home.
OVL_WINDOW := $(shell sed -n 's/.*window (rwx).*LENGTH = \(0x[0-9A-Fa-f]*\).*/\1/p' plus4.ld)

.PHONY: all verify early frametest data d64 clean
all: $(OUT)

$(OUT): $(SRC) plus4.ld Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) -DTREK_OVERLAYS $(LDFLAGS) -Wl,-Map=$(MAP) -o $(ELF) $(SRC)
	@# The overlay sections are cut out SEPARATELY and must not be in the
	@# program image -- they run at $$CC00 but are stored elsewhere in the ELF.
	$(OBJCOPY) -O binary --remove-section=.ovl_* $(ELF) build/body.bin
	@python3 -c "d = open('build/body.bin','rb').read(); \
	    open('$@','wb').write(bytes([0x01,0x10]) + d)"
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

# THE DATA FILES. gen_strings.py owns the numbering, so every port builds the
# same strdata.h and the same STR_COUNT; only the strings that name the machine
# differ, and those live in src/strings.override.txt.
#
# THE MUSIC IS THIS PORT'S OWN COMPOSITION, from tools/make_music.py -- never
# the note data tools/gen_music.py extracts from EGATREK.EXE, which is
# Anderson's creative work and can never ship.
data: build/strings.dat build/music.dat

build/strings.dat: ../c128/src/strings.txt src/strings.override.txt
	@mkdir -p build
	cd .. && python3 tools/gen_strings.py >/dev/null
	cd .. && python3 tools/gen_strings.py --override plus4/src/strings.override.txt \
	    plus4/build/strings.dat

build/music.dat: ../c128/src/music_data.c
	@mkdir -p build
	cd .. && python3 tools/make_music.py >/dev/null
	cp ../c128/build/music.dat $@

# $(1) = the ELF to cut every overlay out of. The C64's macro, with its two
# scars carried rather than rediscovered: a SHELL LOOP and not $(foreach),
# because foreach joins with a space and silently loses one objcopy; and a
# STAMP on every image -- the low sixteen bits of ovl_load's address in the
# link it was cut from -- because an overlay is linked WITH the resident half,
# so yesterday's images beside today's program jump into the middle of some
# other function.
define OVL_IMAGES
	@stamp=`$(NM) $(1) | awk '$$3 == "ovl_load" { print $$1 }'`; \
	for o in $(OVERLAYS); do \
	    $(OBJCOPY) --dump-section=.ovl_$$o=build/ovl_$$o.raw $(1) /dev/null || exit 1; \
	    python3 -c "import sys; \
	        n = sys.argv[1]; a = int(sys.argv[2], 16); \
	        d = open('build/ovl_%s.raw' % n, 'rb').read(); \
	        w = int('$(OVL_WINDOW)', 16); \
	        d = d + bytes([a & 0xFF, (a >> 8) & 0xFF]); \
	        open('build/ovl%s.prg' % n, 'wb').write(bytes([0x00,0xCC]) + d); \
	        sys.exit('overlay %s is %d bytes, the window is %d' % (n, len(d), w)) \
	            if len(d) > w else None" $$o $$stamp || exit 1; \
	done
	@echo "  overlays: $(words $(OVERLAYS)) images cut, window $(OVL_WINDOW)"
endef

# THE DISK IS FORMATTED FROM EMPTY EVERY TIME. `c1541 -write` over an existing
# name answers ERR = 63, FILE EXISTS and LEAVES THE OLD FILE -- which is how a
# deliberately-broken probe reported a PASS in the C64 directory. Formatting is
# the only way -write can be trusted.
d64: $(D64)
$(D64): $(OUT) data ../c128/src/briefing40.txt verify Makefile
	$(call OVL_IMAGES,$(ELF))
	@rm -f $(D64)
	c1541 -format "ega trek p4,01" d64 $(D64) >/dev/null 2>&1
	c1541 -attach $(D64) -write $(OUT) trek4 >/dev/null 2>&1
	@# The hall of fame ships EMPTY. The original's TREK.SCR is Anderson's
	@# file, and an all-zero table is what a fresh install should have.
	@python3 -c "import sys; sys.stdout.buffer.write(b''.join(b'.'*25 + b'\r\n0\r\n' for _ in range(10)))" > build/trek.scr
	c1541 -attach $(D64) -write build/trek.scr trek.scr,s >/dev/null 2>&1
	@# The briefing, STREAMED a byte at a time through plat_open/plat_read.
	c1541 -attach $(D64) -write ../c128/src/briefing40.txt brief.txt,s >/dev/null 2>&1
	@# WRITTEN AS PRG, NOT SEQ. far_load uses the KERNAL's LOAD with secondary
	@# address 0, which reads and DISCARDS a PRG's load address and puts the
	@# rest where it is told. The value written is ignored; $DD00 is used so a
	@# human LOADing one by hand sees where it really belongs.
	@python3 -c "d = open('build/strings.dat','rb').read(); \
	    open('build/strings.pdat','wb').write(bytes([0x00,0xDD]) + d)"
	c1541 -attach $(D64) -write build/strings.pdat strings.dat >/dev/null 2>&1
	@python3 -c "d = open('build/music.dat','rb').read(); \
	    open('build/music.pdat','wb').write(bytes([0x00,0xDD]) + d)"
	c1541 -attach $(D64) -write build/music.pdat music.dat >/dev/null 2>&1
	@for o in $(OVERLAYS); do \
	    c1541 -attach $(D64) -write build/ovl$$o.prg ovl$$o >/dev/null 2>&1; \
	done
	@echo "disk: $(D64)"

clean:
	rm -f build/*.prg build/*.map build/*.raw build/*.pdat build/*.d64
