#ifndef TREK_H
#define TREK_H

#include <stdint.h>

/* 8x8 Trek -- shared game core.
 *
 * Platform-independent by construction: no float, no double, no malloc, no
 * long, and explicit-width types throughout (`int` is 16-bit under cc65 but
 * 32-bit on the Amiga's 68000, so it is never used for a stored value).
 * Fractions are 8.8 fixed point or plain tenths; there is no runtime sqrt.
 *
 * Coordinates are ZERO-BASED internally and vertical-first (y then x), the
 * latter matching the original's own convention -- the manual's "6,2,3,5
 * moves you to quadrant 6,2 sector 3,5" is vertical first. The UI layer is
 * responsible for presenting them 1-based, as the original does.
 */

#define GAL_DIM     8            /* 8x8 quadrants */  /*@CONFIRMED*/
#define QUAD_DIM    8            /* 8x8 sectors per quadrant */  /*@CONFIRMED*/
#define GAL_CELLS   (GAL_DIM * GAL_DIM)  /*@ID*/
#define QUAD_CELLS  (QUAD_DIM * QUAD_DIM)  /*@ID*/

/* Sector cell contents. One byte per cell; the UI maps these to glyph and
   colour (see core/ega.h for the colour rules, which are game rules). */
#define SEC_EMPTY       0  /*@ID*/
#define SEC_STAR        1  /*@ID*/
#define SEC_SHIP        2  /*@ID*/
#define SEC_BASE        3  /*@ID*/
#define SEC_PLANET      4  /*@ID*/
#define SEC_BATTLESHIP  5  /*@ID*/
#define SEC_COMMAND     6  /*@ID*/
#define SEC_SCOUT       7  /*@ID*/
#define SEC_SUPPLY      8  /*@ID*/

#define SEC_IS_ENEMY(c) ((c) >= SEC_BATTLESHIP && (c) <= SEC_SUPPLY)  /*@ID*/

/* Base types, as the long-range scanner reports them (manual l.291):
   1 StarBase, 2 research station, 3 supply depot. */
/* How many StarBases a galaxy holds: `11 - V` at 0x0053C5, where V is
   [0x1DF0]. V IS SETTLED -- the setup prompt at 0x015050 rejects anything
   outside 1..5 and then does `[0x1DF0] = n + 4`, and fourteen lines later
   indexes the rank names at DS:0x102E by `(V - 4) * 14`, which lands on
   "Lt. Commander" for V = 5. So V = command level + 4, and this gives six
   StarBases at level 1 and two at level 5.

   That also disposes of the `cmp V, 9` at 0x005482 (level 5) and the `V > 7`
   that gates the spy (level 4 and up): both are level tests written in the
   offset form. No emulator run needed after all. */
#define STARBASES_AT_LEVEL(l)  (11 - ((l) + 4))  /*@BINARY*/

#define BASE_NONE       0  /*@ID*/
#define BASE_STARBASE   1  /*@ID*/
#define BASE_RESEARCH   2  /*@ID*/
#define BASE_SUPPLY     3  /*@ID*/

/* CONFIRMED against the original running in DOSBox-X, 2026-08-15, by reading
   its ENGINEERING REPORT (the E command) at the start of a level-3 game:

       1) Main Energy:      5000.0   100%
       2) Impulse Engines:   500.0   100%
       3) Shields:          2500.0   100%

   Energy is held in THREE separate pools, not one, and the E command diverts
   between them -- which is why the manual's energy dialog is numbered 1/2/3.
   Shields start FULL. Shield charge and shields-raised are different things;
   an earlier draft here conflated "not raised" with "no charge", and the
   manual's remark about raising shields drawing "a small amount of energy
   from the main energy banks" (l.339) is the raise cost, not the charge.

   The percentage beside each figure is CHARGE, not state of repair -- 4500
   of 5000 reads 90%, 2000 of 2500 reads 80%. Damage is shown by colouring
   the row red instead ("Systems marked in red are damaged"). So these three
   maxima are confirmed values, not guesses. */
#define ENERGY_START    5000  /*@CONFIRMED*/
#define ENERGY_MAX      5000  /*@CONFIRMED*/
#define IMPULSE_START    500  /*@BINARY*/   /* docking writes 500.0 flat */
#define IMPULSE_MAX      500  /*@BINARY*/   /* USE caps [0x1D4E] here, 0x00FAF6 */
#define SHIELD_START    2500  /*@CONFIRMED*/
#define SHIELD_MAX      2500     /* manual l.501, confirmed on screen */  /*@CONFIRMED*/

/* MEASURED at command level 3: nine, shown as a 3x3 array of red stars below
   the shields dial, one per torpedo. Firing one put it out. Three tubes and
   nine torpedoes is a coherent bit of design; an earlier guess of 10 was
   wrong. Whether the count varies by level is untested.

   BINARY 2026-08-27: docking writes `[0x1DBE] = 9` at 0x00F0FF, a flat
   integer with no level term, which settles the "varies by level" question
   for the resupply at least. [0x1DBE] is the torpedo count -- it is
   decremented by the number fired at 0x00B90F, and at 0x009E93 a zero
   picks "May I suggest that we get out of here?" over "...the use of a
   torpedo?". */
#define TORPS_START      9  /*@BINARY*/

/* HOW MANY MONGOLS, read out of the binary 2026-08-26 at 0x005181:
 *
 *     total = Random(10) + ((level + 1) * 8 * (100 - Random(10))) div 100
 *
 * plus THREE for the StarBase that starts the game under attack (0x0054D0),
 * which is base number one always and EVERY base at level five -- and only
 * when that base's quadrant has no enemies in it already.
 *
 * THE FITTED `10 + 8*level + rand(0..8)` WAS A COINCIDENCE OF THE SAMPLE.
 * It reproduced level 3's ten readings as exactly 34..42 because that is the
 * span ten draws happened to occupy; the real range there is 32..44. All
 * nineteen readings across five levels fall inside the law above, and its
 * means track theirs. Three FITTED constants for one expression that was
 * never quite the right shape -- which is what FITTED is supposed to warn
 * about.
 *
 * The (100 - Random(10))/100 term is a 0-to-9 percent shave off the level
 * base, so the count is BIASED DOWNWARD from (level+1)*8 rather than spread
 * evenly around it. Nothing in the readings could have shown that. */
#define ENEMY_PER_LEVEL       8   /* (level + 1) * 8 */  /*@BINARY*/
#define ENEMY_SHAVE_OF_N     10   /* less 0..9 percent of it */  /*@BINARY*/
#define ENEMY_SPREAD_OF_N    10   /* plus a flat Random(10) */  /*@BINARY*/
#define ENEMY_PER_QUADRANT    4   /* Random(4)+1 placed per quadrant, once */  /*@BINARY*/
#define ENEMY_SIEGE           3   /* extra, at the base that starts besieged */  /*@BINARY*/

/* MONGOL COMMANDERS ARE PER-QUADRANT STATE, not a roll made on arrival.
 * `DS:0x23E9` is an 8x8 byte array and the quadrant fill at 0x016133 makes
 * the first `commanders[q]` ships in a quadrant Commanders -- so which
 * quadrant holds one is stable across visits, where this port re-rolled every
 * entry. It is set during generation (0x0052A2), added to by reinforcements,
 * and moved between quadrants by the enemy-movement code, which increments
 * one entry and decrements another.
 *
 * AND THERE ARE NONE BELOW LEVEL 3: `cmp [0x1DF0], 7 / jl` guards the whole
 * commander branch. */
#define COMMANDER_MIN_LEVEL   3   /* below this, none exist */  /*@BINARY*/
#define COMMANDER_IN_N        3   /* a quadrant with >1 enemy gets one ... */  /*@BINARY*/
#define COMMANDER_OF_N        7   /* ... three times in seven */  /*@BINARY*/

/* THE TRACTOR BEAM, read out of fn 0x0C609 at 0x00D83F on 2026-08-26. It is
 * not an event and never was:
 *
 *     after a warp move, for every quadrant in the BOUNDING RECTANGLE of the
 *     trip -- min(old,new) to max(old,new) on both axes --
 *         if that quadrant holds enemies AND a Commander
 *             if (Random(10) > 7)                    -- two times in ten
 *                 pull the ship there, and stop looking
 *
 * "Lexington caught in long range tractor beam. Pulled to quadrant N-N."
 *
 * So flying PAST a Commander is what gets you yanked out of warp into its
 * lap, and a long jump across a defended stretch of the galaxy is genuinely
 * more dangerous than a short one. A scheduled event could not express that.
 * The bounding rectangle is the original's own approximation of the path --
 * it does not trace the line. */
#define TRACTOR_OF_N         10   /* Random(10) ... */  /*@BINARY*/
#define TRACTOR_ABOVE         7   /* ... must exceed 7 */  /*@BINARY*/
#define ENERGY_PER_DAY  400      /* manual l.265, at 100% repair */  /*@CONFIRMED*/

/* Stardates are carried in tenths, so these exceed cc65's 16-bit signed int
   and must be written unsigned -- an unsuffixed 35000 promotes to long,
   which this core forbids (cl65 warns "Constant is long"). */
#define STARDATE_START  35000U   /* 3500.0 */  /*@CONFIRMED*/
#define MISSION_TENTHS  300U     /* 30 stardates to secure the sector */  /*@CONFIRMED*/
#define WARP_MIN        10       /* tenths: warp 1.0 */  /*@CONFIRMED*/
#define WARP_MAX        80       /* tenths: warp 8.0, emergency (manual l.250) */  /*@CONFIRMED*/
#define WARP_CRUISE     60       /* tenths: warp 6.0, safe cruising */  /*@CONFIRMED*/
#define WARP_START      10       /*@CONFIRMED*/ /* CONFIRMED: the original opens at warp 1.0,
                                    not the 5.0 an earlier draft assumed */

/* Travel must cost more than the converter replaces while you travel, or
   energy stops being a resource and the whole supply/docking loop collapses.
   The manual is explicit (l.261-264): "you will most likely be using energy
   faster than you can regenerate it."

   The first draft of these was too cheap and a one-quadrant hop at warp 5
   turned a profit; the core test now asserts a net loss. Units work out to
   energy = distance_in_quadrants * warp_factor * 10 * WARP_ENERGY_SCALE.

   Warp cost is MEASURED at two points -- roughly 194 units for one quadrant
   at warp 5 and 710 at warp 8 -- giving cost = 1.5 * distance * warp^3. The
   exponent (2.76 from those two) is well supported; the 1.5 is not precise.
   It lives in trek_move_warp rather than here because the expression has to
   be staged to stay inside 16 bits. Earlier linear and quadratic guesses
   predicted 320 and 512 at warp 8 and were both refuted.

   IMPULSE_ENERGY_UNIT is MEASURED: sector 6,6 -> 6,5, a distance of exactly
   1, cost 6.3 units (493.8 -> 487.5); an earlier hop cost 6.2. The previous
   value of 60 was ten times too expensive. */
#define IMPULSE_ENERGY_UNIT   6  /* per sector -- measured, see above */  /*@MEASURED*/

/* ------------------------------------------------------- movement timing */

/* MEASURED 2026-08-25, ten samples, and it is the whole in-quadrant law:
   time = distance_travelled / 24 stardates, INDEPENDENT of the warp factor
   (4 sectors cost 0.1667 at warp 1.0 and 0.1666 at warp 3.0).

   That is well under a tenth of a stardate per sector -- 0.0417 -- which is
   why ship.time_frac exists: the clock is carried in tenths and a one-sector
   hop does not reach one. The previous value was 0.1 per sector, PROVISIONAL
   and 2.4x too dear. */
#define IMPULSE_STARDATE_DIV  24  /*@MEASURED*/

/* MEASURED 2026-08-25: time = 11 * distance_in_quadrants / warp^2 stardates,
   with the distance taken over ABSOLUTE sector positions and divided by 8 --
   not over quadrant indices, which is what this port used to do and which
   cannot produce the 17-sector reading below.

   Seven samples across four warp factors, and the fit is exact rather than
   close. The new one is the cleanest: a quadrant change blocked after four
   sectors -- half a quadrant -- at warp 1.0 cost 5.5000, which is 11 * 0.5.
   Re-reading run 1's five warp-3 readings against it resolves every one to an
   exact lattice distance:

       0.8227 -> sqrt(29) sectors   0.9663 -> sqrt(40)   1.2222 -> 8
       2.5972 -> 17                 2.7330 -> sqrt(320)

   Five arbitrary fractions landing on integer-difference distances is not
   something a wrong constant does. The constant this replaces was ~9.98,
   fitted from two readings. */
#define WARP_STARDATE_NUM     11  /*@MEASURED*/

