#include "trek.h"
#include "planet.h"

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

uint16_t trek_rand_n16(uint16_t n) {
    if (n == 0) return 0;
    return (uint16_t)(trek_rand() % n);
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
uint8_t trek_free_sector(void) {
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

uint16_t trek_enemy_full_hp(uint8_t type) { return enemy_strength(type); }

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
    /* MEASURED: laser heat is cleared by LEAVING a quadrant, not by starting
       a volley -- it accumulates across volleys while you stay. */
    ship.laser_heat = 0;

    for (i = 0; i < QUAD_CELLS; i++) {
        sector[i]   = SEC_EMPTY;
        enemy_hp[i] = 0;
    }

    /* The ship is placed first so its sector is reserved; everything else
       fills in around it. */
    sector[(ship.sec_y << 3) | ship.sec_x] = SEC_SHIP;

    for (n = 0; n < gal_stars[q]; n++) {
        cell = trek_free_sector();
        if (cell != 0xFF) sector[cell] = SEC_STAR;
    }

    if (gal_base[q] != BASE_NONE) {
        cell = trek_free_sector();
        if (cell != 0xFF) sector[cell] = SEC_BASE;
    }

    /* Enemy types are rolled per ship. Battleships dominate, matching the
       briefing's "a large number of enemy cruisers, a few command vessels"
       (manual l.211). */
    for (n = 0; n < gal_enemies[q]; n++) {
        cell = trek_free_sector();
        if (cell == 0xFF) break;
        i = trek_rand_n(10);
        if (i < 6)      sector[cell] = SEC_BATTLESHIP;
        else if (i < 8) sector[cell] = SEC_SCOUT;
        else if (i < 9) sector[cell] = SEC_SUPPLY;
        else            sector[cell] = SEC_COMMAND;
        enemy_hp[cell] = enemy_strength(sector[cell]);
    }

    /* Planets last, so they take a cell nothing else wanted. Leaving the
       quadrant also leaves orbit -- the same rule movement applies to a dock,
       and the reason trek_land() can trust ship.orbiting without rechecking
       where the ship is. */
    trek_leave_orbit();
    planet_place();

    reveal_around(ship.quad_y, ship.quad_x);
}

/* Scheduled event dates, one slot per type -- see trek.h for why a slot
   rather than a queue. Declared here rather than beside the event code
   because trek_new_game seeds the schedule and comes first. */
static uint16_t sched[SCHED_COUNT];

/* For core/serial.c only -- see the note in trek.h. */
uint16_t trek_rng_state(void) { return rng_state; }
void trek_rng_restore(uint16_t v) { rng_state = v ? v : 1; }
void trek_sched_restore(uint8_t kind, uint16_t when) {
    if (kind < SCHED_COUNT) sched[kind] = when;
}
uint8_t  base_under_attack = GAL_CELLS;
uint8_t  bases_lost = 0;

void trek_new_game(uint8_t level, uint16_t seed) {
    uint8_t i, q, bases, placed, last_y, last_x;
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
    total = (uint16_t)(ENEMY_BASE + level * ENEMY_PER_LEVEL
                       + trek_rand_n(ENEMY_SPREAD));
    ship.enemies_left = total;

    while (total) {
        q = trek_rand_n(GAL_CELLS);
        if (gal_enemies[q] >= 4) continue;    /* keep quadrants playable */
        gal_enemies[q]++;
        total--;
    }

    /* BASES, READ OUT OF THE BINARY 2026-08-26 (fn 0x04FD1, 0x053BB and
       0x0553D). Two separate loops, and neither resembles what this port did.

       StarBases first: `11 - V` of them, always in quadrants 2..7 on both
       axes so never on the galaxy's edge, and -- the interesting rule --
       REJECTED IF WITHIN TWO QUADRANTS OF THE PREVIOUS ONE on both axes.
       They are deliberately spread across the map, which is what makes
       running for repairs a real decision rather than a lookup.

       Then two to four research stations and supply depots, ALTERNATING by
       loop parity: odd iterations place a research station, even ones a
       supply depot. Any quadrant, edges included.

       V is [0x1DF0], the same open value as the enemy total -- see
       MEASURED.md. `11 - V` and a `cmp V, 9` elsewhere in the routine only
       make sense if V reaches 9, so V = level + 4 is the reading and gives
       7 - level StarBases: six at level 1, two at level 5. FLAGGED. */
    bases = (uint8_t)(STARBASES_AT_LEVEL(level));
    last_y = last_x = 0;              /* the original starts from 0,0 too */
    for (placed = 0; placed < bases; placed++) {
        uint8_t qy = 0, qx = 0, tries;
        for (tries = 0; tries < 200; tries++) {
            qy = (uint8_t)(1 + trek_rand_n(6));   /* 1-based 2..7 */
            qx = (uint8_t)(1 + trek_rand_n(6));
            q  = (uint8_t)((qy << 3) | qx);
            if (gal_base[q] != BASE_NONE) continue;
            if (abs_diff(last_y, qy) <= 2 && abs_diff(last_x, qx) <= 2)
                continue;                          /* too close to the last */
            break;
        }
        if (tries >= 200) break;      /* the original can spin; this cannot */
        last_y = qy; last_x = qx;
        gal_base[q] = BASE_STARBASE;
    }

    bases = (uint8_t)(2 + trek_rand_n(3));
    for (placed = 0; placed < bases; placed++) {
        uint8_t tries;
        for (tries = 0; tries < 200; tries++) {
            q = trek_rand_n(GAL_CELLS);
            if (gal_base[q] == BASE_NONE) break;
        }
        if (tries >= 200) break;
        /* Odd then even, exactly as the loop counter's low bit selects. */
        gal_base[q] = (uint8_t)((placed & 1) ? BASE_SUPPLY : BASE_RESEARCH);
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
    ship.repair_focus = 0;
    ship.warp         = WARP_START;
    ship.shields_up   = 0;
    ship.stardate     = STARDATE_START;
    ship.time_frac    = 0;
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

    /* Before trek_enter_quadrant(), which places the planets it rolls. */
    planet_new();

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
void trek_advance_time(uint16_t tenths) {
    uint16_t gain = (uint16_t)(tenths * (ENERGY_PER_DAY / 10));

    /* Repair is a FOUR-ENTRY RATE TABLE, not a stack of multipliers. Pick the
       row -- docked or not, focused or not -- and apply it. Composing the two
       instead shipped a combined rate near 7x against the manual's 5x; the
       table and the arithmetic that refutes the product are in trek.h.

       Declarations first: cc65 is C89 and rejects a statement before one,
       while the native build is C99 and does not. `make test` passing is not
       evidence that the port compiles. */
    uint8_t  docked = (uint8_t)(ship.docked != BASE_NONE);
    uint16_t rate   = docked ? REPAIR_PER_STARDATE_DOCKED
                             : REPAIR_PER_STARDATE;
    uint16_t frate  = docked ? REPAIR_PER_STARDATE_FOCUS_DOCKED
                             : REPAIR_PER_STARDATE_FOCUS;

    /* Ten stardates already mends a wrecked system twice over, so clamping
       the multiplier here changes no outcome and keeps 100 * tenths inside
       sixteen bits. The composed version had no such bound: it multiplied an
       already-scaled `mend` by 47 and overflowed near a hundred stardates. */
    uint16_t rt    = tenths > 100 ? 100 : tenths;
    uint16_t mend  = (uint16_t)((rate  * rt) / 10);
    uint16_t fmend = (uint16_t)((frate * rt) / 10);

    uint8_t i;

    ship.stardate = (uint16_t)(ship.stardate + tenths);

    /* The converter's output scales with its own state of repair (manual
       l.367-369), so a wrecked converter cannot dig you out of a hole. */
    gain = (uint16_t)((gain * ship.sys[SYS_CONVERTER]) / 100);

    /* THE CONVERTER TOPS UP; IT NEVER DRAINS. The `>` test alone used to
       assign ENERGY_MAX unconditionally once the sum passed it, which quietly
       CONFISCATED any energy above the maximum on the very next turn. Nothing
       could exceed it until energium could -- a used crystal takes the ship to
       7435 of 5000, MEASURED -- and the overcharge would have evaporated on
       the first tenth of a stardate that passed, with no message and no test
       to catch it. trek_dock() already guarded the same way. */
    if (ship.energy < ENERGY_MAX) {
        if (ship.energy + gain > ENERGY_MAX) ship.energy = ENERGY_MAX;
        else ship.energy = (uint16_t)(ship.energy + gain);
    }

    /* MEASURED: floor(20 * stardates), applied to every damaged system at
       the full rate rather than divided between them.
     *
     * MEASURED 2026-08-24, and "concentrate repairs on" turned out to mean
     * exactly what the manual says: the chosen system runs at its own rate
     * and the others get NOTHING. All four rates are the manual's relatives
     * on a base of 20, confirmed against actual repair rather than against
     * the STATE OF REPAIR dialog's rounded estimate. See trek.h. */
    for (i = 0; i < SYS_COUNT; i++) {
        uint16_t m;
        if (ship.sys[i] >= 100) continue;
        if (ship.repair_focus) {
            /* MEASURED: a focus STARVES everything else. Shields sat at 0%
               for eleven consecutive turns while the focused lasers climbed
               0 to 100. Not a reduced rate -- nothing. */
            if (ship.repair_focus != (uint8_t)(i + 1)) continue;
            m = fmend;
        } else {
            m = mend;
        }
        if (!m) continue;
        if (ship.sys[i] + m >= 100) ship.sys[i] = 100;
        else ship.sys[i] = (uint8_t)(ship.sys[i] + m);
    }
}

/* --------------------------------------------------------------- shields */

uint8_t trek_shields_up(void) {
    if (ship.shields_up) return SHIELD_ALREADY;
    if (ship.energy < SHIELD_RAISE_COST) return SHIELD_NO_ENERGY;

    ship.energy = (uint16_t)(ship.energy - SHIELD_RAISE_COST);
    ship.shields_up = 1;
    return SHIELD_OK;
}

uint8_t trek_shields_down(void) {
    if (!ship.shields_up) return SHIELD_ALREADY;
    /* No refund and no charge: "Lowering shields causes no energy change."  */
    ship.shields_up = 0;
    return SHIELD_OK;
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

    /* MEASURED: docking costs 0.1 stardates, and it is the ONLY thing besides
       movement that moves the clock at all -- the whole turn-cost table is in
       MEASURED.md, "Time advances ONLY on movement and docking". This port
       charged nothing for it until 2026-08-26. Exactly one tenth, so
       trek_advance_time() rather than the hundredths carry: there is no fraction
       to keep. The dock is already recorded above, so the repair this buys is
       at the docked rate, which is what spending the turn alongside means. */
    trek_advance_time(1);

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
/* Now public as trek_take_hit -- see trek.h. */

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
                    trek_take_hit(hit, ev, n, max);
                }
                trek_schedule(SCHED_DEATH_POD, trek_expran(500));
                break;
        }
    }
    return *n;
}

