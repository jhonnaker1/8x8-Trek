#include "trek.h"

uint8_t gal_enemies[GAL_CELLS];
uint8_t gal_base[GAL_CELLS];
uint8_t gal_stars[GAL_CELLS];
uint8_t gal_known[GAL_CELLS];
uint8_t sector[QUAD_CELLS];
uint16_t enemy_hp[QUAD_CELLS];
Ship    ship;

/* ---------------------------------------------------------------- random */

static uint16_t rng_state = 1;

void trek_srand(uint16_t seed) {
    rng_state = seed ? seed : 1;   /* xorshift is dead at zero */
}

/* 16-bit xorshift. Shifts only, no multiply -- the 6502 has no multiplier,
   and this runs on every object placed during galaxy generation. */
uint16_t trek_rand(void) {
    rng_state ^= (uint16_t)(rng_state << 7);
    rng_state ^= (uint16_t)(rng_state >> 9);
    rng_state ^= (uint16_t)(rng_state << 8);
    return rng_state;
}

uint8_t trek_rand_n(uint8_t n) {
    if (n == 0) return 0;
    return (uint8_t)(trek_rand() % n);
}

/* -------------------------------------------------------------- distance */

/* sqrt(dx^2 + dy^2) * 256, for dy/dx each 0..7. Precomputed because the
   only distances this game ever needs are between cells of an 8x8 grid --
   64 possibilities -- so a runtime sqrt would be pure waste. Max entry is
   2534 (sqrt(98) * 256), comfortably inside uint16. */
static const uint16_t dist_tab[64] = {
        0,  256,  512,  768, 1024, 1280, 1536, 1792,
      256,  362,  572,  810, 1056, 1305, 1557, 1810,
      512,  572,  724,  923, 1145, 1379, 1619, 1864,
      768,  810,  923, 1086, 1280, 1493, 1717, 1950,
     1024, 1056, 1145, 1280, 1448, 1639, 1846, 2064,
     1280, 1305, 1379, 1493, 1639, 1810, 1999, 2202,
     1536, 1557, 1619, 1717, 1846, 1999, 2172, 2360,
     1792, 1810, 1864, 1950, 2064, 2202, 2360, 2534,
};

uint16_t trek_dist(uint8_t dy, uint8_t dx) {
    if (dy > 7) dy = 7;
    if (dx > 7) dx = 7;
    return dist_tab[(dy << 3) | dx];
}

static uint8_t abs_diff(uint8_t a, uint8_t b) {
    return (uint8_t)(a > b ? a - b : b - a);
}

/* ------------------------------------------------------------- the chart */

/* Long range scanners cover the adjacent quadrants (manual l.276), and the
   computer keeps every past scan, so entering a quadrant reveals the 3x3
   block around it permanently. */
static void reveal_around(uint8_t qy, uint8_t qx) {
    uint8_t y, x;
    for (y = (uint8_t)(qy ? qy - 1 : 0); y <= (uint8_t)(qy + 1) && y < GAL_DIM; y++) {
        for (x = (uint8_t)(qx ? qx - 1 : 0); x <= (uint8_t)(qx + 1) && x < GAL_DIM; x++) {
            gal_known[(y << 3) | x] = 1;
        }
    }
}

/* ---------------------------------------------------------- new galaxy */

/* Picks a random free sector and returns its index, or 0xFF if the quadrant
   is somehow full. Bounded retries rather than a scan, since a quadrant
   holds at most a handful of objects out of 64 cells. */
static uint8_t free_sector(void) {
    uint8_t tries, i;
    for (tries = 0; tries < 100; tries++) {
        i = trek_rand_n(QUAD_CELLS);
        if (sector[i] == SEC_EMPTY) return i;
    }
    for (i = 0; i < QUAD_CELLS; i++) {
        if (sector[i] == SEC_EMPTY) return i;
    }
    return 0xFF;
}

static uint16_t enemy_strength(uint8_t type) {
    switch (type) {
        case SEC_BATTLESHIP: return HP_BATTLESHIP;
        case SEC_COMMAND:    return HP_COMMAND;
        case SEC_SCOUT:      return HP_SCOUT;
        case SEC_SUPPLY:     return HP_SUPPLY;
        default:             return 0;
    }
}

