#ifndef PLANET_H
#define PLANET_H

#include <stdint.h>
#include "trek.h"

/* Planets, and the ORBIT / LAND / USE chain that hangs off them.
 *
 * A SEPARATE MODULE from trek.c for the reason serial.c is one: the linker
 * takes whole modules, so a port that never builds the planet chain should
 * not have to carry it. trek.c calls exactly two things here -- planet_new()
 * when a game starts and planet_place() when a quadrant is built -- so the
 * dependency runs one way and is two symbols wide.
 *
 * The core still formats no text. Every message the original prints for this
 * chain is in reference/strings.txt and is the UI's business; what is here is
 * the model, the gates and the outcomes.
 */

/* --------------------------------------------------- the list, not a byte
 *
 * MEASURED 2026-08-21 off the MAIN VIEWER's PLANET LIST page:
 *
 *     5-4N Gallista-5      6-4N Cygnus-6      6-4M Gallista-6
 *     7-1N Andromeda-7     7-6O Sigma-7
 *
 * QUADRANT 6-4 HOLDS TWO OF THEM. That rules out the obvious model -- one
 * byte per galaxy cell, beside gal_stars and gal_base -- and it is why this
 * is an array of records with a quadrant in each rather than an array
 * indexed by quadrant. The original plainly keeps a list too: it has a page
 * that prints one.
 */

/* DERIVED from the ancestor's setup.c, which rolls its uninhabited planets as
 * MAXUNINHAB/2 + (MAXUNINHAB/2+1)*Rand() with MAXUNINHAB = 10 -- five to ten.
 * The one EGA Trek galaxy whose PLANET LIST we have read held FIVE, which is
 * that range's exact minimum. That is the same shape of evidence that settled
 * the enemy count (see ENEMY_BASE in trek.h): a reading landing on a model's
 * endpoint is worth more than one landing in its middle.
 *
 * The ancestor's other planet population, its NINHAB inhabited worlds, is
 * NOT reproduced. That is a GALSIZE*GALSIZE/2 flood of class-M worlds behind
 * an option flag, and EGA Trek shows no sign of it: seven names in the whole
 * binary, a list page that fits on two lines, and a settlement that is a
 * per-planet find rather than a category of planet. */
/* READ OUT OF THE BINARY 2026-08-26, fn 0x04FD1 at 0x052B3:
 *
 *     count = 10 + Random(10)                         ; 10..19 planets
 *     qy,qx = Random(8)+1; retry while ARRAY[q] != 0   ; ONE PER QUADRANT
 *
 * Two guesses died here. The ancestor's five-to-ten was the wrong population
 * to derive from, and a photograph of the original's PLANET LIST then pushed
 * this port to a provisional twelve-to-twenty-two. The answer is TEN TO
 * NINETEEN, and the photograph is consistent with it -- eleven planets on
 * page 602 with the rest on 601.
 *
 * And planets are ONE PER QUADRANT: the generator retries the roll while the
 * quadrant is occupied. This port modelled a LIST because a 2026-08-21 note
 * transcribed the PLANET LIST page as three columns and showed quadrant 6-4
 * twice; the photograph shows one column and every quadrant distinct. The
 * list survives here only because it also carries the sector and the scan
 * flags, which the original keeps elsewhere. */
#define PLANET_MIN       10  /*@BINARY*/
#define PLANET_SPREAD    10      /* trek_rand_n(10) gives 0..9, so 10..19 */  /*@BINARY*/
#define PLANET_MAX       19  /*@BINARY*/

#define PLANET_NONE    0xFF      /* "no planet" / "not orbiting one" */  /*@ID*/

/* A PLANET'S NAME IS ENTIRELY DETERMINED BY ITS QUADRANT. Nothing about it
 * is stored, which is why there are exactly EIGHT names for eight columns:
 *
 *     name  = planet_name[quad_x]        (1-based column, so index quad_x-1)
 *     digit = quad_y                     (1-based row)
 *
 * READ OUT OF THE BINARY 2026-08-26 in fn 0x151D0, the evacuation message:
 * `mov ax,[0x1E1E]; mov dx,13; mul dx; add di,0x1075` -- the quadrant COLUMN
 * times the name table's stride of thirteen. The row is appended separately
 * with Str().
 *
 * ELEVEN FOR ELEVEN against the PLANET LIST photograph:
 *
 *     5-1 Andromeda-5     6-2 Ceti Alpha-6    7-4 Gallista-7
 *     5-4 Gallista-5      6-5 Gamma Regula-6  7-8 Xevious-7
 *     5-5 Gamma Regula-5  6-7 Vega-6          8-1 Andromeda-8
 *     5-6 Sigma-5         7-2 Ceti Alpha-7
 *
 * The row rule was already measured seven times; the COLUMN rule is new and
 * explains what the seven-sample note could not -- why Gallista appeared
 * twice (columns 4 and 4) and why the table has exactly eight entries.
 *
 * So the Planet record carries no name field. An earlier one rolled a random
 * name index, which could put Xevious in column 1. */
