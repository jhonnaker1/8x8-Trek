#include "trek.h"
#include "planet.h"
#include "overlay.h"   /* OVL_CODE; expands to nothing off-target */

uint8_t gal_enemies[GAL_CELLS];
uint8_t gal_base[GAL_CELLS];
uint8_t gal_stars[GAL_CELLS];
uint8_t gal_known[GAL_CELLS];
uint8_t gal_nova[GAL_CELLS];
uint8_t gal_star_gone[GAL_STAR_GONE_BYTES];

/* Two quadrants to a byte, even in the low nibble. See trek.h for why. */
uint8_t trek_star_gone(uint8_t q) {
    uint8_t b = gal_star_gone[q >> 1];
    return (uint8_t)((q & 1) ? (b >> 4) : (b & 0x0F));
}

static void star_gone_add(uint8_t q) {
    uint8_t i = (uint8_t)(q >> 1), b = gal_star_gone[i];
    if (q & 1) { if ((b >> 4) < 15) gal_star_gone[i] = (uint8_t)(b + 0x10); }
    else       { if ((b & 15) < 15) gal_star_gone[i] = (uint8_t)(b + 1); }
}
uint8_t  nova_quad;
uint8_t  pod_alive;
uint8_t  hole_threw;
uint8_t  bolt_cell = QUAD_CELLS;
uint8_t  bolt_quad;
uint16_t nova_kills;
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
        case SEC_BATTLESHIP: return HP_BATTLESHIP_AT(ship.level);
        case SEC_COMMAND:    return HP_COMMAND_AT(ship.level);
        case SEC_SCOUT:      return HP_SCOUT_AT(ship.level);
        case SEC_SUPPLY:     return HP_SUPPLY_AT(ship.level);
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

    /* The quadrant's stars, of which the first gal_star_gone[q] are the ones
       this ship has already destroyed -- the original's own rule, drawn at
       0x016490 as `if (novas[q] >= k) 'N' else '*'`. It is what makes a
       destroyed star still destroyed when you come back. */
    for (n = 0; n < gal_stars[q]; n++) {
        cell = trek_free_sector();
        if (cell != 0xFF)
            sector[cell] = (n < trek_star_gone(q)) ? SEC_NOVA : SEC_STAR;
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
        /* THE FIRST `gal_commander[q]` SHIPS ARE COMMANDERS, from the
           quadrant's own state -- 0x016133. The rest are still rolled here,
           because the original's battleship/scout/supply split is two more
           per-quadrant arrays this core does not carry yet. */
        if (n < gal_commander[q]) {
            sector[cell] = SEC_COMMAND;
        } else {
            i = trek_rand_n(10);
            if (i < 7)      sector[cell] = SEC_BATTLESHIP;
            else if (i < 9) sector[cell] = SEC_SCOUT;
            else            sector[cell] = SEC_SUPPLY;
        }
        enemy_hp[cell] = enemy_strength(sector[cell]);
    }

    /* Planets last, so they take a cell nothing else wanted. Leaving the
       quadrant also leaves orbit -- the same rule movement applies to a dock,
       and the reason trek_land() can trust ship.orbiting without rechecking
       where the ship is. */
    trek_leave_orbit();
    planet_place();

    /* THE VANDAL DEATH POD, decided on arrival. MEASURED on the original
       2026-08-28, and it overturned a reading made the same day: flying to
       quadrant 5-8 mid-game put an 'R' on the scan and set [0x26DF], in a
       quadrant that was not where the game started. So the quadrant fill DOES
       run on entry and the pod IS re-rolled -- see trek.h.

       The roll sits here, at the end of the fill, because that is where the
       original has it: after the stars, the base, the ships and the planet,
       taking a cell nothing else wanted. Note it does NOT consult pod_alive:
       a destroyed pod disarms the detonation for good, but 'R's keep
       appearing in column 8. That is what the binary does -- the placement
       reads [0x26DF] and writes it, and never looks at [0x26E0]. There is no
       pod_here here: [0x26DF] is vestigial in the original (read at one site
       where both arms store the same colour) and sector[] already says where
       the object is. */
    if (ship.quad_x + 1 == POD_QUAD_COLUMN
        && gal_enemies[q] < POD_MAX_SHIPS
        && trek_rand_n(POD_PLACE_OF_N) > POD_PLACE_ABOVE) {
        cell = trek_free_sector();
        if (cell != 0xFF) {
            sector[cell]   = SEC_POD;
            enemy_hp[cell] = POD_HP;
        }
    }

    /* AND A BLACK HOLE, one quadrant in four -- 0x016525, four instructions
       past the pod's roll in the same fill. It takes a free cell like
       everything else, and from here on it is invisible: the UI draws a
       space where vacuum draws a dot. */
    if (trek_rand_n(HOLE_PLACE_OF_N) < HOLE_PLACE_BELOW) {
        cell = trek_free_sector();
        if (cell != 0xFF) sector[cell] = SEC_BLACKHOLE;
    }

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
uint8_t  hail_quad = GAL_CELLS;   /* the base a hail is waiting on */
uint8_t  gal_commander[GAL_CELLS];
/* Set by a warp move that ended somewhere the captain did not choose. The
   UI reads and clears it; nothing else does. */
uint8_t  tractored;
/* Points of SYS_WARP lost to the last move's speed and distance, 0 for none.
   Read and cleared by the UI, exactly like `tractored` above. */
uint8_t  warp_hurt;
uint8_t  laser_overheated;
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
        gal_nova[i]    = 0;
        gal_commander[i] = 0;
    }

    /* READ OUT OF THE BINARY 2026-08-26 -- see trek.h. The level base is
       shaved by 0 to 9 percent and a flat Random(10) added on top, which is
       not the shape three FITTED constants had. */
    total = (uint16_t)(trek_rand_n(ENEMY_SPREAD_OF_N)
                       + ((uint16_t)((level + 1) * ENEMY_PER_LEVEL)
                          * (uint16_t)(100 - trek_rand_n(ENEMY_SHAVE_OF_N)))
                         / 100);

    /* And placed 1..4 AT A TIME, into a quadrant that has none yet -- so the
       galaxy holds a few busy quadrants and many empty ones rather than a
       thin even scatter. The port used to add them one at a time with a cap
       of four, which is a different distribution entirely. */
    {
        uint16_t placed_n = 0;
        while (placed_n < total) {
            uint16_t want;
            q = trek_rand_n(GAL_CELLS);
            if (gal_enemies[q]) continue;        /* one visit per quadrant */
            want = (uint16_t)(1 + trek_rand_n(ENEMY_PER_QUADRANT));
            if (placed_n + want > total) want = (uint16_t)(total - placed_n);
            gal_enemies[q] = (uint8_t)want;
            /* A quadrant that got more than one ship gets a Commander three
               times in seven -- 0x005287, and it is persistent state, not a
               roll made when you arrive. */
            if (want > 1 && level >= COMMANDER_MIN_LEVEL
                && trek_rand_n(COMMANDER_OF_N) < COMMANDER_IN_N)
                gal_commander[q] = 1;
            placed_n = (uint16_t)(placed_n + want);
        }
    }
    ship.enemies_left = total;

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

        /* THE SIEGE. The first StarBase -- and every one of them at level 5 --
           starts with three Mongols on it, if its quadrant is otherwise
           empty. 0x00547B, and the three go on the total as well as into the
           quadrant, which is the term nineteen readings of the enemy count
           were missing. */
        if ((placed == 0 || level == 5) && gal_enemies[q] == 0) {
            gal_enemies[q]     = ENEMY_SIEGE;
            ship.enemies_left  = (uint16_t)(ship.enemies_left + ENEMY_SIEGE);
            /* The besieging force is led -- 0x0054E3 sets the commander byte
               for this quadrant unconditionally. */
            if (level >= COMMANDER_MIN_LEVEL) gal_commander[q] = 1;
        }
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
    ship.life_reserve = LIFE_RESERVE_MAX_TENTHS;
    ship.life_gone    = 0;
    ship.boarders     = BOARD_NONE;
    ship.moved        = 0;
    ship.mutants      = 0;
    ship.board_until  = 0;
    ship.killed       = 0;
    ship.killed_cmd   = 0;
    ship.stars_gone   = 0;
    ship.wear_last    = STARDATE_START;
    ship.lost_how     = LOSS_NONE;
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

    /* [0x26E0] is true from the first turn, and the detonation reads nothing
       else. Set BEFORE the first quadrant is built, which is also where the
       first pod can be placed. */
    pod_alive = 1;
    bolt_cell = QUAD_CELLS;         /* [0x1E28] == 0: nothing in flight */
    bolt_quad = 0;

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
    for (i = 0; i < GAL_STAR_GONE_BYTES; i++) gal_star_gone[i] = 0;
    base_under_attack = GAL_CELLS;
    hail_quad = GAL_CELLS;          /* nothing has been hailed yet */
    bases_lost = 0;

    /* The FIRST attack is `stardate + 2 + Random(4)` in the original --
       the integer Random, so it lands on a whole stardate. Later ones use
       the continuous form. Read at 0x0054F4. */
    /* REINFORCEMENTS ARE A LEVEL 5 MECHANIC and the slot stays at never
       below it -- 0x005830 sets 9999.0 when level+4 < 9. */
    if (ship.level >= REINF_LEVEL)
        trek_schedule(SCHED_REINFORCE,
                      trek_sched_deviate(REINF_FIRST_TENTHS, REINF_FIRST_SPAN));

    trek_schedule(SCHED_DISTRESS,
                  trek_sched_deviate(DISTRESS_AT_TENTHS, DISTRESS_SPAN_TENTHS));
    trek_schedule(SCHED_BASE_ATTACK,
                  (uint16_t)(SCHED_ATTACK_BASE_TENTHS
                             + trek_rand_n(SCHED_FIRST_ATTACK_DAYS) * 10));
}

