CC65_TARGET = c128
SRC = src/main.c src/vdc.c src/layout.c src/ui.c src/input.c src/sid.c src/music_data.c \
      ../core/trek.c
OUT = build/trek128.prg
DEBUG_OUT = build/trek128-debug.prg

.PHONY: all run monitor test verify music clean

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
	   src/layout.c ../core/trek.c
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
      ../core/ega.h ../core/trek.h

# -m emits a map file. That is not housekeeping: tools/vice_mon.py can write
# guest memory, but only if it knows where a variable lives, and the map is
# the only place that says. Poking ship state and screenshotting the result is
# the fastest way to check a panel without playing to that state.
$(OUT): $(SRC) $(HDR)
	cl65 -t $(CC65_TARGET) -O -m build/trek128.map -o $(OUT) $(SRC)

# A SEPARATE binary, deliberately. TREK_DEBUG_INPUT compiles in a byte that a
# script can poke to fake a keypress (see src/input.h), and the release build
# must not have it. Separate output means `make run` can never accidentally
# launch the instrumented one.
$(DEBUG_OUT): $(SRC) $(HDR)
	cl65 -t $(CC65_TARGET) -O -DTREK_DEBUG_INPUT \
	     -m build/trek128-debug.map -o $(DEBUG_OUT) $(SRC)

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

clean:
	rm -f build/*.prg build/test_egavdc build/test_panels build/test_sid src/*.o src/music_data.h src/music_data.c
