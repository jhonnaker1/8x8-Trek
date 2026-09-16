# EGA Trek for the Apple IIgs -- SCOPING, not yet a port.
#
# WHAT IS ESTABLISHED (2026-09-16, MAME 0.289 from Ample, verified apple2gs
# romset): llvm-mos's mosw65816 compiles C that runs on a real IIgs ROM 03,
# hand-written long addressing reaches Super Hi-Res in bank $E1 from that C,
# and 320 mode gives SIXTEEN freely-placeable colours -- counted off the
# snapshot with tools/count_colours.py, the same instrument that counted FOUR
# in 640 mode on 2026-09-05 and ruled the machine out.
#
# WHAT IS NOT: no ProDOS, no disk, no keyboard, no sound, no game. The rig
# pokes the image into bank 0 and sets the PC. See README.md.
#
# There is no `apple2gs` platform in llvm-mos, so mos-common-clang plus this
# port's own iigs.ld -- the same position the Plus/4 was in, and the reason
# the C runtime's init libraries are named explicitly below: the common
# platform links NONE of them, so without this line __stack is never loaded
# and .bss is never zeroed. Nothing in a map says so.
CC      = $(HOME)/llvm-mos/bin/mos-common-clang
OBJCOPY = $(HOME)/llvm-mos/bin/llvm-objcopy
NM      = $(HOME)/llvm-mos/bin/llvm-nm

CFLAGS  = -mcpu=mosw65816 -Oz -T iigs.ld
LDFLAGS = -Wl,--no-check-sections
LDLIBS  = -linit-stack -lzero-bss -lcopy-zp-data -lcopy-data

MAME    = $(HOME)/ample/Ample.app/Contents/MacOS/mame64
ROMS    = $(HOME)/Library/Application Support/Ample/roms
# -window and -skip_gameinfo always; -nothrottle is right for a measurement
# and wrong for anything a person watches.
MAMEOPT = -rompath "$(ROMS)" -window -skip_gameinfo -nothrottle -sound none \
          -autoboot_delay 0

PROBES  = gsprobe gsmap gsfill1 gsdiv

all: $(PROBES:%=build/%.bin)

build/%.elf: src/%.c iigs.ld | build
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) $< -o $@ $(LDLIBS)

build/%.bin: build/%.elf
	$(OBJCOPY) -O binary $< $@

build:
	mkdir -p build build/snap