#define PLANET_NAMES      8  /*@BINARY*/
extern const char *const planet_name[PLANET_NAMES];

/* Planet class, the Star Trek convention, printed as a letter with no space
 * between the quadrant and the name: "6-4M Gallista-6".
 *
 * CONFIRMED from the binary: `Random(3) + 1`, a flat third, and the ORBIT
 * routine maps 1 to "M.", 2 to "N.", 3 to "O." The eleven-planet photograph
 * that read six N to three M and two O was noise, as eleven samples of a flat
 * third can be. Kept flat, and now for a reason.
 *
 * THE CLASS IS NOT DISPLAY ONLY -- see PFIND_* below. It decides how likely
 * the planet is to be worth landing on, and this port had a note here saying
 * that was unmeasured. It is measured now. */
#define PCLASS_M          0  /*@ID*/
#define PCLASS_N          1  /*@ID*/
#define PCLASS_O          2  /*@ID*/
#define PCLASS_COUNT      3  /*@ID*/
extern const char planet_class_letter[PCLASS_COUNT];

/* What ORBIT's scan finds, and what LAND then collects. These are the four
 * SCIENCE lines in the binary, in the binary's own order:
 *
 *     "Scanners show a [destroyed ]settlement on the planet."
 *     "Scanners show a Mongol supply station on planet."
 *     "Scanners indicate the presence of energium on planet."
 *     "Scanners indicate nothing of interest on planet."
 *
 * "destroyed " is an INSERT into the settlement line, not a fifth find --
 * which is why a ruined settlement is a flag below rather than its own kind.
 * A settlement is destroyed by the same thing that destroys a base: an
 * evacuation deadline nobody met. ~~That event is NOT built here~~ -- it IS,
 * and has been since 2026-08-27: planet_settlement_lost() compares the clock
 * against planet_evac_end and ORBIT inserts the word. See the note on
 * PF_RUINED for why it is derived rather than stored. */
#define PFIND_SETTLERS    0  /*@ID*/
#define PFIND_MONGOL      1      /* a Mongol supply station */  /*@ID*/
#define PFIND_ENERGIUM    2  /*@ID*/
#define PFIND_NOTHING     3  /*@ID*/