/* -------------------------------------------------------------- movement */

/* The converter supplies ENERGY_PER_DAY per stardate at full repair
   (manual l.265), so a tenth of a stardate is a tenth of that. Damage is
   not modelled yet -- there is no damage system. */
/* The original sheds this in its main loop, once per command -- see trek.h. */
void trek_turn_end(void) {
    ship.laser_heat = (uint16_t)(ship.laser_heat > LASER_HEAT_COOL_TURN
                                 ? ship.laser_heat - LASER_HEAT_COOL_TURN : 0);
}

void trek_advance_time(uint16_t tenths) {
    uint16_t gain = (uint16_t)(tenths * (ENERGY_PER_DAY / 10));

    /* The banks cool with the clock as well as with the turn -- 360 points a
       stardate, at 0x0201B3, applied in the same routine as the repair. The
       clamp keeps the product inside sixteen bits; anything past two
       stardates has already cooled them to nothing. */
    {
        uint16_t cool = (uint16_t)((tenths > 180 ? 180 : tenths)
                                   * (LASER_HEAT_COOL_DAY / 10));
        ship.laser_heat = (uint16_t)(cool >= ship.laser_heat
                                     ? 0 : ship.laser_heat - cool);
    }

    /* Repair is a FOUR-ENTRY RATE TABLE, not a stack of multipliers. Pick the
       row -- docked or not, focused or not -- and apply it. Composing the two
       instead shipped a combined rate near 7x against the manual's 5x; the
       table and the arithmetic that refutes the product are in trek.h.

       Declarations first: cc65 is C89 and rejects a statement before one,
       while the native build is C99 and does not. `make test` passing is not
       evidence that the port compiles. */
    uint8_t  docked = (uint8_t)(ship.docked == BASE_STARBASE);
    uint16_t rate   = docked ? REPAIR_PER_STARDATE_DOCKED
                             : REPAIR_PER_STARDATE;
    uint16_t frate  = docked ? REPAIR_PER_STARDATE_FOCUS_DOCKED
                             : REPAIR_PER_STARDATE_FOCUS;

    /* Ten stardates already mends a wrecked system twice over, so clamping
       the multiplier here changes no outcome and keeps 100 * tenths inside
       sixteen bits. The composed version had no such bound: it multiplied an
       already-scaled `mend` by 47 and overflowed near a hundred stardates. */
    uint16_t rt    = tenths > 100 ? 100 : tenths;
    uint16_t mend;

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
     * the STATE OF REPAIR dialog's rounded estimate. See trek.h.
     *
     * BINARY 2026-08-27, fn at 0x02024E: the focus is not a rate row, it is
     * a claim on THE CLOCK. The focused system is repaired first out of the
     * whole elapsed time; if that does not finish it the time is spent and
     * nothing else repairs, but if it DOES finish, the overshoot converts
     * back into leftover time at 0x020317 and every system gets that
     * remainder at the ordinary rate -- and the focus clears itself
     * (0x0202F6). Starving the others is the common case, not the law. */
    if (ship.repair_focus) {
        uint8_t  f = (uint8_t)(ship.repair_focus - 1);
        uint16_t v = (uint16_t)(ship.sys[f] + (frate * rt) / 10);
        if (v > 100) {
            /* The original converts the overshoot back to time by 0.01
               stardates a point -- the DOCKED focus scale -- whether or not
               we are docked. Read at 0x02030E and left as it reads. */
            uint16_t used = (uint16_t)((v - 100) / 10);
            rt = (uint16_t)(rt > used ? rt - used : 0);
            ship.sys[f] = 100;
            ship.repair_focus = 0;
        } else {
            ship.sys[f] = (uint8_t)v;
            rt = 0;
        }
    }

    mend = (uint16_t)((rate * rt) / 10);
    if (mend) {
        for (i = 0; i < SYS_COUNT; i++) {
            if (ship.sys[i] >= 100) continue;
            if (ship.sys[i] + mend >= 100) ship.sys[i] = 100;
            else ship.sys[i] = (uint8_t)(ship.sys[i] + mend);
        }
    }

    /* THE LIFE SUPPORT RESERVE, immediately after the repair loop and in the
       same routine -- BINARY, 0x02042E.
     *
     * `tenths`, NOT `rt`. The original subtracts [bp-8], which is the elapsed
     * time copied at 0x020151 BEFORE the focus block writes a remainder back
     * over the parameter. So concentrating repairs does not slow the drain,
     * and reading it off the same variable the repair used would be wrong.
     *
     * Death is at strictly BELOW zero (`jae` at 0x020476), so a reserve of
     * exactly the elapsed time survives at nought and dies on the next tick.
     * Nothing refills it but a base or the item: repairing life support past
     * 90 stops the drain and does not give the reserve back. */
    if (ship.sys[SYS_LIFE] < LIFE_DRAIN_BELOW && ship.docked == BASE_NONE) {
        if (ship.life_reserve >= tenths) {
            ship.life_reserve = (uint16_t)(ship.life_reserve - tenths);
        } else {
            ship.life_reserve = 0;
            if (!ship.lost) { ship.lost = 1; ship.lost_how = LOSS_LIFE;
                              ship.life_gone = 1; }
        }
    }
}

/* --------------------------------------------------------------- shields */