/* Weapons.
 *
 * Laser damage is CONFIRMED exactly, by writing four predictions down before
 * firing and having all four come back to the unit:
 *
 *     damage = energy * efficiency * (1 - distance / 12)
 *
 * From sector 4,4 on a fresh level-3 game with every system at 100%: 500 at
 * distance 1.414 gave 441 (predicted 441.1), and 250 at 2.000, 4.472 and
 * 5.000 gave 208, 157 and 146 (predicted 208.3, 156.8, 145.8). The two
 * energies in one volley confirm linearity in energy at the same time.
 *
 * Rounding is to NEAREST, not truncation -- 156.83 printed as 157 and 145.83
 * as 146, both of which truncation would have put one lower.
 *
 * Falloff is LINEAR in distance. Inverse-square, and with it the Super Star
 * Trek combat math we had kept as a fallback, is refuted. Note that 12
 * exceeds the 9.9 diagonal of an 8x8 quadrant, so a laser always delivers at
 * least 17% of its energy however far away the target is.
 *
 * Enemy fire uses the SAME law; what differs is the energy each ship commits,
 * which falls as the ship takes damage. See MEASURED.md.
 *
 * `efficiency` is one number combining heat and battle damage, as the
 * original's single gauge does. MEASURED 2026-08-24: the damage half is
 * EXACTLY linear in the Lasers repair percentage -- 100/75/50/25 percent dealt
 * 263/197/131/66 against the same target, ratios 1.0017/0.7503/0.4989/0.2514
 * -- and the heat half never fires at all (see LASER_HEAT_MAX below). So
 * effectiveness is the laser percentage, full stop.
 *
 * LASER_RANGE_ZERO is in whole sectors; distances are 8.8 fixed point, so
 * comparisons against it must shift. */
/* CONFIRMED FROM THE BINARY 2026-08-26, fn 0x09CC1 at 0x009F6C:
 *
 *     r      = Sqrt(((ey-sy)/8)^2 + ((ex-sx)/8)^2)      -- distance/8
 *     damage = Round(amount * (1.5 - r) * 6.67 * lasers% / 1000)
 *
 * 8.0, 1.5 and 1000.0 decode exact; the fourth constant decodes 6.6700 and is
 * plainly a typed decimal rather than 20/3. Fold them: `(1.5 - d/8) * 6.67 *
 * pct / 1000` is `(1 - d/12) * pct/100` times 1.0005. So the linear falloff
 * to zero at TWELVE sectors is the original's, exactly, and this port is
 * five hundredths of a percent light -- under one unit on any shot, which is
 * why all five measured readings reproduced. Left as is rather than carrying
 * Anderson's rounding error into integer arithmetic that cannot express it. */
#define LASER_RANGE_ZERO     12  /* distance at which damage would reach 0 */  /*@BINARY*/

/* Full scale on the original's laser Temp gauge, whose printed scale reads
   "0 ... 1000 1500".
 *
 * MEASURED, twice over. Firing 700 in a volley cost nothing, and the FORTRAN
 * ancestor this game is a port of uses the same number as a hard threshold:
 *
 *     if (rpow > 1500) { chekbrn = (rpow-1500)*0.00038; ... }
 *
 * where rpow is the total energy fired in ONE phaser command. At or below
 * 1500 there is no penalty at all, which is why no gradual heat cost was ever
 * found by measurement -- there isn't one. Above it the ancestor rolls a
 * probabilistic burn against chekbrn.
 *
 * The burn is deliberately NOT implemented here. It has not been confirmed
 * for EGA Trek, and adopting a gameplay rule from the ancestor unverified is
 * exactly what the laser falloff shows to be unsafe: Anderson rewrote that
 * one from exponential to linear. The gauge reports; nothing acts on it yet.
 *
 * **[BOTH BULLETS BELOW RE-READ 2026-08-26. The first is wrong -- the cap is
 * 120, at 0x009EFF. The second is right about what it saw and wrong about
 * what it means: the overheat penalty lands AFTER the damage is computed and
 * falls on the LASERS SYSTEM, and the rig repaired every system between
 * turns. Heat IS a mechanic. See LASER_OVERHEAT_AT above and MEASURED.md.]**
 *
 * MEASURED 2026-08-24 (run 2), and the decision not to implement a burn is now
 * evidence-backed rather than cautious. The original's heat IS in memory --
 * a 16-bit word driving the Temp gauge, which two earlier searches missed
 * because it is neither a real nor the fired amount. Two things follow:
 *
 *   - **The game caps that word at 100.** The gauge draws it against a 0..1500
 *     scale at roughly ten to one, so the Temp bar can never pass its own 1000
 *     tick in normal play. Nothing in EGA Trek can reach 1500.
 *   - **No value of it changes the damage.** A fixed 400-unit volley at a fixed
 *     distance, with the word written to 0, 20, 40, 60, 80, 100, 120, 140, 160,
 *     200 and 300 in turn, dealt 263 every single time against 262.56
 *     predicted at full effectiveness.
 *
 * ~~So heat is a gauge and not a mechanic~~ -- it is a mechanic; see above.
 * LASER_HEAT_MAX itself survives: it is the scale of the READOUT, and with a
 * cap of 120 drawn ten to one the bar reaches 1200 of its 1500, which is
 * still short of the tick nothing in EGA Trek can pass. */
#define LASER_HEAT_MAX     1500  /*@MEASURED*/

/* What the game itself never lets the heat word exceed -- MEASURED, and the
   reason LASER_HEAT_MAX above is the scale of a READOUT rather than a
   threshold anything crosses. The gauge draws heat x LASER_HEAT_SCALE against
   LASER_HEAT_MAX, so a capped word fills two thirds of the bar. */
/* 0x78 at 0x009EFF, and the measured 100 was a floor on the observation
   rather than the cap: nothing in run 2 fired enough in one turn to reach
   it. */
#define LASER_HEAT_CAP      120  /*@BINARY*/
#define LASER_HEAT_SCALE     10  /*@MEASURED*/

/* Energy per point of heat: `heat += amount div 15` at 0x009EEF, an INTEGER
   divide. Was 18, fitted from one eyeballed bar position and called "the
   weakest number in this file"; it was, and it was wrong. */
#define HEAT_PER_UNIT        15  /*@BINARY*/

/* AND HEAT IS A MECHANIC AFTER ALL. fn 0x09CC1 at 0x00A20D, straight after
 * the damage is computed:
 *
 *     if (heat > 90) {
 *         "Lasers overheat. Now running at N% efficiency."
 *         lasers% -= damage div 120 + Random(5)        -- floored at 0
 *     }
 *
 * It does not reduce the shot; it DAMAGES THE LASERS SYSTEM, which reduces
 * every shot after it. That is why run 2 concluded "no value of heat changes
 * the damage" -- it wrote heat, fired once, and read the damage of that same
 * shot, which the overheat branch is downstream of. And the rig repaired all
 * twelve systems on both sides of every turn, so the penalty it did inflict
 * was erased before the next reading. **The measurement was not wrong about
 * what it saw; it was blind to where the effect lands.**
 *
 * Two cooldowns, both floored at zero:
 *
 *     heat -= 20                      once per command, 0x0059BC, main loop
 *     heat -= Round(elapsed * 360)    per stardate, 0x0201B3, with the repair
 *
 * Together they are why heat looked like it "cleared on leaving the quadrant":
 * a warp jump elapses enough time to wipe it. A volley of 300 units adds 20
 * and the turn sheds 20, so sustained fire breaks even around 300 units a
 * turn and climbs above it. */
#define LASER_HEAT_COOL_TURN   20  /* shed once per command */  /*@BINARY*/
#define LASER_HEAT_COOL_DAY   360  /* and per stardate elapsed */  /*@BINARY*/
#define LASER_OVERHEAT_AT      90  /* above this the banks take damage */  /*@BINARY*/
#define LASER_OVERHEAT_DEN    120  /* damage div 120 ... */  /*@BINARY*/
#define LASER_OVERHEAT_OF_N     5  /* ... plus Random(5) */  /*@BINARY*/

/* A note on the provenance tags in this file, now that there is a fifth.
 *
 *   CONFIRMED   -- predicted in advance, then seen to happen in the original.
 *   MEASURED    -- read off the original, by screen or by memory.
 * EVERY #define IN THIS FILE AND core/planet.h CARRIES ITS TIER ON ITS OWN
 * LINE, in a trailing comment reading at-sign then the tier name, and
 * `make tiers` lists them and fails on an untagged one. The tiers used to live only in prose, and a scan for them attributed
 * the nearest preceding paragraph -- reporting constants as fitted that were
 * measured, which is worse than no audit.
 *
 *   BINARY      -- read out of the original's CODE with tools/dis16.py. An
 *                  exact constant of Anderson's, not a sample. This tier did
 *                  not exist before 2026-08-26.
 *   DERIVED     -- taken from the FORTRAN line's source (reference/sst2k),
 *                  which is what EGA Trek is a port of. Shape only. Anderson
 *                  rewrote formulas and dropped rules outright, so a DERIVED
 *                  number is a hypothesis with a good pedigree, not a fact.
 *   FITTED      -- chosen to match readings, but not uniquely determined.
 *   PROVISIONAL -- a guess, kept only because something has to be there.
 *
 * DERIVED items carry the test that would settle them. Two have already been
 * settled this way and the results went opposite ways: the 1500 heat
 * threshold above survived, and the ancestor's rule that firing costs an
 * enemy a quarter of its power was refuted outright -- see MEASURED.md. */

/* Enemy hit points.
 *
 * BATTLESHIP and SUPPLY are READ DIRECTLY out of the original's memory. The
 * game keeps a table of the current quadrant's enemies as 6-byte records of
 * three 16-bit words -- y, x, hit points -- and firing on a ship decrements
 * the third word by exactly the damage the game prints. See MEASURED.md for
 * the addresses and the method.
 *
 * Three ships in one quadrant all read exactly 355, and the one we targeted
 * was named MONGOL BATTLESHIP in the viewer; the other two were not
 * identified, so strictly 355 is confirmed for a battleship and shared by
 * two unnamed ships. A MONGOL SUPPLY SHIP in another quadrant read 120.
 * Identical values across ships mean these are fixed per class, not rolled.
 *
 * Level dependence is UNTESTED -- every reading is from one level-3 game,
 * and neither 355 nor 120 appears as a literal in the binary anywhere near
 * the other, so they may well be computed from the command level.
 *
 * COMMAND is still inferred rather than read: a commander survived a single
 * 441-unit hit and died to roughly 265 + 236, so it lies between 441 and
 * about 501. Now that the table is readable, one commander sighting settles
 * it exactly.
 *
 * SCOUT has never been observed at all. */
/* HIT POINTS SCALE WITH COMMAND LEVEL, and all four classes were read out of
 * the binary on 2026-08-26. Every one is `(level + 4) * m + k` against
 * [0x1DF0], which is the command level plus four:
 *
 *     Commander     0x016173   (level+4)*35 + 450    625  695  765
 *     Battleship    0x016119   (level+4)*15 + 250    325  355  385
 *     Scout         0x0161C7   (level+4)*15 + 150    225  255  285
 *     Supply        0x016232   (level+4)*10 +  50    100  120  140
 *                                             at levels 1, 3 and 5
 *
 * All four level-3 columns are exactly what was measured -- 695, 355, 255 and
 * 120 -- from four separate sightings taken on different days, so this is
 * four independent confirmations at once. The fitted `310 + 15*level` for a
 * battleship was right, and the level-5 game that "would settle it" is not
 * needed.
 *
 * The scout and supply formulas each appear TWICE, which is the galaxy
 * generator and the reinforcement path writing the same expression.
 *
 * And it explains why one torpedo always kills a standard Mongol:
 * TORP_DAMAGE_AT_LEVEL is the SAME expression as the battleship line, at
 * every level. That is a design decision, not a coincidence. */
#define HP_AT_LEVEL(l, m, k)  ((uint16_t)((((l) + 4) * (m)) + (k)))  /*@BINARY*/
#define HP_BATTLESHIP_AT(l)   HP_AT_LEVEL(l, 15, 250)  /*@BINARY*/
#define HP_COMMAND_AT(l)      HP_AT_LEVEL(l, 35, 450)  /*@BINARY*/
#define HP_SCOUT_AT(l)        HP_AT_LEVEL(l, 15, 150)  /*@BINARY*/
#define HP_SUPPLY_AT(l)       HP_AT_LEVEL(l, 10,  50)  /*@BINARY*/

/* The level-3 values, kept because the tests and the INFO panel's percentage
   are written against them. NOTE on HP_SUPPLY: 120 is the SPAWN value and
   INFO puts the class maximum at 150 -- it showed 120 as 80%. The binary
   agrees that 120 is what a supply ship is created with. */
#define HP_BATTLESHIP       355  /*@BINARY*/
#define HP_SUPPLY           120  /*@BINARY*/
#define HP_COMMAND          695  /*@BINARY*/
#define HP_SCOUT            255  /*@BINARY*/

/* Per-sector enemy strength, parallel to `sector` and rebuilt with it.
   Non-zero only where sector[] holds an enemy. */
extern uint16_t enemy_hp[QUAD_CELLS];

/* Commanders per quadrant -- the original's DS:0x23E9. Persistent state, not
   a roll, which is what makes the tractor beam expressible. */
extern uint8_t gal_commander[GAL_CELLS];

