# Top-level: builds and tests the shared core on the build machine.
#
# The core is plain C with no platform dependencies, so cc compiles it
# directly. Proving it here rather than on the target is deliberate: it is
# faster, and a failure points at the logic instead of at a toolchain.
#
# (An earlier version of this comment said a session on this machine could not
# screenshot an emulator. That stopped being true on 2026-08-19 -- see
# tools/vice_mon.py. The reason to test natively is speed and isolation, not
# blindness.)
#
# Per-platform builds live in their own directories: cd c128 && make run

CFLAGS = -Wall -Wextra -std=c99 -O2

# Cross compiler for the port-check target below. Present on this machine at
# ~/amiga-toolchain/bin; override if it lives elsewhere.
M68K = $(HOME)/amiga-toolchain/bin/m68k-amigaos-gcc

.PHONY: all test port-check exit-test clean

all: test port-check

test: build/test_trek
	./build/test_trek

build/test_trek: core/test/test_trek.c core/trek.c core/trek.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ core/test/test_trek.c core/trek.c

# Compiles the core for the 68000 as a portability check -- it is not linked
# and nothing is run.
#
# Worth having as a target rather than an occasional manual step. The core
# makes portability claims all over its comments -- explicit-width types, no
# float, no long, arithmetic staged so intermediates stay inside 16 bits
# because `int` is 16-bit under cc65 and 32-bit on the 68000 -- and until
# 2026-08-19 not one line had ever been compiled for a 68000. It was clean,
# but that was luck rather than evidence.
#
# What this catches that the other two builds cannot: the 68000 is big-endian
# and requires even alignment for word access, so it is the only compiler here
# that would notice byte-level aliasing of a multi-byte value. (There is none
# today; that is the point of checking continuously rather than at port time.)
#
# -Werror on purpose. A portability check that only prints a warning is not a
# check -- the three -Wsign-compare warnings this target first produced had
# been sitting in the core unnoticed precisely because nothing failed.
#
# Skipped with a note if the cross compiler is absent, so a machine without it
# can still run `make`.
port-check:
	@if [ -x "$(M68K)" ]; then \
	    echo "port-check: compiling the core for 68000"; \
	    $(M68K) -c -O2 -Wall -Wextra -Werror -o /dev/null core/trek.c && \
	    echo "port-check: clean"; \
	else \
	    echo "port-check: SKIPPED -- no m68k-amigaos-gcc at $(M68K)"; \
	fi

# Does the C128 build still hand the machine back to BASIC? NOT part of `all`:
# it launches x128 five times and takes a couple of minutes, which is a poor
# fit for a build you run constantly.
#
# It exists because open item 2 -- "returning to BASIC wedges the C128" -- was
# believed for a week on no evidence, and the thing that finally settled it was
# a script, not an opinion. Leaving that script runnable is how the answer
# stays checkable. See NOTES.md, "The exit bug that was never there".
exit-test:
	python3 tools/exit_bisect.py
	python3 tools/exit_real.py

clean:
	rm -rf build
