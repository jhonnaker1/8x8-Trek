# The Commodore 64, and so far only the probe that item 59 turns on.
CC   = $(HOME)/llvm-mos/bin/mos-c64-clang
CFLAGS = -Oz -Wall -Werror

.PHONY: probe logcheck clean

probe: build/probe.d64

build/logprobe.prg: src/logprobe.c Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $<
	@ls -l $@

# THE DISK IS REBUILT FROM EMPTY, EVERY TIME, and that is not tidiness.
# `c1541 -write` over an existing name answers ERR = 63, FILE EXISTS and
# leaves the OLD file in place -- so a rebuilt binary silently does not reach
# the disk, and the next run tests the previous program. That is exactly how a
# deliberately-broken build reported a PASS here on 2026-09-14.
build/probe.d64: build/logprobe.prg ../c128/build/music.pdat
	@mkdir -p build
	@cp ../c128/build/music.pdat build/music.dat
	@rm -f $@
	@c1541 -format "logprobe,01" d64 $@ >/dev/null
	@c1541 -attach $@ -write build/logprobe.prg logprobe >/dev/null
	@c1541 -attach $@ -write build/music.dat music.dat >/dev/null
	@echo "probe disk: $@"

logcheck: build/probe.d64
	@python3 tools/logcheck.py

clean:
	rm -f build/*.prg build/*.d64 build/*.dat