/* Non-zero when the last warp move ended in a quadrant the captain did not
   ask for, because a Commander in the flight path caught the ship. The UI
   reads it after MOVE_OK and clears it. */
extern uint8_t tractored;

/* Set by trek_fire_torpedo when a star went supernova: where we were thrown
   (a galaxy cell index) and how many Mongols it took with it. Same shape as
   `tractored` -- the torpedo returns a code and the UI needs more than one. */
extern uint8_t  nova_quad;
extern uint16_t nova_kills;

/* Outcomes of firing. */
#define FIRE_OK           0   /* hit, target survived */  /*@ID*/
#define FIRE_KILL         1   /* hit, target destroyed */  /*@ID*/
#define FIRE_BAD_COORDS   2  /*@ID*/
#define FIRE_NO_TARGET    3   /* nothing hostile in that sector */  /*@ID*/
#define FIRE_NO_ENERGY    4   /* main banks hold less than requested */  /*@ID*/
#define FIRE_BOARDED      5   /* laser control is held */  /*@ID*/

/* The measured laser formula, as a pure function so it can be tested against
   the readings directly. `dist` is 8.8 fixed point, as trek_dist returns;
   `eff_pct` is the single combined efficiency the original prints. */
uint16_t trek_laser_damage(uint16_t energy, uint8_t eff_pct, uint16_t dist);

/* Fire the lasers at one sector. Deducts `energy` from the main banks
   whatever the outcome of the shot, as the original does, and applies the
   damage to whatever is there. `damage` receives the delivered figure; pass
   NULL if the caller does not care. */
/* Call once at the start of a LASERS command, before the shots. Heat is
   per-command in the ancestor, so it has to be cleared somewhere the core can
   see; the UI knows where a volley begins and the core does not. */
void trek_laser_begin_volley(void);

uint8_t trek_fire_laser(uint8_t sy, uint8_t sx, uint16_t energy,
                        uint16_t *damage);

/* CONFIRMED FROM THE BINARY 2026-08-26: fn at 0x00EA55 sets the shields-up
   flag and then subtracts the real 50.0 from main energy, decoded from
   `86 00 00 00 00 48`. The POWER DISTRIB reading that put it at 4950 of 5000
   was exactly right, and the one thing this port had marked PROVISIONAL on a
   single observation turns out to have been the correct reading all along.
   Lowering shields is still free -- there is no matching subtraction. */
#define SHIELD_RAISE_COST    50  /*@BINARY*/

/* Galaxy, as four flat byte arrays rather than an array of structs. Flat
   arrays carry no padding, so the 68000 alignment tax the commodore-uno
   README documents for its `Card` struct cannot apply here. Indexed
   [y * GAL_DIM + x]. 256 bytes total. */
extern uint8_t gal_enemies[GAL_CELLS];
extern uint8_t gal_base[GAL_CELLS];
extern uint8_t gal_stars[GAL_CELLS];
extern uint8_t gal_known[GAL_CELLS];   /* 0 = never scanned */
/* Quadrants burnt out by a supernova. The original needs no such array: its
   galaxy holds one number per cell and it writes 999 over the lot, which is
   why the chart prints all nines. This core keeps the three digits apart, so
   the sentinel needs somewhere of its own. */
extern uint8_t gal_nova[GAL_CELLS];

/* The current quadrant only, rebuilt on entry. Indexed [y * QUAD_DIM + x]. */
extern uint8_t sector[QUAD_CELLS];

/* Ship systems, in the order the original's ENGINEERING REPORT lists them.
 *
 * MEASURED: the original keeps these as twelve 16-bit repair percentages at
 * DS:235A, all 100 on a fresh game. Twelve rather than the ten on its console
 * because the manual documents a transporter and a shuttlecraft that the
 * console never shows. See MEASURED.md. */
#define SYS_CONVERTER    0  /*@ID*/
#define SYS_SHIELDS      1  /*@ID*/
#define SYS_LIFE         2  /*@ID*/
#define SYS_LASERS       3  /*@ID*/
#define SYS_TUBES        4  /*@ID*/
#define SYS_WARP         5  /*@ID*/
#define SYS_IMPULSE      6  /*@ID*/
#define SYS_SRSCAN       7  /*@ID*/
#define SYS_LRSCAN       8  /*@ID*/
#define SYS_COMPUTER     9  /*@ID*/
#define SYS_TRANSPORTER 10  /*@ID*/
#define SYS_SHUTTLE     11  /*@ID*/
#define SYS_COUNT       12  /*@ID*/

/* Repair rates, MEASURED from the manual 2026-08-24, which gives the whole
 * table (l.464-469):
 *
 *     1x    normal repairs, divided evenly among damaged systems
 *     2.5x  normal repairs while docked at a starbase
 *     3x    repairing only a selected system
 *     5x    repairing a selected system while docked at a starbase
 *
 * FOUR ENTRIES, AND THEY ARE NOT A PRODUCT. 2.5 x 3 is 7.5, not 5. This port
 * shipped the product for a day: it multiplied the docked rate by a
 * REPAIR_FOCUS_FACTOR of 3 and repaired at about 7x when both applied. The
 * table was already quoted right here and the fourth entry was derived by
 * multiplying anyway, which is the whole mistake in one line.
 *
 * Read it as a table. Each row is a RATE, not a multiplier to stack, and the
 * original is free to price the combined case at less than the product --
 * which it plainly does.
 *
 * MEASURED 2026-08-24 on ACTUAL repair -- a system written to 0%, known time
 * passed, the percentage read back -- not on the STATE OF REPAIR dialog, whose
 * estimate rounds in a way that does not invert (100 points to go prints 5.1
 * days at a rate that is exactly 20). Every earlier figure here came from that
 * dialog, and one of them was wrong for it.
 *
 * In points per stardate the manual's relatives are EXACT on a base of 20:
 *
 *     20   undocked, no focus     every damaged system, floor'd
 *     50   docked,   no focus     2.5x -- NOT the 47 solved off the dialog
 *     60   undocked, focused      3x, focused system ONLY
 *    100   docked,   focused      5x, focused system ONLY
 *
 * ALL FOUR CONFIRMED FROM THE BINARY 2026-08-27, the repair routine at
 * 0x02024E. The original stores none of them: it rounds ONE product and
 * divides it, which is why a search for 20.0 and 60.0 as reals finds nothing.
 *
 *     focused:   sys[f] += Round(t * (docked ? 100.0 : 60.0))
 *     the rest:  n = Round(t * 100.0);  sys[i] += docked ? n div 2 : n div 5
 *
 * At tenth-of-a-stardate granularity `n` is always a multiple of ten, so the
 * divisions are exact and this core's `rate * tenths / 10` is the same
 * function. Kept in the simpler form.
 *
 * "DOCKED" HERE IS THE StarBase-ONLY FLAG [0x26DA], tested at 0x02025F and
 * 0x020367 -- the same flag docking CLEARS for research stations and supply
 * depots. This core applied the docked rate at any base until 2026-08-27.
 *
 * AND THE FOCUS IS A CLAIM ON THE CLOCK, NOT A RATE ROW. The focused system
 * is repaired first out of the whole elapsed time. If that does not finish
 * it, the time is spent and nothing else repairs -- the measured starvation.
 * But if it DOES finish, the overshoot converts back into leftover time at
 * 0.01 stardates a point (0x02030E, and it uses that docked scale either
 * way), the focus clears itself (0x0202F6), and every system gets the
 * remainder at the ordinary rate. The eleven turns of starved shields were
 * the common case, not the law.
 *
 * THE DIALOG'S +0.08, which is why solving it gave 46.6 instead of 50. The
 * STATE OF REPAIR estimate at 0x00F37B is
 *
 *     docked   = (100 - pct) * 0.02 * focus + 0.08      focus 0.5 or 1.0
 *     undocked = (100 - pct) * 0.05 * focus + 0.08      focus 0.33 or 1.0
 *
 * printed Str(x:4:1). A CONSTANT OFFSET, not a rate error -- and it
 * reproduces all five measured rows exactly once the row recorded as 60
 * points is read as 59: 2.08/5.08, 1.88/4.58, 1.26/3.03, 0.98/2.33,
 * 0.78/1.83. The measurement was right about the display and the display was
 * never the mechanic.
 *
 * "Focused system only" is the other half of the measurement and it is a
 * different mechanic from a multiplier: while a focus is set, EVERY OTHER
 * DAMAGED SYSTEM REPAIRS AT ZERO. Shields sat at 0% for eleven consecutive
 * turns while the focused lasers climbed 0 to 100. That is the manual's "at
 * the expense of other systems" read literally, and it is why there is no
 * budget to divide -- there are four rates and a rule about who gets one. */
#define REPAIR_PER_STARDATE_FOCUS         60  /*@BINARY*/  /* 0x0203B8 */
#define REPAIR_PER_STARDATE_FOCUS_DOCKED 100  /*@BINARY*/  /* 0x02026E */

typedef struct {
    uint8_t  quad_y, quad_x;
    uint8_t  sec_y, sec_x;
    uint16_t energy;        /* main banks -- warp drive draws from here */
    uint16_t impulse;       /* impulse engines have their own pool */
    uint16_t shields;       /* shield charge, separate from shields_up */
    uint8_t  torps;
    uint8_t  laser_eff;     /* percent -- heat and battle damage combined,
                               as the original's single gauge reports it */
    uint16_t laser_heat;    /* energy fired in the current volley, 0..65535.
                               Per COMMAND, not cumulative over the game, which
                               is what the ancestor's rpow means and the only
                               reading we have is consistent with. Whether it
                               also decays over time is unmeasured. */
    uint8_t  warp;          /* tenths, 10..80 */
    uint8_t  shields_up;
    uint16_t stardate;      /* tenths */
    /* Hundredths of a stardate travelled but not yet worth a tenth.
       MEASURED movement costs distance/24 stardates, so a one-sector
       hop is 0.0417 -- below the clock's own resolution. Without a
       carry, short moves would be free forever. See advance_hundredths
       in trek.c. */
    uint8_t  time_frac;
    uint16_t stardate_end;  /* tenths -- mission deadline */
    uint8_t  level;         /* 1..5, the command level / rank */
    uint16_t enemies_left;
    uint8_t  sys[SYS_COUNT];  /* repair percentage, 0..100 */
    uint8_t  repair_focus;    /* 0 = spread across everything, else 1+index of
                                 the one system engineering is concentrating
                                 on -- the F)ix command. */
    uint16_t killed;          /* standard Mongols destroyed */
    uint16_t killed_cmd;      /* command ships, scored separately */
    uint16_t casualties;
    uint8_t  lost;            /* ship destroyed */
    uint8_t  docked;          /* BASE_NONE, or the type docked at */
    /* Life support's RESERVE, in tenths of a stardate, capped at 2.0. A
       separate quantity from sys[SYS_LIFE], which is its state of repair.
       BINARY: the real at [0x1D30]. */
    uint16_t life_reserve;
    uint8_t  life_gone;       /* depletion happened and has not been reported */
    /* The Mongol boarding party: BOARD_NONE, or which department it holds.
       [0x1E00] in the original, with its deadline at [0x1D84]. */
    uint8_t  boarders;
    uint16_t board_until;     /* tenths; they are thrown off past this */
    /* Index into planets[], or PLANET_NONE. Here rather than in planet.c
       because it is ship state -- it has to be saved, and movement has to
       break it -- and because putting it here keeps planet.h out of trek.h.
       0xFF is PLANET_NONE; core/planet.h owns the name. */
    uint8_t  orbiting;
    /* Settlements evacuated. MEASURED as a scoring line at 200 each on the
       original's Detailed Evaluation, and it printed zero here until there
       were planets to evacuate. */
    uint16_t rescues;
} Ship;

extern Ship ship;

/* Outcomes of a move request. The UI turns these into messages; the core
   never formats text, so it stays free of any platform's character set. */
#define MOVE_OK             0  /*@ID*/
/* MEASURED 2026-08-25: this is a PARTIAL MOVE, not a refusal. The original
   walks the straight line and leaves the ship in the last clear cell, having
   charged it for the distance it actually covered; the message names the cell
   that stopped it, which is one step further on than where the ship now is.
   trek_block_y/x carry that cell. True of quadrant changes too -- the
   departure path is walked through the quadrant being LEFT, and a move
   blocked there leaves the ship in it. */
#define MOVE_BLOCKED        1   /* stopped short: see trek_block_y/x */  /*@ID*/
#define MOVE_NO_ENERGY      2  /*@ID*/
#define MOVE_SAME_PLACE     3  /*@ID*/
#define MOVE_BAD_COORDS     4  /*@ID*/
#define MOVE_WARP_TOO_LOW   5   /* distance needs a higher warp factor */  /*@ID*/
/* Manual: impulse engines below 50% "simply stop functioning", and the
   original refuses with "ENGINEERING: Move aborted; impulse engines are too
   damaged to use" -- a hard no, not a slower move. */
#define MOVE_NO_IMPULSE     6  /*@ID*/

