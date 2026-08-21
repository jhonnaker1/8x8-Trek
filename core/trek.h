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

#define GAL_DIM     8            /* 8x8 quadrants */
#define QUAD_DIM    8            /* 8x8 sectors per quadrant */
#define GAL_CELLS   (GAL_DIM * GAL_DIM)
#define QUAD_CELLS  (QUAD_DIM * QUAD_DIM)

/* Sector cell contents. One byte per cell; the UI maps these to glyph and
   colour (see core/ega.h for the colour rules, which are game rules). */
#define SEC_EMPTY       0
#define SEC_STAR        1
#define SEC_SHIP        2
#define SEC_BASE        3
#define SEC_PLANET      4
#define SEC_BATTLESHIP  5
#define SEC_COMMAND     6
#define SEC_SCOUT       7
#define SEC_SUPPLY      8

#define SEC_IS_ENEMY(c) ((c) >= SEC_BATTLESHIP && (c) <= SEC_SUPPLY)

/* Base types, as the long-range scanner reports them (manual l.291):
   1 StarBase, 2 research station, 3 supply depot. */
#define BASE_NONE       0
#define BASE_STARBASE   1
#define BASE_RESEARCH   2
#define BASE_SUPPLY     3

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
#define ENERGY_START    5000
#define ENERGY_MAX      5000
#define IMPULSE_START    500
#define IMPULSE_MAX      500
#define SHIELD_START    2500
#define SHIELD_MAX      2500     /* manual l.501, confirmed on screen */

/* MEASURED at command level 3: nine, shown as a 3x3 array of red stars below
   the shields dial, one per torpedo. Firing one put it out. Three tubes and
   nine torpedoes is a coherent bit of design; an earlier guess of 10 was
   wrong. Whether the count varies by level is untested. */
#define TORPS_START      9

/* Enemy count, FITTED from five readings of the original (one per command
   level): 18, 32, 40, 42, 53. Those are not a straight line -- level 4 only
   just exceeds level 3 -- so the count is random within a level-dependent
   range. Subtracting a level*10 base leaves 8, 12, 10, 2, 3: small,
   non-negative, and uncorrelated with level.

   Fitted, not confirmed. Five samples fix the base convincingly (every
   reading is at or above level*10) but only bound the spread from below --
   the true upper limit could exceed the 12 we happened to see. More new
   games at a single level would tighten it. */
#define ENEMY_PER_LEVEL   10
#define ENEMY_SPREAD      13     /* trek_rand_n(13) gives the observed 0..12 */
#define ENERGY_PER_DAY  400      /* manual l.265, at 100% repair */

/* Stardates are carried in tenths, so these exceed cc65's 16-bit signed int
   and must be written unsigned -- an unsuffixed 35000 promotes to long,
   which this core forbids (cl65 warns "Constant is long"). */
#define STARDATE_START  35000U   /* 3500.0 */
#define MISSION_TENTHS  300U     /* 30 stardates to secure the sector */
#define WARP_MIN        10       /* tenths: warp 1.0 */
#define WARP_MAX        80       /* tenths: warp 8.0, emergency (manual l.250) */
#define WARP_CRUISE     60       /* tenths: warp 6.0, safe cruising */
#define WARP_START      10       /* CONFIRMED: the original opens at warp 1.0,
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
#define IMPULSE_ENERGY_UNIT   6  /* per sector -- measured, see above */

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
 * original's single gauge does. Temperature alone costs nothing until some
 * threshold above 700 of the gauge's 1500 -- 1250 units of fire went out at
 * a flat 100% in the clean run.
 *
 * LASER_RANGE_ZERO is in whole sectors; distances are 8.8 fixed point, so
 * comparisons against it must shift. */
#define LASER_RANGE_ZERO     12  /* distance at which damage would reach 0 */

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
 * See MEASURED.md. */
#define LASER_HEAT_MAX     1500

