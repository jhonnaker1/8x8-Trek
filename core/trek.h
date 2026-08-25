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

/* Enemy count -- MEASURED 2026-08-24, and the formula is
 *
 *     enemies = 10 + 8 * level + rand(0..8)
 *
 * NINETEEN samples, all inside the predicted range, and the endpoints are hit
 * at three different levels:
 *
 *     level 1   18, 21          predicted 18..26   (18 is the exact minimum)
 *     level 2   30, 32          predicted 26..34
 *     level 3   34 37 37 38 38 38 40 42 42 42   predicted 34..42
 *     level 4   42, 47          predicted 42..50   (42 is the exact minimum)
 *     level 5   53, 55          predicted 50..58
 *
 * Level 3's ten samples span 34..42 exactly -- the whole range, both ends.
 *
 * This REPLACES the earlier fit of `level*10 + rand(0..12)`, which the same
 * data also satisfies but which predicts level 3 could roll 30..33. Ten
 * samples never did, and the chance of missing four of thirteen values ten
 * times running is about one in forty. The level-1 and level-4 readings
 * landing exactly on this model's minima is the other reason to prefer it.
 *
 * The old note called it "fitted, not confirmed ... more new games at a
 * single level would tighten it". That is what happened. */
#define ENEMY_BASE        10
#define ENEMY_PER_LEVEL    8
#define ENEMY_SPREAD       9     /* trek_rand_n(9) gives the observed 0..8 */
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
#define HP_SCOUT            255  /* MEASURED 2026-08-21: the INFO panel names
                                    the 255-hit-point ship "Mongol Scout" at
                                    Shields 100%. This also closes the reading
                                    that had been unexplained since the first
                                    session -- 255 was never damage. */

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

/* MEASURED from the manual 2026-08-24, which gives the whole table:
 *
 *     1x    normal repairs, divided evenly among damaged systems
 *     2.5x  normal repairs while docked at a starbase
 *     3x    repairing only a selected system
 *     5x    repairing a selected system while docked at a starbase
 *
 * So focusing is worth 3x, not the 2 this was DERIVED at. The docked figures
 * are consistent with REPAIR_PER_STARDATE 20 and _DOCKED 47 already measured
 * off the original (2.35x against the manual's 2.5x, within the display's
 * resolution), and 3 x 20 = 60 against 5 x 20 = 100 docked-and-focused. */
#define REPAIR_FOCUS_FACTOR  3

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
    uint8_t  repair_focus;    /* 0 = spread across everything, else 1+index of
                                 the one system engineering is concentrating
                                 on -- the F)ix command. */
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
/* Manual: impulse engines below 50% "simply stop functioning", and the
   original refuses with "ENGINEERING: Move aborted; impulse engines are too
   damaged to use" -- a hard no, not a slower move. */
#define MOVE_NO_IMPULSE     6

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
#define ENEMY_FIRE_PCT          78   /* percent of hit points, before falloff */

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

/* Restoring a saved game needs to put back state that is otherwise private to
   trek.c: the RNG and the schedule as ABSOLUTE dates, where trek_schedule()
   takes an offset from now. Only core/serial.c calls these. They live here
   rather than the serialiser having its own copy of the state, because two
   copies of the schedule is exactly how a reloaded game ends up with
   deadlines the panel promised and the queue has never heard of. */
uint16_t trek_rng_state(void);
void     trek_rng_restore(uint16_t v);
void     trek_sched_restore(uint8_t kind, uint16_t when);

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
/* MEASURED 2026-08-21 off the STATE OF REPAIR dialog, which prints Docked and
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
#define REPAIR_PER_STARDATE_DOCKED  47

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
/* `player_fired` non-zero if the command that ended the turn was an attack.
   MEASURED 2026-08-21: enemies move when the player FIRES, not merely when a
   turn passes. They moved after every volley across a torpedo session, while
   twenty-six consecutive impulse-move turns the day before produced no motion
   at all from two enemies -- shields up and down, and with the clock forced
   across a scheduled event. */
uint8_t trek_enemy_turn(TrekEvent *ev, uint8_t max, uint8_t player_fired);

/* Fire one torpedo at a sector. A torpedo destroys a standard Mongol
   outright, confirmed against the original. */
#define TORP_OK          0   /* hit, target survived */
#define TORP_KILL        1
#define TORP_MISS        2
#define TORP_NONE_LEFT   3
#define TORP_BAD_COORDS  4
/* MEASURED 2026-08-23: firing a torpedo AT a star answers "Torpedo absorbed by
 * star." and the star survives -- it is not a miss, and the shot is spent. A
 * star in the FLIGHT PATH is a different case entirely: it goes supernova and
 * takes the quadrant with it. This port does not ray-march yet, so only the
 * first case is modelled; the second is what the "Stars destroyed @ -5"
 * scoring line comes from. */
