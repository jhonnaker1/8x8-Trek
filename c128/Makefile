# llvm-mos, not cc65. MIGRATED 2026-08-23 -- see NOTES.md "llvm-mos on the
# C128". The short version: cc65 left 210 bytes of MAIN free and the disk seam
# needed 815, llvm-mos leaves about 6,000 with the seam already linked, and the
# charmap bug class that bit this project four separate times does not exist on
# this toolchain.
#
# -Oz, NOT -Os. Measured on this codebase: -Oz is 3,200 bytes smaller than -Os,
# which is most of the win. -flto adds nothing on top of either.
MOSCC = $(HOME)/llvm-mos/bin/mos-c128-clang
CFLAGS = -Oz -I src -I ../core -Wall -T trek128.ld

# -lcopy-data IS NOT OPTIONAL HERE, AND LEAVING IT OFF COST THE SOUND BUG.
#
# llvm-mos only initialises .data if the routine that copies it from its load
# address to its run address is LINKED IN, and the Commodore targets do not
# link it: their stock script aliases c_readonly and c_writeable to the same
# region, so .data's VMA equals its LMA and there is nothing to copy. The c128
# libcrt0.a leaves copy-data.c.obj out on exactly that reasoning.
#
# trek128.ld moves c_writeable to lowram (see the header there). That splits
# VMA from LMA -- and with no copy routine, .data was simply NEVER INITIALISED.
# The three bytes in it came up as whatever was in RAM. One of them was sid.c's
# `enabled`, which is why the port was silent: snd_poll() returns immediately
# on !enabled, so mus_base, mus_ok and the bank-1 note data were all correct
# and the driver never ran a tick. Proven by poking $1301 = 1 in VICE, which
# started the title tune mid-frame.
#
# THE FAILURE IS SILENT AND IT IS NOT LOCAL TO SOUND. Any `static x = 1;` in
# any file lands in .data. The linker does not warn, the map looks right --
# .data has a VMA, an LMA and a size -- and only reading the running machine
# shows the bytes never arrived. `make verify` now checks for the routine.
LDLIBS = -lcopy-data

SRC = src/main.c src/vdc.c src/layout.c src/ui.c src/input.c src/sid.c \
      src/music_data.c src/storage.c src/farmem.c src/overlay.c \
      src/strpool.c src/strdata.c \
      ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c
OUT = build/trek128.prg
DEBUG_OUT = build/trek128-debug.prg

# THE LINK EMITS AN ELF AND objcopy MAKES EVERYTHING ELSE OUT OF IT.
#
# trek128.ld used to carry an OUTPUT_FORMAT that wrote the PRG directly. The
# overlays ended that: each one has to come out as its own file, and they all
# share a run address, so what the link produces has to be something that can
# hold them separately. One link and two objcopy passes means the resident and
# the overlays can never disagree about where anything is -- which two links
# from the same sources could, silently.
#
# --no-check-sections: the overlays deliberately overlap in the address space.
# That is the whole idea, and without this lld refuses the link.
OBJCOPY = $(HOME)/llvm-mos/bin/llvm-objcopy
NM      = $(HOME)/llvm-mos/bin/llvm-nm
LDFLAGS = -Wl,--no-check-sections
# ONE LIST. The image-extraction rules below are generated from this by
# $(foreach) -- they used to be seven hand-written $(call)s, and adding an
# eighth overlay in 2026-08-27 built and linked cleanly while silently never
# producing ovlcmds.prg, so the d64 would have shipped without it. A list
# that only some of the build reads is a list that drifts.
OVERLAYS = eval hof front info repair msgs planet cmds title

# Each overlay ships as a PRG. The two-byte header is the load address, which
# ovl_load() discards by using secondary address 0 -- the window is where it
# goes, whatever the file says. $4000 is written so a human LOADing one by
# hand sees something sensible.
# $(1) = overlay name, $(2) = the ELF to cut it out of. NOT $$< -- $(call)
# expands once and the result is not re-expanded, so an automatic variable
# would arrive at objcopy as a literal dollar sign.
# READ OUT OF THE LINKER SCRIPT, NOT REPEATED HERE. This printed "window is
# 3328" for one build after trek128.ld went to 0x0F00, which reads as an
# overflow on an overlay that fits. One number, one home.
OVL_WINDOW := $(shell sed -n 's/.*window (rwx).*LENGTH = \(0x[0-9A-Fa-f]*\).*/\1/p' trek128.ld)