void trek_enter_quadrant(void) {
    uint8_t q = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
    uint8_t i, n, cell;

    for (i = 0; i < QUAD_CELLS; i++) {
        sector[i]   = SEC_EMPTY;
        enemy_hp[i] = 0;
    }

    /* The ship is placed first so its sector is reserved; everything else
       fills in around it. */
    sector[(ship.sec_y << 3) | ship.sec_x] = SEC_SHIP;

    for (n = 0; n < gal_stars[q]; n++) {
        cell = free_sector();
        if (cell != 0xFF) sector[cell] = SEC_STAR;
    }

    if (gal_base[q] != BASE_NONE) {
        cell = free_sector();
        if (cell != 0xFF) sector[cell] = SEC_BASE;
    }

    /* Enemy types are rolled per ship. Battleships dominate, matching the
       briefing's "a large number of enemy cruisers, a few command vessels"
       (manual l.211). */
    for (n = 0; n < gal_enemies[q]; n++) {
        cell = free_sector();
        if (cell == 0xFF) break;
        i = trek_rand_n(10);
        if (i < 6)      sector[cell] = SEC_BATTLESHIP;
        else if (i < 8) sector[cell] = SEC_SCOUT;
        else if (i < 9) sector[cell] = SEC_SUPPLY;
        else            sector[cell] = SEC_COMMAND;
        enemy_hp[cell] = enemy_strength(sector[cell]);
    }

    reveal_around(ship.quad_y, ship.quad_x);
}

void trek_new_game(uint8_t level, uint16_t seed) {
    uint8_t i, q, bases, placed;
    uint16_t total;

    trek_srand(seed);

    if (level < 1) level = 1;
    if (level > 5) level = 5;

    for (i = 0; i < GAL_CELLS; i++) {
        gal_enemies[i] = 0;
        gal_base[i]    = BASE_NONE;
        /* 0..8. CONFIRMED that empty quadrants exist: the original's chart
           showed 000 at quadrant 8,3 -- no enemies, no base, no stars. An
           earlier draft forced at least one star per quadrant. */
        gal_stars[i]   = trek_rand_n(9);
        gal_known[i]   = 0;
    }

    /* Fitted from five readings of the original, one per command level --
       see the note in trek.h. Higher levels face more ships (manual
       l.229-232), but the count is random within a level's range rather
       than fixed. */
    total = (uint16_t)(level * ENEMY_PER_LEVEL + trek_rand_n(ENEMY_SPREAD));
    ship.enemies_left = total;

    while (total) {
        q = trek_rand_n(GAL_CELLS);
        if (gal_enemies[q] >= 4) continue;    /* keep quadrants playable */
        gal_enemies[q]++;
        total--;
    }

    /* Two to four friendly bases, at most one per quadrant. StarBases are
       the useful ones; research and supply stations are rarer (manual
       l.356-360). */
    bases = (uint8_t)(2 + trek_rand_n(3));
    placed = 0;
    while (placed < bases) {
        q = trek_rand_n(GAL_CELLS);
        if (gal_base[q] != BASE_NONE) continue;
        i = trek_rand_n(10);
        gal_base[q] = (uint8_t)(i < 6 ? BASE_STARBASE
                              : (i < 8 ? BASE_SUPPLY : BASE_RESEARCH));
        placed++;
    }

    ship.energy       = ENERGY_START;
    ship.impulse      = IMPULSE_START;
    ship.shields      = SHIELD_START;    /* full, and lowered -- see trek.h */
    ship.torps        = TORPS_START;
    ship.laser_eff    = 100;
    ship.warp         = WARP_START;
    ship.shields_up   = 0;
    ship.stardate     = STARDATE_START;
    ship.stardate_end = (uint16_t)(STARDATE_START + MISSION_TENTHS);
    ship.level        = level;

    /* Start somewhere without enemies, so the opening move is the player's
       choice rather than an ambush. */
    do {
        q = trek_rand_n(GAL_CELLS);
    } while (gal_enemies[q] != 0);

    ship.quad_y = (uint8_t)(q >> 3);
    ship.quad_x = (uint8_t)(q & 7);
    ship.sec_y  = trek_rand_n(QUAD_DIM);
    ship.sec_x  = trek_rand_n(QUAD_DIM);

    trek_enter_quadrant();
}

