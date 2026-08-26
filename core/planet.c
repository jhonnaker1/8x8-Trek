#include "planet.h"

/* The planet chain. See core/planet.h for where every number in here comes
   from and how much it is worth. */

Planet  planets[PLANET_MAX];
uint8_t planet_count;
uint8_t inventory[ITEM_COUNT];
uint16_t planet_evac_end;

/* The binary's own tables, in the binary's own order. Names, not prose. */
const char *const planet_name[PLANET_NAMES] = {
    "ANDROMEDA", "CETI ALPHA", "CYGNUS", "GALLISTA",
    "GAMMA REGULA", "SIGMA", "VEGA", "XEVIOUS"
};

const char planet_class_letter[PCLASS_COUNT] = { 'M', 'N', 'O' };

const char *const item_name[ITEM_COUNT] = {
    "MONGOL ENERGIUM", "PLASMA BOLTS", "PLASMA BOLT SHIELD",
    "LIFE SUPPORT SUPPLIES", "RAW ENERGIUM"
};

/* -------------------------------------------------------------- new game */

void planet_new(void) {
    uint8_t i;
    uint8_t settled = PLANET_MAX;   /* none, until an energium world lands */

    for (i = 0; i < ITEM_COUNT; i++) inventory[i] = 0;
    ship.orbiting = PLANET_NONE;
    ship.rescues  = 0;
    /* One evacuation per galaxy, announced with one to four stardates of
       warning -- see EVAC_WARNING_MIN_TENTHS. */
    planet_evac_end = (uint16_t)(ship.stardate + EVAC_WARNING_MIN_TENTHS
                                 + trek_rand_n(EVAC_WARNING_SPAN_TENTHS));

    planet_count = (uint8_t)(PLANET_MIN + trek_rand_n(PLANET_SPREAD));

    for (i = 0; i < planet_count; i++) {
        uint8_t q, dup, j, cls1;

        /* ONE PLANET PER QUADRANT. The generator retries the roll while the
           quadrant is occupied, which this port did NOT do -- it allowed
           collisions on the strength of a note that transcribed the PLANET
           LIST page as showing quadrant 6-4 twice. It does not. */
        for (dup = 1; dup;) {
            q = trek_rand_n(GAL_CELLS);
            dup = 0;
            for (j = 0; j < i; j++)
                if (planets[j].quad == q) { dup = 1; break; }
        }
        planets[i].quad  = q;
        planets[i].sec   = 0;
        planets[i].cls   = trek_rand_n(PCLASS_COUNT);
        planets[i].flags = 0;

        /* THE FIND DEPENDS ON THE CLASS -- see planet.h. `cls1` is the
           original's 1-based class, which is what its comparison uses. */
        cls1 = (uint8_t)(planets[i].cls + 1);
        if (cls1 <= trek_rand_n(PFIND_ENERGIUM_OF_N)) {
            planets[i].find = PFIND_ENERGIUM;
            /* THE LAST ENERGIUM PLANET IS THE SETTLED ONE. The original writes
               its quadrant into a single pair of globals here, so each new
               energium world overwrites the last and exactly one survives. */
            settled = i;
        } else if (trek_rand_n(PFIND_MONGOL_OF_N) == 0)
            planets[i].find = PFIND_MONGOL;
        else
            planets[i].find = PFIND_NOTHING;
    }
    if (settled < planet_count) planets[settled].flags |= PF_SETTLED;
    for (; i < PLANET_MAX; i++) {
        /* Slots past the count are cleared rather than left as whatever the
           last galaxy put there. The serialiser writes the whole array, so a
           dirty tail would travel into save files and out again. */
        planets[i].quad  = 0;
        planets[i].sec   = 0;
        planets[i].cls   = 0;
        planets[i].find  = PFIND_NOTHING;
        planets[i].flags = 0;
    }
}

/* ------------------------------------------------------- entering a quadrant

   Called from trek_enter_quadrant() after the ship, the stars and the base
   are down, so a planet takes a cell none of them wanted. */
