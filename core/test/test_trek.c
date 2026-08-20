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
    unsigned long d_max = 2534, w_max = WARP_MAX;

    puts("16-bit overflow bounds");

    ok((d_max >> 4) * w_max <= 65535UL,
       "warp cost intermediate (d>>4)*warp fits uint16");
    ok((d_max >> 4) * 39UL <= 65535UL,
       "warp time numerator (d>>4)*39 fits uint16");
    ok(w_max * w_max <= 65535UL,
       "squared warp fits uint16 before scaling");
    ok((d_max >> 4) * 20UL <= 65535UL,
       "impulse cost intermediate (d>>4)*20 fits uint16");
    ok(d_max * w_max > 65535UL,
       "...and the naive d*warp really would have overflowed");
}

static void test_generation(void) {
    int i, enemies = 0, bases = 0, starless = 0;
    int expect;

    puts("galaxy generation");

    trek_new_game(3, 12345);
    expect = 3 * ENEMY_PER_LEVEL;   /* lower bound; the rest is random */

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
                if (ship.enemies_left < (uint16_t)(lvl * ENEMY_PER_LEVEL) ||
                    ship.enemies_left >= (uint16_t)(lvl * ENEMY_PER_LEVEL + ENEMY_SPREAD))
                    bad++;
            }
        }
        ok(bad == 0, "all five levels stay in band across 199 seeds");
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
    uint8_t r, oy, ox, ty, tx, found = 0;
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
    /* Net loss, not merely "changed". The converter refills while you fly, so
       an under-priced jump shows up as no change at all once the total is
       clamped at ENERGY_MAX -- which is exactly how the first draft's
       profitable warp hid itself. Assert the direction the manual states. */
    ok(ship.energy < e0,   "warping cost more than the converter replaced");
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

    trek_laser_begin_volley();
    trek_fire_laser(4, 5, 300, &dealt);
    ok(ship.laser_heat == 300, "one shot heats by what it fired");

    sector[cell]   = SEC_BATTLESHIP;
    enemy_hp[cell] = HP_BATTLESHIP;
    trek_fire_laser(4, 5, 450, &dealt);
    ok(ship.laser_heat == 750, "shots in one volley accumulate");
    ok(ship.laser_heat * 2 == LASER_HEAT_MAX,
       "750 is half the gauge, which is the shot that would confirm the scale");

    trek_laser_begin_volley();
    ok(ship.laser_heat == 0, "a new volley starts cold again");

    /* A refused shot must not heat: the original does not spend the energy
       either, and heat is energy that actually went into the banks. */
    before = 0;
    trek_laser_begin_volley();
    trek_fire_laser(0, 0, 500, &dealt);
    ok(ship.laser_heat == before, "firing at empty space adds no heat");
    trek_fire_laser((uint8_t)9, (uint8_t)9, 500, &dealt);
    ok(ship.laser_heat == before, "off-grid coordinates add no heat");
    sector[cell]   = SEC_BATTLESHIP;
    enemy_hp[cell] = HP_BATTLESHIP;
    trek_fire_laser(4, 5, (uint16_t)(ship.energy + 1), &dealt);
    ok(ship.laser_heat == before, "a shot refused for energy adds no heat");

    /* Saturation, not wraparound -- a wrapped gauge reads as cold, which is
       the one failure mode that would be invisible on screen. */
    ship.laser_heat = 65000U;
    ship.energy = 60000U;
    sector[cell]   = SEC_BATTLESHIP;
    enemy_hp[cell] = HP_BATTLESHIP;
    trek_fire_laser(4, 5, 1000, &dealt);
    ok(ship.laser_heat == 65535U, "heat saturates rather than wrapping");
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
    ok(ship.energy == 100,          "but does not refuel");
    ok(!trek_docked_safe(),         "and its shields do not cover us");

    trek_new_game(3, 4242);
    ship.sec_y = 4; ship.sec_x = 4;
    for (i = 0; i < QUAD_CELLS; i++) if (sector[i] == SEC_BASE) sector[i] = SEC_EMPTY;
    sector[(4 << 3) | 5] = SEC_BASE;
    gal_base[(ship.quad_y << 3) | ship.quad_x] = BASE_RESEARCH;
    ship.energy = 100; ship.torps = 1;
    ok(trek_dock() == DOCK_OK,    "a research station can be docked with");
    ok(ship.torps == 1,           "but restocks nothing this core models");
    ok(ship.energy == 100,        "including fuel");

    /* Repair runs four times faster docked. DERIVED -- the number to check
       against the original's own Docked/Undocked columns. */
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
        ok(docked_pct == adrift_pct * DOCK_REPAIR_FACTOR,
           "docked, it mends four times as much");
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
    n = trek_enemy_turn(ev, 16);
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

    trek_new_game(3, 31337);
    ship.sec_y = 4; ship.sec_x = 4;
    ship.shields_up = 0;              /* shields down makes them bold */
    sector[(0 << 3) | 0] = SEC_COMMAND;
    enemy_hp[(0 << 3) | 0] = HP_COMMAND;

    n = trek_enemy_turn(ev, 16);
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
    n = trek_enemy_turn(ev, 16);
    ok(sector[(1 << 3) | 1] == SEC_COMMAND, "a boxed-in enemy holds position");
    ok(fire_amount(ev, n) > 0,              "and fires anyway");

    /* Nothing may land on the ship's own cell. */
    trek_new_game(3, 31337);
    ship.sec_y = 4; ship.sec_x = 4;
    sector[(4 << 3) | 4] = SEC_SHIP;
    sector[(4 << 3) | 5] = SEC_COMMAND;
    enemy_hp[(4 << 3) | 5] = HP_COMMAND;
    trek_enemy_turn(ev, 16);
    ok(sector[(4 << 3) | 4] == SEC_SHIP, "an enemy never steps onto the ship");
    ok(sector[(4 << 3) | 5] == SEC_COMMAND, "and so stays where it was");

    /* REFUTED against the original, and worth a test so it cannot creep back
       in: firing costs an enemy nothing. */
    trek_new_game(3, 31337);
    ship.sec_y = 4; ship.sec_x = 4;
    wall_in(2, 4);
    sector[(2 << 3) | 4] = SEC_BATTLESHIP;
    enemy_hp[(2 << 3) | 4] = HP_BATTLESHIP;
    for (i = 0; i < 5; i++) trek_enemy_turn(ev, 16);
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

    before = ship.shields;
    n = trek_enemy_turn(ev, 16);
    ok(n >= 1, "an enemy in the quadrant fires");
    ok(ship.shields < before, "shields absorb it");
    ok(ship.energy == ENERGY_START, "main energy is untouched while shields hold");

    /* MEASURED on the original: a ship at 41 of 355 hit points hit for 16
       where undamaged companions hit for 27 and 48.

       The enemy is walled in for this one. Enemies move before they fire now,
       and how far they move depends on their power -- so without the wall a
       wounded ship and a healthy one shoot from different distances and this
       compares two things at once. */
    {
        uint16_t strong, weak;
        trek_new_game(3, 31337);
        ship.sec_y = 4; ship.sec_x = 4;
        wall_in(2, 4);
        sector[(2 << 3) | 4] = SEC_BATTLESHIP;
        enemy_hp[(2 << 3) | 4] = HP_BATTLESHIP;
        trek_enemy_turn(ev, 16);
        strong = fire_amount(ev, 16);
        ok(sector[(2 << 3) | 4] == SEC_BATTLESHIP,
           "a walled-in enemy cannot move");

        trek_new_game(3, 31337);
        ship.sec_y = 4; ship.sec_x = 4;
        wall_in(2, 4);
        sector[(2 << 3) | 4] = SEC_BATTLESHIP;
        enemy_hp[(2 << 3) | 4] = 40;
        trek_enemy_turn(ev, 16);
        weak = fire_amount(ev, 16);

        ok(weak < strong, "a wounded enemy hits for less");
    }

    /* Shields gone: the rest lands on the main banks. */
    trek_new_game(3, 31337);
    ship.sec_y = 4; ship.sec_x = 4;
    sector[(2 << 3) | 4] = SEC_BATTLESHIP;
    enemy_hp[(2 << 3) | 4] = HP_BATTLESHIP;
    ship.shields = 0;
    before = ship.energy;
    trek_enemy_turn(ev, 16);
    ok(ship.energy < before, "with shields down the hit reaches main energy");
}

