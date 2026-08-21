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

    /* Arriving relieves a base under siege. This is the ancestor's own rule
       read the other way round: its FBATTAK will not pick a base that is in
       the player's quadrant at all ("!same(baseq[j], game.quadrant)"), so
       presence protects. Without this the printed deadline would be a
       countdown the player cannot affect, which is not what a message saying
       "they can last until 3517.8" is for. */
    if (base_under_attack == q) {
        base_under_attack = GAL_CELLS;
        trek_unschedule(SCHED_BASE_FALLS);
    }

    ship.docked = BASE_NONE;

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

/* Scheduled event dates, one slot per type -- see trek.h for why a slot
   rather than a queue. Declared here rather than beside the event code
   because trek_new_game seeds the schedule and comes first. */
static uint16_t sched[SCHED_COUNT];
uint8_t  base_under_attack = GAL_CELLS;
uint8_t  bases_lost = 0;

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
    ship.laser_heat   = 0;
    ship.docked       = BASE_NONE;
    ship.killed       = 0;
    ship.killed_cmd   = 0;
    ship.casualties   = 0;
    ship.lost         = 0;
    for (i = 0; i < SYS_COUNT; i++) ship.sys[i] = 100;
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

    /* Seed the schedule. The ancestor does the same in its setup, with means
       expressed as fractions of the mission length -- so these are too, which
       keeps a short mission eventful and a long one paced.

       The tractor beam starts unscheduled at level 1 and 2: being dragged
       across the galaxy before you have a working ship is the sort of thing
       that makes a first game unwinnable, and the original's own difficulty
       curve has to do something equivalent. DERIVED plus judgement, and
       flagged as such. */
    for (i = 0; i < SCHED_COUNT; i++) sched[i] = SCHED_NEVER;
    base_under_attack = GAL_CELLS;
    bases_lost = 0;

    trek_schedule(SCHED_BASE_ATTACK, trek_expran(MISSION_TENTHS / 3));
    trek_schedule(SCHED_DEATH_POD,   trek_expran(MISSION_TENTHS / 2));
    if (level >= 3)
        trek_schedule(SCHED_TRACTOR, trek_expran(MISSION_TENTHS / 2));
}

/* -------------------------------------------------------------- movement */

/* The converter supplies ENERGY_PER_DAY per stardate at full repair
   (manual l.265), so a tenth of a stardate is a tenth of that. Damage is
   not modelled yet -- there is no damage system. */
static void advance_time(uint16_t tenths) {
    uint16_t gain = (uint16_t)(tenths * (ENERGY_PER_DAY / 10));
    uint16_t mend = (uint16_t)((REPAIR_PER_STARDATE * tenths) / 10);

    uint8_t i;

    /* DERIVED: the ancestor divides the repair period by docfac = 0.25 while
       docked, which is four times the rate. Applied before the clamp below,
       so a long docked rest still stops at 100.

       Declarations first: cc65 is C89 and rejects a statement before one,
       while the native build is C99 and does not. `make test` passing is not
       evidence that the port compiles. */
    if (ship.docked != BASE_NONE) mend = (uint16_t)(mend * DOCK_REPAIR_FACTOR);

    ship.stardate = (uint16_t)(ship.stardate + tenths);

    /* The converter's output scales with its own state of repair (manual
       l.367-369), so a wrecked converter cannot dig you out of a hole. */
    gain = (uint16_t)((gain * ship.sys[SYS_CONVERTER]) / 100);

    if (ship.energy + gain > ENERGY_MAX) ship.energy = ENERGY_MAX;
    else ship.energy = (uint16_t)(ship.energy + gain);

    /* MEASURED: floor(20 * stardates), applied to every damaged system at
       the full rate rather than divided between them. */
    if (mend) {
        for (i = 0; i < SYS_COUNT; i++) {
            if (ship.sys[i] >= 100) continue;
            if (ship.sys[i] + mend >= 100) ship.sys[i] = 100;
            else ship.sys[i] = (uint8_t)(ship.sys[i] + mend);
        }
    }
}

