# EGA Trek for the Commander X16.
#
# Third port. The core compiles for cx16 untouched, and llvm-mos is the same
# compiler family as the C128 and MEGA65 builds, so what was learned about the
# __rc calling convention, overlay sections and -lcopy-data all carries over.
#
# See NOTES.md, "SCOPE: the Commander X16".

LLVM_MOS ?= $(HOME)/llvm-mos
X16CC     = $(LLVM_MOS)/bin/mos-cx16-clang
X16EMU   ?= $(HOME)/x16emu_macos_m1-r48

# TREK_COLOUR_IS_EGA makes EGA_TO_VDC the identity: VERA's palette is
# programmable, so entries 0-15 hold EGA's own values. Same trick as the
# MEGA65, and for the same reason.
CFLAGS = -Wall -Werror -Oz -DTREK_COLOUR_IS_EGA \
         -I src -I ../c128/src -I ../core
OVLFLAGS = $(CFLAGS) -DTREK_OVERLAYS -DOVL_WINDOW=$(OVL_WINDOW)
LDFLAGS  = -T x16.ld -Wl,--no-check-sections
OBJCOPY  = $(LLVM_MOS)/bin/llvm-objcopy
NM       = $(LLVM_MOS)/bin/llvm-nm
OVERLAYS = eval hof front info repair msgs planet cmds title events

# THE WINDOW SIZE, stated once. x16.ld must carry the same number; this is
# what the loader compiles against and what the image cutter enforces.
OVL_WINDOW = 4032

SMOKE_SRC = src/smoke.c src/x16vera.c ../c128/src/layout.c
SMOKE_OUT = build/smoke.prg

.PHONY: all smoke charset game run-charset run-smoke run-game clean

all: smoke

smoke: $(SMOKE_OUT)

$(SMOKE_OUT): $(SMOKE_SRC) src/x16vera.h
	@mkdir -p build
	$(X16CC) $(CFLAGS) -o $@ $(SMOKE_SRC)
	@python3 -c "import os; print('x16: smoke %d bytes' % os.path.getsize('$@'))"

CHARSET_SRC = src/charset.c src/x16vera.c ../c128/src/layout.c
CHARSET_OUT = build/charset.prg

charset: $(CHARSET_OUT)

$(CHARSET_OUT): $(CHARSET_SRC) src/x16vera.h
	@mkdir -p build
	$(X16CC) $(CFLAGS) -o $@ $(CHARSET_SRC)

run-charset: $(CHARSET_OUT)
	cd $(X16EMU) && ./x16emu -prg $(CURDIR)/$(CHARSET_OUT) -run

run-smoke: $(SMOKE_OUT)
	cd $(X16EMU) && ./x16emu -prg $(CURDIR)/$(SMOKE_OUT) -run

clean:
	rm -rf build

# THE EARLY LINK. Not a runnable game -- every seam below the console is a
# stub. It exists to answer one question before any of them is written: does
# the whole thing FIT? See NOTES.md, "SCOPE: the Commander X16", risk 4.
FULL_SRC = ../c128/src/main.c ../c128/src/ui.c ../c128/src/layout.c \
           ../c128/src/strpool.c \
           ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
           src/x16vera.c src/x16storage.c src/x16mem.c src/stubs.c
FULL_OUT = build/full.prg

full: $(FULL_OUT)

$(FULL_OUT): $(FULL_SRC) src/x16vera.h
	@mkdir -p build
	$(X16CC) $(CFLAGS) -o $@ $(FULL_SRC)
	@python3 -c "import os; \
	    n=os.path.getsize('$@')-2; lim=0x96FF; \
	    print('x16: FULL LINK %d bytes of %d, %d free' % (n, lim, lim-n))"

STORE_SRC = src/storetest.c src/x16storage.c
STORE_OUT = build/storetest.prg

storetest: $(STORE_OUT)

$(STORE_OUT): $(STORE_SRC)
	@mkdir -p build
	$(X16CC) $(CFLAGS) -o $@ $(STORE_SRC)

MEM_SRC = src/memtest.c src/x16mem.c src/x16storage.c
MEM_OUT = build/memtest.prg

memtest: $(MEM_OUT)

$(MEM_OUT): $(MEM_SRC)
	@mkdir -p build
	$(X16CC) $(CFLAGS) -o $@ $(MEM_SRC)

# THE GAME WITH OVERLAYS. One link produces the ELF; objcopy then cuts the
# resident half (everything except .ovl_*) into the PRG and each window image
# out separately, concatenated into OVERLAYS.BIN. Both existing ports do this
# and for the same reason -- an overlay is linked WITH the resident half, so
# the two halves are only valid together.
GAME_SRC = ../c128/src/main.c ../c128/src/ui.c ../c128/src/layout.c \
           ../c128/src/strpool.c \
           ../core/trek.c ../core/planet.c ../core/hof.c ../core/serial.c \
           src/x16vera.c src/x16storage.c src/x16mem.c src/x16ovl.c src/stubs.c
GAME_OUT = build/trekx16.prg

game: $(GAME_OUT)

$(GAME_OUT): $(GAME_SRC) src/x16vera.h src/x16mem.h x16.ld
	@mkdir -p build
	$(X16CC) $(OVLFLAGS) $(LDFLAGS) -o build/trekx16.elf $(GAME_SRC)
	$(OBJCOPY) -O binary "--remove-section=.ovl_*" build/trekx16.elf build/trekx16.res
	@python3 -c "d=open('build/trekx16.res','rb').read(); \
	    open('$(GAME_OUT)','wb').write(bytes([0x01,0x08]) + d)"
	@rm -f build/OVERLAYS.BIN
	@for o in $(OVERLAYS); do \
	    $(OBJCOPY) -O binary --only-section=.ovl_$$o build/trekx16.elf build/ovl$$o.bin; \
	    python3 -c "import sys; d=open('build/ovl$$o.bin','rb').read(); \
	        sys.exit('overlay $$o is %d bytes, window is $(OVL_WINDOW)' % len(d)) if len(d) > $(OVL_WINDOW) else None; \
	        open('build/OVERLAYS.BIN','ab').write(d + b'\\0'*($(OVL_WINDOW)-len(d)))" || exit 1; \
	done
	@# THE STAMP, into the last two bytes -- see src/x16ovl.c.
	@python3 -c "import subprocess; \
	    nm=subprocess.check_output(['$(NM)','build/trekx16.elf']).decode(); \
	    a=[int(l.split()[0],16) for l in nm.splitlines() if l.split()[-1:]==['ovl_anchor']][0]; \
	    f=open('build/OVERLAYS.BIN','r+b'); f.seek(-2,2); \
	    f.write(bytes([a & 0xFF, (a >> 8) & 0xFF])); f.close(); \
	    print('x16: overlay stamp', hex(a))"
	@python3 -c "import os; n=os.path.getsize('$(GAME_OUT)')-2; lim=0x96ff-0x1000; \
	    print('x16: resident %d of %d, %d free' % (n, lim, lim-n)); \
	    print('x16: OVERLAYS.BIN %d bytes' % os.path.getsize('build/OVERLAYS.BIN'))"
