/* Native tests for the shared core.
 *
 * Built with cc on the build machine, not cl65. Everything here is pure
 * arithmetic and state, so none of it needs a 6502 or an emulator -- and a
 * session on this machine cannot screenshot an emulator anyway, which is
 * exactly why anything provable natively should be proved natively.
 *
 * Run: make test (from the repo root)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../trek.h"
#include "../planet.h"

static int failures = 0;

static void ok(int cond, const char *what) {
    printf("  %-52s %s\n", what, cond ? "ok" : "FAIL");
    if (!cond) failures++;
}

/* ---------------------------------------------------------------------- */

static void test_distance(void) {
    puts("distance table");

    ok(trek_dist(0, 0) == 0,    "d(0,0) is zero");
    ok(trek_dist(0, 1) == 256,  "d(0,1) is 1.0 in 8.8");
    ok(trek_dist(1, 0) == 256,  "d(1,0) is 1.0 in 8.8");
    ok(trek_dist(0, 7) == 1792, "d(0,7) is 7.0 in 8.8");
    ok(trek_dist(3, 4) == 1280, "d(3,4) is exactly 5.0 (3-4-5 triangle)");
    ok(trek_dist(7, 7) == 2534, "d(7,7) is sqrt(98), the table maximum");

    {
        int dy, dx, sym = 1, mono = 1;
        for (dy = 0; dy < 8; dy++)
            for (dx = 0; dx < 8; dx++) {
                if (trek_dist(dy, dx) != trek_dist(dx, dy)) sym = 0;
                if (dx > 0 && trek_dist(dy, dx) <= trek_dist(dy, dx - 1)) mono = 0;
            }
        ok(sym,  "symmetric in dy/dx");
        ok(mono, "strictly increasing along a row");
    }
}

/* The warp cost and time expressions shift in two stages specifically to
   keep intermediates inside 16 bits. If someone "simplifies" them back to a
   single multiply, this is what notices -- on the build machine the wider
   int would hide it, so the check is written against the 16-bit bound
   directly rather than against the result. */
static void test_no_16bit_overflow(void) {
    /* d16_max is the longest path the walk can produce: corner to corner of
       the galaxy, sqrt(63^2 + 63^2) sectors, in sixteenths. d88_max is the
       same distance handed to warp_energy() in 8.8 quadrants, which is twice
       it. Both are bigger than the old bound of 2534, because the distance is
       now taken over ABSOLUTE sector positions rather than quadrant indices
       -- so these numbers moved when the model was corrected and every one
       below is a fresh check, not a survivor. */
    unsigned long d16_max = 1426, d88_max = 2852, w_max = WARP_MAX;

    puts("16-bit overflow bounds");

    ok((d88_max >> 4) * 14UL + 128UL <= 65535UL,
       "warp cost distance factor 128+d44*14 fits uint16");
    ok(((w_max * w_max) / 10UL) * w_max <= 65535UL,
       "warp cubed, staged, fits uint16");
    ok(43UL * d16_max <= 65535UL,
       "warp time numerator 43*d16 fits uint16");
    ok(((43UL * d16_max) / ((w_max * w_max) / 2UL)) * 10UL <= 65535UL,
       "...and scaling its quotient to hundredths still fits");
    ok((((w_max * w_max) / 2UL) - 1UL) * 10UL <= 65535UL,
       "...and scaling its remainder to hundredths still fits");
    ok(25UL * 158UL <= 65535UL,
       "impulse time numerator 25*d16 fits uint16 (d16 <= 158 in a quadrant)");
    ok(2UL * 63UL * 63UL + 63UL <= 65535UL,
       "path_round's 2*|d|*i+n fits uint16 at galaxy scale");
    ok(w_max * w_max <= 65535UL,
       "squared warp fits uint16 before scaling");
    ok(d16_max * IMPULSE_ENERGY_UNIT <= 65535UL,
       "impulse cost intermediate d16*IMPULSE_ENERGY_UNIT fits uint16");
    ok(d88_max * w_max > 65535UL,
       "...and the naive d*warp really would have overflowed");
}

static void test_generation(void) {
    int i, enemies = 0, bases = 0, starless = 0;
    int expect;

    puts("galaxy generation");

    trek_new_game(3, 12345);
    expect = ENEMY_BASE + 3 * ENEMY_PER_LEVEL;  /* lower bound; rest is random */

    for (i = 0; i < GAL_CELLS; i++) {
        enemies += gal_enemies[i];
        if (gal_base[i] != BASE_NONE) bases++;
        if (gal_stars[i] > 8) starless++;
    }

    /* A range, not an equality: the original's count is random within the
       level's band, so pinning it to one number would be asserting a
       fiction. */
    ok(enemies >= expect && enemies < expect + ENEMY_SPREAD,
       "enemy total sits inside the fitted level band");
    ok(ship.enemies_left == (uint16_t)enemies,
       "enemies_left agrees with the galaxy");

    /* Every level's band must start at level*10 and stay inside the spread,
       across many seeds -- this is what would notice a base or spread that
       drifted away from the five readings taken off the original. */
    {
        uint8_t lvl;
        uint16_t seed;
        int bad = 0;
        for (lvl = 1; lvl <= 5; lvl++) {
            for (seed = 1; seed < 200; seed++) {
                trek_new_game(lvl, seed);
                if (ship.enemies_left <
                        (uint16_t)(ENEMY_BASE + lvl * ENEMY_PER_LEVEL) ||
                    ship.enemies_left >=
                        (uint16_t)(ENEMY_BASE + lvl * ENEMY_PER_LEVEL + ENEMY_SPREAD))
                    bad++;
            }
        }
        ok(bad == 0, "all five levels stay in band across 199 seeds");
    }

    /* The MEASURED ranges, written out rather than derived from the same
       constants the code uses -- so a change to ENEMY_BASE or
       ENEMY_PER_LEVEL fails here instead of silently moving the band with
       the test. Nineteen readings off the original sit inside these, and
       level 3's ten samples hit both ends. See core/trek.h. */
    {
        static const uint16_t lo[6] = { 0, 18, 26, 34, 42, 50 };
        static const uint16_t hi[6] = { 0, 26, 34, 42, 50, 58 };
        uint8_t lvl;
        uint16_t seed;
        int bad = 0;
        for (lvl = 1; lvl <= 5; lvl++)
            for (seed = 1; seed < 200; seed++) {
                trek_new_game(lvl, seed);
                if (ship.enemies_left < lo[lvl] || ship.enemies_left > hi[lvl])
                    bad++;
            }
        ok(bad == 0, "every level matches the ranges measured off the original");
    }

    trek_new_game(3, 12345);   /* restore the state the rest of this test uses */
    ok(bases >= 2 && bases <= 4, "between two and four bases placed");
    ok(starless == 0,            "every quadrant has 0..8 stars");

    {
        int over = 0;
        for (i = 0; i < GAL_CELLS; i++) if (gal_enemies[i] > 4) over++;
        ok(over == 0, "no quadrant holds more than four enemies");
    }

    ok(gal_enemies[(ship.quad_y << 3) | ship.quad_x] == 0,
       "ship starts in a quadrant with no enemies");
    ok(ship.quad_y < 8 && ship.quad_x < 8 && ship.sec_y < 8 && ship.sec_x < 8,
       "ship coordinates are in range");
    ok(ship.energy == ENERGY_START && ship.torps == TORPS_START,
       "ship starts fully supplied");
    /* Three pools, as the original's ENGINEERING REPORT shows. Shields start
       charged and lowered -- charge and raised-state are independent. */
    ok(ship.impulse == IMPULSE_START, "impulse engines start with their own pool");
    ok(ship.shields == SHIELD_START,  "shields start fully charged");
    ok(ship.shields_up == 0,          "...but lowered");
    ok(ship.warp == WARP_START,       "opens at warp 1.0 like the original");
}

static void test_determinism(void) {
    uint8_t a_en[GAL_CELLS], a_st[GAL_CELLS];
    uint8_t qy, qx;

    puts("determinism");

    trek_new_game(3, 4242);
    memcpy(a_en, gal_enemies, sizeof a_en);
    memcpy(a_st, gal_stars, sizeof a_st);
    qy = ship.quad_y; qx = ship.quad_x;

    trek_new_game(3, 4242);
    ok(memcmp(a_en, gal_enemies, sizeof a_en) == 0, "same seed reproduces enemy layout");
    ok(memcmp(a_st, gal_stars, sizeof a_st) == 0,   "same seed reproduces star layout");
    ok(qy == ship.quad_y && qx == ship.quad_x,      "same seed reproduces start position");

    trek_new_game(3, 9999);
    ok(memcmp(a_en, gal_enemies, sizeof a_en) != 0, "a different seed differs");
}

static void test_quadrant_contents(void) {
    int i, ships = 0, stars = 0, enemies = 0;
    uint8_t q;

    puts("quadrant population");

    trek_new_game(3, 777);
    q = (uint8_t)((ship.quad_y << 3) | ship.quad_x);

    for (i = 0; i < QUAD_CELLS; i++) {
        if (sector[i] == SEC_SHIP) ships++;
        else if (sector[i] == SEC_STAR) stars++;
        else if (SEC_IS_ENEMY(sector[i])) enemies++;
    }

    ok(ships == 1, "exactly one ship on the short range scan");
    ok(sector[(ship.sec_y << 3) | ship.sec_x] == SEC_SHIP,
       "the ship cell agrees with the ship's coordinates");
    ok(stars == gal_stars[q],     "star count matches the chart");
    ok(enemies == gal_enemies[q], "enemy count matches the chart");
    ok(gal_known[q] != 0,         "the current quadrant is marked scanned");
}

static void test_reveal(void) {
    int known = 0, i;

    puts("long range reveal");

    trek_new_game(3, 31337);
    for (i = 0; i < GAL_CELLS; i++) if (gal_known[i]) known++;

    /* A corner start reveals 4 quadrants, an edge 6, the interior 9. */
    ok(known >= 4 && known <= 9, "entering reveals the 3x3 block around us");
    ok(known == 9 || ship.quad_y == 0 || ship.quad_y == 7 ||
       ship.quad_x == 0 || ship.quad_x == 7,
       "fewer than 9 only when against an edge");
}

static void test_impulse(void) {
    uint16_t e0;
    uint16_t t0;
    uint8_t r, oy, ox, ty = 0, tx = 0, found = 0;
    int i;

    puts("impulse movement");

    trek_new_game(3, 2024);
    oy = ship.sec_y; ox = ship.sec_x;

    r = trek_move_impulse(oy, ox);
    ok(r == MOVE_SAME_PLACE, "moving to our own sector is refused");
    ok(trek_move_impulse(9, 0) == MOVE_BAD_COORDS, "out-of-range sector is refused");

    /* Find any empty cell to move to. */
    for (i = 0; i < QUAD_CELLS; i++) {
        if (sector[i] == SEC_EMPTY) { ty = (uint8_t)(i >> 3); tx = (uint8_t)(i & 7); found = 1; break; }
    }
    ok(found, "an empty sector exists to move into");

    e0 = ship.energy; t0 = ship.stardate;
    r = trek_move_impulse(ty, tx);
    ok(r == MOVE_OK, "move into an empty sector succeeds");
    ok(ship.sec_y == ty && ship.sec_x == tx, "ship coordinates updated");
    ok(sector[(ty << 3) | tx] == SEC_SHIP,   "ship drawn at the new cell");
    ok(sector[(oy << 3) | ox] == SEC_EMPTY,  "old cell vacated");
    ok(ship.stardate >= t0, "stardate did not go backwards");
    (void)e0;

    /* Moving onto an occupied cell must be refused. */
    for (i = 0; i < QUAD_CELLS; i++) {
        if (sector[i] == SEC_STAR) {
            r = trek_move_impulse((uint8_t)(i >> 3), (uint8_t)(i & 7));
            ok(r == MOVE_BLOCKED, "moving onto a star is blocked");
            break;
        }
    }
}

static void test_warp(void) {
    uint16_t e0, t0;
    uint8_t r, qy, qx;

    puts("warp movement");

    trek_new_game(3, 5150);
    ok(trek_set_warp(5) == 0,   "warp 0.5 rejected (below minimum)");
    ok(trek_set_warp(90) == 0,  "warp 9.0 rejected (above maximum)");
    ok(trek_set_warp(50) == 1,  "warp 5.0 accepted");

    qy = (uint8_t)((ship.quad_y + 1) & 7);
    qx = ship.quad_x;
    if (qy == ship.quad_y) qy = (uint8_t)((qy + 1) & 7);

    e0 = ship.energy; t0 = ship.stardate;
    r = trek_move_warp(qy, qx, 3, 3);
    ok(r == MOVE_OK, "warp to an adjacent quadrant succeeds");
    ok(ship.quad_y == qy && ship.quad_x == qx, "quadrant updated");
    ok(ship.sec_y == 3 && ship.sec_x == 3,     "sector updated");
    ok(ship.stardate > t0, "stardate advanced");
    /* THIS USED TO ASSERT A NET LOSS AT WARP 5, AND THE ORIGINAL DOES NOT
       MAKE ONE. The warp-5 reading in MEASURED.md is `+5.7 = 400 * 0.5 -
       cost`: main energy went UP by 5.7 over an interval containing the jump.
       The old time law was 10% short and truncated a 0.44-stardate jump to
       0.3, which hid the refill and made the assertion pass for the wrong
       reason.

       What is true, and what the manual actually says, is that travel at
       CRUISING speed and above outruns the converter. Break-even sits just
       above warp 5 -- cost goes as warp^3 and the refill as 1/warp^2 -- so
       warp 6 is where the manual's "faster than you can regenerate it"
       starts. Below it, energy is cheap and TIME is the price: a one-quadrant
       hop at warp 1 costs 11 stardates out of a 30-stardate mission. */
    ok(ship.energy <= e0, "warp 5 does not turn a profit");

    {
        uint16_t e1;
        trek_set_warp(WARP_CRUISE);
        e1 = ship.energy;
        qy = (uint8_t)((ship.quad_y + 1) & 7);
        if (qy == ship.quad_y) qy = (uint8_t)((qy + 1) & 7);
        trek_move_warp(qy, ship.quad_x, 3, 3);
        ok(ship.energy < e1, "warping at cruise costs more than the converter replaces");
    }
    ok(sector[(3 << 3) | 3] == SEC_SHIP, "ship placed in the new quadrant");

    /* Shields up doubles the cost -- the one figure the manual states
       outright (l.255). Same jump, same seed, both directions. */
    {
        uint16_t cost_down, cost_up;

        trek_new_game(3, 8080);
        trek_set_warp(50);
        ship.shields_up = 0;
        e0 = ship.energy;
        trek_move_warp((uint8_t)((ship.quad_y + 2) & 7), ship.quad_x, 1, 1);
        cost_down = (uint16_t)(e0 - ship.energy);

        trek_new_game(3, 8080);
        trek_set_warp(50);
        ship.shields_up = 1;
        e0 = ship.energy;
        trek_move_warp((uint8_t)((ship.quad_y + 2) & 7), ship.quad_x, 1, 1);
        cost_up = (uint16_t)(e0 - ship.energy);

        ok(cost_up > cost_down, "raised shields cost more energy to warp");
    }

    /* Same-quadrant warp is really an impulse move. */
    trek_new_game(3, 606);
    r = trek_move_warp(ship.quad_y, ship.quad_x, ship.sec_y, ship.sec_x);
    ok(r == MOVE_SAME_PLACE, "warping to where we already are is refused");
}