void planet_place(void) {
    uint8_t q = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
    uint8_t i, cell;

    for (i = 0; i < planet_count; i++) {
        if (planets[i].quad != q) continue;
        cell = trek_free_sector();
        if (cell == 0xFF) continue;
        sector[cell]   = SEC_PLANET;
        planets[i].sec = cell;
    }
}

uint8_t planet_settlement_lost(void) {
    return (uint8_t)(ship.stardate > planet_evac_end);
}

uint8_t planet_distress_here(void) {
    uint8_t q = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
    uint8_t i;
    if (planet_settlement_lost()) return 0;
    for (i = 0; i < planet_count; i++)
        if ((planets[i].flags & PF_SETTLED) && planets[i].quad == q) return 1;
    return 0;
}

uint8_t planet_here(void) {
    uint8_t q = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
    uint8_t i;
    for (i = 0; i < planet_count; i++)
        if (planets[i].quad == q) return i;
    return PLANET_NONE;
}

/* ----------------------------------------------------------------- ORBIT */

static uint8_t adjacent(uint8_t cell) {
    uint8_t cy = (uint8_t)(cell >> 3), cx = (uint8_t)(cell & 7);
    uint8_t dy = (uint8_t)(cy > ship.sec_y ? cy - ship.sec_y : ship.sec_y - cy);
    uint8_t dx = (uint8_t)(cx > ship.sec_x ? cx - ship.sec_x : ship.sec_x - cx);
    return (uint8_t)(dy <= 1 && dx <= 1);
}

uint8_t trek_orbit(void) {
    uint8_t q = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
    uint8_t i;

    if (ship.orbiting != PLANET_NONE) return ORBIT_ALREADY;

    /* Adjacency, exactly as docking works. With two planets possible in a
       quadrant the first adjacent one wins; being adjacent to both at once
       needs them one cell apart, and the placement above cannot do that
       twice over often enough to be worth a tiebreak. */
    for (i = 0; i < planet_count; i++) {
        if (planets[i].quad != q) continue;
        if (!adjacent(planets[i].sec)) continue;
        ship.orbiting = i;
        /* The scan IS the orbit -- one command, no separate cost. */
        planets[i].flags |= PF_SCANNED;
        return ORBIT_OK;
    }
    return ORBIT_NO_PLANET;
}

void trek_leave_orbit(void) {
    ship.orbiting = PLANET_NONE;
}

/* ------------------------------------------------------------------ LAND */

uint8_t trek_land(uint8_t how, uint16_t *casualties) {
    Planet *p;
    uint16_t lost;

    if (casualties) *casualties = 0;
    if (ship.orbiting == PLANET_NONE) return LAND_NO_ORBIT;

    /* "ENGINEERING: Cannot use transporters or shuttlecraft with shields up."
       Both ways down, one rule, and it is checked before the way down is
       because that is the order the original's message implies. */
    if (ship.shields_up) return LAND_SHIELDS_UP;

    if (how == LAND_BY_SHUTTLE) {
        if (!trek_shuttle_ok()) return LAND_DAMAGED;
    } else {
        if (!trek_transporter_ok()) return LAND_DAMAGED;
    }

    p = &planets[ship.orbiting];
    if (p->flags & PF_TAKEN) return LAND_ALREADY;

    /* SETTLERS FIRST, and they are a property of the planet rather than of
       its find -- exactly one world in a galaxy carries them. A ruined
       settlement has nobody left to take off; the scan still shows it, which
       is what the "destroyed " insert is for. */
    if ((p->flags & PF_SETTLED) && !planet_settlement_lost()) {
        p->flags = (uint8_t)(p->flags & ~PF_SETTLED);
        ship.rescues++;
        return LAND_SETTLERS;
    }

    /* The shuttlecraft's round trip, per the manual: 0.2 stardates against
       the transporter's nothing. Charged BEFORE the outcome, since the trip
       happens whatever the landing party finds -- and charged even on a
       Mongol reception, which is when it hurts. */
    if (how == LAND_BY_SHUTTLE) trek_advance_time(2);

    switch (p->find) {
    case PFIND_ENERGIUM:
        p->flags |= PF_TAKEN;
        /* One crystal per planet. The measured USE dialog read `Raw energium
           (1)` after a single mining run, and the ancestor refuses a second
           helping from the same rock outright. */
        if (inventory[ITEM_RAW_ENERGIUM] < 255) inventory[ITEM_RAW_ENERGIUM]++;
        return LAND_ENERGIUM;


    case PFIND_MONGOL:
        /* The station is not cleared by being walked into: PF_TAKEN is NOT
           set, so a second landing party can be sent to the same reception.
           That is the reading of a find the original never says you removed.

           ONE IN FIVE. The binary rolls Random(5) and only a zero is an
           attack, so most visits to a Mongol station come back empty-handed
           rather than short-handed. */
        if (trek_rand_n(LANDING_ATTACK_OF_N) != 0) return LAND_NOTHING;
        lost = (uint16_t)(LANDING_CASUALTY_MIN
                          + trek_rand_n(LANDING_CASUALTY_SPAN));
        ship.casualties = (uint16_t)(ship.casualties + lost);
        if (casualties) *casualties = lost;
        return LAND_ATTACKED;

    default:
        p->flags |= PF_TAKEN;
        return LAND_NOTHING;
    }
}

