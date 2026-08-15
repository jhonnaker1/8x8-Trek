CC65_TARGET = c128
SRC = src/main.c src/vdc.c src/layout.c src/ui.c src/input.c ../core/trek.c
OUT = build/trek128.prg

.PHONY: all run test verify clean

all: $(OUT)

# The EGA->VDC colour mapping is pure arithmetic, so it is proven natively on
# the build machine rather than by squinting at an emulator. Getting it wrong
# is silent -- every colour still renders, just as the wrong hue.
test:
	cc -Wall -Wextra -o build/test_egavdc test/test_egavdc.c
	./build/test_egavdc

# Inspects the LINKED binary, which is the only place cc65's character-set
# translation is visible. Native cc does no such translation, so `make test`
# above passes whether or not the encoding is right -- three bugs in this port
# proved that the hard way. `run` depends on this, so a binary with a PETSCII
# key table can never reach the emulator.
verify: $(OUT)
	python3 tools/verify_prg.py

$(OUT): $(SRC) src/vdc.h src/layout.h src/egavdc.h src/ui.h src/input.h ../core/ega.h ../core/trek.h
	cl65 -t $(CC65_TARGET) -O -o $(OUT) $(SRC)

# The VDC picture is on x128's second window. VICE opens both the VIC-IIe and
# VDC windows; this port drives only the VDC, so the 40-column window shows
# whatever BASIC left behind and can be ignored.
run: verify
	x128 $(OUT)

clean:
	rm -f build/*.prg build/test_egavdc src/*.o