/* Movement returns a status code and has no event list to fill, so the queue
   is drained from here instead of from trek_advance_time(). The turn loop calls
   this once after whatever consumed the turn -- which also means an event
   fires after the move that carried the clock past it, not in the middle of
   it, and the player sees the two in the order they happened. */
uint8_t trek_run_events(TrekEvent *ev, uint8_t max) {
    uint8_t n = 0;
    run_events(ship.stardate, ev, &n, max);
    return n;
}

uint8_t trek_advance(uint16_t tenths, TrekEvent *ev, uint8_t max) {
    trek_advance_time(tenths);
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

/* ------------------------------------------------- damage has consequences */

/* See trek.h for the manual's wording behind each of these. */

uint8_t trek_max_warp(void) {
    /* warp 1 + 0.09 * pct, in tenths: 10 + (9 * pct) / 10. */
    uint16_t tenths = (uint16_t)(10u + (9u * (uint16_t)ship.sys[SYS_WARP]) / 10u);
    if (tenths > WARP_MAX) tenths = WARP_MAX;
    if (tenths < WARP_MIN) tenths = WARP_MIN;
    return (uint8_t)tenths;
}

uint8_t trek_impulse_ok(void) {
    return (uint8_t)(ship.sys[SYS_IMPULSE] >= 50);
}

uint8_t trek_tubes_available(void) {
    uint8_t pct = ship.sys[SYS_TUBES];
    if (pct >= 100) return 3;
    if (pct >= 67)  return 2;
    if (pct >= 34)  return 1;
    return 0;
}

uint8_t trek_srscan_level(void) {
    uint8_t pct = ship.sys[SYS_SRSCAN];
    if (pct < 50) return SCAN_DEAD;
    if (pct < 90) return SCAN_COARSE;
    return SCAN_FULL;
}

uint8_t trek_lrscan_level(void) {
    uint8_t pct = ship.sys[SYS_LRSCAN];
    if (pct < 50)  return SCAN_DEAD;
    if (pct < 100) return SCAN_COARSE;
    return SCAN_FULL;
}

uint8_t trek_autonav_ok(void)     { return (uint8_t)(ship.sys[SYS_COMPUTER] >= 100); }
uint8_t trek_transporter_ok(void) { return (uint8_t)(ship.sys[SYS_TRANSPORTER] >= 100); }
uint8_t trek_shuttle_ok(void)     { return (uint8_t)(ship.sys[SYS_SHUTTLE] >= 100); }

uint8_t trek_laser_eff(void) {
    /* Heat and battle damage both reduce it, so they multiply rather than one
       masking the other: half-repaired lasers do half the damage at any heat. */
    uint16_t v = ((uint16_t)ship.laser_eff * (uint16_t)ship.sys[SYS_LASERS]) / 100u;
    return (uint8_t)v;
}

uint8_t trek_set_warp(uint8_t tenths) {
    /* Damaged warp engines lower the ceiling -- manual, and trek_max_warp(). */
    if (tenths < WARP_MIN || tenths > trek_max_warp()) return 0;
    ship.warp = tenths;
    return 1;
}

/* Charge the clock in HUNDREDTHS, promoting whole tenths as they accumulate.
 *
 * The clock is carried in tenths and MEASURED movement is finer than that: a
 * one-sector hop costs 0.0417 stardates. Truncating each move to a tenth
 * would make short hops free forever, which is a mechanic, not a rounding
 * error. The remainder lives in ship.time_frac and is saved with the game. */
static void advance_hundredths(uint16_t h) {
    h = (uint16_t)(h + ship.time_frac);
    ship.time_frac = (uint8_t)(h % 10u);
    trek_advance_time((uint16_t)(h / 10u));
}

/* --------------------------------------------------------- movement paths */

/* THE ORIGINAL WALKS A STRAIGHT LINE AND STOPS AT THE FIRST OCCUPIED CELL.
 * MEASURED 2026-08-25; the write-up with every sample is in MEASURED.md,
 * "The movement model, entire".
 *
 * Three things this port had wrong, and all three are here:
 *
 *   1. It TELEPORTED. Only the destination cell was tested, so a move across
 *      a star arrived as if the star were not there.
 *   2. A blocked move is a PARTIAL MOVE. The ship ends in the last clear
 *      cell and is billed for the distance it covered -- it is not refused.
 *   3. Quadrant changes walk the same path, through the quadrant being LEFT,
 *      and a move blocked there leaves the ship in that quadrant.
 *
 * The path: n = max(|dy|,|dx|) steps, step i at the real position
 * (y0 + dy*i/n, x0 + dx*i/n), and the cell tested is that position with each
 * coordinate ROUNDED. Halves break AWAY FROM ZERO -- Turbo Pascal's Round.
 * That was not assumed: a move from 5-2 to 7-6 puts step 3 at exactly
 * (6.5, 5.0), where half-up predicts a block at 7-6 and half-down a block at
 * 6-5. The machine said 7-6, twice.
 */

/* NO DIVISION ANYWHERE BELOW, AND THAT IS THE POINT.
 *
 * The obvious way to write the walk is round(delta * i / n) per step per
 * axis. It is also 631 bytes on this target, because every one of those is a
 * 16-bit divide and the walk does two per step. Both quantities the walk
 * needs are monotone in i and advance by at most one per step, so an
 * accumulator and a compare replace the divide entirely and the whole thing
 * fits in eight-bit arithmetic:
 *
 *   ROUNDED, halves away from zero:  round(a*i/n) = floor((2*a*i + n) / 2n).
 *     Seed the accumulator at n -- that is the +n rounding term -- and add
 *     2a each step; the offset ticks up whenever it reaches 2n.
 *   TRUNCATED:  floor(a*i/n). Seed at 0, add a each step, tick at n.
 *
 * Because a <= n, each accumulator gains at most its modulus per step, so the
 * carry is an `if` rather than a loop. The widest intermediate is n + 2a,
 * which is 189 at galaxy scale and stays inside a byte.
 */

/* Euclidean distance in SIXTEENTHS of a sector, over the whole galaxy grid.
 *
 * The 64-entry table is exact and covers everything inside one quadrant, so
 * it still answers every impulse move and every quadrant-index distance. Only
 * an absolute galaxy displacement can exceed 7, and there the integer square
 * root below takes over -- by which point r is at least 8 and the chord
 * between consecutive squares is within 1/(8r) of the curve, under a
 * hundredth of a sector. Using it for the short diagonals instead would be
 * 6% low on sqrt(2), which is why the table is still consulted first.
 *
 * The fraction is found by ADDING, for the same reason as the walk: there are
 * only seventeen possible answers, and sixteen subtractions cost less code
 * than one 16-bit divide. */
static uint16_t path_dist(uint8_t dy, uint8_t dx) {
    uint16_t v, r, sq, d, t;
    uint8_t frac;

    if (dy <= 7 && dx <= 7) return (uint16_t)(trek_dist(dy, dx) >> 4);

    v = (uint16_t)((uint16_t)dy * dy + (uint16_t)dx * dx);
    r = 0; sq = 1;
    while (v >= sq) { v -= sq; sq += 2; r++; }   /* r = isqrt, v = remainder */

    /* Four bits of fraction by binary long division: v is already the
       remainder and is strictly less than d, so four doublings give the
       sixteenths directly. Cheaper in code than either a 16-bit divide or
       sixteen subtractions, and it was measured against both. */
    d = (uint16_t)(2u * r + 1u);
    for (frac = 0, t = 0; t < 4; t++) {
        v <<= 1; frac = (uint8_t)(frac << 1);
        if (v >= d) { v -= d; frac++; }
    }
    return (uint16_t)((r << 4) + frac);
}

/* What the last walk produced. File statics rather than a returned struct:
   both movers want all of it, and a by-value struct return is not something
   to spend bytes on here. */
static uint8_t walk_y, walk_x;      /* absolute cell the ship reaches, 0..63 */
static uint8_t walk_dy, walk_dx;    /* truncated displacement, for billing */

uint8_t trek_block_y, trek_block_x;

/* Walks from the ship toward the absolute cell (ay1, ax1), testing every step
   that falls inside the CURRENT quadrant -- the only one with a sector map;
   the destination is not generated until arrival. Returns 1 if an object
   stopped it, in which case trek_block_y/x name that object's sector. */
static uint8_t walk_path(uint8_t ay1, uint8_t ax1) {
    uint8_t ay0 = (uint8_t)((ship.quad_y << 3) | ship.sec_y);
    uint8_t ax0 = (uint8_t)((ship.quad_x << 3) | ship.sec_x);
    uint8_t upy = (uint8_t)(ay1 >= ay0), upx = (uint8_t)(ax1 >= ax0);
    uint8_t ady = (uint8_t)(upy ? ay1 - ay0 : ay0 - ay1);
    uint8_t adx = (uint8_t)(upx ? ax1 - ax0 : ax0 - ax1);
    uint8_t n   = (uint8_t)(ady > adx ? ady : adx);
    uint8_t n2  = (uint8_t)(n << 1);
    uint8_t ry = 0, rx = 0, ty = 0, tx = 0;   /* rounded and truncated offsets */
    uint8_t rya, rxa;                         /* rounding accumulators */
    uint8_t tya = 0, txa = 0;                 /* truncating accumulators */
    uint8_t i;

    walk_y = ay0; walk_x = ax0;
    walk_dy = 0;  walk_dx = 0;
    if (n == 0) return 0;
    rya = n; rxa = n;                         /* seeded with the +n of a round */

    for (i = 1; i <= n; i++) {
        uint8_t sy, sx;

        rya = (uint8_t)(rya + (ady << 1));
        if (rya >= n2) { rya = (uint8_t)(rya - n2); ry++; }
        rxa = (uint8_t)(rxa + (adx << 1));
        if (rxa >= n2) { rxa = (uint8_t)(rxa - n2); rx++; }

        sy = (uint8_t)(upy ? ay0 + ry : ay0 - ry);
        sx = (uint8_t)(upx ? ax0 + rx : ax0 - rx);

        /* Only the quadrant we are in has a sector map, so once the path
           leaves it the rest is the destination's business and the jump
           completes. */
        if ((uint8_t)(sy >> 3) != ship.quad_y ||
            (uint8_t)(sx >> 3) != ship.quad_x)
            break;

        if (sector[((sy & 7) << 3) | (sx & 7)] != SEC_EMPTY) {
            trek_block_y = (uint8_t)(sy & 7);
            trek_block_x = (uint8_t)(sx & 7);
            /* Billed on the PREVIOUS step's truncated position. Travelling
               the other way the position is ay0 - a*i/n, whose floor is
               ay0 - ceil(a*i/n), so a non-zero accumulator costs one more --
               that is what the `!= 0` terms are. No sample exercised a
               fractional step in the negative direction; the positive ones
               are the two discriminators in MEASURED.md. */
            walk_dy = (uint8_t)(upy ? ty : ty + (tya != 0));
            walk_dx = (uint8_t)(upx ? tx : tx + (txa != 0));
            return 1;
        }

        walk_y = sy;
        walk_x = sx;
        tya = (uint8_t)(tya + ady);
        if (tya >= n) { tya = (uint8_t)(tya - n); ty++; }
        txa = (uint8_t)(txa + adx);
        if (txa >= n) { txa = (uint8_t)(txa - n); tx++; }
    }

    /* Nothing stopped it, so it goes the whole way and the truncated endpoint
       is the target itself. */
    walk_dy = ady;
    walk_dx = adx;
    return 0;
}

uint8_t trek_move_impulse(uint8_t sy, uint8_t sx) {
    uint16_t d16, cost;
    uint8_t blocked;

    if (sy >= QUAD_DIM || sx >= QUAD_DIM) return MOVE_BAD_COORDS;
    if (sy == ship.sec_y && sx == ship.sec_x) return MOVE_SAME_PLACE;
    /* Manual: below 50% the impulse engines simply stop. The original's
       refusal is "ENGINEERING: Move aborted; impulse engines are too damaged
       to use", a hard no rather than a slower move. */
    if (!trek_impulse_ok()) return MOVE_NO_IMPULSE;

    /* The energy refusal is a PRE-CHECK on the move that was asked for. The
       original's wording -- "Move aborted; we have too little energy to get
       there" -- is a judgement about the destination, made before anything
       moves. What is BILLED, below, is the distance actually covered. That
       split is read off the message rather than measured. */
    d16 = path_dist(abs_diff(sy, ship.sec_y), abs_diff(sx, ship.sec_x));
    if ((uint16_t)((d16 * IMPULSE_ENERGY_UNIT) >> 4) > ship.impulse)
        return MOVE_NO_ENERGY;

    blocked = walk_path((uint8_t)((ship.quad_y << 3) | sy),
                        (uint8_t)((ship.quad_x << 3) | sx));

    /* A move stopped on its very first step has not moved the ship, so it
       does not cast off either. Every other outcome breaks the dock -- and
       the orbit, for the same reason: an orbit is a position relative to a
       planet, and one sector of impulse leaves it. Without this, LAND would
       still find ship.orbiting set from across the quadrant. */
    if (walk_dy || walk_dx) { trek_undock(); trek_leave_orbit(); }

    sector[(ship.sec_y << 3) | ship.sec_x] = SEC_EMPTY;
    ship.sec_y = (uint8_t)(walk_y & 7);
    ship.sec_x = (uint8_t)(walk_x & 7);
    sector[(ship.sec_y << 3) | ship.sec_x] = SEC_SHIP;

    /* Billed on the TRUNCATED endpoint -- see path_floor. */
    d16 = path_dist(walk_dy, walk_dx);

    /* Drawn from the impulse engines' own pool, not the main banks --
       confirmed on screen: the original tracks them separately. */
    cost = (uint16_t)((d16 * IMPULSE_ENERGY_UNIT) >> 4);
    ship.impulse = (uint16_t)(ship.impulse - cost);

    /* MEASURED: distance / IMPULSE_STARDATE_DIV stardates, and independent
       of the warp factor. In hundredths that is 100*d/24 = 25*d/6, and d16
       counts sixteenths of a sector, so the whole charge is 25*d16/96. The
       widest case is 25 * 158, well inside sixteen bits. */
    advance_hundredths((uint16_t)((25u * d16) / (6u * 16u)));
    return blocked ? MOVE_BLOCKED : MOVE_OK;
}

uint8_t trek_move_delta(int16_t dy, int16_t dx) {
    int16_t ry = (int16_t)(ship.quad_y * QUAD_DIM + ship.sec_y) + dy;
    int16_t rx = (int16_t)(ship.quad_x * QUAD_DIM + ship.sec_x) + dx;
    uint8_t qy, qx, sy, sx;

    if (ry < 0 || rx < 0 ||
        ry >= GAL_DIM * QUAD_DIM || rx >= GAL_DIM * QUAD_DIM)
        return MOVE_BAD_COORDS;

    qy = (uint8_t)(ry / QUAD_DIM); sy = (uint8_t)(ry % QUAD_DIM);
    qx = (uint8_t)(rx / QUAD_DIM); sx = (uint8_t)(rx % QUAD_DIM);

    /* Staying in this quadrant is an impulse hop. Warping across a couple of
       sectors would charge warp energy for a journey the impulse engines are
       there to make. */
    if (qy == ship.quad_y && qx == ship.quad_x)
        return trek_move_impulse(sy, sx);

    return trek_move_warp(qy, qx, sy, sx);
}

/* Energy for a warp jump of `d88` quadrants, 8.8 fixed point.
 *
 * MEASURED at two points -- about 194 units for one quadrant at warp 5 and
 * 710 at warp 8 -- giving cost = 1.5 * distance * warp^3. Warp rose x1.6 and
 * cost x3.66, which puts the exponent at 2.76; the cube reproduces both
 * within 8%, while the linear model this replaced predicted 320 at warp 8.
 * FITTED from two points, so the exponent is well supported and the 1.5 is
 * not precise. UNTOUCHED by the 2026-08-25 movement work, which corrected the
 * TIME law and the distance it is taken over, not this.
 *
 * Cost is SUBLINEAR in distance: two quadrants at warp 8 cost 1174, only
 * 1.65x the 710 that one cost, where a linear model demanded 2x. A fixed
 * overhead plus a per-distance term fits -- roughly (0.5 + 0.9 * distance) *
 * warp^3, in 1/256ths below.
 *
 * Staged to stay in sixteen bits: warp^3 is built in two steps from tenths
 * (max 512), and the distance factor is divided down before the multiply
 * rather than after, which a single expression could not do without
 * overflowing. */
static uint16_t warp_energy(uint16_t d88) {
    uint16_t d44 = (uint16_t)(d88 >> 4);                      /* max 178 */
    uint16_t wc  = (uint16_t)(((uint16_t)ship.warp *
                               (uint16_t)ship.warp) / 10);    /* max 640 */
    uint16_t f;
    wc = (uint16_t)((wc * (uint16_t)ship.warp) / 100);        /* warp^3 */
    f = (uint16_t)(128 + d44 * 14);
    return (uint16_t)((wc * (f >> 5)) >> 3);
}

/* Stardate cost of a warp jump of `d16` sixteenths of a SECTOR, in hundredths.
 *
 * MEASURED: 11 * distance_in_quadrants / warp^2 -- see WARP_STARDATE_NUM in
 * trek.h for the seven samples. In tenths that is 1375 * d_sectors / warp^2
 * where warp is in tenths, and d16 counts sixteenths, so the numerator is
 * 1375/32 = 42.97 per d16 against a denominator of warp_tenths^2/2. Rounding
 * that to 43 costs 0.07%, which is a fourteenth of a tenth at the very worst
 * case and nothing at any real distance.
 *
 * Staged because 43 * d16 reaches 61,318 and multiplying THAT by ten to reach
 * hundredths would not fit: the quotient and the remainder are scaled
 * separately, and neither exceeds 32,000. */
static uint16_t warp_hundredths(uint16_t d16) {
    uint16_t den = (uint16_t)(((uint16_t)ship.warp * (uint16_t)ship.warp) >> 1);
    uint16_t num = (uint16_t)(43u * d16);
    if (den == 0) den = 1;
    return (uint16_t)((num / den) * 10u + ((num % den) * 10u) / den);
}

uint8_t trek_move_warp(uint8_t qy, uint8_t qx, uint8_t sy, uint8_t sx) {
    uint16_t d16, cost;
    uint8_t ay1, ax1;

    if (qy >= GAL_DIM || qx >= GAL_DIM ||
        sy >= QUAD_DIM || sx >= QUAD_DIM) return MOVE_BAD_COORDS;

    if (qy == ship.quad_y && qx == ship.quad_x)
        return trek_move_impulse(sy, sx);

    ay1 = (uint8_t)((qy << 3) | sy);
    ax1 = (uint8_t)((qx << 3) | sx);

    /* MEASURED 2026-08-25: the distance is taken over ABSOLUTE sector
       positions and divided by eight. This port used to take it over quadrant
       INDICES, which cannot produce run 1's 2.5972 at warp 3 -- that reading
       is 17 sectors, or 2.125 quadrants, and no pair of quadrant indices is
       2.125 apart. */
    d16 = path_dist(abs_diff(ay1, (uint8_t)((ship.quad_y << 3) | ship.sec_y)),
                    abs_diff(ax1, (uint8_t)((ship.quad_x << 3) | ship.sec_x)));

    /* The refusal is judged on the jump asked for, as for impulse. Doubled
       with shields raised -- the one part of the cost the manual states
       outright (l.255). */
    cost = warp_energy((uint16_t)(d16 * 2u));
    if (ship.shields_up) cost = (uint16_t)(cost * 2);
    if (cost > ship.energy) return MOVE_NO_ENERGY;

    /* MEASURED: the departure path is walked through the quadrant being LEFT,
       and an object in it stops the jump there. The ship stays in this
       quadrant, in the last clear sector, and is billed for what it covered:
       four sectors of a westward jump at warp 1.0 cost 5.5000 stardates,
       which is 11 * 0.5 and not 11 * 1. */
    if (walk_path(ay1, ax1)) {
        if (walk_dy || walk_dx) { trek_undock(); trek_leave_orbit(); }

        sector[(ship.sec_y << 3) | ship.sec_x] = SEC_EMPTY;
        ship.sec_y = (uint8_t)(walk_y & 7);
        ship.sec_x = (uint8_t)(walk_x & 7);
        sector[(ship.sec_y << 3) | ship.sec_x] = SEC_SHIP;

        d16 = path_dist(walk_dy, walk_dx);
        cost = warp_energy((uint16_t)(d16 * 2u));
        if (ship.shields_up) cost = (uint16_t)(cost * 2);
        if (cost > ship.energy) cost = ship.energy;
        ship.energy = (uint16_t)(ship.energy - cost);
        advance_hundredths(warp_hundredths(d16));
        return MOVE_BLOCKED;
    }

    /* No trek_undock() here: trek_enter_quadrant() clears the dock as part
       of arriving anywhere, and calling it twice would only cost bytes. */
    ship.quad_y = qy;
    ship.quad_x = qx;
    ship.sec_y  = sy;
    ship.sec_x  = sx;

    ship.energy = (uint16_t)(ship.energy - cost);
    advance_hundredths(warp_hundredths(d16));

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

/* Everything that has to happen when an enemy dies, in one place.
 *
 * It was in two places and they disagreed: the torpedo path counted the kill
 * against ship.killed / killed_cmd and the laser path did not, so laser kills
 * -- which is how most things die -- scored nothing at all and contributed
 * nothing to the kill/day ratio either. Nothing caught it because both paths
 * cleared the sector and decremented the galaxy counters correctly, so the
 * game looked right and only the sheet was wrong. */
static void kill_enemy(uint8_t cell) {
    uint8_t q = (uint8_t)((ship.quad_y << 3) | ship.quad_x);

    if (sector[cell] == SEC_COMMAND) ship.killed_cmd++;
    else                             ship.killed++;

    sector[cell]   = SEC_EMPTY;
    enemy_hp[cell] = 0;
    if (gal_enemies[q]) gal_enemies[q]--;
    if (ship.enemies_left) ship.enemies_left--;
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

/* Kept as a no-op rather than deleted, because the volley IS a real unit of
   the game -- the dialog asks once per live enemy -- and the next thing to be
   measured about it will want somewhere to go. It used to zero the heat, which
   was wrong: MEASURED 2026-08-24, heat accumulates ACROSS volleys within a
   quadrant visit and is cleared by LEAVING the quadrant. Jamie watched the
   gauge do it. trek_enter_quadrant() clears it now. */
void trek_laser_begin_volley(void) {
}

uint8_t trek_fire_laser(uint8_t sy, uint8_t sx, uint16_t energy,
                        uint16_t *damage) {
    uint8_t cell;
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

    /* HEAT IS ON ITS OWN SMALL SCALE AND THE GAME CAPS IT AT 100.
     *
     * MEASURED 2026-08-24 (run 2): the original's Temp gauge is driven by a
     * 16-bit word that is neither a real nor the fired amount -- which is why
     * two searches for it failed -- and the game never lets it exceed 100.
     * The gauge draws it times ten against a 0..1500 scale, so the bar cannot
     * pass its own 1000 tick in normal play.
     *
     * This port stored the raw fired energy, up to 65535, against that same
     * 1500 scale: the bar pegged red after one serious volley and the stored
     * value bore no relation to the original's.
     *
     * HEAT_PER_UNIT is FITTED from a single reading and is the weakest thing
     * here: 1,250 units fired left the gauge "around 700 of its 1500 scale",
     * so a word near 70, giving about 18 units of energy per point. One
     * eyeballed bar position, so treat the shape as measured and the constant
     * as a placeholder. Nothing reads heat but the gauge, so being wrong
     * costs a picture and not a mechanic. */
    {
        /* Summed first and clamped after, deliberately. Testing
           `heat >= CAP - add` UNDERFLOWS when one shot alone exceeds the cap:
           100 - 277 wraps to 65359, the test reads false, and the heat sails
           past the cap it was meant to enforce. The core test caught it.
           Nothing can overflow here -- energy/18 is at most 3,640 and the
           heat it is added to is at most 100. */
        uint16_t h = (uint16_t)(ship.laser_heat + energy / HEAT_PER_UNIT);
        ship.laser_heat = h > LASER_HEAT_CAP ? LASER_HEAT_CAP : h;
    }

    d = trek_dist(abs_diff(sy, ship.sec_y), abs_diff(sx, ship.sec_x));
    dealt = trek_laser_damage(energy, trek_laser_eff(), d);
    if (damage) *damage = dealt;

    if (dealt < enemy_hp[cell]) {
        enemy_hp[cell] = (uint16_t)(enemy_hp[cell] - dealt);
        return FIRE_OK;
    }

    kill_enemy(cell);
    return FIRE_KILL;
}

/* ------------------------------------------------------------ the enemy */

/* An enemy fires its remaining hit points, less a tenth. The ship's CLASS
   does not enter into it -- see trek.h: forcing a supply ship's hit points to
   a Commander's made it fire like a Commander, which is what removed the
   per-class table this function used to carry.

   Staged so the product stays inside 16 bits -- hp * 90 would reach 62550 for
   a Commander and overflow outright for anything wounded off a larger ship.
   Splitting at the hundreds keeps both halves small and the result exact for
   any percentage, which a shortcut like hp - hp/10 would not: that only
   happens to be right because 90 divides evenly, and would quietly compute
   83% if this constant were ever retuned to 85. */
static uint16_t enemy_fire_energy(uint16_t hp) {
    uint16_t whole = (uint16_t)((hp / 100) * ENEMY_FIRE_PCT);
    uint16_t frac  = (uint16_t)(((hp % 100) * ENEMY_FIRE_PCT) / 100);
    return (uint16_t)(whole + frac);
}

/* Damage lands on the shields first and on the main banks once those are
   gone. MEASURED: with shields up and sufficient, the printed figure comes
   out of the shield pool entirely and main energy is untouched. */
/* Wrecks one system, and reports it. MEASURED severity: the resulting
   percentage was ZERO eight times in eleven, and the four survivors read 5%,
   22%, 38% and 42%. This port used to take 20 to 59 points off a system,
   which is a modest bite where the original usually annihilates. */
void trek_wreck_system(uint8_t which, TrekEvent *ev, uint8_t *n, uint8_t max) {
    ship.sys[which] = (trek_rand_n(SYS_WRECK_OF_N) < SYS_WRECK_IN_N)
        ? 0
        : (uint8_t)(SYS_RESIDUAL_MIN + trek_rand_n(SYS_RESIDUAL_SPAN));

    ship.casualties = (uint16_t)(ship.casualties + trek_rand_n(10));
    if (*n < max) {
        ev[*n].kind   = EV_SYSTEM_HIT;
        ev[*n].y      = which;
        ev[*n].amount = ship.sys[which];
        (*n)++;
    }
}

void trek_take_hit(uint16_t amount, TrekEvent *ev, uint8_t *n, uint8_t max) {
    uint16_t absorbed = 0, through, drain = 0;

    /* SHIELDS ARE A PROPORTIONAL ABSORBER -- see the law and its evidence in
       trek.h. Three things changed here at once, and this port had all three
       wrong: the pool used to be drained whether the shields were UP or DOWN;
       it used to subtract rather than absorb a share; and the shield system's
       own state did not enter into it at all. */
    if (ship.shields_up && ship.shields) {
        /* SHIELD_MAX is 2500, so the charge as a percentage is a divide by
           25 and needs no wider arithmetic. scale_pct rounds to nearest and
           is exact at 100, which is what makes full shields with an
           undamaged system absorb the hit WHOLE, as measured. */
        uint8_t charge_pct = (uint8_t)(ship.shields / (SHIELD_MAX / 100));
        absorbed = scale_pct(scale_pct(amount, charge_pct),
                             ship.sys[SYS_SHIELDS]);

        /* THE POOL LOSES FOUR FIFTHS OF WHAT IT STOPS -- see
           SHIELD_ABSORB_NUM in trek.h. The 0.8 in fn 0x16844 is applied where
           the shield real is updated and where the "Shields absorb N" figure
           is printed, NOT to the protection itself, and that is the only
           reading under which all four measurements hold: the drained
           fractions come out 0.32/0.48/0.64 against 0.33/0.47/0.65, AND full
           shields still absorb a hit whole with energy untouched.

           Putting the 0.8 on the protection instead makes full shields let a
           fifth of every hit through, which is measured NOT to happen. The
           test suite caught that immediately, which is the only reason this
           distinction got made rather than shipped. */
        drain = (uint16_t)((absorbed * SHIELD_ABSORB_NUM) / SHIELD_ABSORB_DEN);
        if (absorbed > ship.shields) absorbed = ship.shields;
        if (drain > ship.shields) drain = ship.shields;
        ship.shields = (uint16_t)(ship.shields - drain);
    }

    through = (uint16_t)(amount - absorbed);

    /* MEASURED: the shield SYSTEM takes damage when the POOL absorbs a big
       hit, and it does NOT need anything to get through -- one turn damaged
       it with nothing reaching energy at all. That is why this sits here,
       above the penetration test, rather than inside it. */
    if (absorbed >= SHIELD_SYS_HIT_MIN)
        trek_wreck_system(SYS_SHIELDS, ev, n, max);

    if (!through) return;

    if (through >= ship.energy) {
        ship.energy = 0;
        ship.lost = 1;
        /* All hands. The original scores losing the ship purely as casualties
           -- see MEASURED.md -- so the complement has to land here, not as a
           flat penalty in trek_score(). */
        ship.casualties = CREW_COMPLEMENT;
        if (*n < max) { ev[*n].kind = EV_SHIP_LOST; ev[*n].amount = 0; (*n)++; }
        return;
    }
    ship.energy = (uint16_t)(ship.energy - through);

    /* MEASURED: a penetrating hit wrecks something about three times in five,
       and TWO systems can go in one turn. The threshold itself is still not
       pinned -- no system was ever damaged while the shields took the whole
       hit, and they died on roughly three hits in five once a few hundred
       units were reaching energy. */
    if (through >= SYSTEM_DAMAGE_THRESHOLD &&
        trek_rand_n(SYS_DAMAGE_OF_N) < SYS_DAMAGE_IN_N) {
        uint8_t first = trek_rand_n(SYS_COUNT);
        trek_wreck_system(first, ev, n, max);

        if (trek_rand_n(SYS_SECOND_OF_N) < SYS_SECOND_IN_N) {
            uint8_t second = trek_rand_n(SYS_COUNT);
            /* Never the same one twice in a turn: the original's pairs were
               two DIFFERENT systems, and re-wrecking one already at 0 would
               print a second message saying nothing happened. */
            if (second != first) trek_wreck_system(second, ev, n, max);
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

/* Enemy movement was a port of the ancestor's movebaddy() -- a "forces" score
   built from the enemy's power, how many are present, our shields, energy and
   torpedoes, deciding advance / hold / retreat and how far. All of it is gone,
   because MEASURED 2026-08-21 says the original does something far simpler.

   Thirty-six turns against one Commander, our position changed five times:
   every one of the fourteen turns where it was NOT adjacent produced a move,
   one sector toward the ship. Every turn where it WAS adjacent produced none.
   Fourteen out of fourteen and twenty-two out of twenty-two -- no randomness,
   no retreat at any range, and never more than one sector.

   Holding when adjacent needs no code: the only cell closer is the ship's own,
   and enemy_step() refuses an occupied destination. */

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
    uint8_t cell, dest;
    /* Which cells have already had their turn, so a ship that steps into a
       not-yet-visited cell does not get a second one. */
    uint8_t moved[QUAD_CELLS];
    uint8_t i;

    for (i = 0; i < QUAD_CELLS; i++) moved[i] = 0;

    for (cell = 0; cell < QUAD_CELLS; cell++) {
        if (!SEC_IS_ENEMY(sector[cell]) || moved[cell]) continue;
        /* Commanders and nothing else. MEASURED 2026-08-21: across a long
           session only the Commander ever changed sector, and the console
           narrates it specifically -- "The commander has moved. He is now at
           1-3". It is also exactly what the ancestor's caller does:
           moveklings() runs movebaddy() for the commander and the super
           commander unconditionally, and for ordinary ships only at expert
           skill. Our port moved everything. */
        if (sector[cell] != SEC_COMMAND) continue;

        /* One sector toward the ship, every time. See the note above. */
        {
            dest = enemy_step(cell, 0);
            if (dest == NO_STEP) { moved[cell] = 1; continue; }
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

uint8_t trek_enemy_turn(TrekEvent *ev, uint8_t max, uint8_t player_fired) {
    uint8_t cell, n = 0;
    uint16_t d, energy, dmg;

    /* Only on an attack -- see trek.h. This is a different trigger from the
       ancestor's, where moveklings() runs from the turn resolution whatever
       the player did. */
    if (player_fired) enemies_move(ev, &n, max);

    /* NOT here, deliberately: the ancestor drains a quarter of an enemy's
       power every time it fires. Tested directly against the original -- five
       zero-energy turns while two Mongols shot at us, and neither lost a
       single point. Anderson dropped that rule. See MEASURED.md. */

    for (cell = 0; cell < QUAD_CELLS && n < max; cell++) {
        if (!SEC_IS_ENEMY(sector[cell])) continue;
        if (ship.lost) break;

        /* Enemies hold fire on about half their turns. MEASURED; see trek.h.
           This is per enemy and per turn, so a quadrant full of them still
           delivers something most turns -- it is the single duel that goes
           quiet, which is where it was measured. */
        if (trek_rand_n(ENEMY_FIRE_ONE_IN)) continue;

        d = trek_dist(abs_diff((uint8_t)(cell >> 3), ship.sec_y),
                      abs_diff((uint8_t)(cell & 7), ship.sec_x));
        energy = enemy_fire_energy(enemy_hp[cell]);
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

        trek_take_hit(dmg, ev, &n, max);
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

uint8_t trek_fire_torpedo(uint8_t sy, uint8_t sx, uint16_t *damage) {
    uint8_t  cell, whole;
    uint16_t d, dmg;

    if (damage) *damage = 0;
    if (sy >= QUAD_DIM || sx >= QUAD_DIM) return TORP_BAD_COORDS;
    if (ship.torps == 0) return TORP_NONE_LEFT;

    ship.torps--;
    cell = (uint8_t)((sy << 3) | sx);

    if (sector[cell] == SEC_STAR) return TORP_ABSORBED;
    if (!SEC_IS_ENEMY(sector[cell])) return TORP_MISS;

    d     = trek_dist(abs_diff(sy, ship.sec_y), abs_diff(sx, ship.sec_x));
    whole = (uint8_t)(d >> 8);

    /* Certain inside the sure range, then degrading. MEASURED: 6/6, 6/6, 4/7
       at ranges 2.24, 5.00 and 7.62. */
    if (whole > TORP_SURE_DIST) {
        uint8_t miss = (uint8_t)((whole - TORP_SURE_DIST) * TORP_MISS_PCT_PER_UNIT);
        if (miss > 90) miss = 90;
        if (trek_rand_n(100) < miss) return TORP_MISS;
    }

    /* Same falloff the lasers use, at full efficiency -- torpedoes carry their
       own charge, so the ship's laser heat and battle damage do not enter. */
    dmg = trek_laser_damage(TORP_BASE, 100, d);
    dmg = (uint16_t)(dmg + trek_rand_n(TORP_SPREAD));
    if (dmg > TORP_MAX_DAMAGE) dmg = TORP_MAX_DAMAGE;
    if (damage) *damage = dmg;

    /* It no longer always kills. A Commander at 695 walks away from one with
       340 left, which is MEASURED and is the whole point of the change. */
    if (dmg < enemy_hp[cell]) {
        enemy_hp[cell] = (uint16_t)(enemy_hp[cell] - dmg);
        return TORP_OK;
    }

    kill_enemy(cell);
    return TORP_KILL;
}

uint8_t trek_self_destruct(void) {
    /* IT DESTROYS NOTHING, AND THAT IS MEASURED. This used to carry the
       ancestor's kaboom() -- everything whose power times distance was within
       25 times our remaining energy went with us. EGA Trek dropped it: run 5
       self-destructed with four enemies present, the nearest at range 1.41,
       and killed ZERO. Another ancestor rule Anderson did not take.

       The return value stays, because the UI still reports a count and the
       other loss endings will want the same shape. It is now always nought. */
    ship.lost = 1;
    ship.casualties = CREW_COMPLEMENT;   /* as with any loss of the ship */
    return 0;
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

void trek_score_sheet(ScoreSheet *s) {
    uint16_t elapsed = (uint16_t)(ship.stardate - STARDATE_START);
    uint16_t kills   = (uint16_t)(ship.killed + ship.killed_cmd);

    /* Mechanisms that do not exist yet. Written out rather than left to a
       memset so it is obvious they are zero by absence, not by outcome. */
    s->rescues     = ship.rescues;
    s->rescue_pts  = (int16_t)(ship.rescues * SCORE_PER_RESCUE);
    s->enemy_bases = 0; s->enemy_base_pts = 0;
    s->stars       = 0; s->star_pts      = 0;

    /* MEASURED: a flat -200 for losing the ship, separate from the crew.
       Absent from a surviving ship's sheet, present on both a combat loss and
       a self-destruction. */
    s->ship_lost_pts = ship.lost ? SCORE_SHIP_LOST : 0;

    s->mongols       = ship.killed;
    s->mongol_pts    = (int16_t)(ship.killed * SCORE_PER_MONGOL);
    s->commanders    = ship.killed_cmd;
    s->commander_pts = (int16_t)(ship.killed_cmd * SCORE_PER_COMMANDER);
    s->casualties    = ship.casualties;
    s->casualty_pts  = (int16_t)(0 - (int16_t)ship.casualties);
    s->bases_hit     = bases_lost;
    s->bases_hit_pts = (int16_t)(bases_lost * SCORE_BASE_LOST);

    /* The rate term credits only on a finished mission, and the penalty only
       on an unfinished one -- they are the two halves of one condition. */
    if (ship.enemies_left) {
        s->incomplete_pts  = SCORE_INCOMPLETE;
        s->rate_hundredths = 0;
        s->rate_pts        = 0;
    } else {
        uint16_t t = elapsed < SCORE_MIN_TENTHS ? SCORE_MIN_TENTHS : elapsed;
        s->incomplete_pts  = 0;
        /* kills per stardate, x100, staged to stay inside 16 bits. */
        s->rate_hundredths = (uint16_t)((uint16_t)((kills > 64 ? 64 : kills) * 1000u) / t);
        s->rate_pts        = (int16_t)kill_rate_points(kills, elapsed);
    }

    s->total = (int16_t)(s->ship_lost_pts
                       + s->rescue_pts + s->incomplete_pts + s->mongol_pts
                       + s->commander_pts + s->enemy_base_pts + s->rate_pts
                       + s->casualty_pts + s->star_pts + s->bases_hit_pts);
}

/* Derived from the sheet rather than computed separately, so the total the
   player is shown and the total recorded are the same arithmetic. */
int16_t trek_score(void) {
    ScoreSheet s;
    trek_score_sheet(&s);
    return s.total;
}