/* Deterministic PRNG. Owned here rather than taken from libc so that a seed
   reproduces the same galaxy on every platform -- which is what makes a
   cross-platform comparison of the same game meaningful. */
void     trek_srand(uint16_t seed);
uint16_t trek_rand(void);
uint8_t  trek_rand_n(uint8_t n);        /* 0 .. n-1 */
/* The same, for a range wider than a byte. One divide, and the only caller is
   the energium crystal's 0..4499 -- which is once per crystal, not once per
   object placed, so the 6502 can afford it where trek_rand_n could not. */
uint16_t trek_rand_n16(uint16_t n);

/* Distance in 8.8 fixed point between two cells of an 8x8 grid, from a
   64-entry table. Serves both sectors and quadrants, since both are 8x8.
   Arguments are absolute differences, each 0..7. */
uint16_t trek_dist(uint8_t dy, uint8_t dx);

/* Bearing from the ship to a sector, in whole degrees, 0..359.
 *
 * MEASURED convention: with the ship at 6,4 and the Mongol Commander at 5,5 --
 * one row up and one column right -- the original's viewer read 45.0. So east
 * is zero and degrees increase anticlockwise, which is the ordinary
 * mathematical convention rather than a compass one.
 *
 * Integer throughout: an octant chosen from the signs and the relative
 * magnitudes of dy and dx, then a 17-entry arctangent table for the ratio
 * within it. Worst case is half a degree out, which is finer than a display
 * that prints one decimal can show. Returns 0 for the ship's own cell. */
uint16_t trek_bearing(uint8_t sy, uint8_t sx);

/* Enemy movement. MEASURED 2026-08-21 over thirty-six turns against one
 * Commander, our position moved five times:
 *
 *   a Commander closes ONE SECTOR toward the ship on every turn the player
 *   fires, until it is adjacent, and then holds.
 *
 * Fourteen of fourteen non-adjacent turns moved; none of the adjacent ones
 * did. No randomness, no retreat at any range, never more than one sector,
 * and nothing about our shields, energy or torpedoes enters into it.
 *
 * Only commanders move at all -- watched across a full session, and it is the
 * ancestor's own gate: moveklings() runs movebaddy() for the commander and the
 * super commander unconditionally, and for ordinary ships only at expert skill.
 *
 * The ancestor's forces score -- power, numbers, our shields and torpedoes,
 * deciding advance / hold / retreat and how far -- was ported here and is now
 * DELETED. None of it survived contact with the original. */
void trek_new_game(uint8_t level, uint16_t seed);
void trek_enter_quadrant(void);         /* rebuild `sector` from the chart */

/* A random empty cell of the current quadrant, or 0xFF if there is none.
   Public so core/planet.c can put a planet down after trek_enter_quadrant()
   has placed everything else; nothing outside the core should call it. */
uint8_t trek_free_sector(void);

/* Move the clock by `tenths`: the converter tops up, the crew repair, and
   NOTHING ELSE. Scheduled events are deliberately not run here -- movement
   uses this and lets the turn loop call trek_run_events() afterwards, so
   anything else that consumes time (a shuttlecraft round trip) does the same
   and the messages arrive by one route rather than two. */
void trek_advance_time(uint16_t tenths);

/* Energy transfer between the three pools -- the E command.
 *
 * CONFIRMED in DOSBox-X: transferring into a pool that is already at its
 * maximum destroys the surplus. Diverting 500 from main into full shields
 * left main at 4500 and shields still at 2500; the energy simply vanished.
 * The original has strings for exactly this ("ILLOGICAL", "ENERGY LOST"),
 * which we had extracted from the binary long before seeing it happen.
 * Total energy is therefore NOT conserved across a careless transfer.
 *
 * `lost` receives the amount destroyed, so the UI can report it; pass NULL
 * if the caller does not care. */
#define POOL_MAIN        1  /*@ID*/
#define POOL_IMPULSE     2  /*@ID*/
#define POOL_SHIELDS     3  /*@ID*/

#define DIVERT_OK        0  /*@ID*/
#define DIVERT_ILLOGICAL 1   /* same pool, or a pool that does not exist */  /*@ID*/
#define DIVERT_SHORT     2   /* source does not hold that much */  /*@ID*/

uint8_t trek_divert(uint8_t from, uint8_t to, uint16_t amount, uint16_t *lost);

/* Shields up and down -- SHUP and SHDN.
 *
 * From the manual (l.339-342): "Raising the shields draws a small amount of
 * energy from the main energy banks so you do not want to raise the shields
 * needlessly. Lowering shields causes no energy change." So the cost is
 * asymmetric on purpose, and SHIELD_RAISE_COST above is that small amount.
 *
 * Note `shields_up` and `shields` are independent: the first is whether they
 * are raised, the second how much charge they hold. Raising flat shields is
 * allowed and useless, exactly as it is in the original -- the manual makes
 * the same distinction when it says enemy fire drains the shields while they
 * hold and reaches main energy once they do not.
 *
 * Also from the manual (l.578-583): raising them turns the ship's own glyph
 * on the short range scanner yellow, and lowering returns it to white. That
 * is a UI consequence, but it is a game rule -- it is how the player reads
 * the state at a glance -- so it is recorded here rather than left to the
 * presentation layer to invent. */
#define SHIELD_OK          0  /*@ID*/
#define SHIELD_ALREADY     1  /*@ID*/
#define SHIELD_NO_ENERGY   2   /* main banks cannot pay the raising cost */  /*@ID*/
#define SHIELD_BOARDED     3   /* engineering is held */  /*@ID*/

uint8_t trek_shields_up(void);
uint8_t trek_shields_down(void);


/* MEASURED: floor(20 * elapsed_stardates), and it does NOT divide between
   damaged systems -- two damaged at once each repaired at the full rate. The
   manual claims the crew divide their time evenly; the original does not. */
#define REPAIR_PER_STARDATE  20  /*@BINARY*/   /* Round(t*100) div 5, 0x0203D3 */

/* Short range scanner resolution, MEASURED off the original's own code:
   above 90 everything shows, at or below 50 nothing does, and in between
   only the ship, stars and novas. */
#define SRSCAN_FULL          90  /*@CONFIRMED*/
#define SRSCAN_BLIND         50  /*@CONFIRMED*/

/* Enemy fire. MEASURED 2026-08-21 by a controlled run against the original --
   one enemy, its hit points written back every turn, our sector fixed, damage
   read out of the pinned pools and corrected for death pods. See MEASURED.md.
 *
 * CONFIRMED: the amount depends on the firer's REMAINING HIT POINTS and not
 * on its class. A supply ship at its own 120 hit points was effectively
 * silent; writing 695 into that same ship -- same class, same sector, same
 * range -- it fired 496, 515, 499, 527 immediately. So there is no per-class
 * table to keep: hit points already carry the class difference. The four
 * invented ENEMY_FIRE_* constants that used to live here are gone.
 *
 * CONFIRMED: raising shields does not reduce the figure. 358 against 367 at
 * one range with five firing turns each.
 *
 * MEASURED 2026-08-21, thirty-six turns against one Commander across eleven
 * distinct ranges: the falloff is LINEAR IN EUCLIDEAN DISTANCE and reaches
 * zero at 12, exactly the law our own lasers use.
 *
 *   dmg = hit_points * 0.78 * (1 - distance / 12)
 *
 * k came out at 0.782 with a standard deviation of 0.038 over all thirty-six,
 * so the residual scatter is about 5% -- the random component, and small.
 *
 * The metric is Euclidean and nothing else. Offsets of (4,2) and (2,4) dealt
 * 314.7 and 314.2, so it is symmetric in dy and dx; the same Chebyshev
 * distance spanned 0.41 to 0.52 of hit points and the same Manhattan distance
 * 0.48 to 0.54, which rules both out.
 *
 * This supersedes an earlier four-point estimate at a single range that put
 * the coefficient nearer 0.95 and suggested the curve flattened into a floor.
 * It does not flatten; that reading was one range with n=4. */
/* READ OUT OF THE BINARY 2026-08-26, fn 0x16844 at 0x01696F:
 *
 *     hit = hp * (0.6 + Random*0.1) * (1.5 - distance/8)
 *
 * 0.6, 0.1, 1.5 and 8.0 all decode exact, and `1.5 - d/8` is identically
 * `1.5 * (1 - d/12)` -- so THE FALLOFF AND ITS ZERO AT TWELVE ARE THE
 * ORIGINAL'S, confirmed, and the thirty-six-turn measurement got the hardest
 * part of this exactly right.
 *
 * IT ALSO SAYS THERE IS A RANDOM COMPONENT, which this port did not have:
 * the shot is uniform across a band 15.4% wide. The measurement saw it --
 * "the residual scatter is about 5% -- the random component, and small" --
 * and a uniform band of that width has a standard deviation of 4.4% of its
 * mean against the 4.9% observed. That is the shape, and it is now modelled.
 *
 * BUT THE SCALE CONFLICTS, AND IT IS NOT RESOLVED. The binary's mean factor
 * is 0.65 * 1.5 = 0.975 of hit points at point-blank; the measurement, over
 * thirty-six turns at eleven ranges, gives 0.782 with a standard deviation of
 * 0.038. The ratio is 0.8022 -- and **0.8 is a constant in this very
 * routine**, the one applied to the shield pool drain and to the printed
 * "Shields absorb N unit hit from" figure at 0x017077. The likeliest reading
 * is that the thirty-six samples measured a quantity carrying that 0.8.
 *
 * NOT RESOLVED BY FLIPPING THE CONSTANT. Thirty-six samples at eleven ranges
 * is a serious measurement and this is one instruction's worth of doubt
 * against it. The band below keeps the MEASURED centre and takes the
 * BINARY's width. What settles it is one run: shields DOWN, read main energy
 * before and after a single shot from a Commander at a known range. The
 * binary predicts 0.9 to 1.05 of hit points times the falloff; the port
 * predicts 0.72 to 0.84. Nothing else needs to be measured. */
#define ENEMY_FIRE_PCT_MIN      72   /* mean 78 kept from the 36 readings */  /*@MEASURED*/
#define ENEMY_FIRE_PCT_SPAN     13   /* 72..84, the binary's relative width */  /*@MEASURED*/

/* MEASURED: enemies hold fire on about half of their turns -- 5/10, 5/10,
   7/10 and 4/8 across four blocks. Not an artefact of dropped turns: a
   dropped turn shows up as a zero followed by a doubled reading, and the
   surviving figures were tight with no doubles.

   AND THERE IS NO SUCH ROLL IN fn 0x16844. The firing loop skips an enemy
   only when its hit points are zero or when the cell holds 'R', the death
   pod, which never fires. Every other ship in the quadrant fires every time
   the routine runs. So the half-the-turns is real and is decided somewhere
   ELSE -- most likely the routine is not called every turn, or a ship that
   moved does not also shoot. Kept, because the observation is solid; flagged,
   because the mechanism is not where it was assumed to be. */
#define ENEMY_FIRE_ONE_IN        2  /*@MEASURED*/

/* SYSTEM DAMAGE FROM COMBAT -- read out of the binary 2026-08-26, and it
 * replaced a threshold that never existed.
 *
 * The original does NOT decide this per hit, and it does not test how much
 * got through. `fn 0x0213AD` runs once per turn from the main loop at
 * 0x005993, and the loop around it is:
 *
 *     rounds = Round(raw_hits_this_turn / 350.0) + 1          ; 0x00595E
 *     for r = 1 to rounds:
 *         if (fired_on_this_turn)          goto roll          ; [0x26DE]
 *         if (level + 4 < 6)               return
 *         if (absorbed_this_turn > 700)    goto roll          ; [0x1DC6]
 *         if (Random(raw_hits) <= 175)     return             ; [0x1DC8]
 *       roll:
 *         if (Random(3) == 0)              return
 *         DamageReport(2)                                     ; fn 0x020DCE
 *
 * `[0x26DE]` is set by the enemy-fire routine whenever enemies engaged and
 * cleared at the top of every turn, and `[0x1DC8]` is only ever non-zero on
 * the same turns -- so the level and threshold branches below it are
 * UNREACHABLE in play. What actually happens is: if you were fired on, roll
 * `Round(total/350) + 1` times, and each roll damages a system two times in
 * three. That is why two systems could go in one turn: 860 units of hits is
 * three rounds at two-thirds each.
 *
 * The port's old `through >= SYSTEM_DAMAGE_THRESHOLD` was invented, and its
 * "roughly three hits in five" was the observable shadow of these rounds. */
#define DMG_ROUNDS_PER         350   /* rounds = round(hits/350) + 1 */  /*@BINARY*/
#define DMG_ROLL_OF_N            3   /* Random(3) == 0 spares the ship */  /*@BINARY*/
#define DMG_SYS_OF_N            11   /* Random(11): never the Shuttlecraft */  /*@BINARY*/

