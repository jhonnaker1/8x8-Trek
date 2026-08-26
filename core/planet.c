#include "planet.h"

/* The planet chain. See core/planet.h for where every number in here comes
   from and how much it is worth. */

Planet  planets[PLANET_MAX];
uint8_t planet_count;
uint8_t inventory[ITEM_COUNT];

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

/* The ancestor's cryprob, which doubles with every crystal loaded. Module
   state rather than a Ship field because it is a property of how far the
   captain has pushed his luck, and it is one byte the save file carries. */
static uint8_t crystal_defect_pct;

uint8_t planet_defect_pct(void)          { return crystal_defect_pct; }
void    planet_defect_restore(uint8_t v) { crystal_defect_pct = v; }

/* -------------------------------------------------------------- new game */

void planet_new(void) {
    uint8_t i, roll;

    for (i = 0; i < ITEM_COUNT; i++) inventory[i] = 0;
    crystal_defect_pct = CRYSTAL_DEFECT_PCT_0;
    ship.orbiting = PLANET_NONE;
    ship.rescues  = 0;

    planet_count = (uint8_t)(PLANET_MIN + trek_rand_n(PLANET_SPREAD));

    for (i = 0; i < planet_count; i++) {
        /* No uniqueness test on the quadrant, deliberately: the measured
           PLANET LIST had two planets in 6-4, so collisions are the original's
           behaviour rather than a bug to design out. Nor on the name -- the
           same list carried Gallista twice. */
        planets[i].quad  = trek_rand_n(GAL_CELLS);
        planets[i].sec   = 0;
        planets[i].name  = trek_rand_n(PLANET_NAMES);
        planets[i].cls   = trek_rand_n(PCLASS_COUNT);
        planets[i].flags = 0;

        roll = trek_rand_n(PFIND_ROLL_OF_N);
        if      (roll <= PFIND_ENERGIUM_TO) planets[i].find = PFIND_ENERGIUM;
        else if (roll <= PFIND_SETTLERS_TO) planets[i].find = PFIND_SETTLERS;
        else if (roll <= PFIND_MONGOL_TO)   planets[i].find = PFIND_MONGOL;
        else                                planets[i].find = PFIND_NOTHING;
    }
    for (; i < PLANET_MAX; i++) {
        /* Slots past the count are cleared rather than left as whatever the
           last galaxy put there. The serialiser writes the whole array, so a
           dirty tail would travel into save files and out again. */
        planets[i].quad  = 0;
        planets[i].sec   = 0;
        planets[i].name  = 0;
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

    case PFIND_SETTLERS:
        /* A ruined settlement has nobody left to take off. The scan still
           shows it -- that is what the "destroyed " insert is for -- so the
           landing is allowed and finds nothing, rather than being refused. */
        if (p->flags & PF_RUINED) {
            p->flags |= PF_TAKEN;
            return LAND_NOTHING;
        }
        p->flags |= PF_TAKEN;
        ship.rescues++;
        return LAND_SETTLERS;

    case PFIND_MONGOL:
        /* The station is not cleared by being walked into: PF_TAKEN is NOT
           set, so a second landing party can be sent to the same reception.
           That is the reading of a find the original never says you removed. */
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
    uint8_t n = 0;
    uint16_t gain;

    if (!inventory[ITEM_RAW_ENERGIUM]) return USE_NO_ITEM;
    if (!trek_energium_allowed())      return USE_REFUSED;

    inventory[ITEM_RAW_ENERGIUM]--;

    /* The gamble, and it gets worse every time. Roll the catastrophe first:
       a defective crystal is a defective crystal whether or not it would
       also have been a dud. */
    if (trek_rand_n(100) < crystal_defect_pct) {
        /* Doubled on a FAILURE only would make the mechanic safer the more it
           misfires, which is backwards. The ancestor doubles on success; this
           doubles on use, which covers both and is the same thing whenever a
           failure is survivable. */
        if (crystal_defect_pct <= 50) crystal_defect_pct *= 2;
        trek_wreck_system(SYS_CONVERTER, ev, &n, max);
        return USE_DEFECTIVE;
    }
    if (crystal_defect_pct <= 50) crystal_defect_pct *= 2;

    if (trek_rand_n(100) < CRYSTAL_DUD_PCT) return USE_DUD;

    /* OVERCHARGE. Deliberately past ENERGY_MAX -- see planet.h. The sum
       cannot leave 16 bits: the gate above caps energy at 999 before this
       runs, so the worst case is 999 + 9499. */
    gain = (uint16_t)(CRYSTAL_ENERGY_BASE + trek_rand_n16(CRYSTAL_ENERGY_SPAN));
    ship.energy  = (uint16_t)(ship.energy + gain);
    ship.shields = SHIELD_MAX;
    return USE_GOOD;
}