# $(1) = the ELF to cut every overlay out of.
# A SHELL LOOP OVER $(OVERLAYS), not one $(call) per overlay. It used to be
# seven hand-written calls; adding an eighth overlay linked cleanly and
# silently never produced ovlcmds.prg, so the d64 would have shipped without
# it. Rewriting it as $(foreach) over a multi-line define did not work
# either -- foreach joins expansions with a SPACE, which breaks the tab that
# must start the next recipe line, and exactly one objcopy went missing. Worse,
# the failure hid: a stale .raw from a hand-run objcopy made the build pass.
# This is the idiom the d64 rule below already uses, and it reads one list.
define OVL_IMAGES
	@for o in $(OVERLAYS); do \
	    $(OBJCOPY) --dump-section=.ovl_$$o=build/ovl_$$o.raw $(1) /dev/null || exit 1; \
	    python3 -c "import sys; \
	        n = sys.argv[1]; \
	        d = open('build/ovl_%s.raw' % n, 'rb').read(); \
	        w = int('$(OVL_WINDOW)', 16); \
	        open('build/ovl%s.prg' % n, 'wb').write(bytes([0x00,0x40]) + d); \
	        print('  overlay %s: %d bytes, window is %d' % (n, len(d), w)); \
	        sys.exit('overlay %s does not fit the window' % n) if len(d) > w else None" $$o || exit 1; \
	done
endef

.PHONY: all run rund d64 monitor test verify music loadbench clean

all: $(OUT)

# The note data is Nels Anderson's, so it is generated rather than committed --
# see tools/gen_music.py. On a clone without reference/ this still writes a
# valid header with empty tracks, so the build never depends on gitignored
# material being present.
music src/music_data.h src/music_data.c:
	cd .. && python3 tools/gen_music.py

# The prose. Unlike the music this list IS committed -- it is the port's own
# wording, not Anderson's -- so a clone with no reference/ still gets a game
# with words in it.
strings src/strdata.h src/strdata.c:
	cd .. && python3 tools/gen_strings.py

# The EGA->VDC colour mapping is pure arithmetic, so it is proven natively on
# the build machine rather than by squinting at an emulator. Getting it wrong
# is silent -- every colour still renders, just as the wrong hue.
test:
	cc -Wall -Wextra -o build/test_egavdc test/test_egavdc.c
	./build/test_egavdc
	cc -Wall -Wextra -o build/test_panels test/test_panels.c src/ui.c \
	   src/layout.c ../core/trek.c ../core/planet.c ../core/hof.c \
	   ../core/serial.c
	./build/test_panels
	cc -Wall -Wextra -o build/test_sid test/test_sid.c -lm
	./build/test_sid

# Inspects the LINKED binary, which is the only place cc65's character-set
# translation is visible. Native cc does no such translation, so `make test`
# above passes whether or not the encoding is right -- three bugs in this port
# proved that the hard way. `run` depends on this, so a binary with a PETSCII
# key table can never reach the emulator.
verify: $(OUT)
	@# LOWRAM HEADROOM. Added 2026-08-27 after the save record was packed and
	@# the 192 bytes it freed turned out to land in a pool NOBODY WAS
	@# WATCHING: writable data lives in lowram, 0x1300..0x1C00, and it had 77
	@# bytes left. Every other pool in this build is reported by name every
	@# time; this one was invisible because it is a linker region and not an
	@# overlay. It is not any more.
	@python3 -c "import subprocess,sys; \
	    o = subprocess.run(['$(NM)','--radix=d','build/trek128.elf'], \
	                       capture_output=True, text=True).stdout; \
	    e = [int(l.split()[0]) for l in o.splitlines() if l.split()[1:2] == ['B'] \
	         and l.split()[-1] == '__bss_end'][0]; \
	    free = 0x1C00 - e; \
	    print('verify: lowram (writable data) %d of 2304 used, %d free' % (e - 0x1300, free)); \
	    sys.exit('lowram is FULL -- writable data has overrun 0x1C00') if free < 0 else None"
	python3 tools/verify_prg.py

HDR = src/vdc.h src/layout.h src/egavdc.h src/ui.h src/input.h \
      src/sid.h src/sidfreq.h src/music_data.h \
      ../core/ega.h ../core/trek.h ../core/hof.h ../core/storage.h \
      ../core/farmem.h ../core/strpool.h src/strdata.h \
      ../core/serial.h