/* ---------------------------------------------------------------- docking */

/* The base's own cell, or QUAD_CELLS if the quadrant has none. */
static uint8_t base_cell(void) {
    uint8_t c;
    for (c = 0; c < QUAD_CELLS; c++)
        if (sector[c] == SEC_BASE) return c;
    return QUAD_CELLS;
}

uint8_t trek_dock(void) {
    uint8_t c, by, bx, dy, dx, type;

    if (ship.docked != BASE_NONE) return DOCK_ALREADY;

    c = base_cell();
    if (c == QUAD_CELLS) return DOCK_NO_BASE;

    by = (uint8_t)(c >> 3);
    bx = (uint8_t)(c & 7);
    dy = abs_diff(by, ship.sec_y);
    dx = abs_diff(bx, ship.sec_x);
    if (dy > 1 || dx > 1) return DOCK_NO_BASE;

    type = gal_base[(ship.quad_y << 3) | ship.quad_x];
    if (type == BASE_NONE) return DOCK_NO_BASE;
    ship.docked = type;

    /* What each type replenishes, per the manual -- see trek.h. Life support
       supplies are not a resource in this core, so a Research Station gives
       nothing here beyond the docked repair rate. */
    if (type == BASE_STARBASE) {
        if (ship.energy < ENERGY_START) ship.energy = ENERGY_START;
        ship.impulse = IMPULSE_START;
        ship.shields = SHIELD_MAX;
        ship.torps   = TORPS_START;
    } else if (type == BASE_SUPPLY) {
        ship.torps = TORPS_START;
    }

    return DOCK_OK;
}

void trek_undock(void) {
    ship.docked = BASE_NONE;
}

uint8_t trek_docked_safe(void) {
    return (uint8_t)(ship.docked == BASE_STARBASE);
}

/* ------------------------------------------------------ scheduled events */

/* take_damage lives with the combat code further down; events need it for the
   death pod, which damages the ship exactly as enemy fire does. */
static void take_damage(uint16_t amount, TrekEvent *ev, uint8_t *n, uint8_t max);

/* A base to put under attack, or GAL_CELLS if the galaxy has none left. Walks
   from a random start so the choice is uniform without needing a count first. */
static uint8_t pick_base(void) {
    uint8_t i, q = trek_rand_n(GAL_CELLS);
    for (i = 0; i < GAL_CELLS; i++) {
        if (gal_base[q] != BASE_NONE && q != base_under_attack) return q;
        q = (uint8_t)((q + 1) & (GAL_CELLS - 1));
    }
    return GAL_CELLS;
}

/* Likewise for a quadrant holding enemies -- where a tractor beam drags you,
   since it is an enemy doing the dragging. */
static uint8_t pick_enemy_quadrant(void) {
    uint8_t i, q = trek_rand_n(GAL_CELLS);
    for (i = 0; i < GAL_CELLS; i++) {
        if (gal_enemies[q]) return q;
        q = (uint8_t)((q + 1) & (GAL_CELLS - 1));
    }
    return GAL_CELLS;
}

void trek_schedule(uint8_t kind, uint16_t offset_tenths) {
    uint16_t when;
    if (kind >= SCHED_COUNT) return;
    /* Saturate rather than wrap. A schedule that wrapped would land in the
       past and fire immediately, which is the opposite of what was asked. */
    {
        uint16_t headroom = (uint16_t)(SCHED_NEVER - ship.stardate);
        if (offset_tenths >= headroom) when = (uint16_t)(SCHED_NEVER - 1);
        else when = (uint16_t)(ship.stardate + offset_tenths);
    }
    sched[kind] = when;
}

void trek_unschedule(uint8_t kind) {
    if (kind < SCHED_COUNT) sched[kind] = SCHED_NEVER;
}

uint8_t trek_is_scheduled(uint8_t kind) {
    return (uint8_t)(kind < SCHED_COUNT && sched[kind] != SCHED_NEVER);
}

uint16_t trek_scheduled(uint8_t kind) {
    return (kind < SCHED_COUNT) ? sched[kind] : SCHED_NEVER;
}