# The colour measurement, end to end. 16 is the whole point: the console needs
# fifteen and 640 mode has four.
colours: build/gsprobe.bin
	rm -f build/snap/apple2gs/*.png
	GS_BIN=build/gsprobe.bin GS_SHOT=1 $(MAME) apple2gs $(MAMEOPT) \
	  -snapshot_directory build/snap -autoboot_script tools/run.lua \
	  -seconds_to_run 25
	python3 tools/count_colours.py build/snap/apple2gs/0000.png --expect 16

# Every page of SHR written with its own page number and read straight back.
# Runs with SHR OFF because a readback taken with the display on is doubled.
pagemap: build/gsmap.bin
	GS_BIN=build/gsmap.bin GS_WAIT=5 \
	  GS_DUMP='0xE12000:125:0x100' $(MAME) apple2gs $(MAMEOPT) \
	  -video none -autoboot_script tools/run.lua -seconds_to_run 22

# THE BOOT CHAIN, which is the half the Plus/4 never got. Block 0 is ours, it
# reads the payload off the disk through the slot's ProDOS block driver, and
# jumps to $2000. No ProDOS anywhere.
build/boot.bin: src/boot.S boot.ld | build
	$(CC) -mcpu=mosw65816 -nostdlib -T boot.ld -Wl,--no-check-sections \
	  src/boot.S -o build/boot.elf
	$(OBJCOPY) -O binary build/boot.elf $@

# The same block, asking for a block one past the end of the disk -- the only
# way to make the drive say no, and therefore the only way to execute the
# failed-read path at all.
build/bootbad.bin: src/boot.S boot.ld | build
	$(CC) -mcpu=mosw65816 -nostdlib -DSTART_BLOCK=1600 -T boot.ld \
	  -Wl,--no-check-sections src/boot.S -o build/bootbad.elf
	$(OBJCOPY) -O binary build/bootbad.elf $@

build/egatrek.po: build/boot.bin build/gsprobe.bin tools/mkdisk.py
	python3 tools/mkdisk.py $@ --boot build/boot.bin --payload build/gsprobe.bin

build/badread.po: build/bootbad.bin build/gsprobe.bin tools/mkdisk.py
	python3 tools/mkdisk.py $@ --boot build/bootbad.bin --payload build/gsprobe.bin

disk: build/egatrek.po

# Boot the disk and count the colours. This is the whole chain: disk -> our
# block 0 -> block reads through the slot firmware -> compiled C at $2000 ->
# Super Hi-Res.
bootcheck: build/egatrek.po
	mkdir -p build/snap/apple2gs && rm -f build/snap/apple2gs/*.png
	GS_SHOT=1 $(MAME) apple2gs $(MAMEOPT) -flop3 build/egatrek.po \
	  -snapshot_directory build/snap -autoboot_script tools/bootrun.lua \
	  -seconds_to_run 30
	python3 tools/count_colours.py build/snap/apple2gs/0000.png --expect 16

# And the failure path, which is a comment until something has executed it.
failcheck: build/badread.po
	mkdir -p build/snap/apple2gs && rm -f build/snap/apple2gs/*.png
	GS_SHOT=1 $(MAME) apple2gs $(MAMEOPT) -flop3 build/badread.po \
	  -snapshot_directory build/snap -autoboot_script tools/bootrun.lua \
	  -seconds_to_run 30
	python3 tools/count_colours.py build/snap/apple2gs/0000.png --expect 1

# THE CONSOLE. gsvid.c draws c128/src/vdc.h's 40x25 grid into Super Hi-Res,
# and gssmoke.c is the proof sheet: every text code, the box set, a drawn
# panel, sixteen colours and reverse video, on a real machine. The host-side
# sheet coco3/tools/gen_font.py renders is the other half -- a font that looks
# right there and wrong here is a bug in gsvid.c, and one picture cannot tell.
VIDFLAGS = -mcpu=mosw65816 -Oz -I src -I ../c128/src -I ../core \
           -DTREK_40COL -DTREK_COLOUR_IS_EGA -T iigs.ld

build/gssmoke.bin: src/gssmoke.c src/gsvid.c iigs.ld | build
	$(CC) $(VIDFLAGS) $(LDFLAGS) src/gssmoke.c src/gsvid.c \
	  -o build/gssmoke.elf $(LDLIBS)
	$(OBJCOPY) -O binary build/gssmoke.elf $@

# The same sheet with the box glyphs SHIFTED like the text instead of authored
# at eight wide -- the obvious wrong answer, kept buildable so the gate can be
# shown to catch it.
build/gssmokebad.bin: src/gssmoke.c src/gsvid.c iigs.ld | build
	$(CC) $(VIDFLAGS) -DGS_BOX_NAIVE $(LDFLAGS) src/gssmoke.c src/gsvid.c \
	  -o build/gssmokebad.elf $(LDLIBS)
	$(OBJCOPY) -O binary build/gssmokebad.elf $@

build/smoke.po: build/boot.bin build/gssmoke.bin tools/mkdisk.py
	python3 tools/mkdisk.py $@ --boot build/boot.bin --payload build/gssmoke.bin

build/smokebad.po: build/boot.bin build/gssmokebad.bin tools/mkdisk.py
	python3 tools/mkdisk.py $@ --boot build/boot.bin --payload build/gssmokebad.bin

define BOOT_AND_SHOT
	mkdir -p build/snap/apple2gs && rm -f build/snap/apple2gs/*.png
	GS_SHOT=1 $(MAME) apple2gs $(MAMEOPT) -flop3 $(1) 	  -snapshot_directory build/snap -autoboot_script tools/bootrun.lua 	  -seconds_to_run 35
endef

sheet: build/smoke.po
	$(call BOOT_AND_SHOT,build/smoke.po)
	python3 tools/check_sheet.py build/snap/apple2gs/0000.png

# ARM-CHECK: the gate must catch the wrong box glyphs, or it is decoration.
# Inverted, because here a PASS is the failure.
sheetfail: build/smokebad.po
	$(call BOOT_AND_SHOT,build/smokebad.po)
	@if python3 tools/check_sheet.py build/snap/apple2gs/0000.png; then \
	  echo "ARM-CHECK FAILED: check_sheet passed the naive box glyphs"; \
	  exit 1; \
	else \
	  echo "arm-check ok: check_sheet rejected the naive box glyphs"; \
	fi

# THE SIZE QUESTION, answered the way the Plus/4 answered it: link the real
# game against a stubbed seam and read the map. The number is a FLOOR -- every
# stub costs a handful of bytes where a driver costs hundreds, and the Falcon
# measured video at 4,636 bytes for a 1,559-byte driver because THE CALLERS
# GROW. Read it as "not less than", never as "about".
SHARED = ../c128/src/main.c ../c128/src/layout40.c ../c128/src/ui.c \
         ../c128/src/strpool.c ../core/trek.c ../core/planet.c \
         ../core/hof.c ../core/serial.c
EARLYFLAGS = -mcpu=mosw65816 -Oz -I src -I ../c128/src -I ../core \
             -DTREK_40COL -DTREK_OVERLAYS -T iigs.ld

build/early.elf: $(SHARED) src/gsstub.c iigs.ld | build
	$(CC) $(EARLYFLAGS) $(LDFLAGS) -Wl,-Map=build/early.map \
	  $(SHARED) src/gsstub.c -o $@ $(LDLIBS)

build/earlyvid.elf: $(SHARED) src/gsstub.c src/gsvid.c iigs.ld | build
	$(CC) $(EARLYFLAGS) -DTREK_COLOUR_IS_EGA -DGS_REAL_VIDEO $(LDFLAGS) \
	  -Wl,-Map=build/earlyvid.map $(SHARED) src/gsstub.c src/gsvid.c \
	  -o $@ $(LDLIBS)

early: build/early.elf build/earlyvid.elf
	@echo "-- seam entirely stubbed"
	python3 tools/verify_gs.py build/early.elf --window 0xB000 --org 0x0800
	@echo "-- with the REAL video driver linked"
	python3 tools/verify_gs.py build/earlyvid.elf --window 0xB000 --org 0x0800

verify: colours bootcheck failcheck early sheet sheetfail

clean:
	rm -rf build

.PHONY: all colours pagemap disk bootcheck failcheck early sheet sheetfail verify clean

.PRECIOUS: build/%.elf