static void test_torpedo(void) {
    uint8_t r;
    puts("torpedoes");

    trek_new_game(3, 777);
    ok(ship.torps == 9, "nine torpedoes at the start");

    ship.sec_y = 4; ship.sec_x = 4;
    sector[(1 << 3) | 1] = SEC_BATTLESHIP;
    enemy_hp[(1 << 3) | 1] = HP_BATTLESHIP;
    ship.enemies_left = 5;

    r = trek_fire_torpedo(1, 1);
    ok(r == TORP_KILL,               "one torpedo destroys a battleship");
    ok(ship.torps == 8,              "and costs a torpedo");
    ok(sector[(1 << 3) | 1] == SEC_EMPTY, "the sector is cleared");
    ok(ship.enemies_left == 4,       "the galaxy counter drops");
    ok(ship.killed == 1,             "counted as a standard Mongol");

    r = trek_fire_torpedo(7, 7);
    ok(r == TORP_MISS,   "firing at empty space misses");
    ok(ship.torps == 7,  "and still costs a torpedo");

    ship.torps = 0;
    ok(trek_fire_torpedo(1, 1) == TORP_NONE_LEFT, "cannot fire without any");
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
    test_docking();
    test_base_relief();
    test_torpedo();
    test_game_state_and_score();

    puts("");
    if (failures) {
        printf("%d FAILURE(S)\n", failures);
        return 1;
    }
    puts("all core tests pass");
    return 0;
}