uint8_t trek_shields_up(void) {
    /* Engineering held: 0x00EA12, and it is tested BEFORE "already up". */
    if (ship.boarders == BOARD_ENGINEERING) return SHIELD_BOARDED;
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

    /* EVERY base type refills the reserve -- 0x00F072, ahead of the branch
       that separates them. It is the Research Station's ONLY gift, and the
       reason it is worth flying to at all. */
    ship.life_reserve = LIFE_RESERVE_MAX_TENTHS;

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

/* THE DEATH RAY. fn 0x07375, dispatch at 0x0750E -- see trek.h. The caller
   has already asked the confirmation question. */
uint8_t trek_fire_ray(void) {
    uint8_t cell, roll, any = 0;

    for (cell = 0; cell < QUAD_CELLS; cell++)
        if (SEC_IS_ENEMY(sector[cell])) { any = 1; break; }
    if (!any) return RAY_NO_TARGET;      /* and no turn is spent */

    roll = trek_rand_n(RAY_OF_N);

    if (roll == 0) {
        /* 0x7026 walks the enemy slots and kills every live one. */
        for (cell = 0; cell < QUAD_CELLS; cell++)
            if (SEC_IS_ENEMY(sector[cell])) {
                sector[cell] = SEC_EMPTY;
                enemy_hp[cell] = 0;
                if (ship.enemies_left) ship.enemies_left--;
            }
        return RAY_WORKED;
    }
    if (roll == 2) { ship.mutants = 1; return RAY_MUTANTS; }
    if (roll >= 4) { ship.lost = 1; ship.lost_how = LOSS_RAY; return RAY_FATAL; }

    /* ROLL 3 IS THE ONE THAT FILLS THE QUADRANT WITH BLACK HOLES. Both rolls
       1 and 3 print "Death ray misfires.", which is why this port had them as
       one outcome and trek.h called both COSMETIC -- but 0x0074F5 sends roll
       1 to fn 0x70C0(0), which prints and stops, and roll 3 to fn 0x70C0(1),
       which then walks every cell and turns each EMPTY one into a hole on a
       coin flip. Half the vacuum you are standing in becomes lethal, one RAY
       in six.

       The ship's own cell is EMPTY of nothing -- it holds SEC_SHIP -- so the
       loop cannot bury the ship where it stands, and the original's loop over
       rows and columns 1..8 has the same property. */
    if (roll == 3) {
        for (cell = 0; cell < QUAD_CELLS; cell++)
            if (sector[cell] == SEC_EMPTY
                && trek_rand_n(RAY_HOLE_OF_N) > RAY_HOLE_ABOVE)
                sector[cell] = SEC_BLACKHOLE;
        return RAY_MISFIRE_HOLES;
    }
    return RAY_MISFIRE;                  /* roll 1: the message and nothing */
}

/* Defined down with the torpedo ray march, which is its other caller. The
   hail needs a real distance out of a squared one; see trek.h. */
static uint16_t isqrt16(uint16_t n);

uint8_t trek_hail(uint8_t *qy, uint8_t *qx) {
    uint8_t  lo_y, hi_y, lo_x, hi_x, y, x, c;
    uint16_t best = HAIL_START_Q, d;
    uint8_t  fy = 0, fx = 0, found = 0;

    lo_y = (uint8_t)(ship.quad_y > HAIL_BOX ? ship.quad_y - HAIL_BOX : 0);
    hi_y = (uint8_t)(ship.quad_y + HAIL_BOX < GAL_DIM ? ship.quad_y + HAIL_BOX
                                                      : GAL_DIM - 1);
    lo_x = (uint8_t)(ship.quad_x > HAIL_BOX ? ship.quad_x - HAIL_BOX : 0);
    hi_x = (uint8_t)(ship.quad_x + HAIL_BOX < GAL_DIM ? ship.quad_x + HAIL_BOX
                                                      : GAL_DIM - 1);

    for (y = lo_y; y <= hi_y; y++) {
        for (x = lo_x; x <= hi_x; x++) {
            int16_t dy, dx;
            c = (uint8_t)((y << 3) | x);
            /* A StarBase and nothing else answers. */
            if (gal_base[c] != BASE_STARBASE) continue;
            dy = (int16_t)((int16_t)y - (int16_t)ship.quad_y);
            dx = (int16_t)((int16_t)x - (int16_t)ship.quad_x);
            d  = (uint16_t)(dy * dy + dx * dx);
            if (d < best) { best = d; fy = y; fx = x; found = 1; }
        }
    }

    /* No trek_advance_time() here: hailing costs nothing. See trek.h. */

    /* THE ORDER OF THESE THREE IS THE MECHANIC, and it is read from the
       branches rather than from the order of the addresses -- both message
       paths in the original JUMP OVER the scheduling code that follows them.

       Blocked: nothing goes out, so nothing comes back. */
    if (trek_rand_n(HAIL_BLOCK_OF_N) < HAIL_BLOCK_BELOW) return HAIL_BLOCKED;

    if (!found || best > HAIL_RANGE_Q) {
        /* Out of immediate range. THIS is the branch that schedules: the base
           heard you and its answer is still travelling. `found` is the v<8.0
           gate -- best only leaves HAIL_START_Q when a base was seen, and the
           box cannot hold one further than 32 squared anyway.

           delay = sqrt(25 * best) - 5 tenths; see trek.h for why that is the
           whole of (v-1.0)*0.5 with no fractional root. best > HAIL_RANGE_Q
           means best >= 5, so the delay is at least 6 tenths and never
           underflows. */
        if (found)
            trek_schedule(SCHED_HAIL,
                          (uint16_t)(isqrt16((uint16_t)(25u * best)) - 5u));
        hail_quad = found ? (uint8_t)((fy << 3) | fx) : GAL_CELLS;
        return HAIL_SILENT;
    }

    /* Close enough to answer now, so there is nothing to wait for. */
    if (qy) *qy = fy;
    if (qx) *qx = fx;
    return HAIL_RESPONDS;
}

void trek_undock(void) {
    ship.docked = BASE_NONE;
}

/* The reserve life support canister. BINARY at 0x009861: it adds a whole
   stardate and clamps to the same 2.0 ceiling docking uses, and it is REFUSED
   unless the ship is actually on reserve -- "Not on reserve life support."
   The original tests [0x26D6], the flag the console sets when it swaps the
   panel, so the gate is the PANEL threshold of 100 and not the drain's 90.
   Between 90 and 99 the canister may be spent on a countdown that has not
   started, which is the original's behaviour and not an oversight here. */
uint8_t trek_life_replenish(void) {
    if (ship.sys[SYS_LIFE] >= LIFE_PANEL_BELOW) return 0;
    ship.life_reserve = (uint16_t)(ship.life_reserve + LIFE_RESERVE_ITEM_TENTHS);
    if (ship.life_reserve > LIFE_RESERVE_MAX_TENTHS)
        ship.life_reserve = LIFE_RESERVE_MAX_TENTHS;
    return 1;
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

/* The original's deviate, and it is not the ancestor's. Every reschedule in
   the binary is `stardate + base + spread * Random`, with Turbo Pascal's
   argument-less Random -- flat on [0,1). See trek.h for the eight slots and
   the sites. The 32-entry -ln table this replaced is gone. */
uint16_t trek_sched_deviate(uint16_t base_tenths, uint16_t spread_tenths) {
    return (uint16_t)(base_tenths + (spread_tenths
                                     ? trek_rand_n16(spread_tenths) : 0));
}

/* Fires everything due between now and `until`, in date order, and returns
   how many were reported. The ancestor walks its array picking the earliest
   date under the horizon and repeats; so does this. */
OVL_CODE("events")
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
                /* `stardate + 2 + Random*2` at 0x01FB75. That deadline is
                   the number the COMMUNICATIONS panel prints, and 2..4
                   stardates is what the captured messages showed. */
                trek_schedule(SCHED_BASE_FALLS,
                              trek_sched_deviate(SCHED_FALLS_BASE_TENTHS,
                                                 SCHED_FALLS_SPAN_TENTHS));
                if (ev && *n < max) {
                    ev[*n].kind   = EV_BASE_ATTACKED;
                    ev[*n].y      = (uint8_t)(q >> 3);
                    ev[*n].x      = (uint8_t)(q & 7);
                    ev[*n].amount = sched[SCHED_BASE_FALLS];
                    (*n)++;
                }
                break;
            }
            case SCHED_DISTRESS: {
                /* fn 0x151D0. The slot disarms itself and the DEADLINE
                   starts here, not at game start. */
                uint8_t i, q = GAL_CELLS;
                for (i = 0; i < planet_count; i++)
                    if (planets[i].flags & PF_SETTLED) q = planets[i].quad;
                if (q >= GAL_CELLS) break;      /* nobody left to call */
                planet_evac_end =
                    (uint16_t)(ship.stardate + EVAC_WARNING_MIN_TENTHS
                               + trek_rand_n(EVAC_WARNING_SPAN_TENTHS));
                if (ev && *n < max) {
                    ev[*n].kind   = EV_DISTRESS;
                    ev[*n].y      = (uint8_t)(q >> 3);
                    ev[*n].x      = (uint8_t)(q & 7);
                    ev[*n].amount = planet_evac_end;
                    (*n)++;
                }
                break;
            }
            case SCHED_HAIL: {
                /* THE BASE MUST STILL BE THERE. 0x020689 re-reads the chart
                   word for the remembered quadrant and requires the base type
                   to still be 1 before it prints anything -- so hail a distant
                   base, let the Mongols take it while the signal is in flight,
                   and the answer simply never comes. That is the original's
                   own behaviour and it is the reason this slot remembers a
                   quadrant rather than just a deadline. */
                uint8_t q = hail_quad;
                hail_quad = GAL_CELLS;
                if (q >= GAL_CELLS || gal_base[q] != BASE_STARBASE) break;
                if (ev && *n < max) {
                    ev[*n].kind   = EV_HAIL_REPLY;
                    ev[*n].y      = (uint8_t)(q >> 3);
                    ev[*n].x      = (uint8_t)(q & 7);
                    ev[*n].amount = 0;
                    (*n)++;
                }
                break;
            }
            case SCHED_REINFORCE: {
                /* COLUMN 1 ONLY, and a quadrant with no Mongols in it -- see
                   trek.h for the four places that agree on the column. */
                uint8_t r, q, pick = GAL_CELLS, ships;
                for (r = 0; r < GAL_DIM; r++) {
                    q = (uint8_t)((r << 3) | REINF_COLUMN);
                    if (gal_enemies[q] == 0 && !gal_nova[q]) { pick = q; break; }
                }
                /* Rescheduled whether or not anywhere was found. */
                trek_schedule(SCHED_REINFORCE,
                              trek_sched_deviate(REINF_AGAIN_TENTHS,
                                                 REINF_AGAIN_SPAN));
                if (pick >= GAL_CELLS) break;
                ships = (uint8_t)(REINF_MIN + trek_rand_n(REINF_OF_N));
                gal_enemies[pick] = ships;
                ship.enemies_left = (uint16_t)(ship.enemies_left + ships);
                if (ev && *n < max) {
                    ev[*n].kind   = EV_REINFORCE;
                    ev[*n].y      = (uint8_t)(pick >> 3);
                    ev[*n].x      = (uint8_t)(pick & 7);
                    ev[*n].amount = ships;
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
                trek_schedule(SCHED_BASE_ATTACK,
                              trek_sched_deviate(SCHED_ATTACK_BASE_TENTHS,
                                                 SCHED_ATTACK_SPAN_TENTHS));
                break;
            }
        }
    }
    return *n;
}

/* Movement returns a status code and has no event list to fill, so the queue
   is drained from here instead of from trek_advance_time(). The turn loop calls
   this once after whatever consumed the turn -- which also means an event
   fires after the move that carried the clock past it, not in the middle of
   it, and the player sees the two in the order they happened. */
/* The boarding party, tested every turn -- fn 0x15D6E from the turn loop at
   0x005954. The order is the original's: an expiry is checked BEFORE a new
   arrival is rolled, and the routine returns after reporting one, so the turn
   they leave is never also a turn they arrive. */
static void run_boarders(TrekEvent *ev, uint8_t *n, uint8_t max) {
    uint8_t cell;

    if (ship.boarders != BOARD_NONE) {
        if (ship.stardate > ship.board_until) {
            ship.boarders = BOARD_NONE;
            if (*n < max) { ev[*n].kind = EV_BOARDERS_GONE;
                            ev[*n].y = ev[*n].x = 0;
                            ev[*n].amount = 0; (*n)++; }
        }
        return;                       /* one party at a time, either way */
    }

    if (ship.level < BOARD_MIN_LEVEL) return;
    if (ship.shields_up)              return;   /* they beam in */

    cell = gal_enemies[(ship.quad_y << 3) | ship.quad_x];
    if (cell == 0) return;

    if (trek_rand_n(BOARD_OF_N) <= BOARD_ABOVE) return;

    ship.boarders    = (uint8_t)(trek_rand_n(BOARD_DEPARTMENTS) + 1);
    ship.board_until = (uint16_t)(ship.stardate + BOARD_BASE_TENTHS
                                  + trek_rand_n(BOARD_SPAN_TENTHS));
    if (*n < max) { ev[*n].kind = EV_BOARDED;
                    ev[*n].y = ship.boarders;
                    ev[*n].x = 0; ev[*n].amount = 0; (*n)++; }
}

/* The spontaneous supernova -- fn 0x1ED00, once a turn. See trek.h for the
   whole routine; the two things that surprise are that it never fires
   anywhere in the ship's own quadrant ROW, and that it can finish the
   mission for you. */
/* The mutants, once a turn -- 0x005B1A. One turn in ten they are gone;
   otherwise the crew files another report. No mechanical penalty. */
