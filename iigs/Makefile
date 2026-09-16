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

verify: colours bootcheck failcheck

clean:
	rm -rf build

.PHONY: all colours pagemap disk bootcheck failcheck verify clean

.PRECIOUS: build/%.elf