/* -------------------------------------------------------------- movement */

/* The converter supplies ENERGY_PER_DAY per stardate at full repair
   (manual l.265), so a tenth of a stardate is a tenth of that. Damage is
   not modelled yet -- there is no damage system. */
static void advance_time(uint16_t tenths) {
    uint16_t gain = (uint16_t)(tenths * (ENERGY_PER_DAY / 10));

    ship.stardate = (uint16_t)(ship.stardate + tenths);

    if (ship.energy + gain > ENERGY_MAX) ship.energy = ENERGY_MAX;
    else ship.energy = (uint16_t)(ship.energy + gain);
}

/* Pool access by the 1/2/3 numbering the original's dialog uses. Returning
   pointers keeps trek_divert free of a switch per side. */
static uint16_t *pool(uint8_t which) {
    switch (which) {
        case POOL_MAIN:    return &ship.energy;
        case POOL_IMPULSE: return &ship.impulse;
        case POOL_SHIELDS: return &ship.shields;
        default:           return 0;
    }
}

static uint16_t pool_max(uint8_t which) {
    switch (which) {
        case POOL_MAIN:    return ENERGY_MAX;
        case POOL_IMPULSE: return IMPULSE_MAX;
        case POOL_SHIELDS: return SHIELD_MAX;
        default:           return 0;
    }
}

uint8_t trek_divert(uint8_t from, uint8_t to, uint16_t amount, uint16_t *lost) {
    uint16_t *src = pool(from);
    uint16_t *dst = pool(to);
    uint16_t cap, room, wasted = 0;

    if (lost) *lost = 0;
    if (!src || !dst || from == to) return DIVERT_ILLOGICAL;
    if (amount > *src) return DIVERT_SHORT;

    cap = pool_max(to);
    room = (uint16_t)(*dst >= cap ? 0 : cap - *dst);

    /* Anything over the destination's ceiling is destroyed, not returned --
       see the note in trek.h. This is why energy is not conserved here. */
    if (amount > room) {
        wasted = (uint16_t)(amount - room);
        *dst = cap;
    } else {
        *dst = (uint16_t)(*dst + amount);
    }

    *src = (uint16_t)(*src - amount);
    if (lost) *lost = wasted;

    return DIVERT_OK;
}

uint8_t trek_set_warp(uint8_t tenths) {
    if (tenths < WARP_MIN || tenths > WARP_MAX) return 0;
    ship.warp = tenths;
    return 1;
}

uint8_t trek_move_impulse(uint8_t sy, uint8_t sx) {
    uint16_t d, cost;
    uint8_t target;

    if (sy >= QUAD_DIM || sx >= QUAD_DIM) return MOVE_BAD_COORDS;
    if (sy == ship.sec_y && sx == ship.sec_x) return MOVE_SAME_PLACE;

    target = (uint8_t)((sy << 3) | sx);
    if (sector[target] != SEC_EMPTY) return MOVE_BLOCKED;

    d = trek_dist(abs_diff(sy, ship.sec_y), abs_diff(sx, ship.sec_x));

    /* PROVISIONAL: IMPULSE_ENERGY_UNIT per sector of distance. Nothing in
       the manual fixes impulse cost; it is set above the converter's output
       over the same interval so crossing a quadrant is a net loss. */
    /* Drawn from the impulse engines' own pool, not the main banks --
       confirmed on screen: the original tracks them separately. */
    cost = (uint16_t)(((d >> 4) * IMPULSE_ENERGY_UNIT) >> 4);
    if (cost > ship.impulse) return MOVE_NO_ENERGY;

    sector[(ship.sec_y << 3) | ship.sec_x] = SEC_EMPTY;
    ship.sec_y = sy;
    ship.sec_x = sx;
    sector[target] = SEC_SHIP;

    ship.impulse = (uint16_t)(ship.impulse - cost);
    advance_time((uint16_t)(d >> 8));   /* PROVISIONAL: 0.1 stardate/sector */
    return MOVE_OK;
}

