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
#define HP_COMMAND          500  /* inferred, bracketed 441..501 */
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

typedef struct {
    uint8_t  quad_y, quad_x;
    uint8_t  sec_y, sec_x;
    uint16_t energy;        /* main banks -- warp drive draws from here */
    uint16_t impulse;       /* impulse engines have their own pool */
    uint16_t shields;       /* shield charge, separate from shields_up */
    uint8_t  torps;
    uint8_t  laser_eff;     /* percent -- heat and battle damage combined,
                               as the original's single gauge reports it */
    uint8_t  warp;          /* tenths, 10..80 */
    uint8_t  shields_up;
    uint16_t stardate;      /* tenths */
    uint16_t stardate_end;  /* tenths -- mission deadline */
    uint8_t  level;         /* 1..5, the command level / rank */
    uint16_t enemies_left;
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

uint8_t trek_set_warp(uint8_t tenths);  /* 0 if rejected */
uint8_t trek_move_impulse(uint8_t sy, uint8_t sx);
uint8_t trek_move_warp(uint8_t qy, uint8_t qx, uint8_t sy, uint8_t sx);

#endif