/* HOW HARD, from fn 0x020DCE at 0x020E37..0x020FA7. The subtraction is in
 * HIT UNITS against a 0..100 scale, which is why a system that gets hit is
 * usually annihilated -- the measured "0% eight times in eleven" is this
 * formula, not a special case:
 *
 *     shields up:    sys -= Round(hits * (1.25 - charge/2500) / (2 + 3*rnd))
 *     shields down:  sys -= Round(hits * 0.5                  / (2 + 3*rnd))
 *     sys -= Random(5)
 *     if (sys > 90) sys -= 10 + Random(10)
 *     if (sys < 0)  sys = 0
 *
 * where `rnd` is Turbo Pascal's argument-less Random, a real in [0,1). The
 * constants 3.0, 2.0, 2500.0, 1.25 and 0.5 all decode as exact round numbers,
 * which is the check on the decoding.
 *
 * Note what the first line says: RAISED BUT EMPTY SHIELDS ARE THE WORST PLACE
 * TO BE. At a full 2500 the factor is 0.25, at zero charge it is 1.25 -- two
 * and a half times worse than dropping them. */
#define DMG_FACTOR_UP_X100     125   /* 1.25 - charge/SHIELD_MAX */  /*@BINARY*/
#define DMG_FACTOR_DOWN_X100    50   /* flat 0.5 with shields down */  /*@BINARY*/
#define DMG_DIV_MIN_X100       200   /* 2.0 */  /*@BINARY*/
#define DMG_DIV_SPAN_X100      300   /* + 3.0 * Random */  /*@BINARY*/
#define DMG_EXTRA_OF_N           5   /* then Random(5) more */  /*@BINARY*/
#define DMG_TOPUP_ABOVE         90   /* anything still above 90 ... */  /*@BINARY*/
#define DMG_TOPUP_MIN           10   /* ... loses 10 + Random(10) */  /*@BINARY*/
#define DMG_TOPUP_SPAN          10  /*@BINARY*/

/* CASUALTIES, from 0x021006: `Round(Sign(hits - 500)) * Random(10)`, so a
 * turn whose hits never exceeded 500 units costs no lives at all, and one
 * that did costs 0..9. The count feeds the running total at [0x1DDE] that
 * the Top Secret report prints. */
#define DMG_CASUALTY_HIT_MIN   500  /*@BINARY*/
#define DMG_CASUALTY_OF_N       10  /*@BINARY*/

/* ------------------------------------------- shields, and what a hit does
 *
 * MEASURED 2026-08-24 (run 3), and it replaced `through = amount - shields`
 * outright. THE SHIELDS ARE A PROPORTIONAL ABSORBER, NOT A BUCKET. Three
 * behaviours, each seen repeatedly:
 *
 *   shields DOWN         the pool is untouched and the whole hit reaches
 *                        main energy. Five hits, pool unchanged every time.
 *   shields UP and FULL  the hit comes out of the POOL, energy untouched, and
 *                        no system is damaged. Six hits of 385 to 518.
 *   shields UP and FLAT  (200 of 2500) a small, TIGHTLY CONSTANT share is
 *                        absorbed and the rest reaches energy. Five hits gave
 *                        0.0650, 0.0651, 0.0651, 0.0650, 0.0651 -- a formula,
 *                        not a roll.
 *
 * THE LAW, and it is a hypothesis that fits rather than a measurement:
 *
 *     absorbed = hit * (charge / SHIELD_MAX) * (shield system % / 100)
 *
 * The charge term is the obvious one. The SYSTEM term is what makes the two
 * clean readings agree: full charge with an undamaged system absorbs the
 * whole hit, which is exactly what was seen, and 0.065 at a charge of 200
 * needs the shield system to have been at about 81% -- plausible for a ship
 * that had been under fire, and not verified.
 *
 * It also explains why the mid-range sweep could not be fitted. Those turns
 * had TWO enemies firing, and MEASURED.md names the confound: the first hit
 * damages the shield SYSTEM before the second arrives, so every reading comes
 * out low. They do: 0.327, 0.466 and 0.647 against 0.40, 0.60 and 0.80 from
 * the charge term alone. The law needs a one-enemy quadrant to settle.
 *
 * A damaged system working worse is also how every other system in this port
 * behaves, so this is the house rule rather than a special case. */

/* MEASURED: the shield SYSTEM takes damage when the POOL absorbs a big hit,
   which is a mechanic separate from the pool draining and one this port had
   no part of. 795 absorbed took it to 71%; 385 to 518 absorbed left it alone.
   PROVISIONAL: the threshold is somewhere in 519..795 and 600 is the middle
   of that bracket, not a reading. */
/* THE ABSORPTION CONSTANT, READ OUT OF THE BINARY 2026-08-26. fn 0x16844
   computes, in Turbo Pascal reals:

       absorbed = damage * (shields / 2500) * (sys[SHIELDS] / 100) * 0.8

   with 2500 and 100 decoded from the real constants at 0x16A5A and 0x16A73
   and 0.8 from `80 CD CC CC CC 4C` at 0x17044. 2500 is SHIELD_MAX and the
   percentage is [0x235C], element ONE of a twelve-word array -- the same
   index SYS_SHIELDS has here.

   So the law's SHAPE was already right, including the system term this file
   called "a hypothesis that fits rather than a measurement". It is measured
   now. What was missing is this flat four-fifths, and it is exactly what
   reconciles the law with the readings:

       charge 1000   0.4 * 0.8 = 0.32   measured 0.33
       charge 1500   0.6 * 0.8 = 0.48   measured 0.47
       charge 2000   0.8 * 0.8 = 0.64   measured 0.65

   and the fitted `charge / 3100` in MEASURED.md was approximating
   2500 / 0.8 = 3125. */
#define SHIELD_ABSORB_NUM   4  /*@BINARY*/
#define SHIELD_ABSORB_DEN   5  /*@BINARY*/

/* The shield SYSTEM wears from what the POOL stops, and it is a graded
   reduction rather than a wrecking. From fn 0x016844 at 0x0171CC, on the
   TURN's total absorbed, once per turn:

       if (absorbed > 800 && shield_sys > 0)
           shield_sys -= Round((absorbed - 700) / 10.0)     ; floored at 0

   so 800 absorbed costs 10 points and 1500 costs 80. The old 600 here was
   fitted from one reading and wrecked the system outright. */
#define SHIELD_SYS_WEAR_MIN      800  /*@BINARY*/
#define SHIELD_SYS_WEAR_BASE     700  /*@BINARY*/
#define SHIELD_SYS_WEAR_DEN       10  /*@BINARY*/

/* What happened during a turn. The core never formats text, so it hands the
   UI a list of facts and lets each platform word them. */
#define EV_NONE          0  /*@ID*/
#define EV_HIT           1   /* y,x = firing sector; amount = damage */  /*@ID*/
#define EV_SHIELD_HOLD   2   /* amount = absorbed entirely by shields */  /*@ID*/
#define EV_SYSTEM_HIT    3   /* y = system index; amount = new repair pct */  /*@ID*/
#define EV_SHIP_LOST     4  /*@ID*/
#define EV_ENEMY_MOVED   5   /* y,x = where it moved to */  /*@ID*/
/* Scheduled events, reported the same way. y,x are the GALAXY cell they
   concern, not a sector, except EV_POD_HIT which concerns the whole
   quadrant. `amount` on EV_BASE_ATTACKED is the deadline as a stardate in
   tenths, so the UI can print "they can last until". */
#define EV_BASE_ATTACKED 6  /*@ID*/
#define EV_BASE_LOST     7  /*@ID*/
#define EV_TRACTORED     8   /* y,x = the quadrant we were dragged to */  /*@ID*/
#define EV_POD_HIT       9   /* amount = damage, to us and to every enemy */  /*@ID*/
#define EV_LIFE_GONE    10   /* the reserve ran out; the ship is lost */  /*@ID*/
#define EV_BOARDED      11   /* y = which department they took */  /*@ID*/
#define EV_BOARDERS_GONE 12  /* security has cleared them */  /*@ID*/
#define EV_NOVA         13   /* y,x = the quadrant; amount = Mongols with it */  /*@ID*/

typedef struct {
    uint8_t  kind;
    uint8_t  y, x;
    uint16_t amount;
} TrekEvent;

/* THE ONE ENTRY POINT FOR INCOMING DAMAGE, whoever it comes from -- enemy
   fire, a death pod, and whatever the unbuilt hazards turn out to be. It was
   static until 2026-08-26; making it public is what lets the shield law below
   be tested at a chosen hit size instead of whatever an enemy happened to
   fire, which is the only way to assert a proportion. Appends its events to
   `ev` in the usual way. */
void trek_take_hit(uint16_t amount, TrekEvent *ev, uint8_t *n, uint8_t max);

/* Damage ONE NAMED system by `hits` HIT UNITS -- the turn's total, not one
   hit. Public for the same reason trek_take_hit is: damage arrives from
   places that are not enemy fire, and those know which system they mean.
   Note the casualties come with it; they are part of what this does. */
void trek_wreck_system(uint8_t which, uint16_t hits,
                       TrekEvent *ev, uint8_t *n, uint8_t max);

/* The once-per-turn system-damage roll, on what the turn actually cost.
   trek_enemy_turn() calls it for you; nothing else should. */
void trek_combat_damage(TrekEvent *ev, uint8_t *n, uint8_t max);


/* ------------------------------------------------------- scheduled events
 *
 * THE ORIGINAL DOES HAVE A SCHEDULER, and it is this shape. Read out of the
 * binary 2026-08-26, after three decoded events in a row turned out not to be
 * scheduled at all and left the whole subsystem under suspicion.
 *
 * EGA Trek keeps **eight Turbo Pascal reals in a fixed array at DS:0x1D78**,
 * one slot per event type, each an ABSOLUTE stardate, each tested `if
 * (stardate > deadline)` once a turn. There is no queue, no list and no
 * allocation -- exactly the fixed-slot design this port took from the
 * ancestor. The array is written and read whole by the save and restore
 * routines, so the deadlines are game state.
 *
 *     DS:0x1D78  the hail response          fn 0x02066B
 *     DS:0x1D7E  REINFORCEMENTS             fn 0x015A4C
 *     DS:0x1D84  the boarding party         fn 0x015D6E
 *     DS:0x1D8A  a Union ship's distress    fn 0x0158EC
 *     DS:0x1D90  a StarBase comes under attack   fn 0x01F9D5
 *     DS:0x1D96  ... and falls                   fn 0x01F9D5
 *     DS:0x1D9C  the supernova              set by galaxy generation
 *     DS:0x1DA2  the evacuation deadline    fn 0x0151D0
 *
 * **9999.0 is never**, which is what SCHED_NEVER already meant.
 *
 * THREE CORRECTIONS THIS FORCED:
 *
 *  1. **The deviate is UNIFORM, not exponential.** Every reschedule in the
 *     binary is `stardate + base + spread * Random`, with Turbo Pascal's
 *     argument-less Random -- a flat [0,1). The port used the ancestor's
 *     `expran()`, an exponential with a long tail, and that is gone. The
 *     32-entry -ln table went with it, which the C128 image was glad of.
 *
 *  2. **The two base slots have read constants now** (below), and the
 *     original's INITIAL schedule uses the INTEGER `Random(4)`, so the first
 *     attack lands on a whole stardate where later ones do not.
 *
 *  3. **The tractor beam and the death pod are NOT SCHEDULED.** Neither has a
 *     slot. THE TRACTOR IS NOW BUILT PROPERLY -- see TRACTOR_OF_N below, it
 *     is action-triggered inside MOVE -- and its slot is gone from this
 *     enum. The death pod is an OBJECT and is still a scheduled stand-in;
 *     see SCHED_POD_BASE_TENTHS.
 *
 * FOUR SLOTS THIS CORE DOES NOT HAVE: the hail response, the boarding party,
 * a Union ship's distress call, and the supernova. All four are real events
 * with real slots, and all four are unbuilt.
 *
 * Deliberately NOT included: the ancestor's snapshot-for-time-warp, deep
 * space probes, and supercommander movement. */
#define SCHED_BASE_ATTACK    0   /* DS:0x1D90 */  /*@BINARY*/
#define SCHED_BASE_FALLS     1   /* DS:0x1D96 */  /*@BINARY*/
#define SCHED_DEATH_POD      2   /* NOT an event at all -- see below */  /*@PROVISIONAL*/
#define SCHED_COUNT          3  /*@ID*/

#define SCHED_NEVER     0xFFFFU  /* the original writes the real 9999.0 */  /*@BINARY*/

/* The two slots that ARE the original's, in tenths of a stardate.
 *
 *     first attack   stardate + 2 + Random(4)   0x0054F4, whole days
 *     next attack    stardate + 2 + Random*4    0x01FA5A
 *     base falls     stardate + 2 + Random*2    0x01FB75
 *
 * The "they can last until" figure the COMMUNICATIONS panel prints is that
 * third line, and 2..4 stardates is what the captured messages showed. */
#define SCHED_ATTACK_BASE_TENTHS   20  /*@BINARY*/
#define SCHED_ATTACK_SPAN_TENTHS   40  /*@BINARY*/
#define SCHED_FALLS_BASE_TENTHS    20  /*@BINARY*/
#define SCHED_FALLS_SPAN_TENTHS    20  /*@BINARY*/
#define SCHED_FIRST_ATTACK_DAYS     4  /* Random(4) WHOLE stardates */  /*@BINARY*/