/* -ln(u) * 32, for u at the midpoint of each thirty-second of (0,1).
   Indexed by five bits of the PRNG, so the lookup is a shift and never a
   division -- the 6502 has no divide and this runs whenever an event is
   rescheduled. Mean of the table is 1.00 to two places, as it must be. */
static const uint8_t neglog32[32] = {
    133, 98, 82, 71, 63, 56, 51, 46, 42, 39, 36, 33, 30, 28, 25, 23,
     21, 19, 18, 16, 14, 13, 11, 10,  9,  7,  6,  5,  4,  3,  2,  1
};

uint16_t trek_expran(uint16_t mean_tenths) {
    uint16_t t = neglog32[(uint8_t)(trek_rand() >> 11)];   /* top 5 bits */
    uint16_t hi, whole, frac;

    /* mean * t / 32, staged AND saturated.
     *
     * Staged because `long` is forbidden here and `int` is 16 bits under cc65
     * but 32 on the 68000 -- a version relying on promotion would compute a
     * different schedule on the Amiga than on the C128, silently, from the
     * same seed.
     *
     * Saturated because splitting is not by itself enough: the deviate can
     * reach 4.16x its mean, so any mean above about 15750 tenths genuinely
     * cannot be represented. Wrapping there would return a small number, and
     * a small number is not a harmless wrong answer -- it schedules an event
     * that should have been distant into the next few turns. No caller passes
     * a mean that large today; this is here so that none ever can. */
    hi = (uint16_t)(mean_tenths >> 5);
    if (t && hi > (uint16_t)(65535U / t)) return 65535U;

    whole = (uint16_t)(hi * t);
    frac  = (uint16_t)(((uint16_t)(mean_tenths & 31) * t) >> 5);
    {
        uint16_t room = (uint16_t)(65535U - frac);
        if (whole > room) return 65535U;
    }
    return (uint16_t)(whole + frac);
}

/* Fires everything due between now and `until`, in date order, and returns
   how many were reported. The ancestor walks its array picking the earliest
   date under the horizon and repeats; so does this. */
