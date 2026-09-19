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
# The HOST compiler, for the one check that is arithmetic rather than hardware.
HOSTCC  = cc
ROMS    = $(HOME)/Library/Application Support/Ample/roms
# -window and -skip_gameinfo always; -nothrottle is right for a measurement
# and wrong for anything a person watches.
MAMEOPT = -rompath "$(ROMS)" -window -skip_gameinfo -nothrottle -sound none \
          -autoboot_delay 0

PROBES  = gsprobe gsmap gsfill1 gsdiv gsram

# ---------------------------------------------------------------- THE GAME
# Everything but sound is real. src/gsstub.c still supplies snd_*, behind
# -DGS_REAL_* switches that say exactly which seams are finished -- so the
# size measurement and the game are the same link with different flags rather
# than two things that can drift apart.
GAME_SRC = ../c128/src/main.c ../c128/src/layout40.c ../c128/src/ui.c \
           ../c128/src/strpool.c ../c128/src/music_data.c \
           ../core/trek.c ../core/planet.c \
           ../core/hof.c ../core/serial.c \
           src/gsstub.c src/gsvid.c src/gsblk.c src/gsfar.c src/gsovl.c \
           src/gskey.c src/gssnd.c
GAME_FLAGS = -mcpu=mosw65816 -Oz -I src -I ../c128/src -I ../core \
             -DTREK_40COL -DTREK_OVERLAYS -DTREK_COLOUR_IS_EGA \
             -DGS_REAL_VIDEO -DGS_REAL_STORAGE -DGS_REAL_FAR \
             -DGS_REAL_OVL -DGS_REAL_KEY -DGS_REAL_SOUND -T iigs.ld
OVL_NAMES = eval hof front info repair msgs planet cmds title events xtra

# music_data.c IS GENERATED, so the game cannot be linked before make_music.py
# has run. Named as a prerequisite because it was not, and the first build with
# sound in it failed on `undefined symbol: mus_offset` -- while `make game`'s
# output was being piped to /dev/null, so the run that followed recorded a
# STALE disk and reported silence. Check the exit status before the output.
../c128/src/music_data.c: ../tools/make_music.py
	cd .. && python3 tools/make_music.py >/dev/null

build/trek.elf: $(GAME_SRC) ../c128/src/music_data.c iigs.ld Makefile | build
	$(CC) $(GAME_FLAGS) $(LDFLAGS) -Wl,-Map=build/trek.map $(GAME_SRC) \
	  -o $@ $(LDLIBS)

# THE STAGING REGIONS MUST COME OUT. The overlay images are linked at $10000
# upward so each gets its own AT> region; objcopy -O binary over the whole ELF
# writes all eleven of them into the file and the image comes out 106,401
# bytes instead of 38,395. They are cut separately by tools/cut_overlays.py.
build/trek.bin: build/trek.elf
	$(OBJCOPY) -O binary \
	  $(foreach o,$(OVL_NAMES),-R .ovl_$(o)) build/trek.elf $@

build/OVERLAYS.BIN: build/trek.elf tools/cut_overlays.py
	python3 tools/cut_overlays.py build/trek.elf $@

build/STRINGS.DAT: ../c128/src/strings.txt src/strings.override.txt ../tools/gen_strings.py | build
	cd .. && python3 tools/gen_strings.py >/dev/null
	cd .. && python3 tools/gen_strings.py --override iigs/src/strings.override.txt \
	    iigs/build/STRINGS.DAT

build/MUSIC.DAT: ../tools/make_music.py | build
	cd .. && python3 tools/make_music.py >/dev/null
	cp ../c128/build/music.dat $@
	@cmp ../c128/build/music.dat $@