/* The tractor and the pod, which the original does not schedule at all.
 *
 * THE DEATH POD IS AN OBJECT, NOT AN EVENT -- found 2026-08-26 in the
 * quadrant fill at 0x01649D. On entering a quadrant:
 *
 *     if (ships_here >= 5)          no pod
 *     if (quadrant COLUMN != 8)     no pod          <-- yes, really
 *     if (Random(10) <= 5)          no pod          -- so 4 in 10
 *     place 'R' in a free sector, enemy table type 6
 *
 * It is a sector object with a table entry, which is why a torpedo aimed at
 * it answers "No damage reported." rather than killing it. Rebuilding it that
 * way needs a sector code and its own turn behaviour, so the scheduled
 * stand-in below stays until then -- but it is a stand-in for a mechanic of a
 * completely different shape, not a mistuned interval.
 *
 * The COLUMN 8 condition is extraordinary and is recorded as read: the
 * instruction is `cmp word ptr [0x1DE6], 8` reached from a verified jump
 * target, and [0x1DE6] is the quadrant column everywhere else in the binary.
 * It is the second such quirk in this program -- reinforcements only ever
 * arrive in column 1, which the message text itself confirms. One emulator
 * run flying column 8 repeatedly would settle it; nothing else needs to.
 *
 * These reproduce the means this port has always used, in the uniform form
 * the original uses everywhere else. Replace them, do not tune them. */
#define SCHED_POD_BASE_TENTHS      100  /*@PROVISIONAL*/
#define SCHED_POD_SPAN_TENTHS      900  /*@PROVISIONAL*/

/* End of a player's turn, whatever it was. The original sheds 20 points of
   laser heat here, in its main loop, whether or not the turn moved the clock
   -- so a core whose time only advances on movement needs this hook or the
   banks never cool between volleys. The UI calls it once per turn. */
void     trek_turn_end(void);

/* Set by trek_fire_laser when the shot pushed the banks past
   LASER_OVERHEAT_AT and cost the Lasers system some percentage. The UI reads
   it to print the original's "Lasers overheat" line; nothing else does. */
extern uint8_t laser_overheated;

void     trek_schedule(uint8_t kind, uint16_t offset_tenths);
void     trek_unschedule(uint8_t kind);
uint8_t  trek_is_scheduled(uint8_t kind);
uint16_t trek_scheduled(uint8_t kind);   /* absolute stardate, or SCHED_NEVER */

/* Restoring a saved game needs to put back state that is otherwise private to
   trek.c: the RNG and the schedule as ABSOLUTE dates, where trek_schedule()
   takes an offset from now. Only core/serial.c calls these. They live here
   rather than the serialiser having its own copy of the state, because two
   copies of the schedule is exactly how a reloaded game ends up with
   deadlines the panel promised and the queue has never heard of. */
uint16_t trek_rng_state(void);
void     trek_rng_restore(uint16_t v);
void     trek_sched_restore(uint8_t kind, uint16_t when);

/* The original's deviate: `base + spread * Random`, in tenths of a stardate.
 * Uniform on [base, base+spread), which is what every reschedule in the
 * binary computes. It replaced the ancestor's exponential expran().
 *
 * Integer by necessity and by choice. The core forbids float and long, and
 * `int` is 16 bits under cc65 but 32 on the 68000, so anything relying on
 * promotion would compute different event schedules on the Amiga than on the
 * C128 -- silently. The whole schedule stays reproducible from a seed on
 * every target, which is what makes the native test suite an oracle for
 * platforms we cannot run. */
uint16_t trek_sched_deviate(uint16_t base_tenths, uint16_t spread_tenths);

/* Which base is under attack, as a galaxy cell index; GAL_CELLS if none.
   The UI needs it to name the base in its message, and the player needs to
   know where to fly. */
extern uint8_t base_under_attack;

/* Advance the clock by `tenths` of a stardate: the converter tops up main
   energy, the crew repair damaged systems, and anything scheduled to happen
   inside that window happens, in date order. Movement already does this;
   call it directly only for a turn that passes time without moving.
 *
 * `ev` may be NULL when the caller does not want the report -- the events
 * still fire. Returns how many were written. */
uint8_t trek_advance(uint16_t tenths, TrekEvent *ev, uint8_t max);

/* Fires anything now due. Movement moves the clock without an event list to
   fill, so the turn loop calls this once after whatever consumed the turn. */
uint8_t trek_run_events(TrekEvent *ev, uint8_t max);

/* ---------------------------------------------------------------- docking
 *
 * The manual is authoritative here and it is more specific than the ancestor,
 * which resupplies everything at any base:
 *
 *   "When you are in a sector directly adjacent to a StarBase, issue this
 *    command. You can also dock at Research Stations and Supply Bases, but
 *    they cannot provide everything that a StarBase can." (l.436-439)
 *   "A StarBase is the most useful because you can replenish all ships
 *    supplies there. Supply stations can provide life support supplies and
 *    energy torpedoes. Research stations can provide only life support
 *    supplies." (l.356-359)
 *   "When docked at a StarBase its shields will protect your ship from enemy
 *    lasers." (l.440-441)
 *
 * READ FROM THE BINARY 2026-08-27, `fn 0x0F022`, segment base 0x9310. The
 * manual is confirmed exactly, and this is the LAST routine on the read list.
 * The base type is the chart's TENS DIGIT -- `(galaxy[q] mod 100) div 10` at
 * 0x00F044, against the galaxy array at DS:0x2560 -- and the type names come
 * out of `fn 0x01F8D1` as a literal string table:
 *
 *     1  StarBase        2  research station        3  supply depot
 *
 * Note what that means for the chart: the middle digit is a TYPE, not a
 * COUNT. A quadrant holds at most one base, and the manual agrees --
 * "the number indicates base type (1 is a StarBase, 2 a research station
 * and 3 a supply depot)".
 *
 * What docking writes, in the order the routine writes it:
 *
 *     ALL THREE TYPES     life support reserve [0x1D30] = 2.0 days   0x00F072
 *                         reserve-panel flag   [0x26D6] = 0          0x00F084
 *     if type > 1         docked flag          [0x26DA] = 0          0x00F06D
 *     StarBase only       impulse  [0x1D4E] = 500.0                  0x00F08F
 *                         if (energy  < 5000) energy  = 5000         0x00F0A1
 *                         if (shields < 2500) shields = 2500         0x00F0CD
 *     if type != 2        torpedoes [0x1DBE] = 9                     0x00F0FF
 *
 * then prints "NAVIGATION: Docked." and spends 0.1 stardates (the real
 * `7D CD CC CC CC 4C` at 0x00F160), which confirms the measured turn cost.
 *
 * Three things worth pulling out:
 *
 *   - **Energy and shields are TOP-UPS, not assignments.** This core assigns
 *     shields and tops up energy. It makes no difference: every path that
 *     adds to shields caps them at 2500 (0x00FB4D), so they cannot be above
 *     it when docking runs. Energy CAN exceed 5000 -- one measured reading
 *     went to 7435 -- which is exactly why that one had to be a top-up.
 *   - **"energy torpedoes" is ONE noun.** The manual's "life support supplies
 *     and energy torpedoes" reads like two gifts; the game calls the weapon
 *     "energy torpedoes (EnTorps)" and the binary gives a supply depot no
 *     main energy at all. This core already had it right.
 *   - **Only a StarBase counts as docked.** [0x26DA] is cleared for types 2
 *     and 3, and that flag is what makes the enemy skip its whole firing
 *     routine at 0x016887 -- the manual's "when docked at a StarBase its
 *     shields will protect your ship". trek_docked_safe() matches.
 *
 * OPEN, and NOT settled by this read: whether REPAIR_PER_STARDATE_DOCKED is
 * gated on that same StarBase-only flag. This core applies the docked rate at
 * any base. None of the nine [0x26DA] sites is the repair routine, and the
 * rate reals are not loaded in the register form a search would find, so the
 * next step is to locate the repair routine itself rather than to guess.
 *
 * Adjacency is the ancestor's rule and the manual's alike: any of the eight
 * neighbouring sectors, not the base's own cell. */
/* SUPERSEDED 2026-08-24: this is 50, not 47, and the reasoning below is the
 * cautionary part. Every figure in it came from the STATE OF REPAIR dialog,
 * whose printed times are ESTIMATES with their own rounding -- 100 points to
 * repair prints 5.1 days at a rate that is exactly 20 a day, so solving across
 * those readings measures the display, not the mechanic. Writing a system to
 * 0%, passing known time and reading the percentage back gives 50 exactly,
 * which is the manual's 2.5x on a base of 20. Kept because the method is the
 * lesson. Original note follows.
 *
 * MEASURED 2026-08-21 off the STATE OF REPAIR dialog, which prints Docked and
 * Undocked times side by side, so one screenshot with a system damaged reads
 * both rates at once. Six readings at 0%, 10%, 40%, 55%, 65% and 95%:
 *
 *     points to repair   docked   undocked
 *     100                2.1      5.1
 *      90                1.9      4.6
 *      60                1.3      3.0
 *      45                1.0      2.3
 *      35                0.8      1.8
 *
 * Solving across all of them: undocked ~19.75 points per stardate, docked
 * ~46.6. So the docked advantage is about **2.35x**, and the ancestor's 4x --
 * DERIVED, and flagged here as the one number worth checking -- is REFUTED.
 *
 * The undocked figure confirms REPAIR_PER_STARDATE 20 to within the display's
 * rounding, and the rate was identical with one system damaged and with three,
 * which independently confirms that repair does not divide between them. */
#define REPAIR_PER_STARDATE_DOCKED  50  /*@BINARY*/  /* Round(t*100) div 2, 0x020389 */

/* -------------------------------------------------- life support reserve
 *
 * BUILT 2026-08-27, and the reason a Research Station is not the useless
 * stop this core used to make it. Life support is not only one of the
 * twelve repair percentages: there is a SEPARATE RESERVE, a real at
 * [0x1D30] measured in stardates, with a hard cap of 2.0.
 *
 *   - docking at ANY of the three base types refills it to 2.0 (0x00F072)
 *   - the reserve life support ITEM adds 1.0 and clamps to 2.0 (0x009861,
 *     0x009888), prints "Replenishing reserve life support.", and decrements
 *     the inventory count at [0x234F]; used when not on reserve it answers
 *     "Not on reserve life support."
 *   - [0x26D6] is the flag for "the console is showing the reserve panel",
 *     set at 0x01FF4B when life support fails and cleared by both the dock
 *     and the item
 *
 * The panel swap is at 0x01FF2F: it repaints the box at (160,250)-(319,349)
 * and guards the repaint by reading a pixel back, which is the original's own
 * "is this panel already swapped" test. That region is columns 20..39 and
 * rows 17..24 in this port, which is exactly panels[P_SYSTEMS] -- so the
 * panel the original replaces was identified rather than chosen. See
 * draw_reserve() in c128/src/ui.c.
 *
 * The drain lives in trek_advance_time and uses the ORIGINAL elapsed time,
 * not the remainder a repair focus leaves. See the comment there. */
#define LIFE_RESERVE_MAX_TENTHS    20  /* 2.0 stardates */  /*@BINARY*/
#define LIFE_RESERVE_ITEM_TENTHS   10  /* the item adds 1.0 */  /*@BINARY*/

/* TWO THRESHOLDS, AND THEY ARE NOT THE SAME NUMBER -- both read 2026-08-27.
 *
 *   the console panel swaps as soon as Life Support is not PERFECT. The draw
 *   routine opens `cmp word ptr [0x235E], 0x64` / `je` at 0x01FDC9, so
 *   anything below 100 shows the reserve panel instead of the normal one.
 *
 *   the reserve only DRAINS below 90, at 0x02042E, and only while undocked.
 *
 * So there is a band, 90..99, where the panel has already changed and the
 * countdown has not started. Modelling it as one threshold would lose that. */
#define LIFE_PANEL_BELOW          100  /*@BINARY*/
#define LIFE_DRAIN_BELOW           90  /*@BINARY*/

/* ------------------------------------------- the Mongol boarding party
 *
 * BINARY 2026-08-27, `fn 0x15D6E`, segment base 0x150C0 (5/5 on disjoint
 * spans). The routine map called this "department damage" and it is nothing
 * of the kind -- the third label in that map to be wrong, after the plasma
 * bolt and the main program.
 *
 * Called from the turn loop at 0x005954, so it is tested EVERY TURN:
 *
 *     if (aboard && stardate > deadline)  they are eliminated, and the
 *                                         routine returns without rolling
 *     if (level < 4)                      never -- `cmp [0x1DF0],7 / jg`
 *     if (aboard)                         one party at a time
 *     if (shields up)                     they beam in, so shields stop them
 *     if (no enemy in the quadrant)       `cmp galaxy[q], 99 / jg`
 *     if (Random(100) <= 95)              4 in 100
 *     aboard   = Random(3) + 1
 *     deadline = stardate + 0.5 + Random*0.5
 *
 * A REDUNDANCY WORTH RECORDING RATHER THAN BUILDING: the Engineering branch
 * at 0x015E61 clears the shields-up flag [0x26DC]. It can never do anything,
 * because the gate five instructions earlier already required that flag to
 * be clear. Reading it as "taking Engineering drops your shields" would have
 * invented a mechanic the original cannot reach.
 *
 * What each department costs, and the original's own words:
 *
 *   Engineering    SHIELDS UP refused -- "Cannot raise shields; Mongol
 *                  boarding party controls engineering."  0x00EA12
 *   Laser control  "Mongol boarding party controls the lasers."  0x009CD0
 *   EnTorp control "EnTorp control is held by the Mongol boarding party."
 *                                                          0x00B60B */