static uint8_t run_events(uint16_t until, TrekEvent *ev, uint8_t *n, uint8_t max) {
    uint8_t guard;

    /* Bounded rather than while(1): an event that reschedules itself inside
       the same window would otherwise spin forever, and on a 6502 that is a
       hang with no console to break into. */
    for (guard = 0; guard < 16; guard++) {
        uint8_t i, next = SCHED_COUNT;
        uint16_t best = SCHED_NEVER;

        for (i = 0; i < SCHED_COUNT; i++)
            if (sched[i] <= until && sched[i] < best) { best = sched[i]; next = i; }
        if (next == SCHED_COUNT) break;

        sched[next] = SCHED_NEVER;

        switch (next) {
            case SCHED_BASE_ATTACK: {
                uint8_t q = pick_base();
                if (q == GAL_CELLS) break;
                base_under_attack = q;
                /* The ancestor gives 1 + 3*Rand() stardates before the base
                   falls. That deadline is the number EGA Trek prints. */
                trek_schedule(SCHED_BASE_FALLS,
                              (uint16_t)(10 + trek_rand_n(30)));
                if (ev && *n < max) {
                    ev[*n].kind   = EV_BASE_ATTACKED;
                    ev[*n].y      = (uint8_t)(q >> 3);
                    ev[*n].x      = (uint8_t)(q & 7);
                    ev[*n].amount = sched[SCHED_BASE_FALLS];
                    (*n)++;
                }
                break;
            }
            case SCHED_BASE_FALLS: {
                uint8_t q = base_under_attack;
                if (q >= GAL_CELLS) break;
                gal_base[q] = BASE_NONE;
                base_under_attack = GAL_CELLS;
                if (bases_lost < 255) bases_lost++;
                if (ev && *n < max) {
                    ev[*n].kind   = EV_BASE_LOST;
                    ev[*n].y      = (uint8_t)(q >> 3);
                    ev[*n].x      = (uint8_t)(q & 7);
                    ev[*n].amount = 0;
                    (*n)++;
                }
                /* Another base will be attacked in due course. */
                trek_schedule(SCHED_BASE_ATTACK, trek_expran(300));
                break;
            }
            case SCHED_TRACTOR:
                /* MEASURED that this exists -- a long range tractor beam
                   pulled the ship into quadrant 6,5 mid-measurement and
                   wrecked three systems on arrival (MEASURED.md). What is
                   NOT measured is what decides the destination, so it is a
                   random quadrant holding enemies, which is the ancestor's
                   rule (a commander does the dragging). */
                {
                    uint8_t q = pick_enemy_quadrant();
                    if (q == GAL_CELLS) break;
                    ship.quad_y = (uint8_t)(q >> 3);
                    ship.quad_x = (uint8_t)(q & 7);
                    trek_enter_quadrant();
                    if (ev && *n < max) {
                        ev[*n].kind   = EV_TRACTORED;
                        ev[*n].y      = ship.quad_y;
                        ev[*n].x      = ship.quad_x;
                        ev[*n].amount = 0;
                        (*n)++;
                    }
                }
                trek_schedule(SCHED_TRACTOR, trek_expran(400));
                break;

            case SCHED_DEATH_POD:
                /* MEASURED: one figure, applied to the ship AND to every
                   enemy in the quadrant. 59 units on us and 59 on each of two
                   Mongols in the same turn. The size is not measured beyond
                   that single reading, so it is that reading give or take. */
                {
                    uint16_t hit = (uint16_t)(40 + trek_rand_n(40));
                    uint8_t  cell;
                    for (cell = 0; cell < QUAD_CELLS; cell++) {
                        if (!SEC_IS_ENEMY(sector[cell])) continue;
                        if (enemy_hp[cell] > hit) enemy_hp[cell] = (uint16_t)(enemy_hp[cell] - hit);
                        else { enemy_hp[cell] = 0; sector[cell] = SEC_EMPTY; }
                    }
                    if (ev && *n < max) {
                        ev[*n].kind   = EV_POD_HIT;
                        ev[*n].y      = ship.sec_y;
                        ev[*n].x      = ship.sec_x;
                        ev[*n].amount = hit;
                        (*n)++;
                    }
                    take_damage(hit, ev, n, max);
                }
                trek_schedule(SCHED_DEATH_POD, trek_expran(500));
                break;
        }
    }
    return *n;
}

/* Movement returns a status code and has no event list to fill, so the queue
   is drained from here instead of from advance_time(). The turn loop calls
   this once after whatever consumed the turn -- which also means an event
   fires after the move that carried the clock past it, not in the middle of
   it, and the player sees the two in the order they happened. */
uint8_t trek_run_events(TrekEvent *ev, uint8_t max) {
    uint8_t n = 0;
    run_events(ship.stardate, ev, &n, max);
    return n;
}

