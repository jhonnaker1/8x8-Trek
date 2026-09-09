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
 * FOUR RULES, AND THEY ARE NOT OPTIONAL.
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
 *   4. ONLY main() MAY CALL INTO AN OVERLAY, or a caller listed in
 *      tools/overlay_check.py's PAIRED table with the load it pairs with.
 *      There are two, both deliberate and both read: run_turn reaching a rare
 *      event's prose in OVL_MSGS with load_msgs() on the line above, and
 *      trek_run_events reaching OVL_EVENTS guarded by trek_events_due() --
 *      the same predicate the platform loads on. Every other resident function
 *      must stay resident all the way down, because the load/call pairing
 *      lives in main() and nowhere else -- a resident function that reaches
 *      into a window is a function whose correctness depends on which overlay
 *      happens to be loaded when someone calls it, which nothing states and
 *      nothing enforces.
 *
 *      ADDED 2026-09-06, after the first game anyone played to the end
 *      dropped the machine into its monitor. trek_score() was resident and
 *      its whole body was trek_score_sheet(), which is OVL_CODE("eval"); it
 *      was called one statement after load_hof() had swapped the window, so
 *      the call went to the address trek_score_sheet has in the EVAL layout,
 *      the shorter hof image does not reach that far, and the CPU ran into
 *      unwritten bytes. All three ports carried it. The fix was to annotate
 *      trek_score() as well, which turns a hidden dependency into a call
 *      main() can see it must pair with a load.
 *
 *      `make verify` enforces this on all three ports, from -fno-lto objects
 *      -- because in the shipped binary LTO folds the offending caller into
 *      main() and the bad call becomes indistinguishable from the good ones.
 *
 *      AND IT WAS BLIND TO MOST OF THEM UNTIL 2026-09-06. A call to a static
 *      in the SAME object relocates against the SECTION (".ovl.msgs+0x15b"),
 *      not against a symbol, so the check looked its target up in a symbol
 *      map, found nothing, and said "ok". It had been reporting cross-object
 *      calls only. Both of the exceptions above were invisible to it, and
 *      both turned out to be correct -- but the check was weaker than every
 *      commit message that cited it claimed.
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

/* THE ELEVENTH, added 2026-09-05 for the X16 and useful to every port.
   `trek_new_game` is 1,382 bytes, runs once per game, and is called from
   `main` -- which is RESIDENT, and that is what makes it eligible: an overlay
   may not call into another overlay, so a candidate has to be reachable from
   resident code that can page it in first.

   The C128 had 211 bytes of resident free and the X16 was 530 short with stub
   sound and input still to come; this is the structural answer both needed,
   and it is cheapest here because the X16 keeps its images in banked RAM
   rather than as separate disk files. */
#define OVL_XTRA   10   /* trek_new_game */

/* THE ELEVEN ABOVE ARE EVERY PORT'S. THE TWO BELOW ARE OPT-IN, and the reason
 * is the cost of a swap rather than anything about the code.
 *
 * Every rule of thumb in this file -- "the test is FREQUENCY, not size",
 * "fire_one_torpedo must NOT move because firing is the most frequent action"
 * -- rests on one assumption: that loading an overlay means reading a disk.
 * On the C128 it does, and on the X16 it is a copy out of banked RAM.
 *
 * ON THE ATARI + VBXE IT IS A COPY OUT OF VIDEO RAM, and that changes which
 * splits are affordable rather than which are correct. These two page code on
 * the HOT PATH -- the enemy turn runs on essentially every command and the
 * move command is the most common one a player types -- which is ruinous on a
 * 1541 and merely costs milliseconds through a MEMAC window. So they are
 * enabled per port, by the Makefile, and no released port pays for them.
 *
 * MEASURED ON THE ATARI 2026-09-09: 3,910 and 1,578 bytes, which is what took
 * that target from 5,161 over to linking with about 330 spare. The enemy split
 * was measured at 3,520 on the C128, so the figure is per-target and must be
 * re-measured, not carried across.
 *
 * IDS STAY CONTIGUOUS whichever combination is on, because they are indices
 * into the image file. */
#define OVL_BASE_COUNT 11

#ifdef TREK_OVL_ENEMY
#define OVL_ENEMY       (OVL_BASE_COUNT)
#define OVL_CODE_ENEMY  OVL_CODE("enemy")
#define OVL_N_ENEMY     1
#else
#define OVL_CODE_ENEMY
#define OVL_N_ENEMY     0
#endif

#ifdef TREK_OVL_MOVE
#define OVL_MOVE        (OVL_BASE_COUNT + OVL_N_ENEMY)
#define OVL_CODE_MOVE   OVL_CODE("move")
#define OVL_N_MOVE      1
#else
#define OVL_CODE_MOVE
#define OVL_N_MOVE      0
#endif

#define OVL_COUNT  (OVL_BASE_COUNT + OVL_N_ENEMY + OVL_N_MOVE)

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
/* CORRECTED 2026-09-02: this was `#ifdef __mos__`, on the reasoning that every
   6502 target wanting overlays is built by llvm-mos "and none of the roomy
   ones are". The MEGA65 is built by llvm-mos and IS roomier -- 45,055 bytes
   against the C128's 37,823 -- so the test was asking the wrong question. It
   is now an explicit opt-in, and each platform's Makefile says whether it
   wants windows. Both current ports do; a target with 66K of contiguous RAM
   will not. */
#ifdef TREK_OVERLAYS
#define OVL_CODE(sec) __attribute__((noinline, section(".ovl." sec)))
#else
#define OVL_CODE(sec)
#endif

/* RULE 3's STUB, and NOINLINE is the whole point of it.
 *
 * `ovl_load(OVL_TITLE); ui_title();` written inline inside a long function
 * lets the compiler put the 8 in a register EARLY and reload it at the call --
 * and main()'s outer loop spans a whole game, so that register has to survive
 * every command, every file read and every library call in between. It did
 * not: on the MEGA65 the second pass loaded overlay 0 instead of 8, jumped
 * into it, and the end-of-game screens looped for ever instead of asking
 * "play again". MEASURED 2026-09-03 by logging every ovl_load request.
 *
 * Inside a stub the id is an immediate two instructions from the call, so
 * there is nothing to hold and nothing to clobber. Both ports compile the
 * same pattern -- `lda <reg>; jsr ovl_load` at the loop top -- so both were
 * exposed; only the MEGA65 had a library careless enough to trip it.
 *
 * It expands to nothing off-target, where ovl_load is already empty. */
#ifdef TREK_OVERLAYS
#define OVL_LOADER static __attribute__((noinline)) void
#else
#define OVL_LOADER static void
#endif

/* Makes `which` resident, and is IDEMPOTENT: asking for the overlay that is
   already loaded costs nothing, so a stub may call it on every entry. A
   platform with no overlays implements this as an empty function. */
void ovl_load(uint8_t which);

#endif
