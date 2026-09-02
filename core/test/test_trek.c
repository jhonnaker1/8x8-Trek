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


/* Empty the quadrant of everything except the ship.

   Movement and enemy-motion tests are about the ENGINE, not about what the
   galaxy happened to roll into the way. Planets went from five-to-ten spread
   over a list to ten-to-nineteen one per quadrant on 2026-08-26, and six
   tests that had passed for months started failing because a planet now sat
   on a path -- for a reason none of them is about. Same lesson as the torpedo
   test that asserted a miss at sector 7,7. */
static void clear_quadrant(void) {
    uint8_t i;
    for (i = 0; i < QUAD_CELLS; i++) {
        sector[i]   = SEC_EMPTY;
        enemy_hp[i] = 0;
    }
    /* And put the ship where ship.sec_y/x SAY it is. Several tests assign
       those fields directly without touching the map, so preserving whatever
       cell happened to hold SEC_SHIP left a phantom ship behind -- which
       blocks a path just as well as a planet does. */
    sector[(ship.sec_y << 3) | ship.sec_x] = SEC_SHIP;
}

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
    /* warp^3 is STAGED now: wc = 100q + r, and each half is bounded on its
       own. The unstaged form is 100,000 at warp 10 and this test is what
       found that when the ceiling moved -- so it now asserts the staging,
       and the line beneath asserts the form that FAILS, so the guard cannot
       quietly start testing nothing. */
    {
        unsigned long wc = (w_max * w_max) / 10UL;    /* 1000 at warp 10 */
        unsigned long q = wc / 100UL, r = wc % 100UL;
        ok(q * w_max <= 65535UL && r * w_max <= 65535UL,
           "warp cubed, staged at 100, fits uint16");
        ok(wc * w_max > 65535UL,
           "...and the unstaged form really would have overflowed");
    }

    /* The SECOND multiply, which had no guard at all until 2026-08-29 and
       overflows at warp 10 exactly as the first one does. */
    {
        unsigned long wc = 1000UL;                    /* warp^3 at warp 10 */
        unsigned long g = (128UL + 178UL * 14UL) >> 5;
        ok((wc >> 3) * g <= 65535UL && (wc & 7UL) * g <= 65535UL,
           "warp cost final multiply, staged at the shift, fits uint16");
        ok(wc * g > 65535UL,
           "...and unstaged it would have overflowed too");
    }
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
    int i, enemies = 0, starless = 0;
    int expect;

    puts("galaxy generation");

    trek_new_game(3, 12345);
    expect = 0;                                  /* set per level below */

    for (i = 0; i < GAL_CELLS; i++) {
        enemies += gal_enemies[i];
        if (gal_stars[i] > 8) starless++;
    }
    ok(ship.enemies_left == (uint16_t)enemies,
       "enemies_left agrees with the galaxy");

    /* THE RANGE THE BINARY CAN PRODUCE, written out rather than derived from
       the same constants the code uses, so a drifting constant fails here
       instead of moving the band with the test. Lower bound is the fully
       shaved base with no spread and no siege; upper is the unshaved base
       with both. */
    {
        static const uint16_t lo[6] = { 0, 14, 21, 29, 36, 43 };
        static const uint16_t hi[6] = { 0, 28, 36, 44, 52, 63 };
        uint8_t lvl;
        uint16_t seed;
        int bad = 0;
        for (lvl = 1; lvl <= 5; lvl++)
            for (seed = 1; seed < 300; seed++) {
                trek_new_game(lvl, seed);
                if (ship.enemies_left < lo[lvl] || ship.enemies_left > hi[lvl])
                    bad++;
            }
        ok(bad == 0, "every level stays inside what the formula can produce");
    }

    /* AND IT REPRODUCES ALL NINETEEN READINGS taken off the original. The
       fitted model these replaced also did -- what it could not do is
       produce 32 or 43 at level 3, which this one can and which no
       measurement ever ruled out. See trek.h. */
    {
        static const uint16_t m1[2]  = { 18, 21 };
        static const uint16_t m2[2]  = { 30, 32 };
        static const uint16_t m3[10] = { 34,37,37,38,38,38,40,42,42,42 };
        static const uint16_t m4[2]  = { 42, 47 };
        static const uint16_t m5[2]  = { 53, 55 };
        static const uint16_t *sets[6] = { 0, m1, m2, m3, m4, m5 };
        static const uint8_t   lens[6] = { 0, 2, 2, 10, 2, 2 };
        uint8_t lvl, k;
        uint16_t seed;
        int missed = 0;
        for (lvl = 1; lvl <= 5; lvl++) {
            for (k = 0; k < lens[lvl]; k++) {
                int seen = 0;
                for (seed = 1; seed < 400 && !seen; seed++) {
                    trek_new_game(lvl, seed);
                    if (ship.enemies_left == sets[lvl][k]) seen = 1;
                }
                if (!seen) missed++;
            }
        }
        ok(missed == 0, "and every one of the nineteen readings is reachable");
    }

    /* THE SIEGE: the first StarBase starts with three Mongols on it whenever
       its quadrant is otherwise empty. Checked as a frequency because the
       quadrant is not always empty. */
    {
        uint16_t seed;
        int besieged = 0, k;
        for (seed = 1; seed < 200; seed++) {
            trek_new_game(3, seed);
            for (k = 0; k < GAL_CELLS; k++)
                if (gal_base[k] == BASE_STARBASE && gal_enemies[k] == ENEMY_SIEGE)
                    { besieged++; break; }
        }
        ok(besieged > 100, "a StarBase usually starts the game under siege");
    }

    /* PLACED IN CLUMPS, not scattered. The original puts 1..4 into a quadrant
       and never returns to it, so most quadrants are empty -- which the old
       one-at-a-time placement could not produce. */
    {
        int occupied = 0, over1 = 0, k;
        uint16_t seed;
        for (seed = 1; seed < 60; seed++) {
            trek_new_game(3, seed);
            for (k = 0; k < GAL_CELLS; k++) {
                if (gal_enemies[k]) occupied++;
                if (gal_enemies[k] > 1) over1++;
            }
        }
        ok(occupied < 60 * 25, "enemies occupy well under half the galaxy");
        ok(over1 * 2 > occupied, "and most occupied quadrants hold more than one");
    }

    trek_new_game(3, 12345);
    (void)expect;

    trek_new_game(3, 12345);   /* restore the state the rest of this test uses */
    /* TWO LOOPS, not one -- see trek.c. StarBases are 11 - V of them (the
       reading gives 7 - level), then two to four research/supply stations.
       The old assertion of "two to four bases" was this port's own model. */
    {
        uint8_t sb = 0, other = 0, k;
        for (k = 0; k < GAL_CELLS; k++) {
            if (gal_base[k] == BASE_STARBASE) sb++;
            else if (gal_base[k] != BASE_NONE) other++;
        }
        ok(sb == STARBASES_AT_LEVEL(3), "a level-3 galaxy holds 7-level StarBases");
        ok(other >= 2 && other <= 4, "plus two to four research/supply stations");
    }

    /* MEASURED: StarBases never sit on the galaxy's edge, and never within
       two quadrants of the previously placed one. */
    {
        uint8_t k, edge = 0;
        for (k = 0; k < GAL_CELLS; k++)
            if (gal_base[k] == BASE_STARBASE) {
                uint8_t y = (uint8_t)(k >> 3), x = (uint8_t)(k & 7);
                if (y == 0 || y == 7 || x == 0 || x == 7) edge++;
            }
        ok(edge == 0, "and no StarBase on the galaxy's edge");
    }
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
    clear_quadrant();
    /* THE CEILING IS 10.0. This asserted that warp 9 was refused, from a
       WARP_MAX of 80 that cited the manual's "allowed warp 8 in emergencies".
       The binary's own WARP command clamps at 1.0 and 10.0 (0x00DADE and
       0x00DAF8) and 9 is accepted; the manual sentence is about what the ship
       is PERMITTED, and exceeding it is what breaks the engines. */
    ok(trek_set_warp(5) == 0,    "warp 0.5 rejected (below minimum)");
    ok(trek_set_warp(90) == 1,   "warp 9.0 accepted -- the ceiling is 10");
    ok(trek_set_warp(100) == 1,  "warp 10.0 accepted, the binary's own limit");
    ok(trek_set_warp(101) == 0,  "warp 10.1 rejected");
    ok(trek_set_warp(50) == 1,   "warp 5.0 accepted");

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
        clear_quadrant();     /* the departure path must be clear -- see above */
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

/* LASER HEAT, rewritten 2026-08-26 against fn 0x09CC1. It is not a gauge --
   it damages the banks. The constants are read, not fitted. */