#define BOARD_NONE          0  /*@ID*/
#define BOARD_ENGINEERING   1  /*@BINARY*/
#define BOARD_LASERS        2  /*@BINARY*/
#define BOARD_TUBES         3  /*@BINARY*/
#define BOARD_DEPARTMENTS   3   /* Random(3) + 1 */  /*@BINARY*/
#define BOARD_MIN_LEVEL     4   /* [0x1DF0] > 7, and [0x1DF0] is level + 4 */  /*@BINARY*/
#define BOARD_OF_N        100  /*@BINARY*/
#define BOARD_ABOVE        95   /* Random(100) > 95, so four in a hundred */  /*@BINARY*/
#define BOARD_BASE_TENTHS   5   /* 0.5 stardates */  /*@BINARY*/
#define BOARD_SPAN_TENTHS   5   /* plus Random*0.5 */  /*@BINARY*/

/* ------------------------------------------------ the SPONTANEOUS supernova
 *
 * BINARY 2026-08-27, `fn 0x1ED00`, segment base 0x1BD60. This is the SECOND
 * of the two routes and it is nothing like the first: a torpedo detonating a
 * star (the 4% branch of 0x0A8C8) throws the ship across the galaxy, and this
 * one CANNOT TOUCH THE SHIP AT ALL. The emulator observation of being blown
 * to quadrant 8-4 belongs to the torpedo route only.
 *
 *     if (Random(100) != 0)  return            ; one in a hundred, per turn
 *     repeat
 *         row = Random(8)+1;  col = Random(8)+1
 *     until row != [0x1DE4]                    ; NOT the ship's quadrant ROW
 *       and (galaxy[q] mod 10) > 0             ; the quadrant has a star
 *       and galaxy[q] != 999                   ; and is not already burnt
 *
 * THE ROW EXCLUSION IS NOT A TYPO and it is the third quirk of its shape in
 * this program, after the death pod's column 8 and reinforcements' column 1.
 * It tests the row alone, so a supernova never occurs anywhere along the
 * ship's own rank -- eight quadrants, not one.
 *
 * Then it kills what is there and tells you:
 *
 *     n = galaxy[q] div 100                    ; the Mongols in it
 *     [0x1DC2] -= n                            ; enemies remaining
 *     if ([0x1DC2] == 0) [0x26C6] = 1          ; SO IT CAN WIN THE GAME
 *     galaxy[q] = 999                          ; 0x1EEEB
 *     chart[q]  = 2                            ; 0x1EEFF -- the player LEARNS
 *                                                of it without scanning
 *
 * and it cancels what was pending there: the base-under-attack marker at
 * [0x1E0A]/[0x1E0C] is cleared if it names this quadrant, and a second
 * quadrant pair at [0x1E1C]/[0x1E1E] is cleared along with its schedule slot
 * [0x1D9C], which is set to the real 9999 -- never. That second pair is NOT
 * identified; this core maps the behaviour onto its own base state and says
 * so at the call site rather than guessing what the pair is.
 *
 * Its message is "COMMUNICATIONS:  Dept. of Space warns of a star having
 * gone supernova in quadrant R-C", ending "." with nothing there and
 * ";  N Mongols reported destroyed." otherwise. */
#define NOVA_OF_N         100   /* Random(100) == 0 */  /*@BINARY*/
#define NOVA_CHART_BURNT  999   /* what the chart prints for a burnt cell */  /*@BINARY*/

/* ---------------------------------------- the TORPEDO's supernova, fn 0x0A8C8
 *
 * BINARY 2026-08-27, base 0x9310 at 10/10. THE OTHER ROUTE, and the one that
 * can kill you -- everything the emulator saw and could not explain.
 *
 *     if (Random(100) <= 95) it is not a supernova     ; 4% if it is
 *     "Star at R-C goes supernova!"
 *
 * WHERE THE SHIP GOES IS NOT RANDOM. It is pushed one quadrant, and the
 * direction compares the star's SECTOR coordinate against the ship's QUADRANT
 * coordinate -- dimensionally incoherent, and exactly what the code does:
 *
 *     nqy = qy;  nqx = qx
 *     if      (star_row < qy && qy < 8)  nqy = qy + 1
 *     else if (star_row > qy && qy > 1)  nqy = qy - 1
 *     else if (star_col < qx && qx < 8)  nqx = qx + 1
 *     else if (qy < 8)                   nqy = qy + 1
 *     else                               nqy = qy - 1
 *
 * That reproduces the one measured observation: a star at sector 4-4 with the
 * ship in quadrant 7-4 gives 4 < 7 and 7 < 8, so nqy = 8 -- "Lexington blown
 * to quad 8-4", which is what the screen said.
 *
 *     every Mongol in the quadrant dies (the four hp slots are zeroed at
 *         0x00AAC8) and the count drops; reaching zero WINS
 *     damage = Random(600)
 *         shields up   shields -= damage, whole      "unit hit absorbed"
 *         shields down energy  -= damage, floor 0    "units of damage"
 *                      and the damage is ADDED TO THE TURN'S HIT TALLY at
 *                      0x00ABFE, so it can wreck systems through the ordinary
 *                      Round(hits/350)+1 path
 *     galaxy[here] = 999                                        0x00AC84
 *     if (galaxy[destination] == 999)  "Lexington destroyed."    0x00AC2F
 *
 * THE LAST LINE IS THE STING: thrown into a quadrant that is ALREADY burnt,
 * the ship is destroyed. It is also why the "blown to quad" message is
 * suppressed when the destination is 999 -- you never arrive.
 *
 * Unlike the spontaneous route this one does NOT write the recorded chart.
 * It does not need to: you were there. */
#define NOVA_STAR_ABOVE    95   /* Random(100) > 95 on a star hit */  /*@BINARY*/
#define NOVA_HIT_MAX      600   /* Random(600) */  /*@BINARY*/

#define DOCK_OK              0  /*@ID*/
#define DOCK_NO_BASE         1   /* no base adjacent */  /*@ID*/
#define DOCK_ALREADY         2  /*@ID*/

uint8_t trek_dock(void);
void    trek_undock(void);       /* any movement breaks the dock */

/* Spend a reserve life support canister. Returns 0 if the ship is not on
   reserve, which the UI reports as the original's own refusal. */
uint8_t trek_life_replenish(void);

/* Docked at a StarBase specifically -- the case where the base's shields
   cover us. Research stations and supply bases do not. */
uint8_t trek_docked_safe(void);

/* Every enemy in the quadrant fires. Returns how many events were written.
   Damage lands on the shields first and on the main banks after those are
   gone, which is what the original does. */
/* `player_fired` non-zero if the command that ended the turn was an attack.
   MEASURED 2026-08-21: enemies move when the player FIRES, not merely when a
   turn passes. They moved after every volley across a torpedo session, while
   twenty-six consecutive impulse-move turns the day before produced no motion
   at all from two enemies -- shields up and down, and with the clock forced
   across a scheduled event. */
uint8_t trek_enemy_turn(TrekEvent *ev, uint8_t max, uint8_t player_fired);

/* Fire one torpedo at a sector. A torpedo destroys a standard Mongol
   outright, confirmed against the original. */
#define TORP_OK          0   /* hit, target survived */  /*@ID*/
#define TORP_KILL        1  /*@ID*/
#define TORP_MISS        2  /*@ID*/
#define TORP_NONE_LEFT   3  /*@ID*/
#define TORP_BAD_COORDS  4  /*@ID*/
#define TORP_BOARDED     10  /* EnTorp control is held */  /*@ID*/
#define TORP_NOVA        11  /* the star went supernova */  /*@ID*/
/* A star ANYWHERE IN THE FLIGHT PATH stops the torpedo -- fn 0x00A8C8, which
 * the march calls on '*'. Read out of the binary 2026-08-26, and it is three
 * outcomes, not one:
 *
 *     Random(100) > 95        4.0%   the star goes SUPERNOVA           UNBUILT
 *     then Random(100) < 40  38.4%   "Torpedo absorbed by star."
 *     otherwise              57.6%   the star is DESTROYED, cell -> 'N'  UNBUILT
 *
 * The 57.6% case is where the "Stars destroyed @ -5" scoring line comes from:
 * it increments [0x1DF6] and rewrites the sector map. Both unbuilt cases want
 * things this core does not have (a nova cell type, quadrant destruction), so
 * every star hit currently answers TORP_ABSORBED. See MEASURED.md. */
#define TORP_ABSORBED    5   /* a star stopped it */  /*@ID*/
#define TORP_PLANET      6   /* "Torpedo hit a planet." */  /*@BINARY*/
#define TORP_BASE_HIT    7   /* "Are you mad? You damaged a base!" */  /*@BINARY*/
#define TORP_DUD         8   /* "EnTorp fails to detonate." */  /*@BINARY*/
#define TORP_THROUGH     9   /* passed through the cell, no effect */  /*@BINARY*/

/* ------------------------------------------------- THE TORPEDO, as it is
 *
 * Read out of fn 0x00B5FD and fn 0x00B1CE on 2026-08-26, and it replaced
 * three FITTED constants and a model that was a stand-in for the real one.
 * THE ORIGINAL RAY-MARCHES. There is no accuracy roll, no range falloff and
 * no damage spread; all three were the visible shadow of the march.
 *
 *     y = ship_y;  x = ship_x                      (1-based, as reals)
 *     if shields up:  y += (charge/25000)*rnd;  x += (charge/25000)*rnd
 *     dy = target_y - ship_y;  dx = target_x - ship_x
 *     step = 1 / max(|dy|, |dx|)
 *     loop:
 *         y += step*dy + rnd*0.1
 *         x += step*dx + rnd*0.1
 *         if y or x leaves 0.5..8.5  ->  "Clean miss, sir."
 *         d = Sqrt(Frac(x)^2 + Frac(y)^2)
 *         look at sector[Round(y), Round(x)] and stop on anything but empty
 *
 * `rnd` is Turbo Pascal's argument-less Random, a real in [0,1) -- so BOTH
 * wobbles are ONE-SIDED. Every torpedo drifts toward higher y and x, and the
 * further it flies the further it drifts. That is not a reading of a
 * distribution; it is what the code does, and it is why the nineteen measured
 * shots looked the way they did.
 *
 * On reaching an enemy, fn 0x00B1CE:
 *
 *     if (d >= 0.6)          nothing -- it passed through the cell
 *     base = (level + 4) * 15 + 250
 *     if (Random(50) == 0)   "EnTorp fails to detonate."
 *     if (d < 0.3)           hp -= base                  a direct hit
 *     else                   hp -= Round((1 - d) * base) a graze
 *
 * WHICH SETTLES THE MEASUREMENT. Nineteen shots read 355 at close range,
 * 210/355/355/247/209/296 at range 5 and 176/209/229/247 with three misses at
 * 7.62. `base` at level 3 is 7*15+250 = 355 exactly -- the "cap" was the
 * damage all along -- and the largest possible graze is 0.7*355 = 248.5, so
 * 248. The largest non-355 reading was 247. Eighteen of the nineteen fall
 * inside the law; the 296 does not, and no level produces it. Recorded as an
 * open discrepancy rather than smoothed over -- see MEASURED.md. */
#define TORP_DAMAGE_AT_LEVEL(l)  ((uint16_t)((((l) + 4) * 15) + 250))  /*@BINARY*/
#define TORP_DUD_OF_N            50   /* 1 in 50 fails to detonate */  /*@BINARY*/

/* Three tubes. MEASURED from the original's own refusals -- "Captain, we have
   only three tubes." and "Captain, only N tubes are functional." -- and it
   asks how many to fire before it asks where. The binary agrees: the volley
   loop at 0x00B962 runs 1..N with N read from "Number to fire: ". */
#define TORP_TUBES             3  /*@CONFIRMED*/

/* The march, in 8.8 fixed point and 1-BASED so Frac and Round mean what they
   mean in the original. Everything here is a decoded real: 0.1, 25000.0, 0.5,
   8.5, 0.3, 0.6. */