/* READ OUT OF THE BINARY 2026-08-26, and it replaced an invented split.
 *
 * The original keeps ONE BYTE per quadrant at DS:0x24A9 holding
 * **find * 10 + class**, and ORBIT decodes it by dividing by ten:
 *
 *     > 20   Mongol supply station
 *     > 10   energium
 *     else   nothing of interest
 *
 * Generation, at 0x0531A:
 *
 *     if (class <= Random(5))      byte += 10     ; energium
 *     else if (Random(2) == 0)     byte += 20     ; Mongol supply station
 *
 * AND NOTHING RAISES IT AGAIN. Read 2026-08-28: the only writes outside
 * generation are 0x00E88F and 0x00E905, both in the landing code and both
 * the same instruction -- `byte = byte mod 10`, stripping the find and
 * keeping the class. That is PF_TAKEN, which this file arrived at without
 * knowing the original does it in the same byte.
 *
 * **THE FIND DEPENDS ON THE CLASS**, with class as 1=M, 2=N, 3=O:
 *
 *     class M   energium 4/5   Mongol 1/10   nothing 1/10
 *     class N   energium 3/5   Mongol 2/10   nothing 2/10
 *     class O   energium 2/5   Mongol 3/10   nothing 3/10
 *
 * so a class-M world is worth landing on four times in five and a class-O
 * world two times in five. Overall, with class flat: energium 60%, Mongol
 * 20%, nothing 20%. This port shipped energium at one in three, derived from
 * the ancestor, with the rest invented -- wrong twice over, and it had no
 * class dependence at all.
 *
 * SETTLERS ARE NOT IN THIS BYTE, AND NOW WE KNOW WHY: THERE IS EXACTLY ONE
 * SETTLED PLANET IN A GALAXY, and it is not stored per-quadrant at all.
 *
 * Generation keeps its quadrant in a pair of globals, DS:0x1E1C and 0x1E1E,
 * written inside the ENERGIUM branch of the planet loop -- so every energium
 * planet overwrites it and THE LAST ONE WINS. The settled planet is therefore
 * always an energium planet.
 *
 * ORBIT tests the ship's quadrant against that pair BEFORE it decodes the
 * per-quadrant byte, so the settled world gets BOTH scan lines: "Scanners
 * show a settlement on the planet." and then the energium line. And
 * "destroyed " is inserted when a stardate at [0x1D42] has passed a deadline
 * at [0x1DA2].
 *
 * The same globals are read by fn 0x15105 ("A distress signal is being
 * received...") and fn 0x151D0 (", requests evacuation. They can only hold
 * out until "), which is the whole evacuation mechanic and the source of the
 * +200 rescue line.
 *
 * Two earlier readings of this are retracted in MEASURED.md: settlers as a
 * fourth per-planet find (this file's invention) and a twelve-entry table at
 * DS:0x1188 (which is the SYSTEM names). One planet, one pair of globals.
 *
 * PF_SETTLED below carries it. The DEADLINE is the real at [0x1DA2] and its
 * schedule slot is [0x1D9C]. `fn 0x0E3A1` opens by testing the ship's quadrant
 * against the pair above and, while [0x1DA2] is still ahead of the stardate,
 * prints "Planet settlers found..." / "Evacuating settlers..." and sets that
 * slot to 9999. Both supernova routes do the same two writes, which is why
 * planet_quadrant_lost() exists.
 *
 * ~~What remains unread is where [0x1DA2] is SET, and by how much.~~ **READ
 * 2026-09-02.** Setup arms the slot at `3505.0 + Random()*3.0`, and ONLY when
 * `[0x1DF0] >= 7`. fn 0x151D0 then fires once, sets the slot to 9999 so it
 * never repeats, and writes `[0x1DA2] = stardate + 1.0 + Random()*3.0`. See
 * MEASURED.md, "Clearing the unread list with the disassembler".
 *
 * ~~NOT BUILT: this core has PF_SETTLED and the +200 rescue but no clock, so a
 * settlement cannot yet be lost to time.~~ **ALL OF IT IS BUILT.** The clock
 * had been there since 2026-08-27 -- SCHED_DISTRESS, planet_evac_end,
 * EVAC_WARNING_MIN_TENTHS -- and the sentence above was written the same day
 * without checking. DISTRESS_MIN_LEVEL closed the one real gap that afternoon.
 * Twice in one file, in one session, about the same feature. */
#define PFIND_ENERGIUM_OF_N   5   /* energium when class <= Random(5) */  /*@BINARY*/
#define PFIND_MONGOL_OF_N     2   /* else Mongol when Random(2) == 0 */  /*@BINARY*/

#define PF_SCANNED     0x01      /* ORBIT has revealed `find` to the player */  /*@ID*/
/* The find is spent. It is the original's `[0x24A9+q] = [0x24A9+q] mod 10`,
   which strips the tens digit, and BOTH landing paths do it UNGUARDED after
   the call -- 0x00E88F and 0x00E905. So every landing spends the find:
   energium mined, supplies captured, settlers evacuated, nothing found, and
   a party that was attacked and came back with none of it. */
#define PF_TAKEN       0x02  /*@ID*/
/* A settlement that was not relieved in time. The scheduled evacuation event
   ("Planet Gallista-8, quad 8-4, requests evacuation. They can only hold out
   until 3516.5.") IS built -- SCHED_DISTRESS and EV_DISTRESS, 2026-08-27 --
   and it is what arms planet_evac_end. It landed as a schedule slot and a
   message with no model change, which is what this flag and the
   ruined-settlement scan line were put here for. */
/* NO PF_RUINED. A settlement being "destroyed" is not a stored flag: ORBIT
   computes it live, comparing the stardate at [0x1D42] against the deadline
   at [0x1DA2] and inserting "destroyed " when the deadline has passed.
   Nothing in the binary ever writes such a flag. Use
   planet_settlement_lost(). */
#define PF_SETTLED     0x08  /*@ID*/

