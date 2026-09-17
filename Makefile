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

.PHONY: release release-clean all test port-check c89-check check-tables tiers exit-test sound-check ports check-makefiles clean

c89-check:
	@echo "port-check: the core must stay C89 (cc65 needs it)"
	@cc -std=c89 -fsyntax-only -Werror=declaration-after-statement \
	    -I core core/*.c

# `ports` IS IN HERE, and that is open-list item 17 rather than a preference.
# It was written as the honest gate for all five ports and then left out of
# `all` because it needs five cross compilers -- so it only ran when somebody
# remembered, which is the same failure it was built to remove, one level up.
#
# What made it safe to add: a port whose compiler is not installed SKIPS rather
# than fails, naming the variable and path it looked for. A fresh clone with no
# toolchains gets five skips and a green build; this machine gets five real
# verifies for about ten seconds. Only a port that COULD have been built and
# was not turns `make` red.
all: test port-check check-tables tiers check-makefiles ports

test: build/test_trek build/test_serial build/test_hof
	./build/test_trek
	./build/test_serial
	./build/test_hof

build/test_trek: core/test/test_trek.c core/trek.c core/planet.c \
                 core/trek.h core/planet.h Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ core/test/test_trek.c core/trek.c core/planet.c

# The hall of fame's FILE FORMAT, which is the original's and not ours. Kept
# apart from the game rules for the same reason as test_serial.
build/test_hof: core/test/test_hof.c core/hof.c core/hof.h Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ core/test/test_hof.c core/hof.c

# The disk seam's core half. Worth its own binary rather than more cases in
# test_trek: it is the only test whose real subject is the FILE FORMAT, and it
# has to keep passing unchanged when the game rules around it move.
build/test_serial: core/test/test_serial.c core/serial.c core/trek.c \
                   core/planet.c core/serial.h core/trek.h core/planet.h Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ core/test/test_serial.c core/serial.c core/trek.c \
	    core/planet.c

# What in the core is measured and what is still a guess. Every #define in
# core/trek.h and core/planet.h carries a /*@TIER*/ marker on its own line;
# this fails if one arrives without a provenance. The markers used to live in
# the prose above a constant, and a scan for them attributed the nearest
# preceding paragraph -- which reported constants as fitted that were not.
tiers:
	@python3 tools/tiers.py

# EVERY PORT'S OWN GATE, WITH THE EXIT STATUS ACTUALLY CHECKED. Not part of
# `all`, because it needs five cross compilers; run it before a release or
# after anything that touches core/. See tools/check_ports.py for why a
# one-liner with a pipe in it was never good enough.
#
#   make ports            all five
#   make ports P=atari    one
# THE OPEN LIST IS CHECKED FIRST, and it costs nothing: it reads one file and
# needs no toolchain, where the ports below want six cross compilers. The
# header has been wrong five separate ways, always because status sat where a
# count could not see it -- see NOTES item 56.
# EVERY PORT'S RELEASE ARTEFACT, IN ONE PLACE. Cutting a release used to mean
# remembering seven directories and running `make release` in each -- and the
# v0.14.0 release attached six assets because six was what somebody remembered.
# Each port decides its own artefact: a .d64, a .d81, an .atr, a .zip, a disk
# image. This just makes sure none is forgotten.
#
# IT DOES NOT CLEAN FOR YOU. The clean rebuild is an instrument on this project
# and it is the RELEASE MANAGER's job to wipe first -- `make release-clean`.
# THE RELEASE BODY'S "How to run each one" SECTION, out of RUNNING.md rather
# than retyped into the GitHub release every time. Two copies of the same
# instructions is how this project has drifted six times before; the one in the
# repo is the source and this is what gets pasted.
#   gh release edit vX.Y.Z --notes-file <(gh release view vX.Y.Z --json body \
#       -q .body | sed '/^## How to run each one$/,$d'; make -s running-section)
.PHONY: running-section
running-section:
	@sed '1s/^# /## /' RUNNING.md