/* A note on the provenance tags in this file, now that there is a fifth.
 *
 *   CONFIRMED   -- predicted in advance, then seen to happen in the original.
 *   MEASURED    -- read off the original, by screen or by memory.
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
/* HIT POINTS SCALE WITH COMMAND LEVEL. A battleship reads 325 in a level-1
 * game and 355 in a level-3 one -- 30 across two levels, so 15 per level:
 *
 *     battleship hp = 310 + 15 * level
 *
 * Both readings were confirmed as battleships in the viewer, and nine ships
 * across three level-1 quadrants all read exactly 325. Two points define a
 * line, so a level-5 game (predicting 385) would settle it.
 *
 * The constants below are the LEVEL-3 values, which is what the core uses
 * today; the scaling is not modelled yet. Whether supply ships scale the same
 * way is untested -- 120 is a single level-3 reading. */
#define HP_BATTLESHIP       355  /* read from memory, level 3 */
#define HP_SUPPLY           120  /* read from memory, level 3 */
#define HP_COMMAND          695  /* MEASURED: read from the enemy table at
                                    level 3, for the ship the console names
                                    the Mongol Commander. The earlier 500 was
                                    inferred from damage arithmetic and
                                    bracketed 441..501; it was wrong. */
#define HP_SCOUT            100  /* unmeasured */

/* Per-sector enemy strength, parallel to `sector` and rebuilt with it.
   Non-zero only where sector[] holds an enemy. */
extern uint16_t enemy_hp[QUAD_CELLS];

/* Outcomes of firing. */
#define FIRE_OK           0   /* hit, target survived */
#define FIRE_KILL         1   /* hit, target destroyed */
#define FIRE_BAD_COORDS   2
#define FIRE_NO_TARGET    3   /* nothing hostile in that sector */
#define FIRE_NO_ENERGY    4   /* main banks hold less than requested */

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

/* MEASURED: the POWER DISTRIB report taken immediately after SHUP on a fresh
   game, with nothing else having happened, showed main at 4950 of 5000. This
   is the manual's unquantified "small amount of energy from the main energy
   banks" (l.339-341). Lowering shields is free. */
#define SHIELD_RAISE_COST    50

/* Galaxy, as four flat byte arrays rather than an array of structs. Flat
   arrays carry no padding, so the 68000 alignment tax the commodore-uno
   README documents for its `Card` struct cannot apply here. Indexed
   [y * GAL_DIM + x]. 256 bytes total. */
extern uint8_t gal_enemies[GAL_CELLS];
extern uint8_t gal_base[GAL_CELLS];
extern uint8_t gal_stars[GAL_CELLS];
extern uint8_t gal_known[GAL_CELLS];   /* 0 = never scanned */

/* The current quadrant only, rebuilt on entry. Indexed [y * QUAD_DIM + x]. */
extern uint8_t sector[QUAD_CELLS];

/* Ship systems, in the order the original's ENGINEERING REPORT lists them.
 *
 * MEASURED: the original keeps these as twelve 16-bit repair percentages at
 * DS:235A, all 100 on a fresh game. Twelve rather than the ten on its console
 * because the manual documents a transporter and a shuttlecraft that the
 * console never shows. See MEASURED.md. */
#define SYS_CONVERTER    0
#define SYS_SHIELDS      1
#define SYS_LIFE         2
#define SYS_LASERS       3
#define SYS_TUBES        4
#define SYS_WARP         5
#define SYS_IMPULSE      6
#define SYS_SRSCAN       7
#define SYS_LRSCAN       8
#define SYS_COMPUTER     9
#define SYS_TRANSPORTER 10
#define SYS_SHUTTLE     11
#define SYS_COUNT       12

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
    uint16_t stardate_end;  /* tenths -- mission deadline */
    uint8_t  level;         /* 1..5, the command level / rank */
    uint16_t enemies_left;
    uint8_t  sys[SYS_COUNT];  /* repair percentage, 0..100 */
    uint16_t killed;          /* standard Mongols destroyed */
    uint16_t killed_cmd;      /* command ships, scored separately */
    uint16_t casualties;
    uint8_t  lost;            /* ship destroyed */
    uint8_t  docked;          /* BASE_NONE, or the type docked at */
} Ship;