typedef struct {
    uint8_t quad;      /* galaxy cell, 0..63 -- fixed for the game */
    uint8_t sec;       /* sector cell, 0..63 -- chosen when the quadrant is
                          built, and stored rather than recomputed so that a
                          game saved in orbit reloads in the same orbit */
    uint8_t cls;       /* PCLASS_* */
    uint8_t find;      /* PFIND_* */
    uint8_t flags;     /* PF_* */
} Planet;

extern Planet  planets[PLANET_MAX];
extern uint8_t planet_count;

/* ----------------------------------------------------------- the inventory
 *
 * MEASURED off the USE AN ITEM dialog, which is a numbered list of item types
 * with quantities in brackets -- `1. Raw energium (1)` after one mining run.
 * So it is counts per type with the empty types left out, which is what this
 * array is.
 *
 * The five names are the binary's own table and appear in its order. ALL FIVE
 * now have a source: raw energium from mining, and the other four from
 * trek_capture_supplies() -- items 0..2 rolled, life support always two.
 *
 * FOUR OF THE FIVE ALSO HAVE A USE, as of 2026-09-02: raw energium, the life
 * support canister, the plasma bolt and the plasma bolt shield. Only
 * ITEM_MONGOL_ENERGIUM is still carried with nothing that spends it, and USE
 * says so rather than doing nothing. */
#define ITEM_MONGOL_ENERGIUM  0  /*@ID*/
#define ITEM_PLASMA_BOLTS     1  /*@ID*/
#define ITEM_PLASMA_SHIELD    2  /*@ID*/
#define ITEM_LIFE_SUPPORT     3  /*@ID*/
#define ITEM_RAW_ENERGIUM     4  /*@ID*/
#define ITEM_COUNT            5  /*@ID*/
extern const char *const item_name[ITEM_COUNT];
extern uint8_t inventory[ITEM_COUNT];

/* --------------------------------------------------------------- the chain */

void planet_new(void);            /* called by trek_new_game */
void planet_place(void);          /* called by trek_enter_quadrant */

/* The planet in the current quadrant nearest the given sector, or
   PLANET_NONE. Quadrant 6-4 held two, so "the planet here" is a question with
   more than one answer and the UI needs the one being addressed. */
uint8_t planet_here(void);

/* ORBIT. Adjacency, exactly as docking works, and the original's refusal says
   so: "NAVIGATION: Not adjacent to planet."

   MEASURED: it costs 0.0 stardates, and the scan is part of it -- one command
   both enters orbit and reveals the find. The manual agrees: "This will allow
   the planet to be scanned for the presence of energium crystals and other
   things." */
#define ORBIT_OK           0  /*@ID*/
#define ORBIT_NO_PLANET    1     /* none adjacent */  /*@ID*/
#define ORBIT_ALREADY      2  /*@ID*/
uint8_t trek_orbit(void);

/* Leaving orbit. Any movement does it, the same way movement breaks a dock. */
void trek_leave_orbit(void);

/* LAND, by one of the two ways down.
 *
 * MEASURED: the transporter costs 0.0 stardates and runs a four-beat
 * sequence. The manual gives the shuttlecraft's price and the reason to
 * prefer the transporter: it "takes 0.2 stardays to make the round trip
 * whereas the transporter is virtually instantaneous" (l.490-492).
 *
 * Both are 100%-or-nothing, which is the manual's rule for these two systems
 * and is already in trek_transporter_ok()/trek_shuttle_ok(). Shields up
 * refuses both: "ENGINEERING: Cannot use transporters or shuttlecraft with
 * shields up." */
#define LAND_BY_SHUTTLE      1  /*@ID*/
#define LAND_BY_TRANSPORTER  2  /*@ID*/

#define LAND_NO_ORBIT        0   /* "NAVIGATION: Not orbitting a planet" */  /*@ID*/
#define LAND_SHIELDS_UP      1  /*@ID*/
#define LAND_DAMAGED         2   /* that way down is not at 100% */  /*@ID*/
#define LAND_NOTHING         3   /* "Nothing found." */  /*@ID*/
#define LAND_ENERGIUM        4   /* "energium successfully mined." */  /*@ID*/
#define LAND_SETTLERS        5   /* "Planet settlers found... Evacuating" */  /*@ID*/
#define LAND_ATTACKED        6   /* "Landing party attacked... casualties." */  /*@ID*/
/* THIS PORT'S OWN message, not a mechanic: the original has no "already
   landed" case and simply reaches "Nothing found." a second time. It is
   returned AFTER the attack roll, so going back for another look is no safer
   than the first trip -- that part IS a mechanic. */