RELEASE_PORTS = c128 x16 mega65 atari amiga falcon coco3 c64 st coco3gime iigs

release:
	@python3 tools/open_list.py
	@for d in $(RELEASE_PORTS); do \
	    echo "=== $$d ==="; \
	    $(MAKE) -C $$d release || exit 1; \
	done
	@echo ""
	@echo "release artefacts:"
	@# ONLY RELEASE_PORTS, AND THE GLOB USED TO BE `*/build/`. release-clean
	@# wipes the build directory of every port in RELEASE_PORTS and no other,
	@# so a PARKED port's artefact survives and the listing offered it up
	@# beside the real ones -- plus4/build/egatrek-plus4.d64 sat in this list
	@# on the v0.19.0 build, for a port that does not boot. A list of what to
	@# upload must not include something that must not ship.
	@for d in $(RELEASE_PORTS); do \
	    ls -l $$d/build/egatrek-*.zip $$d/build/egatrek-*.d64 \
	          $$d/build/egatrek-*.d81 $$d/build/egatrek-*.atr 2>/dev/null; \
	done | awk '{printf "  %10s  %s\n", $$5, $$9}'

release-clean:
	@for d in $(RELEASE_PORTS); do rm -rf $$d/build; done
	@echo "every port's build/ wiped -- now run: make release"

ports:
	@python3 tools/open_list.py
	@python3 tools/check_colours.py
	@python3 tools/check_glyphs.py
	@python3 tools/check_portcost.py
	@python3 tools/check_assets.py
	@python3 tools/check_ports.py $(P)

# A VARIABLE USED IN A RULE'S PREREQUISITES MUST BE DEFINED ABOVE THAT RULE.
# Make expands prerequisites as it parses the line, so one defined below
# expands to nothing and the rule silently depends on less than it says. Three
# instances in two days, two of them leaving `make verify` reading STALE
# objects. Costs milliseconds; see tools/check_makefiles.py.
check-makefiles:
	@python3 tools/check_makefiles.py

# The port's fixed tables against the ORIGINAL BINARY. Added 2026-08-26 after
# core/planet.c shipped SEVEN planet names against the binary's EIGHT -- the
# list had been taken from reference/strings.txt, and `strings` dropped Vega.
# Skips itself on a tree without reference/.
check-tables:
	@python3 tools/check_tables.py

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
# C89, AND IT IS NOT PEDANTRY. cc65 is C89 and rejects a declaration after a
# statement; it is the toolchain the F256 currently needs, and the only one on
# the list that cares. THREE VIOLATIONS HAD ACCUMULATED IN core/trek.c by
# 2026-09-05, one of them three lines below a comment explaining the rule --
# because the rule was only ever a comment. The 68000 pass below cannot catch
# them: gcc there is C99 and perfectly happy. This is the check that closes it.
port-check: c89-check
	@if [ -x "$(M68K)" ]; then \
	    echo "port-check: compiling the core for 68000"; \
	    $(M68K) -c -O2 -Wall -Wextra -Werror -o /dev/null core/trek.c && \
	    $(M68K) -c -O2 -Wall -Wextra -Werror -o /dev/null core/planet.c && \
	    $(M68K) -c -O2 -Wall -Wextra -Werror -o /dev/null core/serial.c && \
	    $(M68K) -c -O2 -Wall -Wextra -Werror -o /dev/null core/hof.c && \
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

# Does the port play the right notes? NOT part of `all`: it launches x128 twice
# and records audio, which takes a couple of minutes.
#
# Sound fails silently -- a wrong frequency constant plays every note at the
# wrong pitch and nothing reports an error. This records the title screen on
# BOTH regions and measures. Both, because getting one right and the other
# wrong is exactly what a broken region detector looks like, and that is the
# bug this was written for.
# Depends on the D64: the music is a file ON THE DISK now, not in the binary.
sound-check:
	$(MAKE) -C c128 d64
	python3 tools/sound_check.py

clean:
	rm -rf build