# BRIEF.TXT IS A FILE THE GAME OPENS AND THE BUILD HAS TO PUT THERE, and the
# first disk did not. ui_briefing() says what happens then, in its own comment:
# "NO FILE, NO BRIEFING. A disk without BRIEF.TXT skips it silently and the
# game starts." So the briefing prompt answered Y and nothing happened -- no
# error, no blank page, nothing -- and every gate in this directory passed,
# because not one of them ever answered that prompt. Jamie found it in three
# minutes of playing.
#
# briefing40.txt, not briefing.txt: the 40-column ports ship the reflowed one.
build/trek.po: build/boot.bin build/trek.bin build/OVERLAYS.BIN \
               build/STRINGS.DAT build/MUSIC.DAT \
               ../c128/src/briefing40.txt tools/mkdisk.py
	python3 tools/mkdisk.py $@ --boot build/boot.bin \
	  --payload build/trek.bin \
	  --file STRINGS.DAT=build/STRINGS.DAT \
	  --file MUSIC.DAT=build/MUSIC.DAT \
	  --file BRIEF.TXT=../c128/src/briefing40.txt \
	  --file OVERLAYS.BIN=build/OVERLAYS.BIN --slots 4

game: build/trek.po
	python3 tools/verify_gs.py build/trek.elf --window 0xD000 --org 0x0800