#define TORP_ABSORBED    5   /* the target cell held a star */

/* Torpedo damage. MEASURED 2026-08-21 against the original, by writing a large
 * hit-point value into the target so it SURVIVES the shot and then reading the
 * damage out of the enemy table -- the game never prints it, which is what had
 * kept this unmeasurable. Nineteen shots at three ranges:
 *
 *     range 1.41-2.24   6 shots   355 every time, no variance
 *     range 5.00        6 shots   210, 355, 355, 247, 209, 296
 *     range 7.62        7 shots   176, 209, 229, 247, and three misses
 *
 * CONFIRMED: 355 is a CAP, not the damage. It binds every time up close --
 * which is exactly why one torpedo has always killed a 355-hit-point
 * battleship and nothing ever survived to report a figure -- binds twice in
 * six at range 5, and never at 7.6. A Commander takes 355 as well, so the cap
 * is a constant and not the target's own strength.
 *
 * The tactical consequence: a Commander SURVIVES a torpedo at 695-355=340 and
 * needs two.
 *
 * FITTED: the base and the spread. 500 through the same falloff our lasers use
 * plus a 0..100 roll reproduces all three ranges -- always capped inside 2.5,
 * capped 37% of the time at range 5 against 33% observed, and 183..283 at 7.6
 * against 176..247 observed. Nineteen shots is not many for three constants,
 * and the ancestor's own 700+100*Rand() is a different base entirely. */
#define TORP_MAX_DAMAGE      355   /* MEASURED, the cap */
#define TORP_BASE            500   /* FITTED, before falloff */
#define TORP_SPREAD          101   /* FITTED, a 0..100 roll on top */

/* Accuracy, MEASURED the same day: 6/6 inside range 2.24, 6/6 at range 5.00,
 * 4/7 at 7.62. Certain out to five and degrading past it. The original
 * announces a miss as "Clean miss, sir!".
 *
 * FITTED from one long-range block, so the slope is soft: 16% per unit past
 * the sure range gives 68% at range 7 where 4/7 was seen. */
/* Three tubes. MEASURED from the original's own refusals -- "Captain, we have
 * only three tubes." and "Captain, only N tubes are functional." -- and it
 * asks how many to fire before it asks where. */
#define TORP_TUBES             3

#define TORP_SURE_DIST         5   /* whole sectors; inside this it cannot miss */
#define TORP_MISS_PCT_PER_UNIT 16
/* `damage` receives the figure delivered; pass NULL if not wanted. */
uint8_t trek_fire_torpedo(uint8_t sy, uint8_t sx, uint16_t *damage);

/* Full strength of a class, so a display can show an enemy's remaining hit
   points as the percentage the original's INFO panel calls "Shields". */
uint16_t trek_enemy_full_hp(uint8_t type);

/* Self destruct. DERIVED from the ancestor's kaboom(): everything in the
   quadrant whose power times its distance is within 25 times our remaining
   energy is destroyed with us. So a full tank takes the quadrant with it and
   an empty one takes almost nothing, which is what makes it a last act rather
   than a weapon.
 *
 * The password is the UI's business -- the core is not told it, because the
 * core formats no text and compares no strings. Returns how many enemies went
 * with us, and sets ship.lost. */
#define SELFDESTRUCT_FACTOR  25
uint8_t trek_self_destruct(void);

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

/* The Detailed Evaluation, line by line, exactly as the original prints it --
   MEASURED 2026-08-21 from a real end screen (see MEASURED.md). The core fills
   this and derives trek_score() from its total, so the sheet the player reads
   and the number recorded can never disagree.
 *
 * Three items have no mechanism behind them yet and are always zero: rescues,
 * enemy bases destroyed, and stars destroyed. They are kept as fields rather
 * than omitted because the original prints all nine rows whatever happened,
 * and because their absence is a to-do rather than a design choice. */
typedef struct {
    uint16_t rescues;        int16_t rescue_pts;
                             int16_t incomplete_pts;
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
#define SCAN_DEAD    0
#define SCAN_COARSE  1   /* stars only -- no ships */
#define SCAN_FULL    2
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
uint8_t trek_move_impulse(uint8_t sy, uint8_t sx);
uint8_t trek_move_warp(uint8_t qy, uint8_t qx, uint8_t sy, uint8_t sx);

#endif