static void run_mutants(TrekEvent *ev, uint8_t *n, uint8_t max) {
    if (!ship.mutants) return;
    if (trek_rand_n(MUTANT_CLEAR_OF_N) == 0) { ship.mutants = 0; return; }
    if (*n < max) { ev[*n].kind = EV_MUTANTS; ev[*n].y = ev[*n].x = 0;
                    ev[*n].amount = trek_rand_n(5); (*n)++; }
}

static void run_nova(TrekEvent *ev, uint8_t *n, uint8_t max) {
    uint8_t row, col, c, tries, killed;

    if (trek_rand_n(NOVA_OF_N) != 0) return;

    /* The original loops until it finds one. Bounded here: a galaxy with no
       unburnt star outside the ship's row would spin forever, and that state
       is reachable near the end of a long game. */
    for (tries = 0; tries < GAL_CELLS; tries++) {
        row = trek_rand_n(GAL_DIM);
        col = trek_rand_n(GAL_DIM);
        if (row == ship.quad_y) continue;
        c = (uint8_t)((row << 3) | col);
        if (gal_stars[c] == 0 || gal_nova[c]) continue;

        killed = gal_enemies[c];

        /* What was pending there stops being pending. The original clears the
           base-under-attack pair at [0x1E0A] and a second pair at [0x1E1C]
           whose identity is not read; this core clears the marker it has and
           cancels the fall that marker drives, which is the same behaviour
           for the state this core models. */
        if (base_under_attack == c) {
            base_under_attack = GAL_CELLS;
            trek_sched_restore(SCHED_BASE_FALLS, SCHED_NEVER);
        }
        /* And the SETTLED PLANET, which is the other pair the original clears
           here -- [0x1E1C]/[0x1E1E], with its evacuation slot set to never. */
        planet_quadrant_lost(c);
        gal_base[c]    = BASE_NONE;
        gal_enemies[c] = 0;
        gal_stars[c]   = 0;
        gal_nova[c]    = 1;
        gal_known[c]   = 1;      /* the chart learns it without a scan */

        ship.enemies_left = (uint16_t)(ship.enemies_left > killed
                                       ? ship.enemies_left - killed : 0);

        if (*n < max) { ev[*n].kind = EV_NOVA; ev[*n].y = row; ev[*n].x = col;
                        ev[*n].amount = killed; (*n)++; }
        return;
    }
}

/* The Vandal Death Pod, fn 0x20B38. A per-turn roll gated on the pod being
   ALIVE -- galaxy-wide, not on it being in this quadrant. See trek.h: the
   binary opens with `cmp byte [0x26E0], 0` and never reads the position. */
static void run_pod(TrekEvent *ev, uint8_t *n, uint8_t max) {
    uint16_t hit;
    uint8_t  cell, odds;

    if (!pod_alive) return;

    /* `cmp [0x1DE6], 6 / jle` on the 1-based column, so 0-based 6 and 7. */
    odds = (uint8_t)(ship.quad_x + 1 > POD_EDGE_COLUMN ? POD_FIRE_OF_N_EDGE
                                                       : POD_FIRE_OF_N);
    if (trek_rand_n(odds) != 0) return;

    hit = (uint16_t)(POD_HIT_MIN + trek_rand_n(POD_HIT_SPAN));

    /* The SHIELD CHARGE takes it whole -- 0x020B9F, no absorption split and
       no reference to whether the shields are raised. */
    ship.shields = (uint16_t)(ship.shields > hit ? ship.shields - hit : 0);

    /* Everything else in the quadrant takes the same figure. The pod skips
       table type 6, which is itself -- and SEC_IS_ENEMY excludes SEC_POD, so
       an 'R' standing in this sector is spared without a special case. */
    for (cell = 0; cell < QUAD_CELLS; cell++) {
        if (!SEC_IS_ENEMY(sector[cell])) continue;
        if (enemy_hp[cell] > hit) enemy_hp[cell] = (uint16_t)(enemy_hp[cell] - hit);
        else { enemy_hp[cell] = 0; sector[cell] = SEC_EMPTY; }
    }

    if (*n < max) { ev[*n].kind = EV_POD_HIT; ev[*n].y = ship.sec_y;
                    ev[*n].x = ship.sec_x; ev[*n].amount = hit; (*n)++; }

    /* 0x020C64: shields at nothing and the ship is gone. A captain flying on
       a flat charge dies to the first detonation. */
    if (ship.shields == 0) { ship.lost = 1; ship.lost_how = LOSS_POD; }
}

uint8_t trek_events_due(void) {
    uint8_t i;
    for (i = 0; i < SCHED_COUNT; i++)
        if (sched[i] <= ship.stardate) return 1;
    return 0;
}

/* WEAR AND TEAR, fn 0x0213F3. See trek.h for the branches and the reals.
   Per turn, and resident because the ROLL is -- only the report travels. */
static void run_wear(TrekEvent *ev, uint8_t *n, uint8_t max) {
    uint16_t e, sum = 0;
    uint8_t  i, which = SYS_COUNT, roll;

    if (ship.stardate <= WEAR_AFTER_TENTHS) return;
    if (ship.level < WEAR_MIN_LEVEL) return;

    /* e = Round((level + 3) * elapsed). Both are tenths here and the original
       works in whole stardates, so the product is divided back down by ten. */
    e = (uint16_t)(((uint16_t)(ship.level + 3)
                    * (uint16_t)(ship.stardate - ship.wear_last) + 5u) / 10u);
    /* THE ORIGINAL COMPARES SIGNED, and e outruns 98 within a few stardates
       of the last breakdown: `Random(100) > 98 - e` is then always true, not
       true 99 times in 100. Clamping e and comparing unsigned would leave a
       roll of 0 falling through to the converter branch for ever. */
    roll = trek_rand_n(WEAR_ROLL_OF_N);
    if (e > WEAR_ROLL_A_BELOW || roll > (uint8_t)(WEAR_ROLL_A_BELOW - e)) {
        /* Only a ship in near-perfect repair invents a new fault. */
        for (i = 0; i < SYS_COUNT; i++) sum = (uint16_t)(sum + ship.sys[i]);
        if (sum / SYS_COUNT <= WEAR_AVG_ABOVE) return;
        which = trek_rand_n(WEAR_PICK_OF_N);
    } else if (e > WEAR_ROLL_B_BELOW
               || trek_rand_n(WEAR_ROLL_OF_N)
                  > (uint8_t)(WEAR_ROLL_B_BELOW - e)) {
        which = SYS_CONVERTER;
    } else {
        return;
    }

    ship.wear_last = ship.stardate;

    /* 0x020E24: a system already at nothing is left alone. */
    if (ship.sys[which] == 0) return;
    {
        uint8_t loss = (uint8_t)(WEAR_LOSS_MIN + trek_rand_n(WEAR_LOSS_SPAN));
        ship.sys[which] = (uint8_t)(loss >= ship.sys[which]
                                    ? 0 : ship.sys[which] - loss);
    }
    if (ev && *n < max) {
        ev[*n].kind = EV_WEAR; ev[*n].y = 0; ev[*n].x = 0;
        ev[*n].amount = which;
        (*n)++;
    }
}