#define TORP_JITTER_88      26    /* 0.1 of a cell, per step, per axis */  /*@BINARY*/
#define TORP_DEFLECT_DEN    98    /* charge/25000 in 8.8 is charge/97.66 */  /*@BINARY*/
#define TORP_EDGE_LO_88    128    /* 0.5 */  /*@BINARY*/
#define TORP_EDGE_HI_88   2176    /* 8.5 */  /*@BINARY*/
/* NOTE: on an 8x8 grid the rounded-cell guard in trek_fire_torpedo subsumes
   the upper edge -- widening TORP_EDGE_HI_88 changes no test, and that was
   checked rather than assumed. The original needs both because its sector map
   is TEN wide with a border the torpedo may legitimately round into. Kept
   here so the two tests read the same way in both. */
/* d is compared SQUARED, so the direct/graze/through decisions need no square
   root at all: (0.3*256)^2 = 5898.24 and (0.6*256)^2 = 23592.96, each rounded
   up so a `<` test means exactly `d < 0.3` and `d < 0.6`.

   The full-precision square is what makes the damage HOLE exact. An earlier
   version halved the fractions to keep the product small and put the boundary
   at 0.297 instead of 0.300, which let 250 out -- a value the original cannot
   produce. Neither fraction can exceed 153 without the shot being a
   pass-through anyway, and 2*153^2 fits in sixteen bits, so nothing is lost. */
#define TORP_FRAC_MAX      153    /* above this on either axis it cannot hit */  /*@ID*/
#define TORP_DIRECT_Q     5899    /* d < 0.3  -- a direct hit */  /*@BINARY*/
#define TORP_GRAZE_Q     23593    /* d < 0.6  -- a graze; beyond it, nothing */  /*@BINARY*/
/* `damage` receives the figure delivered; pass NULL if not wanted. */
uint8_t trek_fire_torpedo(uint8_t sy, uint8_t sx, uint16_t *damage);

/* Full strength of a class, so a display can show an enemy's remaining hit
   points as the percentage the original's INFO panel calls "Shields". */
uint16_t trek_enemy_full_hp(uint8_t type);

/* Self destruct. MEASURED 2026-08-24: it destroys NOTHING.
 *
 * This port carried the ancestor's kaboom() -- everything whose power times
 * its distance fell within 25 times our remaining energy died with us, so a
 * full tank took the quadrant with it. Run 5 tested it directly: four enemies
 * present, the nearest at range 1.41, zero killed. SELFDESTRUCT_FACTOR is
 * gone with the rule it belonged to.
 *
 * The password is the UI's business -- the core is not told it, because the
 * core formats no text and compares no strings. Returns how many enemies went
 * with us, which is now always nought, and sets ship.lost. */
uint8_t trek_self_destruct(void);

/* Mission state. */
#define GAME_ON          0  /*@ID*/
#define GAME_WON         1   /* every Mongol destroyed */  /*@ID*/
#define GAME_LOST        2   /* ship gone */  /*@ID*/
uint8_t trek_game_state(void);

/* The itemised score, using the original's own rubric. MEASURED, every weight
 * read off its evaluation screen -- see MEASURED.md for the sheet, whose
 * arithmetic closes exactly, so no term is hidden.
 *
 * The kill-rate term was open item 13 for three sessions: it printed 0.00
 * against two kills in 1.2 elapsed stardates, which should have been worth
 * 833. The ancestor explains the shape and half the answer:
 *
 *     perdate = (initial_enemies - remaining) / timused;
 *     score_itemf("%6.2f Klingons per stardate  %5d", perdate,
 *                 500 * perdate + 0.5);
 *
 * The coefficient is 500 in both games -- EGA Trek's own sheet says "@ 500 per
 * day" -- so this is the same term. And the ancestor already treats "enemies
 * remain" as a special case for THIS term and no other, inflating timused to a
 * floor of five stardates when the mission is unfinished.
 *
 * That floor alone gives 2/5 = 0.40, not 0.00, so the ancestor's version is
 * refuted by our reading. What survives is the condition: EGA Trek gates on
 * the same thing the ancestor clamps on, and zeroes the term outright rather
 * than merely inflating the divisor. One reading supports that, so it is
 * FITTED, not measured -- but it is a far better-founded guess than before,
 * because the condition is no longer invented.
 *
 * Terms this core does not have: stars destroyed (-5), which needs a torpedo
 * that can hit one. Rescues (+200) WERE on this list and are now live -- see
 * core/planet.h and SCORE_PER_RESCUE below. */
/* MEASURED off the Detailed Evaluation's own rubric, and reachable at last:
   settlements evacuated, at 200 apiece. See core/planet.h for what produces
   them. */
#define SCORE_PER_RESCUE       200  /*@BINARY*/
#define SCORE_PER_MONGOL        10  /*@MEASURED*/
#define SCORE_PER_COMMANDER     20  /*@MEASURED*/
#define SCORE_PER_ENEMY_BASE    50  /*@MEASURED*/
#define SCORE_PER_KILL_DAY     500  /*@BINARY*/
#define SCORE_BASE_LOST       (-200)   /* "Bases hit", i.e. ours, lost */  /*@BINARY*/
#define SCORE_INCOMPLETE      (-300)  /*@MEASURED*/

/* MEASURED 2026-08-24 off two loss sheets, combat and self-destruct alike. It
   is a flat penalty for losing the ship, SEPARATE from the casualties, and it
   is absent from a surviving ship's sheet. See the note below CREW_COMPLEMENT
   for why this constant was deleted once and why that was a mistake. */
#define SCORE_SHIP_LOST       (-200)  /*@MEASURED*/

/* SETTLED 2026-08-24 by a combat death: the line is REAL and it is -200.

   The Lexington was destroyed in battle with no kills and no elapsed time. Its
   sheet printed `Penalty for loss of ship ... -200` above the casualties line
   and totalled -930: -200 -300 -430. A self-destruct sheet the same day showed
   the same line. A surviving ship's sheet does NOT carry it, so it is
   conditional on losing the ship and on nothing else.

   The 2026-08-20 reading that produced -730 and removed this constant was
   wrong. SCORE_SHIP_LOST is restored below.

   Historical note follows.

   CONTESTED 2026-08-24 -- read this before trusting the paragraph below.
   A self-destruct sheet from run 4 PRINTS "Penalty for loss of ship .... -200"
   as its own line, above the casualties line, and the arithmetic closes to the
   unit: -200 -300 +20 +20 -430 = the -890 shown. So the line exists.

   Either the 2026-08-20 reading missed it, or the penalty attaches to
   self-destruction and not to being shot down. ONE sample of dying in combat
   settles it, and it is cheap. The constant stays out until then, because
   restoring it on one screenshot would be the same mistake pointing the other
   way. See MEASURED.md, "Runs 4 and 5".

   The original note follows.

   There is no ship-loss line on the original's sheet. MEASURED 2026-08-20 by
   losing the ship and reading the Detailed Evaluation: the whole penalty is
   the crew, at a point each, and -300 + -430 came to exactly the -730 printed.
   An earlier SCORE_SHIP_LOST (-200) here was invented and double-counted. */
#define CREW_COMPLEMENT        430  /*@MEASURED*/

/* THE GATE ON THE RATE TERM, read out of fn 0x1DD4F at 0x01E270:
 *
 *     if (stardate < 3503.0)  rate = 0
 *     else                    rate = kills / (stardate - 3500.0)
 *
 * A hard zero below THREE stardates, and no floor above it -- not the
 * ancestor's five-stardate clamp, which this port had been applying always.
 * And **it is not gated on finishing the mission**, which is the other half
 * of what this port invented: the rate is computed whether or not enemies
 * remain, and only the -300 incomplete penalty depends on that.
 *
 * This is the "something gates it that we do not understand" from
 * 2026-08-21. The sheet printed 0.00 against two kills in 1.2 stardates
 * because 3501.2 is under 3503, full stop. */
#define SCORE_MIN_TENTHS        30   /* under 3.0 stardates the term is zero */  /*@BINARY*/

int16_t trek_score(void);

/* The Detailed Evaluation, line by line, exactly as the original prints it --
   MEASURED 2026-08-21 from a real end screen (see MEASURED.md). The core fills
   this and derives trek_score() from its total, so the sheet the player reads
   and the number recorded can never disagree.
 *
 * Two items have no mechanism behind them yet and are always zero: enemy
 * bases destroyed, and stars destroyed -- the latter is [0x1DF6] in the
 * original, at -5 each, incremented by a torpedo that destroys a star, which
 * is a mechanic this core does not have. Rescues DO count now, and note that
 * they are forfeit with the ship: the binary credits them in the same branch
 * that applies the -200 (0x01E3A0). Bases hit counts OUR bases lost to ANY
 * cause including our own torpedo (0x00B10C), where this port counts only
 * the ones a siege takes.
 *
 * All nine rows are kept as fields whatever happened, because the original
 * prints all nine, and because an absence is a to-do rather than a design
 * choice. */
typedef struct {
    uint16_t rescues;        int16_t rescue_pts;
                             int16_t incomplete_pts;
    int16_t  ship_lost_pts;  /* -200 when the ship was lost, else 0 */
    uint16_t mongols;        int16_t mongol_pts;
    uint16_t commanders;     int16_t commander_pts;
    uint16_t enemy_bases;    int16_t enemy_base_pts;
    uint16_t rate_hundredths; int16_t rate_pts;   /* kills per stardate x100 */
    uint16_t casualties;     int16_t casualty_pts;
    uint16_t stars;          int16_t star_pts;
    uint16_t bases_hit;      int16_t bases_hit_pts;
    int16_t  total;
} ScoreSheet;

void trek_score_sheet(ScoreSheet *s);

/* Union bases lost to enemy sieges. Scored, and worth having separately
   because it is the one number that says whether the deadlines were met. */
extern uint8_t bases_lost;

/* ------------------------------------------------- damage has consequences
 *
 * MEASURED from the manual 2026-08-24, which specifies an effect for every
 * system. Until then this port modelled all twelve repair percentages, drew
 * them, repaired them -- and consulted exactly ONE of them, the converter. A
 * ship at 10% on everything played identically to a ship at 100%.
 *
 * These are queries rather than rules scattered through the commands, so that
 * every platform gets the same answer and the manual's wording lives in one
 * place next to the number it produced.
 */

/* Maximum warp in tenths. Manual: "the maximum warp speed is approximately
   warp 1 plus 0.09 times percentage of repair" -- so 100% gives warp 10,
   which WARP_MAX then clamps to the emergency ceiling of 8. */
uint8_t trek_max_warp(void);

/* Manual: impulse engines "either work or they don't. When they are at less
   than 50% they simply stop functioning." */
uint8_t trek_impulse_ok(void);

/* Manual: "At 100% there are three functional tubes, 67-99% only two tubes
   work and 34-66% only one." Below 34% none. */
uint8_t trek_tubes_available(void);

/* Scanner resolution. Manual, short range: "Above 90% they are fully
   functional, but below 90% they are unable to detect anything smaller than
   a star. Below 50% they do not function at all." Long range: "When less
   than 100% repaired they can no longer detect enemy ships. Below 50% they
   are not functional." */
#define SCAN_DEAD    0  /*@ID*/
#define SCAN_COARSE  1   /* stars only -- no ships */  /*@ID*/
#define SCAN_FULL    2  /*@ID*/
uint8_t trek_srscan_level(void);
uint8_t trek_lrscan_level(void);

/* Manual: "Automatic navigation requires the computer to be 100% repaired."
   Below that the original falls back to manual DeltaX/DeltaY entry. */
uint8_t trek_autonav_ok(void);

/* Manual: transporter and shuttlecraft "must be at 100% to be used". */
uint8_t trek_transporter_ok(void);
uint8_t trek_shuttle_ok(void);

/* Lasers, combining heat with battle damage. Manual: laser repair percentage
   "is a direct indication of what percentage of energy is converted to
   destructive force ... 100% working lasers will do twice the damage of 50%
   working lasers", and the effectiveness gauge "goes down due to excess heat
   AND due to damage from enemy fire". */
uint8_t trek_laser_eff(void);

uint8_t trek_set_warp(uint8_t tenths);  /* 0 if rejected */
/* The occupied sector that stopped the last move, 0-based, valid only after
   a call that returned MOVE_BLOCKED. Always in the quadrant the ship is in. */
extern uint8_t trek_block_y, trek_block_x;

uint8_t trek_move_impulse(uint8_t sy, uint8_t sx);
uint8_t trek_move_warp(uint8_t qy, uint8_t qx, uint8_t sy, uint8_t sx);

/* Manual movement: a signed offset in SECTORS on each axis, `dy` vertical and
 * `dx` horizontal. The UI parses the original's "1.0" and "-2.2" notation --
 * digit before the point is quadrants, after it is sectors -- and hands the
 * result here as eight-sectors-to-the-quadrant.
 *
 * This is core rather than UI because it is arithmetic on the galaxy, and
 * because the platform layer is where nothing can test it. It also picks the
 * right engine: a delta that stays inside the current quadrant is an impulse
 * hop, not a warp jump.
 *
 * Manual l.515-529, and the ONLY way to move with the computer below 100%. */
uint8_t trek_move_delta(int16_t dy, int16_t dx);

#endif
