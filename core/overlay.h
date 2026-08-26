#ifndef OVERLAY_H
#define OVERLAY_H

#include <stdint.h>

/* Code overlays: the fourth seam, and the one that lifts the ceiling.
 *
 * The C128 has about 42K for code and read-only data together and the port
 * fills it. Bank 1 does not help: it holds DATA, reached a byte at a time
 * through the KERNAL, and nothing executes from there. What DOES help is that
 * large parts of this program never run at the same time as each other -- the
 * title and setup screens, the end-of-game evaluation, the hall of fame, the
 * modal panels. Each of those can live on the disk and be pulled into one
 * fixed window when it is wanted.
 *
 * MEASURED 2026-08-26: 10,672 bytes of the image are phases or modal screens
 * that never coexist, against a window the size of the largest. See NOTES.md,
 * "SCOPE: the overlay seam", for the arithmetic and the call graph behind it.
 *
 * SAME SHAPE AS THE OTHER THREE SEAMS: contract here, mechanism per platform,
 * and `core/` never calls any of it.
 *
 *     C128    KERNAL LOAD straight into the window, about a sixth of a second
 *     X16, F256, CoCo 3   want one too; all three are as tight or tighter
 *     MEGA65, Amiga       an empty function -- they have the room
 *
 * So a call site reads the same everywhere and costs nothing on a roomy
 * machine:
 *
 *     ovl_load(OVL_EVAL);
 *     ui_evaluation();
 *
 * THREE RULES, AND THEY ARE NOT OPTIONAL.
 *
 *   1. Every overlay function is `noinline`. Without it the compiler inlines
 *      it into a resident caller and the overlay is silently EMPTY -- which
 *      is not hypothetical: every one of these functions was already being
 *      inlined into main(), which is why main() measured 14,846 bytes.
 *   2. An overlay calls RESIDENT code only, never another overlay. The window
 *      would be overwritten underneath it.
 *   3. One entry point per overlay, reached through a resident stub that does
 *      the load and the call together. That is what makes "forgot to load it"
 *      impossible rather than merely unlikely.
 *
 * An overlay's statics do not survive a swap.
 */

/* The overlay list is the PORT'S OWN, not part of the contract -- a platform
   with room ignores it entirely. Ids are indices; keep them contiguous. */
#define OVL_EVAL   0    /* ui_evaluation, and the score sheet it draws from */
#define OVL_HOF    1    /* ui_hall_of_fame, and all of core/hof.c with it */
/* The front end AND the save machinery, together and not by choice: setup's
   restore path and SAVE both reach core/serial.c, so splitting them would
   leave the serialiser resident and save nothing. */
#define OVL_FRONT  2    /* ui_title, ui_setup, ui_save_game, the serialiser */
#define OVL_INFO   3    /* ui_info_panel */
#define OVL_REPAIR 4    /* ui_repair_report */
#define OVL_MSGS   5    /* ui_messages_view */
#define OVL_PLANET 6    /* ui_planet_list */
#define OVL_COUNT  7

#define OVL_NONE   0xFF

/* Marks a function as living in overlay `sec`.
 *
 * TWO THINGS AT ONCE, and both are required. `noinline` keeps the compiler
 * from folding the function into a resident caller, which would leave the
 * overlay empty; the section name is what the linker script picks up to place
 * it at the window.
 *
 * IT EXPANDS TO NOTHING OFF-TARGET, and that is not a nicety. The native test
 * builds compile the same UI sources with the host's compiler, and a Mach-O
 * target REJECTS an ELF section name outright -- "mach-o section specifier
 * requires a segment and section separated by a comma". A flat platform wants
 * it empty for its own reasons: the function simply stays where it is.
 *
 * `__mos__` rather than `__C128__` because every 6502 target that will want
 * overlays -- X16, F256, CoCo 3 is a different CPU but the same argument --
 * is built by this toolchain, and none of the roomy ones are. */
#ifdef __mos__
#define OVL_CODE(sec) __attribute__((noinline, section(".ovl." sec)))
#else
#define OVL_CODE(sec)
#endif

/* Makes `which` resident, and is IDEMPOTENT: asking for the overlay that is
   already loaded costs nothing, so a stub may call it on every entry. A
   platform with no overlays implements this as an empty function. */
void ovl_load(uint8_t which);

#endif