#define LAND_ALREADY         7  /*@ID*/
#define LAND_SUPPLIES        8   /* "Mongol supplies captured..." */  /*@ID*/

/* ------------------------------------------------- captured Mongol supplies
 *
 * BINARY 2026-08-27, `fn 0x181E8`, reached from the landing party when the
 * planet's byte exceeds 20 -- which is `find == PFIND_MONGOL`.
 *
 *     print "Mongol supplies captured..."
 *     if (Random(4) == 0) { print "none."; return }
 *     for item in 1..3:                     ; ONE roll each, not a loop
 *         if (Random(item + 2) <= 1) { inventory[item]++; print its name }
 *     inventory[4] += 2                     ; life support, ALWAYS on success
 *
 * The original's indices are one above this core's, so its items 1..3 are
 * ITEM_MONGOL_ENERGIUM, ITEM_PLASMA_BOLTS and ITEM_PLASMA_SHIELD, rolled at
 * Random(3), Random(4) and Random(5) -- two in three, two in four, two in
 * five. Raw energium is NEVER captured; life support is never rolled.
 *
 * The item table at DS:0x10D4 and the inventory bytes at DS:0x234B settle
 * that mapping: [0x234F] is life support, the byte the USE item decrements,
 * and [0x2350] is raw energium, the byte a mined planet increments. Three
 * independent reads, one set of indices.
 *
 * Returns a BITMASK of the items gained, or 0 for "none." */
#define CAPTURE_NONE_OF_N      4   /* Random(4) == 0 and you get nothing */  /*@BINARY*/
#define CAPTURE_ITEMS          3   /* only items 0..2 are rolled */  /*@BINARY*/
#define CAPTURE_ODDS_BASE      3   /* item i rolls Random(i + 3) */  /*@BINARY*/
#define CAPTURE_ODDS_HIT       1   /* and a 0 or a 1 is a hit */  /*@BINARY*/
#define CAPTURE_LIFE_SUPPORT   2   /* always, on any success */  /*@BINARY*/
uint8_t trek_capture_supplies(void);

/* `casualties` receives the losses on LAND_ATTACKED, so the UI can print the
   number the original prints. Pass NULL if the caller does not care. */
uint8_t trek_land(uint8_t how, uint16_t *casualties);

/* A landing party that walks into a Mongol supply station comes back short.
 *
 * READ OUT OF THE BINARY 2026-08-26, fn 0x0E3A1, decoded from raw bytes at
 * 0x0E435:
 *
 *     mov ax,5; Random(5); or ax,ax; jnz  ->  attacked only on a ZERO roll
 *     "Landing party attacked..."
 *     mov ax,5; Random(5); add ax,2       ->  casualties = 2..6
 *
 * So the count is `Random(5) + 2`, and this port shipped 1..10 invented. Two
 * casualties is the floor: a raided landing party ALWAYS loses at least two.
 *
 * ~~The gate is a one-in-five roll, and it is certain as a gate. WHICH case it
 * guards is not yet read ... This port keeps it on the Mongol find.~~
 * **SUPERSEDED THE NEXT DAY -- read the paragraph below, not this one.** It is
 * a general landing hazard, it replaces the find rather than accompanying it,
 * and the reasoning above ("the block SITS BETWEEN two cases") is the
 * order-not-branches mistake in its purest form. Kept as the record of how it
 * was got wrong. */
/* THE ATTACK IS A GENERAL LANDING HAZARD, not a Mongol one -- BINARY
 * 2026-08-27, fn 0x0E3A1 at 0x00E435. It is rolled once for every landing
 * that is not the settlement rescue, BEFORE the find is looked at.
 *
 * ~~and execution CONTINUES afterwards: you can be attacked and still come
 * back with something.~~ **WRONG, corrected 2026-09-02.** The attack ends
 * with `jmp 0xE4DD`, the common tail that the supply capture, the energium
 * line and "Nothing found." all reach, and fn 0x0E3A1 returns at 0xE4EE.
 * AN ATTACKED PARTY BRINGS BACK NOTHING. The 2026-08-27 read got the roll and
 * the casualties right and then described the control flow from the order of
 * the blocks instead of from the branch -- the same mistake this project has
 * now made three times.
 *
 * The settlement rescue is different again: it ends `jmp 0xE4EB`, straight to
 * the epilogue, so a rescue skips the attack roll AND the tail. */
