# EGA Trek for the MEGA65, native C65 mode.
#
# NATIVE MODE IS NOT A PREFERENCE, IT IS THE WHOLE POINT. A $0801 binary puts
# the machine in C64 mode where 80 columns do not exist. Linking at $2001 with
# a BASIC 65 header selects C65/native mode, where VIC-IV's H640 is real and
# colour RAM is the full 2000 cells rather than the 1K window at $D800.
# llvm-mos's mega65 target does that for us.

# STATED, NOT POSITIONAL. make's default goal is whatever target it reads
# first, and this file grew a smoke harness above `all:` -- so a bare `make`
# here built build/smoke2.prg and not the game, for as long as the harness has
# existed. Nobody noticed because every command in the README says `make all`.
.DEFAULT_GOAL := all

LLVM_MOS  ?= $(HOME)/llvm-mos
M65CC      = $(LLVM_MOS)/bin/mos-mega65-clang

# mega65-libc supplies the VIC-IV conio. Built for llvm-mos, NOT cc65 -- the
# two artifacts are not interchangeable. The commodore-uno tree already has
# one; point MEGA65_LIBC elsewhere to use your own.
MEGA65_LIBC ?= $(HOME)/claude-code/commodore-uno/mega65/mega65-libc-llvm
LIBC_INC     = $(MEGA65_LIBC)/include
LIBC_A       = $(MEGA65_LIBC)/build/src/libmega65libc.a

XMEGA65 ?= $(HOME)/xemu/bin/xmega65
M65ROM  ?= $(HOME)/Library/Application Support/xemu-lgb/mega65/MEGA65.ROM

# THE SHARED HALF. ui.c and main.c are 4,500 lines of console and command
# handling that both ports use; strpool.c is shared too, because it reaches the
# string pool through core/farmem.h and this port supplies that. Only the
# hardware files below are the MEGA65's own.
SHARED = ../c128/src/ui.c ../c128/src/main.c ../c128/src/layout.c \
         ../c128/src/strpool.c \
         ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c
PLATFORM = src/m65vid.c src/m65input.c src/m65snd.c src/m65storage.c \
           src/m65mem.c src/music_data.c
SRC = $(SHARED) $(PLATFORM)
OUT = build/egatrek.prg

# TREK_COLOUR_IS_EGA makes EGA_TO_VDC the identity -- see c128/src/egavdc.h.
# The VIC-IV palette is loaded with EGA's own values, so no conversion is
# needed or wanted.
CFLAGS = -Wall -Werror -Oz -DTREK_COLOUR_IS_EGA -DTREK_OVERLAYS \
         -I $(LIBC_INC) -I src -I ../c128/src -I ../core
LDFLAGS = -T mega65.ld -Wl,--no-check-sections
OBJCOPY = $(LLVM_MOS)/bin/llvm-objcopy
OVERLAYS = eval hof front info repair msgs planet cmds title events xtra

SMOKE2_SRC = src/smoke2.c src/m65vid.c ../c128/src/layout.c ../c128/src/strpool.c \
             src/m65mem.c src/m65storage.c src/m65snd.c src/music_data.c \
             ../c128/src/ui.c ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c src/m65input.c
SMOKE2_OUT = build/smoke2.prg

smoke2: $(SMOKE2_OUT)

# THE OBJCOPY STEP IS NOT OPTIONAL. mega65.ld emits an ELF on purpose (so the
# overlay images can be cut out of it), so linking straight to a .prg produces
# an ELF that Xemu loads at $457F -- where it is not recognised as BASIC, so it
# is never auto-run and the screen shows a SYS line instead. An hour went into
# a "black screen" that was this.
$(SMOKE2_OUT): $(SMOKE2_SRC) src/m65vid.h mega65.ld
	@mkdir -p build
	$(M65CC) $(CFLAGS) $(LDFLAGS) -o build/smoke2.elf $(SMOKE2_SRC) $(LIBC_A)
	$(OBJCOPY) -O binary --remove-section=.ovl_* build/smoke2.elf build/smoke2.res
	@python3 -c "d=open('build/smoke2.res','rb').read(); \
	    open('$(SMOKE2_OUT)','wb').write(bytes([0x01,0x20]) + d)"

