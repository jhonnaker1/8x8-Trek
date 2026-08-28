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
/* Setup AND the save machinery, together and not by choice: setup's restore
   path and SAVE both reach core/serial.c, so splitting THESE TWO would leave
   the serialiser resident and save nothing. The title screen used to be here
   as well and was the one part that could leave -- see OVL_TITLE.

   THIS IS THE OVERLAY THAT GROWS. The serialiser is inlined into
   ui_save_game, so every field added to the save record lands here. It hit
   3,994 of 4,096 on 2026-08-27 and the split bought it back to 3,353. When
   it next runs out there is nothing cheap left to move, and the answer will
   be to grow the window -- which costs resident RAM one for one. */
#define OVL_FRONT  2    /* ui_setup, ui_save_game, the serialiser */
#define OVL_INFO   3    /* ui_info_panel */
#define OVL_REPAIR 4    /* ui_repair_report */
#define OVL_MSGS   5    /* ui_messages_view */
#define OVL_PLANET 6    /* ui_planet_list, and ORBIT/LAND/USE with it */
/* The RARE MODAL COMMANDS. Added 2026-08-27 to bank resident space before
   life support is built, not to fix anything. The test is FREQUENCY, not
   size: the fattest resident candidate is fire_one_torpedo at 1,450 bytes
   and it must NOT move, because an overlay swap is a disk load and firing is
   the most frequent action in the game. These five are dialogs the player
   opens a handful of times a game. */
#define OVL_CMDS   7    /* D)ock, E)nergy, S)elf destruct, F)ix */
/* The title screen, split out of OVL_FRONT on 2026-08-27 because front had
   reached 3,994 of 4,096. It is the only one of front's three functions that
   does NOT touch the serialiser -- setup restores through it and SAVE writes
   through it, so those two cannot be separated from it or from each other.
   Costs one extra disk load at game start, where the sequence was already
   two ovl_load calls. */
#define OVL_TITLE  8    /* ui_title */
/* THE SCHEDULED-EVENT HANDLERS. Third overlay pass, 2026-08-27: enemy_turn()
   had grown back to 6,055 resident bytes, and the growth was `run_events`'s
   switch being inlined into it -- a base attacked, a base falling, the
   settlers' distress call. Those fire a handful of times in a WHOLE GAME and
   were sitting in the hottest routine in the program.

   The guard is `trek_events_due()`, which stays resident and is a scan of two
   or three words. Nothing is loaded on a turn where nothing is due, which is
   almost every turn. */
#define OVL_EVENTS 9    /* run_events, the scheduled-event switch */
#define OVL_COUNT  10

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