uint8_t trek_move_warp(uint8_t qy, uint8_t qx, uint8_t sy, uint8_t sx) {
    uint16_t d, cost, tenths;

    if (qy >= GAL_DIM || qx >= GAL_DIM ||
        sy >= QUAD_DIM || sx >= QUAD_DIM) return MOVE_BAD_COORDS;

    if (qy == ship.quad_y && qx == ship.quad_x)
        return trek_move_impulse(sy, sx);

    d = trek_dist(abs_diff(qy, ship.quad_y), abs_diff(qx, ship.quad_x));

    /* PROVISIONAL cost model: distance * warp factor, doubled with shields
       raised -- the doubling is the one part the manual states outright
       (l.255). Shifted in two stages rather than one so the intermediate
       stays inside 16 bits: d maxes at 2534 and warp at 80, whose product
       would overflow. */
    /* MEASURED: cost = 1.5 * distance * warp^3, from two readings off the
       original -- ~194 for one quadrant at warp 5, ~710 at warp 8. Warp rose
       x1.6 and cost x3.66, which puts the exponent at 2.76; cube reproduces
       both within 8%, while the linear model this replaces predicted 320 at
       warp 8. Fitted from two points, so the exponent is well supported but
       the 1.5 is not precise.

       Staged to stay in 16 bits: warp^3 is built in two steps from tenths
       (max 512), and the distance factor is divided down before the multiply
       rather than after, which a single expression could not do without
       overflowing. */
    {
        uint16_t d44 = (uint16_t)(d >> 4);                      /* max 158 */
        uint16_t wc  = (uint16_t)(((uint16_t)ship.warp *
                                   (uint16_t)ship.warp) / 10);  /* max 640 */
        uint16_t f;
        wc = (uint16_t)((wc * (uint16_t)ship.warp) / 100);      /* warp^3 */

        /* Cost is SUBLINEAR in distance: two quadrants at warp 8 cost 1174,
           only 1.65x the 710 that one quadrant cost, where a linear model
           demanded 2x. A fixed overhead per jump plus a per-distance term
           fits -- roughly (0.5 + 0.9 * distance) * warp^3, in 1/256ths
           below. Time, by contrast, IS linear in distance (0.2 -> 0.4
           stardates over the same pair), so only the cost needed changing. */
        f = (uint16_t)(128 + d44 * 14);                         /* max 2340 */
        cost = (uint16_t)((wc * (f >> 5)) >> 3);                /* max 4672 */
    }
    if (ship.shields_up) cost = (uint16_t)(cost * 2);
    if (cost > ship.energy) return MOVE_NO_ENERGY;

    /* MEASURED: time goes as distance / warp SQUARED, not distance / warp.
       Three readings off the original -- 1 quadrant at warp 8 in 0.2
       stardates, 1 at warp 5 in ~0.4, and 4 at warp 2 in 11.0. Warp 2 to 8
       is four times the speed but 13.75 times the time per quadrant; 1/warp
       predicts 4, 1/warp^2 predicts 16. The fitted constant is 10, giving
       0.16 / 0.4 / 10.0 against those observations.

       The previous distance/warp model was five times too fast at warp 2,
       which is what let a 4-quadrant trip look like 2 stardates instead of
       11 -- and that error is why the energy reading for that trip clipped
       against the 5000 ceiling and was lost.

       Held in 16 bits by scaling distance down to 4.4 fixed point and the
       squared warp down by the same 16, rather than by a single multiply
       that would overflow. */
    {
        uint16_t d44 = (uint16_t)(d >> 4);                       /* max 158 */
        uint16_t wsq = (uint16_t)(((uint16_t)ship.warp *
                                   (uint16_t)ship.warp) >> 4);   /* max 400 */
        if (wsq == 0) wsq = 1;
        tenths = (uint16_t)((d44 * 39u) / wsq);                  /* max 6162 */
    }

    ship.quad_y = qy;
    ship.quad_x = qx;
    ship.sec_y  = sy;
    ship.sec_x  = sx;

    ship.energy = (uint16_t)(ship.energy - cost);
    advance_time(tenths);

    trek_enter_quadrant();
    return MOVE_OK;
}

/* --------------------------------------------------------------- weapons */