extern Ship ship;

/* Outcomes of a move request. The UI turns these into messages; the core
   never formats text, so it stays free of any platform's character set. */
#define MOVE_OK             0
#define MOVE_BLOCKED        1   /* an object is in the way */
#define MOVE_NO_ENERGY      2
#define MOVE_SAME_PLACE     3
#define MOVE_BAD_COORDS     4
#define MOVE_WARP_TOO_LOW   5   /* distance needs a higher warp factor */

/* Deterministic PRNG. Owned here rather than taken from libc so that a seed
   reproduces the same galaxy on every platform -- which is what makes a
   cross-platform comparison of the same game meaningful. */
void     trek_srand(uint16_t seed);
uint16_t trek_rand(void);
uint8_t  trek_rand_n(uint8_t n);        /* 0 .. n-1 */

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

/* Enemy tactical movement.
 *
 * DERIVED from the ancestor's movebaddy() (reference/sst2k, ai.c), and
 * anchored to observation: in a level-3 game the Mongol Commander walked
 * 3-8 -> 3-7 -> 4-6 -> 5-5 over four turns, one sector per turn, closing on
 * the ship and then holding, while a second enemy never moved at all. The
 * console narrates each step ("The commander has moved. He is now at 5-5").
 *
 * The ancestor decides advance / hold / retreat from a "forces" score built
 * out of the enemy's power, how many enemies are present, and how dangerous
 * we look -- shields up, energy in the banks, torpedoes left. That shape is
 * kept; its constants are not measured for EGA Trek and are marked where they
 * appear in trek.c.
 *
 * Reported through EV_ENEMY_MOVED, which was already defined below and until
 * now never emitted by anything. */
void trek_new_game(uint8_t level, uint16_t seed);
void trek_enter_quadrant(void);         /* rebuild `sector` from the chart */

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
#define POOL_MAIN        1
#define POOL_IMPULSE     2
#define POOL_SHIELDS     3

#define DIVERT_OK        0
#define DIVERT_ILLOGICAL 1   /* same pool, or a pool that does not exist */
#define DIVERT_SHORT     2   /* source does not hold that much */

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
#define SHIELD_OK          0
#define SHIELD_ALREADY     1
#define SHIELD_NO_ENERGY   2   /* main banks cannot pay the raising cost */

uint8_t trek_shields_up(void);
uint8_t trek_shields_down(void);


/* MEASURED: floor(20 * elapsed_stardates), and it does NOT divide between
   damaged systems -- two damaged at once each repaired at the full rate. The
   manual claims the crew divide their time evenly; the original does not. */
#define REPAIR_PER_STARDATE  20

/* Short range scanner resolution, MEASURED off the original's own code:
   above 90 everything shows, at or below 50 nothing does, and in between
   only the ship, stars and novas. */
#define SRSCAN_FULL          90
#define SRSCAN_BLIND         50

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
 * FITTED: the coefficient. Damage over hit points read 0.733 at range 2.828,
 * 0.511 at 4.243 and 0.522 at 5.657, and 90% through our own laser falloff is
 * the best single number across those three.
 *
 * NOT MEASURED, and known to be wrong: the falloff SHAPE. Those last two
 * ranges are indistinguishable, which no falloff reaching zero at 12 allows.
 * The real curve flattens into a floor somewhere near half the firer's hit
 * points. It is left alone here rather than fitted to three points, all of
 * which were diagonal offsets. */
#define ENEMY_FIRE_PCT          90   /* percent of hit points, before falloff */

/* MEASURED: enemies hold fire on about half of their turns -- 5/10, 5/10,
   7/10 and 4/8 across four blocks. Not an artefact of dropped turns: a
   dropped turn shows up as a zero followed by a doubled reading, and the
   surviving figures were tight with no doubles. */