static void test_laser_heat(void) {
    uint16_t dealt;
    uint8_t  cell, before;

    puts("laser heat (BINARY: it damages the banks)");

    trek_new_game(3, 4242);
    ok(ship.laser_heat == 0, "a new game starts cold");

    ship.sec_y = 4; ship.sec_x = 4;
    cell = (uint8_t)((4 << 3) | 5);
    sector[cell]   = SEC_BATTLESHIP;
    enemy_hp[cell] = HP_BATTLESHIP;

    trek_laser_begin_volley();
    trek_fire_laser(4, 5, 360, &dealt);
    ok(ship.laser_heat == 360 / 15, "one shot heats by fired div 15");

    sector[cell]   = SEC_BATTLESHIP;
    enemy_hp[cell] = HP_BATTLESHIP;
    trek_fire_laser(4, 5, 360, &dealt);
    ok(ship.laser_heat == 2 * (360 / 15), "and shots accumulate");

    trek_laser_begin_volley();
    ok(ship.laser_heat == 2 * (360 / 15),
       "a NEW VOLLEY does not clear it -- the original accumulates across them");

    /* A refused shot must not heat: the original does not spend the energy
       either, and heat is energy that actually went into the banks. */
    before = (uint8_t)ship.laser_heat;
    trek_fire_laser(0, 0, 500, &dealt);
    ok(ship.laser_heat == before, "firing at empty space adds no heat");
    trek_fire_laser((uint8_t)9, (uint8_t)9, 500, &dealt);
    ok(ship.laser_heat == before, "off-grid coordinates add no heat");
    sector[cell]   = SEC_BATTLESHIP;
    enemy_hp[cell] = HP_BATTLESHIP;
    trek_fire_laser(4, 5, (uint16_t)(ship.energy + 1), &dealt);
    ok(ship.laser_heat == before, "a shot refused for energy adds no heat");

    /* 0x78 at 0x009EFF. The measured 100 was a floor on the observation --
       nothing in that run fired enough in a turn to reach the real cap. */
    ship.laser_heat = LASER_HEAT_CAP - 1;
    ship.energy = 60000U;
    ship.sys[SYS_LASERS] = 100;
    sector[cell]   = SEC_BATTLESHIP;
    enemy_hp[cell] = HP_BATTLESHIP;
    trek_fire_laser(4, 5, 5000, &dealt);
    ok(LASER_HEAT_CAP == 120, "the cap is 120, not the 100 that was measured");
    ok(ship.laser_heat == LASER_HEAT_CAP, "heat caps there, it does not wrap");

    /* THE MECHANIC. Above 90 the banks take damage -- this is what run 2
       could not see, because it read the damage of the same shot the penalty
       lands after, with a rig that repaired every system between turns. */
    trek_new_game(3, 4242);
    ship.sec_y = 4; ship.sec_x = 4;
    clear_quadrant();
    sector[cell] = SEC_BATTLESHIP;
    enemy_hp[cell] = 60000U;
    ship.energy = 60000U;
    ship.sys[SYS_LASERS] = 100;
    ship.laser_heat = LASER_OVERHEAT_AT;          /* exactly at, not above */
    trek_fire_laser(4, 5, 15, &dealt);            /* +1 heat, so now 91 */
    ok(laser_overheated == 1, "past 90 the ship is told the banks overheated");
    /* The bite is `damage div 120 + Random(5)`, so a single hot shot can cost
       nothing at all -- the message still prints. Degradation is asserted
       over a run rather than on one draw. */
    {
        uint8_t k;
        for (k = 0; k < 20; k++) {
            enemy_hp[cell] = 60000U;
            sector[cell] = SEC_BATTLESHIP;
            ship.energy = 60000U;
            ship.laser_heat = LASER_HEAT_CAP;
            trek_fire_laser(4, 5, 400, &dealt);
        }
        ok(ship.sys[SYS_LASERS] < 60,
           "and twenty hot volleys wreck them");
    }

    trek_new_game(3, 4242);
    ship.sec_y = 4; ship.sec_x = 4;
    clear_quadrant();
    sector[cell] = SEC_BATTLESHIP;
    enemy_hp[cell] = 60000U;
    ship.energy = 60000U;
    ship.sys[SYS_LASERS] = 100;
    ship.laser_heat = 0;
    trek_fire_laser(4, 5, 300, &dealt);
    ok(ship.sys[SYS_LASERS] == 100, "a cold shot costs the banks nothing");
    ok(laser_overheated == 0,       "and says nothing");

    /* THE SHOT ITSELF IS AT FULL STRENGTH. The penalty lands after the
       damage is computed, which is the whole reason it was missed. */
    {
        uint16_t hot, cold;
        trek_new_game(3, 4242);
        ship.sec_y = 4; ship.sec_x = 4;
        clear_quadrant();
        sector[cell] = SEC_BATTLESHIP; enemy_hp[cell] = 60000U;
        ship.energy = 60000U; ship.sys[SYS_LASERS] = 100; ship.laser_heat = 0;
        trek_fire_laser(4, 5, 400, &cold);

        trek_new_game(3, 4242);
        ship.sec_y = 4; ship.sec_x = 4;
        clear_quadrant();
        sector[cell] = SEC_BATTLESHIP; enemy_hp[cell] = 60000U;
        ship.energy = 60000U; ship.sys[SYS_LASERS] = 100;
        ship.laser_heat = LASER_HEAT_CAP;
        trek_fire_laser(4, 5, 400, &hot);
        ok(hot == cold, "heat does not weaken the shot it is measured on");
    }

    /* TWO COOLDOWNS. Twenty a command and 360 a stardate, both floored. */
    trek_new_game(3, 4242);
    ship.laser_heat = 100;
    trek_turn_end();
    ok(ship.laser_heat == 80, "a turn sheds twenty points");
    ship.laser_heat = 10;
    trek_turn_end();
    ok(ship.laser_heat == 0, "and it floors at zero rather than wrapping");

    ship.laser_heat = 100;
    trek_advance_time(1);            /* 0.1 stardates -> 36 points */
    ok(ship.laser_heat == 64, "a tenth of a stardate sheds thirty-six");
    ship.laser_heat = 100;
    trek_advance_time(10);
    ok(ship.laser_heat == 0, "a whole stardate cools them completely");

    /* Which is why it looked like leaving the quadrant cleared it: a warp
       jump elapses more than enough time. */
    trek_new_game(3, 4242);
    ship.laser_heat = LASER_HEAT_CAP;
    clear_quadrant();
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

    /* trek_advance_time, NOT trek_advance: this is about the CONVERTER, and
       trek_advance also fires whatever is scheduled inside the window. A
       death pod landing in that stardate costs energy and the test reads it
       as a converter fault. It broke exactly that way when an unrelated
       change consumed one more random number and shifted the schedule. */
    trek_new_game(3, 5150);
    ship.energy = 1000;
    trek_advance_time(10);
    healthy = ship.energy;

    trek_new_game(3, 5150);
    ship.energy = 1000;
    ship.sys[SYS_CONVERTER] = 50;
    trek_advance_time(10);
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
/* The deviate. The original's is UNIFORM -- `base + spread * Random` -- and
   that is what these check: the range is exactly [base, base+spread), the
   mean sits in the middle of it, there is NO long tail, and a seed reproduces
   the sequence on every target. The exponential this replaced had a tail that
   reached 4.16x its mean, which is a visibly different game. */
static void test_expran(void) {
    uint16_t i, v, lo = 0xFFFF, hi = 0;
    uint32_t total = 0;              /* test-side only; the core forbids this */

    puts("the schedule deviate (BINARY: uniform, not exponential)");

    trek_srand(4242);
    for (i = 0; i < 4096; i++) {
        v = trek_sched_deviate(200, 400);
        total += v;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    ok(lo >= 200 && hi < 600, "every draw lands inside [base, base+spread)");
    {
        uint16_t mean = (uint16_t)(total / 4096);
        ok(mean >= 380 && mean <= 420, "and the mean is the middle of it");
    }
    ok(hi > 590, "the top of the range is reached");
    ok(lo < 210, "and so is the bottom");

    /* NO TAIL. This is the property that changed, and the one a later edit
       reaching for the ancestor's expran() again would break. */
    {
        uint16_t over = 0;
        trek_srand(11);
        for (i = 0; i < 4096; i++)
            if (trek_sched_deviate(200, 400) >= 600) over++;
        ok(over == 0, "nothing ever lands beyond the spread -- there is no tail");
    }

    /* A zero spread is a fixed interval, not a divide by zero. */
    ok(trek_sched_deviate(123, 0) == 123, "a zero spread is a fixed interval");

    /* Same seed, same sequence -- the property the whole cross-platform
       comparison rests on. */
    {
        uint16_t a[8], b[8];
        trek_srand(99);
        for (i = 0; i < 8; i++) a[i] = trek_sched_deviate(50, 250);
        trek_srand(99);
        for (i = 0; i < 8; i++) b[i] = trek_sched_deviate(50, 250);
        ok(memcmp(a, b, sizeof a) == 0, "a seed reproduces the sequence exactly");
    }

    /* THE TWO SLOTS THAT ARE THE ORIGINAL'S. A base under attack can last
       between 2 and 4 stardates and no other figure -- that is the number the
       COMMUNICATIONS panel prints, and it was captured on screen twice. */
    {
        uint16_t k, f, flo = 0xFFFF, fhi = 0;
        trek_srand(5150);
        for (k = 0; k < 2000; k++) {
            f = trek_sched_deviate(SCHED_FALLS_BASE_TENTHS,
                                   SCHED_FALLS_SPAN_TENTHS);
            if (f < flo) flo = f;
            if (f > fhi) fhi = f;
        }
        ok(flo == 20 && fhi == 39,
           "a base under attack lasts 2.0 to 3.9 stardates, never longer");
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
    /* The literal: the binary subtracts the real 50.0 at 0x00EA6C. Written
       in terms of the #define this would move with it and check nothing. */
    ok(ship.energy == before - 50,
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
        /* The Commander must have a clear run at us: this asserts that it
           closes EVERY turn, and a planet standing in its way makes it hold
           for a reason the test is not about. */
        clear_quadrant();
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

/* THE ENEMY'S SHOT IS A BAND, NOT A FLAT PERCENTAGE -- fn 0x16844 at
   0x01696F rolls `0.6 + Random*0.1` on the hit points. */
static void test_enemy_shot_band(void) {
    puts("the enemy's shot (BINARY: a band, not a flat percentage)");
    {
        TrekEvent tev[16];
        uint16_t lo = 0xFFFF, hi = 0;
        uint8_t  k, tn, tcell = (4 << 3) | 5;
        trek_new_game(3, 9001);
        for (k = 0; k < 250; k++) {
            uint8_t j;
            ship.sec_y = 4; ship.sec_x = 4;
            clear_quadrant();
            sector[tcell] = SEC_BATTLESHIP;
            enemy_hp[tcell] = 1000;
            ship.energy = 60000U; ship.shields = 0; ship.shields_up = 0;
            ship.lost = 0;
            tn = trek_enemy_turn(tev, 16, 0);
            for (j = 0; j < tn; j++)
                if (tev[j].kind == EV_HIT || tev[j].kind == EV_SHIELD_HOLD) {
                    if (tev[j].amount < lo) lo = tev[j].amount;
                    if (tev[j].amount > hi) hi = tev[j].amount;
                }
        }
        /* Range 1.0, so the falloff factor is 235/256. The band is 90..105
           of hit points -- the binary's 0.6..0.7 carrying the 1.5 this core
           divides out of the falloff -- so 0.90*1000*235/256 = 826 up to
           1.05*1000*235/256 = 964.
           MEASURED AGAINST THE ORIGINAL, 2026-08-27: nine shots at exactly
           this range, hit points and shield state came back 851.4 to 953.0,
           which sits inside that band with room at both ends. The old
           assertion here was 661..771 and every one of those readings would
           have been outside it. */
        ok(lo >= 815 && lo <= 845, "the weakest shot is about 90% of hit points");
        ok(hi >= 950 && hi <= 975, "the strongest about 105%");
        ok(hi - lo > 80, "and it genuinely varies -- it is not a flat figure");
    }
}

/* THE TRACTOR BEAM, read out of fn 0x0C609 on 2026-08-26. It is not an event:
   flying PAST a Commander is what catches you. */
static void test_tractor(void) {
    uint8_t i, caught = 0, past = 0;
    puts("the tractor beam (BINARY: it is a move, not an event)");

    /* No Commander anywhere: it can never fire. */
    trek_new_game(3, 4242);
    for (i = 0; i < GAL_CELLS; i++) gal_commander[i] = 0;
    for (i = 0; i < 100; i++) {
        ship.quad_y = 0; ship.quad_x = 0; ship.sec_y = 0; ship.sec_x = 0;
        ship.energy = 60000U; ship.shields_up = 0;
        clear_quadrant();
        tractored = 0;
        trek_move_warp(7, 7, 3, 3);
        if (tractored) caught++;
    }
    ok(caught == 0, "with no Commander in the galaxy nothing ever catches us");

    /* One Commander squarely in the flight path. Two in ten per quadrant, and
       a corner-to-corner jump crosses its quadrant once. */
    trek_new_game(3, 4242);
    caught = 0;
    for (i = 0; i < 200; i++) {
        uint8_t k;
        for (k = 0; k < GAL_CELLS; k++) { gal_commander[k] = 0; gal_enemies[k] = 0; }
        gal_commander[(4 << 3) | 4] = 1;
        gal_enemies[(4 << 3) | 4]   = 2;
        ship.quad_y = 0; ship.quad_x = 0; ship.sec_y = 0; ship.sec_x = 0;
        ship.energy = 60000U; ship.shields_up = 0;
        clear_quadrant();
        tractored = 0;
        trek_move_warp(7, 7, 3, 3);
        if (tractored) {
            caught++;
            /* And it pulls us to the Commander, not somewhere random. */
            if (ship.quad_y == 4 && ship.quad_x == 4) past++;
        }
    }
    ok(caught > 20 && caught < 70, "one Commander in the path catches us about two times in ten");
    ok(past == caught, "and pulls us to ITS quadrant, not a random one");

    /* THE SAME COMMANDER OUTSIDE THE BOUNDING RECTANGLE CANNOT REACH US.
       This is the discriminator against the old scheduled model, which had no
       geometry at all. */
    trek_new_game(3, 4242);
    caught = 0;
    for (i = 0; i < 200; i++) {
        uint8_t k;
        for (k = 0; k < GAL_CELLS; k++) { gal_commander[k] = 0; gal_enemies[k] = 0; }
        gal_commander[(7 << 3) | 7] = 1;      /* far corner */
        gal_enemies[(7 << 3) | 7]   = 2;
        ship.quad_y = 0; ship.quad_x = 0; ship.sec_y = 0; ship.sec_x = 0;
        ship.energy = 60000U; ship.shields_up = 0;
        clear_quadrant();
        tractored = 0;
        trek_move_warp(1, 1, 3, 3);           /* a short hop nowhere near it */
        if (tractored) caught++;
    }
    ok(caught == 0, "a Commander outside the trip's rectangle cannot reach us");

    /* Commanders are PER-QUADRANT STATE, so a quadrant that holds one holds
       one every time you visit -- the port used to re-roll on arrival. */
    trek_new_game(3, 4242);
    {
        uint8_t k, q = GAL_CELLS, seen1 = 0, seen2 = 0;
        for (k = 0; k < GAL_CELLS; k++)
            if (gal_commander[k] && gal_enemies[k]) { q = k; break; }
        ok(q < GAL_CELLS, "a level-3 galaxy has Commanders in it");
        if (q < GAL_CELLS) {
            ship.quad_y = (uint8_t)(q >> 3); ship.quad_x = (uint8_t)(q & 7);
            trek_enter_quadrant();
            for (k = 0; k < QUAD_CELLS; k++) if (sector[k] == SEC_COMMAND) seen1++;
            trek_enter_quadrant();
            for (k = 0; k < QUAD_CELLS; k++) if (sector[k] == SEC_COMMAND) seen2++;
            ok(seen1 >= 1 && seen1 == seen2,
               "and the same quadrant holds the same Commanders on every visit");
        }
    }

    /* None below level 3 -- `cmp [0x1DF0], 7 / jl` guards the whole branch. */
    {
        uint16_t seed;
        int any = 0;
        for (seed = 1; seed < 100; seed++) {
            uint8_t k;
            trek_new_game(2, seed);
            for (k = 0; k < GAL_CELLS; k++) if (gal_commander[k]) any++;
        }
        ok(any == 0, "below level 3 there are no Commanders at all");
    }

    /* AND THEY ARE NOT EVERYWHERE. A quadrant with more than one ship gets
       one three times in seven, so a level-3 galaxy holds a handful -- not
       one per busy quadrant, and not only the besieged base. */
    {
        uint16_t seed;
        int total = 0, busy = 0, k;
        for (seed = 1; seed < 100; seed++) {
            trek_new_game(3, seed);
            for (k = 0; k < GAL_CELLS; k++) {
                if (gal_commander[k]) total++;
                if (gal_enemies[k] > 1) busy++;
            }
        }
        ok(total * 3 < busy * 2, "commanders lead well under half the busy quadrants");
        ok(total > 100, "but a galaxy holds several of them");
    }
}

/* Reaching a besieged base relieves it -- the deadline is actionable. */
static void test_base_relief(void) {
    TrekEvent ev[16];
    uint8_t q;

    puts("relieving a base");

    trek_new_game(3, 4242);
    trek_unschedule(SCHED_BASE_FALLS);
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
    trek_schedule(SCHED_BASE_FALLS, 60000);
    ok(trek_scheduled(SCHED_BASE_FALLS) > ship.stardate,
       "a huge offset saturates rather than wrapping into the past");

    /* Two events due in the same window both fire, oldest first.
       RESTORED 2026-08-27. It was retired when the death pod's slot turned
       out not to be a scheduled event at all, leaving only BASE_ATTACK and
       BASE_FALLS, which reschedule each other and so can never both be due in
       one call. SCHED_DISTRESS is a real third slot -- DS:0x1D9C, the
       settlers' call -- and it is independent of both. */
    trek_new_game(3, 4242);
    trek_schedule(SCHED_BASE_ATTACK, 10);
    trek_schedule(SCHED_DISTRESS, 20);
    n = trek_advance(50, ev, 16);
    {
        int8_t first = -1, second = -1;
        for (i = 0; i < n; i++) {
            if (ev[i].kind == EV_BASE_ATTACKED && first < 0)  first = (int8_t)i;
            if (ev[i].kind == EV_DISTRESS      && second < 0) second = (int8_t)i;
        }
        ok(first >= 0 && second >= 0, "both events in one window fire");
        ok(first < second, "and they are reported in date order");
    }

    /* --- THE SETTLERS' DEADLINE STARTS WITH THE CALL, NOT THE GAME --- */
    {
        TrekEvent ev3[8];
        uint16_t t3;
        uint8_t k, got = 0;

        trek_new_game(3, 4242);
        ok(planet_evac_end == SCHED_NEVER,
              "a fresh galaxy has no evacuation deadline at all");
        ok(!planet_settlement_lost(),
              "so the settlement cannot be lost on turn one");

        /* Run past the call and catch it. */
        for (t3 = 0; t3 < 200 && !got; t3++) {
            n = trek_advance(1, ev3, 8);
            for (k = 0; k < n; k++) if (ev3[k].kind == EV_DISTRESS) got = 1;
        }
        ok(got, "the settlers call for help");
        ok(ship.stardate >= STARDATE_START + DISTRESS_AT_TENTHS,
              "and never before 3505.0");
        ok(planet_evac_end >= ship.stardate + EVAC_WARNING_MIN_TENTHS,
              "the deadline is a stardate or more away when it is set");
        ok(planet_evac_end <= ship.stardate + EVAC_WARNING_MIN_TENTHS
                              + EVAC_WARNING_SPAN_TENTHS,
              "and four at the most");
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
    clear_quadrant();     /* the Commander needs a clear run -- see above */
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
        /* STANDING STILL, EVERY ENEMY FIRES EVERY TURN. The old assertion
           here was "roughly half", which was a MEASURED observation of a
           mechanism somewhere else entirely -- the gate is on the whole enemy
           turn and only applies when the ship MOVED. There is no per-ship
           hold-fire roll in the original at all. See trek.h. */
        if (fired != 200) printf("    (fired %d of 200)\n", fired);
        ok(fired == 200, "standing still, every enemy fires every turn");
    }

    /* --- and moving buys a 40%% chance of no enemy turn at all --- */
    {
        TrekEvent ev[16];
        int i, acted = 0;
        trek_new_game(3, 31337);
        ship.sec_y = 4; ship.sec_x = 4;
        wall_in(2, 4);
        sector[(2 << 3) | 4]   = SEC_BATTLESHIP;
        enemy_hp[(2 << 3) | 4] = HP_BATTLESHIP;
        ship.shields_up = 1;
        for (i = 0; i < 400; i++) {
            uint8_t n2;
            ship.shields = SHIELD_MAX;
            ship.energy  = ENERGY_MAX;
            ship.sys[SYS_SHIELDS] = 100;
            ship.moved = 1;                 /* as MOVE sets [0x26E3] */
            n2 = trek_enemy_turn(ev, 16, 1);
            if (n2 > 0) acted++;
            ok(ship.moved == 0 || i < 0, "the flag is cleared either way");
            break;
        }
        acted = 0;
        for (i = 0; i < 400; i++) {
            uint8_t n2;
            ship.shields = SHIELD_MAX;
            ship.energy  = ENERGY_MAX;
            ship.sys[SYS_SHIELDS] = 100;
            ship.moved = 1;
            n2 = trek_enemy_turn(ev, 16, 1);
            if (n2 > 0) acted++;
        }
        /* 60 in 100. Four hundred turns puts the expectation at 240; the band
           is wide enough to be stable and narrow enough to fail at 100%%. */
        if (acted < 200 || acted > 280) printf("    (acted %d of 400)\n", acted);
        ok(acted > 200 && acted < 280,
           "having moved, the enemy turn happens about 60 times in 100");

        /* AND MOVING MUST ACTUALLY SET THE FLAG. Without this the two cases
           above both pass with `ship.moved = 1` deleted from every movement
           routine, because they set it by hand -- which is exactly what
           happened when the gate was broken deliberately and nothing failed. */
        trek_new_game(3, 31337);
        ship.moved = 0;
        (void)trek_move_impulse(3, 3);
        ok(ship.moved, "impulse movement sets the flag");

        trek_new_game(3, 31337);
        ship.moved = 0;
        ship.energy = ENERGY_MAX;
        (void)trek_move_warp((uint8_t)((ship.quad_y + 1) & 7), ship.quad_x, 3, 3);
        ok(ship.moved, "and so does a warp");
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

/* Fires one torpedo into a cleared quadrant and returns the result. */
static uint8_t torp_shot(uint8_t sy0, uint8_t sx0, uint8_t ty, uint8_t tx,
                         uint8_t up, uint16_t *dmg) {
    ship.sec_y = sy0; ship.sec_x = sx0;
    clear_quadrant();
    sector[(ty << 3) | tx] = SEC_COMMAND;
    enemy_hp[(ty << 3) | tx] = 60000;
    ship.torps = 9;
    ship.shields_up = up;
    ship.shields = up ? SHIELD_MAX : 0;
    return trek_fire_torpedo(ty, tx, dmg);
}

static void test_torpedo(void) {
    uint8_t r;
    uint16_t dmg;
    puts("torpedoes (BINARY: the original ray-marches)");

    ok(TORP_DAMAGE_AT_LEVEL(1) == 325, "torpedo damage is 325 at level 1");
    ok(TORP_DAMAGE_AT_LEVEL(3) == 355, "355 at level 3 -- the measured figure");
    ok(TORP_DAMAGE_AT_LEVEL(5) == 385, "and 385 at level 5");
    /* The SAME expression as a battleship's hit points, at every level. That
       is why one torpedo has always killed a standard Mongol. */
    ok(TORP_DAMAGE_AT_LEVEL(1) == HP_BATTLESHIP_AT(1) &&
       TORP_DAMAGE_AT_LEVEL(3) == HP_BATTLESHIP_AT(3) &&
       TORP_DAMAGE_AT_LEVEL(5) == HP_BATTLESHIP_AT(5),
       "and is exactly a battleship's hit points at every level");

    /* All four classes, read out of the binary. Each level-3 column is a
       separate measured sighting. */
    ok(HP_COMMAND_AT(3) == 695 && HP_BATTLESHIP_AT(3) == 355 &&
       HP_SCOUT_AT(3) == 255 && HP_SUPPLY_AT(3) == 120,
       "the four class strengths reproduce every level-3 sighting");
    ok(HP_COMMAND_AT(1) == 625 && HP_BATTLESHIP_AT(1) == 325 &&
       HP_SCOUT_AT(1) == 225 && HP_SUPPLY_AT(1) == 100,
       "and scale down at level 1, where 325 was also measured");

    trek_new_game(3, 777);
    ok(ship.torps == 9, "nine torpedoes at the start");

    /* Close in, where the march has one or two steps to wander in and the
       torpedo always arrives on the cell's own corner. MEASURED: six shots
       inside range 2.24 all read exactly 355. */
    ship.sec_y = 4; ship.sec_x = 4;
    clear_quadrant();
    sector[(2 << 3) | 4] = SEC_BATTLESHIP;
    enemy_hp[(2 << 3) | 4] = HP_BATTLESHIP;
    ship.enemies_left = 5;

    r = trek_fire_torpedo(2, 4, &dmg);
    ok(r == TORP_KILL,               "one torpedo destroys a battleship up close");
    ok(dmg == 355,                   "for exactly the measured 355");
    ok(ship.torps == 8,              "and costs a torpedo");
    ok(sector[(2 << 3) | 4] == SEC_EMPTY, "the sector is cleared");
    ok(ship.enemies_left == 4,       "the galaxy counter drops");
    ok(ship.killed == 1,             "counted as a standard Mongol");

    /* A Commander walks away from one. 695 - 355 = 340, MEASURED. */
    trek_new_game(3, 777);
    ship.sec_y = 4; ship.sec_x = 4;
    clear_quadrant();
    sector[(2 << 3) | 4] = SEC_COMMAND;
    enemy_hp[(2 << 3) | 4] = HP_COMMAND;
    r = trek_fire_torpedo(2, 4, &dmg);
    ok(r == TORP_OK,                 "a Commander SURVIVES a torpedo");
    ok(dmg == 355,                   "taking the same 355");
    ok(enemy_hp[(2 << 3) | 4] == HP_COMMAND - 355, "and is left with 340");
    ok(sector[(2 << 3) | 4] == SEC_COMMAND, "still there, still a Commander");
    ok(ship.killed_cmd == 0,         "and is not counted as a kill");

    /* THE HOLE IN THE DAMAGE. A direct hit is `base`; a graze is
       `(1 - d) * base` with d in [0.3, 0.6), so at most 0.7 * 355 = 248. NO
       VALUE BETWEEN 249 AND 354 CAN EVER COME OUT. That is the sharpest
       consequence of the law and the one the measurement disagrees with --
       one of nineteen shots read 296. See trek.h. */
    {
        int i, in_hole = 0, direct = 0, graze = 0;
        trek_new_game(3, 4242);
        for (i = 0; i < 600; i++) {
            uint8_t res = torp_shot(6, 4, 1, 4, 0, &dmg);
            if (res != TORP_OK && res != TORP_KILL) continue;
            if (dmg == 355) direct++;
            else if (dmg <= 248) graze++;
            else in_hole++;
        }
        ok(in_hole == 0, "no damage figure between 249 and 354 is possible");
        ok(direct > 20,  "some shots at range 5 land dead centre for 355");
        ok(graze > 200,  "and most graze for 248 or less");
    }

    /* ACCURACY IS AN ANGLE, NOT A DISTANCE. Both wobbles are one-sided, so a
       torpedo fired along the diagonal drifts ALONG its own line of travel
       and barely errs, while the same range off the diagonal drifts sideways.
       The old model had accuracy fall with distance alone and could not tell
       these apart. */
    {
        int i, diag_bad = 0, off_bad = 0;
        trek_new_game(3, 4243);
        for (i = 0; i < 400; i++) {
            uint8_t a = torp_shot(1, 1, 7, 7, 0, &dmg);     /* range 8.49, 45 deg */
            uint8_t b = torp_shot(7, 1, 0, 4, 0, &dmg);     /* range 7.62, off it */
            if (a == TORP_MISS || a == TORP_THROUGH) diag_bad++;
            if (b == TORP_MISS || b == TORP_THROUGH) off_bad++;
        }
        ok(diag_bad * 4 < off_bad,
           "a long diagonal shot is far more accurate than the same range off it");
        ok(off_bad > 0, "and the off-diagonal one does fail sometimes");
    }

    /* Shields deflect our OWN torpedoes -- the manual says so and the binary
       puts a charge/25000 wobble on the launch. It costs accuracy at range. */
    {
        int i, up_bad = 0, down_bad = 0;
        trek_new_game(3, 4244);
        for (i = 0; i < 400; i++) {
            uint8_t a = torp_shot(7, 1, 0, 4, 1, &dmg);
            uint8_t b = torp_shot(7, 1, 0, 4, 0, &dmg);
            if (a == TORP_MISS || a == TORP_THROUGH) up_bad++;
            if (b == TORP_MISS || b == TORP_THROUGH) down_bad++;
        }
        ok(up_bad > down_bad * 2,
           "firing through raised shields throws torpedoes off course");
    }

    /* One in fifty fails to detonate -- "EnTorp fails to detonate." */
    {
        int i, duds = 0;
        trek_new_game(3, 4245);
        for (i = 0; i < 2000; i++)
            if (torp_shot(4, 4, 2, 4, 0, &dmg) == TORP_DUD) duds++;
        ok(duds > 20 && duds < 65, "about one torpedo in fifty is a dud");
    }

    /* A STAR IN THE PATH stops it, even though the target is beyond. That is
       a consequence of marching that the old model could not have. */
    {
        int i, stopped = 0, nova = 0, gone = 0;
        uint8_t r;
        trek_new_game(3, 4246);
        for (i = 0; i < 400; i++) {
            trek_new_game(3, (uint16_t)(4246 + i));
            ship.quad_y = 3; ship.quad_x = 3;   /* room to be thrown in any way */
            ship.sec_y = 6; ship.sec_x = 4;
            clear_quadrant();
            sector[(3 << 3) | 4] = SEC_STAR;
            sector[(1 << 3) | 4] = SEC_COMMAND;
            enemy_hp[(1 << 3) | 4] = 60000;
            ship.torps = 9; ship.shields_up = 0;
            r = trek_fire_torpedo(1, 4, &dmg);
            /* THREE outcomes since 2026-08-29, not two: the star can also
               be DESTROYED. This counted TORP_ABSORBED and TORP_NOVA and
               called the sum "always stops", which stopped being the same
               statement the moment a third stopping outcome existed. */
            if (r == TORP_ABSORBED) stopped++;
            else if (r == TORP_NOVA) nova++;
            else if (r == TORP_STAR_GONE) gone++;
        }
        ok(stopped + nova + gone == 400,
              "a star in the flight path always stops the torpedo");
        ok(stopped > 0 && gone > 0,
              "and it is absorbed sometimes and destroyed sometimes");
        /* 4 / 38.4 / 57.6 on 400 shots. The bands are four sd wide and are
           written as literals, not as the constants they test. */
        ok(gone > 175 && gone < 285, "the star is destroyed about 57.6% of hits");
        ok(stopped > 110 && stopped < 195, "and absorbed about 38.4%");

        /* A DESTROYED STAR STAYS DESTROYED, which is the half of this that is
           not a cell change. The original keeps a per-quadrant COUNT and the
           fill draws that many stars as 'N' -- so leaving and coming back
           must not put the star back. */
        {
            uint8_t q, before, after, tries;
            uint16_t scored = 0;

            for (tries = 0; tries < 200; tries++) {
                trek_new_game(3, (uint16_t)(6100 + tries));
                ship.quad_y = 3; ship.quad_x = 3;
                ship.sec_y = 6; ship.sec_x = 4;
                q = (uint8_t)((3 << 3) | 3);
                gal_stars[q] = 4;
                clear_quadrant();
                sector[(3 << 3) | 4] = SEC_STAR;
                ship.torps = 9; ship.shields_up = 0;
                if (trek_fire_torpedo(3, 4, &dmg) == TORP_STAR_GONE) break;
            }
            ok(trek_star_gone(q) == 1, "destroying a star is remembered by the quadrant");
            ok(ship.stars_gone == 1, "and counted against the captain");
            scored = ship.stars_gone;

            /* Leave and come back: the fill must draw one N and three stars. */
            trek_enter_quadrant();
            before = after = 0;
            for (q = 0; q < QUAD_CELLS; q++) {
                if (sector[q] == SEC_NOVA) after++;
                if (sector[q] == SEC_STAR) before++;
            }
            ok(after == 1, "and it is still a nova when the quadrant is rebuilt");
            ok(before == 3, "with the quadrant's other stars intact");

            /* And the score sheet charges for it. */
            {
                ScoreSheet sh;
                ship.stars_gone = scored;
                trek_score_sheet(&sh);
                ok(sh.stars == 1, "the score sheet counts the star");
                ok(sh.star_pts == -5, "at -5, as the rubric says");
            }
        }
        /* fn 0x0A8C8 rolls Random(100) > 95, so four in a hundred. Four
           hundred shots put the expectation at 16; this band is wide enough
           to be stable and narrow enough to fail if the roll is dropped. */
        ok(nova >= 4 && nova <= 34,
              "and about four in a hundred send it supernova");
    }

    /* --- what a supernova does, fn 0x0A8C8 --- */
    {
        int i;
        uint8_t r, got = 0;

        /* The MEASURED throw: a star at sector row 3 (0-based) with the ship
           in quadrant row 6 gives 3 < 6 and 6 < 7, so the ship goes to row 7
           -- which in the original's 1-based numbers is "blown to quad 8-4",
           the line the emulator captured. */
        for (i = 0; i < 4000 && !got; i++) {
            trek_new_game(3, (uint16_t)(1000 + i));
            ship.quad_y = 6; ship.quad_x = 3;
            ship.sec_y = 6; ship.sec_x = 4;
            clear_quadrant();
            sector[(3 << 3) | 4] = SEC_STAR;
            ship.torps = 9; ship.shields_up = 0;
            gal_enemies[(6 << 3) | 3] = 3;
            ship.enemies_left = 10;
            r = trek_fire_torpedo(1, 4, &dmg);
            if (r == TORP_NOVA) got = 1;
        }
        ok(got, "a supernova can be provoked");
        ok(ship.quad_y == 7 && ship.quad_x == 3,
              "and the ship is thrown one quadrant, away from the star");
        ok(gal_nova[(6 << 3) | 3], "the quadrant it left is burnt");
        ok(nova_kills == 3, "every Mongol in it died");
        ok(ship.enemies_left == 7, "and the count came down");
        ok(dmg > 0 && dmg < 600, "the hit is Random(600)");
        ok(!ship.lost, "an ordinary destination is survivable");

        /* THE OTHER DIRECTION, and this is the case that actually
           discriminates. With the star's sector row BELOW the ship's quadrant
           row the second branch fires and the ship goes UP -- whereas the
           first branch and the fallback both give quad_y + 1, so a test that
           only ever exercises those cannot tell them apart. Found by
           inverting the first condition and watching nothing fail. */
        got = 0;
        for (i = 0; i < 4000 && !got; i++) {
            trek_new_game(3, (uint16_t)(1000 + i));
            ship.quad_y = 2; ship.quad_x = 3;
            ship.sec_y = 6; ship.sec_x = 4;
            clear_quadrant();
            sector[(5 << 3) | 4] = SEC_STAR;
            ship.torps = 9; ship.shields_up = 0;
            if (trek_fire_torpedo(1, 4, &dmg) == TORP_NOVA) got = 1;
        }
        ok(got, "a supernova below the ship can be provoked");
        ok(ship.quad_y == 1 && ship.quad_x == 3,
              "a star below the ship's row throws it UP one quadrant");

        /* Thrown into a quadrant that is ALREADY burnt: destroyed. */
        got = 0;
        for (i = 0; i < 4000 && !got; i++) {
            trek_new_game(3, (uint16_t)(1000 + i));
            ship.quad_y = 6; ship.quad_x = 3;
            ship.sec_y = 6; ship.sec_x = 4;
            clear_quadrant();
            sector[(3 << 3) | 4] = SEC_STAR;
            ship.torps = 9; ship.shields_up = 0;
            gal_nova[(7 << 3) | 3] = 1;          /* the destination */
            if (trek_fire_torpedo(1, 4, &dmg) == TORP_NOVA) got = 1;
        }
        ok(got, "a supernova can be provoked again");
        ok(ship.lost, "thrown into a burnt quadrant, the ship is destroyed");
        ok(ship.quad_y == 6, "and it never arrives");
    }

    /* --- THE PLASMA BOLT, BINARY 0x016CF4 and fn 0x1658F --- */
    puts("the plasma bolt");
    {
        TrekEvent ev5[16];
        int i;
        uint8_t k, n5;

        /* FIRED BY WARSHIPS ONLY, and only at level 3 and up. */
        {
            uint16_t fired_by[9];
            uint8_t cls, any_low = 0;
            for (cls = 0; cls < 9; cls++) fired_by[cls] = 0;
            for (cls = SEC_BATTLESHIP; cls <= SEC_SUPPLY; cls++) {
                for (i = 0; i < 900; i++) {
                    trek_new_game(3, (uint16_t)(700 + i));
                    clear_quadrant();
                    ship.sec_y = 4; ship.sec_x = 4;
                    clear_quadrant();
                    sector[(2 << 3) | 2] = (uint8_t)cls;
                    enemy_hp[(2 << 3) | 2] = 400;
                    bolt_cell = QUAD_CELLS;
                    ship.moved = 0; ship.lost = 0;
                    n5 = trek_enemy_turn(ev5, 16, 0);
                    for (k = 0; k < n5; k++)
                        if (ev5[k].kind == EV_BOLT_FIRED) fired_by[cls]++;
                }
            }
            ok(fired_by[SEC_BATTLESHIP] > 0 && fired_by[SEC_COMMAND] > 0,
               "battleships and Commanders fire plasma bolts");
            ok(fired_by[SEC_SCOUT] == 0 && fired_by[SEC_SUPPLY] == 0,
               "scouts and supply ships never do");
            /* Six in a hundred of 900. */
            ok(fired_by[SEC_BATTLESHIP] > 30 && fired_by[SEC_BATTLESHIP] < 90,
               "about six turns in a hundred");

            /* Level 1 and 2 never. */
            for (i = 0; i < 900; i++) {
                trek_new_game(2, (uint16_t)(700 + i));
                clear_quadrant();
                ship.sec_y = 4; ship.sec_x = 4;
                clear_quadrant();
                sector[(2 << 3) | 2] = SEC_BATTLESHIP;
                enemy_hp[(2 << 3) | 2] = 400;
                bolt_cell = QUAD_CELLS; ship.moved = 0; ship.lost = 0;
                n5 = trek_enemy_turn(ev5, 16, 0);
                for (k = 0; k < n5; k++)
                    if (ev5[k].kind == EV_BOLT_FIRED) any_low = 1;
            }
            ok(!any_low, "and never below command level 3");
        }

        /* THE ROLL IS MADE BEFORE THE TYPE TEST -- 0x016CF4 tests level and
           the in-flight flag, rolls, and only THEN asks the class. So the
           stream does not depend on who is in the quadrant. Written the other
           way round first; the class-vs-damage test caught it. */
        {
            uint16_t after_scout, after_battle;
            uint8_t  cls;
            uint16_t seen[2];
            for (cls = 0; cls < 2; cls++) {
                trek_new_game(3, 5150);
                clear_quadrant();
                ship.sec_y = 4; ship.sec_x = 4;
                clear_quadrant();
                sector[(2 << 3) | 2] = cls ? SEC_BATTLESHIP : SEC_SCOUT;
                enemy_hp[(2 << 3) | 2] = 400;
                bolt_cell = QUAD_CELLS; ship.moved = 0; ship.lost = 0;
                ship.shields = 30000; ship.energy = ENERGY_MAX;
                /* RUN THE TURN. An earlier version of this compared the
                   stream WITHOUT running one, so it could not fail. */
                (void)trek_enemy_turn(ev5, 16, 0);
                seen[cls] = trek_rand_n(10000);
            }
            after_scout = seen[0]; after_battle = seen[1];
            ok(after_scout == after_battle,
               "a scout consumes the same draws a battleship does");
        }

        /* ONE IN FLIGHT AT A TIME. Needs a CROWDED quadrant: with a single
           enemy the guard is never reached, which is how it survived the
           first break run untested. Eight battleships at six in a hundred
           would put two bolts in the air about one turn in ten without it. */
        {
            uint8_t most = 0;
            for (i = 0; i < 500; i++) {
                uint8_t c2, this_turn = 0;
                trek_new_game(3, (uint16_t)(6100 + i));
                clear_quadrant();
                ship.sec_y = 4; ship.sec_x = 4;
                clear_quadrant();
                for (c2 = 0; c2 < 8; c2++) {
                    sector[(c2 << 3) | 0] = SEC_BATTLESHIP;
                    enemy_hp[(c2 << 3) | 0] = 400;
                }
                bolt_cell = QUAD_CELLS;
                ship.moved = 0; ship.lost = 0;
                ship.shields = 60000; ship.energy = ENERGY_MAX;
                n5 = trek_enemy_turn(ev5, 16, 0);
                for (k = 0; k < n5; k++)
                    if (ev5[k].kind == EV_BOLT_FIRED) this_turn++;
                if (this_turn > most) most = this_turn;
            }
            ok(most == 1, "eight enemies still put at most ONE bolt in the air");
        }

        /* AIMED WHERE THE SHIP IS, and one at a time. */
        {
            uint8_t placed = 0, twice = 0;
            for (i = 0; i < 900 && !placed; i++) {
                trek_new_game(3, (uint16_t)(2200 + i));
                clear_quadrant();
                ship.sec_y = 3; ship.sec_x = 6;
                clear_quadrant();
                sector[(1 << 3) | 1] = SEC_BATTLESHIP;
                enemy_hp[(1 << 3) | 1] = 400;
                bolt_cell = QUAD_CELLS; ship.moved = 0; ship.lost = 0;
                n5 = trek_enemy_turn(ev5, 16, 0);
                for (k = 0; k < n5; k++)
                    if (ev5[k].kind == EV_BOLT_FIRED) placed = 1;
                if (placed) {
                    ok(bolt_cell == ((3 << 3) | 6),
                       "the bolt takes the ship's CURRENT sector");
                    ok(bolt_quad == ((ship.quad_y << 3) | ship.quad_x),
                       "and remembers the quadrant it was fired in");
                }
            }
            ok(placed, "a bolt was fired");
            (void)twice;
        }

        /* IT TAKES THE SHIELD CHARGE WHOLE, and energy is untouched. */
        {
            uint16_t e0, s0;
            uint8_t got = 0;
            trek_new_game(3, 4242);
            clear_quadrant();
            ship.sec_y = 4; ship.sec_x = 4;
            clear_quadrant();
            bolt_cell = (uint8_t)((4 << 3) | 5);     /* one sector away */
            bolt_quad = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
            ship.shields = 3000; ship.energy = ENERGY_MAX;
            ship.shields_up = 1; ship.moved = 0;
            e0 = ship.energy; s0 = ship.shields;
            n5 = trek_enemy_turn(ev5, 16, 0);
            for (k = 0; k < n5; k++)
                if (ev5[k].kind == EV_BOLT_HIT) {
                    got = 1;
                    ok(ship.energy == e0, "the bolt drains NO energy");
                    ok(s0 - ship.shields == ev5[k].amount,
                       "and the shield charge takes it whole");
                    /* d = 1, so (90..99) * 7 = 630..693. */
                    ok(ev5[k].amount >= 630 && ev5[k].amount <= 693,
                       "at one sector it is (90..99) x 7");
                }
            ok(got, "a bolt one sector away detonates");
            ok(bolt_cell >= QUAD_CELLS, "and it goes off exactly once");
        }

        /* MOVING IS THE COUNTER: damage falls with distance and dies at 8. */
        {
            uint16_t near_hit = 0, far_hit = 0;
            uint8_t d;
            for (d = 1; d <= 7; d += 6) {
                trek_new_game(3, 4242);
                clear_quadrant();
                ship.sec_y = 0; ship.sec_x = 0;
                clear_quadrant();
                bolt_cell = (uint8_t)((0 << 3) | d);
                bolt_quad = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
                ship.shields = 30000; ship.moved = 0;
                n5 = trek_enemy_turn(ev5, 16, 0);
                for (k = 0; k < n5; k++)
                    if (ev5[k].kind == EV_BOLT_HIT) {
                        if (d == 1) near_hit = ev5[k].amount;
                        else        far_hit  = ev5[k].amount;
                    }
            }
            ok(near_hit > 0 && far_hit > 0, "it reaches at one and at seven");
            ok(near_hit > far_hit * 3,
               "and moving away is worth about a hundred a sector");
        }

        /* LEAVING THE QUADRANT LOSES IT -- 0x01659E. */
        {
            trek_new_game(3, 4242);
            clear_quadrant();
            bolt_cell = (uint8_t)((4 << 3) | 4);
            bolt_quad = (uint8_t)(((ship.quad_y ^ 1) << 3) | ship.quad_x);
            ship.shields = 3000; ship.moved = 0;
            n5 = trek_enemy_turn(ev5, 16, 0);
            {
                /* Counted, not asserted INSIDE the loop: written that way
                   first, the loop ran zero times and the check printed
                   nothing at all -- a pass that never happened. */
                uint8_t hits = 0;
                for (k = 0; k < n5; k++)
                    if (ev5[k].kind == EV_BOLT_HIT) hits++;
                ok(hits == 0, "a bolt fired elsewhere never goes off");
            }
            ok(bolt_cell >= QUAD_CELLS, "it is simply lost");
            ok(ship.shields == 3000, "and costs nothing");
        }
    }

    /* --- BLACK HOLES, BINARY 0x016525 and fn 0x0C609 at 0x0D0B6 --- */
    puts("black holes");
    {
        uint16_t withhole = 0, toomany = 0;
        uint8_t  cell, col;
        int      i;

        /* ONE QUADRANT IN FOUR, on every entry. Counted rather than sampled:
           a single look at a quadrant agrees with "never placed" three times
           in four. */
        trek_new_game(3, 40828);
        for (i = 0; i < 1600; i++) {
            uint8_t here = 0;
            ship.quad_y = 3; ship.quad_x = 3;
            gal_enemies[(3 << 3) | 3] = 1;
            trek_enter_quadrant();
            for (cell = 0; cell < QUAD_CELLS; cell++)
                if (sector[cell] == SEC_BLACKHOLE) here++;
            if (here) withhole++;
            if (here > 1) toomany++;
        }
        ok(toomany == 0, "the fill never places more than one hole");
        ok(withhole > 0, "a quadrant can be built with a black hole in it");
        /* 1 in 4 of 1600 is 400. Wide band: this checks the roll is a roll. */
        ok(withhole > 300 && withhole < 500,
           "about one quadrant in four has one");

        /* NOT column-dependent, unlike the pod -- the roll reads nothing. */
        {
            uint16_t seen_in[GAL_DIM];
            uint8_t any_empty = 0;
            for (col = 0; col < GAL_DIM; col++) {
                seen_in[col] = 0;
                for (i = 0; i < 200; i++) {
                    ship.quad_y = 3; ship.quad_x = col;
                    gal_enemies[(3 << 3) | col] = 1;
                    trek_enter_quadrant();
                    for (cell = 0; cell < QUAD_CELLS; cell++)
                        if (sector[cell] == SEC_BLACKHOLE) { seen_in[col]++; break; }
                }
                if (!seen_in[col]) any_empty = 1;
            }
            ok(!any_empty, "and every column gets them, not just column 8");
        }

        /* ENTERING ONE. Put a hole next door and fly into it. */
        {
            uint16_t fatal = 0, thrown = 0;
            for (i = 0; i < 900; i++) {
                uint8_t r;
                trek_new_game(3, (uint16_t)(600 + i));
                ship.quad_y = 3; ship.quad_x = 3;
                trek_enter_quadrant();
                clear_quadrant();
                ship.sec_y = 4; ship.sec_x = 4;
                clear_quadrant();
                sector[(4 << 3) | 5] = SEC_BLACKHOLE;
                ship.impulse = IMPULSE_START; ship.lost = 0; hole_threw = 0;
                r = trek_move_impulse(4, 5);
                if      (r == MOVE_HOLE_LOST)   { fatal++;  ok(ship.lost, "a fatal hole loses the ship"); }
                else if (r == MOVE_HOLE_THROWN) { thrown++; }
                else { ok(0, "flying into a hole gave neither outcome"); break; }
                if (fatal && thrown > 3) break;
            }
            ok(fatal > 0 && thrown > 0, "a hole both kills and throws");
            /* One in five is fatal. Counted over the whole run below. */
        }

        /* THE ODDS, and the throw's reach. */
        {
            uint16_t fatal = 0, moved_quad = 0;
            for (i = 0; i < 1000; i++) {
                uint8_t r, oy, ox;
                trek_new_game(3, (uint16_t)(9000 + i));
                ship.quad_y = 3; ship.quad_x = 3;
                trek_enter_quadrant();
                clear_quadrant();
                ship.sec_y = 4; ship.sec_x = 4;
                clear_quadrant();
                sector[(4 << 3) | 5] = SEC_BLACKHOLE;
                ship.impulse = IMPULSE_START; ship.lost = 0;
                oy = ship.quad_y; ox = ship.quad_x;
                r = trek_move_impulse(4, 5);
                if (r == MOVE_HOLE_LOST) fatal++;
                else if (ship.quad_y != oy || ship.quad_x != ox) moved_quad++;
            }
            /* 200 of 1000. The band would fail at one in four or one in six. */
            ok(fatal > 150 && fatal < 260, "one entry in five is fatal");
            ok(moved_quad > 700,
               "and a survivor is thrown to another quadrant almost always");
        }

        /* A TORPEDO IS SWALLOWED OR TURNED, never simply stopped. */
        {
            uint16_t swallowed = 0, other = 0, dmg;
            uint8_t  r;
            int      k;
            for (k = 0; k < 600; k++) {
                trek_new_game(3, (uint16_t)(3300 + k));
                ship.quad_y = 3; ship.quad_x = 3;
                trek_enter_quadrant();
                clear_quadrant();
                ship.sec_y = 0; ship.sec_x = 0;
                clear_quadrant();
                sector[(3 << 3) | 3] = SEC_BLACKHOLE;
                ship.torps = 10; ship.shields_up = 0;
                ship.boarders = BOARD_NONE;
                r = trek_fire_torpedo(3, 3, &dmg);
                if (r == TORP_SWALLOWED) swallowed++; else other++;
            }
            ok(swallowed > 0, "a torpedo can be swallowed by a black hole");
            ok(other > 0, "and can also survive it -- the shot is deflected");
            /* Aimed AT the hole, so most shots pass near its corner and go
               in; the deflected ones are the wide passes. Both halves have
               to appear or the branch is untested. */
            ok(swallowed > other,
               "aimed straight at one, most are swallowed");
        }

        /* IT IS NOT AN OBSTACLE: the ship ends up IN the cell's quadrant
           somewhere else, not stopped one short of it. */
        {
            uint8_t r, i2, found = 0;
            for (i2 = 0; i2 < 60 && !found; i2++) {
                trek_new_game(3, (uint16_t)(1234 + i2));
                ship.quad_y = 3; ship.quad_x = 3;
                trek_enter_quadrant();
                clear_quadrant();
                ship.sec_y = 0; ship.sec_x = 0;
                clear_quadrant();
                sector[(0 << 3) | 3] = SEC_BLACKHOLE;
                ship.impulse = IMPULSE_START; ship.lost = 0;
                r = trek_move_impulse(0, 6);
                if (r == MOVE_HOLE_THROWN || r == MOVE_HOLE_LOST) {
                    found = 1;
                    ok(1, "a hole in the path is entered, not bumped into");
                    ok(r != MOVE_BLOCKED, "and it never reports MOVE_BLOCKED");
                }
            }
            ok(found, "the hole-in-the-path case was reached");
        }
    }

    /* --- the Vandal Death Pod, BINARY fn 0x20B38 and the 0x15F51 build --- */
    {
        TrekEvent ev[8];
        int i;
        uint8_t seen, n;
        uint16_t lo = 0xFFFF, hi = 0, fired;

        /* PLACED ON EVERY QUADRANT ENTRY, IN COLUMN 8 AND NOWHERE ELSE.
           MEASURED on the original 2026-08-28 and it overturned that day's
           own reading -- see trek.h. Counted over many entries rather than
           sampled once: the roll is four in ten, so a single look at a
           column-8 quadrant agrees with "never placed" three times in five,
           and the test this replaces did exactly that and passed. */
        {
            uint16_t col8 = 0, other = 0, wrong_hp = 0, crowded = 0;
            uint8_t  col, cell;

            trek_new_game(3, 4242);
            for (col = 0; col < GAL_DIM; col++) {
                uint8_t q = (uint8_t)((2 << 3) | col);
                for (i = 0; i < 400; i++) {
                    uint8_t found = QUAD_CELLS;
                    ship.quad_y = 2; ship.quad_x = col;
                    gal_enemies[q] = 1;
                    trek_enter_quadrant();
                    for (cell = 0; cell < QUAD_CELLS; cell++)
                        if (sector[cell] == SEC_POD) { found = cell; break; }
                    if (found == QUAD_CELLS) continue;
                    /* The LITERAL, not POD_HP -- an assertion written in
                       terms of the constant it tests cannot fail when the
                       constant moves, and this one did not until it was
                       spelled out. */
                    if (enemy_hp[found] != 900) wrong_hp++;
                    if (col + 1 == POD_QUAD_COLUMN) col8++; else other++;
                }
            }
            ok(other == 0, "a pod is never placed outside quadrant column 8");
            ok(col8 > 0, "and column 8 does place them");
            ok(wrong_hp == 0, "every pod placed carries 900 points");
            /* Four in ten of 400 entries. Wide band: this checks the roll is
               a roll, not that its rate is exactly 0.4. */
            ok(col8 > 120 && col8 < 240, "about four entries in ten carry one");

            /* Never once the quadrant is crowded. */
            {
                uint8_t q = (uint8_t)((2 << 3) | (POD_QUAD_COLUMN - 1));
                for (i = 0; i < 400; i++) {
                    ship.quad_y = 2; ship.quad_x = POD_QUAD_COLUMN - 1;
                    gal_enemies[q] = POD_MAX_SHIPS;
                    trek_enter_quadrant();
                    for (cell = 0; cell < QUAD_CELLS; cell++)
                        if (sector[cell] == SEC_POD) crowded++;
                }
                ok(crowded == 0, "and never with five ships already there");
            }
        }

        /* ALIVE FROM TURN ONE, PLACED OR NOT. That is the correction the
           object was built for: [0x26E0] is initialised true and the
           detonation reads nothing else. */
        {
            uint8_t always = 1;
            for (i = 0; i < 200; i++) {
                trek_new_game(3, (uint16_t)(2200 + i));
                if (!pod_alive) always = 0;
            }
            ok(always, "the pod is alive from the first turn of every game");
        }

        /* AND RE-ENTERING RE-ROLLS IT. Both outcomes have to appear over
           repeated entries to the SAME quadrant, or the quadrant is not
           being rebuilt. */
        {
            uint8_t cell, saw_pod = 0, saw_none = 0;
            uint8_t q = (uint8_t)((2 << 3) | (POD_QUAD_COLUMN - 1));
            trek_new_game(3, 909);
            for (i = 0; i < 200 && !(saw_pod && saw_none); i++) {
                uint8_t here = 0;
                ship.quad_y = 2; ship.quad_x = POD_QUAD_COLUMN - 1;
                gal_enemies[q] = 1;
                trek_enter_quadrant();
                for (cell = 0; cell < QUAD_CELLS; cell++)
                    if (sector[cell] == SEC_POD) here = 1;
                if (here) saw_pod = 1; else saw_none = 1;
            }
            ok(saw_pod && saw_none,
               "re-entering a column-8 quadrant re-rolls the pod");
        }

        /* AND THE PLACEMENT DOES NOT CONSULT pod_alive. Destroying it
           disarms the detonation for good, but 'R's keep appearing: the
           original's placement reads [0x26DF] and never [0x26E0]. */
        {
            uint8_t cell, seen = 0;
            uint8_t q = (uint8_t)((2 << 3) | (POD_QUAD_COLUMN - 1));
            trek_new_game(3, 5150);
            pod_alive = 0;
            for (i = 0; i < 200 && !seen; i++) {
                ship.quad_y = 2; ship.quad_x = POD_QUAD_COLUMN - 1;
                gal_enemies[q] = 1;
                trek_enter_quadrant();
                for (cell = 0; cell < QUAD_CELLS; cell++)
                    if (sector[cell] == SEC_POD) seen = 1;
            }
            ok(seen, "a destroyed pod does not stop new ones being placed");
        }

        /* THE HIT IS 50..99 -- the port's old band was 40..79, invented
           around one reading of 59. Collect the extremes over many turns. */
        fired = 0;
        for (i = 0; i < 6000; i++) {
            trek_new_game(3, (uint16_t)(3000 + i));
            clear_quadrant();
            pod_alive = 1;
            ship.shields = 60000;          /* survive, so the loop continues */
            n = trek_run_events(ev, 8);
            { uint8_t k; for (k = 0; k < n; k++)
                if (ev[k].kind == EV_POD_HIT) {
                    fired++;
                    if (ev[k].amount < lo) lo = ev[k].amount;
                    if (ev[k].amount > hi) hi = ev[k].amount;
                } }
        }
        ok(fired > 0, "the pod does detonate");
        ok(lo >= 50 && hi <= 99, "its hit lands inside 50..99");
        ok(lo < 55 && hi > 94, "and spans the band, not a corner of it");

        /* It takes the SHIELD CHARGE whole, and nothing reaches energy. */
        {
            uint16_t e0;
            for (i = 0; i < 6000; i++) {
                trek_new_game(3, (uint16_t)(3000 + i));
                clear_quadrant();
                pod_alive = 1;
                ship.shields = 5000; e0 = ship.energy;
                n = trek_run_events(ev, 8);
                { uint8_t k, got = 0; for (k = 0; k < n; k++)
                    if (ev[k].kind == EV_POD_HIT) got = 1;
                  if (got) {
                      ok(ship.shields == (uint16_t)(5000 - ev[0].amount)
                         || ship.shields < 5000, "the shield charge takes it");
                      ok(ship.energy == e0, "and main energy is untouched");
                      break;
                  } }
            }
        }

        /* FLAT SHIELDS AND IT KILLS YOU. */
        for (i = 0; i < 6000; i++) {
            trek_new_game(3, (uint16_t)(3000 + i));
            clear_quadrant();
            pod_alive = 1;
            ship.shields = 0;
            n = trek_run_events(ev, 8);
            { uint8_t k, got = 0; for (k = 0; k < n; k++)
                if (ev[k].kind == EV_POD_HIT) got = 1;
              if (got) { ok(ship.lost, "a detonation on flat shields is fatal");
                         break; } }
        }

        /* A DESTROYED POD NEVER DETONATES AGAIN -- and this is the gate that
           was wrong: it used to read "is the pod in this quadrant". */
        seen = 0;
        for (i = 0; i < 4000; i++) {
            trek_new_game(3, (uint16_t)(3000 + i));
            clear_quadrant();
            pod_alive = 0;
            n = trek_run_events(ev, 8);
            { uint8_t k; for (k = 0; k < n; k++)
                if (ev[k].kind == EV_POD_HIT) seen = 1; }
        }
        ok(!seen, "a destroyed pod never detonates");

        /* AND AN ALIVE ONE DETONATES WHEREVER YOU ARE. Nothing is in the
           quadrant at all here -- no 'R', no ships. */
        seen = 0;
        for (i = 0; i < 4000 && !seen; i++) {
            trek_new_game(3, (uint16_t)(3000 + i));
            clear_quadrant();
            pod_alive = 1;
            ship.quad_y = 3; ship.quad_x = 0;   /* column 1, far from Vandal space */
            ship.shields = 60000;
            n = trek_run_events(ev, 8);
            { uint8_t k; for (k = 0; k < n; k++)
                if (ev[k].kind == EV_POD_HIT) seen = 1; }
        }
        ok(seen, "and an alive one detonates in an empty quadrant in column 1");

        /* IT SPARES ITSELF and hits everything else. */
        {
            uint8_t podcell = (3 << 3) | 3, foecell = (5 << 3) | 5;
            uint8_t hit = 0;
            for (i = 0; i < 6000; i++) {
                trek_new_game(3, (uint16_t)(3000 + i));
                clear_quadrant();
                pod_alive = 1;
                sector[podcell] = SEC_POD;        enemy_hp[podcell] = POD_HP;
                sector[foecell] = SEC_BATTLESHIP; enemy_hp[foecell] = 5000;
                ship.shields = 60000;
                n = trek_run_events(ev, 8);
                { uint8_t k; for (k = 0; k < n; k++)
                    if (ev[k].kind == EV_POD_HIT) hit = 1; }
                if (hit) {
                    ok(enemy_hp[podcell] == POD_HP,
                       "its own detonation spares the pod");
                    ok(enemy_hp[foecell] < 5000,
                       "and takes the same figure off a Mongol");
                    break;
                }
            }
            ok(hit, "the spare-itself case was reached");
        }

        /* A TORPEDO CANNOT TOUCH IT. The shot is spent, the pod is not
           scratched, and it is still alive. */
        {
            uint8_t r, cloaked = 0, tried = 0;
            uint16_t dmg;
            for (i = 0; i < 400 && !cloaked; i++) {
                uint8_t podcell;
                trek_new_game(3, (uint16_t)(4000 + i));
                clear_quadrant();
                ship.sec_y = 0; ship.sec_x = 0;
                clear_quadrant();
                podcell = (2 << 3) | 2;
                sector[podcell] = SEC_POD; enemy_hp[podcell] = POD_HP;
                ship.torps = 10; ship.shields_up = 0;
                ship.boarders = BOARD_NONE;
                tried = 1;
                r = trek_fire_torpedo(2, 2, &dmg);
                if (r == TORP_CLOAKED) {
                    cloaked = 1;
                    ok(ship.torps == 9, "the cloaked torpedo is still spent");
                    ok(enemy_hp[podcell] == POD_HP, "the pod is not scratched");
                    ok(sector[podcell] == SEC_POD, "and it is still there");
                    ok(pod_alive, "and still alive");
                    ok(dmg == 0, "and no damage is reported");
                }
            }
            ok(tried, "a torpedo was fired at a pod");
            ok(cloaked, "and it answered with the cloaking device");
        }

        /* LASERS KILL IT, AND THE KILL IS WORTH NOTHING. */
        {
            uint8_t podcell = (2 << 3) | 2;
            uint16_t dealt, killed0, left0, gal0;
            uint8_t q;

            trek_new_game(3, 4321);
            clear_quadrant();
            ship.sec_y = 0; ship.sec_x = 0;
            clear_quadrant();
            sector[podcell] = SEC_POD; enemy_hp[podcell] = POD_HP;
            ship.boarders = BOARD_NONE;
            ship.energy = 60000; ship.laser_heat = 0;
            for (i = 0; i < SYS_COUNT; i++) ship.sys[i] = 100;
            ok(trek_fire_laser(2, 2, 300, &dealt) == FIRE_OK,
               "a laser can be aimed at the pod");
            ok(enemy_hp[podcell] < POD_HP && enemy_hp[podcell] > 0,
               "and wounds it");

            q = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
            killed0 = ship.killed; left0 = ship.enemies_left;
            gal0 = gal_enemies[q];
            enemy_hp[podcell] = 100;          /* one shot from dead */
            ok(trek_fire_laser(2, 2, 4000, &dealt) == FIRE_KILL,
               "and enough of it destroys the pod");
            ok(sector[podcell] == SEC_EMPTY, "the cell is cleared");
            ok(!pod_alive, "the pod is dead galaxy-wide");
            ok(ship.killed == killed0, "and the kill is not scored");
            ok(ship.enemies_left == left0, "the Mongols left are unchanged");
            ok(gal_enemies[q] == gal0, "and so is the galaxy count");
        }

        /* IT IS NOT AN ENEMY: it never moves and it never fires. */
        {
            uint8_t podcell = (2 << 3) | 2;
            uint8_t moved = 0, fired_at_us = 0;
            uint16_t sh0;
            for (i = 0; i < 400; i++) {
                trek_new_game(3, (uint16_t)(5000 + i));
                clear_quadrant();
                ship.sec_y = 6; ship.sec_x = 6;
                clear_quadrant();
                sector[podcell] = SEC_POD; enemy_hp[podcell] = POD_HP;
                pod_alive = 0;                /* no detonation in the way */
                ship.shields = 60000; sh0 = ship.shields;
                ship.moved = 0;
                (void)trek_enemy_turn(ev, 8, 0);
                if (sector[podcell] != SEC_POD) moved = 1;
                if (ship.shields != sh0 || ship.energy == 0) fired_at_us = 1;
            }
            ok(!moved, "the pod never moves");
            ok(!fired_at_us, "and never fires");
        }
    }

    /* --- REINFORCEMENTS, fn 0x015A4C: level 5 only, column 1 always --- */
    {
        int i;
        uint8_t lv, c, seen, col_wrong = 0, found = 0;
        TrekEvent ev3[8];
        uint8_t n3, j;

        /* BELOW LEVEL 5 THE SLOT IS NEVER SET. This is the discriminator for
           the level gate: a build that scheduled it for everyone passes every
           other assertion here. */
        seen = 0;
        for (lv = 1; lv <= 4; lv++) {
            trek_new_game(lv, (uint16_t)(8000 + lv));
            if (trek_is_scheduled(SCHED_REINFORCE)) seen++;
        }
        ok(seen == 0, "below command level 5 no reinforcements are scheduled");

        trek_new_game(5, 8100);
        ok(trek_is_scheduled(SCHED_REINFORCE),
           "at level 5 they are, and the first check is 12 to 17 days out");
        ok(trek_scheduled(SCHED_REINFORCE) >= STARDATE_START + 120
           && trek_scheduled(SCHED_REINFORCE) <= STARDATE_START + 170,
           "3512 + Random*5, as the reals say");

        /* AND THEY ARRIVE IN COLUMN 1, whatever else is empty. Every other
           column is emptied too, so a build that picked the first empty
           quadrant anywhere would land outside column 0 and fail. */
        for (i = 0; i < 40; i++) {
            trek_new_game(5, (uint16_t)(8200 + i));
            for (c = 0; c < GAL_CELLS; c++) { gal_enemies[c] = 0; gal_nova[c] = 0; }
            trek_schedule(SCHED_REINFORCE, 1);
            n3 = trek_advance(20, ev3, 8);
            for (j = 0; j < n3; j++)
                if (ev3[j].kind == EV_REINFORCE) {
                    found++;
                    if (ev3[j].x != REINF_COLUMN) col_wrong++;
                    if (gal_enemies[(ev3[j].y << 3) | ev3[j].x] < 2
                        || gal_enemies[(ev3[j].y << 3) | ev3[j].x] > 5) col_wrong++;
                }
        }
        ok(found == 40, "the slot fires when its date passes");
        ok(col_wrong == 0, "always in column 1, and two to five ships");
        ok(trek_is_scheduled(SCHED_REINFORCE), "and it reschedules itself");

        /* Nowhere to put them: still rescheduled, nothing reported. */
        trek_new_game(5, 8300);
        for (c = 0; c < GAL_CELLS; c++) gal_enemies[c] = 1;
        trek_schedule(SCHED_REINFORCE, 1);
        n3 = trek_advance(20, ev3, 8);
        for (j = 0, seen = 0; j < n3; j++)
            if (ev3[j].kind == EV_REINFORCE) seen = 1;
        ok(!seen, "with column 1 full nothing arrives");
        ok(trek_is_scheduled(SCHED_REINFORCE), "but the check is still rebooked");
    }

    /* --- WEAR AND TEAR, fn 0x0213F3 --- */
    {
        int i;
        uint8_t j, broke, worst;
        TrekEvent ev4[8];
        uint8_t n4;

        /* NOTHING BREAKS IN THE FIRST THREE DAYS. */
        broke = 0;
        for (i = 0; i < 300; i++) {
            trek_new_game(3, (uint16_t)(8400 + i));
            ship.stardate = 35029;                 /* 3502.9, just inside */
            n4 = trek_advance(0, ev4, 8);
            for (j = 0; j < n4; j++) if (ev4[j].kind == EV_WEAR) broke++;
        }
        ok(broke == 0, "nothing wears out before stardate 3503");

        /* AND NEVER AT COMMAND LEVEL 1. */
        broke = 0;
        for (i = 0; i < 300; i++) {
            trek_new_game(1, (uint16_t)(8700 + i));
            ship.stardate = 35600;
            ship.wear_last = STARDATE_START;
            n4 = trek_advance(0, ev4, 8);
            for (j = 0; j < n4; j++) if (ev4[j].kind == EV_WEAR) broke++;
        }
        ok(broke == 0, "and never at command level 1");

        /* A PERFECT SHIP, LATE, AT LEVEL 3: something does break, and it is
           one of the first six. */
        broke = 0; worst = 0;
        for (i = 0; i < 400; i++) {
            trek_new_game(3, (uint16_t)(9000 + i));
            ship.stardate = 35600;
            ship.wear_last = STARDATE_START;
            n4 = trek_advance(0, ev4, 8);
            for (j = 0; j < n4; j++)
                if (ev4[j].kind == EV_WEAR) {
                    broke++;
                    if (ev4[j].amount > 5) worst++;   /* outside the first six */
                }
        }
        ok(broke > 0, "a perfect ship does break down eventually");
        ok(worst == 0, "and only ever one of the first six systems");

        /* THE 95% TEST, and it is the interesting half: damage something and
           the game stops inventing NEW faults -- only the converter can go. */
        broke = 0; worst = 0;
        for (i = 0; i < 400; i++) {
            trek_new_game(3, (uint16_t)(9500 + i));
            /* A SMALL e, deliberately: e = (level+3) * elapsed, so a long gap
               since the last breakdown makes the first branch a certainty and
               the converter branch unreachable. One stardate gives e = 6. */
            ship.stardate  = 35100;
            ship.wear_last = 35090;
            ship.sys[SYS_SHUTTLE] = 20;            /* average now well under 95 */
            n4 = trek_advance(0, ev4, 8);
            for (j = 0; j < n4; j++)
                if (ev4[j].kind == EV_WEAR) {
                    broke++;
                    if (ev4[j].amount != SYS_CONVERTER) worst++;
                }
        }
        ok(worst == 0, "a ship already damaged only ever loses the converter");
        ok(broke > 0, "and it still loses it sometimes");
    }

    /* --- WARP STRAIN: speed AND distance, fn 0x0C609 at 0x0D623 --- */
    {
        int i;
        uint8_t hurt, clear_all;
        uint16_t before;

        /* A long jump needs an empty galaxy to land in, and the strain is
           read off sys[SYS_WARP] rather than off any message. */
        #define SETUP_JUMP(w)                                                \
            do {                                                             \
                trek_new_game(3, (uint16_t)(7000 + i));                      \
                for (clear_all = 0; clear_all < GAL_CELLS; clear_all++) {    \
                    gal_enemies[clear_all]   = 0;                            \
                    gal_commander[clear_all] = 0;                            \
                    gal_stars[clear_all]     = 0;                            \
                    gal_base[clear_all]      = BASE_NONE;                    \
                }                                                            \
                ship.quad_y = 0; ship.quad_x = 0;                            \
                ship.sec_y = 0;  ship.sec_x = 0;                             \
                trek_enter_quadrant();                                       \
                /* AND CLEAR WHAT ENTERING PLACED. trek_enter_quadrant()      \
                   drops the death pod and rolls a black hole on EVERY entry, \
                   regardless of the galaxy arrays above, and either one in    \
                   the departure path blocks the jump -- which showed up as    \
                   the strain "not firing" on a fraction of the seeds. */      \
                clear_quadrant();                                            \
                ship.energy = ENERGY_MAX;                                    \
                ship.sys[SYS_WARP] = 100;                                    \
                ship.warp = (w);                                             \
            } while (0)

        /* BELOW WARP 8, NOTHING BREAKS -- however far you go. This is the
           assertion the 2026-08-28 read would have failed: it recorded the
           rule as distance-only and "warp appears nowhere". */
        hurt = 0;
        for (i = 0; i < 200; i++) {
            SETUP_JUMP(79);                      /* warp 7.9 */
            before = ship.sys[SYS_WARP];
            trek_move_warp(4, 0, 0, 0);          /* exactly 4.0 quadrants */
            if (ship.sys[SYS_WARP] != before) hurt++;
        }
        ok(hurt == 0, "below warp 8 the engines never strain, at any distance");

        /* AT WARP 8 AND BEYOND 4 QUADRANTS, THEY ALWAYS DO. (d-1.5)/2.5 is
           past 1 from 4.0 quadrants, and 0,0 -> 7,7 is 7 diagonal. */
        hurt = 0;
        for (i = 0; i < 200; i++) {
            SETUP_JUMP(80);                      /* warp 8.0 exactly */
            before = ship.sys[SYS_WARP];
            trek_move_warp(4, 0, 0, 0);
            if (ship.sys[SYS_WARP] < before) hurt++;
        }
        ok(hurt == 200, "at warp 8 a jump past four quadrants always strains");

        /* THE MIDDLE OF THE BAND, which is the only place the ROLL is live.
           Everything above tests the two ends -- certain past 4.0, impossible
           below 1.5 -- and INVERTING the comparison passed all of it, because
           neither end reaches the roll. Found by breaking it on purpose.

           2.5 quadrants gives (2.5 - 1.5) / 2.5 = 0.4 exactly. In 600 jumps
           that is 240 +- 12 (one sd), so the band below is four sd wide and
           the inverted rule, at 0.6, lands 120 outside it. */
        {
            int damaged = 0;
            for (i = 0; i < 600; i++) {
                SETUP_JUMP(80);
                before = ship.sys[SYS_WARP];
                trek_move_warp(2, 0, 4, 0);      /* 20 sectors = 2.5 quadrants */
                if (ship.sys[SYS_WARP] < before) damaged++;
            }
            ok(damaged > 190 && damaged < 290,
               "at 2.5 quadrants the engines strain about two times in five");
        }

        /* AND A SHORT HOP NEVER DOES, even at the ceiling. One quadrant is
           1.0, and (1.0 - 1.5)/2.5 is negative -- which is the ten-hop
           measurement of 2026-08-28, as a test. */
        hurt = 0;
        for (i = 0; i < 200; i++) {
            SETUP_JUMP(WARP_MAX);
            before = ship.sys[SYS_WARP];
            trek_move_warp(0, 1, 0, 0);          /* one quadrant east */
            if (ship.sys[SYS_WARP] != before) hurt++;
        }
        ok(hurt == 0, "one quadrant never strains, even at warp 10");

        /* THE LOSS IS 10 PLUS UP TO TEN PER QUADRANT, so a jump of exactly
           four quadrants loses 10..50. The band is written as LITERALS, not
           as WARP_DMG_BASE + something * WARP_RISK_SPAN -- an assertion in
           terms of the constants it tests cannot fail when they are wrong. */
        {
            uint8_t lo = 255, hi = 0;
            for (i = 0; i < 300; i++) {
                SETUP_JUMP(WARP_MAX);
                before = ship.sys[SYS_WARP];
                trek_move_warp(4, 0, 0, 0);
                {
                    uint8_t got = (uint8_t)(before - ship.sys[SYS_WARP]);
                    if (got < lo) lo = got;
                    if (got > hi) hi = got;
                }
            }
            ok(lo >= 10, "over four quadrants the loss is at least ten");
            ok(hi <= 50, "and at most ten plus ten a quadrant");
            ok(hi > lo, "the loss is a roll, not a constant");
            ok(lo < 20 && hi > 40, "and it spans most of that band");
        }

        /* IT COMES OFF THE WARP ENGINES AND NOTHING ELSE. */
        for (i = 0; i < 1; i++) {
            uint8_t j, others_moved = 0;
            uint8_t snap[SYS_COUNT];
            SETUP_JUMP(WARP_MAX);
            for (j = 0; j < SYS_COUNT; j++) snap[j] = ship.sys[j];
            trek_move_warp(4, 0, 0, 0);
            for (j = 0; j < SYS_COUNT; j++)
                if (j != SYS_WARP && ship.sys[j] != snap[j]) others_moved++;
            ok(others_moved == 0, "and it comes off SYS_WARP alone");
            ok(warp_hurt > 0, "the UI is told how much was lost");
        }
        #undef SETUP_JUMP
    }

    /* --- HAIL, BINARY fn 0x207FD --- */
    {
        int i;
        uint8_t r, qy, qx, blocked = 0, responds = 0, silent = 0, wrong = 0;

        /* A StarBase two quadrants away answers; the roll blocks one in five,
           so both outcomes must appear and nothing else may. */
        for (i = 0; i < 600; i++) {
            trek_new_game(3, (uint16_t)(200 + i));
            { uint8_t c; for (c = 0; c < GAL_CELLS; c++) gal_base[c] = BASE_NONE; }
            ship.quad_y = 4; ship.quad_x = 4;
            gal_base[(4 << 3) | 6] = BASE_STARBASE;     /* two columns away */
            qy = qx = 0xFF;
            r = trek_hail(&qy, &qx);
            if      (r == HAIL_BLOCKED)  blocked++;
            else if (r == HAIL_RESPONDS) { responds++;
                                           if (qy != 4 || qx != 6) wrong++; }
            else silent++;
        }
        ok(blocked > 0, "one hail in five is blocked by interference");
        ok(responds > 0, "and a StarBase in range answers");
        ok(silent == 0, "a base at two quadrants is never out of range");
        ok(wrong == 0, "and the reply names the right quadrant");

        /* Three quadrants away is beyond 2.0 and does NOT answer. */
        silent = responds = 0;
        for (i = 0; i < 600; i++) {
            trek_new_game(3, (uint16_t)(200 + i));
            { uint8_t c; for (c = 0; c < GAL_CELLS; c++) gal_base[c] = BASE_NONE; }
            ship.quad_y = 4; ship.quad_x = 4;
            gal_base[(4 << 3) | 7] = BASE_STARBASE;     /* three columns away */
            r = trek_hail(&qy, &qx);
            if (r == HAIL_RESPONDS) responds++;
            if (r == HAIL_SILENT)   silent++;
        }
        ok(responds == 0, "a base at three quadrants is out of range");
        ok(silent > 0, "and the answer is silence, not a block every time");

        /* Only a StarBase answers -- a research station in the same cell
           must not. This is the discriminator for the type test. */
        responds = 0;
        for (i = 0; i < 600; i++) {
            trek_new_game(3, (uint16_t)(200 + i));
            { uint8_t c; for (c = 0; c < GAL_CELLS; c++) gal_base[c] = BASE_NONE; }
            ship.quad_y = 4; ship.quad_x = 4;
            gal_base[(4 << 3) | 5] = BASE_RESEARCH;
            gal_base[(4 << 3) | 3] = BASE_SUPPLY;
            if (trek_hail(&qy, &qx) == HAIL_RESPONDS) responds++;
        }
        ok(responds == 0, "neither a research station nor a supply depot answers");

        /* --- THE DELAYED REPLY, and its GATING is the whole point ---
         *
         * Read 2026-08-29 off the branches, not the address order: BOTH the
         * blocked path (0x0209A3) and the in-range path (0x020A11) JUMP OVER
         * the scheduling code at 0x020A65. Only "No response." falls into it.
         * So these three cases are the discriminator -- a build that schedules
         * on every hail passes nothing here but the third. */
        {
            uint8_t  qy2, qx2, c, tries;
            uint16_t due;
            TrekEvent ev2[8];
            uint8_t  n2, j, seen;

            /* IN RANGE: answers now, so there is nothing to wait for. */
            responds = 0; wrong = 0;
            for (i = 0; i < 400; i++) {
                trek_new_game(3, (uint16_t)(900 + i));
                for (c = 0; c < GAL_CELLS; c++) gal_base[c] = BASE_NONE;
                ship.quad_y = 4; ship.quad_x = 4;
                gal_base[(4 << 3) | 6] = BASE_STARBASE;      /* d^2 = 4 */
                if (trek_hail(&qy2, &qx2) != HAIL_RESPONDS) continue;
                responds++;
                if (trek_is_scheduled(SCHED_HAIL)) wrong++;
            }
            ok(responds > 0, "a base in range still answers on the spot");
            ok(wrong == 0, "and an answer NOW schedules no delayed reply");

            /* BLOCKED: nothing went out, so nothing comes back. */
            blocked = 0; wrong = 0;
            for (i = 0; i < 400; i++) {
                trek_new_game(3, (uint16_t)(1300 + i));
                for (c = 0; c < GAL_CELLS; c++) gal_base[c] = BASE_NONE;
                ship.quad_y = 4; ship.quad_x = 4;
                gal_base[(4 << 3) | 7] = BASE_STARBASE;      /* out of range */
                if (trek_hail(&qy2, &qx2) != HAIL_BLOCKED) continue;
                blocked++;
                if (trek_is_scheduled(SCHED_HAIL)) wrong++;
            }
            ok(blocked > 0, "interference still blocks one hail in five");
            ok(wrong == 0, "and a blocked hail schedules no reply either");

            /* OUT OF RANGE: this is the branch that schedules.
               THE DELAYS ARE LITERALS, not the formula -- an assertion
               written as sqrt(25*d)-5 would agree with a wrong sqrt. Three
               distances, each solved by hand from (v - 1.0) * 0.5:
                 d^2 = 5  -> v 2.236 -> 0.618 -> 6 tenths
                 d^2 = 9  -> v 3.000 -> 1.000 -> 10 tenths
                 d^2 = 32 -> v 5.657 -> 2.328 -> 23 tenths          */
            {
                static const struct { uint8_t y, x; uint16_t tenths; } far_base[] = {
                    { 5, 6,  6 },      /* dy 1, dx 2 */
                    { 4, 7, 10 },      /* dy 0, dx 3 */
                    { 0, 0, 23 }       /* the corner of the +-4 box */
                };
                uint8_t b;
                for (b = 0; b < 3; b++) {
                    char msg[72];
                    uint16_t got = SCHED_NEVER, t0 = 0;
                    for (tries = 0; tries < 60; tries++) {
                        trek_new_game(3, (uint16_t)(2000 + tries + b * 97));
                        for (c = 0; c < GAL_CELLS; c++) gal_base[c] = BASE_NONE;
                        ship.quad_y = 4; ship.quad_x = 4;
                        gal_base[(far_base[b].y << 3) | far_base[b].x] =
                            BASE_STARBASE;
                        t0 = ship.stardate;
                        if (trek_hail(&qy2, &qx2) != HAIL_SILENT) continue;
                        got = trek_scheduled(SCHED_HAIL);
                        break;
                    }
                    sprintf(msg, "a base at %u,%u is answered in %u tenths",
                            far_base[b].y, far_base[b].x, far_base[b].tenths);
                    ok(got == (uint16_t)(t0 + far_base[b].tenths), msg);
                }
            }

            /* AND IT ARRIVES, naming the base that answered. */
            due = SCHED_NEVER;
            for (tries = 0; tries < 60; tries++) {
                trek_new_game(3, (uint16_t)(3000 + tries));
                for (c = 0; c < GAL_CELLS; c++) gal_base[c] = BASE_NONE;
                ship.quad_y = 4; ship.quad_x = 4;
                gal_base[(4 << 3) | 7] = BASE_STARBASE;
                if (trek_hail(&qy2, &qx2) != HAIL_SILENT) continue;
                due = trek_scheduled(SCHED_HAIL);
                break;
            }
            ok(due != SCHED_NEVER, "an out-of-range hail is waiting on a reply");

            n2 = trek_advance(5, ev2, 8);
            for (j = 0, seen = 0; j < n2; j++)
                if (ev2[j].kind == EV_HAIL_REPLY) seen = 1;
            ok(!seen, "the reply does not arrive before its date");

            n2 = trek_advance(20, ev2, 8);
            for (j = 0, seen = 0; j < n2; j++)
                if (ev2[j].kind == EV_HAIL_REPLY) {
                    seen = 1;
                    ok(ev2[j].y == 4 && ev2[j].x == 7,
                       "and it names the StarBase that answered");
                }
            ok(seen, "the reply arrives once the clock passes it");
            ok(!trek_is_scheduled(SCHED_HAIL), "and the slot goes back to never");

            /* THE BASE MUST STILL BE THERE -- 0x020689 re-reads the chart
               word before printing. Take the base away while the signal is in
               flight and the answer never comes. Without that re-check this
               fires and names a quadrant with no base in it. */
            for (tries = 0; tries < 60; tries++) {
                trek_new_game(3, (uint16_t)(4000 + tries));
                for (c = 0; c < GAL_CELLS; c++) gal_base[c] = BASE_NONE;
                ship.quad_y = 4; ship.quad_x = 4;
                gal_base[(4 << 3) | 7] = BASE_STARBASE;
                if (trek_hail(&qy2, &qx2) == HAIL_SILENT) break;
            }
            gal_base[(4 << 3) | 7] = BASE_NONE;        /* the Mongols took it */
            n2 = trek_advance(40, ev2, 8);
            for (j = 0, seen = 0; j < n2; j++)
                if (ev2[j].kind == EV_HAIL_REPLY) seen = 1;
            ok(!seen, "a base destroyed in flight never answers");
            ok(!trek_is_scheduled(SCHED_HAIL),
               "and the slot is cleared rather than left pending for ever");
        }

        /* IT COSTS NO TIME. Twenty-five HAILs under dosbox-automation left
           the stardate at 3500.00; the "0.1 stardates" this asserted came
           from a claim retracted in 2026-08-24 that survived in trek.h. */
        trek_new_game(3, 4242);
        { uint16_t t0 = ship.stardate;
          (void)trek_hail(&qy, &qx);
          ok(ship.stardate == t0, "hailing costs no time at all"); }

        /* And so does docking, which had only INDIRECT cover -- removing
           trek_advance_time(1) from both sites failed the hail case and not
           the dock one. One line, and the pair is honest now. */
        trek_new_game(3, 4242);
        gal_base[(ship.quad_y << 3) | ship.quad_x] = BASE_STARBASE;
        sector[0] = SEC_BASE; ship.sec_y = 0; ship.sec_x = 1;
        { uint16_t t0 = ship.stardate;
          ok(trek_dock() == DOCK_OK, "we dock");
          ok(ship.stardate == (uint16_t)(t0 + 1), "and docking costs 0.1 too"); }

    /* --- THE DEATH RAY, BINARY fn 0x07375 --- */
    {
        int i;
        uint8_t  r;
        /* uint16_t: 1200 rolls put two of these near 400 and a
           uint8_t wrapped, which read as the odds being wrong. */
        uint16_t worked = 0, misfire = 0, mutants = 0, fatal = 0, holes = 0;

        /* Refused with nothing to shoot at, and it costs no turn. */
        trek_new_game(3, 77);
        clear_quadrant();
        { uint16_t t0 = ship.stardate;
          ok(trek_fire_ray() == RAY_NO_TARGET, "with no enemy the ray is refused");
          ok(ship.stardate == t0, "and refusing costs no time"); }

        /* SIX ROLLS OVER FOUR HANDLERS: 1/6, 2/6, 1/6, 2/6. */
        trek_new_game(3, 77);
        for (i = 0; i < 1200; i++) {
            clear_quadrant();
            sector[(2 << 3) | 2] = SEC_BATTLESHIP;
            enemy_hp[(2 << 3) | 2] = 500;
            ship.enemies_left = 40;
            ship.lost = 0; ship.mutants = 0;
            r = trek_fire_ray();
            if      (r == RAY_WORKED)  worked++;
            else if (r == RAY_MISFIRE) misfire++;
            else if (r == RAY_MISFIRE_HOLES) holes++;
            else if (r == RAY_MUTANTS) mutants++;
            else if (r == RAY_FATAL)   fatal++;
        }
        ok(worked + misfire + holes + mutants + fatal == 1200,
           "every shot has an outcome");
        /* FIVE outcomes over six rolls now, not four: rolls 1 and 3 print the
           same line but only roll 3 fills the quadrant with black holes, so
           the two "misfires" are counted apart. 200 each except the fatal
           pair; bands wide enough to be stable and narrow enough to fail if
           any handler were given the wrong share. */
        ok(worked  > 140 && worked  < 265, "it works one time in six");
        ok(misfire > 140 && misfire < 265, "a plain misfire one time in six");
        ok(holes   > 140 && holes   < 265, "and a hole-making one, also one in six");
        ok(mutants > 140 && mutants < 265, "mutants one time in six");
        ok(fatal   > 320 && fatal   < 480, "and it kills you one time in THREE");
        ok(fatal > mutants, "the fatal pair really is twice as likely");
        ok(misfire + holes > 320 && misfire + holes < 480,
           "and the two misfires together are still two in six");

        /* What each outcome does. */
        trek_new_game(3, 77);
        for (i = 0; i < 400; i++) {
            clear_quadrant();
            sector[(2 << 3) | 2] = SEC_BATTLESHIP;
            enemy_hp[(2 << 3) | 2] = 500;
            sector[(5 << 3) | 5] = SEC_COMMAND;
            enemy_hp[(5 << 3) | 5] = 900;
            ship.enemies_left = 40; ship.lost = 0; ship.mutants = 0;
            r = trek_fire_ray();
            if (r == RAY_WORKED) {
                ok(sector[(2 << 3) | 2] == SEC_EMPTY
                   && sector[(5 << 3) | 5] == SEC_EMPTY,
                      "a working ray kills EVERY enemy in the quadrant");
                ok(ship.enemies_left == 38, "and both come off the count");
                break;
            }
        }
        trek_new_game(3, 77);
        for (i = 0; i < 400; i++) {
            clear_quadrant();
            sector[(2 << 3) | 2] = SEC_BATTLESHIP;
            enemy_hp[(2 << 3) | 2] = 500;
            ship.lost = 0; ship.mutants = 0;
            if (trek_fire_ray() == RAY_FATAL) {
                ok(ship.lost, "the unstable apparatus destroys the ship");
                break;
            }
        }
        trek_new_game(3, 77);
        for (i = 0; i < 400; i++) {
            clear_quadrant();
            sector[(2 << 3) | 2] = SEC_BATTLESHIP;
            enemy_hp[(2 << 3) | 2] = 500;
            ship.lost = 0; ship.mutants = 0;
            if (trek_fire_ray() == RAY_MUTANTS) {
                ok(ship.mutants, "and the mutants are a state the ship carries");
                break;
            }
        }

        /* ROLL 1 CHANGES NOTHING. Roll 3 prints the same line and fills the
           quadrant with black holes, which is why they are separate results
           now -- this assertion used to say "both variants are cosmetic" and
           was passing while being false about half the cases it named. */
        {
            uint8_t saw1 = 0, saw3 = 0;
            for (i = 0; i < 600 && !(saw1 && saw3); i++) {
                uint8_t c, before_empty = 0, after_holes = 0;
                trek_new_game(3, (uint16_t)(77 + i));
                clear_quadrant();
                sector[(2 << 3) | 2] = SEC_BATTLESHIP;
                enemy_hp[(2 << 3) | 2] = 500;
                ship.enemies_left = 40; ship.lost = 0; ship.mutants = 0;
                for (c = 0; c < QUAD_CELLS; c++)
                    if (sector[c] == SEC_EMPTY) before_empty++;
                r = trek_fire_ray();
                for (c = 0; c < QUAD_CELLS; c++)
                    if (sector[c] == SEC_BLACKHOLE) after_holes++;

                if (r == RAY_MISFIRE && !saw1) {
                    saw1 = 1;
                    ok(sector[(2 << 3) | 2] == SEC_BATTLESHIP && !ship.lost
                       && !ship.mutants && ship.enemies_left == 40
                       && after_holes == 0,
                       "roll 1 misfires and changes nothing at all");
                }
                if (r == RAY_MISFIRE_HOLES && !saw3) {
                    saw3 = 1;
                    ok(after_holes > 0,
                       "roll 3 misfires and fills the quadrant with holes");
                    /* HALF, not "not all". `after_holes < before_empty` was
                       the first version and it could not fail: at a 99%
                       chance per cell one survivor still satisfies it. The
                       flip is per cell, so with ~60 empty cells the count is
                       binomial(60, 0.5) -- mean 30, sd 3.9 -- and a quarter
                       to three quarters is nearly four sd either way. */
                    ok(after_holes * 4 > before_empty
                       && after_holes * 4 < before_empty * 3,
                       "about half the empty cells -- a coin flip each");
                    ok(sector[(ship.sec_y << 3) | ship.sec_x] == SEC_SHIP,
                       "and never the cell the ship is standing in");
                }
            }
            ok(saw1 && saw3, "both misfire rolls were reached");
        }

        /* The mutants clear one turn in ten and report otherwise. */
        {
            TrekEvent ev4[8];
            uint16_t turns = 0;
            uint8_t k, n4, reports = 0;
            trek_new_game(3, 77);
            ship.mutants = 1;
            while (ship.mutants && turns < 500) {
                n4 = trek_run_events(ev4, 8);
                for (k = 0; k < n4; k++)
                    if (ev4[k].kind == EV_MUTANTS) reports++;
                turns++;
            }
            ok(!ship.mutants, "the mutants do eventually clear");
            ok(reports > 0, "and they file a report every turn until they do");
        }
    }
    }

    /* The level scales the damage, which is the same expression as a
       battleship's hit points -- one torpedo kills one battleship at EVERY
       level, by construction. */
    trek_new_game(5, 777);
    ship.sec_y = 4; ship.sec_x = 4;
    clear_quadrant();
    sector[(2 << 3) | 4] = SEC_COMMAND;
    enemy_hp[(2 << 3) | 4] = 60000;
    ship.torps = 9; ship.shields_up = 0;
    r = trek_fire_torpedo(2, 4, &dmg);
    ok(r == TORP_OK && dmg == 385, "at level 5 a close hit does 385");

    /* Empty space: the torpedo flies past and off the grid. Same answer as
       before, for a completely different reason. */
    trek_new_game(3, 777);
    ship.sec_y = 4; ship.sec_x = 4;
    clear_quadrant();
    ship.shields_up = 0;
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

    /* THE RATE TERM IS GATED ON THE CLOCK, NOT ON FINISHING -- fn 0x1DD4F at
       0x01E270 tests `stardate < 3503.0` and nothing else. The 0.00 the
       original printed against two kills in 1.2 stardates was that gate, not
       the unfinished mission it was attributed to. */
    trek_new_game(3, 4242);
    ship.enemies_left = 3;
    ship.killed = 2;
    ship.stardate = (uint16_t)(STARDATE_START + 12);   /* 1.2 stardates */
    ok(trek_score() == 2 * 10 - 300,
       "under three stardates the rate is nothing, as the original printed");

    /* Same 1.2 stardates, mission FINISHED: still nothing. The old model
       credited this and the original does not. */
    trek_new_game(3, 4242);
    ship.enemies_left = 0;
    ship.killed = 2;
    ship.stardate = (uint16_t)(STARDATE_START + 12);
    ok(trek_score() == 2 * 10,
       "and finishing early does not unlock it -- the gate is the clock");

    /* Four stardates, mission UNFINISHED: the original PAYS. This is the
       half the port had backwards -- a captain who ran out of time still
       scores for the ships he killed. */
    trek_new_game(3, 4242);
    ship.enemies_left = 3;
    ship.killed = 2;
    ship.stardate = (uint16_t)(STARDATE_START + 40);   /* 4 stardates */
    ok(trek_score() == 2 * 10 - 300 + 250,
       "an unfinished mission still earns the rate term");

    trek_new_game(3, 4242);
    ship.enemies_left = 0;
    ship.killed = 2;
    ship.stardate = (uint16_t)(STARDATE_START + 100);  /* 10 stardates */
    ok(trek_score() == 2 * 10 + 100,
       "2 kills in 10 stardates is 0.2/day, worth 100");

    /* No division by zero at the boundary, and none below it. */
    trek_new_game(3, 4242);
    ship.enemies_left = 0;
    ship.killed = 2;
    ship.stardate = STARDATE_START;                    /* no time at all */
    ok(trek_score() == 2 * 10, "no elapsed time scores nothing, and does not divide by zero");

    trek_new_game(3, 4242);
    ship.enemies_left = 0;
    ship.killed = 2;
    ship.stardate = (uint16_t)(STARDATE_START + 30);   /* exactly 3.0 */
    ok(trek_score() == 2 * 10 + 333, "and 3.0 exactly is the first stardate that pays");

    /* RESCUES ARE FORFEIT WITH THE SHIP. The binary credits them in the
       `survived` arm of the same branch that applies the -200. */
    trek_new_game(3, 4242);
    ship.enemies_left = 0;
    ship.rescues = 2;
    /* The literal, not the constant -- an assertion written in terms of the
       #define it is testing moves with it and checks nothing. */
    ok(trek_score() == 400, "a surviving captain is paid 200 a rescue");
    trek_new_game(3, 4242);
    ship.enemies_left = 0;
    ship.rescues = 2;
    ship.lost = 1;
    ship.casualties = 0;
    ok(trek_score() == -200,
       "and a dead one is not -- they go with the ship");

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

        /* A focus that does NOT finish spends the whole day: the others get
           nothing. Lasers at 0 climb 60 in a stardate and stop short. */
        trek_new_game(3, 1234);
        ship.sys[SYS_LASERS] = 0;
        ship.sys[SYS_SHIELDS] = 50;
        ship.repair_focus = SYS_LASERS + 1;
        { TrekEvent ev[8]; (void)trek_advance(10, ev, 8); }
        focused = ship.sys[SYS_LASERS];
        ok(focused == 60, "an unfinished focus mends 60 in a stardate");
        ok(ship.sys[SYS_SHIELDS] == 50,
              "and the others get nothing -- the focus took the whole clock");
        ok(ship.repair_focus == SYS_LASERS + 1,
              "an unfinished focus stays set");

        /* A focus that DOES finish hands the leftover time back. Lasers at 50
           reach 100 with 10 points to spare, which is 0.1 stardates at the
           original's 0.01-a-point scale; the remaining 0.9 mends everything
           at 20 a stardate. Literals, not the constants under test. */
        trek_new_game(3, 1234);
        ship.sys[SYS_LASERS] = 50;
        ship.sys[SYS_SHIELDS] = 50;
        ship.repair_focus = SYS_LASERS + 1;
        { TrekEvent ev[8]; (void)trek_advance(10, ev, 8); }
        ok(ship.sys[SYS_LASERS] == 100, "a finished focus reaches exactly 100");
        ok(ship.sys[SYS_SHIELDS] == 68,
              "and the leftover 0.9 stardates mends the rest by 18");
        ok(ship.repair_focus == 0, "a finished focus clears itself");

        /* BINARY: the docked rate is gated on the StarBase-only flag
           [0x26DA], which docking CLEARS for types 2 and 3. A research
           station repairs at the undocked 20 a stardate, not 50. */
        trek_new_game(3, 1234);
        ship.sys[SYS_SHIELDS] = 0;
        ship.docked = BASE_RESEARCH;
        ship.repair_focus = 0;
        { TrekEvent ev[8]; (void)trek_advance(10, ev, 8); }
        ok(ship.sys[SYS_SHIELDS] == 20,
              "a research station gives the UNDOCKED repair rate");

        trek_new_game(3, 1234);
        ship.sys[SYS_SHIELDS] = 0;
        ship.docked = BASE_STARBASE;
        ship.repair_focus = 0;
        { TrekEvent ev[8]; (void)trek_advance(10, ev, 8); }
        ok(ship.sys[SYS_SHIELDS] == 50,
              "and only a StarBase gives the docked 50");
        (void)plain;
    }

    /* --- the life support reserve, BINARY 0x02042E --- */
    {
        TrekEvent ev[8];
        uint8_t n;

        trek_new_game(3, 1234);
        ok(ship.life_reserve == 20, "the reserve starts at 2.0 stardates");

        /* Perfect life support: no drain. */
        (void)trek_advance(10, ev, 8);
        ok(ship.life_reserve == 20, "a healthy life support does not drain it");

        /* 90..99 is the band where the PANEL has swapped and the countdown
           has not started. One threshold would lose this.
           ONE TENTH, not ten: repair runs BEFORE the drain check, and at 20
           points a stardate a full day lifts 95 clean past 100, so the
           original ten-tenth version of this test passed under BOTH
           thresholds and proved nothing. 92 + 2 = 94 stays in the band. */
        trek_new_game(3, 1234);
        ship.sys[SYS_LIFE] = 92;
        (void)trek_advance(1, ev, 8);
        ok(ship.sys[SYS_LIFE] == 94, "the repair leaves it inside the band");
        ok(ship.life_reserve == 20,
              "between 90 and 99 the reserve still does not drain");

        /* Below 90, undocked: it drains by the elapsed time. */
        trek_new_game(3, 1234);
        ship.sys[SYS_LIFE] = 50;
        (void)trek_advance(5, ev, 8);
        ok(ship.life_reserve == 15, "below 90 it drains by the elapsed time");

        /* Docked at ANY base, it does not drain. */
        trek_new_game(3, 1234);
        ship.sys[SYS_LIFE] = 50;
        ship.docked = BASE_RESEARCH;
        (void)trek_advance(10, ev, 8);
        ok(ship.life_reserve == 20, "docked, it does not drain at all");

        /* A repair focus does NOT slow the drain -- the original subtracts
           the elapsed time it saved BEFORE the focus rewrote it. */
        trek_new_game(3, 1234);
        ship.sys[SYS_LIFE] = 50;
        ship.sys[SYS_LASERS] = 0;
        ship.repair_focus = SYS_LASERS + 1;
        (void)trek_advance(10, ev, 8);
        ok(ship.life_reserve == 10,
              "a repair focus does not slow the drain");

        /* Exactly exhausted is NOT death: the original dies below zero. */
        trek_new_game(3, 1234);
        ship.sys[SYS_LIFE] = 50;
        ship.life_reserve = 5;
        (void)trek_advance(5, ev, 8);
        ok(ship.life_reserve == 0, "spending the last of it reaches nought");
        ok(!ship.lost, "and nought is not yet death");

        /* One more tick past nought is. */
        n = trek_advance(1, ev, 8);
        ok(ship.lost, "the tick past nought loses the ship");
        ok(n >= 1 && ev[0].kind == EV_LIFE_GONE, "and it is reported");
        ok(trek_game_state() == GAME_LOST, "the mission is over");

        /* Docking at a RESEARCH STATION refills it -- its only gift. */
        trek_new_game(3, 1234);
        ship.sys[SYS_LIFE] = 50;
        ship.life_reserve = 3;
        gal_base[(ship.quad_y << 3) | ship.quad_x] = BASE_RESEARCH;
        sector[0] = SEC_BASE;
        ship.sec_y = 0; ship.sec_x = 1;
        ok(trek_dock() == DOCK_OK, "we can dock at a research station");
        ok(ship.life_reserve == 20, "and it refills the reserve to 2.0");

        /* The canister. Refused at perfect life support, spent otherwise. */
        trek_new_game(3, 1234);
        ok(trek_life_replenish() == 0, "the canister is refused off reserve");
        ship.sys[SYS_LIFE] = 50;
        ship.life_reserve = 4;
        ok(trek_life_replenish() == 1, "and accepted on reserve");
        ok(ship.life_reserve == 14, "adding one whole stardate");
        ship.life_reserve = 15;
        (void)trek_life_replenish();
        ok(ship.life_reserve == 20, "clamped at the same 2.0 ceiling");
    }

    /* --- the Mongol boarding party, BINARY fn 0x15D6E --- */
    {
        TrekEvent ev[8];
        uint16_t t;
        uint8_t n, seen, cell;

        /* Level 3 is never boarded: `cmp [0x1DF0],7 / jg`, and [0x1DF0] is
           level + 4. Five hundred turns with every other gate open. */
        trek_new_game(3, 4321);
        cell = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
        gal_enemies[cell] = 2;
        ship.shields_up = 0;
        seen = 0;
        for (t = 0; t < 500; t++) {
            ship.boarders = BOARD_NONE;
            (void)trek_run_events(ev, 8);
            if (ship.boarders != BOARD_NONE) seen = 1;
        }
        ok(!seen, "level 3 is never boarded");

        /* Level 4 is, and only with the shields down. */
        trek_new_game(4, 4321);
        cell = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
        gal_enemies[cell] = 2;
        ship.shields_up = 1;
        seen = 0;
        for (t = 0; t < 500; t++) {
            ship.boarders = BOARD_NONE;
            (void)trek_run_events(ev, 8);
            if (ship.boarders != BOARD_NONE) seen = 1;
        }
        ok(!seen, "raised shields keep them out");

        ship.shields_up = 0;
        seen = 0;
        for (t = 0; t < 500; t++) {
            ship.boarders = BOARD_NONE;
            (void)trek_run_events(ev, 8);
            if (ship.boarders != BOARD_NONE) seen = 1;
        }
        ok(seen, "with the shields down at level 4 they do board");

        /* An empty quadrant keeps them out -- they come off a ship. */
        trek_new_game(4, 4321);
        cell = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
        gal_enemies[cell] = 0;
        ship.shields_up = 0;
        seen = 0;
        for (t = 0; t < 500; t++) {
            ship.boarders = BOARD_NONE;
            (void)trek_run_events(ev, 8);
            if (ship.boarders != BOARD_NONE) seen = 1;
        }
        ok(!seen, "an empty quadrant keeps them out");

        /* What each department costs. Set directly: the roll is 4% and the
           effect is what is being tested, not the odds. */
        trek_new_game(4, 4321);
        ship.boarders = BOARD_ENGINEERING;
        ship.shields_up = 0;
        ok(trek_shields_up() == SHIELD_BOARDED,
              "engineering held: shields cannot be raised");
        ok(!ship.shields_up, "and they stay down");

        ship.boarders = BOARD_LASERS;
        ok(trek_fire_laser(0, 0, 100, 0) == FIRE_BOARDED,
              "laser control held: the lasers will not fire");

        ship.boarders = BOARD_TUBES;
        ok(trek_fire_torpedo(0, 0, 0) == TORP_BOARDED,
              "entorp control held: the tubes will not fire");
        ok(ship.torps == TORPS_START, "and no torpedo is spent");

        /* Holding one department does not block the others. */
        ship.boarders = BOARD_TUBES;
        ship.shields_up = 0;
        ok(trek_shields_up() == SHIELD_OK,
              "holding the tubes does not stop the shields");

        /* They are thrown off past the deadline, and it is reported. */
        trek_new_game(4, 4321);
        ship.boarders = BOARD_LASERS;
        ship.board_until = ship.stardate;      /* not yet PAST it */
        n = trek_run_events(ev, 8);
        ok(ship.boarders == BOARD_LASERS, "on the deadline they are still aboard");
        ship.board_until = (uint16_t)(ship.stardate - 1);
        n = trek_run_events(ev, 8);
        ok(ship.boarders == BOARD_NONE, "past it they are eliminated");
        ok(n >= 1 && ev[0].kind == EV_BOARDERS_GONE, "and security says so");
    }

    /* --- the spontaneous supernova, BINARY fn 0x1ED00 --- */
    {
        TrekEvent ev[8];
        uint16_t t;
        uint8_t i, k, n, novas = 0, in_row = 0, target, off_target = 0;

        /* Stars everywhere, so the only thing that can steer the choice is
           the row rule. */
        trek_new_game(3, 999);
        for (t = 0; t < 8000; t++) {
            for (i = 0; i < GAL_CELLS; i++) { gal_stars[i] = 1; gal_nova[i] = 0; }
            n = trek_run_events(ev, 8);
            for (k = 0; k < n; k++)
                if (ev[k].kind == EV_NOVA) {
                    novas++;
                    if (ev[k].y == ship.quad_y) in_row++;
                }
        }
        ok(novas > 0, "supernovae do occur");
        ok(in_row == 0,
              "and NEVER in the ship's own quadrant row -- eight of them");

        /* A star is required. Exactly one quadrant has one, off our row. */
        trek_new_game(3, 999);
        target = (uint8_t)((((ship.quad_y + 3) & 7) << 3) | 2);
        novas = 0;
        for (t = 0; t < 8000; t++) {
            for (i = 0; i < GAL_CELLS; i++) { gal_stars[i] = 0; gal_nova[i] = 0; }
            gal_stars[target] = 1;
            n = trek_run_events(ev, 8);
            for (k = 0; k < n; k++)
                if (ev[k].kind == EV_NOVA) {
                    novas++;
                    if ((uint8_t)((ev[k].y << 3) | ev[k].x) != target) off_target++;
                }
        }
        ok(novas > 0, "a starless galaxy but for one quadrant still novas");
        ok(off_target == 0, "and only ever in the quadrant that has the star");

        /* It kills what is there, and it can finish the mission. */
        trek_new_game(3, 999);
        target = (uint8_t)((((ship.quad_y + 3) & 7) << 3) | 2);
        for (i = 0; i < GAL_CELLS; i++) { gal_stars[i] = 0; gal_nova[i] = 0;
                                          gal_enemies[i] = 0; }
        gal_stars[target]   = 1;
        gal_enemies[target] = 5;
        ship.enemies_left   = 5;
        base_under_attack   = target;
        for (t = 0; t < 8000 && ship.enemies_left; t++)
            (void)trek_run_events(ev, 8);
        ok(ship.enemies_left == 0, "the supernova takes the Mongols with it");
        ok(gal_enemies[target] == 0, "the quadrant is emptied");
        ok(gal_nova[target] == 1, "and marked burnt");
        ok(gal_known[target] == 1, "the chart learns it without a scan");
        ok(base_under_attack == GAL_CELLS, "a siege there is called off");
        ok(trek_game_state() == GAME_WON, "and it can WIN the mission");

        /* A SUPERNOVA IN THE SETTLED PLANET'S QUADRANT ENDS THE SETTLEMENT.
           The original clears [0x1E1C]/[0x1E1E] and sets the evacuation slot
           to never -- that pair is the settled planet, which the supernova
           write-up first mistook for a base. */
        {
            TrekEvent ev2[8];
            uint16_t t2;
            uint8_t k, sq = 0xFF;
            trek_new_game(3, 999);
            for (k = 0; k < planet_count; k++)
                if (planets[k].flags & PF_SETTLED) sq = planets[k].quad;
            ok(sq != 0xFF, "a galaxy has a settled planet");

            /* Put the ship somewhere that cannot be the settled row, so the
               row exclusion does not stop the nova landing there. */
            ship.quad_y = (uint8_t)(((sq >> 3) + 4) & 7);
            ship.quad_x = 0;
            for (k = 0; k < GAL_CELLS; k++) { gal_stars[k] = 0; gal_nova[k] = 0; }
            gal_stars[sq] = 1;
            for (t2 = 0; t2 < 8000 && !gal_nova[sq]; t2++)
                (void)trek_run_events(ev2, 8);
            ok(gal_nova[sq], "the settled quadrant can be burnt");
            for (k = 0; k < planet_count; k++)
                if (planets[k].quad == sq)
                    ok(!(planets[k].flags & PF_SETTLED),
                          "and the settlement goes with it");
        }
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

    /* THE RATIO OF POOL DRAIN TO ENERGY DRAIN, MEASURED ON THE ORIGINAL
       2026-08-28. This is the absorption half of the 0.8, and it is the one
       assertion here that a wrong placement of that constant cannot survive.

       Null volley on the original -- fire zero, which costs no energy and
       still gives the enemy its turn -- with the twelve systems, both pools,
       the enemy's hit points and the number of firers all pinned, and the
       death pod disarmed at [0x26E0] so a detonation could not land in the
       shield reading. Twenty-four clean turns:

           charge 1000   S/E = 0.5333        (n=5,  four decimal places)
           charge 2000   S/E = 3.1995..3.2003 (n=19)

       The binary's placement predicts 0.8f/(1-f) = 0.5333 and 3.2000. The
       alternative -- the 0.8 on the protection rather than the drain --
       predicts 0.6667 and 4.0000, and is excluded by 25% at both charges.

       Asserted as a RATIO because it is independent of hit points, range and
       the random band, which is what let the measurement ignore all three. */
    {
        uint16_t c, expect_num[2] = { 8, 32 }, expect_den[2] = { 15, 10 };
        uint16_t charges[2] = { 1000, 2000 };
        uint8_t  k;

        for (k = 0; k < 2; k++) {
            uint16_t e0, s0, drain, through;
            trek_new_game(3, 4711);
            clear_quadrant();
            ship.shields_up = 1;
            ship.shields    = charges[k];
            ship.energy     = ENERGY_MAX;
            ship.sys[SYS_SHIELDS] = 100;
            s0 = ship.shields; e0 = ship.energy;

            {
                TrekEvent dev[8]; uint8_t dn = 0;
                trek_take_hit(1000, dev, &dn, 8);
            }

            drain   = (uint16_t)(s0 - ship.shields);
            through = (uint16_t)(e0 - ship.energy);
            /* drain/through == expect_num/expect_den, cross-multiplied so
               the check is exact rather than a float comparison. 8/15 is
               0.5333 and 32/10 is 3.2. */
            c = (uint16_t)(drain * expect_den[k]);
            ok(c == (uint16_t)(through * expect_num[k]),
               charges[k] == 1000
               ? "charge 1000: the pool loses 0.5333 of what energy loses"
               : "charge 2000: the pool loses 3.2000 of what energy loses");
        }
    }

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
    /* THE POOL LOSES FOUR FIFTHS, not the whole hit. This assertion used to
       read `SHIELD_MAX - 409`, which was an over-reading of the measurement:
       MEASURED.md says "the PRINTED FIGURE comes out of the shield pool
       entirely and main energy is untouched", and fn 0x16844 shows the
       printed figure is 0.8 x the protection. The clean part of that reading
       -- energy untouched -- is asserted below and still holds. */
    ok(ship.shields == SHIELD_MAX - (409 * SHIELD_ABSORB_NUM) / SHIELD_ABSORB_DEN,
       "up and full: the pool loses four fifths of the hit");
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

/* SYSTEM DAMAGE, rewritten 2026-08-26 against fn 0x0213AD and fn 0x020DCE.
   The old version drove trek_take_hit() and asserted "about three times in
   five", which was the observable shadow of a per-turn loop of
   `Round(hits/350) + 1` rounds at two-thirds each. It is that loop these test
   now, because that is what the binary does. Statistical, so they run many
   turns and check the shape. */
static void test_system_damage_severity(void) {
    TrekEvent ev[16];
    uint8_t n, i, j;
    int wrecked = 0, dented = 0, turns_with_two = 0, turns_damaged = 0;
    int hits_small = 0, hits_big = 0, up_empty = 0, down = 0;

    puts("a system hit is usually annihilation (BINARY)");

    /* ONE game and one RNG stream, sampled many times. An earlier version
       called trek_new_game() inside the loop with sequential seeds and read
       the first few draws of each -- which correlates, and read a share that
       said the generator was biased when the TEST was. */
    trek_new_game(3, 4242);
    ship.shields_up = 0;              /* let it all through */
    for (i = 0; i < 200; i++) {
        uint8_t hits = 0;
        for (j = 0; j < SYS_COUNT; j++) ship.sys[j] = 100;
        ship.energy = ENERGY_MAX;
        ship.lost = 0;
        n = 0;
        /* The measured turn: 860 units across three hits. */
        trek_take_hit(430, ev, &n, 16);
        trek_take_hit(430, ev, &n, 16);
        trek_combat_damage(ev, &n, 16);
        for (j = 0; j < n; j++)
            if (ev[j].kind == EV_SYSTEM_HIT) {
                hits++;
                if (ev[j].amount == 0) wrecked++; else dented++;
            }
        if (hits) turns_damaged++;
        if (hits >= 2) turns_with_two++;
    }

    /* 860 units is Round(860/350) + 1 = 3 rounds, each sparing the ship one
       time in three, so a quiet turn is (1/3)^3 -- rare, not the usual case
       the old test asserted. */
    ok(turns_damaged > 200 * 4 / 5,
       "860 units of hits damages something on nearly every turn");
    /* MEASURED: zero eight times in eleven. That is not a special case in the
       original, it is what `hits * 0.5 / (2..5)` does to a 0..100 scale at
       this size -- the amount clears 100 for most of the divisor's range. */
    ok(wrecked * 3 > dented * 5,
       "the result is 0% far more often than not (measured 8 in 11)");
    /* And TWO systems in a turn, seen twice in the original, is ordinary at
       three rounds rather than the rarity the old model made of it. */
    ok(turns_with_two > turns_damaged / 3,
       "two systems in one turn is common at 860 units, not a rarity");

    /* THE COUNT SCALES WITH THE TURN'S TOTAL. This is the discriminator
       against the old per-hit model, which had no such dependence: the number
       of rounds is the only place the hit size enters the count. */
    trek_new_game(3, 4243);
    ship.shields_up = 0;
    for (i = 0; i < 100; i++) {
        for (j = 0; j < SYS_COUNT; j++) ship.sys[j] = 100;
        ship.energy = ENERGY_MAX; ship.lost = 0;
        n = 0; trek_take_hit(200, ev, &n, 16); trek_combat_damage(ev, &n, 16);
        for (j = 0; j < n; j++) if (ev[j].kind == EV_SYSTEM_HIT) hits_small++;

        for (j = 0; j < SYS_COUNT; j++) ship.sys[j] = 100;
        ship.energy = ENERGY_MAX; ship.lost = 0;
        n = 0; trek_take_hit(1400, ev, &n, 16); trek_combat_damage(ev, &n, 16);
        for (j = 0; j < n; j++) if (ev[j].kind == EV_SYSTEM_HIT) hits_big++;
    }
    ok(hits_big > hits_small * 2,
       "a 1400-unit turn wrecks far more than a 200-unit one");

    /* RAISED BUT EMPTY SHIELDS ARE THE WORST PLACE TO BE -- the factor is
       1.25 with them up and flat against 0.5 with them down. Counter-
       intuitive enough that it is worth a test that would catch the sign
       being flipped back. */
    trek_new_game(3, 4244);
    for (i = 0; i < 100; i++) {
        for (j = 0; j < SYS_COUNT; j++) ship.sys[j] = 100;
        ship.energy = ENERGY_MAX; ship.lost = 0; ship.shields = 0;
        ship.shields_up = 1;
        n = 0; trek_take_hit(300, ev, &n, 16); trek_combat_damage(ev, &n, 16);
        for (j = 0; j < n; j++)
            if (ev[j].kind == EV_SYSTEM_HIT && ev[j].amount == 0) up_empty++;

        for (j = 0; j < SYS_COUNT; j++) ship.sys[j] = 100;
        ship.energy = ENERGY_MAX; ship.lost = 0; ship.shields = 0;
        ship.shields_up = 0;
        n = 0; trek_take_hit(300, ev, &n, 16); trek_combat_damage(ev, &n, 16);
        for (j = 0; j < n; j++)
            if (ev[j].kind == EV_SYSTEM_HIT && ev[j].amount == 0) down++;
    }
    ok(up_empty > down,
       "raised but empty shields wreck MORE than no shields at all");

    /* Round(Sign(hits - 500)) * Random(10): under 500 in a turn costs nobody. */
    trek_new_game(3, 4245);
    ship.shields_up = 0;
    for (i = 0; i < 100; i++) {
        for (j = 0; j < SYS_COUNT; j++) ship.sys[j] = 100;
        ship.energy = ENERGY_MAX; ship.lost = 0;
        n = 0; trek_take_hit(400, ev, &n, 16); trek_combat_damage(ev, &n, 16);
    }
    ok(ship.casualties == 0,
       "a turn that never passed 500 units costs no lives");

    /* The shield SYSTEM wears from what the POOL stopped, graded rather than
       wrecked: 1500 absorbed is (1500-700)/10 = 80 points. */
    /* SEED-ROBUST, and it was not: this asserted == 20 on one seed, and the
       turn's own random damage rounds can wreck the shield system a SECOND
       time, taking it to 0. Adding a Random() call to trek_enter_quadrant for
       the black holes shifted the stream and the assertion failed -- on a
       mechanism it was not testing. Find a turn whose rounds left the shield
       system alone, and check the WEAR there. */
    {
        uint16_t seed;
        uint8_t clean = 0, k;
        for (seed = 4246; seed < 4346 && !clean; seed++) {
            trek_new_game(3, seed);
            ship.shields_up = 1;
            ship.shields = SHIELD_MAX; ship.energy = ENERGY_MAX;
            ship.sys[SYS_SHIELDS] = 100;
            n = 0; trek_take_hit(1500, ev, &n, 16);
            if (seed == 4246)
                ok(ship.sys[SYS_SHIELDS] == 100,
                   "the shield system is untouched until the turn is resolved");
            trek_combat_damage(ev, &n, 16);
            clean = 1;
            for (k = 0; k < n; k++)
                if (ev[k].kind == EV_SYSTEM_HIT && ev[k].y == SYS_SHIELDS)
                    clean = 0;
            if (clean)
                ok(ship.sys[SYS_SHIELDS] == 20,
                   "1500 absorbed takes the shield system to 20%, not to zero");
        }
        ok(clean, "and a turn without a second hit on the shields was found");
    }
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
    planets[0].cls   = PCLASS_M;
    planets[0].find  = find;
    planets[0].flags = 0;
    sector[planets[0].sec] = SEC_PLANET;
    ship.orbiting = PLANET_NONE;
    ship.shields_up = 0;
}

static void test_planet_generation(void) {
    uint16_t seed, doubled_up = 0;
    uint8_t  lo = 255, hi = 0, i, j, bad_name = 0, bad_class = 0;
    uint16_t en_cls[3] = {0,0,0}, en_hit[3] = {0,0,0};

    puts("planet generation");

    for (seed = 1; seed < 401; seed++) {
        trek_new_game(3, seed);
        if (planet_count < lo) lo = planet_count;
        if (planet_count > hi) hi = planet_count;

        for (i = 0; i < planet_count; i++) {
            en_cls[planets[i].cls]++;
            if (planets[i].find == PFIND_ENERGIUM) en_hit[planets[i].cls]++;
            /* The NAME is derived from the quadrant column, not stored, so
               what there is to check is that every column maps to a name. */
            if (PLANET_NAME_OF(planets[i].quad) == 0) bad_name++;
            if (planets[i].cls  >= PCLASS_COUNT)  bad_class++;
            for (j = (uint8_t)(i + 1); j < planet_count; j++)
                if (planets[j].quad == planets[i].quad) doubled_up++;
        }
    }

    /* DERIVED from the ancestor: five to ten, and the one galaxy we have read
       held five. Both ends have to be reachable or the model is not this one. */
    ok(lo == PLANET_MIN, "the smallest galaxy holds PLANET_MIN planets");
    ok(hi == PLANET_MIN + PLANET_SPREAD - 1,
       "and the largest holds nineteen");

    ok(bad_name == 0,  "every planet's column maps to a name");
    ok(bad_class == 0, "every planet's class is M, N or O");

    /* THE FIND DEPENDS ON THE CLASS, and that is the assertion worth making
       -- a flat distribution passes an aggregate test and would be wrong.
       Class M should carry energium four times in five and class O twice. */
    printf("  energium by class M/N/O: %u%% %u%% %u%%  (want 80/60/40)\n",
           (unsigned)(en_cls[0] ? 100u * en_hit[0] / en_cls[0] : 0),
           (unsigned)(en_cls[1] ? 100u * en_hit[1] / en_cls[1] : 0),
           (unsigned)(en_cls[2] ? 100u * en_hit[2] / en_cls[2] : 0));
    ok(100u * en_hit[0] / en_cls[0] > 100u * en_hit[1] / en_cls[1] &&
       100u * en_hit[1] / en_cls[1] > 100u * en_hit[2] / en_cls[2],
       "energium is likelier on M than N, and on N than O");
    ok(100u * en_hit[0] / en_cls[0] >= 76 && 100u * en_hit[0] / en_cls[0] <= 84,
       "class M carries energium about four times in five");

    /* EXACTLY ONE settled planet per galaxy, and it is always an energium
       one -- the original overwrites a single pair of globals inside the
       energium branch, so the last such planet wins. */
    {
        uint16_t sd; uint8_t bad_count = 0, bad_kind = 0;
        for (sd = 1; sd < 200; sd++) {
            uint8_t k, c = 0;
            trek_new_game(3, sd);
            for (k = 0; k < planet_count; k++)
                if (planets[k].flags & PF_SETTLED) {
                    c++;
                    if (planets[k].find != PFIND_ENERGIUM) bad_kind++;
                }
            if (c != 1) bad_count++;
        }
        ok(bad_count == 0, "every galaxy has exactly one settled planet");
        ok(bad_kind == 0,  "and it always carries energium");
    }

    /* MEASURED: the generator retries an occupied quadrant, so two planets
       never share one. This port allowed collisions until 2026-08-26. */
    ok(doubled_up == 0, "no two planets share a quadrant");
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
    one_planet(PFIND_ENERGIUM, 0, 1);
    planets[0].flags |= PF_SETTLED;   /* settlers are a FLAG, not a find */
    trek_orbit();
    ok(trek_land(LAND_BY_TRANSPORTER, 0) == LAND_SETTLERS, "settlers are found");
    ok(ship.rescues == 1, "and evacuating them counts as a rescue");

    /* A ruined settlement scans the same and rescues nobody. */
    trek_new_game(3, 912);
    ship.sec_y = 4; ship.sec_x = 4;
    one_planet(PFIND_NOTHING, 0, 1);
    planets[0].flags |= PF_SETTLED;
    planet_evac_end = 0;      /* the deadline has passed */
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
    /* A Mongol station hands over supplies -- BINARY 0x00E4A3, which calls
       the capture and returns. It is not an ambush; the ambush is a separate
       roll made on EVERY landing. */
    ok(trek_land(LAND_BY_TRANSPORTER, &cas) == LAND_SUPPLIES,
       "a Mongol supply station gives up its supplies");
    ok((planets[0].flags & PF_TAKEN) == 0,
       "the station is still there on the next visit");

    /* THE ATTACK IS GENERAL, and it does not replace the find. Land on a
       BARREN world until the roll comes up: the outcome is still the find,
       and the casualties come back through the out-param. */
    {
        int i;
        uint8_t got_att = 0, got_find = 0;
        for (i = 0; i < 4000 && !got_att; i++) {
            trek_new_game(3, (uint16_t)(880 + i));
            ship.sec_y = 4; ship.sec_x = 4;
            one_planet(PFIND_NOTHING, 0, 1);
            trek_orbit();
            cas = 0;
            if (trek_land(LAND_BY_TRANSPORTER, &cas) == LAND_NOTHING
                && cas != 0) { got_att = 1; got_find = 1; }
        }
        ok(got_att, "a landing party can be attacked on a barren world");
        ok(got_find, "and the find is still reported alongside");
        ok(cas >= LANDING_CASUALTY_MIN &&
           cas <= LANDING_CASUALTY_MIN + LANDING_CASUALTY_SPAN - 1,
           "the casualties are 2..6");
        ok(ship.casualties == cas, "and land on the ship's tally, which scores");
    }

    /* The capture itself: one in four gives nothing, and any success always
       carries two life support supplies. */
    {
        int i;
        uint16_t none = 0, some = 0;
        uint8_t  r2, bad_life = 0, bad_raw = 0;

        /* ONE seed for the whole run. Reseeding per iteration measures the
           seeding, not the distribution -- the first draw after a fresh seed
           is not uniform across consecutive seeds, and an earlier version of
           this test failed for that reason and not for the code's. */
        trek_new_game(3, 5000);
        for (i = 0; i < 2000; i++) {
            uint8_t k;
            for (k = 0; k < ITEM_COUNT; k++) inventory[k] = 0;
            r2 = trek_capture_supplies();
            if (r2 == 0) { none++; continue; }
            some++;
            if (!(r2 & (1u << ITEM_LIFE_SUPPORT))
                || inventory[ITEM_LIFE_SUPPORT] != CAPTURE_LIFE_SUPPORT)
                bad_life++;
            if (r2 & (1u << ITEM_RAW_ENERGIUM)) bad_raw++;
        }
        if (none < 400 || none > 600) printf("    (none %u of 2000)\n", none);
        ok(none > 400 && none < 600, "one capture in four yields nothing");
        ok(some > 1400, "and the rest yield something");
        ok(bad_life == 0, "every success carries two life support supplies");
        ok(bad_raw == 0, "and raw energium is never captured");
    }
}

static void test_energium(void) {
    uint16_t seed, good = 0, defective = 0, dud = 0, before, e0;
    uint8_t  r;

    puts("USE -- raw energium");

    trek_new_game(3, 700);
    ship.energy = 100; ship.shields = 100;
    ok(trek_use_energium(0, 0) == USE_NO_ITEM, "with no crystal there is nothing to load");

    /* The gate is the manual's, to the unit: shields under 50% AND main
       energy under 20%. Both boundaries, both directions. */
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

    /* THE ODDS ARE THE BINARY'S: Random(6), roll 0 defective, 1..2 dud,
       3..5 good. Half of crystals work. This is the assertion that would
       have caught what shipped -- 5% escalating and a 10% dud, both
       invented -- so it is written against the proportions and not against
       "all three occur". */
    trek_new_game(3, 701);
    for (seed = 1; seed < 601; seed++) {
        trek_srand(seed);
        ship.energy  = 500;
        ship.shields = 300;
        inventory[ITEM_RAW_ENERGIUM] = 1;
        r = trek_use_energium(0, 0);
        if      (r == USE_GOOD)      good++;
        else if (r == USE_DEFECTIVE) defective++;
        else if (r == USE_DUD)       dud++;
    }
    printf("  good %u  dud %u  defective %u of %u (want 50/33/17)\n",
           good, dud, defective, (unsigned)(good + dud + defective));
    ok(good * 100u / 600u >= 46 && good * 100u / 600u <= 54,
       "a crystal works three times in six");
    ok(dud * 100u / 600u >= 29 && dud * 100u / 600u <= 38,
       "duds two times in six");
    ok(defective * 100u / 600u >= 13 && defective * 100u / 600u <= 21,
       "and is defective one time in six");

    /* A defective crystal takes ENERGY. It does not wreck the converter,
       which is what this port did on the strength of the message. */
    trek_srand(3);
    ship.energy = 900; ship.shields = 100;
    for (r = 0; r < SYS_COUNT; r++) ship.sys[r] = 100;
    inventory[ITEM_RAW_ENERGIUM] = 20;
    while (trek_use_energium(0, 0) != USE_DEFECTIVE) {
        ship.energy = 900; ship.shields = 100;
        inventory[ITEM_RAW_ENERGIUM] = 20;
    }
    ok(ship.energy < 900, "a defective crystal costs energy");
    ok(ship.sys[SYS_CONVERTER] == 100, "and damages no system");

    /* A good crystal overcharges past ENERGY_MAX, and TOPS UP the shields
       rather than filling them -- a badly damaged pool does not come back
       full from one crystal. */
    trek_srand(9);
    ship.energy = 500; ship.shields = 0;
    inventory[ITEM_RAW_ENERGIUM] = 40;
    while (trek_use_energium(0, 0) != USE_GOOD) {
        ship.energy = 500; ship.shields = 0;
        inventory[ITEM_RAW_ENERGIUM] = 40;
    }
    ok(ship.energy > ENERGY_MAX, "a good crystal takes energy above ENERGY_MAX");
    e0 = (uint16_t)(CRYSTAL_V * CRYSTAL_ENERGY_BASE);
    ok(ship.energy >= 500 + e0 &&
       ship.energy <  500 + e0 + CRYSTAL_V * CRYSTAL_ENERGY_SPAN,
       "by V x (700 + rand 700), the binary's own expression");
    ok(ship.shields > 0 && ship.shields <= SHIELD_MAX,
       "and tops the shields up without setting them full");

    /* THE CONVERTER MUST NOT CONFISCATE THE OVERCHARGE. */
    before = ship.energy;
    trek_advance_time(1);
    ok(ship.energy == before,
       "a passing turn does not confiscate energy above the maximum");
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
    test_enemy_shot_band();
    test_tractor();
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