/* v * f / 256, rounded to nearest.
 *
 * Written as two products because a single one overflows: energy reaches
 * 5000 and f reaches 256, whose product needs 21 bits while `int` is 16 on
 * cc65. Splitting v at the byte boundary keeps the high part's multiply
 * after its own shift, so neither term can exceed 65535 -- the tightest case
 * is v = 65535, f = 256, which lands exactly on 65535.
 *
 * The whole fractional part comes from the low byte, so rounding the low
 * product alone is exact rather than an approximation. */
static uint16_t scale_256(uint16_t v, uint16_t f) {
    uint16_t hi = (uint16_t)(v >> 8);
    uint16_t lo = (uint16_t)(v & 0xFF);
    return (uint16_t)(hi * f + (uint16_t)(((lo * f) + 128) >> 8));
}

/* v * pct / 100, rounded to nearest, by the same splitting argument. Done
   against 100 rather than through a 256ths factor so that the common case of
   100% is an exact identity and cannot perturb a measured figure. */
static uint16_t scale_pct(uint16_t v, uint8_t pct) {
    uint16_t q = (uint16_t)(v / 100);
    uint16_t r = (uint16_t)(v % 100);
    return (uint16_t)(q * pct + (uint16_t)(((r * pct) + 50) / 100));
}

/* (1 - dist/12) in 256ths, rounded. dist is 8.8 fixed point, so the reach is
   12 * 256; the divide by 12 lands the result on 0..256 directly. Rounding
   here rather than truncating is not cosmetic -- truncation puts the 500-unit
   shot at range 1.414 on 439 against the original's 441.
 *
 * Holding the factor in 256ths quantises the model by at most half a step,
 * so the delivered figure can differ from the real formula by up to
 * energy/512. That is under one unit at the energies every reading was taken
 * at, which is why all five reproduce exactly, and just under 10 at a full
 * 5000-unit discharge. The core test asserts that bound. Finer fixed point
 * is not available: the products already reach the 16-bit ceiling. */
static uint16_t laser_factor(uint16_t dist) {
    uint16_t reach = (uint16_t)(LASER_RANGE_ZERO << 8);
    if (dist >= reach) return 0;
    return (uint16_t)((reach - dist + (LASER_RANGE_ZERO / 2)) / LASER_RANGE_ZERO);
}

/* MEASURED exactly; see trek.h and MEASURED.md. Distance is applied before
   efficiency so that the readings, all of which were taken at 100%, come
   back bit for bit. */
uint16_t trek_laser_damage(uint16_t energy, uint8_t eff_pct, uint16_t dist) {
    uint16_t f = laser_factor(dist);

    if (f == 0 || eff_pct == 0 || energy == 0) return 0;
    if (eff_pct > 100) eff_pct = 100;

    return scale_pct(scale_256(energy, f), eff_pct);
}

uint8_t trek_fire_laser(uint8_t sy, uint8_t sx, uint16_t energy,
                        uint16_t *damage) {
    uint8_t cell, q;
    uint16_t d, dealt;

    if (damage) *damage = 0;

    if (sy >= QUAD_DIM || sx >= QUAD_DIM) return FIRE_BAD_COORDS;

    cell = (uint8_t)((sy << 3) | sx);
    if (!SEC_IS_ENEMY(sector[cell])) return FIRE_NO_TARGET;

    /* The original refuses the shot outright rather than firing what is left
       -- "Captain, we have insufficient energy!" -- and the energy is not
       spent when it refuses. */
    if (energy > ship.energy) return FIRE_NO_ENERGY;
    ship.energy = (uint16_t)(ship.energy - energy);

    d = trek_dist(abs_diff(sy, ship.sec_y), abs_diff(sx, ship.sec_x));
    dealt = trek_laser_damage(energy, ship.laser_eff, d);
    if (damage) *damage = dealt;

    if (dealt < enemy_hp[cell]) {
        enemy_hp[cell] = (uint16_t)(enemy_hp[cell] - dealt);
        return FIRE_OK;
    }

    enemy_hp[cell] = 0;
    sector[cell]   = SEC_EMPTY;

    q = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
    if (gal_enemies[q]) gal_enemies[q]--;
    if (ship.enemies_left) ship.enemies_left--;

    return FIRE_KILL;
}