# -m emits a map file. That is not housekeeping: tools/vice_mon.py can write
# guest memory, but only if it knows where a variable lives, and the map is
# the only place that says. Poking ship state and screenshotting the result is
# the fastest way to check a panel without playing to that state.
build/trek128.elf: $(SRC) $(HDR) trek128.ld Makefile
	@mkdir -p build
	$(MOSCC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=build/trek128.map \
	    -o $@ $(SRC) $(LDLIBS)

# The resident image: everything except the overlay sections, with the load
# address in front of it. `-O binary` over this ELF was checked byte for byte
# against what the old OUTPUT_FORMAT produced.
$(OUT): build/trek128.elf
	$(OBJCOPY) -O binary --remove-section=.ovl_* $< build/trek128.res
	@python3 -c "d = open('build/trek128.res','rb').read(); \
	    open('$(OUT)','wb').write(bytes([0x01,0x1C]) + d)"
	$(call OVL_IMAGES,build/trek128.elf)

# A SEPARATE binary, deliberately. TREK_DEBUG_INPUT compiles in a byte that a
# script can poke to fake a keypress (see src/input.h), and the release build
# must not have it. Separate output means `make run` can never accidentally
# launch the instrumented one.
$(DEBUG_OUT): $(SRC) $(HDR) trek128.ld Makefile
	@mkdir -p build
	$(MOSCC) $(CFLAGS) $(LDFLAGS) -DTREK_DEBUG_INPUT \
	     -Wl,-Map=build/trek128-debug.map -o build/trek128-debug.elf $(SRC) $(LDLIBS)
	$(OBJCOPY) -O binary --remove-section=.ovl_* build/trek128-debug.elf build/trek128-debug.res
	@python3 -c "d = open('build/trek128-debug.res','rb').read(); \
	    open('$(DEBUG_OUT)','wb').write(bytes([0x01,0x1C]) + d)"
	@# The instrumented build's overlays differ from the release build's, so
	@# they are cut from its own ELF. A debug disk needs these, not those.
	$(call OVL_IMAGES,build/trek128-debug.elf)

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
	@# Bulk data, LOADed into bank 1 at startup. WRITTEN AS PRG, NOT SEQ,
	@# and the two-byte header is why: far_load() uses the KERNAL's LOAD
	@# with secondary address 0, which reads and discards a PRG's load
	@# address and puts the rest where it is told. That is eight to eighteen
	@# times faster than the byte-at-a-time read this replaced -- see
	@# c128/test/loadbench.c and NOTES.md. The header value is ignored, so
	@# $4000 is written only so a human LOADing the file by hand sees
	@# something sensible.
	@# STRINGS.DAT always exists; MUSIC.DAT only on a tree that has
	@# reference/, and without it the game simply plays silently.
	@python3 -c "import sys; d = open('build/strings.dat','rb').read(); \
	    open('build/strings.pdat','wb').write(bytes([0x00,0x40]) + d)"
	c1541 -attach $(D64) -write build/strings.pdat strings.dat >/dev/null
	@if [ -f build/music.dat ]; then \
	    python3 -c "import sys; d = open('build/music.dat','rb').read(); \
	        open('build/music.pdat','wb').write(bytes([0x00,0x40]) + d)"; \
	    c1541 -attach $(D64) -write build/music.pdat music.dat >/dev/null; \
	else \
	    echo "d64: NO MUSIC.DAT -- the game will play SILENTLY."; \
	    echo "     run \`make music\` (needs reference/) to generate it."; \
	fi
	@# The overlays, one PRG each. ovl_load() LOADs them into the window.
	@for o in $(OVERLAYS); do \
	    c1541 -attach $(D64) -write build/ovl$$o.prg ovl$$o >/dev/null; \
	done
	@echo "d64: $(D64)"
	@c1541 -attach $(D64) -dir

# Which way of reading a file is faster: LOAD, or the CHRIN loop the disk
# seam uses? Answer on 2026-08-26: LOAD, by 8.2x. See test/loadbench.c, and
# NOTES.md "LOAD against the CHRIN loop". Not part of the game.
loadbench: test/loadbench.c
	@mkdir -p build
	@# NO -T trek128.ld here: that script puts writable data in lowram, and
	@# a 4K buffer does not fit there. The stock script is what this wants.
	$(MOSCC) -Oz -I src -I ../core -Wall -Wl,-Map=build/loadbench.map \
	    -o build/loadbench.prg test/loadbench.c
	@python3 -c "p = bytes((i*7 + i//251) & 0xFF for i in range(4096)); \
	    open('build/bench.seq','wb').write(p); \
	    open('build/bench.prg','wb').write(bytes([0,0x40]) + p)"
	rm -f build/bench.d64
	c1541 -format "loadbench,01" d64 build/bench.d64 >/dev/null
	c1541 -attach build/bench.d64 -write build/loadbench.prg loadbench >/dev/null
	c1541 -attach build/bench.d64 -write build/bench.seq bench,s >/dev/null
	c1541 -attach build/bench.d64 -write build/bench.prg benchp >/dev/null
	@echo "run:  x128 -binarymonitor -binarymonitoraddress ip4://127.0.0.1:6502 \
-autostart build/bench.d64"
	@echo "then read the two jiffy counts from \`result\` in build/loadbench.map"

rund: $(D64)
	x128 -autostart $(D64)

clean:
	rm -f build/*.prg build/test_egavdc build/test_panels build/test_sid src/*.o src/music_data.h src/music_data.c