uint8_t trek_advance(uint16_t tenths, TrekEvent *ev, uint8_t max) {
    advance_time(tenths);
    return trek_run_events(ev, max);
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

    /* Any movement breaks the dock. Placed after every rejection above, so a
       move the game refuses does not silently cast off. */
    trek_undock();

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

void trek_laser_begin_volley(void) {
    ship.laser_heat = 0;
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

    /* Heat is the energy that went into the volley, whether or not it hit --
       and this point is past every refusal, so a rejected shot adds none.
       Saturates rather than wrapping: the gauge is already off its scale long
       before 65535 and a wrap would read as cold. */
    {
        uint16_t room = (uint16_t)(65535U - energy);
        if (ship.laser_heat > room) ship.laser_heat = 65535U;
        else ship.laser_heat = (uint16_t)(ship.laser_heat + energy);
    }

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

/* ------------------------------------------------------------ the enemy */

/* PROVISIONAL, and the weakest thing in this file. See trek.h: the shape is
   a per-class energy scaled by how much of the ship is left, fired through
   the same falloff our own lasers use. Only the falloff and the
   remaining-strength dependence have any evidence behind them. */
static uint16_t enemy_fire_energy(uint8_t type, uint16_t hp) {
    uint16_t base, full;

    switch (type) {
        case SEC_COMMAND: base = ENEMY_FIRE_COMMAND;    full = HP_COMMAND;    break;
        case SEC_SCOUT:   base = ENEMY_FIRE_SCOUT;      full = HP_SCOUT;      break;
        case SEC_SUPPLY:  base = ENEMY_FIRE_SUPPLY;     full = HP_SUPPLY;     break;
        default:          base = ENEMY_FIRE_BATTLESHIP; full = HP_BATTLESHIP; break;
    }
    if (full == 0) return 0;
    if (hp > full) hp = full;
    /* base * hp / full, staged so the product stays inside 16 bits: base
       reaches 300 and hp 695, whose product does not fit. */
    return (uint16_t)(((base / 5) * hp) / (full / 5 ? full / 5 : 1));
}

/* Damage lands on the shields first and on the main banks once those are
   gone. MEASURED: with shields up and sufficient, the printed figure comes
   out of the shield pool entirely and main energy is untouched. */
static void take_damage(uint16_t amount, TrekEvent *ev, uint8_t *n, uint8_t max) {
    uint16_t through;

    if (amount <= ship.shields) {
        ship.shields = (uint16_t)(ship.shields - amount);
        return;
    }

    through = (uint16_t)(amount - ship.shields);
    ship.shields = 0;

    if (through >= ship.energy) {
        ship.energy = 0;
        ship.lost = 1;
        if (*n < max) { ev[*n].kind = EV_SHIP_LOST; ev[*n].amount = 0; (*n)++; }
        return;
    }
    ship.energy = (uint16_t)(ship.energy - through);

    /* PROVISIONAL: a hit heavy enough to get through can wreck something.
       The original does this -- one ambush took out three systems at once --
       but neither the chance nor the severity is measured. */
    if (through >= SYSTEM_DAMAGE_THRESHOLD) {
        uint8_t which = trek_rand_n(SYS_COUNT);
        uint8_t hurt  = (uint8_t)(20 + trek_rand_n(40));
        ship.sys[which] = (uint8_t)(ship.sys[which] > hurt ? ship.sys[which] - hurt : 0);
        ship.casualties = (uint16_t)(ship.casualties + trek_rand_n(10));
        if (*n < max) {
            ev[*n].kind = EV_SYSTEM_HIT;
            ev[*n].y = which;
            ev[*n].amount = ship.sys[which];
            (*n)++;
        }
    }
}

/* How keen an enemy is to close, DERIVED from the ancestor's movebaddy().
 *
 * The ancestor builds a "forces" score out of the enemy's own power, how many
 * of them are present, and how dangerous we look, then turns it into a signed
 * number of sectors: positive advance, negative retreat, zero hold. Its
 * constants are reproduced here in integer form. NONE of them is measured for
 * EGA Trek; what is measured is the resulting behaviour at level 3, where a
 * Commander advanced one sector per turn and then held (MEASURED.md).
 *
 * Deliberately not reproduced: the ancestor's supercommander bolt-hole, its
 * docked-at-base back-off, and its expert-skill enemy weighting. Those depend
 * on features this core does not have yet, and inventing them would be worse
 * than leaving them out. */
static int16_t enemy_motion(uint16_t hp, uint16_t dist_whole) {
    uint16_t forces;
    int16_t  motion;

    /* forces = own power + 100 per enemy present, +1000 if our shields are
       down, less a term for the energy and torpedoes we could bring. Staged
       to stay inside 16 bits: hp reaches 695 and the additions are bounded. */
    forces = hp;
    if (ship.enemies_left < 40) forces = (uint16_t)(forces + 100 * ship.enemies_left);
    if (!ship.shields_up)       forces = (uint16_t)(forces + 1000);

    if (ship.energy > 2500) {
        uint16_t bold = (uint16_t)((ship.energy - 2500) / 5);   /* 0.2 * excess */
        forces = (forces > bold) ? (uint16_t)(forces - bold) : 0;
    }
    {
        uint16_t torp = (uint16_t)(50 * ship.torps);
        forces = (forces > torp) ? (uint16_t)(forces - torp) : 0;
    }

    if (forces > 1000) {
        /* Very strong: move in for the kill, but never past us. */
        motion = (int16_t)(dist_whole + 1);
    } else {
        /* The ancestor's forces/150 - 5, which is negative below 750 and
           positive above -- so a weak ship backs off and a strong one closes. */
        motion = (int16_t)(forces / 150) - 5;
    }

    /* Limited by command level, as the ancestor limits by skill. */
    if (motion >  (int16_t)ship.level) motion =  (int16_t)ship.level;
    if (motion < -(int16_t)ship.level) motion = -(int16_t)ship.level;
    return motion;
}

/* One step of pursuit or flight: the sign of the row and column difference,
   which is the ancestor's krawl. Returns NO_STEP if the destination is not
   free -- enemies do not push through stars, bases or each other.
 *
 * Returns the destination rather than filling in a `uint8_t *dest`, and that
 * is not a style preference. The out-parameter version crashed cc65 outright
 * at -O ("Subprocess 'cc65' aborted by signal 11") while compiling fine
 * without it; the return-value form compiles at every level. Bisected to this
 * function, and then to the parameter itself -- rewriting the branches and
 * removing all signed-char arithmetic changed nothing. Native `make test`
 * cannot see this: it only shows up in the cross compiler. */
#define NO_STEP 0xFF

static uint8_t enemy_step(uint8_t cell, uint8_t away) {
    uint8_t y = (uint8_t)(cell >> 3), x = (uint8_t)(cell & 7);
    uint8_t ny = y, nx = x, dest;

    if (!away) {
        if (ship.sec_y > y)      ny = (uint8_t)(y + 1);
        else if (ship.sec_y < y) ny = (uint8_t)(y - 1);
        if (ship.sec_x > x)      nx = (uint8_t)(x + 1);
        else if (ship.sec_x < x) nx = (uint8_t)(x - 1);
    } else {
        if (ship.sec_y > y)      { if (y == 0) return NO_STEP; ny = (uint8_t)(y - 1); }
        else if (ship.sec_y < y) { if (y >= QUAD_DIM - 1) return NO_STEP; ny = (uint8_t)(y + 1); }
        if (ship.sec_x > x)      { if (x == 0) return NO_STEP; nx = (uint8_t)(x - 1); }
        else if (ship.sec_x < x) { if (x >= QUAD_DIM - 1) return NO_STEP; nx = (uint8_t)(x + 1); }
    }

    dest = (uint8_t)((ny << 3) | nx);
    if (dest == cell) return NO_STEP;
    if (sector[dest] != SEC_EMPTY) return NO_STEP;
    return dest;
}

/* Moves every enemy in the quadrant, then leaves firing to the caller. Done as
   a separate pass so that an enemy fires from where it ended up, which is what
   the original's scanner report implies -- it announces the move first and the
   damage afterwards. */
static void enemies_move(TrekEvent *ev, uint8_t *n, uint8_t max) {
    uint8_t cell, dest, steps, away;
    int16_t motion;
    uint16_t d;
    /* Which cells have already had their turn, so a ship that steps into a
       not-yet-visited cell does not get a second one. */
    uint8_t moved[QUAD_CELLS];
    uint8_t i;

    for (i = 0; i < QUAD_CELLS; i++) moved[i] = 0;

    for (cell = 0; cell < QUAD_CELLS; cell++) {
        if (!SEC_IS_ENEMY(sector[cell]) || moved[cell]) continue;

        d = trek_dist(abs_diff((uint8_t)(cell >> 3), ship.sec_y),
                      abs_diff((uint8_t)(cell & 7), ship.sec_x));
        motion = enemy_motion(enemy_hp[cell], (uint16_t)(d >> 8));
        if (motion == 0) { moved[cell] = 1; continue; }

        away  = (uint8_t)(motion < 0);
        steps = (uint8_t)(away ? -motion : motion);

        while (steps--) {
            dest = enemy_step(cell, away);
            if (dest == NO_STEP) break;
            sector[dest]   = sector[cell];
            enemy_hp[dest] = enemy_hp[cell];
            sector[cell]   = SEC_EMPTY;
            enemy_hp[cell] = 0;
            cell = dest;
        }
        moved[cell] = 1;

        if (*n < max) {
            ev[*n].kind   = EV_ENEMY_MOVED;
            ev[*n].y      = (uint8_t)(cell >> 3);
            ev[*n].x      = (uint8_t)(cell & 7);
            ev[*n].amount = 0;
            (*n)++;
        }
    }
}

uint8_t trek_enemy_turn(TrekEvent *ev, uint8_t max) {
    uint8_t cell, n = 0;
    uint16_t d, energy, dmg;

    enemies_move(ev, &n, max);

    /* NOT here, deliberately: the ancestor drains a quarter of an enemy's
       power every time it fires. Tested directly against the original -- five
       zero-energy turns while two Mongols shot at us, and neither lost a
       single point. Anderson dropped that rule. See MEASURED.md. */

    for (cell = 0; cell < QUAD_CELLS && n < max; cell++) {
        if (!SEC_IS_ENEMY(sector[cell])) continue;
        if (ship.lost) break;

        d = trek_dist(abs_diff((uint8_t)(cell >> 3), ship.sec_y),
                      abs_diff((uint8_t)(cell & 7), ship.sec_x));
        energy = enemy_fire_energy(sector[cell], enemy_hp[cell]);
        dmg = trek_laser_damage(energy, 100, d);
        if (dmg == 0) continue;

        /* "When docked at a StarBase its shields will protect your ship from
           enemy lasers" (manual l.440-441). Only a StarBase -- research
           stations and supply bases do not, which is why trek_docked_safe()
           tests the type rather than merely being docked. */
        if (trek_docked_safe()) {
            ev[n].kind   = EV_SHIELD_HOLD;
            ev[n].y      = (uint8_t)(cell >> 3);
            ev[n].x      = (uint8_t)(cell & 7);
            ev[n].amount = dmg;
            n++;
            continue;
        }

        ev[n].kind   = (dmg <= ship.shields) ? EV_SHIELD_HOLD : EV_HIT;
        ev[n].y      = (uint8_t)(cell >> 3);
        ev[n].x      = (uint8_t)(cell & 7);
        ev[n].amount = dmg;
        n++;

        take_damage(dmg, ev, &n, max);
    }
    return n;
}

/* arctan(i/16) in whole degrees, i = 0..16. The table is the whole of the
   trigonometry in this file; everything else is octant bookkeeping. */
static const uint8_t atan_deg[17] = {
    0, 4, 7, 11, 14, 17, 21, 24, 27, 29, 32, 35, 37, 39, 41, 43, 45
};

uint16_t trek_bearing(uint8_t sy, uint8_t sx) {
    uint8_t up, right;      /* screen rows grow downwards; bearings do not */
    uint16_t dy, dx, a;

    if (sy == ship.sec_y && sx == ship.sec_x) return 0;

    up    = (uint8_t)(sy < ship.sec_y);
    right = (uint8_t)(sx > ship.sec_x);
    dy = abs_diff(sy, ship.sec_y);
    dx = abs_diff(sx, ship.sec_x);

    /* Reduce to the first octant, where the smaller leg is over the larger,
       and index the table by that ratio in sixteenths. */
    if (dx >= dy) a = atan_deg[(dy * 16) / dx];
    else          a = (uint16_t)(90 - atan_deg[(dx * 16) / dy]);

    /* The modulo matters only in one place, and it is the place a compass is
       most often asked about: due east is a == 0 in the lower-right octant,
       where 360 - 0 would report 360 rather than 0. */
    if (right)  return up ? a : (uint16_t)((360 - a) % 360);
    return up ? (uint16_t)(180 - a) : (uint16_t)(180 + a);
}

/* ---------------------------------------------------------------- torps */

uint8_t trek_fire_torpedo(uint8_t sy, uint8_t sx) {
    uint8_t cell;

    if (sy >= QUAD_DIM || sx >= QUAD_DIM) return TORP_BAD_COORDS;
    if (ship.torps == 0) return TORP_NONE_LEFT;

    ship.torps--;
    cell = (uint8_t)((sy << 3) | sx);

    if (!SEC_IS_ENEMY(sector[cell])) return TORP_MISS;

    /* CONFIRMED: one torpedo destroys an undamaged standard Mongol outright.
       It killed a 355-hit-point battleship at range 3.0 with no damage
       figure printed, which is why torpedo damage itself is still unknown --
       nothing has ever survived one to report a number. */
    if (sector[cell] == SEC_COMMAND) ship.killed_cmd++;
    else                             ship.killed++;

    sector[cell]   = SEC_EMPTY;
    enemy_hp[cell] = 0;
    {
        uint8_t q = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
        if (gal_enemies[q]) gal_enemies[q]--;
    }
    if (ship.enemies_left) ship.enemies_left--;
    return TORP_KILL;
}

/* --------------------------------------------------------- mission state */

uint8_t trek_game_state(void) {
    if (ship.lost) return GAME_LOST;
    if (ship.enemies_left == 0) return GAME_WON;
    return GAME_ON;
}

/* MEASURED: every weight comes off the original's evaluation screen, and the
   arithmetic on that sheet closed exactly. The kill/day term is deliberately
   absent -- it printed 0.00 against two kills in 1.2 stardates, so something
   gates it that we do not understand, and implementing a guess would be
   worse than leaving it out. */
/* 500 x kills / elapsed_stardates, in integers and without leaving 16 bits.
 *
 * Done as (500 * kills) * 10 / tenths rather than 5000 * kills / tenths: the
 * first product is at most 500 x 64 = 32000 and fits, while 5000 x 64 does
 * not. The remainder is carried so a fractional stardate is not simply
 * dropped -- at the five-stardate floor, dropping it would cost up to 18%. */
static uint16_t kill_rate_points(uint16_t kills, uint16_t tenths) {
    uint16_t a, q, r;

    if (tenths < SCORE_MIN_TENTHS) tenths = SCORE_MIN_TENTHS;
    if (kills > 64) kills = 64;          /* keeps `a` inside 16 bits */

    a = (uint16_t)(SCORE_PER_KILL_DAY * kills);
    q = (uint16_t)(a / tenths);
    r = (uint16_t)(a - (uint16_t)(q * tenths));

    /* r < tenths, so r * 10 overflows only for a mission longer than 6553
       stardates -- far beyond anything playable, but the guard costs one
       comparison and turns an impossible case into an approximate answer
       rather than a wrong one. */
    if (tenths > 6553) return (uint16_t)(q * 10);
    return (uint16_t)((uint16_t)(q * 10) + (uint16_t)((uint16_t)(r * 10) / tenths));
}

int16_t trek_score(void) {
    int16_t s = 0;

    s = (int16_t)(s + ship.killed * SCORE_PER_MONGOL);
    s = (int16_t)(s + ship.killed_cmd * SCORE_PER_COMMANDER);
    s = (int16_t)(s - (int16_t)ship.casualties);
    s = (int16_t)(s + (int16_t)bases_lost * SCORE_BASE_LOST);

    /* The rate term credits only on a finished mission. See trek.h: FITTED
       from one reading of the original, on the condition the ancestor uses
       for this same term. */
    if (ship.enemies_left) {
        s = (int16_t)(s + SCORE_INCOMPLETE);
    } else {
        uint16_t elapsed = (uint16_t)(ship.stardate - STARDATE_START);
        uint16_t kills   = (uint16_t)(ship.killed + ship.killed_cmd);
        s = (int16_t)(s + (int16_t)kill_rate_points(kills, elapsed));
    }

    if (ship.lost) s = (int16_t)(s + SCORE_SHIP_LOST);
    return s;
}
