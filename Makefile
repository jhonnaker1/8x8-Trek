# Top-level: builds and tests the shared core on the build machine.
#
# The core is plain C with no platform dependencies, so cc compiles it
# directly. Proving it here rather than on the target is deliberate -- a
# session on this machine cannot screenshot an emulator, so anything that can
# be asserted natively should be.
#
# Per-platform builds live in their own directories: cd c128 && make run

CFLAGS = -Wall -Wextra -std=c99 -O2

.PHONY: all test clean

all: test

test: build/test_trek
	./build/test_trek

build/test_trek: core/test/test_trek.c core/trek.c core/trek.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ core/test/test_trek.c core/trek.c

clean:
	rm -rf build
