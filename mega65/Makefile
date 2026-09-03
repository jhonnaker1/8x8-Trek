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

SMOKE_SRC = src/smoke.c src/m65vid.c ../c128/src/layout.c
SMOKE_OUT = build/smoke.prg

.PHONY: all smoke run clean

all: smoke

smoke: $(SMOKE_OUT)

$(SMOKE_OUT): $(SMOKE_SRC) src/m65vid.h
	@mkdir -p build
	$(M65CC) -Wall -Os -I $(LIBC_INC) -I ../c128/src -I ../core \
	    -o $@ $(SMOKE_SRC) $(LIBC_A)

# Xemu auto-detects C65 mode from the $2001 load address, so -prg alone is
# enough; -prgmode 65 states it anyway.
run: $(SMOKE_OUT)
	$(XMEGA65) -rom "$(M65ROM)" -prgmode 65 -prg $(CURDIR)/$(SMOKE_OUT)

clean:
	rm -f build/*.prg src/*.o
