# EGA Trek for the MEGA65, native C65 mode.
#
# NATIVE MODE IS NOT A PREFERENCE, IT IS THE WHOLE POINT. A $0801 binary puts
# the machine in C64 mode where 80 columns do not exist. Linking at $2001 with
# a BASIC 65 header selects C65/native mode, where VIC-IV's H640 is real and
# colour RAM is the full 2000 cells rather than the 1K window at $D800.
# llvm-mos's mega65 target does that for us.

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
OVERLAYS = eval hof front info repair msgs planet cmds title events

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

.PHONY: all smoke smoke2 run run-smoke music disk sd sd-setup clean

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
	@python3 -c "import os; r=os.path.getsize('$(OUT)')-2; \
	    print('mega65: resident %d bytes of 40959, %d free' % (r, 40959-r)); \
	    print('mega65: OVERLAYS.BIN %d bytes' % os.path.getsize('build/OVERLAYS.BIN'))"

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

disk: $(OUT)
	@mkdir -p $(DISK)
	@cd .. && python3 tools/gen_strings.py >/dev/null && python3 tools/make_music.py >/dev/null
	@cp ../c128/build/strings.dat $(DISK)/STRINGS.DAT
	@cp ../c128/build/music.dat   $(DISK)/MUSIC.DAT
	@cp ../c128/src/briefing.txt  $(DISK)/BRIEF.TXT
	@cp build/OVERLAYS.BIN        $(DISK)/OVERLAYS.BIN
	@python3 -c "import sys; sys.stdout.buffer.write(b''.join(b'.'*25 + b'\r\n0\r\n' for _ in range(10)))" > $(DISK)/TREK.SCR
	@ls -l $(DISK)

# The SD image the Hypervisor will accept: an MBR with a FAT32 partition, at
# least 4GB (it prints "LOOKING FOR SDHC CARD >=4GB" and means it), the ROM at
# the root, and the game's data files beside it. Sparse, so it costs the few
# hundred KB actually written.
SD = build/sd.img

$(SD): disk
	python3 tools/mksd.py $(SD) "$(M65ROM)" $(DISK)/*

sd: $(SD)

# ONE-TIME SETUP, and it needs a human: Xemu writes its own system files onto a
# new card through Disks -> SD-card -> Update files, and there is no command
# line for it. Run this once, do that, quit; after which `make run` works.
sd-setup: $(SD)
	$(XMEGA65) -rom "$(M65ROM)" -sdimg $(CURDIR)/$(SD)

run: $(SD)
	$(XMEGA65) -rom "$(M65ROM)" -prgmode 65 -sdimg $(CURDIR)/$(SD) \
	    -prg $(CURDIR)/$(OUT)

run-smoke: $(SMOKE_OUT)
	$(XMEGA65) -rom "$(M65ROM)" -prgmode 65 -prg $(CURDIR)/$(SMOKE_OUT)

clean:
	rm -f build/*.prg src/*.o