SMOKE_SRC = src/smoke.c src/m65vid.c ../c128/src/layout.c
SMOKE_OUT = build/smoke.prg

.PHONY: all debug smoke smoke2 run run-smoke drive music disk sd verify clean

all: $(OUT)

music src/music_data.c src/music_data.h:
	cd .. && python3 tools/make_music.py && cp c128/src/music_data.c c128/src/music_data.h mega65/src/

$(OUT): $(SRC) src/m65vid.h src/m65input.h src/m65snd.h mega65.ld
	@mkdir -p build
	$(M65CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=build/egatrek.map \
	    -o build/egatrek.elf $(SRC) $(LIBC_A)
	@# The resident half becomes the PRG; the ten window images are cut out
	@# separately and concatenated into OVERLAYS.BIN, which m65mem.c reads
	@# into banked RAM at startup.
	$(OBJCOPY) -O binary --remove-section=.ovl_* build/egatrek.elf build/egatrek.res
	@python3 -c "d=open('build/egatrek.res','rb').read(); \
	    open('$(OUT)','wb').write(bytes([0x01,0x20]) + d)"
	@rm -f build/OVERLAYS.BIN
	@for o in $(OVERLAYS); do \
	    $(OBJCOPY) -O binary --only-section=.ovl_$$o build/egatrek.elf build/ovl$$o.bin; \
	    python3 -c "import sys;d=open('build/ovl$$o.bin','rb').read(); \
	        open('build/OVERLAYS.BIN','ab').write(d + b'\\0'*(4096-len(d)))"; \
	done
	@# THE STAMP THAT TIES THE TWO FILES TOGETHER -- see m65mem.c. ovl_load's
	@# address comes from this link, so images and program can only agree if
	@# they were built together. Written into the last overlay's padding.
	@python3 -c "import subprocess,os; \
	    nm=subprocess.check_output(['$(LLVM_MOS)/bin/llvm-nm','build/egatrek.elf']).decode(); \
	    a=[int(l.split()[0],16) for l in nm.splitlines() if l.split()[-1]=='ovl_load'][0]; \
	    f=open('build/OVERLAYS.BIN','r+b'); f.seek(-2,2); \
	    f.write(bytes([a & 0xFF, (a >> 8) & 0xFF])); f.close(); \
	    print('mega65: overlay stamp', hex(a))"
	@# THE LIMIT COMES FROM mega65.ld, not from a number typed here. It used
	@# to say 40959 while the linker script's `ram` region was 40447, so every
	@# free-space figure this port ever printed was 512 bytes too generous --
	@# and free space is exactly the thing those figures get consulted for.
	@python3 -c "import os,re; \
	    n = int(re.search(r'ram \(rw\) : ORIGIN = 0x[0-9a-f]+, LENGTH = (0x[0-9a-f]+)', \
	                      open('mega65.ld').read()).group(1), 16); \
	    r = os.path.getsize('$(OUT)')-2; \
	    print('mega65: resident %d bytes of %d, %d free' % (r, n, n-r)); \
	    print('mega65: OVERLAYS.BIN %d bytes' % os.path.getsize('build/OVERLAYS.BIN'))"

# A SEPARATE binary, deliberately, exactly as the C128 port keeps one:
# TREK_DEBUG_INPUT compiles in a byte a script can poke to fake a keypress, and
# the release build must not have it. Its overlay images differ from the release
# build's, so they are cut from its own ELF and written beside it.
DEBUG_OUT = build/egatrek-debug.prg

debug: $(DEBUG_OUT)

$(DEBUG_OUT): $(SRC) src/m65vid.h src/m65input.h src/m65snd.h mega65.ld Makefile
	@mkdir -p build
	$(M65CC) $(CFLAGS) -DTREK_DEBUG_INPUT $(LDFLAGS) \
	    -o build/egatrek-debug.elf $(SRC) $(LIBC_A)
	$(OBJCOPY) -O binary --remove-section=.ovl_* build/egatrek-debug.elf build/egatrek-debug.res
	@python3 -c "d=open('build/egatrek-debug.res','rb').read(); \
	    open('$(DEBUG_OUT)','wb').write(bytes([0x01,0x20]) + d)"
	@rm -f build/OVERLAYS-DEBUG.BIN
	@for o in $(OVERLAYS); do \
	    $(OBJCOPY) -O binary --only-section=.ovl_$$o build/egatrek-debug.elf build/dovl$$o.bin; \
	    python3 -c "import sys;d=open('build/dovl$$o.bin','rb').read(); \
	        open('build/OVERLAYS-DEBUG.BIN','ab').write(d + b'\\0'*(4096-len(d)))"; \
	done
	@python3 -c "import subprocess; \
	    nm=subprocess.check_output(['$(LLVM_MOS)/bin/llvm-nm','build/egatrek-debug.elf']).decode(); \
	    a=[int(l.split()[0],16) for l in nm.splitlines() if l.split()[-1]=='ovl_load'][0]; \
	    f=open('build/OVERLAYS-DEBUG.BIN','r+b'); f.seek(-2,2); \
	    f.write(bytes([a & 0xFF, (a >> 8) & 0xFF])); f.close()"

smoke: $(SMOKE_OUT)

$(SMOKE_OUT): $(SMOKE_SRC) src/m65vid.h
	@mkdir -p build
	$(M65CC) -Wall -Os -I $(LIBC_INC) -I ../c128/src -I ../core \
	    -o $@ $(SMOKE_SRC) $(LIBC_A)

# Xemu auto-detects C65 mode from the $2001 load address, so -prg alone is
# enough; -prgmode 65 states it anyway.
# THE DATA FILES, on a virtual FAT32 fed from a directory. No disk image to
# build: -virtsd takes -sdimg as a folder and presents it to the Hypervisor,
# which is what open()/read512() read through.
DISK = build/disk

# THE CHECKS THE COMPILER AND THE TEST SUITE STRUCTURALLY CANNOT MAKE.
# This port had none for six days after the C128's overlay call-graph check
# caught a guaranteed crash -- same architecture, same one window, same
# `.ovl_*` sections, and nothing looking at them. tools/verify_prg.py says what
# it checks and, just as importantly, what it deliberately leaves to
# `make -C c128 verify` because the sources are shared.
# OBJECTS WITHOUT LTO, so verify can read the call graph AS WRITTEN -- rule 4
# is invisible in the linked binary because LTO folds the offending caller
# into main(). See tools/overlay_check.py.
NOLTO = build/nolto

nolto: $(SRC)
	@mkdir -p $(NOLTO)
	@for f in $(SHARED) $(filter %.c,$(PLATFORM)); do \
	    $(M65CC) -c -fno-lto $(CFLAGS) -o $(NOLTO)/`basename $$f .c`.o $$f || exit 1; \
	done

verify: $(OUT) nolto
	python3 tools/verify_prg.py

# `disk` depends on verify so a binary that cannot run -- wrong load address,
# stale images, an overlay calling out of its window -- can never reach the SD
# card. The C128 hangs its verify off `run` for exactly this reason.
disk: $(OUT) verify
	@mkdir -p $(DISK)
	@cd .. && python3 tools/gen_strings.py >/dev/null && python3 tools/make_music.py >/dev/null
	@# THE POOL IS SHARED, THE TWO STRINGS THAT NAME THE MACHINE ARE NOT.
	@# Same ids, same count, same strdata.h -- see src/strings.override.txt.
	@cd .. && python3 tools/gen_strings.py --override mega65/src/strings.override.txt \
	    mega65/build/strings.dat
	@cp build/strings.dat $(DISK)/STRINGS.DAT
	@cp ../c128/build/music.dat   $(DISK)/MUSIC.DAT
	@cp ../c128/src/briefing.txt  $(DISK)/BRIEF.TXT
	@cp build/OVERLAYS.BIN        $(DISK)/OVERLAYS.BIN
	@python3 -c "import sys; sys.stdout.buffer.write(b''.join(b'.'*25 + b'\r\n0\r\n' for _ in range(10)))" > $(DISK)/TREK.SCR
	@ls -l $(DISK)

# `-sdimg @mega65.img` STAYS, but only because the MACHINE boots off the card.
# The game no longer reads or writes it. That flag names Xemu's own image in its
# preferences directory, which Xemu partitions and formats itself, so it needs
# no ONBOARDing and nothing waits for a human -- which is what makes `-headless`
# work at all. Three hand-built cards failed before that was understood.
#
# THE FILES USED TO GO ON WITH mtools, through tools/putfiles.sh. That is gone
# with hyppo: c1541 writes the D81 instead, and the CHS-geometry wrinkle
# putfiles.sh existed for went with it. THIS IS THE SECOND TIME A SCRIPT HAS
# OUTLIVED ITS RULE HERE -- an `sd` target once called a tools/mksd.py that did
# not exist, and `make run` was broken for a day before anyone noticed. Deleted
# rather than left lying about.
# ONE D81, AND THE SD CARD IS NO LONGER THE GAME'S BUSINESS.
#
# The port used to read its files off Xemu's own SD card through the
# Hypervisor. It cannot write there -- hyppo has no write call at all -- so the
# save had to go to a D81 on device 8 instead, and hyppo cannot SEE inside a
# D81. Splitting reads and writes across two filesystems means saving
# successfully and never reading it back, so everything moved to the D81 and
# hyppo is gone. See src/m65storage.c.
#
# `-sdimg` stays because the MACHINE still boots off the card; the game no
# longer touches it. On real hardware the player mounts this image and RUNs
# EGATREK, which is how MEGA65 software normally ships -- and it makes this
# port distribute like the C128's .d64.
#
# Names go on in lower case: c1541 writes them as shifted PETSCII, which is
# what the drive matches against the upper-case names the game asks for. That
# is measured, not styled -- probe 3 opened "OVL,S,R" against an entry c1541
# lists as "ovl".
D81 = build/egatrek.d81

d81: disk
	@rm -f $(D81)
	@c1541 -format "ega trek,01" d81 $(D81) >/dev/null 2>&1
	@c1541 -attach $(D81) -write $(OUT) egatrek >/dev/null 2>&1
	@for f in STRINGS.DAT MUSIC.DAT OVERLAYS.BIN BRIEF.TXT; do \
	    n=`echo $$f | tr 'A-Z' 'a-z'`; \
	    c1541 -attach $(D81) -write $(DISK)/$$f "$$n,s" >/dev/null 2>&1 \
	        || { echo "d81: could not write $$f"; exit 1; }; \
	done
	@c1541 -attach $(D81) -list 2>&1 | grep -v "OPENCBM\|recognised\|attached\|detached"

# THE RELEASE ARTEFACT IS THE D81 ITSELF, plus the README beside it. Unlike the
# X16 -- which has no disk image and ships a folder of files for HostFS -- this
# machine mounts one image, so a zip would only be a wrapper around a wrapper.
#
# MUSIC.DAT IS THIS PORT'S OWN COMPOSITION, from tools/make_music.py. The
# extracted note data that tools/gen_music.py pulls out of EGATREK.EXE is
# Anderson's creative work and must never reach a release; make_music.py exists
# so that a release can have sound at all.
release: d81
	@cp $(D81) build/egatrek-mega65.d81
	@cp README-release.txt build/egatrek-mega65.txt
	@ls -l build/egatrek-mega65.d81 build/egatrek-mega65.txt

# Kept as an alias for a while: `sd` is in three docs and a release script.
sd: d81

run: d81
	$(XMEGA65) -rom "$(M65ROM)" -prgmode 65 -sdimg @mega65.img \
	    -8 $(CURDIR)/$(D81) -prg $(CURDIR)/$(OUT)

# Headless, scripted, and screenshotted -- how this port is actually checked.
# `make debug` first: the release build has no kb_inject to poke.
DRIVE_KEYS ?= RETURN N RETURN N RETURN J A M I E RETURN 3 RETURN X RETURN
# DRIVE_PEEK dumps symbols out of the running machine before the screenshot.
# A picture cannot answer "what is in `ship`", and the hall-of-fame report this
# port chased for days was entirely a claim about memory.
#   make drive DRIVE_PEEK="--peek ship" DRIVE_KEYS="... S X RETURN X RETURN"
DRIVE_PEEK ?=
drive: debug disk
	python3 tools/drive.py build/screen.png $(DRIVE_PEEK) $(DRIVE_KEYS)

# THE TWO BANKING PROBES, kept because they are the evidence behind the write
# path and because the first run of probe 1 got the $01 bits backwards -- which
# is exactly the sort of thing that is cheap to re-run and expensive to argue
# about. See README, "What the C65 DOS actually needs".
#
#   make probe-bank              what stops being our RAM while the ROM is mapped
#   make probe-dos CFG=3         can the DOS write under config C
#
# probe-dos needs a D81 on drive 8; the script formats a fresh one per run and
# lists it afterwards, because a KERNAL call can report success and write
# nothing. The DIRECTORY is the result, not the screen.
PROBECC = $(M65CC) -Wall -Werror -Oz \
          -I $(MEGA65_LIBC)/include -o $@ $< $(LIBC_A)
CFG ?= 3

build/probe_bank.prg: src/probe_bank.c
	@mkdir -p build
	$(PROBECC)

probe-bank: build/probe_bank.prg
	sh tools/run.sh "$(CURDIR)/$<" build/probe1.png 14
	@echo "read build/probe1.png"

build/probe_dos$(CFG).prg: src/probe_dos.c
	@mkdir -p build
	$(M65CC) -Wall -Werror -Oz -DCFG=$(CFG) -I $(MEGA65_LIBC)/include -o $@ $< $(LIBC_A)

probe-dos: build/probe_dos$(CFG).prg
	sh tools/run_dos_probe.sh $(CFG)

build/probe_read.prg: src/probe_read.c
	@mkdir -p build
	$(PROBECC)

# Needs OVERLAYS.BIN on a D81 as a SEQ file called OVL; `make` first.
probe-read: build/probe_read.prg build/OVERLAYS.BIN
	@rm -f build/read.d81
	@c1541 -format "probe,01" d81 build/read.d81 >/dev/null 2>&1
	@c1541 -attach build/read.d81 -write build/OVERLAYS.BIN "ovl,s" >/dev/null 2>&1
	sh tools/run_read_probe.sh 40
	@python3 -c "d=open('build/OVERLAYS.BIN','rb').read(); c=0;\
	    [c := c ^ b for b in d]; \
	    print('expect %d bytes, xor \$$%02X' % (len(d), c))"

build/probe_cmd.prg: src/probe_cmd.c
	@mkdir -p build
	$(PROBECC)

probe-cmd: build/probe_cmd.prg
	sh tools/run_cmd_probe.sh

run-smoke: $(SMOKE_OUT)
	$(XMEGA65) -rom "$(M65ROM)" -prgmode 65 -prg $(CURDIR)/$(SMOKE_OUT)

clean:
	rm -f build/*.prg src/*.o