/* Energy transfer, including the rule that surplus poured into a full pool
   is destroyed rather than refused or returned. */
static void test_divert(void) {
    uint16_t lost;
    uint8_t r;

    puts("energy transfer");

    /* Every pool starts at its maximum, so at the opening of a game there is
       nowhere for energy to go and ANY transfer destroys what it moves. Room
       has to be made first -- which is why the observed 5000 -> 4500 into
       full shields was not bad luck but the only possible outcome. */
    trek_new_game(3, 111);
    ok(ship.energy == ENERGY_MAX && ship.impulse == IMPULSE_MAX &&
       ship.shields == SHIELD_MAX, "all three pools start full");

    ship.energy = (uint16_t)(ENERGY_MAX - 500);   /* make exactly 500 of room */
    r = trek_divert(POOL_SHIELDS, POOL_MAIN, 500, &lost);
    ok(r == DIVERT_OK,                     "shields -> main accepted");
    ok(ship.shields == SHIELD_START - 500, "shields debited");
    ok(ship.energy == ENERGY_MAX,          "main credited, at its ceiling");
    ok(lost == 0,                          "nothing lost when there is room");

    ok(trek_divert(POOL_MAIN, POOL_MAIN, 10, &lost) == DIVERT_ILLOGICAL,
       "same pool both ways is illogical");
    ok(trek_divert(POOL_MAIN, 9, 10, &lost) == DIVERT_ILLOGICAL,
       "a pool that does not exist is illogical");
    ok(trek_divert(POOL_IMPULSE, POOL_MAIN, 60000, &lost) == DIVERT_SHORT,
       "cannot divert more than the source holds");

    /* The exact transfer observed in DOSBox-X: 500 from main into shields
       that were already full. Main fell to 4500, shields stayed at 2500,
       and the energy was gone. */
    trek_new_game(3, 222);
    r = trek_divert(POOL_MAIN, POOL_SHIELDS, 500, &lost);
    ok(r == DIVERT_OK,                        "main -> full shields accepted");
    ok(ship.energy == ENERGY_START - 500,     "main debited the full 500");
    ok(ship.shields == SHIELD_MAX,            "shields unchanged at maximum");
    ok(lost == 500,                           "all 500 reported destroyed");

    /* Partial overflow: half lands, half is destroyed. */
    trek_new_game(3, 333);
    trek_divert(POOL_SHIELDS, POOL_MAIN, 200, &lost);   /* make 200 of room */
    r = trek_divert(POOL_MAIN, POOL_SHIELDS, 500, &lost);
    ok(r == DIVERT_OK,             "partial overflow accepted");
    ok(ship.shields == SHIELD_MAX, "shields back to the ceiling");
    ok(lost == 300,                "the 300 that did not fit is destroyed");
}

/* Every exact reading taken off the original, replayed. These are the whole
   point of the laser code: the formula was predicted before firing and came
   back to the unit four times, so anything but an exact match here is a bug
   in our arithmetic rather than a difference of opinion about the model.

   Efficiency was 100% for all of them -- the ship was fresh and the lasers
   cold -- which is also what makes them exact enough to assert on. */
static void test_laser_readings(void) {
    puts("laser damage -- readings off the original");

    ok(trek_laser_damage(500, 100, trek_dist(1, 1)) == 441,
       "500 at range 1.414 delivers 441");
    ok(trek_laser_damage(250, 100, trek_dist(0, 2)) == 208,
       "250 at range 2.000 delivers 208");
    ok(trek_laser_damage(250, 100, trek_dist(4, 2)) == 157,
       "250 at range 4.472 delivers 157");
    ok(trek_laser_damage(250, 100, trek_dist(4, 3)) == 146,
       "250 at range 5.000 delivers 146");
    ok(trek_laser_damage(100, 100, trek_dist(2, 1)) == 81,
       "100 at range 2.236 delivers 81");

    /* 157 and 146 are 156.83 and 145.83 exactly; truncation would print one
       lower for both, which is how we know the original rounds. */
    ok(trek_laser_damage(250, 100, trek_dist(4, 2)) == 157 &&
       trek_laser_damage(250, 100, trek_dist(4, 3)) == 146,
       "rounds to nearest rather than truncating");
}

static void test_laser_model(void) {
    uint16_t near_d, far_d, half, full;

    puts("laser damage -- shape of the model");

    near_d = trek_laser_damage(300, 100, trek_dist(1, 1));
    far_d  = trek_laser_damage(300, 100, trek_dist(7, 7));
    ok(near_d > far_d, "damage falls with distance");

    /* 12 exceeds the 9.9 diagonal of an 8x8 quadrant, so a laser always
       delivers something however far the target is -- roughly 17%. */
    ok(far_d > 0, "still bites at the far corner of the quadrant");

    /* Corner to corner is 7x the range of an adjacent sector. Inverse-square
       would cut the damage by about 49; the measured linear law cuts it by
       5. Ten separates the two comfortably. */
    ok(far_d * 10 > near_d, "and the falloff is gentle, not inverse-square");

    /* Linear in energy: confirmed on the original in a single volley, where
       500 and 250 were fired at known ranges. */
    full = trek_laser_damage(500, 100, trek_dist(2, 1));
    half = trek_laser_damage(250, 100, trek_dist(2, 1));
    ok(full == half * 2 || full == half * 2 + 1, "linear in energy");

    /* The manual states this outright: 100% lasers do twice the damage of
       50% ones for the same energy (l.380-384). Half of an odd figure rounds
       up, so doubling it can land one over. */
    {
        uint16_t at_full = trek_laser_damage(400, 100, trek_dist(2, 1));
        uint16_t at_half = trek_laser_damage(400,  50, trek_dist(2, 1));
        ok(at_half * 2 == at_full || at_half * 2 == at_full + 1,
           "linear in efficiency, as the manual states");
    }

    ok(trek_laser_damage(400, 0, trek_dist(2, 1)) == 0,
       "dead lasers deliver nothing");
    ok(trek_laser_damage(0, 100, trek_dist(2, 1)) == 0,
       "firing nothing delivers nothing");
    ok(trek_laser_damage(500, 100, LASER_RANGE_ZERO << 8) == 0,
       "damage is zero at the model's reach");

    /* energy * factor needs 21 bits before it is shifted back down, and int
       is 16-bit under cc65. The split in scale_256 has to survive the worst
       case rather than merely the plausible one. */
    ok(trek_laser_damage(65535, 100, 0) == 65535,
       "no 16-bit wrap at maximum energy and zero range");
    ok(trek_laser_damage(ENERGY_MAX, 100, trek_dist(1, 1)) == 4414,
       "no wrap at a full main bank");

    /* Holding the factor in 256ths quantises the model. The step is rounded,
       so the error cannot exceed half a step -- energy/512 -- which is why
       every reading above lands exactly (all were fired at 500 units or
       less, where half a step is under one unit) and why a full 5000-unit
       discharge can sit up to 10 out. 4414 above against the true 4410.8 is
       that effect, not a mistake.

       Asserted against the unquantised model in integers: ours * 3072 must
       stay within 6*energy of energy * (3072 - dist), plus half a unit for
       the final rounding. The worst case over all 64 distances is the corner
       at 6,7. */
    {
        unsigned long d67  = trek_dist(6, 7);
        unsigned long got  = (unsigned long)trek_laser_damage(5000, 100,
                                 (uint16_t)d67) * 3072ul;
        unsigned long want = 5000ul * (3072ul - d67);
        unsigned long slack = 6ul * 5000ul + 1536ul;
        ok(got < want + slack && want < got + slack,
           "the 256ths factor stays within half a step of the true model");
    }
}

/* Laser heat: the volley total, cleared where a volley begins. The only
   number with evidence behind it is the 1500 full scale -- both from firing
   700 for free in the original and from the FORTRAN ancestor's threshold --
   so what is tested here is the accounting, not a damage rule. */
static void test_laser_heat(void) {
    uint16_t dealt;
    uint8_t  cell, before;

    puts("laser heat");

    trek_new_game(3, 4242);
    ok(ship.laser_heat == 0, "a new game starts cold");

    ship.sec_y = 4; ship.sec_x = 4;
    cell = (uint8_t)((4 << 3) | 5);
    sector[cell]   = SEC_BATTLESHIP;
    enemy_hp[cell] = HP_BATTLESHIP;

    /* MEASURED 2026-08-24, and these used to assert the opposite of all
       three: heat is on its own small scale, the game CAPS IT AT 100, and it
       is cleared by LEAVING the quadrant rather than by starting a volley.
       The old tests encoded heat = energy fired, reset per volley, and a
       750-is-half-the-gauge assertion built on a scale that was never the
       original's. See LASER_HEAT_CAP in trek.h. */
    trek_laser_begin_volley();
    trek_fire_laser(4, 5, 360, &dealt);
    ok(ship.laser_heat == 360 / HEAT_PER_UNIT, "one shot heats by fired/18");

    sector[cell]   = SEC_BATTLESHIP;
    enemy_hp[cell] = HP_BATTLESHIP;
    trek_fire_laser(4, 5, 360, &dealt);
    ok(ship.laser_heat == 2 * (360 / HEAT_PER_UNIT),
       "and shots accumulate");

    trek_laser_begin_volley();
    ok(ship.laser_heat == 2 * (360 / HEAT_PER_UNIT),
       "a NEW VOLLEY does not clear it -- the original accumulates across them");

    /* A refused shot must not heat: the original does not spend the energy
       either, and heat is energy that actually went into the banks. */
    before = ship.laser_heat;
    trek_fire_laser(0, 0, 500, &dealt);
    ok(ship.laser_heat == before, "firing at empty space adds no heat");
    trek_fire_laser((uint8_t)9, (uint8_t)9, 500, &dealt);
    ok(ship.laser_heat == before, "off-grid coordinates add no heat");
    sector[cell]   = SEC_BATTLESHIP;
    enemy_hp[cell] = HP_BATTLESHIP;
    trek_fire_laser(4, 5, (uint16_t)(ship.energy + 1), &dealt);
    ok(ship.laser_heat == before, "a shot refused for energy adds no heat");

    /* The cap is the whole point: it is what makes LASER_HEAT_MAX the scale
       of a readout rather than a threshold anything crosses. */
    ship.laser_heat = LASER_HEAT_CAP - 1;
    ship.energy = 60000U;
    sector[cell]   = SEC_BATTLESHIP;
    enemy_hp[cell] = HP_BATTLESHIP;
    trek_fire_laser(4, 5, 5000, &dealt);
    ok(ship.laser_heat == LASER_HEAT_CAP, "heat caps at 100, it does not wrap");
    ok((uint16_t)(ship.laser_heat * LASER_HEAT_SCALE) <= LASER_HEAT_MAX,
       "so the drawn bar can never leave its own scale");

    /* Cleared by leaving the quadrant, which is what Jamie watched it do. */
    trek_move_warp((uint8_t)((ship.quad_y + 1) & 7), ship.quad_x, 3, 3);
    ok(ship.laser_heat == 0, "changing quadrant clears it");
}

static void test_fire_laser(void) {
    uint16_t dealt, before;
    uint8_t  r, cell, q;

    puts("firing the lasers");

    trek_new_game(3, 4242);

    /* Put a known target next to the ship rather than hunting for one, so
       the arithmetic under test is not at the mercy of galaxy generation. */
    ship.sec_y = 4; ship.sec_x = 4;
    cell = (4 << 3) | 5;
    sector[cell]   = SEC_BATTLESHIP;
    enemy_hp[cell] = HP_BATTLESHIP;

    r = trek_fire_laser(9, 9, 100, &dealt);
    ok(r == FIRE_BAD_COORDS, "off-grid coordinates refused");

    r = trek_fire_laser(0, 0, 100, &dealt);
    ok(r == FIRE_NO_TARGET, "firing at empty space refused");
    ok(dealt == 0,          "and nothing is reported as delivered");

    before = ship.energy;
    r = trek_fire_laser(4, 5, (uint16_t)(ship.energy + 1), &dealt);
    ok(r == FIRE_NO_ENERGY,   "asking for more than the banks hold is refused");
    ok(ship.energy == before, "and a refused shot costs nothing");

    /* Range 1.0, so the factor is (3072-256)/12 = 235 of 256: 100 units
       deliver 92. Two of those leave the battleship alive at 200 hp. */
    r = trek_fire_laser(4, 5, 100, &dealt);
    ok(r == FIRE_OK,                "a hit that does not kill reports OK");
    ok(dealt == 92,                 "delivers 92 at range 1.0");
    ok(ship.energy == before - 100, "the shot costs its energy from main");
    ok(enemy_hp[cell] == HP_BATTLESHIP - 92, "target loses exactly that much");
    ok(sector[cell] == SEC_BATTLESHIP,       "and is still there");

    q = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
    gal_enemies[q]    = 1;
    ship.enemies_left = 7;

    r = trek_fire_laser(4, 5, 500, &dealt);
    ok(r == FIRE_KILL,             "enough damage destroys the target");
    ok(sector[cell] == SEC_EMPTY,  "the sector is cleared");
    ok(enemy_hp[cell] == 0,        "and its strength with it");
    ok(gal_enemies[q] == 0,        "the quadrant's count drops");
    ok(ship.enemies_left == 6,     "and so does the galaxy's");

    r = trek_fire_laser(4, 5, 100, &dealt);
    ok(r == FIRE_NO_TARGET, "the wreck cannot be fired at again");
}

static void test_repair(void) {
    uint8_t i;
    puts("repair");

    trek_new_game(3, 99);
    for (i = 0; i < SYS_COUNT; i++)
        if (ship.sys[i] != 100) break;
    ok(i == SYS_COUNT, "every system starts intact");

    ship.sys[SYS_SRSCAN] = 5;
    trek_advance(10, 0, 0);                      /* one full stardate */
    ok(ship.sys[SYS_SRSCAN] == 25, "one stardate mends 20 points");

    ship.sys[SYS_SRSCAN] = 5;
    ship.sys[SYS_LASERS] = 5;
    trek_advance(10, 0, 0);
    ok(ship.sys[SYS_SRSCAN] == 25 && ship.sys[SYS_LASERS] == 25,
       "two damaged systems each mend the full amount, not half");

    ship.sys[SYS_SRSCAN] = 95;
    trek_advance(10, 0, 0);
    ok(ship.sys[SYS_SRSCAN] == 100, "repair stops at 100");

    /* MEASURED as floor(): 0.172 stardates gave +3, not +3.44. */
    ship.sys[SYS_SRSCAN] = 0;
    trek_advance(1, 0, 0);
    ok(ship.sys[SYS_SRSCAN] == 2, "a tenth of a stardate mends 2, truncated");
}