#define ENEMY_FIRE_ONE_IN        2

/* PROVISIONAL: a hit that gets past the shields can wreck a system. The
   original clearly does this -- a single ambush took out the scanner, life
   support and the shield generators -- but neither the chance nor the
   severity has been measured. */
#define SYSTEM_DAMAGE_THRESHOLD  100   /* penetrating hit needed to risk it */

/* What happened during a turn. The core never formats text, so it hands the
   UI a list of facts and lets each platform word them. */
#define EV_NONE          0
#define EV_HIT           1   /* y,x = firing sector; amount = damage */
#define EV_SHIELD_HOLD   2   /* amount = absorbed entirely by shields */
#define EV_SYSTEM_HIT    3   /* y = system index; amount = new repair pct */
#define EV_SHIP_LOST     4
#define EV_ENEMY_MOVED   5   /* y,x = where it moved to */
/* Scheduled events, reported the same way. y,x are the GALAXY cell they
   concern, not a sector, except EV_POD_HIT which concerns the whole
   quadrant. `amount` on EV_BASE_ATTACKED is the deadline as a stardate in
   tenths, so the UI can print "they can last until". */
#define EV_BASE_ATTACKED 6
#define EV_BASE_LOST     7
#define EV_TRACTORED     8   /* y,x = the quadrant we were dragged to */
#define EV_POD_HIT       9   /* amount = damage, to us and to every enemy */

typedef struct {
    uint8_t  kind;
    uint8_t  y, x;
    uint16_t amount;
} TrekEvent;

/* ------------------------------------------------------- scheduled events
 *
 * DERIVED from the ancestor's events.c (reference/sst2k), which is where the
 * deadline messages in EGA Trek's COMMUNICATIONS panel come from. Two seen in
 * captures, both carrying a stardate the player can still act before:
 *
 *   "The StarBase in 6-6 reports that it is under attack. They can last
 *    until 3517.8."
 *   "Planet Gallista-8, quad 8-4, requests evacuation. They can only hold
 *    out until 3516.5."
 *
 * That deadline IS the queue, visible from outside: something is scheduled to
 * destroy the base, and the game tells you when.
 *
 * The ancestor's design, kept: a fixed slot per event type rather than a real
 * queue, so only one of each can be pending. It says so itself -- "This isn't
 * a real event queue a la BSD Trek yet". For a core that must run in 16 bits
 * on a 6502 that limitation is a feature: no allocation, no list, one array
 * of dates.
 *
 * Deliberately NOT included: the ancestor's snapshot-for-time-warp, deep
 * space probes, and supercommander movement. Those need features this core
 * does not have, and inventing them would be worse than leaving them out. */
#define SCHED_BASE_ATTACK    0   /* a base comes under attack */
#define SCHED_BASE_FALLS     1   /* ... and is destroyed if not relieved */
#define SCHED_TRACTOR        2   /* a commander drags us across the galaxy */
#define SCHED_DEATH_POD      3   /* a Vandal death pod arrives */
#define SCHED_COUNT          4

#define SCHED_NEVER     0xFFFFU  /* the ancestor's FOREVER */

void     trek_schedule(uint8_t kind, uint16_t offset_tenths);
void     trek_unschedule(uint8_t kind);
uint8_t  trek_is_scheduled(uint8_t kind);
uint16_t trek_scheduled(uint8_t kind);   /* absolute stardate, or SCHED_NEVER */

/* Exponential deviate with the given mean, both in tenths of a stardate --
 * the ancestor's expran(), which is -mean * ln(u).
 *
 * Integer by necessity and by choice. The core forbids float and long, and
 * `int` is 16 bits under cc65 but 32 on the 68000, so anything relying on
 * promotion would compute different event schedules on the Amiga than on the
 * C128 -- silently. This is a table of -ln(u) indexed by five bits of the
 * PRNG, with the multiply staged so no intermediate leaves 16 bits. The whole
 * schedule is therefore reproducible from a seed on every target, which is
 * what makes the native test suite an oracle for platforms we cannot run. */
