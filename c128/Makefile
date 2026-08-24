# llvm-mos, not cc65. MIGRATED 2026-08-23 -- see NOTES.md "llvm-mos on the
# C128". The short version: cc65 left 210 bytes of MAIN free and the disk seam
# needed 815, llvm-mos leaves about 6,000 with the seam already linked, and the
# charmap bug class that bit this project four separate times does not exist on
# this toolchain.
#
# -Oz, NOT -Os. Measured on this codebase: -Oz is 3,200 bytes smaller than -Os,
# which is most of the win. -flto adds nothing on top of either.
MOSCC = $(HOME)/llvm-mos/bin/mos-c128-clang
CFLAGS = -Oz -I src -I ../core -Wall

SRC = src/main.c src/vdc.c src/layout.c src/ui.c src/input.c src/sid.c \
      src/music_data.c src/storage.c \
      ../core/trek.c ../core/hof.c ../core/serial.c
OUT = build/trek128.prg
DEBUG_OUT = build/trek128-debug.prg

.PHONY: all run rund d64 monitor test verify music clean

all: $(OUT)

# The note data is Nels Anderson's, so it is generated rather than committed --
# see tools/gen_music.py. On a clone without reference/ this still writes a
# valid header with empty tracks, so the build never depends on gitignored
# material being present.
music src/music_data.h src/music_data.c:
	cd .. && python3 tools/gen_music.py

# The EGA->VDC colour mapping is pure arithmetic, so it is proven natively on
# the build machine rather than by squinting at an emulator. Getting it wrong
# is silent -- every colour still renders, just as the wrong hue.
test:
	cc -Wall -Wextra -o build/test_egavdc test/test_egavdc.c
	./build/test_egavdc
	cc -Wall -Wextra -o build/test_panels test/test_panels.c src/ui.c \
	   src/layout.c ../core/trek.c ../core/hof.c
	./build/test_panels
	cc -Wall -Wextra -o build/test_sid test/test_sid.c -lm
	./build/test_sid

# Inspects the LINKED binary, which is the only place cc65's character-set
# translation is visible. Native cc does no such translation, so `make test`
# above passes whether or not the encoding is right -- three bugs in this port
# proved that the hard way. `run` depends on this, so a binary with a PETSCII
# key table can never reach the emulator.
verify: $(OUT)
	python3 tools/verify_prg.py

HDR = src/vdc.h src/layout.h src/egavdc.h src/ui.h src/input.h \
      src/sid.h src/sidfreq.h src/music_data.h \
      ../core/ega.h ../core/trek.h ../core/hof.h ../core/storage.h \
      ../core/serial.h

# -m emits a map file. That is not housekeeping: tools/vice_mon.py can write
# guest memory, but only if it knows where a variable lives, and the map is
# the only place that says. Poking ship state and screenshotting the result is
# the fastest way to check a panel without playing to that state.
$(OUT): $(SRC) $(HDR)
	@mkdir -p build
	$(MOSCC) $(CFLAGS) -Wl,-Map=build/trek128.map -o $(OUT) $(SRC)

# A SEPARATE binary, deliberately. TREK_DEBUG_INPUT compiles in a byte that a
# script can poke to fake a keypress (see src/input.h), and the release build
# must not have it. Separate output means `make run` can never accidentally
# launch the instrumented one.
$(DEBUG_OUT): $(SRC) $(HDR)
	@mkdir -p build
	$(MOSCC) $(CFLAGS) -DTREK_DEBUG_INPUT \
	     -Wl,-Map=build/trek128-debug.map -o $(DEBUG_OUT) $(SRC)

# The VDC picture is on x128's second window. VICE opens both the VIC-IIe and
# VDC windows; this port drives only the VDC, so the 40-column window shows
# whatever BASIC left behind and can be ignored.
# Same as `run`, but with VICE's BINARY monitor open on port 6502 so
# tools/vice_mon.py can screenshot the VDC and read or write memory. Not the
# remote (text) monitor -- that one halts emulation the moment anything
# connects, which is what made this look impossible for a while.
monitor: $(DEBUG_OUT)
	x128 -binarymonitor -binarymonitoraddress ip4://127.0.0.1:6502 \
	     -autostart $(DEBUG_OUT)

run: verify
	# -autostart, not a bare filename, so VICE loads AND runs the PRG.
	# It still probes the file as a disk image, a tape and a cartridge on
	# the way -- the three "Error" lines in the log are that autodetect
	# and are harmless. An earlier version of this comment claimed the
	# flag suppressed them. It does not; only the RUN is its doing.
	x128 -autostart $(OUT)

# A real disk, because file I/O needs one. c1541 ships with VICE and is already
# installed here. This is what `SAVE`, the hall of fame and the briefing will
# all load and store through.
#
# NOTE WHAT THIS CHANGES: `make run` autostarts a bare PRG and always has, so
# a build that reads TREK.SCR sees no drive. tools/exit_real.py and
# tools/sound_check.py autostart the PRG directly too, and are unaffected only
# because neither touches a file. Use `make rund` for anything that does.
D64 = build/trek128.d64

d64: $(D64)

$(D64): $(OUT)
	@mkdir -p build
	rm -f $(D64)
	c1541 -format "ega trek,01" d64 $(D64) >/dev/null
	c1541 -attach $(D64) -write $(OUT) trek128 >/dev/null
	@# The hall of fame ships EMPTY rather than copied from reference/: the
	@# original's TREK.SCR is Anderson's file, and an all-zero table is
	@# exactly what a fresh install should have anyway.
	@python3 -c "import sys; sys.stdout.buffer.write(b''.join(b'.'*25 + b'\r\n0\r\n' for _ in range(10)))" > build/trek.scr
	c1541 -attach $(D64) -write build/trek.scr trek.scr,s >/dev/null
	@echo "d64: $(D64)"
	@c1541 -attach $(D64) -dir

rund: $(D64)
	x128 -autostart $(D64)

clean:
	rm -f build/*.prg build/test_egavdc build/test_panels build/test_sid src/*.o src/music_data.h src/music_data.c