uint8_t trek_run_events(TrekEvent *ev, uint8_t max) {
    uint8_t n = 0;
    /* Before the scheduled events: the ship is already gone, and anything the
       schedule has to say about bases is happening to someone else. */
    if (ship.life_gone && n < max) {
        ship.life_gone = 0;
        ev[n].kind = EV_LIFE_GONE;
        ev[n].y = ev[n].x = 0;
        ev[n].amount = 0;
        n++;
    }
    /* Only when something is due -- run_events lives in OVL_EVENTS and the
       window holds something else the rest of the time. The platform loads it
       on the same predicate. */
    if (trek_events_due()) run_events(ship.stardate, ev, &n, max);
    run_boarders(ev, &n, max);
    run_nova(ev, &n, max);
    run_pod(ev, &n, max);
    run_mutants(ev, &n, max);
    run_wear(ev, &n, max);
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

/* IN OVL_CMDS. Both callers are commands that already live there -- ENERGY
   and MAX -- and nothing else in the core or the platform calls this. The
   fourth overlay pass, 2026-08-28: the candidates the third pass named
   (run_boarders, run_nova, run_pod) turned out to COST 863 bytes when split,
   because each is called once and inlining them is free. Frequency picks
   candidates, but so does whether the compiler has already merged them into
   their only caller. */
OVL_CODE("cmds")
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
static uint8_t walk_hole;           /* the walk ended IN a black hole */

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
    walk_hole = 0;
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

        /* A BLACK HOLE IS NOT AN OBSTACLE. The path ends here either way,
           but the ship goes IN rather than stopping one short, so the walk
           reports it separately and the caller resolves it. */
        if (sector[((sy & 7) << 3) | (sx & 7)] == SEC_BLACKHOLE) {
            walk_y = sy; walk_x = sx;
            walk_dy = (uint8_t)(upy ? ty : ty + (tya != 0));
            walk_dx = (uint8_t)(upx ? tx : tx + (txa != 0));
            walk_hole = 1;
            return 1;
        }

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

/* THE SHIP HAS ENTERED A BLACK HOLE -- fn 0x0C609 at 0x0D0B6.
 *
 * One time in five it does not come out. Otherwise it is thrown to a
 * uniformly random quadrant AND sector -- the whole galaxy, not a nudge --
 * with burnt quadrants redrawn, which the original does by rejecting a chart
 * word of 999.
 *
 * Called with the ship already standing in the hole's cell, because that is
 * what the walk reports. Returns the MOVE_ code for the caller. */
static uint8_t enter_black_hole(void) {
    uint8_t q, tries;

    if (trek_rand_n(HOLE_FATAL_OF_N) == 0) {
        ship.lost = 1;
        ship.lost_how = LOSS_HOLE;
        ship.casualties = CREW_COMPLEMENT;
        return MOVE_HOLE_LOST;
    }

    /* Uniform over the galaxy, rejecting quadrants the chart shows burnt.
       Bounded, because a galaxy could in principle be all nova and the
       original's loop is not. */
    for (tries = 0; tries < 100; tries++) {
        q = trek_rand_n(GAL_CELLS);
        if (!gal_nova[q]) break;
    }
    ship.quad_y = (uint8_t)(q >> 3);
    ship.quad_x = (uint8_t)(q & 7);
    ship.sec_y  = trek_rand_n(QUAD_DIM);
    ship.sec_x  = trek_rand_n(QUAD_DIM);

    /* The destination is built around wherever it put us, exactly as any
       other arrival is. */
    trek_enter_quadrant();
    hole_threw = 1;
    return MOVE_HOLE_THROWN;
}

uint8_t trek_move_impulse(uint8_t sy, uint8_t sx) {
    /* [0x26E3] in fn 0x0C609: moving buys a 40%% chance the enemy turn is
       skipped. Set on every path that actually moves the ship. */
    ship.moved = 1;

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

    /* AFTER the clock and the pools, because the trip happened: the ship
       flew that far and then fell in. */
    if (walk_hole) return enter_black_hole();
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
/* RESTAGED WHEN THE CEILING WENT TO WARP 10 (2026-08-29), and the overflow
   guards in the test suite are what caught it. Both multiplies below fitted a
   uint16 at warp 8 and neither does at warp 10:

       wc * warp    640 * 80 = 51,200   ->  1000 * 100 = 100,000
       wc * (f>>5)  512 * 81 = 41,472   ->  1000 *  81 =  81,000

   Split at a power of ten and at a power of two respectively, the same
   argument as scale_256() below: `(100q + r) * w / 100` is `q*w + r*w/100`
   exactly, and `(wc * g) >> 3` is `(wc>>3)*g + (((wc&7)*g)>>3)` exactly.
   NO 32-BIT ARITHMETIC -- core/trek.c has none and is not about to gain any
   for a formula that fits with one more line. */
static uint16_t warp_energy(uint16_t d88) {
    uint16_t d44 = (uint16_t)(d88 >> 4);                      /* max 178 */
    uint16_t wc  = (uint16_t)(((uint16_t)ship.warp *
                               (uint16_t)ship.warp) / 10);    /* max 1000 */
    uint16_t f, g;

    /* warp^3, staged: wc = 100*q + r, and (100q + r)*w/100 = q*w + r*w/100. */
    {
        uint16_t q = (uint16_t)(wc / 100), r = (uint16_t)(wc % 100);
        wc = (uint16_t)(q * (uint16_t)ship.warp
                        + (r * (uint16_t)ship.warp) / 100);
    }

    f = (uint16_t)(128 + d44 * 14);
    g = (uint16_t)(f >> 5);                                   /* max 81 */

    /* (wc * g) >> 3, staged at the shift rather than after it. */
    return (uint16_t)((uint16_t)((wc >> 3) * g)
                      + (uint16_t)(((wc & 7u) * g) >> 3));
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
/* The engines break on SPEED AND DISTANCE -- see trek.h above WARP_DMG_AT for
   the branches. Returns the points lost, or 0. Called only from the completed
   warp move: a BLOCKED one never leaves the departure quadrant, so it cannot
   cover the 1.5 quadrants the roll needs, and applying it there would be
   unobservable either way. */
static uint8_t warp_strain(uint16_t d16) {
    /* EIGHT BITS THROUGHOUT, deliberately. The longest jump the galaxy holds
       is corner to corner, 1,425 sixteenths of a sector, which is 111 tenths
       of a quadrant -- so d10 and everything derived from it fit a uint8_t,
       and the 6502 pays for every 16-bit operation it does not need. */
    uint8_t d10 = (uint8_t)((d16 * 5u) / 64u);
    uint8_t over, loss;

    if (ship.warp < WARP_DMG_AT || d10 <= WARP_RISK_FROM) return 0;

    over = (uint8_t)(d10 - WARP_RISK_FROM);
    if (over < WARP_RISK_SPAN && trek_rand_n(WARP_RISK_SPAN) >= over) return 0;

    /* Round(Random * d * 10 + 10): ten points flat, plus up to ten for every
       quadrant crossed. d10 IS d * 10. */
    loss = (uint8_t)(WARP_DMG_BASE + trek_rand_n((uint8_t)(d10 + 1u)));
    if (loss > ship.sys[SYS_WARP]) loss = ship.sys[SYS_WARP];
    ship.sys[SYS_WARP] = (uint8_t)(ship.sys[SYS_WARP] - loss);
    return loss;
}

static uint16_t warp_hundredths(uint16_t d16) {
    uint16_t den = (uint16_t)(((uint16_t)ship.warp * (uint16_t)ship.warp) >> 1);
    uint16_t num = (uint16_t)(43u * d16);
    if (den == 0) den = 1;
    return (uint16_t)((num / den) * 10u + ((num % den) * 10u) / den);
}

uint8_t trek_move_warp(uint8_t qy, uint8_t qx, uint8_t sy, uint8_t sx) {
    /* [0x26E3] in fn 0x0C609: moving buys a 40%% chance the enemy turn is
       skipped. Set on every path that actually moves the ship. */
    ship.moved = 1;

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

        /* The departure path can end in a hole just as an impulse move can,
           and it is resolved the same way -- after the trip is paid for. */
        if (walk_hole) return enter_black_hole();
        return MOVE_BLOCKED;
    }

    /* No trek_undock() here: trek_enter_quadrant() clears the dock as part
       of arriving anywhere, and calling it twice would only cost bytes. */
    {
        uint8_t oy = ship.quad_y, ox = ship.quad_x;

        ship.quad_y = qy;
        ship.quad_x = qx;
        ship.sec_y  = sy;
        ship.sec_x  = sx;

        ship.energy = (uint16_t)(ship.energy - cost);
        advance_hundredths(warp_hundredths(d16));

        /* THE TRACTOR BEAM. Flying PAST a Commander is what gets you caught:
           the original walks the bounding rectangle of the trip and rolls
           two in ten for every quadrant in it that holds one. See
           TRACTOR_OF_N in trek.h. */
        {
            uint8_t y0 = oy < qy ? oy : qy, y1 = oy < qy ? qy : oy;
            uint8_t x0 = ox < qx ? ox : qx, x1 = ox < qx ? qx : ox;
            uint8_t y, x, c;
            for (y = y0; y <= y1; y++) {
                for (x = x0; x <= x1; x++) {
                    c = (uint8_t)((y << 3) | x);
                    if (!gal_enemies[c] || !gal_commander[c]) continue;
                    if (trek_rand_n(TRACTOR_OF_N) <= TRACTOR_ABOVE) continue;
                    ship.quad_y = y;
                    ship.quad_x = x;
                    tractored = 1;
                    y = y1; break;          /* the original stops at the first */
                }
            }
        }
    }

    /* AFTER the trip is paid for and before the new quadrant is built, which
       is where the original does it -- the loss is reported as part of the
       move, not as an event. */
    warp_hurt = warp_strain(d16);

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
    /* Laser control held: 0x009CD0, ahead of every other check. */
    if (ship.boarders == BOARD_LASERS) return FIRE_BOARDED;

    if (sy >= QUAD_DIM || sx >= QUAD_DIM) return FIRE_BAD_COORDS;

    cell = (uint8_t)((sy << 3) | sx);
    /* The pod is a laser target and nothing else is -- see trek.h. It sits
       outside SEC_IS_ENEMY on purpose, so the one place that may shoot it
       has to name it. */
    if (!SEC_IS_ENEMY(sector[cell]) && sector[cell] != SEC_POD)
        return FIRE_NO_TARGET;

    /* The original refuses the shot outright rather than firing what is left
       -- "Captain, we have insufficient energy!" -- and the energy is not
       spent when it refuses. */
    if (energy > ship.energy) return FIRE_NO_ENERGY;
    ship.energy = (uint16_t)(ship.energy - energy);

    /* HEAT IS ON ITS OWN SMALL SCALE AND THE GAME CAPS IT AT 120.
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

    /* OVERHEATING DAMAGES THE BANKS, it does not weaken the shot -- see
       trek.h. It lands AFTER the damage is computed, so this volley is at
       full strength and every one after it is not. */
    laser_overheated = 0;
    if (ship.laser_heat > LASER_OVERHEAT_AT) {
        uint16_t bite = (uint16_t)(dealt / LASER_OVERHEAT_DEN
                                   + trek_rand_n(LASER_OVERHEAT_OF_N));
        ship.sys[SYS_LASERS] = (uint8_t)(bite >= ship.sys[SYS_LASERS]
                                         ? 0 : ship.sys[SYS_LASERS] - bite);
        laser_overheated = 1;
    }

    if (dealt < enemy_hp[cell]) {
        enemy_hp[cell] = (uint16_t)(enemy_hp[cell] - dealt);
        return FIRE_OK;
    }

    /* 0x01E9BE: when the destroyed thing is the pod, both flags are cleared
       and the routine jumps CLEAR of the accounting that follows. No galaxy
       count, no enemies-left, no scoreboard. Nine hundred points of laser
       fire buys you the sky back and nothing on the sheet. */
    if (sector[cell] == SEC_POD) {
        sector[cell]   = SEC_EMPTY;
        enemy_hp[cell] = 0;
        pod_alive      = 0;
        return FIRE_KILL;
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
    /* A UNIFORM BAND, not a flat percentage -- the binary rolls
       `0.6 + Random*0.1`, and eighteen pinned shots put the measured factor
       inside that band 18 times out of 18. See trek.h; the 0.8 that made the
       old scale look wrong was the measurement, not the code. */
    uint8_t  pct   = (uint8_t)(ENEMY_FIRE_PCT_MIN
                               + trek_rand_n(ENEMY_FIRE_PCT_SPAN));
    uint16_t whole = (uint16_t)((hp / 100) * pct);
    uint16_t frac  = (uint16_t)(((hp % 100) * pct) / 100);
    return (uint16_t)(whole + frac);
}

/* Damage lands on the shields first and on the main banks once those are
   gone. MEASURED: with shields up and sufficient, the printed figure comes
   out of the shield pool entirely and main energy is untouched. */
/* What the enemy threw at us this turn, and what the pool stopped -- the
   original's [0x1DC8] and [0x1DC6]. Both are zeroed at the top of the fire
   routine and read once, after it, by the damage roll. Keeping them here
   rather than in Ship is deliberate: they do not survive a turn, so they must
   not reach the save file. */
static uint16_t turn_hits, turn_absorbed;

/* Damages ONE NAMED system by an amount in HIT UNITS, and reports it.
   fn 0x020DCE at 0x020E37..0x020FA7; the law and its constants are in trek.h
   above DMG_FACTOR_UP_X100. `hits` is the turn's total, not one hit. */
/* THE CHART ERODES WHEN THE COMPUTER IS HIT -- see trek.h above
   CHART_KEEP_ABOVE. Called from trek_wreck_system with the system's state
   AFTER the damage, which is what the original tests. */
static void erode_chart(TrekEvent *ev, uint8_t *n, uint8_t max) {
    uint8_t pct = ship.sys[SYS_COMPUTER], q, gone = 0;

    if (pct >= CHART_KEEP_ABOVE) return;

    for (q = 0; q < GAL_CELLS; q++) {
        if (!gal_known[q]) continue;
        /* Under 30 every entry goes; between 30 and 69 each takes its own
           coin flip. The order of the two tests is the original's: the wipe
           is checked first and does not roll. */
        if (pct >= CHART_WIPE_BELOW
            && trek_rand_n(CHART_COIN_OF_N) >= CHART_COIN_BELOW) continue;
        gal_known[q] = 0;
        if (gone < 255) gone++;
    }
    if (gone && ev && *n < max) {
        ev[*n].kind = EV_CHART_LOST; ev[*n].y = 0; ev[*n].x = 0;
        ev[*n].amount = gone;
        (*n)++;
    }
}

void trek_wreck_system(uint8_t which, uint16_t hits,
                       TrekEvent *ev, uint8_t *n, uint8_t max) {
    uint16_t factor, divisor, off, q, r;
    int16_t  left;

    /* 1.25 - charge/2500 with the shields up, a flat 0.5 with them down.
       SHIELD_MAX/100 is 25, so the charge as a percentage is an exact divide
       and the whole factor stays in hundredths. */
    factor = ship.shields_up
        ? (uint16_t)(DMG_FACTOR_UP_X100 - ship.shields / (SHIELD_MAX / 100))
        : DMG_FACTOR_DOWN_X100;

    /* 2 + 3*Random, in hundredths: 2.00 up to but not including 5.00. */
    divisor = (uint16_t)(DMG_DIV_MIN_X100 + trek_rand_n16(DMG_DIV_SPAN_X100));

    /* hits * factor / divisor, rounded, without ever forming hits*factor --
       that overflows 16 bits at a few hundred units. The remainder term is
       bounded by divisor*factor, which is not. */
    q = (uint16_t)(hits / divisor);
    r = (uint16_t)(hits % divisor);
    off = (uint16_t)(q * factor + ((r * factor + divisor / 2) / divisor));

    off = (uint16_t)(off + trek_rand_n(DMG_EXTRA_OF_N));

    left = (int16_t)((int16_t)ship.sys[which] - (int16_t)off);
    /* Anything the hit LEFT above 90 gets a second, smaller bite. It sits
       after the subtraction in the original too, so it only ever fires when
       the main term was small. */
    if (left > DMG_TOPUP_ABOVE)
        left = (int16_t)(left - (DMG_TOPUP_MIN + trek_rand_n(DMG_TOPUP_SPAN)));
    ship.sys[which] = (uint8_t)(left < 0 ? 0 : left);

    /* And the chart, if it was the computer that took it. */
    if (which == SYS_COMPUTER) erode_chart(ev, n, max);

    /* Round(Sign(hits - 500)) * Random(10): no lives are lost on a turn whose
       hits never passed 500. */
    if (hits > DMG_CASUALTY_HIT_MIN)
        ship.casualties = (uint16_t)(ship.casualties
                                     + trek_rand_n(DMG_CASUALTY_OF_N));
    if (*n < max) {
        ev[*n].kind   = EV_SYSTEM_HIT;
        ev[*n].y      = which;
        ev[*n].amount = ship.sys[which];
        (*n)++;
    }
}

/* Once per turn, after the enemies have fired -- fn 0x0213AD, called from the
   main loop at 0x005993 inside a `for r = 1 to Round(hits/350) + 1`. The
   level and penetration gates in that routine are unreachable in play (see
   trek.h), so what is left is the count and a two-in-three roll. */
void trek_combat_damage(TrekEvent *ev, uint8_t *n, uint8_t max) {
    uint16_t rounds, i;

    /* The shield SYSTEM wears from what the POOL stopped, once, on the
       turn's total -- fn 0x016844 at 0x0171CC. A reduction, not a wrecking,
       and it reports nothing of its own. */
    /* The tally is CONSUMED here. trek_enemy_turn() also clears it on entry,
       which is where the original does it -- this second clear is what keeps
       a hit taken outside the fire loop (a death pod) from leaking into the
       next turn's roll, and what makes the function safe to call on its
       own. */
    if (turn_absorbed > SHIELD_SYS_WEAR_MIN && ship.sys[SYS_SHIELDS]) {
        uint16_t wear = (uint16_t)((turn_absorbed - SHIELD_SYS_WEAR_BASE
                                    + SHIELD_SYS_WEAR_DEN / 2)
                                   / SHIELD_SYS_WEAR_DEN);
        ship.sys[SYS_SHIELDS] = (uint8_t)(wear >= ship.sys[SYS_SHIELDS]
                                          ? 0 : ship.sys[SYS_SHIELDS] - wear);
    }

    if (!turn_hits) { turn_absorbed = 0; return; }

    rounds = (uint16_t)((turn_hits + DMG_ROUNDS_PER / 2) / DMG_ROUNDS_PER + 1);
    for (i = 0; i < rounds; i++) {
        if (trek_rand_n(DMG_ROLL_OF_N) == 0) continue;
        trek_wreck_system(trek_rand_n(DMG_SYS_OF_N), turn_hits, ev, n, max);
    }
    turn_hits = turn_absorbed = 0;
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

    /* Everything the turn cost, for the once-per-turn roll after the fire
       loop. The original accumulates both the same way, per hit, and reads
       them once -- system damage is not decided here. */
    turn_hits     = (uint16_t)(turn_hits + amount);
    turn_absorbed = (uint16_t)(turn_absorbed + absorbed);

    if (!through) return;

    if (through >= ship.energy) {
        ship.energy = 0;
        ship.lost = 1;
        ship.lost_how = LOSS_ENEMY;
        /* All hands. The original scores losing the ship purely as casualties
           -- see MEASURED.md -- so the complement has to land here, not as a
           flat penalty in trek_score(). */
        ship.casualties = CREW_COMPLEMENT;
        if (*n < max) { ev[*n].kind = EV_SHIP_LOST; ev[*n].amount = 0; (*n)++; }
        return;
    }
    ship.energy = (uint16_t)(ship.energy - through);
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

/* THE PLASMA BOLT GOES OFF -- fn 0x1658F, called at 0x00592C from inside the
   enemy-turn gate and BEFORE the fire routine. Both of those matter: a turn
   the enemy skips is a turn the bolt does not detonate, and when it does it
   lands before the ordinary shots, so their absorption is computed against
   the charge the bolt has already reduced. That ordering is what made the
   measured turn add up to 722.48 against a predicted 722.65.

   The damage is taken by the SHIELD CHARGE whole -- no absorption split, no
   0.8, and main energy is untouched. */
static void run_bolt(TrekEvent *ev, uint8_t *n, uint8_t max) {
    uint8_t  here = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
    uint16_t reach, dmg;
    uint8_t  by, bx;

    if (bolt_cell >= QUAD_CELLS) return;

    /* Fired somewhere else: it is simply lost -- 0x01659E. */
    if (bolt_quad != here) { bolt_cell = QUAD_CELLS; return; }

    by = (uint8_t)(bolt_cell >> 3);
    bx = (uint8_t)(bolt_cell & 7);
    bolt_cell = QUAD_CELLS;          /* it goes off exactly once, either way */

    /* THE PLASMA BOLT SHIELD stops it dead -- [0x26E1], the captured item.
       This core has the item (ITEM_PLASMA_SHIELD) but nothing that arms it,
       so the test is written and always false for now; see planet.h. */

    /* (8 - d) in 8.8, so nothing past eight sectors. */
    reach = (uint16_t)(BOLT_REACH << 8);
    {
        uint16_t d = trek_dist(abs_diff(by, ship.sec_y),
                               abs_diff(bx, ship.sec_x));
        if (d >= reach) return;
        reach = (uint16_t)(reach - d);
    }

    /* (90 + Random(10)) * (8 - d), the product staged through scale_256 so it
       stays inside sixteen bits: 99 * 2048 does not. */
    dmg = scale_256(reach,
                    (uint16_t)(BOLT_DMG_BASE + trek_rand_n(BOLT_DMG_SPAN)));

    ship.shields = (uint16_t)(ship.shields > dmg ? ship.shields - dmg : 0);

    if (*n < max) { ev[*n].kind = EV_BOLT_HIT; ev[*n].y = by; ev[*n].x = bx;
                    ev[*n].amount = dmg; (*n)++; }
}

uint8_t trek_enemy_turn(TrekEvent *ev, uint8_t max, uint8_t player_fired) {
    uint8_t cell, n = 0;
    uint16_t d, energy, dmg;

    /* THE WHOLE TURN IS GATED, and only when the ship moved -- 0x005917.
       Cleared either way, exactly as the original clears [0x26E3] at 0x00594A
       whether the turn ran or was skipped. */
    {
        uint8_t skip = (uint8_t)(ship.moved
                                 && trek_rand_n(ENEMY_TURN_OF_N)
                                        >= ENEMY_TURN_AFTER_MOVE);
        ship.moved = 0;
        if (skip) return 0;
    }

    /* 0x00592C: the bolt detonates here, before anything else the enemy does
       and inside the gate above. */
    run_bolt(ev, &n, max);

    /* The turn's tally starts here and is read once, at the bottom. Anything
       a scheduled event put in it before now is discarded, which is what the
       original does: [0x1DC8] and [0x1DC6] are zeroed on entry to the fire
       routine and nothing outside it is ever read from them. */
    turn_hits = turn_absorbed = 0;

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

        /* A PLASMA BOLT, 0x016CF4 -- level 3 and up, one in flight at a time,
           six in a hundred per enemy per turn, and only from the two warship
           classes. It takes the ship's CURRENT sector, which is what makes
           moving the counter. Rolled before the ordinary shot because the
           original tests it first, and an enemy that fires a bolt still
           shoots. */
        /* THE ROLL COMES BEFORE THE TYPE TEST, and the order is not
           cosmetic: 0x016CF4 tests the level and the in-flight flag, THEN
           rolls Random(100), and only then asks what class the enemy is. So
           a scout consumes the same draw a Commander does and the stream does
           not depend on who is in the quadrant. Written the other way round
           first, and the suite caught it -- a supply ship and a Commander
           with equal hit points stopped firing for equal damage. */
        if (ship.level >= BOLT_MIN_LEVEL
            && bolt_cell >= QUAD_CELLS
            && trek_rand_n(BOLT_OF_N) > BOLT_ABOVE
            && (sector[cell] == SEC_BATTLESHIP || sector[cell] == SEC_COMMAND)) {
            bolt_cell = (uint8_t)((ship.sec_y << 3) | ship.sec_x);
            bolt_quad = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
            if (n < max) { ev[n].kind = EV_BOLT_FIRED;
                           ev[n].y = (uint8_t)(cell >> 3);
                           ev[n].x = (uint8_t)(cell & 7);
                           ev[n].amount = 0; n++; }
            if (n >= max) break;
        }

        /* Enemies hold fire on about half their turns. MEASURED; see trek.h.
           This is per enemy and per turn, so a quadrant full of them still
           delivers something most turns -- it is the single duel that goes
           quiet, which is where it was measured. */
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

    /* AFTER the loop, on the totals -- not per hit. See trek_combat_damage. */
    if (!ship.lost) trek_combat_damage(ev, &n, max);
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

/* sqrt of a 16-bit square, bit by bit, ROUNDED to nearest. Only the torpedo
   needs it, and only for the graze term -- the direct/graze/through decisions
   are squared compares that need no root at all.

   The rounding is not cosmetic. Truncating puts the smallest graze at 250,
   and 250 is a damage figure the original cannot produce: the graze term is
   (1 - d) * base with d >= 0.3 exactly, so it tops out at 248. See the hole
   in the damage table in trek.h. */
static uint16_t isqrt16(uint16_t n) {
    uint16_t r = 0, b = (uint16_t)1u << 14, t, keep = n;
    while (b > n) b >>= 2;
    while (b) {
        t = (uint16_t)(r + b);
        r >>= 1;
        if (n >= t) { n = (uint16_t)(n - t); r = (uint16_t)(r + b); }
        b >>= 2;
    }
    if ((uint16_t)(keep - r * r) > r) r++;
    return r;
}

/* THE TORPEDO RAY-MARCHES. See trek.h above TORP_DAMAGE_AT_LEVEL for the law
   and where it was read; this is that, in 8.8 fixed point.

   Held ONE-BASED, like the original, so `y & 0xFF` is exactly Turbo Pascal's
   Frac and `(y + 128) >> 8` is exactly its Round. Doing it zero-based would
   put the ship's own row at y = 0, where Frac of a slightly negative number
   stops meaning what the 0.3 and 0.6 tests assume. */
/* A torpedo detonated a star. fn 0x0A8C8, and the whole shape is in trek.h.
   The direction the ship is thrown compares the star's SECTOR row against the
   ship's QUADRANT row, which is dimensionally incoherent and is what the
   original does; it reproduces the one measured throw exactly. */
static uint8_t star_supernova(uint8_t sy, uint8_t sx, uint16_t *damage) {
    uint8_t nqy = ship.quad_y, nqx = ship.quad_x, here, dest, i;
    uint16_t dmg;

    if      (sy < ship.quad_y && ship.quad_y < GAL_DIM - 1) nqy++;
    else if (sy > ship.quad_y && ship.quad_y > 0)           nqy--;
    else if (sx < ship.quad_x && ship.quad_x < GAL_DIM - 1) nqx++;
    else if (ship.quad_y < GAL_DIM - 1)                     nqy++;
    else                                                    nqy--;

    here = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
    dest = (uint8_t)((nqy << 3) | nqx);

    /* Everything in the quadrant goes, and the four hp slots are zeroed
       outright at 0x00AAC8 -- there is no survival roll. */
    nova_kills = gal_enemies[here];
    if (nova_kills) {
        ship.enemies_left = (uint16_t)(ship.enemies_left > nova_kills
                                       ? ship.enemies_left - nova_kills : 0);
        for (i = 0; i < QUAD_CELLS; i++)
            if (SEC_IS_ENEMY(sector[i])) { sector[i] = SEC_EMPTY; enemy_hp[i] = 0; }
    }
    if (base_under_attack == here) {
        base_under_attack = GAL_CELLS;
        trek_sched_restore(SCHED_BASE_FALLS, SCHED_NEVER);
    }

    dmg = trek_rand_n16(NOVA_HIT_MAX);
    if (ship.shields_up) {
        ship.shields = (uint16_t)(ship.shields > dmg ? ship.shields - dmg : 0);
    } else {
        ship.energy = (uint16_t)(ship.energy > dmg ? ship.energy - dmg : 0);
        /* 0x00ABFE adds it to the TURN'S hit tally, so a supernova can wreck
           systems through the ordinary Round(hits/350)+1 path. */
        turn_hits = (uint16_t)(turn_hits + dmg);
    }
    if (damage) *damage = dmg;

    planet_quadrant_lost(here);
    gal_enemies[here] = 0;
    gal_base[here]    = BASE_NONE;
    gal_stars[here]   = 0;
    gal_nova[here]    = 1;

    nova_quad  = dest;
    ship.moved = 1;          /* 0x00AC9B -- being thrown counts as moving */

    /* Thrown into a quadrant that is already burnt: "Lexington destroyed."
       0x00AC2F, and it is why the "blown to quad" line is suppressed there --
       you never arrive. */
    if (gal_nova[dest]) { ship.lost = 1; ship.lost_how = LOSS_NOVA;
                         return TORP_NOVA; }

    ship.quad_y = nqy;
    ship.quad_x = nqx;
    trek_enter_quadrant();
    return TORP_NOVA;
}

uint8_t trek_fire_torpedo(uint8_t sy, uint8_t sx, uint16_t *damage) {
    int16_t  y, x, stepy, stepx;
    uint8_t  m, cy, cx, cell, c;
    uint16_t fy, fx, q, base, dmg, wobble;

    if (damage) *damage = 0;
    /* EnTorp control held: 0x00B60B, ahead of every other check. */
    if (ship.boarders == BOARD_TUBES) return TORP_BOARDED;
    if (sy >= QUAD_DIM || sx >= QUAD_DIM) return TORP_BAD_COORDS;
    if (ship.torps == 0) return TORP_NONE_LEFT;

    ship.torps--;

    /* Aiming at our own cell leaves the original's step scalar uninitialised,
       so there is nothing to reproduce. Refuse it. */
    if (sy == ship.sec_y && sx == ship.sec_x) return TORP_MISS;

    y = (int16_t)((ship.sec_y + 1) << 8);
    x = (int16_t)((ship.sec_x + 1) << 8);

    /* "Firing torpedos through the shields tends to throw them somewhat off
       course" (manual). It is charge/25000 of a cell, once, at launch -- so a
       full 2500 buys a tenth of a cell of error and a flat pool buys none. */
    if (ship.shields_up) {
        wobble = (uint16_t)(ship.shields / TORP_DEFLECT_DEN);
        if (wobble) {
            y = (int16_t)(y + (int16_t)trek_rand_n16((uint16_t)(wobble + 1)));
            x = (int16_t)(x + (int16_t)trek_rand_n16((uint16_t)(wobble + 1)));
        }
    }

    /* step = 1/max(|dy|,|dx|), so the dominant axis advances exactly one cell
       a step. Kept UNSIGNED and negated afterwards: a signed 16-bit divide
       costs the C128 image several hundred bytes and this needs none. */
    {
        uint8_t ay = (uint8_t)(sy > ship.sec_y ? sy - ship.sec_y : ship.sec_y - sy);
        uint8_t ax = (uint8_t)(sx > ship.sec_x ? sx - ship.sec_x : ship.sec_x - sx);
        m = (uint8_t)(ay > ax ? ay : ax);
        stepy = (int16_t)((uint16_t)((uint16_t)ay << 8) / m);
        stepx = (int16_t)((uint16_t)((uint16_t)ax << 8) / m);
        if (sy < ship.sec_y) stepy = (int16_t)(-stepy);
        if (sx < ship.sec_x) stepx = (int16_t)(-stepx);
    }

    for (;;) {
        /* BOTH wobbles are one-sided in the original, so torpedoes drift
           toward higher y and x the further they fly. That asymmetry IS the
           accuracy model -- see trek.h. */
        y = (int16_t)(y + stepy + (int16_t)trek_rand_n16(TORP_JITTER_88));
        x = (int16_t)(x + stepx + (int16_t)trek_rand_n16(TORP_JITTER_88));

        if (y < TORP_EDGE_LO_88 || y > TORP_EDGE_HI_88 ||
            x < TORP_EDGE_LO_88 || x > TORP_EDGE_HI_88) return TORP_MISS;

        cy = (uint8_t)(((y + 128) >> 8) - 1);
        cx = (uint8_t)(((x + 128) >> 8) - 1);
        if (cy >= QUAD_DIM || cx >= QUAD_DIM) return TORP_MISS;

        cell = (uint8_t)((cy << 3) | cx);
        c    = sector[cell];
        /* Our own cell reads 'E' in the original and matches none of its
           cases, so the torpedo flies over it. */
        if (c == SEC_EMPTY || c == SEC_SHIP) continue;

        /* A BLACK HOLE EITHER SWALLOWS THE TORPEDO OR TURNS IT -- fn 0x0AFE7.
           Under 0.3 of the cell's corner it is sucked in and the shot is
           over; otherwise the caller SWAPS dy and dx, which mirrors the
           flight ninety degrees about the diagonal and lets it fly on. The
           0.3 is the same threshold a direct hit uses, so it is the same
           constant -- see TORP_DIRECT_Q.

           This is the one case that continues the march with the STEP
           changed, which is why it lives inside the loop rather than in the
           dispatch below. */
        if (c == SEC_BLACKHOLE) {
            uint16_t hy = (uint16_t)(y & 0xFF), hx = (uint16_t)(x & 0xFF);
            if (hy <= TORP_FRAC_MAX && hx <= TORP_FRAC_MAX
                && (uint16_t)(hy * hy + hx * hx) < TORP_DIRECT_Q)
                return TORP_SWALLOWED;
            { int16_t t = stepy; stepy = stepx; stepx = t; }
            continue;
        }
        break;
    }

    if (c == SEC_STAR) {
        /* fn 0x0A8C8: four times in a hundred the star goes up. The other
           96% is TORP_ABSORBED here; the original splits it 38.4/57.6
           between "absorbed" and "the star is DESTROYED, cell -> 'N'", and
           that second case wants a nova sector code this core does not have
           yet. One branch of three, and it says so. */
        if (trek_rand_n(NOVA_STAR_OF_N) > NOVA_STAR_ABOVE)
            return star_supernova(cy, cx, damage);
        if (trek_rand_n(STAR_ABSORB_OF_N) < STAR_ABSORB_BELOW)
            return TORP_ABSORBED;

        /* THE MAJORITY CASE: the star is destroyed. The cell becomes 'N' and
           the quadrant remembers, so it is still 'N' when you come back --
           see trek.h. Both this and the supernova score against you. */
        {
            uint8_t q = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
            sector[cell] = SEC_NOVA;
            if (trek_star_gone(q) < gal_stars[q]) star_gone_add(q);
            if (ship.stars_gone < 0xFFFFu) ship.stars_gone++;
        }
        return TORP_STAR_GONE;
    }
    if (c == SEC_PLANET) return TORP_PLANET;
    if (c == SEC_BASE)   return TORP_BASE_HIT;
    /* 0x00BE93. The flight stops on the pod and nothing is computed: no
       graze test, no dud roll, no damage, and the hit points are not even
       read. The torpedo is already spent. */
    if (c == SEC_POD)    return TORP_CLOAKED;

    /* How far the torpedo passed from the cell's own corner, SQUARED -- see
       trek.h. The early-out on either fraction alone is what keeps the sum
       inside sixteen bits without losing a bit of the boundary. */
    fy = (uint16_t)(y & 0xFF);
    fx = (uint16_t)(x & 0xFF);
    if (fy > TORP_FRAC_MAX || fx > TORP_FRAC_MAX) return TORP_THROUGH;
    q = (uint16_t)(fy * fy + fx * fx);

    if (q >= TORP_GRAZE_Q) return TORP_THROUGH;
    if (trek_rand_n(TORP_DUD_OF_N) == 0) return TORP_DUD;

    base = TORP_DAMAGE_AT_LEVEL(ship.level);
    if (q < TORP_DIRECT_Q) {
        dmg = base;
    } else {
        /* Round((1 - d) * base). base is a whole number, so the rounding can
           come off the subtrahend and the product stays inside 16 bits. */
        uint16_t d88 = isqrt16(q);       /* d in 8.8; q is d*d in 16.16 */
        dmg = (uint16_t)(base - (uint16_t)(((d88 * base) + 128) >> 8));
    }
    if (damage) *damage = dmg;

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
    ship.lost_how = LOSS_SELF;
    ship.casualties = CREW_COMPLEMENT;   /* as with any loss of the ship */
    return 0;
}

/* --------------------------------------------------------- mission state */

uint8_t trek_game_state(void) {
    if (ship.lost) return GAME_LOST;
    if (ship.enemies_left == 0) return GAME_WON;
    return GAME_ON;
}

/* 500 x kills / elapsed_stardates, in integers and without leaving 16 bits.
 * The 500 and the three-stardate gate are both read from fn 0x1DD4F -- see
 * SCORE_MIN_TENTHS in trek.h.
 *
 * Done as (500 * kills) * 10 / tenths rather than 5000 * kills / tenths: the
 * first product is at most 500 x 64 = 32000 and fits, while 5000 x 64 does
 * not. The remainder is carried so a fractional stardate is not simply
 * dropped -- at the gate, dropping it would cost up to 30%. */
static uint16_t kill_rate_points(uint16_t kills, uint16_t tenths) {
    uint16_t a, q, r;

    /* A HARD ZERO under three stardates, not a clamp. The original gives no
       credit at all before then, and no floor after it. */
    if (tenths < SCORE_MIN_TENTHS) return 0;
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

/* Only ever reached from the evaluation screen, which is itself an overlay --
   so 1.3K of resident image was being held for a page shown once a game. The
   macro is a no-op off-target and on any platform with room. */
OVL_CODE("eval")
void trek_score_sheet(ScoreSheet *s) {
    uint16_t elapsed = (uint16_t)(ship.stardate - STARDATE_START);
    uint16_t kills   = (uint16_t)(ship.killed + ship.killed_cmd);

    /* Mechanisms that do not exist yet. Written out rather than left to a
       memset so it is obvious they are zero by absence, not by outcome.
       Stars left this list on 2026-08-29 when SEC_NOVA was built. */
    s->rescues     = ship.rescues;
    s->rescue_pts  = ship.lost ? 0
                   : (int16_t)(ship.rescues * SCORE_PER_RESCUE);
    s->enemy_bases = 0; s->enemy_base_pts = 0;
    s->stars       = ship.stars_gone;
    s->star_pts    = (int16_t)((int16_t)ship.stars_gone * SCORE_PER_STAR);

    /* A flat -200 for losing the ship, separate from the crew. The binary
       decides it by testing whether the SHIELD REAL is still above zero --
       the game zeroes it when the ship dies (0x00A6FF, 0x016FA3) and uses it
       as the survival flag at 0x01E3A0. `ship.lost` is the same thing said
       properly.

       AND IT IS THE SAME BRANCH AS THE RESCUES: `if (survived) score +=
       rescues*200 else score -= 200`. Lose the ship and the people you
       rescued earn you nothing. */
    s->ship_lost_pts = ship.lost ? SCORE_SHIP_LOST : 0;

    s->mongols       = ship.killed;
    s->mongol_pts    = (int16_t)(ship.killed * SCORE_PER_MONGOL);
    s->commanders    = ship.killed_cmd;
    s->commander_pts = (int16_t)(ship.killed_cmd * SCORE_PER_COMMANDER);
    s->casualties    = ship.casualties;
    s->casualty_pts  = (int16_t)(0 - (int16_t)ship.casualties);
    s->bases_hit     = bases_lost;
    s->bases_hit_pts = (int16_t)(bases_lost * SCORE_BASE_LOST);

    /* THE TWO ARE INDEPENDENT. The -300 depends on enemies remaining; the
       rate term depends only on the clock. This port used to pair them, so a
       captain who ran out of time scored nothing for the ships he did kill.
       The original credits him. */
    s->incomplete_pts  = ship.enemies_left ? SCORE_INCOMPLETE : 0;
    if (elapsed < SCORE_MIN_TENTHS) {
        s->rate_hundredths = 0;
        s->rate_pts        = 0;
    } else {
        /* kills per stardate, x100, staged to stay inside 16 bits. */
        s->rate_hundredths =
            (uint16_t)((uint16_t)((kills > 64 ? 64 : kills) * 1000u) / elapsed);
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