uint16_t trek_expran(uint16_t mean_tenths);

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
 * So the three base types give different things, and only a StarBase makes a
 * quadrant safe. Life support supplies are not modelled as a resource -- this
 * core carries life support as one of the twelve repair percentages -- which
 * leaves a Research Station offering nothing but the docked repair rate. That
 * is faithful rather than useful, and is what the manual describes.
 *
 * Adjacency is the ancestor's rule and the manual's alike: any of the eight
 * neighbouring sectors, not the base's own cell.
 *
 * DERIVED, and the one number here worth checking: the ancestor repairs at
 * 1/docfac = 4x while docked. EGA Trek's own STATE OF REPAIR dialog prints
 * Docked and Undocked columns side by side, so a single screenshot with any
 * system damaged settles it. */
#define DOCK_REPAIR_FACTOR   4

#define DOCK_OK              0
#define DOCK_NO_BASE         1   /* no base adjacent */
#define DOCK_ALREADY         2

uint8_t trek_dock(void);
void    trek_undock(void);       /* any movement breaks the dock */

/* Docked at a StarBase specifically -- the case where the base's shields
   cover us. Research stations and supply bases do not. */
uint8_t trek_docked_safe(void);

/* Every enemy in the quadrant fires. Returns how many events were written.
   Damage lands on the shields first and on the main banks after those are
   gone, which is what the original does. */
uint8_t trek_enemy_turn(TrekEvent *ev, uint8_t max);

/* Fire one torpedo at a sector. A torpedo destroys a standard Mongol
   outright, confirmed against the original. */
#define TORP_OK          0
#define TORP_KILL        1
#define TORP_MISS        2
#define TORP_NONE_LEFT   3
#define TORP_BAD_COORDS  4
uint8_t trek_fire_torpedo(uint8_t sy, uint8_t sx);

/* Mission state. */
#define GAME_ON          0
#define GAME_WON         1   /* every Mongol destroyed */
#define GAME_LOST        2   /* ship gone */
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
 * Terms this core does not have: rescues (+200), which need planet
 * evacuations, and stars destroyed (-5), which need a torpedo that can hit
 * one. Both are in the rubric and neither is reachable yet. */
#define SCORE_PER_MONGOL        10
#define SCORE_PER_COMMANDER     20
#define SCORE_PER_ENEMY_BASE    50
#define SCORE_PER_KILL_DAY     500
#define SCORE_BASE_LOST       (-200)   /* "Bases hit", i.e. ours, lost */
#define SCORE_INCOMPLETE      (-300)

/* There is no ship-loss line on the original's sheet. MEASURED 2026-08-20 by
   losing the ship and reading the Detailed Evaluation: the whole penalty is
   the crew, at a point each, and -300 + -430 came to exactly the -730 printed.
   An earlier SCORE_SHIP_LOST (-200) here was invented and double-counted. */
#define CREW_COMPLEMENT        430

/* Minimum elapsed time for the rate term, in tenths -- the ancestor's five
   stardates. It applies unconditionally here, where the ancestor applies it
   only when time is zero or enemies remain; with the term gated on a finished
   mission, its other trigger cannot fire. Without a floor a win at 0.1
   stardates would compute 500 x kills x 10, which leaves 16 bits. */
#define SCORE_MIN_TENTHS        50

int16_t trek_score(void);

/* Union bases lost to enemy sieges. Scored, and worth having separately
   because it is the one number that says whether the deadlines were met. */
extern uint8_t bases_lost;

uint8_t trek_set_warp(uint8_t tenths);  /* 0 if rejected */
uint8_t trek_move_impulse(uint8_t sy, uint8_t sx);
uint8_t trek_move_warp(uint8_t qy, uint8_t qx, uint8_t sy, uint8_t sx);

#endif