/* ------------------------------------------------------------------- USE */

uint8_t trek_energium_allowed(void) {
    /* The manual's own two conditions, and both must hold. ENERGY_MAX/5 is
       1000, which is the ancestor's literal threshold as well. */
    if (ship.energy  >= ENERGY_MAX / 5) return 0;
    if (ship.shields >= SHIELD_MAX / 2) return 0;
    return 1;
}

uint8_t trek_use_energium(TrekEvent *ev, uint8_t max) {
    uint8_t roll;
    uint16_t v, gain, loss;

    (void)ev; (void)max;    /* nothing here damages a system -- see planet.h */

    if (!inventory[ITEM_RAW_ENERGIUM]) return USE_NO_ITEM;
    if (!trek_energium_allowed())      return USE_REFUSED;

    inventory[ITEM_RAW_ENERGIUM]--;

    /* ONE ROLL OF SIX, and it is the binary's own. No escalating risk: that
       was the ancestor's mechanic and EGA Trek does not have it. */
    roll = trek_rand_n(CRYSTAL_ROLL_OF_N);

    if (roll == CRYSTAL_DEFECT_ROLL) {
        /* Energy, not a system. Floored at zero rather than allowed to
           underflow -- the original does the same test against 0.0 and
           stores zero when the subtraction goes negative. */
        loss = trek_rand_n16(CRYSTAL_DEFECT_LOSS);
        ship.energy = (uint16_t)(loss >= ship.energy ? 0 : ship.energy - loss);
        return USE_DEFECTIVE;
    }
    if (roll <= CRYSTAL_DUD_TO) return USE_DUD;

    /* Both gains scale by the same V, and the one measurement pins V to five
       -- see planet.h for why it is not the command level. Staged so no
       intermediate leaves 16 bits: 5 x 1399 is 6995, and the gate caps energy
       below 1000 before this runs, so the sum tops out at 7994. */
    v = CRYSTAL_V;
    gain = (uint16_t)(v * (CRYSTAL_ENERGY_BASE
                           + trek_rand_n16(CRYSTAL_ENERGY_SPAN)));
    ship.energy = (uint16_t)(ship.energy + gain);

    /* TOPPED UP, not set to full. The measured 300-to-2500 was this amount
       reaching the ceiling, which is a different mechanic from assignment --
       a badly damaged shield pool does NOT come back full from one crystal. */
    gain = (uint16_t)(v * (CRYSTAL_SHIELD_BASE
                           + trek_rand_n16(CRYSTAL_SHIELD_SPAN)));
    if (ship.shields + gain > SHIELD_MAX) ship.shields = SHIELD_MAX;
    else ship.shields = (uint16_t)(ship.shields + gain);

    return USE_GOOD;
}
