# Included by c64/Makefile. KEPT OUT OF IT because it is not part of the
# C64 port: tools/check_portcost.py counts that Makefile as what a C64 port
# costs, and nothing here is needed to build or play the release.
#
# THE REU BUILD: overlays that may call each other, swapped by DMA.
#
# Not a release. It exists because C64 OS gives an app 30,976 bytes and this
# game only fits that if the engine and the console drawing leave resident
# memory -- which on a disk would cost seconds a call, and from an REU costs
# ~4ms (NOTES.md, "THE TRIAL LINK"). The bare C64 is where the mechanism can
# be built and driven headlessly first; `make reucheck` plays the same script
# on this and on the disk build and compares every screen.
#
# THE SAME SOURCES, with three differences: the seven TREK_OVL_REU groups are
# switched on (core/overlay.h), src/c64reu.c replaces the disk loader, and
# the link is PATCHED after it is made -- tools/reu_thunks.py routes every
# call that crosses overlays through a thunk in src/reuovl.s. The patch is in
# the ELF's own recipe, so no unpatched ELF ever sits under the target's name.
#
# ITS OWN DIRECTORY, because its twenty ovl*.prg would otherwise overwrite the
# release build's eleven under the same names -- the hazard MAKE_D64 already
# names for the debug build.
.PHONY: reu reu-debug d64-reu d64-reu-debug runreu reucheck

REU_OVERLAYS = $(OVERLAYS) enemy move view panel nav time turn laser torp
REU_CFLAGS   = $(filter-out -T c64.ld,$(CFLAGS)) -T c64reu.ld \
               -DTREK_OVL_ENEMY -DTREK_OVL_MOVE -DTREK_OVL_REU
REU_SRC      = $(filter-out ../c128/src/overlay.c,$(SRC)) src/c64reu.c src/reuovl.s

# $(1) = the directory, $(2) = extra flags
define REU_LINK
	@mkdir -p $(1)
	$(CC) $(REU_CFLAGS) $(2) $(LDFLAGS) -Wl,--emit-relocs \
	    -Wl,-Map=$(1)/trek64.map -o $(1)/trek64.elf.tmp $(REU_SRC)
	python3 tools/reu_thunks.py $(1)/trek64.elf.tmp
	@# The relocations were kept for the patch alone. Left in, they pin the
	@# .ovl_* sections and objcopy refuses to cut the PRG without them.
	$(OBJCOPY) -w -R '.rela*' $(1)/trek64.elf.tmp $(1)/trek64.elf
	@rm -f $(1)/trek64.elf.tmp
endef

reu: build/reu/trek64.prg
build/reu/trek64.elf: $(REU_SRC) c64reu.ld tools/reu_thunks.py ../core/overlay.h Makefile reu.mk
	$(call REU_LINK,build/reu,)
build/reu/trek64.prg: build/reu/trek64.elf
	$(call MAKE_PRG,$<,$@)
	@python3 tools/report_size.py $< $@

reu-debug: build/reu-debug/trek64.prg
build/reu-debug/trek64.elf: $(REU_SRC) c64reu.ld tools/reu_thunks.py ../core/overlay.h Makefile reu.mk
	$(call REU_LINK,build/reu-debug,-DTREK_DEBUG_INPUT)
build/reu-debug/trek64.prg: build/reu-debug/trek64.elf
	$(call MAKE_PRG,$<,$@)

d64-reu: build/reu/egatrek-c64-reu.d64
build/reu/egatrek-c64-reu.d64: build/reu/trek64.prg build/strings.dat build/music.dat ../c128/src/briefing40.txt Makefile reu.mk
	$(call MAKE_D64,$@,build/reu/trek64.prg,build/reu/trek64.elf,build/reu,$(REU_OVERLAYS))

d64-reu-debug: build/reu-debug/egatrek-c64-reu.d64
build/reu-debug/egatrek-c64-reu.d64: build/reu-debug/trek64.prg build/strings.dat build/music.dat ../c128/src/briefing40.txt Makefile reu.mk
	$(call MAKE_D64,$@,build/reu-debug/trek64.prg,build/reu-debug/trek64.elf,build/reu-debug,$(REU_OVERLAYS))

# THE REU BUILD'S TEST, and the only one that can see the thunks work: one
# pinned game played on the disk build and on the REU build, every screen
# compared in characters and colours. Sabotaged -- the DMA that puts the
# caller's image back removed -- it fails at the first console; see
# tools/reucheck.py. Headless, in warp, about four minutes.
reucheck: $(DEBUG_D64) build/reu-debug/egatrek-c64-reu.d64
	python3 tools/reucheck.py $(DEBUG_D64) build/trek64-debug.map build/reucheck-disk.json
	python3 tools/reucheck.py build/reu-debug/egatrek-c64-reu.d64 \
	    build/reu-debug/trek64.map build/reucheck-reu.json --reu
	python3 tools/reucheck.py --compare build/reucheck-disk.json build/reucheck-reu.json

# A 128K 1700, the smallest REU made: twenty 4K images need 80K.
#
# +saveres, OR VICE WRITES THIS COMMAND LINE INTO ~/.config/vice/vicerc on
# exit. The first `make runreu` did exactly that: Jamie's C64 came back up
# with a 128K REU switched on where his own settings had a 16MB one off.
runreu: build/reu/egatrek-c64-reu.d64
	$(X64) +saveres -reu -reusize 128 -autostart $(CURDIR)/$<:trek64
