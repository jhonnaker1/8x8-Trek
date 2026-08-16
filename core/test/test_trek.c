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

    puts("");
    if (failures) {
        printf("%d FAILURE(S)\n", failures);
        return 1;
    }
    puts("all core tests pass");
    return 0;
}