# THE BRIEFING, ANSWERED. Not one of the other gates ever pressed Y at that
# prompt, which is why a disk with no BRIEF.TXT on it passed all fifteen of
# them -- ui_briefing() skips silently when the file is missing, so the failure
# had no symptom to see. Jamie found it in three minutes.
briefing: build/trek.po
	mkdir -p build/snap/apple2gs && rm -f build/snap/apple2gs/*.png
	GS_WAIT=26 GS_KEYS="Return,y,Return" GS_STEP=8 $(MAME) apple2gs \
	  $(MAMEOPT) -flop3 build/trek.po -snapshot_directory build/snap \
	  -autoboot_script tools/play.lua -seconds_to_run 120
	python3 tools/check_briefing.py build/snap/apple2gs/0003.png

# ARM-CHECK: the same disk WITHOUT BRIEF.TXT, which is the bug exactly.
# Inverted -- a PASS is the failure.
build/nobrief.po: build/boot.bin build/trek.bin build/OVERLAYS.BIN \
                  build/STRINGS.DAT build/MUSIC.DAT tools/mkdisk.py
	python3 tools/mkdisk.py $@ --boot build/boot.bin \
	  --payload build/trek.bin --file STRINGS.DAT=build/STRINGS.DAT \
	  --file MUSIC.DAT=build/MUSIC.DAT \
	  --file OVERLAYS.BIN=build/OVERLAYS.BIN --slots 4

brieffail: build/nobrief.po
	mkdir -p build/snap/apple2gs && rm -f build/snap/apple2gs/*.png
	@GS_WAIT=26 GS_KEYS="Return,y,Return" GS_STEP=8 $(MAME) apple2gs \
	  $(MAMEOPT) -flop3 build/nobrief.po -snapshot_directory build/snap \
	  -autoboot_script tools/play.lua -seconds_to_run 120 > /dev/null 2>&1; \
	if python3 tools/check_briefing.py build/snap/apple2gs/0003.png \
	    > build/brieffail.log 2>&1; then \
	  echo "ARM-CHECK FAILED: check_briefing passed a disk with no BRIEF.TXT"; \
	  exit 1; \
	else echo "arm-check ok: check_briefing rejected a disk with no BRIEF.TXT"; fi

# ---------------------------------------------------------------- release
#
# `verify` FIRST, and that is the same wiring the MEGA65 and the C64 use: a
# binary that cannot pass the gates can never reach a disk. It is also the
# thing that catches a STRINGS.DAT built against a different STR_COUNT, which
# blanks every label in the game and complains about nothing.
REL = build/release

release: verify build/trek.po
	@rm -rf $(REL) && mkdir -p $(REL)/egatrek-iigs
	@cp build/trek.po $(REL)/egatrek-iigs/EGATREK.PO
	@cp README-release.txt $(REL)/egatrek-iigs/README.txt
	@cd $(REL) && zip -q -r ../egatrek-iigs.zip egatrek-iigs
	@echo "release: build/egatrek-iigs.zip"
	@unzip -l build/egatrek-iigs.zip | tail -n +4

# FOR A HUMAN. No -nothrottle, sound ON, a real window, no -seconds_to_run and
# no autoboot script -- every one of those is right for a measurement and
# wrong for somebody playing. `-nothrottle` in particular has fooled this
# project before: the CoCo 3's sound was reported broken for an afternoon
# because every session ran at 1,200% and the tune was a chirp.
#
# IT LEAVES THE MACHINE RUNNING. That is the point of this target; `make play`
# is the one that drives itself and exits.
run: build/trek.po
	$(MAME) apple2gs -rompath "$(ROMS)" -window -skip_gameinfo \
	  -flop3 build/trek.po

# Boot the game and drive it. GS_KEYS is a comma-separated list of ioport
# field names -- "Return", "Space", "n", "1" -- because this machine's
# keyboard is an ADB keyboard and neither natkeyboard:post_coded() nor a poke
# at $C000 reaches it. See tools/play.lua.
play: build/trek.po
	mkdir -p build/snap/apple2gs && rm -f build/snap/apple2gs/*.png
	GS_WAIT=$${GS_WAIT:-22} GS_KEYS="$${GS_KEYS:-Return}" \
	  GS_STEP=$${GS_STEP:-8} $(MAME) apple2gs $(MAMEOPT) \
	  -flop3 build/trek.po -snapshot_directory build/snap \
	  -autoboot_script tools/play.lua -seconds_to_run $${GS_SECS:-140}


all: $(PROBES:%=build/%.bin) build/trek.po

build/%.elf: src/%.c iigs.ld Makefile | build
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
build/boot.bin: src/boot.S boot.ld Makefile | build
	$(CC) -mcpu=mosw65816 -nostdlib -T boot.ld -Wl,--no-check-sections \
	  src/boot.S -o build/boot.elf
	$(OBJCOPY) -O binary build/boot.elf $@

# The same block, asking for a block one past the end of the disk -- the only
# way to make the drive say no, and therefore the only way to execute the
# failed-read path at all.
build/bootbad.bin: src/boot.S boot.ld Makefile | build
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

# WHAT MEMORY IS THERE, and the answer that matters is not the total. Run it
# at both ends of what MAME will emulate: anything whose reading MOVES with
# -ramsize is expansion RAM this project cannot rely on, because MAME's
# smallest apple2gs is 1M and a stock 256K machine is not testable here.
ram: build/gsram.bin
	@for sz in 1M 8M; do \
	  printf "ramsize %-3s  " $$sz; \
	  GS_BIN=build/gsram.bin GS_WAIT=6 \
	    GS_DONE=$$($(NM) build/gsram.elf | awk '/ gs_done$$/{print "0x"$$1}') \
	    GS_DUMP="$$($(NM) build/gsram.elf | awk '/ bank_ok$$/{print "0x"$$1}'):3;$$($(NM) build/gsram.elf | awk '/ lc_result$$/{print "0x"$$1}'):7" \
	    $(MAME) apple2gs $(MAMEOPT) -ramsize $$sz -video none \
	    -autoboot_script tools/run.lua -seconds_to_run 25 2>&1 \
	    | grep -E "^DUMP|NEVER" | tr '\n' '|'; echo; \
	done

# SOUND: the Ensoniq 5503, two oscillators, CALIBRATED AT THREE POINTS.
# src/gssndt.c plays four tones through the SHIPPING driver's own voice_note
# -- a test that reimplements the frequency arithmetic proves the test -- and
# hearit.py reads the pitch back out of the recording. 440 on voice 1 is the
# fourth, because "two voices" is a claim and a second oscillator addressed
# independently is the evidence.
build/gssndt.bin: src/gssndt.c src/gssnd.c src/gsfar.c src/gsblk.c iigs.ld Makefile | build
	$(CC) -mcpu=mosw65816 -Oz -I src -I ../c128/src -I ../core -DTREK_40COL \
	  -DGS_SNDTEST -T iigs.ld $(LDFLAGS) src/gssndt.c src/gssnd.c \
	  src/gsfar.c src/gsblk.c -o build/gssndt.elf $(LDLIBS)
	$(OBJCOPY) -O binary build/gssndt.elf $@

build/sndt.po: build/boot.bin build/gssndt.bin tools/mkdisk.py
	python3 tools/mkdisk.py $@ --boot build/boot.bin --payload build/gssndt.bin

sound: build/sndt.po
	rm -f build/sndt.wav
	$(MAME) apple2gs $(MAMEOPT) -video none -flop3 build/sndt.po \
	  -wavwrite build/sndt.wav -autoboot_script tools/bootrun.lua \
	  -seconds_to_run 50
	python3 ../coco3gime/tools/hearit.py build/sndt.wav 440 1000 200 440

# ARM-CHECK: the pitch gate must reject an octave. Inverted -- a PASS is the
# failure, and an octave is exactly the error this project shipped twice.
soundfail: build/sndt.po
	@if python3 ../coco3gime/tools/hearit.py build/sndt.wav 880 2000 400 880 \
	    > build/soundfail.log 2>&1; then \
	  echo "ARM-CHECK FAILED: hearit passed pitches an octave out"; exit 1; \
	else echo "arm-check ok: hearit rejected pitches an octave out"; fi

# AND THAT THE TUNE ADVANCES, which `sound` cannot see: it settles whether the
# pitches are RIGHT, and a stuck note is right at one pitch forever. Boot the
# real game, record it, and slice the burst -- hearit reports a working tune
# and a held note identically, as "441.0 Hz for 14.6 seconds".
build/game.wav: build/trek.po
	rm -f $@
	mkdir -p build/snap/apple2gs
	GS_WAIT=26 GS_KEYS=Return GS_STEP=10 $(MAME) apple2gs $(MAMEOPT) \
	  -flop3 build/trek.po -wavwrite $@ -snapshot_directory build/snap \
	  -autoboot_script tools/play.lua -seconds_to_run 70

music: build/game.wav
	python3 tools/check_music.py build/game.wav

# ARM-CHECK: a synthesised steady tone is what a stuck note sounds like and
# what looked like success the first time. Inverted -- a PASS is the failure.
build/stuck.wav: | build
	@python3 -c "import array,math,wave; r=48000; \
	  a=array.array('h',(int(12000*math.sin(2*math.pi*440*t/r)) for t in range(r*15))); \
	  w=wave.open('build/stuck.wav','wb'); w.setnchannels(1); w.setsampwidth(2); \
	  w.setframerate(r); w.writeframes(a.tobytes()); w.close()"

musicfail: build/stuck.wav
	@if python3 tools/check_music.py build/stuck.wav > build/musicfail.log 2>&1; \
	then echo "ARM-CHECK FAILED: check_music passed a single held note"; exit 1; \
	else echo "arm-check ok: check_music rejected a single held note"; fi

# THE CONSOLE. gsvid.c draws c128/src/vdc.h's 40x25 grid into Super Hi-Res,
# and gssmoke.c is the proof sheet: every text code, the box set, a drawn
# panel, sixteen colours and reverse video, on a real machine. The host-side
# sheet coco3/tools/gen_font.py renders is the other half -- a font that looks
# right there and wrong here is a bug in gsvid.c, and one picture cannot tell.
VIDFLAGS = -mcpu=mosw65816 -Oz -I src -I ../c128/src -I ../core \
           -DTREK_40COL -DTREK_COLOUR_IS_EGA -T iigs.ld

build/gssmoke.bin: src/gssmoke.c src/gsvid.c iigs.ld Makefile | build
	$(CC) $(VIDFLAGS) $(LDFLAGS) src/gssmoke.c src/gsvid.c \
	  -o build/gssmoke.elf $(LDLIBS)
	$(OBJCOPY) -O binary build/gssmoke.elf $@

# The same sheet with the box glyphs SHIFTED like the text instead of authored
# at eight wide -- the obvious wrong answer, kept buildable so the gate can be
# shown to catch it.
build/gssmokebad.bin: src/gssmoke.c src/gsvid.c iigs.ld Makefile | build
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

# FILES, WITH NO PRODOS ON THE DISK. gsblk.c is core/storage.h's five plat_*
# over the slot firmware's block driver plus this port's own directory --
# mkdisk.py's format, which is atari/src/atarisio.c's at 512 bytes.
build/gsfile.bin: src/gsfile.c src/gsblk.c iigs.ld Makefile | build
	$(CC) -mcpu=mosw65816 -Oz -I ../core -T iigs.ld $(LDFLAGS) \
	  src/gsfile.c src/gsblk.c -o build/gsfile.elf $(LDLIBS)
	$(OBJCOPY) -O binary build/gsfile.elf $@

build/testdata.dat:
	@mkdir -p build
	@printf 'HELLO FROM A FILE ON OUR OWN DISK.' > $@

build/file.po: build/boot.bin build/gsfile.bin build/testdata.dat tools/mkdisk.py
	python3 tools/mkdisk.py $@ --boot build/boot.bin \
	  --payload build/gsfile.bin --file TEST.DAT=build/testdata.dat --slots 2

GSFILE_REP = $$($(NM) build/gsfile.elf | awk '/ rep$$/{print "0x"$$1}')
GSFILE_DONE = $$($(NM) build/gsfile.elf | awk '/ gs_done$$/{print "0x"$$1}')

files: build/file.po
	@GS_DONE=$(GSFILE_DONE) GS_REP=$(GSFILE_REP) \
	  $(MAME) apple2gs $(MAMEOPT) -video none -flop3 build/file.po \
	  -autoboot_script tools/filerun.lua -seconds_to_run 30 2>&1 \
	  | python3 tools/check_files.py

# ARM-CHECK: poke the same image in instead of BOOTING it, so src/boot.S never
# runs and the handoff signature is absent. Everything must fail cleanly and
# nothing must hang -- a jmp through a driver address the ROM happened to
# leave behind is the failure the signature exists to prevent. Inverted,
# because here a PASS is the failure.
filesfail: build/gsfile.bin
	@GS_BIN=build/gsfile.bin GS_WAIT=8 GS_DONE=$(GSFILE_DONE) \
	  GS_DUMP="$(GSFILE_REP):48" \
	  $(MAME) apple2gs $(MAMEOPT) -video none \
	  -autoboot_script tools/run.lua -seconds_to_run 25 2>&1 \
	  | sed 's/^DUMP [0-9A-F]*+48\*1 /REP /' \
	  | python3 tools/check_files.py > build/filesfail.log 2>&1; \
	if [ $$? -eq 0 ]; then \
	  echo "ARM-CHECK FAILED: check_files passed with no boot handoff"; \
	  exit 1; \
	else \
	  grep -q "handoff signature was valid: rep\[2\] is .00" build/filesfail.log \
	    && echo "arm-check ok: no handoff detected, every file call refused, nothing hung" \
	    || { echo "ARM-CHECK INCONCLUSIVE: it failed for some other reason"; \
	         cat build/filesfail.log; exit 1; }; \
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

build/early.elf: $(SHARED) src/gsstub.c iigs.ld Makefile | build
	$(CC) $(EARLYFLAGS) $(LDFLAGS) -Wl,-Map=build/early.map \
	  $(SHARED) src/gsstub.c -o $@ $(LDLIBS)

build/earlyfar.elf: $(SHARED) src/gsstub.c src/gsvid.c src/gsblk.c src/gsfar.c iigs.ld Makefile | build
	$(CC) $(EARLYFLAGS) -DTREK_COLOUR_IS_EGA -DGS_REAL_VIDEO \
	  -DGS_REAL_STORAGE -DGS_REAL_FAR $(LDFLAGS) \
	  -Wl,-Map=build/earlyfar.map $(SHARED) src/gsstub.c src/gsvid.c \
	  src/gsblk.c src/gsfar.c -o $@ $(LDLIBS)

build/earlyfull.elf: $(SHARED) src/gsstub.c src/gsvid.c src/gsblk.c iigs.ld Makefile | build
	$(CC) $(EARLYFLAGS) -DTREK_COLOUR_IS_EGA -DGS_REAL_VIDEO \
	  -DGS_REAL_STORAGE $(LDFLAGS) -Wl,-Map=build/earlyfull.map \
	  $(SHARED) src/gsstub.c src/gsvid.c src/gsblk.c -o $@ $(LDLIBS)

build/earlyvid.elf: $(SHARED) src/gsstub.c src/gsvid.c iigs.ld Makefile | build
	$(CC) $(EARLYFLAGS) -DTREK_COLOUR_IS_EGA -DGS_REAL_VIDEO $(LDFLAGS) \
	  -Wl,-Map=build/earlyvid.map $(SHARED) src/gsstub.c src/gsvid.c \
	  -o $@ $(LDLIBS)

early: build/early.elf build/earlyvid.elf build/earlyfull.elf build/earlyfar.elf
	@echo "-- seam entirely stubbed"
	python3 tools/verify_gs.py build/early.elf --window 0xD000 --org 0x0800
	@echo "-- with the REAL video driver"
	python3 tools/verify_gs.py build/earlyvid.elf --window 0xD000 --org 0x0800
	@echo "-- with the REAL video driver AND the real storage seam"
	python3 tools/verify_gs.py build/earlyfull.elf --window 0xD000 --org 0x0800
	@echo "-- and far memory too: the log and the pool leave bank 0"
	python3 tools/verify_gs.py build/earlyfar.elf --window 0xD000 --org 0x0800

# TWO LEVELS, AND THE SPLIT IS NOT COSMETIC. `make ports` runs every port's
# `verify` from the repository root, so a verify that boots MAME seventeen
# times turns a cross-port gate that takes seconds into one that takes an
# hour -- and a gate nobody runs is not a gate. Every other port's verify is
# static, and so is this one now.
#
#   verify   builds the game and checks the link. Seconds. What `make ports`
#            and `make release` depend on.
#   gates    the whole suite on real hardware-in-emulation: colours, the boot
#            chain, files, the proof sheet, sound, the tune, the briefing, and
#            the six arm-checks that prove those can fail. Minutes.
verify: build/trek.elf build/test_gsregion
	python3 ../c128/tools/layout40_check.py
	python3 tools/verify_gs.py build/trek.elf --window 0xD000 --org 0x0800
	@./build/test_gsregion

# THE 50Hz BRANCH SHIPS WITHOUT EVER RUNNING ON A MACHINE HERE. MAME has no
# 50Hz IIgs -- every apple2gs clone is a ROM revision -- so the region
# classifier's PAL path cannot be reached by any emulator run. It is pure
# uint16_t arithmetic, so it is tested natively instead, against the header the
# driver itself includes rather than a copy. Cheap, static, and it belongs in
# `verify` rather than `gates` because it needs no emulator.
build/test_gsregion: tools/test_gsregion.c src/gsregion.h Makefile | build
	$(HOSTCC) -O1 -o $@ $<

gates: colours bootcheck failcheck early sheet sheetfail files filesfail sound soundfail game music musicfail briefing brieffail

clean:
	rm -rf build

.PHONY: all colours pagemap disk bootcheck failcheck early sheet sheetfail ram files filesfail sound soundfail game play run music musicfail briefing brieffail release verify gates clean

.PRECIOUS: build/%.elf