static void test_converter_scales_with_repair(void) {
    uint16_t healthy, broken;
    puts("energy converter");

    trek_new_game(3, 5150);
    ship.energy = 1000;
    trek_advance(10, 0, 0);
    healthy = ship.energy;

    trek_new_game(3, 5150);
    ship.energy = 1000;
    ship.sys[SYS_CONVERTER] = 50;
    trek_advance(10, 0, 0);
    broken = ship.energy;

    ok(healthy > broken, "a damaged converter generates less");
    ok(healthy - 1000 == 400, "at full repair it is 400 per stardate");
    ok(broken - 1000 == 200, "at half repair it is half that");
}

/* The first shot in an event list. Enemies move before they fire, so the
   fire event is no longer necessarily first. */
static uint16_t fire_amount(const TrekEvent *ev, uint8_t n) {
    uint8_t i;
    for (i = 0; i < n; i++)
        if (ev[i].kind == EV_HIT || ev[i].kind == EV_SHIELD_HOLD)
            return ev[i].amount;
    return 0;
}

/* Enemies hold fire on about half their turns, so a test that needs to see a
   shot has to be willing to wait for one. Returns the event count of the turn
   that fired, or 0 if none did in a generous number of tries. */
static uint8_t fire_turn(TrekEvent *ev, uint8_t max) {
    uint8_t tries, n;
    for (tries = 0; tries < 40; tries++) {
        n = trek_enemy_turn(ev, max, 1);
        if (fire_amount(ev, n) > 0) return n;
        if (ship.lost) break;
    }
    return 0;
}

/* abs difference, for tests that need trek_dist directly. */
static uint8_t abs_diff8(uint8_t a, uint8_t b) { return (uint8_t)(a > b ? a - b : b - a); }


/* Fills the eight cells around one sector with stars, so whatever is in the
   middle has nowhere to step. */
static void wall_in(uint8_t y, uint8_t x) {
    int8_t dy, dx, ny, nx;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++) {
            if (!dy && !dx) continue;
            ny = (int8_t)(y + dy); nx = (int8_t)(x + dx);
            if (ny < 0 || ny >= (int8_t)QUAD_DIM) continue;
            if (nx < 0 || nx >= (int8_t)QUAD_DIM) continue;
            sector[(ny << 3) | nx] = SEC_STAR;
        }
}

/* Enemy movement. DERIVED from the ancestor and anchored to one observed run:
   at level 3 a Commander advanced one sector per turn and then held, while a
   second enemy never moved. What is tested here is the behaviour that
   observation pins down -- that a strong enemy closes, that nothing walks
   through an occupied cell, and that no ship gets two turns in one round. */
/* Bearing. The one measured point is the original's own reading: ship at 6,4,
   Commander at 5-5, viewer showing 45.0. The rest of the compass follows from
   the convention that fixes -- east zero, anticlockwise. */
/* The exponential deviate. What matters is not that it matches libc's
   -mean*log(u) closely -- it cannot, from 32 table entries -- but that its
   MEAN is right, that it never leaves 16 bits, and that it is identical on
   every target. The last is why the multiply is staged; see trek.h. */