#define LANDING_ATTACK_OF_N    5   /* attacked when Random(5) == 0 */  /*@BINARY*/
#define LANDING_CASUALTY_MIN   2  /*@BINARY*/
#define LANDING_CASUALTY_SPAN  5   /* Random(5) + 2, so 2..6 */  /*@BINARY*/

/* --------------------------------------------------------------------- USE
 *
 * The gate is the one place in this chain that is CONFIRMED rather than
 * derived, and the manual states it outright: "regulations prohibit the use
 * of raw energium except in extreme emergencies; your shields must be under
 * 50% and main energy under 20%" (l.351-354).
 *
 * 20% of ENERGY_MAX is 1000, WHICH IS THE ANCESTOR'S OWN THRESHOLD TO THE
 * UNIT -- its usecrystals() refuses at `game.energy >= 1000`. Two independent
 * documents agreeing on a number is the strongest evidence in this file.
 *
 * MEASURED, and it is the reading worth staring at: energy went 500 to 7435
 * and shields 300 to 2500. SEVEN THOUSAND is well above the 5000 maximum
 * every other mechanic clamps to -- energium OVERCHARGES the ship, and that
 * is the whole point of a mechanic gated on being nearly dead.
 *
 * ------------------------------------------------------------------------
 * READ OUT OF THE BINARY 2026-08-26, replacing an invented model.
 *
 * The routine is fn 0x0ED3B and the outcome is `Random(6)`, decoded from raw
 * bytes at 0x0EE80:
 *
 *     roll 0        DEFECTIVE   energy -= Random(1000), floored at zero
 *     roll 1, 2     DUD         nothing happens
 *     roll 3, 4, 5  GOOD        calls fn 0x0934E
 *
 * A crystal WORKS HALF THE TIME, duds a third of the time and hurts one time
 * in six. This port shipped 5% escalating (the ancestor's cryprob, doubled
 * per use) with a 10% dud invented outright. Both wrong, and THE ESCALATION
 * DOES NOT EXIST -- there is no state here, each crystal is an independent
 * roll of six. planet_defect_pct() and its save-file byte are gone with it.
 *
 * The defective branch also does something other than what was built: it
 * SUBTRACTS ENERGY and floors it at zero. It does not wreck the converter.
 * The port called trek_wreck_system(SYS_CONVERTER) on the strength of the
 * message "Damage to main energy systems.", which reads like system damage
 * and is not.
 *
 * The success handler, fn 0x0934E, is two lines of arithmetic:
 *
 *     energy  += V * (700 + Random(700))
 *     shields += V * (300 + Random(300))
 *
 * -- so shields are TOPPED UP by an amount rather than set to full as this
 * port had it. The measured 300-to-2500 was that amount reaching the ceiling.
 *
 * V is [0x1DCC], and the one measurement PINS IT TO FIVE. The gain was 6935,
 * and 6935 = V x (700 + r) with r in 0..699 has exactly ONE solution in V:
 * 5 x 1387. No other divisor lands in the window.
 *
 * The obvious guess was that V is the command level, and this file used that
 * for about an hour. The evidence is against it: run 1's tooling starts games
 * at level 3, and V is 5. So V is taken as the constant the measurement gives
 * rather than a level dependence the measurement contradicts.
 *
 * What would settle it beyond doubt is one emulator run -- load a crystal at
 * a KNOWN level and divide the gain -- because a second sample at a different
 * level separates "constant 5" from "the level" immediately. Until then this
 * reproduces the only reading we have, exactly. */
#define CRYSTAL_V              5     /* [0x1DCC]; see above */  /*@MEASURED*/
#define CRYSTAL_ROLL_OF_N     6  /*@BINARY*/
#define CRYSTAL_DEFECT_ROLL   0      /* roll 0        -- one in six */  /*@BINARY*/
#define CRYSTAL_DUD_TO        2      /* rolls 1..2    -- two in six */  /*@BINARY*/
                                     /* rolls 3..5 are good -- three in six */
