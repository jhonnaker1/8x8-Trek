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
    ok(d_max * 25UL <= 65535UL,
       "warp time numerator d*25 fits uint16");
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
    expect = 12 + 3 * 4;

    for (i = 0; i < GAL_CELLS; i++) {
        enemies += gal_enemies[i];
        if (gal_base[i] != BASE_NONE) bases++;
        if (gal_stars[i] < 1 || gal_stars[i] > 8) starless++;
    }

    ok(enemies == expect,       "enemy total matches the level formula");
    ok(ship.enemies_left == (uint16_t)expect, "enemies_left agrees with the galaxy");
    ok(bases >= 2 && bases <= 4, "between two and four bases placed");
    ok(starless == 0,            "every quadrant has 1..8 stars");

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

int main(void) {
    test_distance();
    test_no_16bit_overflow();
    test_generation();
    test_determinism();
    test_quadrant_contents();
    test_reveal();
    test_impulse();
    test_warp();

    puts("");
    if (failures) {
        printf("%d FAILURE(S)\n", failures);
        return 1;
    }
    puts("all core tests pass");
    return 0;
}