static void test_expran(void) {
    uint16_t i, v, lo = 0xFFFF, hi = 0;
    uint32_t total = 0;              /* test-side only; the core forbids this */

    puts("exponential deviate");

    trek_srand(4242);
    for (i = 0; i < 4096; i++) {
        v = trek_expran(400);        /* mean 40.0 stardates */
        total += v;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    {
        uint16_t mean = (uint16_t)(total / 4096);
        ok(mean >= 380 && mean <= 420, "the mean of 4096 draws is the mean asked for");
    }
    ok(lo < 100,  "some draws are much shorter than the mean");
    ok(hi > 1000, "and some much longer -- the tail is there");

    /* A large mean must not wrap. The deviate reaches 4.16x its mean, so
       anything above ~15750 tenths cannot be represented at all -- and a
       wrapped result is not a harmless wrong number, it is a distant event
       rescheduled into the next few turns. Checked across many draws because
       only the long tail of the table triggers it. */
    {
        /* Monotonicity in the mean is the property wrapping breaks, and a
           small result is NOT by itself evidence of it -- for a mean of 60000
           a draw of 1875 is the honest short tail, -ln(u) = 1/32, which comes
           up one time in thirty-two. Reseeding before each draw fixes the
           table entry, so the only thing varying is the mean. */
        static const uint16_t means[] = { 100, 1000, 8000, 15000, 30000, 60000 };
        uint16_t seed, k, prev, cur, bad = 0;
        for (seed = 1; seed < 200; seed++) {
            prev = 0;
            for (k = 0; k < sizeof means / sizeof means[0]; k++) {
                trek_srand(seed);
                cur = trek_expran(means[k]);
                if (cur < prev) bad++;
                prev = cur;
            }
        }
        ok(bad == 0, "a bigger mean never yields a smaller draw -- no wrap");
    }

    /* Just under the limit, the answer is still real rather than saturated. */
    {
        uint16_t any_mid = 0;
        trek_srand(7);
        for (i = 0; i < 512; i++) {
            v = trek_expran(15000);
            if (v > 1000 && v < 60000) any_mid++;
        }
        ok(any_mid > 0, "a mean just inside the limit still varies");
    }

    /* Same seed, same sequence -- the property the whole cross-platform
       comparison rests on. */
    {
        uint16_t a[8], b[8];
        trek_srand(99);
        for (i = 0; i < 8; i++) a[i] = trek_expran(250);
        trek_srand(99);
        for (i = 0; i < 8; i++) b[i] = trek_expran(250);
        ok(memcmp(a, b, sizeof a) == 0, "a seed reproduces the sequence exactly");
    }
}

/* The queue itself. */
/* Docking. The manual is more specific than the ancestor here -- the three
   base types replenish different things -- so most of this is testing the
   manual rather than the derivation. */
/* SHUP and SHDN. The asymmetry is the point: raising costs, lowering is free.
   Manual l.339-342. */
static void test_shields(void) {
    uint16_t before;

    puts("shields up and down");

    trek_new_game(3, 4242);
    ok(!ship.shields_up, "a new game starts with shields lowered");

    before = ship.energy;
    ok(trek_shields_up() == SHIELD_OK, "they can be raised");
    ok(ship.shields_up,               "and read as raised");
    ok(ship.energy == before - SHIELD_RAISE_COST,
       "raising costs the main banks, not the shield charge");
    ok(trek_shields_up() == SHIELD_ALREADY, "raising twice is refused");

    before = ship.energy;
    ok(trek_shields_down() == SHIELD_OK, "they can be lowered");
    ok(!ship.shields_up,                 "and read as lowered");
    ok(ship.energy == before, "lowering costs nothing and refunds nothing");
    ok(trek_shields_down() == SHIELD_ALREADY, "lowering twice is refused");

    /* Flat shields can still be raised. Raised and charged are independent --
       the original distinguishes them and so must we. */
    trek_new_game(3, 4242);
    ship.shields = 0;
    ok(trek_shields_up() == SHIELD_OK, "flat shields can still be raised");
    ok(ship.shields == 0,              "which charges nothing");

    /* And they cannot be raised on an empty tank. */
    trek_new_game(3, 4242);
    ship.energy = SHIELD_RAISE_COST - 1;
    ok(trek_shields_up() == SHIELD_NO_ENERGY,
       "too little energy to raise is refused");
    ok(!ship.shields_up,  "and they stay down");
    ok(ship.energy == SHIELD_RAISE_COST - 1, "a refused raise costs nothing");

    /* Movement, MEASURED 2026-08-21 over thirty-six turns: a Commander closes
       one sector every firing turn until it is adjacent, then holds. Fourteen
       of fourteen non-adjacent turns moved and none of the adjacent ones did.
       There is no forces score, no randomness and no retreat, so nothing about
       OUR state -- shields, energy, torpedoes -- changes it. */
    {
        TrekEvent ev[16];
        uint8_t mid = (uint8_t)((1 << 3) | 1);
        int steps;
        uint16_t last, now;

        trek_new_game(3, 31337);
        ship.sec_y = 6; ship.sec_x = 6;
        ship.shields_up = 1;
        sector[mid] = SEC_COMMAND; enemy_hp[mid] = HP_COMMAND;
        last = 0xFFFF;
        for (steps = 0; steps < 12; steps++) {
            uint8_t k;
            trek_enemy_turn(ev, 16, 1);
            for (k = 0; k < QUAD_CELLS; k++) if (sector[k] == SEC_COMMAND) break;
            now = trek_dist(abs_diff8((uint8_t)(k >> 3), ship.sec_y),
                            abs_diff8((uint8_t)(k & 7), ship.sec_x));
            if (now >= last) break;
            last = now;
        }
        ok(steps >= 4, "a Commander closes turn after turn, not now and then");
        ok(last <= 362, "until it is adjacent");

        /* And then it stops, however long we leave it. */
        {
            uint8_t before, k;
            for (k = 0; k < QUAD_CELLS; k++) if (sector[k] == SEC_COMMAND) break;
            before = k;
            for (steps = 0; steps < 10; steps++) trek_enemy_turn(ev, 16, 1);
            for (k = 0; k < QUAD_CELLS; k++) if (sector[k] == SEC_COMMAND) break;
            ok(k == before, "and holds there rather than circling");
        }
    }

}

static void test_docking(void) {
    TrekEvent ev[16];
    uint8_t n, i;

    puts("docking");

    /* Put a base next to the ship by hand rather than hunting the galaxy. */
    trek_new_game(3, 4242);
    ship.sec_y = 4; ship.sec_x = 4;
    for (i = 0; i < QUAD_CELLS; i++) if (sector[i] == SEC_BASE) sector[i] = SEC_EMPTY;
    sector[(4 << 3) | 5] = SEC_BASE;
    gal_base[(ship.quad_y << 3) | ship.quad_x] = BASE_STARBASE;

    ship.energy = 100; ship.shields = 0; ship.torps = 1; ship.impulse = 7;

    ok(trek_dock() == DOCK_OK,        "a base one sector away can be docked with");
    ok(ship.energy  == ENERGY_START,  "a StarBase refuels the main banks");
    ok(ship.shields == SHIELD_MAX,    "and recharges the shields");
    ok(ship.torps   == TORPS_START,   "and restocks the torpedo tubes");
    ok(ship.impulse == IMPULSE_START, "and the impulse engines");
    ok(trek_dock() == DOCK_ALREADY,   "docking twice is refused");
    ok(trek_docked_safe(),            "a StarBase counts as safe harbour");

    /* Diagonal counts; two sectors away does not. */
    trek_undock();
    ship.sec_y = 3; ship.sec_x = 4;                 /* diagonal to 4,5 */
    ok(trek_dock() == DOCK_OK, "diagonally adjacent is adjacent");
    trek_undock();
    ship.sec_y = 4; ship.sec_x = 7;
    ok(trek_dock() == DOCK_NO_BASE, "two sectors away is too far");

    /* Supply and research bases give less, per manual l.356-359. */
    trek_new_game(3, 4242);
    ship.sec_y = 4; ship.sec_x = 4;
    for (i = 0; i < QUAD_CELLS; i++) if (sector[i] == SEC_BASE) sector[i] = SEC_EMPTY;
    sector[(4 << 3) | 5] = SEC_BASE;
    gal_base[(ship.quad_y << 3) | ship.quad_x] = BASE_SUPPLY;
    ship.energy = 100; ship.torps = 1;
    ok(trek_dock() == DOCK_OK,      "a supply base can be docked with");
    ok(ship.torps == TORPS_START,   "a supply base restocks torpedoes");
    /* NOT `== 100`. Docking costs 0.1 stardates (MEASURED) and the converter
       runs for them, so a tenth's worth of energy arrives whatever the base
       is. What is being asserted is that the BASE did not refuel us -- that
       the pool is still near empty rather than back at ENERGY_START. */
    ok(ship.energy < 200,           "but does not refuel");
    ok(!trek_docked_safe(),         "and its shields do not cover us");

    trek_new_game(3, 4242);
    ship.sec_y = 4; ship.sec_x = 4;
    for (i = 0; i < QUAD_CELLS; i++) if (sector[i] == SEC_BASE) sector[i] = SEC_EMPTY;
    sector[(4 << 3) | 5] = SEC_BASE;
    gal_base[(ship.quad_y << 3) | ship.quad_x] = BASE_RESEARCH;
    ship.energy = 100; ship.torps = 1;
    ok(trek_dock() == DOCK_OK,    "a research station can be docked with");
    ok(ship.torps == 1,           "but restocks nothing this core models");
    ok(ship.energy < 200,         "including fuel");

    /* MEASURED off the original's Docked/Undocked columns: 47 a stardate
       against 20. The ancestor's 4x was DERIVED and is refuted. */
    {
        uint8_t docked_pct, adrift_pct;
        trek_new_game(3, 4242);
        ship.sys[SYS_LASERS] = 0;
        trek_advance(10, ev, 16);
        adrift_pct = ship.sys[SYS_LASERS];

        trek_new_game(3, 4242);
        ship.sec_y = 4; ship.sec_x = 4;
        for (i = 0; i < QUAD_CELLS; i++) if (sector[i] == SEC_BASE) sector[i] = SEC_EMPTY;
        sector[(4 << 3) | 5] = SEC_BASE;
        gal_base[(ship.quad_y << 3) | ship.quad_x] = BASE_STARBASE;
        trek_dock();
        ship.sys[SYS_LASERS] = 0;
        trek_advance(10, ev, 16);
        docked_pct = ship.sys[SYS_LASERS];

        ok(adrift_pct == REPAIR_PER_STARDATE, "adrift, one stardate mends 20");
        /* MEASURED off the original's own STATE OF REPAIR dialog, which prints
           both columns: 47 a stardate docked against 20 adrift, about 2.35x.
           The ancestor's 4x was DERIVED and is refuted. */
        ok(docked_pct == REPAIR_PER_STARDATE_DOCKED,
           "docked, it mends 50 -- the manual's 2.5x, measured on real repair");
    }

    /* All four rows of the manual's repair table (trek.h), taken at half a
       stardate so no row saturates and each lands on its own figure. All four
       are MEASURED off the original on actual repair, not on its estimate
       dialog -- 10, 25, 30, 50 points in half a stardate. */
    {
        uint8_t adrift, docked, focus, both;

        trek_new_game(3, 4242);
        ship.sys[SYS_LASERS] = 0;
        trek_advance(5, ev, 16);
        adrift = ship.sys[SYS_LASERS];

        trek_new_game(3, 4242);
        ship.sys[SYS_LASERS] = 0;
        ship.repair_focus = SYS_LASERS + 1;
        trek_advance(5, ev, 16);
        focus = ship.sys[SYS_LASERS];

        trek_new_game(3, 4242);
        ship.sec_y = 4; ship.sec_x = 4;
        for (i = 0; i < QUAD_CELLS; i++)
            if (sector[i] == SEC_BASE) sector[i] = SEC_EMPTY;
        sector[(4 << 3) | 5] = SEC_BASE;
        gal_base[(ship.quad_y << 3) | ship.quad_x] = BASE_STARBASE;
        trek_dock();
        ship.sys[SYS_LASERS] = 0;
        trek_advance(5, ev, 16);
        docked = ship.sys[SYS_LASERS];

        trek_new_game(3, 4242);
        ship.sec_y = 4; ship.sec_x = 4;
        for (i = 0; i < QUAD_CELLS; i++)
            if (sector[i] == SEC_BASE) sector[i] = SEC_EMPTY;
        sector[(4 << 3) | 5] = SEC_BASE;
        gal_base[(ship.quad_y << 3) | ship.quad_x] = BASE_STARBASE;
        trek_dock();
        ship.sys[SYS_LASERS] = 0;
        ship.repair_focus = SYS_LASERS + 1;
        trek_advance(5, ev, 16);
        both = ship.sys[SYS_LASERS];

        ok(adrift == 10, "half a stardate adrift mends 10");
        ok(docked == 25, "docked, 25 -- the manual's 2.5x");
        ok(focus  == 30, "focused, 30 -- the manual's 3x");
        ok(both   == 50, "focused and docked, 50 -- the manual's 5x");
    }

    /* A focus STARVES the rest. MEASURED: shields sat at 0% for eleven
       consecutive turns while the focused lasers climbed 0 to 100. */
    {
        trek_new_game(3, 4242);
        ship.sys[SYS_LASERS]  = 0;
        ship.sys[SYS_SHIELDS] = 0;
        ship.repair_focus = SYS_LASERS + 1;
        trek_advance(5, ev, 16);
        ok(ship.sys[SYS_LASERS] == 30, "the focused system mends");
        ok(ship.sys[SYS_SHIELDS] == 0,
           "and every other damaged system mends NOTHING while it does");
    }

    /* A StarBase's shields absorb enemy fire outright. */
    trek_new_game(3, 4242);
    ship.sec_y = 4; ship.sec_x = 4;
    for (i = 0; i < QUAD_CELLS; i++) if (sector[i] == SEC_BASE) sector[i] = SEC_EMPTY;
    sector[(4 << 3) | 5] = SEC_BASE;
    gal_base[(ship.quad_y << 3) | ship.quad_x] = BASE_STARBASE;
    wall_in(1, 1);
    sector[(1 << 3) | 1] = SEC_BATTLESHIP;
    enemy_hp[(1 << 3) | 1] = HP_BATTLESHIP;
    trek_dock();
    ship.shields = 0;             /* our own shields are flat */
    ship.energy  = ENERGY_START;
    n = trek_enemy_turn(ev, 16, 1);
    ok(ship.energy == ENERGY_START,
       "docked at a StarBase, enemy fire does not reach the hull");
    for (i = 0; i < n; i++)
        if (ev[i].kind == EV_HIT)
            ok(0, "and nothing is reported as a hull hit");

    /* Movement casts off -- but a refused move does not. */
    trek_new_game(3, 4242);
    ship.sec_y = 4; ship.sec_x = 4;
    for (i = 0; i < QUAD_CELLS; i++) if (sector[i] == SEC_BASE) sector[i] = SEC_EMPTY;
    sector[(4 << 3) | 5] = SEC_BASE;
    gal_base[(ship.quad_y << 3) | ship.quad_x] = BASE_STARBASE;
    trek_dock();
    trek_move_impulse(9, 9);
    ok(ship.docked == BASE_STARBASE, "a refused move does not cast off");
    trek_move_impulse(6, 6);
    ok(ship.docked == BASE_NONE,     "a real move does");
}

/* Reaching a besieged base relieves it -- the deadline is actionable. */
static void test_base_relief(void) {
    TrekEvent ev[16];
    uint8_t q;

    puts("relieving a base");

    trek_new_game(3, 4242);
    trek_unschedule(SCHED_TRACTOR);
    trek_unschedule(SCHED_DEATH_POD);
    trek_schedule(SCHED_BASE_ATTACK, 10);
    trek_advance(20, ev, 16);

    q = base_under_attack;
    ok(q < GAL_CELLS, "a base is besieged");
    if (q < GAL_CELLS) {
        ok(trek_is_scheduled(SCHED_BASE_FALLS), "with its fall scheduled");

        ship.quad_y = (uint8_t)(q >> 3);
        ship.quad_x = (uint8_t)(q & 7);
        trek_enter_quadrant();

        ok(base_under_attack == GAL_CELLS, "arriving lifts the siege");
        ok(!trek_is_scheduled(SCHED_BASE_FALLS), "and cancels the destruction");
        trek_advance(600, ev, 16);
        ok(gal_base[q] != BASE_NONE, "so the base outlives its old deadline");
    }
}

static void test_event_queue(void) {
    TrekEvent ev[16];
    uint8_t n, i, seen;

    puts("scheduled events");

    trek_new_game(3, 4242);
    ok(trek_is_scheduled(SCHED_BASE_ATTACK), "a new game schedules a base attack");
    ok(!trek_is_scheduled(SCHED_BASE_FALLS),
       "but nothing is yet scheduled to destroy one");

    trek_unschedule(SCHED_BASE_ATTACK);
    ok(!trek_is_scheduled(SCHED_BASE_ATTACK), "an event can be cancelled");
    ok(trek_scheduled(SCHED_BASE_ATTACK) == SCHED_NEVER, "and reads as never");

    trek_schedule(SCHED_BASE_ATTACK, 100);
    ok(trek_scheduled(SCHED_BASE_ATTACK) == ship.stardate + 100,
       "scheduling is relative to now");

    /* Nothing fires early. */
    n = trek_advance(50, ev, 16);
    for (i = 0, seen = 0; i < n; i++) if (ev[i].kind == EV_BASE_ATTACKED) seen = 1;
    ok(!seen, "an event does not fire before its date");
    ok(trek_is_scheduled(SCHED_BASE_ATTACK), "and is still pending");

    /* And does when the clock passes it. */
    n = trek_advance(60, ev, 16);
    for (i = 0, seen = 0; i < n; i++) if (ev[i].kind == EV_BASE_ATTACKED) seen = 1;
    ok(seen, "it fires once the clock passes its date");
    ok(!trek_is_scheduled(SCHED_BASE_ATTACK), "and is not left pending");
    ok(base_under_attack < GAL_CELLS, "a base is named as under attack");
    ok(trek_is_scheduled(SCHED_BASE_FALLS),
       "and its destruction is now scheduled -- that is the deadline printed");

    /* The deadline the UI prints must be in the future, or the message is a
       lie the moment it appears. */
    for (i = 0; i < n; i++)
        if (ev[i].kind == EV_BASE_ATTACKED)
            ok(ev[i].amount > ship.stardate, "the deadline is still ahead");

    /* Letting it run destroys the base. */
    {
        uint8_t q = base_under_attack;
        ok(gal_base[q] != BASE_NONE, "the base is there before the deadline");
        n = trek_advance(400, ev, 16);
        for (i = 0, seen = 0; i < n; i++) if (ev[i].kind == EV_BASE_LOST) seen = 1;
        ok(seen, "the base falls when its deadline passes");
        ok(gal_base[q] == BASE_NONE, "and is gone from the chart");
        ok(base_under_attack == GAL_CELLS, "with nothing left under attack");
    }

    /* Relief: reaching the base cancels the attack, which is the whole point
       of printing a deadline. */
    trek_new_game(3, 777);
    trek_schedule(SCHED_BASE_ATTACK, 10);
    trek_advance(20, ev, 16);
    if (base_under_attack < GAL_CELLS) {
        uint8_t q = base_under_attack;
        trek_unschedule(SCHED_BASE_FALLS);
        base_under_attack = GAL_CELLS;
        trek_advance(500, ev, 16);
        ok(gal_base[q] != BASE_NONE, "a relieved base survives its deadline");
    }

    /* Scheduling must never wrap into the past. */
    trek_new_game(3, 4242);
    trek_schedule(SCHED_DEATH_POD, 60000);
    ok(trek_scheduled(SCHED_DEATH_POD) > ship.stardate,
       "a huge offset saturates rather than wrapping into the past");

    /* Two events due in the same window both fire, oldest first. */
    trek_new_game(3, 4242);
    trek_unschedule(SCHED_TRACTOR);
    trek_schedule(SCHED_BASE_ATTACK, 10);
    trek_schedule(SCHED_DEATH_POD, 20);
    n = trek_advance(50, ev, 16);
    {
        int8_t first = -1, second = -1;
        for (i = 0; i < n; i++) {
            if (ev[i].kind == EV_BASE_ATTACKED && first < 0)  first = (int8_t)i;
            if (ev[i].kind == EV_POD_HIT       && second < 0) second = (int8_t)i;
        }
        ok(first >= 0 && second >= 0, "both events in one window fire");
        ok(first < second, "and they are reported in date order");
    }
}

static void test_bearing(void) {
    puts("bearing");

    trek_new_game(3, 31337);
    ship.sec_y = 5; ship.sec_x = 3;   /* 6,4 in the original's 1-based display */

    ok(trek_bearing(4, 4) == 45,  "one up and one right reads 45, as the original does");
    ok(trek_bearing(5, 4) == 0,   "due right is 0");
    ok(trek_bearing(4, 3) == 90,  "straight up is 90");
    ok(trek_bearing(5, 2) == 180, "due left is 180");
    ok(trek_bearing(6, 3) == 270, "straight down is 270");
    ok(trek_bearing(4, 2) == 135, "up and left is 135");
    ok(trek_bearing(6, 4) == 315, "down and right is 315");
    ok(trek_bearing(6, 2) == 225, "down and left is 225");

    /* Off the diagonals, within the half degree the table can promise. */
    {
        uint16_t b = trek_bearing(3, 4);   /* two up, one right -> 63.43 */
        ok(b >= 63 && b <= 64, "two up and one right is about 63");
        b = trek_bearing(4, 5);            /* one up, two right -> 26.57 */
        ok(b >= 26 && b <= 27, "one up and two right is about 27");
    }

    ok(trek_bearing(5, 3) == 0, "the ship's own cell has no bearing");
}

static void test_enemy_movement(void) {
    TrekEvent ev[16];
    uint8_t n, i, moved, cell;

    puts("enemy movement");

    /* A crippled ship invites attack: shields down, banks empty, no
       torpedoes. Those are the three terms enemy_motion subtracts for, so
       this is the unambiguous "advance" case rather than one that depends on
       where the ancestor's thresholds happen to fall. */
    trek_new_game(3, 31337);
    ship.sec_y = 4; ship.sec_x = 4;
    ship.shields_up = 0;
    ship.energy = 0;
    ship.torps  = 0;
    sector[(0 << 3) | 0] = SEC_COMMAND;
    enemy_hp[(0 << 3) | 0] = HP_COMMAND;

    n = trek_enemy_turn(ev, 16, 1);
    ok(sector[(0 << 3) | 0] != SEC_COMMAND, "a strong enemy leaves its cell");

    for (cell = 0, moved = 0; cell < QUAD_CELLS; cell++)
        if (sector[cell] == SEC_COMMAND) moved = cell;
    {
        /* abs_diff is private to trek.c, so spell it out here. */
        uint8_t my = (uint8_t)(moved >> 3), mx = (uint8_t)(moved & 7);
        uint8_t dy = (uint8_t)(my > ship.sec_y ? my - ship.sec_y : ship.sec_y - my);
        uint8_t dx = (uint8_t)(mx > ship.sec_x ? mx - ship.sec_x : ship.sec_x - mx);
        ok(trek_dist(dy, dx) < trek_dist(4, 4), "and it is closer than it was");
    }

    for (i = 0, cell = 0; i < n; i++)
        if (ev[i].kind == EV_ENEMY_MOVED) cell++;
    ok(cell == 1, "the move is reported exactly once");

    /* Boxed in on every side, it stays put and still shoots. */
    trek_new_game(3, 31337);
    ship.sec_y = 4; ship.sec_x = 4;
    wall_in(1, 1);
    sector[(1 << 3) | 1] = SEC_COMMAND;
    enemy_hp[(1 << 3) | 1] = HP_COMMAND;
    n = fire_turn(ev, 16);
    ok(sector[(1 << 3) | 1] == SEC_COMMAND, "a boxed-in enemy holds position");
    ok(fire_amount(ev, n) > 0,              "and fires anyway");

    /* Nothing may land on the ship's own cell. */
    trek_new_game(3, 31337);
    ship.sec_y = 4; ship.sec_x = 4;
    ship.shields_up = 0; ship.energy = 0; ship.torps = 0;
    sector[(4 << 3) | 4] = SEC_SHIP;
    sector[(4 << 3) | 5] = SEC_COMMAND;
    enemy_hp[(4 << 3) | 5] = HP_COMMAND;
    trek_enemy_turn(ev, 16, 1);
    ok(sector[(4 << 3) | 4] == SEC_SHIP, "an enemy never steps onto the ship");
    ok(sector[(4 << 3) | 5] == SEC_COMMAND, "and so stays where it was");

    /* REFUTED against the original, and worth a test so it cannot creep back
       in: firing costs an enemy nothing. */
    trek_new_game(3, 31337);
    ship.sec_y = 4; ship.sec_x = 4;
    wall_in(2, 4);
    sector[(2 << 3) | 4] = SEC_BATTLESHIP;
    enemy_hp[(2 << 3) | 4] = HP_BATTLESHIP;
    for (i = 0; i < 5; i++) trek_enemy_turn(ev, 16, 1);
    ok(enemy_hp[(2 << 3) | 4] == HP_BATTLESHIP,
       "five turns of firing cost the enemy no hit points");
}

static void test_enemy_turn(void) {
    TrekEvent ev[16];
    uint8_t n;
    uint16_t before;

    puts("the enemy turn");

    trek_new_game(3, 31337);
    ship.sec_y = 4; ship.sec_x = 4;
    sector[(2 << 3) | 4] = SEC_BATTLESHIP;
    enemy_hp[(2 << 3) | 4] = HP_BATTLESHIP;

    /* SHIELDS UP FIRST. A new game starts with them DOWN, and MEASURED
       2026-08-24 that means the pool is not touched at all and the whole hit
       reaches main energy -- which is what these two used to assert the
       opposite of, because take_damage() drained the pool either way. */
    before = ship.shields;
    n = fire_turn(ev, 16);
    ok(n >= 1, "an enemy in the quadrant fires");
    ok(ship.shields == before, "shields DOWN: the pool is not touched");
    ok(ship.energy < ENERGY_START, "and the whole hit reaches main energy");

    trek_new_game(3, 31337);
    ship.sec_y = 4; ship.sec_x = 4;
    sector[(2 << 3) | 4] = SEC_BATTLESHIP;
    enemy_hp[(2 << 3) | 4] = HP_BATTLESHIP;
    ship.shields_up = 1;
    before = ship.shields;
    n = fire_turn(ev, 16);
    ok(n >= 1, "an enemy fires again");
    ok(ship.shields < before, "shields UP and full: the pool absorbs it");
    ok(ship.energy == ENERGY_START,
       "and main energy is untouched -- full shields absorb the hit WHOLE");

    /* MEASURED on the original: a ship at 41 of 355 hit points hit for 16
       where undamaged companions hit for 27 and 48.

       The enemy is walled in for this one. Enemies move before they fire now,
       and how far they move depends on their power -- so without the wall a
       wounded ship and a healthy one shoot from different distances and this
       compares two things at once. */
    {
        uint16_t strong, weak;
        uint8_t nf;
        trek_new_game(3, 31337);
        ship.sec_y = 4; ship.sec_x = 4;
        wall_in(2, 4);
        sector[(2 << 3) | 4] = SEC_BATTLESHIP;
        enemy_hp[(2 << 3) | 4] = HP_BATTLESHIP;
        nf = fire_turn(ev, 16);
        strong = fire_amount(ev, nf);
        ok(sector[(2 << 3) | 4] == SEC_BATTLESHIP,
           "a walled-in enemy cannot move");

        trek_new_game(3, 31337);
        ship.sec_y = 4; ship.sec_x = 4;
        wall_in(2, 4);
        sector[(2 << 3) | 4] = SEC_BATTLESHIP;
        enemy_hp[(2 << 3) | 4] = 40;
        nf = fire_turn(ev, 16);
        weak = fire_amount(ev, nf);

        ok(weak < strong, "a wounded enemy hits for less");
    }

    /* The experiment that settled it, run against our own core: the same hit
       points in two different classes must hit for the same amount. On the
       original, forcing a supply ship's hit points to a Commander's made it
       fire like a Commander. */
    {
        uint16_t as_supply, as_command;
        uint8_t nf;
        trek_new_game(3, 31337);
        ship.sec_y = 4; ship.sec_x = 4;
        wall_in(2, 4);
        sector[(2 << 3) | 4]   = SEC_SUPPLY;
        enemy_hp[(2 << 3) | 4] = 400;
        nf = fire_turn(ev, 16);
        as_supply = fire_amount(ev, nf);

        trek_new_game(3, 31337);
        ship.sec_y = 4; ship.sec_x = 4;
        wall_in(2, 4);
        sector[(2 << 3) | 4]   = SEC_COMMAND;
        enemy_hp[(2 << 3) | 4] = 400;
        nf = fire_turn(ev, 16);
        as_command = fire_amount(ev, nf);

        ok(as_supply > 0 && as_supply == as_command,
           "class does not change the damage -- only hit points do");
    }

    /* And they hold fire about half the time. */
    {
        int i, fired = 0;
        trek_new_game(3, 31337);
        ship.sec_y = 4; ship.sec_x = 4;
        wall_in(2, 4);
        sector[(2 << 3) | 4]   = SEC_BATTLESHIP;
        enemy_hp[(2 << 3) | 4] = HP_BATTLESHIP;
        ship.shields_up = 1;
        for (i = 0; i < 200; i++) {
            uint8_t n2;
            /* Keep the ship alive for the count. Topping the POOL up is no
               longer enough on its own: the shields absorb a share now rather
               than the whole hit, so energy drains too and the ship would die
               part way through and stop being shot at. */
            ship.shields = SHIELD_MAX;
            ship.energy  = ENERGY_MAX;
            ship.sys[SYS_SHIELDS] = 100;
            n2 = trek_enemy_turn(ev, 16, 1);
            /* Scan only this turn's events. Passing the array size instead
               would re-read last turn's shot out of the untouched tail. */
            if (fire_amount(ev, n2) > 0) fired++;
        }
        if (fired <= 70 || fired >= 130) printf("    (fired %d of 200)\n", fired);
        ok(fired > 70 && fired < 130,
           "enemies fire on roughly half their turns, not every one");
    }

    /* Movement is triggered by the player attacking, not by the turn.
       MEASURED: enemies moved after every volley in a torpedo session, while
       twenty-six consecutive impulse-move turns produced none at all. */
    {
        int i;
        uint8_t start;
        trek_new_game(3, 31337);
        ship.sec_y = 8; ship.sec_x = 8;
        sector[(1 << 3) | 1] = SEC_COMMAND;
        enemy_hp[(1 << 3) | 1] = HP_COMMAND;
        /* Crippled, so enemy_motion definitely wants to close. Otherwise this
           tests the motion thresholds -- which are known wrong, MEASURED --
           instead of the trigger it is about. */
        ship.energy = 0; ship.torps = 0; ship.shields_up = 0;
        start = sector[(1 << 3) | 1];
        for (i = 0; i < 30; i++) {
            ship.shields = 30000; ship.energy = 0; ship.torps = 0;
            trek_enemy_turn(ev, 16, 0);          /* a turn we did not fire on */
        }
        ok(sector[(1 << 3) | 1] == start,
           "an enemy holds position on a turn the player did not fire");

        for (i = 0; i < 30; i++) {
            ship.shields = 30000; ship.energy = 0; ship.torps = 0;
            trek_enemy_turn(ev, 16, 1);          /* a turn we did */
        }
        ok(sector[(1 << 3) | 1] != start,
           "and manoeuvres once we shoot at it");

        /* And only commanders do. MEASURED: across a long session no other
           class ever changed sector, which is also the ancestor's own gate --
           moveklings() moves ordinary ships only at expert skill. */
        trek_new_game(3, 31337);
        ship.sec_y = 8; ship.sec_x = 8;
        sector[(1 << 3) | 1] = SEC_BATTLESHIP;
        enemy_hp[(1 << 3) | 1] = HP_BATTLESHIP;
        for (i = 0; i < 30; i++) {
            ship.shields = 30000; ship.energy = 0; ship.torps = 0;
            trek_enemy_turn(ev, 16, 1);
        }
        ok(sector[(1 << 3) | 1] == SEC_BATTLESHIP,
           "a battleship never moves, however hard we provoke it");
    }

    /* Shields gone: the rest lands on the main banks. */
    trek_new_game(3, 31337);
    ship.sec_y = 4; ship.sec_x = 4;
    sector[(2 << 3) | 4] = SEC_BATTLESHIP;
    enemy_hp[(2 << 3) | 4] = HP_BATTLESHIP;
    ship.shields = 0;
    before = ship.energy;
    fire_turn(ev, 16);
    ok(ship.energy < before, "with shields down the hit reaches main energy");
}

static void test_torpedo(void) {
    uint8_t r;
    uint16_t dmg;
    puts("torpedoes");

    trek_new_game(3, 777);
    ok(ship.torps == 9, "nine torpedoes at the start");

    /* Close in, where the measured cap binds every time, so the outcome is
       deterministic and the test has no coin to flip. */
    ship.sec_y = 4; ship.sec_x = 4;
    sector[(2 << 3) | 4] = SEC_BATTLESHIP;
    enemy_hp[(2 << 3) | 4] = HP_BATTLESHIP;
    ship.enemies_left = 5;

    r = trek_fire_torpedo(2, 4, &dmg);
    ok(r == TORP_KILL,               "one torpedo destroys a battleship up close");
    ok(dmg == TORP_MAX_DAMAGE,       "for exactly the measured 355");
    ok(ship.torps == 8,              "and costs a torpedo");
    ok(sector[(2 << 3) | 4] == SEC_EMPTY, "the sector is cleared");
    ok(ship.enemies_left == 4,       "the galaxy counter drops");
    ok(ship.killed == 1,             "counted as a standard Mongol");

    /* The change this measurement bought: a Commander walks away from one.
       MEASURED on the original -- the cap is a constant, not the target's own
       strength, so 695 - 355 = 340. */
    trek_new_game(3, 777);
    ship.sec_y = 4; ship.sec_x = 4;
    sector[(2 << 3) | 4] = SEC_COMMAND;
    enemy_hp[(2 << 3) | 4] = HP_COMMAND;
    r = trek_fire_torpedo(2, 4, &dmg);
    ok(r == TORP_OK,                 "a Commander SURVIVES a torpedo");
    ok(dmg == TORP_MAX_DAMAGE,       "taking the same capped 355");
    ok(enemy_hp[(2 << 3) | 4] == HP_COMMAND - TORP_MAX_DAMAGE,
       "and is left with 340");
    ok(sector[(2 << 3) | 4] == SEC_COMMAND, "still there, still a Commander");
    ok(ship.killed_cmd == 0,         "and is not counted as a kill");

    /* Accuracy: certain inside the sure range, fallible past it. */
    {
        int i, hits = 0, far_hits = 0;
        trek_new_game(3, 777);
        ship.sec_y = 1; ship.sec_x = 1;
        for (i = 0; i < 60; i++) {
            sector[(4 << 3) | 4] = SEC_COMMAND;      /* distance 4.24 */
            enemy_hp[(4 << 3) | 4] = 60000;
            ship.torps = 9;
            if (trek_fire_torpedo(4, 4, &dmg) != TORP_MISS) hits++;

            /* (7,7) not (8,8): sector indices are 0..7, and 8 returns
               TORP_BAD_COORDS, which this loop first miscounted as a hit. */
            sector[(7 << 3) | 7] = SEC_COMMAND;      /* distance 8.49 */
            enemy_hp[(7 << 3) | 7] = 60000;
            ship.torps = 9;
            if (trek_fire_torpedo(7, 7, &dmg) != TORP_MISS) far_hits++;
        }
        ok(hits == 60,      "inside the sure range a torpedo cannot miss");
        ok(far_hits < 50,   "at long range it can");
        ok(far_hits > 10,   "but not always");
    }

    trek_new_game(3, 777);
    ship.sec_y = 4; ship.sec_x = 4;
    r = trek_fire_torpedo(7, 7, &dmg);
    ok(r == TORP_MISS,   "firing at empty space misses");
    ok(dmg == 0,         "and reports no damage");

    ship.torps = 0;
    ok(trek_fire_torpedo(1, 1, &dmg) == TORP_NONE_LEFT, "cannot fire without any");
}

static void test_self_destruct(void) {
    uint8_t n;
    puts("self destruct");

    /* The bug this pair exists to prevent: killing with LASERS never counted
       toward the score. Both weapons share one kill routine now. */
    trek_new_game(3, 777);
    ship.sec_y = 4; ship.sec_x = 4;
    sector[(2 << 3) | 4] = SEC_BATTLESHIP;
    enemy_hp[(2 << 3) | 4] = 1;
    ok(trek_fire_laser(2, 4, 500, NULL) == FIRE_KILL, "a laser can kill");
    ok(ship.killed == 1, "and the kill is counted -- it never used to be");

    trek_new_game(3, 777);
    ship.sec_y = 4; ship.sec_x = 4;
    sector[(2 << 3) | 4] = SEC_COMMAND;
    enemy_hp[(2 << 3) | 4] = 1;
    trek_fire_laser(2, 4, 500, NULL);
    ok(ship.killed_cmd == 1, "a Commander counts against the Commander line");

    /* A full tank takes the quadrant with it. */
    trek_new_game(3, 777);
    ship.sec_y = 4; ship.sec_x = 4;
    sector[(1 << 3) | 1] = SEC_COMMAND;   enemy_hp[(1 << 3) | 1] = HP_COMMAND;
    sector[(6 << 3) | 6] = SEC_BATTLESHIP; enemy_hp[(6 << 3) | 6] = HP_BATTLESHIP;
    ship.energy = ENERGY_START;
    /* MEASURED 2026-08-24: it kills NOTHING, at any energy. The ancestor's
       kaboom() took the quadrant with a full tank; EGA Trek dropped the rule,
       and run 5 proved it -- four enemies, nearest at range 1.41, zero
       killed. These two cases used to assert 2 and 0 and were the ancestor
       being tested, not the original. */
    n = trek_self_destruct();
    ok(n == 0,              "it destroys nothing, even at full energy");
    ok(ship.lost,           "but the ship is lost");
    ok(ship.casualties == CREW_COMPLEMENT, "all hands, as with any loss");
    ok(trek_game_state() == GAME_LOST, "the mission is over");

    trek_new_game(3, 777);
    ship.sec_y = 4; ship.sec_x = 4;
    sector[(1 << 3) | 1] = SEC_COMMAND; enemy_hp[(1 << 3) | 1] = HP_COMMAND;
    ship.energy = 0;
    n = trek_self_destruct();
    ok(n == 0,  "and nothing with the banks empty either");
    ok(ship.lost, "the ship is still lost");
}

static void test_game_state_and_score(void) {
    puts("mission state and score");

    trek_new_game(3, 4242);
    ok(trek_game_state() == GAME_ON, "a fresh game is in progress");

    ship.enemies_left = 0;
    ok(trek_game_state() == GAME_WON, "no Mongols left is a win");

    trek_new_game(3, 4242);
    ship.lost = 1;
    ok(trek_game_state() == GAME_LOST, "a destroyed ship is a loss");

    /* Weights read off the original's own evaluation screen. An unfinished
       mission, so the rate term is gated off and only the flat items count --
       which is exactly the sheet MEASURED.md records. */
    trek_new_game(3, 4242);
    ship.enemies_left = 5;
    ship.killed = 1; ship.killed_cmd = 1; ship.casualties = 18;
    ok(trek_score() == 10 + 20 - 18 - 300,
       "the recorded sheet reproduces exactly: 1 Mongol, 1 Commander, 18 casualties, unfinished");
    ok(trek_score() == -288, "and its printed total, -288");

    /* Nothing achieved, mission unfinished, ship lost with all hands.
       MEASURED 2026-08-24 off a combat-death sheet: -200 for the ship, -300
       for the unfinished mission and -430 for the crew, printing -930. An
       earlier reading of this same case recorded -730 and had the ship-loss
       constant deleted on the strength of it; the line is on the sheet. */
    trek_new_game(3, 4242);
    ship.enemies_left = 34;
    ship.casualties = CREW_COMPLEMENT;
    ship.lost = 1;
    ok(trek_score() == -200 - 300 - 430, "the ship, the mission and the crew");
    ok(trek_score() == -930, "and its printed total, -930");

    /* The sheet, line by line, against the same measured game. Every row the
       original printed is checked, including the three that are zero because
       we have no mechanism for them yet -- if one ever becomes non-zero by
       accident this catches it. */
    {
        ScoreSheet sh;
        trek_score_sheet(&sh);
        ok(sh.casualties == CREW_COMPLEMENT,  "sheet: 430 casualties");
        ok(sh.casualty_pts == -430,           "sheet: costing 430");
        ok(sh.incomplete_pts == -300,         "sheet: the incomplete penalty");
        ok(sh.rate_pts == 0,                  "sheet: no rate credit unfinished");
        ok(sh.mongol_pts == 0 && sh.commander_pts == 0, "sheet: nothing killed");
        ok(sh.rescues == 0 && sh.enemy_bases == 0 && sh.stars == 0,
           "sheet: the three unimplemented items are zero");
        ok(sh.ship_lost_pts == -200,          "sheet: the ship-loss penalty");
        ok(sh.total == -930,                  "sheet: total matches the original");
        ok(sh.total == sh.ship_lost_pts + sh.rescue_pts + sh.incomplete_pts
                     + sh.mongol_pts
                     + sh.commander_pts + sh.enemy_base_pts + sh.rate_pts
                     + sh.casualty_pts + sh.star_pts + sh.bases_hit_pts,
           "sheet: the printed lines add up to the printed total");
        ok(sh.total == trek_score(), "sheet: and to what the score records");
    }

    /* Quitting with nothing done is the flat penalty alone. */
    trek_new_game(3, 4242);
    ship.enemies_left = 5;
    ok(trek_score() == -300, "nothing done, mission unfinished, is -300");

    /* Bases lost to a siege are scored against us. */
    trek_new_game(3, 4242);
    ship.enemies_left = 5;
    bases_lost = 2;
    ok(trek_score() == -300 + 2 * SCORE_BASE_LOST, "each base lost costs 200");

    /* The rate term. Gated on a finished mission -- the whole point of open
       item 13, where the original printed 0.00 with kills on the board. */
    trek_new_game(3, 4242);
    ship.enemies_left = 3;
    ship.killed = 2;
    ship.stardate = (uint16_t)(STARDATE_START + 12);   /* 1.2 stardates */
    ok(trek_score() == 2 * 10 - 300,
       "unfinished: the rate term contributes nothing, as the original printed");

    trek_new_game(3, 4242);
    ship.enemies_left = 0;
    ship.killed = 2;
    ship.stardate = (uint16_t)(STARDATE_START + 100);  /* 10 stardates */
    ok(trek_score() == 2 * 10 + 100,
       "finished: 2 kills in 10 stardates is 0.2/day, worth 100");

    /* The five-stardate floor, which stops an instant win paying out
       absurdly -- and stops the arithmetic leaving 16 bits. */
    trek_new_game(3, 4242);
    ship.enemies_left = 0;
    ship.killed = 2;
    ship.stardate = STARDATE_START;                    /* no time at all */
    ok(trek_score() == 2 * 10 + 200,
       "no elapsed time is floored at five stardates, not divided by zero");

    trek_new_game(3, 4242);
    ship.enemies_left = 0;
    ship.killed = 2;
    ship.stardate = (uint16_t)(STARDATE_START + 10);   /* 1 stardate */
    ok(trek_score() == 2 * 10 + 200, "and so is anything under five");

    /* A fractional stardate must not be dropped: 4 kills in 7.5 days is
       0.533/day = 266, where truncating to 7 days would say 285 and
       truncating the rate to 0 would say nothing at all. */
    trek_new_game(3, 4242);
    ship.enemies_left = 0;
    ship.killed = 4;
    ship.stardate = (uint16_t)(STARDATE_START + 75);
    {
        int16_t sc = (int16_t)(trek_score() - 4 * 10);
        ok(sc >= 265 && sc <= 267, "the fractional part of a stardate counts");
    }

    /* Nothing overflows at the top end. */
    trek_new_game(3, 4242);
    ship.enemies_left = 0;
    ship.killed = 60; ship.killed_cmd = 4;
    ship.stardate = (uint16_t)(STARDATE_START + 50);
    ok(trek_score() > 0, "a maximal game still scores positive, not wrapped");
}

static void test_star_and_focus(void) {
    /* --- a torpedo aimed at a star is absorbed, not a miss (2026-08-23) */
    {
        uint16_t dmg = 99;
        uint8_t r;
        trek_new_game(3, 1234);
        /* put a star where we can shoot it, and make sure we have a torpedo */
        sector[(3 << 3) | 4] = SEC_STAR;
        ship.torps = 5;
        r = trek_fire_torpedo(3, 4, &dmg);
        ok(r == TORP_ABSORBED, "a torpedo aimed at a star is absorbed");
        ok(dmg == 0, "an absorbed torpedo does no damage");
        ok(ship.torps == 4, "and it is still spent");
        ok(sector[(3 << 3) | 4] == SEC_STAR, "the star survives");
    }

    /* --- F)ix concentrates repairs on one system */
    {
        uint8_t plain, focused;
        trek_new_game(3, 1234);
        ship.sys[SYS_LASERS] = 50;
        ship.sys[SYS_SHIELDS] = 50;
        ship.repair_focus = 0;
        { TrekEvent ev[8]; (void)trek_advance(10, ev, 8); }
        plain = ship.sys[SYS_LASERS];
        ok(ship.sys[SYS_SHIELDS] == plain,
              "with no focus every damaged system mends at the same rate");

        trek_new_game(3, 1234);
        ship.sys[SYS_LASERS] = 50;
        ship.sys[SYS_SHIELDS] = 50;
        ship.repair_focus = SYS_LASERS + 1;
        { TrekEvent ev[8]; (void)trek_advance(10, ev, 8); }
        focused = ship.sys[SYS_LASERS];
        ok(focused > ship.sys[SYS_SHIELDS],
              "the focused system mends faster than the rest");
        ok(ship.sys[SYS_SHIELDS] == 50,
              "and the others do not mend at all -- MEASURED, see trek.h");
        (void)plain;
    }
}

/* Damage has consequences. Every threshold here is the manual's, quoted in
 * trek.h beside the function it produced. Before 2026-08-24 this port
 * modelled all twelve repair percentages and consulted exactly one of them,
 * so a ship at 10% on everything played identically to a ship at 100%. */
static void test_damage_effects(void) {
    printf("damage has consequences\n");

    trek_new_game(3, 42);

    /* Warp: 1 + 0.09 * pct, clamped to the emergency ceiling. */
    ship.sys[SYS_WARP] = 100;
    ok(trek_max_warp() == WARP_MAX, "undamaged engines reach the warp ceiling");
    ship.sys[SYS_WARP] = 50;
    ok(trek_max_warp() == 55, "half-repaired engines cap at warp 5.5");
    ship.sys[SYS_WARP] = 0;
    ok(trek_max_warp() == WARP_MIN, "wrecked engines still manage warp 1");

    ship.sys[SYS_WARP] = 50;
    ok(trek_set_warp(80) == 0, "warp 8 is refused with damaged engines");
    ok(trek_set_warp(50) != 0, "warp 5 is accepted");
    ship.sys[SYS_WARP] = 100;

    /* Impulse: a cliff at 50%, not a slope. */
    ship.sys[SYS_IMPULSE] = 50;
    ok(trek_impulse_ok(), "impulse works at exactly 50%");
    ship.sys[SYS_IMPULSE] = 49;
    ok(!trek_impulse_ok(), "and stops at 49%");
    ok(trek_move_impulse(1, 1) == MOVE_NO_IMPULSE,
       "an impulse move is refused outright, not slowed");
    ship.sys[SYS_IMPULSE] = 100;

    /* Torpedo tubes: 100 / 67-99 / 34-66 / below. */
    ship.sys[SYS_TUBES] = 100; ok(trek_tubes_available() == 3, "100% = three tubes");
    ship.sys[SYS_TUBES] =  99; ok(trek_tubes_available() == 2, "99% = two tubes");
    ship.sys[SYS_TUBES] =  67; ok(trek_tubes_available() == 2, "67% = two tubes");
    ship.sys[SYS_TUBES] =  66; ok(trek_tubes_available() == 1, "66% = one tube");
    ship.sys[SYS_TUBES] =  34; ok(trek_tubes_available() == 1, "34% = one tube");
    ship.sys[SYS_TUBES] =  33; ok(trek_tubes_available() == 0, "33% = no tubes");
    ship.sys[SYS_TUBES] = 100;

    /* Scanners degrade in steps, and the two have DIFFERENT thresholds --
       short range keeps working to 90%, long range only at a full 100%. */
    ship.sys[SYS_SRSCAN] =  90; ok(trek_srscan_level() == SCAN_FULL,   "SR full at 90%");
    ship.sys[SYS_SRSCAN] =  89; ok(trek_srscan_level() == SCAN_COARSE, "SR coarse at 89%");
    ship.sys[SYS_SRSCAN] =  50; ok(trek_srscan_level() == SCAN_COARSE, "SR coarse at 50%");
    ship.sys[SYS_SRSCAN] =  49; ok(trek_srscan_level() == SCAN_DEAD,   "SR dead at 49%");
    ship.sys[SYS_LRSCAN] = 100; ok(trek_lrscan_level() == SCAN_FULL,   "LR full only at 100%");
    ship.sys[SYS_LRSCAN] =  99; ok(trek_lrscan_level() == SCAN_COARSE, "LR coarse at 99%");
    ship.sys[SYS_LRSCAN] =  49; ok(trek_lrscan_level() == SCAN_DEAD,   "LR dead at 49%");
    ship.sys[SYS_SRSCAN] = 100; ship.sys[SYS_LRSCAN] = 100;

    /* Three systems that need a perfect 100%. */
    ship.sys[SYS_COMPUTER] = 99;
    ok(!trek_autonav_ok(), "automatic navigation needs a full 100%");
    ship.sys[SYS_COMPUTER] = 100;
    ok(trek_autonav_ok(), "and works at 100%");
    ship.sys[SYS_TRANSPORTER] = 99; ok(!trek_transporter_ok(), "transporter needs 100%");
    ship.sys[SYS_SHUTTLE]     = 99; ok(!trek_shuttle_ok(),     "shuttlecraft needs 100%");
    ship.sys[SYS_TRANSPORTER] = 100; ship.sys[SYS_SHUTTLE] = 100;

    /* Lasers: the manual's own example -- half-repaired lasers do half the
       damage of fully repaired ones at the same energy and range. */
    {
        uint16_t full, half;
        ship.laser_eff = 100;
        ship.sys[SYS_LASERS] = 100;
        full = trek_laser_damage(500, trek_laser_eff(), 100);
        ship.sys[SYS_LASERS] = 50;
        half = trek_laser_damage(500, trek_laser_eff(), 100);
        ok(full > 0 && half > 0, "both volleys do damage");
        ok(full / 2 == half || full / 2 == half + 1 || full / 2 + 1 == half,
           "50% lasers do half the damage of 100% lasers");
        ship.sys[SYS_LASERS] = 100;
    }
}

/* Movement now WALKS THE SECTOR MAP, so a test that teleports the ship by
   assigning ship.quad_* and ship.sec_* has to make the map agree -- otherwise
   the walk starts from a cell the map says is empty and runs into whatever
   the previous quadrant left lying around. Clearing the quadrant is also what
   makes the arithmetic below the subject of the test rather than the galaxy
   the seed happened to generate. */
static void place_ship(uint8_t qy, uint8_t qx, uint8_t sy, uint8_t sx) {
    int i;
    ship.quad_y = qy; ship.quad_x = qx;
    ship.sec_y  = sy; ship.sec_x  = sx;
    for (i = 0; i < QUAD_CELLS; i++) sector[i] = SEC_EMPTY;
    sector[(sy << 3) | sx] = SEC_SHIP;
}

/* THE MEASURED MOVEMENT MODEL, replayed sample by sample.
 *
 * Every case below is a reading taken off the original in DOSBox on
 * 2026-08-25 -- see MEASURED.md, "The movement model, entire". They are
 * written in the core's 0-based coordinates; the original's 1-based figures
 * are in the comments so the two can be checked against each other.
 *
 * Two of them are DISCRIMINATORS rather than confirmations: case B separates
 * round-half-up from round-half-down, and cases B and D separate a clock
 * billed on the truncated endpoint from one billed on the rounded cell. If
 * someone "simplifies" path_round or path_floor, those are what notice.
 */

/* Hundredths of a stardate elapsed since the game began. The clock itself is
   carried in tenths; movement is finer than that, so the sub-tenth remainder
   lives in ship.time_frac and both halves have to be read to see a move. */
static uint16_t elapsed_hundredths(void) {
    return (uint16_t)((uint16_t)(ship.stardate - STARDATE_START) * 10u
                      + ship.time_frac);
}

static void test_blocked_movement(void) {
    uint8_t r;
    uint16_t t0;

    puts("movement blocks on a straight line (MEASURED)");

    /* A: 1-based 1-3 to 1-8 with a star at 1-6. Straight along a row, so no
       rounding is involved: this one only pins "stops in the last clear
       cell", and that the message names the cell one step further on. */
    trek_new_game(3, 11);
    place_ship(3, 3, 0, 2);
    sector[(0 << 3) | 5] = SEC_STAR;
    t0 = elapsed_hundredths();
    r = trek_move_impulse(0, 7);
    ok(r == MOVE_BLOCKED, "A: a move across a star is blocked");
    ok(ship.sec_y == 0 && ship.sec_x == 4, "A: ship stops in the last clear cell");
    ok(trek_block_y == 0 && trek_block_x == 5, "A: the star's cell is reported");
    ok(sector[(0 << 3) | 4] == SEC_SHIP, "A: the map shows the ship where it stopped");
    ok(elapsed_hundredths() - t0 == 8,
       "A: billed 2 sectors, 0.08 stardates (original: 0.0833)");

    /* B: 1-based 5-2 to 7-6, stars at 6-5 and 7-6. THE ROUNDING
       DISCRIMINATOR. Step 3 lands on exactly (5.5, 4.0) in 0-based terms:
       half-up puts it at 6-4 and the block at 6-5, half-down puts the step
       itself on the star at 5-4. The original blocked at 1-based 7-6, which
       is 0-based 6-5, so it is half-up. */
    trek_new_game(3, 12);
    place_ship(3, 3, 4, 1);
    sector[(5 << 3) | 4] = SEC_STAR;
    sector[(6 << 3) | 5] = SEC_STAR;
    t0 = elapsed_hundredths();
    r = trek_move_impulse(6, 5);
    ok(r == MOVE_BLOCKED, "B: blocked");
    ok(ship.sec_y == 6 && ship.sec_x == 4,
       "B: halves round AWAY FROM ZERO -- the path passes 6-4, not 5-4");
    ok(trek_block_y == 6 && trek_block_x == 5, "B: blocked by the far star");
    /* Billed on the TRUNCATED endpoint (1,3) -> sqrt(10), not the rounded
       cell (2,3) -> sqrt(13). 13 hundredths against the rounded model's 14,
       and the original charged 0.1317. */
    ok(elapsed_hundredths() - t0 == 13,
       "B: billed on the truncated endpoint, 0.13 (original: 0.1317)");

    /* C: 1-based 8-3 to 4-7 with a star at 6-5. A diagonal blocked on its
       second step, both coordinates whole, so trunc and round agree. */
    trek_new_game(3, 13);
    place_ship(3, 3, 7, 2);
    sector[(5 << 3) | 4] = SEC_STAR;
    t0 = elapsed_hundredths();
    r = trek_move_impulse(3, 6);
    ok(r == MOVE_BLOCKED, "C: blocked");
    ok(ship.sec_y == 6 && ship.sec_x == 3, "C: stopped after one diagonal step");
    ok(trek_block_y == 5 && trek_block_x == 4, "C: the star's cell is reported");
    ok(elapsed_hundredths() - t0 == 5,
       "C: billed sqrt(2), 0.05 (original: 0.0589)");

    /* D: 1-based 8-1 to 1-6 with a star at 2-5. THE SECOND TRUNCATION
       DISCRIMINATOR, and the one that was designed as a three-way test
       before the answer was known: step 5 sits at (2, 3.571) in 0-based
       terms, so trunc gives sqrt(34), round gives sqrt(41) and the real
       position gives sqrt(37.75). The original billed 5.8320. */
    trek_new_game(3, 14);
    place_ship(3, 3, 7, 0);
    sector[(1 << 3) | 4] = SEC_STAR;
    t0 = elapsed_hundredths();
    r = trek_move_impulse(0, 5);
    ok(r == MOVE_BLOCKED, "D: blocked");
    ok(ship.sec_y == 2 && ship.sec_x == 4, "D: stopped at 0-based 2-4");
    ok(trek_block_y == 1 && trek_block_x == 4, "D: the star's cell is reported");
    ok(elapsed_hundredths() - t0 == 24,
       "D: billed sqrt(34), 0.24 (original: 0.2430; rounded would be 0.26)");

    /* A clear move is charged the whole distance, and the clock carries the
       sub-tenth remainder rather than dropping it. Five sectors is 0.2084 in
       the original; the port bills 20 hundredths, which is two tenths and a
       remainder that the NEXT move gets to keep. */
    trek_new_game(3, 15);
    place_ship(3, 3, 0, 4);
    t0 = elapsed_hundredths();
    r = trek_move_impulse(4, 1);
    ok(r == MOVE_OK, "clear diagonal succeeds");
    ok(ship.sec_y == 4 && ship.sec_x == 1, "and arrives where it was sent");
    ok(elapsed_hundredths() - t0 == 20,
       "billed 5 sectors, 0.20 (original: 0.2084)");

    /* Impulse time does not depend on the warp factor -- measured at warp 1.0
       and warp 3.0 on the same four-sector hop. */
    {
        uint16_t at_warp_1, at_warp_3;
        trek_new_game(3, 16);
        place_ship(3, 3, 6, 4);
        ship.warp = WARP_MIN;
        t0 = elapsed_hundredths();
        trek_move_impulse(6, 0);
        at_warp_1 = (uint16_t)(elapsed_hundredths() - t0);

        trek_new_game(3, 16);
        place_ship(3, 3, 6, 4);
        ship.warp = 30;
        t0 = elapsed_hundredths();
        trek_move_impulse(6, 0);
        at_warp_3 = (uint16_t)(elapsed_hundredths() - t0);
        ok(at_warp_1 == at_warp_3 && at_warp_1 == 16,
           "impulse time ignores the warp factor (original: 0.1667 and 0.1666)");
    }
}

/* The warp clock, and the departure path.
 *
 * MEASURED: 11 * distance_in_quadrants / warp^2, with the distance taken over
 * ABSOLUTE sector positions. Both facts are checked here because both were
 * wrong: the port used ~9.98 and measured the distance between quadrant
 * INDICES. */
static void test_warp_timing(void) {
    uint16_t t0;
    uint8_t r;

    puts("warp timing (MEASURED)");

    trek_new_game(3, 21);
    place_ship(3, 3, 4, 4);
    ship.energy = ENERGY_MAX;
    ship.warp = WARP_MIN;                      /* warp 1.0 */
    t0 = elapsed_hundredths();
    r = trek_move_warp(3, 4, 4, 4);            /* one quadrant east, same sector */
    ok(r == MOVE_OK, "a clear one-quadrant jump succeeds");
    ok(elapsed_hundredths() - t0 == 1100,
       "one quadrant at warp 1.0 costs 11.00 stardates");

    /* Quartering with warp squared: the same jump at warp 2.0 is a quarter of
       the time. Run 1 read four quadrants at warp 2 as 11.0, which is this. */
    trek_new_game(3, 21);
    place_ship(3, 3, 4, 4);
    ship.energy = ENERGY_MAX;
    ship.warp = 20;
    t0 = elapsed_hundredths();
    trek_move_warp(3, 4, 4, 4);
    ok(elapsed_hundredths() - t0 == 275,
       "and 2.75 at warp 2.0 -- time goes as 1/warp^2");

    /* The distance is ABSOLUTE, not a quadrant count. Jumping one quadrant
       east but seven sectors back lands 1 sector away, not 8, so it costs an
       eighth of the time. A model built on quadrant indices cannot tell these
       two jumps apart -- and could not produce run 1's 17-sector reading. */
    trek_new_game(3, 21);
    place_ship(3, 3, 4, 7);
    ship.energy = ENERGY_MAX;
    ship.warp = WARP_MIN;
    t0 = elapsed_hundredths();
    trek_move_warp(3, 4, 4, 0);
    ok(elapsed_hundredths() - t0 == 137,
       "distance is over absolute sectors, not quadrant indices");

    /* MEASURED: the departure path is walked through the quadrant being LEFT,
       the ship stays in it, and it is billed for the distance it covered --
       four sectors at warp 1.0 cost 5.5000, which is 11 * 0.5 and not 11. */
    trek_new_game(3, 22);
    place_ship(3, 3, 6, 0);
    sector[(6 << 3) | 5] = SEC_STAR;
    ship.energy = ENERGY_MAX;
    ship.warp = WARP_MIN;
    t0 = elapsed_hundredths();
    r = trek_move_warp(3, 4, 6, 0);
    ok(r == MOVE_BLOCKED, "a quadrant change is blocked by the quadrant it leaves");
    ok(ship.quad_y == 3 && ship.quad_x == 3, "and the ship stays in that quadrant");
    ok(ship.sec_y == 6 && ship.sec_x == 4, "in the last clear sector");
    ok(trek_block_y == 6 && trek_block_x == 5, "with the blocking cell reported");
    ok(elapsed_hundredths() - t0 == 550,
       "billed half a quadrant, 5.50 (original: 5.5000)");

    /* Blocked on the very first step: nothing moves and nothing is charged. */
    trek_new_game(3, 23);
    place_ship(3, 3, 1, 3);
    sector[(1 << 3) | 4] = SEC_STAR;
    ship.energy = ENERGY_MAX;
    ship.warp = WARP_MIN;
    t0 = elapsed_hundredths();
    r = trek_move_warp(3, 4, 1, 3);
    ok(r == MOVE_BLOCKED, "blocked on the first step");
    ok(ship.sec_y == 1 && ship.sec_x == 3, "the ship has not moved");
    ok(elapsed_hundredths() == t0, "and the clock has not moved either");
}

/* Manual movement, the manual's own worked example.
 *
 * "if you want to move one quadrant down and two quadrants plus two sectors
 * left (i.e., from 1,8,1,8 to 2,6,1,6) DeltaX would be 1.0 and DeltaY would
 * be -2.2" -- l.522-526. Those are the original's 1-based coordinates; the
 * core is 0-based, so 1,8,1,8 is quad (0,7) sector (0,7).
 *
 * The offsets arrive in SECTORS: 1.0 is +8, -2.2 is -(2*8+2) = -18. */
/* THE SHIELD LAW, and the system damage that comes with it.
 *
 * All MEASURED 2026-08-24 (run 3) -- see the block above SHIELD_SYS_HIT_MIN
 * in trek.h. take_damage() is reached through the enemy turn, so these drive
 * it directly through the same path the events use.
 */
static void test_shield_absorption(void) {
    TrekEvent ev[16];
    uint8_t n;

    puts("shields absorb a share (MEASURED)");

    /* Shields DOWN: the pool is untouched and the whole hit reaches energy.
       This port used to drain the pool whether they were up or down. */
    trek_new_game(3, 900);
    ship.shields_up = 0;
    ship.shields = SHIELD_MAX;
    ship.energy  = ENERGY_MAX;
    n = 0; trek_take_hit(400, ev, &n, 16);
    ok(ship.shields == SHIELD_MAX,          "down: the pool is not touched");
    ok(ship.energy == ENERGY_MAX - 400,     "down: the whole hit reaches energy");

    /* Shields UP and FULL with an undamaged shield system: absorbed WHOLE,
       energy untouched. Six hits of 385 to 518 did exactly this. */
    trek_new_game(3, 901);
    ship.shields_up = 1;
    ship.shields = SHIELD_MAX;
    ship.energy  = ENERGY_MAX;
    ship.sys[SYS_SHIELDS] = 100;
    n = 0; trek_take_hit(409, ev, &n, 16);
    ok(ship.shields == SHIELD_MAX - 409,    "up and full: the pool takes it all");
    ok(ship.energy == ENERGY_MAX,           "up and full: energy is untouched");

    /* Shields UP but nearly flat: a small PROPORTIONAL share, the rest
       through. At 200 of 2500 the charge term is 8%, and the original's five
       hits sat at 6.5% -- which the shield system term accounts for. The
       assertion is the SHAPE: a small share absorbed, most of it through, and
       the absorbed amount scaling with the hit rather than being fixed. */
    {
        uint16_t a1, a2;
        trek_new_game(3, 902);
        ship.shields_up = 1;
        ship.sys[SYS_SHIELDS] = 100;

        ship.shields = 200; ship.energy = ENERGY_MAX;
        n = 0; trek_take_hit(500, ev, &n, 16);
        a1 = (uint16_t)(200 - ship.shields);

        ship.shields = 200; ship.energy = ENERGY_MAX;
        n = 0; trek_take_hit(1000, ev, &n, 16);
        a2 = (uint16_t)(200 - ship.shields);

        ok(a1 > 0 && a1 < 100, "flat: a small share of a 500 hit is absorbed");
        ok(a2 > a1, "flat: absorbed scales with the hit, it is not a fixed bite");
        ok(a2 >= (uint16_t)(2 * a1 - 2) && a2 <= (uint16_t)(2 * a1 + 2),
           "flat: and scales LINEARLY -- double the hit, double the absorbed");
    }

    /* The shield SYSTEM degrades the absorption, which is what makes the two
       clean readings agree. Half a shield system absorbs half as much. */
    {
        uint16_t full, half;
        trek_new_game(3, 903);
        ship.shields_up = 1;
        ship.shields = SHIELD_MAX / 2; ship.energy = ENERGY_MAX;
        ship.sys[SYS_SHIELDS] = 100;
        n = 0; trek_take_hit(400, ev, &n, 16);
        full = (uint16_t)(SHIELD_MAX / 2 - ship.shields);

        ship.shields = SHIELD_MAX / 2; ship.energy = ENERGY_MAX;
        ship.sys[SYS_SHIELDS] = 50;
        n = 0; trek_take_hit(400, ev, &n, 16);
        half = (uint16_t)(SHIELD_MAX / 2 - ship.shields);

        ok(half * 2 >= full - 2 && half * 2 <= full + 2,
           "a shield system at 50% absorbs half as much");
    }

    /* The pool can never go negative, and never absorbs more than it holds. */
    trek_new_game(3, 904);
    ship.shields_up = 1;
    ship.shields = 50; ship.energy = ENERGY_MAX;
    ship.sys[SYS_SHIELDS] = 100;
    n = 0; trek_take_hit(5000, ev, &n, 16);
    ok(ship.shields == 0, "a hit bigger than the pool empties it");
    ok(ship.energy < ENERGY_MAX, "and the rest gets through");
}

/* MEASURED: a penetrating hit usually ANNIHILATES a system rather than
   denting it -- zero eight times in eleven -- and two can go in one turn.
   Statistical, so these run the turn many times and check the shape. */
static void test_system_damage_severity(void) {
    TrekEvent ev[16];
    uint8_t n, i, j;
    int wrecked = 0, dented = 0, turns_with_two = 0, turns_damaged = 0;

    puts("a system hit is usually annihilation (MEASURED)");

    /* ONE game and one RNG stream, sampled 400 times. An earlier version
       called trek_new_game() inside the loop with sequential seeds and read
       the first few draws of each -- which correlates, and read a wreck share
       of 0.646 against the 0.727 the generator actually produces. The
       generator was checked and is uniform to within a rounding error for
       every n from 2 to 12; the test was what was biased. */
    trek_new_game(3, 4242);
    ship.shields_up = 0;              /* let it all through */
    for (i = 0; i < 200; i++) {
        uint8_t hits = 0;
        for (j = 0; j < SYS_COUNT; j++) ship.sys[j] = 100;
        ship.energy = ENERGY_MAX;
        ship.lost = 0;
        n = 0;
        trek_take_hit(600, ev, &n, 16);
        for (j = 0; j < n; j++)
            if (ev[j].kind == EV_SYSTEM_HIT) {
                hits++;
                if (ev[j].amount == 0) wrecked++; else dented++;
            }
        if (hits) turns_damaged++;
        if (hits >= 2) turns_with_two++;
    }

    /* Roughly three turns in five do any damage at all. */
    ok(turns_damaged > 200 * 2 / 5 && turns_damaged < 200 * 4 / 5,
       "a penetrating hit damages something about three times in five");
    /* And when it does it is usually a wreck: MEASURED 8 of 11, which is
       0.727, so the assertion is a clear majority rather than that figure to
       the decimal -- eleven observations do not pin it that finely. */
    ok(wrecked * 3 > dented * 5,
       "the result is 0% far more often than not (measured 8 in 11)");
    ok(turns_with_two > 0 && turns_with_two < turns_damaged / 2,
       "two systems go in one turn sometimes, and not usually");
}

static void test_manual_movement(void) {
    printf("manual movement (DeltaX/DeltaY)\n");

    trek_new_game(3, 7);
    place_ship(0, 7, 0, 7);
    ship.energy = ENERGY_MAX;
    ship.warp   = WARP_MAX;

    trek_move_delta(8, -18);
    ok(ship.quad_y == 1 && ship.quad_x == 5,
       "the manual's example lands in quadrant 2,6 (0-based 1,5)");
    ok(ship.sec_y == 0 && ship.sec_x == 5,
       "and in sector 1,6 (0-based 0,5)");

    /* Off the edge in either direction is refused, not wrapped. A galaxy that
       wraps would let a delta walk the ship out of the 8x8 sector entirely. */
    ship.quad_y = 0; ship.quad_x = 0; ship.sec_y = 0; ship.sec_x = 0;
    ok(trek_move_delta(-1, 0) == MOVE_BAD_COORDS, "north of the galaxy is refused");
    ok(trek_move_delta(0, -1) == MOVE_BAD_COORDS, "west of the galaxy is refused");
    ship.quad_y = 7; ship.quad_x = 7; ship.sec_y = 7; ship.sec_x = 7;
    ok(trek_move_delta(1, 0) == MOVE_BAD_COORDS, "south of the galaxy is refused");
    ok(trek_move_delta(0, 1) == MOVE_BAD_COORDS, "east of the galaxy is refused");

    /* A delta that stays inside the quadrant must use the impulse engines,
       not warp -- otherwise a two-sector hop is billed as a warp jump. */
    {
        uint16_t before;
        trek_new_game(3, 11);
        ship.quad_y = 3; ship.quad_x = 3;
        ship.sec_y  = 3; ship.sec_x  = 3;
        ship.impulse = IMPULSE_MAX;
        ship.energy  = ENERGY_MAX;
        before = ship.impulse;
        sector[(3 << 3) | 5] = SEC_EMPTY;
        trek_move_delta(0, 2);
        ok(ship.quad_y == 3 && ship.quad_x == 3, "a small delta stays in the quadrant");
        ok(ship.impulse < before, "and is billed to the impulse engines");
    }

    /* And with the impulse engines out, that same hop is refused rather than
       silently promoted to a warp jump. */
    trek_new_game(3, 11);
    ship.quad_y = 3; ship.quad_x = 3; ship.sec_y = 3; ship.sec_x = 3;
    ship.sys[SYS_IMPULSE] = 10;
    ok(trek_move_delta(0, 2) == MOVE_NO_IMPULSE,
       "a within-quadrant delta needs working impulse engines");
}

/* ------------------------------------------------------------- planets */

/* Put exactly one planet where the test wants it, with the find the test
   wants. Everything about the chain downstream of generation is easier to
   assert against a scene than against whatever a galaxy rolled. */
static void one_planet(uint8_t find, uint8_t dy, uint8_t dx) {
    uint8_t q = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
    uint8_t sy = (uint8_t)((ship.sec_y + dy) & 7);
    uint8_t sx = (uint8_t)((ship.sec_x + dx) & 7);

    planet_count     = 1;
    planets[0].quad  = q;
    planets[0].sec   = (uint8_t)((sy << 3) | sx);
    planets[0].name  = 0;
    planets[0].cls   = PCLASS_M;
    planets[0].find  = find;
    planets[0].flags = 0;
    sector[planets[0].sec] = SEC_PLANET;
    ship.orbiting = PLANET_NONE;
    ship.shields_up = 0;
}

static void test_planet_generation(void) {
    uint16_t seed, energium = 0, total = 0, doubled_up = 0;
    uint8_t  lo = 255, hi = 0, i, j, bad_name = 0, bad_class = 0;

    puts("planet generation");

    for (seed = 1; seed < 401; seed++) {
        trek_new_game(3, seed);
        if (planet_count < lo) lo = planet_count;
        if (planet_count > hi) hi = planet_count;

        for (i = 0; i < planet_count; i++) {
            total++;
            if (planets[i].find == PFIND_ENERGIUM) energium++;
            if (planets[i].name >= PLANET_NAMES)  bad_name++;
            if (planets[i].cls  >= PCLASS_COUNT)  bad_class++;
            for (j = (uint8_t)(i + 1); j < planet_count; j++)
                if (planets[j].quad == planets[i].quad) doubled_up++;
        }
    }

    /* DERIVED from the ancestor: five to ten, and the one galaxy we have read
       held five. Both ends have to be reachable or the model is not this one. */
    ok(lo == PLANET_MIN, "the smallest galaxy holds PLANET_MIN planets");
    ok(hi == PLANET_MIN + PLANET_SPREAD - 1, "and the largest holds ten");

    ok(bad_name == 0,  "every planet's name is in the table");
    ok(bad_class == 0, "every planet's class is M, N or O");

    /* One in three, and this is the only DERIVED probability in the file that
       is worth asserting -- the ancestor's own comment says one in three, so a
       drift here means the roll was rewritten, not that a constant moved. */
    printf("  energium on %u of %u planets (%u%%), want 33\n",
           energium, total, (unsigned)(100u * energium / total));
    ok(energium * 100u / total >= 30 && energium * 100u / total <= 36,
       "energium falls on one planet in three");

    /* MEASURED: quadrant 6-4 held two of them. A generator that refused
       collisions would pass every other test in this file and silently
       contradict the only PLANET LIST page we have. */
    ok(doubled_up > 0, "two planets can share a quadrant, as 6-4 did");
}

static void test_orbit(void) {
    uint16_t before;

    puts("ORBIT");

    trek_new_game(3, 4242);
    ship.sec_y = 4; ship.sec_x = 4;
    one_planet(PFIND_ENERGIUM, 3, 3);
    ok(trek_orbit() == ORBIT_NO_PLANET, "a planet three sectors off is not adjacent");
    ok(ship.orbiting == PLANET_NONE,    "and no orbit was entered");

    one_planet(PFIND_ENERGIUM, 1, 1);
    before = ship.stardate;
    ok(trek_orbit() == ORBIT_OK,         "a planet diagonally adjacent is");
    ok(ship.orbiting == 0,               "and the orbit records which planet");
    ok(ship.stardate == before,          "ORBIT costs no time (MEASURED: 0.0)");
    ok((planets[0].flags & PF_SCANNED) != 0,
       "the scan is part of the orbit, not a second command");
    ok(trek_orbit() == ORBIT_ALREADY,    "orbiting twice is refused");

    /* Leaving the quadrant leaves orbit. Without this a LAND after a warp
       jump would land on a planet in a quadrant the ship is no longer in --
       and every field trek_land() checks would still look right. */
    trek_enter_quadrant();
    ok(ship.orbiting == PLANET_NONE, "entering a quadrant breaks the orbit");

    /* And so does one sector of impulse. An orbit is a position relative to
       a planet; without this, LAND would still find ship.orbiting set from
       clear across the quadrant and every field it checks would look right. */
    trek_new_game(3, 4243);
    ship.sec_y = 4; ship.sec_x = 4;
    one_planet(PFIND_ENERGIUM, 1, 1);
    ok(trek_orbit() == ORBIT_OK, "in orbit again");
    trek_move_impulse(0, 0);
    ok(ship.orbiting == PLANET_NONE, "and moving one sector breaks it too");
}

static void test_landing(void) {
    uint16_t before, cas = 0;

    puts("LAND");

    trek_new_game(3, 909);
    ship.sec_y = 4; ship.sec_x = 4;
    one_planet(PFIND_ENERGIUM, 0, 1);

    ok(trek_land(LAND_BY_TRANSPORTER, 0) == LAND_NO_ORBIT,
       "landing without orbiting first is refused");

    trek_orbit();
    ship.shields_up = 1;
    ok(trek_land(LAND_BY_TRANSPORTER, 0) == LAND_SHIELDS_UP,
       "neither way down works with the shields up");
    ship.shields_up = 0;

    ship.sys[SYS_TRANSPORTER] = 99;
    ok(trek_land(LAND_BY_TRANSPORTER, 0) == LAND_DAMAGED,
       "the transporter is 100% or nothing (manual)");
    ship.sys[SYS_TRANSPORTER] = 100;

    before = ship.stardate;
    ok(trek_land(LAND_BY_TRANSPORTER, 0) == LAND_ENERGIUM, "energium is mined");
    ok(ship.stardate == before, "the transporter costs no time (MEASURED)");
    ok(inventory[ITEM_RAW_ENERGIUM] == 1, "and one crystal comes aboard");
    ok(trek_land(LAND_BY_TRANSPORTER, 0) == LAND_ALREADY,
       "the same planet does not give up a second crystal");

    /* The shuttlecraft's price, and the whole reason to prefer the
       transporter: 0.2 stardays for the round trip (manual l.490-492). */
    trek_new_game(3, 910);
    ship.sec_y = 4; ship.sec_x = 4;
    one_planet(PFIND_NOTHING, 0, 1);
    trek_orbit();
    before = ship.stardate;
    ok(trek_land(LAND_BY_SHUTTLE, 0) == LAND_NOTHING, "a barren planet yields nothing");
    ok(ship.stardate == (uint16_t)(before + 2),
       "the shuttlecraft costs 0.2 stardates where the transporter costs none");

    /* Settlers, which is what makes the +200 line on the score sheet real. */
    trek_new_game(3, 911);
    ship.sec_y = 4; ship.sec_x = 4;
    one_planet(PFIND_SETTLERS, 0, 1);
    trek_orbit();
    ok(trek_land(LAND_BY_TRANSPORTER, 0) == LAND_SETTLERS, "settlers are found");
    ok(ship.rescues == 1, "and evacuating them counts as a rescue");

    /* A ruined settlement scans the same and rescues nobody. */
    trek_new_game(3, 912);
    ship.sec_y = 4; ship.sec_x = 4;
    one_planet(PFIND_SETTLERS, 0, 1);
    planets[0].flags |= PF_RUINED;
    trek_orbit();
    ok(trek_land(LAND_BY_TRANSPORTER, 0) == LAND_NOTHING,
       "a destroyed settlement has nobody left to take off");
    ok(ship.rescues == 0, "and scores nothing");

    /* A Mongol supply station costs a landing party and is NOT cleared by
       being walked into -- the party can be sent again, and lose again. */
    trek_new_game(3, 913);
    ship.sec_y = 4; ship.sec_x = 4;
    one_planet(PFIND_MONGOL, 0, 1);
    trek_orbit();
    ok(trek_land(LAND_BY_TRANSPORTER, &cas) == LAND_ATTACKED,
       "a Mongol supply station receives the landing party badly");
    ok(cas >= LANDING_CASUALTY_MIN &&
       cas <= LANDING_CASUALTY_MIN + LANDING_CASUALTY_SPAN - 1,
       "the casualties are reported for the message");
    ok(ship.casualties == cas, "and land on the ship's tally, which scores");
    ok(trek_land(LAND_BY_TRANSPORTER, 0) == LAND_ATTACKED,
       "the station is still there on the next visit");
}

static void test_energium(void) {
    uint16_t seed, good = 0, defective = 0, dud = 0, before;
    uint8_t  r, first_pct;

    puts("USE -- raw energium");

    trek_new_game(3, 700);
    ship.energy = 100; ship.shields = 100;
    ok(trek_use_energium(0, 0) == USE_NO_ITEM, "with no crystal there is nothing to load");

    /* The gate is the manual's, to the unit: shields under 50% AND main
       energy under 20%. Both boundaries, both directions -- an off-by-one
       either way would still let the mechanic look like it works. */
    inventory[ITEM_RAW_ENERGIUM] = 1;
    ship.shields = 100;
    ship.energy  = ENERGY_MAX / 5;
    ok(!trek_energium_allowed(), "energy at exactly 20% is refused");
    ship.energy = (uint16_t)(ENERGY_MAX / 5 - 1);
    ok(trek_energium_allowed(), "one unit under 20% is allowed");
    ship.shields = SHIELD_MAX / 2;
    ok(!trek_energium_allowed(), "shields at exactly 50% are refused");
    ship.shields = (uint16_t)(SHIELD_MAX / 2 - 1);
    ok(trek_energium_allowed(), "one unit under 50% is allowed");

    ship.energy = 3000;
    ok(trek_use_energium(0, 0) == USE_REFUSED, "and a healthy ship is refused outright");
    ok(inventory[ITEM_RAW_ENERGIUM] == 1, "a refusal does not consume the crystal");

    /* The measured outcome: energy 500 -> 7435 of a 5000 maximum. The point
       of the whole mechanic is that it goes OVER. */
    trek_new_game(3, 701);
    for (seed = 1; seed < 400; seed++) {
        trek_srand(seed);
        ship.energy  = 500;
        ship.shields = 300;
        inventory[ITEM_RAW_ENERGIUM] = 1;
        planet_defect_restore(CRYSTAL_DEFECT_PCT_0);
        r = trek_use_energium(0, 0);
        if (r == USE_GOOD) {
            good++;
            if (ship.energy <= ENERGY_MAX)      good += 10000;  /* poisons the count */
            if (ship.shields != SHIELD_MAX)     good += 10000;
        } else if (r == USE_DEFECTIVE) defective++;
        else if (r == USE_DUD)         dud++;
    }
    ok(good < 10000, "a good crystal ALWAYS takes energy above ENERGY_MAX");
    ok(defective > 0 && dud > 0 && good > 0, "all three outcomes occur");
    printf("  good %u, defective %u, dud %u of %u\n",
           good, defective, dud, (unsigned)(good + defective + dud));

    /* One measured sample: 500 -> 7435, a gain of 6935. The ancestor's
       formula is 5000 + 0..4499, so every gain must be in that band. */
    trek_srand(12345);
    ship.energy = 500; ship.shields = 300;
    inventory[ITEM_RAW_ENERGIUM] = 20;
    planet_defect_restore(0);
    while (trek_use_energium(0, 0) != USE_GOOD) { ship.energy = 500; }
    ok(ship.energy >= 500 + CRYSTAL_ENERGY_BASE &&
       ship.energy <  500 + CRYSTAL_ENERGY_BASE + CRYSTAL_ENERGY_SPAN,
       "the gain is the ancestor's 5000..9499, which contains the measured 6935");

    /* THE CONVERTER MUST NOT CONFISCATE THE OVERCHARGE. This is the whole
       reason the mechanic is worth having, and until 2026-08-26 the next
       tenth of a stardate silently reset energy to 5000 -- no message, and
       nothing else in the game could ever exceed the maximum to notice. */
    before = ship.energy;
    trek_advance_time(1);
    ok(ship.energy == before,
       "a passing turn does not confiscate energy above the maximum");

    /* The odds get worse every time -- the ancestor's cryprob, doubling. A
       mechanic you can repeat safely is not the gamble either game describes. */
    trek_new_game(3, 702);
    planet_defect_restore(CRYSTAL_DEFECT_PCT_0);
    first_pct = planet_defect_pct();
    ship.energy = 400; ship.shields = 100;
    inventory[ITEM_RAW_ENERGIUM] = 3;
    trek_use_energium(0, 0);
    ok(planet_defect_pct() > first_pct, "each crystal loaded raises the odds of the next");
}

static void test_rescue_scoring(void) {
    ScoreSheet s;

    puts("rescues on the score sheet");

    trek_new_game(3, 55);
    ship.rescues = 3;
    trek_score_sheet(&s);
    ok(s.rescues == 3, "the sheet counts the evacuations");
    ok(s.rescue_pts == 3 * SCORE_PER_RESCUE, "at 200 apiece (MEASURED rubric)");
}

int main(void) {
    test_distance();
    test_no_16bit_overflow();
    test_generation();
    test_determinism();
    test_quadrant_contents();
    test_reveal();
    test_divert();
    test_impulse();
    test_warp();
    test_laser_readings();
    test_laser_model();
    test_fire_laser();
    test_laser_heat();
    test_repair();
    test_converter_scales_with_repair();
    test_enemy_turn();
    test_enemy_movement();
    test_bearing();
    test_expran();
    test_event_queue();
    test_shields();
    test_docking();
    test_base_relief();
    test_torpedo();
    test_self_destruct();
    test_game_state_and_score();
    test_star_and_focus();
    test_damage_effects();
    test_shield_absorption();
    test_system_damage_severity();
    test_blocked_movement();
    test_warp_timing();
    test_manual_movement();
    test_planet_generation();
    test_orbit();
    test_landing();
    test_energium();
    test_rescue_scoring();

    puts("");
    if (failures) {
    

    printf("%d FAILURE(S)\n", failures);
        return 1;
    }
    puts("all core tests pass");
    return 0;
}