#define CRYSTAL_DEFECT_LOSS 1000     /* energy -= Random(1000) */  /*@BINARY*/
#define CRYSTAL_ENERGY_BASE  700     /* both scaled by V, per the binary */  /*@BINARY*/
#define CRYSTAL_ENERGY_SPAN  700  /*@BINARY*/
#define CRYSTAL_SHIELD_BASE  300  /*@BINARY*/
#define CRYSTAL_SHIELD_SPAN  300  /*@BINARY*/

#define USE_NO_ITEM        0   /* "No energium crystals are available to load" */  /*@ID*/
#define USE_REFUSED        1   /* the First Officer's regulations speech */  /*@ID*/
#define USE_GOOD           2  /*@ID*/
#define USE_DEFECTIVE      3   /* main energy systems damaged */  /*@ID*/
#define USE_DUD            4   /* nothing happens */  /*@ID*/

/* The name of a planet, derived from its quadrant. Never stored. */
#define PLANET_NAME_OF(q)  (planet_name[(q) & 7])  /*@BINARY*/
#define PLANET_DIGIT_OF(q) (((q) >> 3) + 1)  /*@BINARY*/

/* The evacuation deadline, fn 0x151D0:  stardate + 3.0*Random + 1.0, so one
   to four stardates of warning. [0x1D9C] is set to 9999.0 on the way out,
   which is this game's "never again" -- a galaxy gets ONE evacuation. */
/* THE DEADLINE DOES NOT START AT GAME START -- BINARY 2026-08-27, fn 0x151D0.
 * The settlers do not need help until their DISTRESS CALL fires, and that is
 * a scheduled event of its own at DS:0x1D9C, set at 0x00589B to the real
 * 3505.0 plus an argument-less Random times 3.0. Only when it fires does the
 * handler set the deadline at [0x1DA2] to `stardate + 1.0 + Random*3.0` and
 * print "COMMUNICATIONS: Planet <name>, quad <r>-<c>, requests evacuation.
 * They can only hold out until <date>."
 *
 * The one-to-four stardates below were already right; this core just started
 * counting them from turn zero, so a settlement could be lost by stardate
 * 3501 where the original cannot lose one before 3506. */
#define EVAC_WARNING_MIN_TENTHS   10  /*@BINARY*/
#define EVAC_WARNING_SPAN_TENTHS  30  /*@BINARY*/
/* AND IT IS A LEVEL 3 MECHANIC. `cmp word [0x1DF0], 7 / jl` at 0x005876 skips
   the arming entirely below it -- V = level + 4, so levels 1 and 2 never hear
   from their settlers at all and the +200 rescue is out of reach. This port
   armed it at every level until 2026-09-02. */
#define DISTRESS_MIN_LEVEL         3  /*@BINARY*/
#define DISTRESS_AT_TENTHS        50   /* 3505.0, five stardates in */  /*@BINARY*/
#define DISTRESS_SPAN_TENTHS      30   /* plus Random*3.0 */  /*@BINARY*/

/* When the settlers run out of time, as a stardate in tenths. */
extern uint16_t planet_evac_end;

/* A quadrant has been burnt out by a supernova. BINARY 2026-08-27: both
   supernova routes clear the settled planet's quadrant pair [0x1E1C]/[0x1E1E]
   and set its evacuation slot [0x1D9C] to the real 9999 -- never. That pair
   is the SETTLED PLANET, not a base: `fn 0x0E3A1` tests the ship's quadrant
   against it before anything else and answers "Planet settlers found..." /
   "Evacuating settlers...", and the same two writes appear there when you
   DO evacuate. A settlement inside a supernova is gone, and nobody is coming
   for it. */
void planet_quadrant_lost(uint8_t q);

/* Has the settlement been lost? DERIVED, never stored -- see PF_SETTLED. */
uint8_t planet_settlement_lost(void);

/* Is the ship in the settled planet's quadrant with time still on the clock?
   fn 0x15105 prints the distress signal on exactly this test, so the signal
   is LOCATION-TRIGGERED rather than a scheduled event -- you hear it by
   flying there, which is a different mechanic from the base-under-attack
   warning and was worth finding out before building one as the other. */
uint8_t planet_distress_here(void);

/* The Y/N confirmation ENGINEERING asks for is the UI's; by the time this is
   called the captain has said yes. `ev`/`max` carry the system damage on
   USE_DEFECTIVE in the usual way, and may be NULL/0. */
uint8_t trek_use_energium(TrekEvent *ev, uint8_t max);

/* Whether the gate is open, so the UI can ask before it offers. */
uint8_t trek_energium_allowed(void);

#endif
