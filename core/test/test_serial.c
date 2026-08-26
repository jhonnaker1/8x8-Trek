/* The disk seam's core half: state to bytes and back.
 *
 * Runs on the build machine with no disk and no emulator, which is the whole
 * point of putting serialisation in core/. The one test that matters most is
 * not the round trip -- it is the byte-order assertion, because a round trip
 * passes happily on a machine that is writing files no other machine can
 * read. See NOTES.md "The byte order pin". */
#include <stdio.h>
#include <string.h>
#include "../trek.h"
#include "../planet.h"
#include "../serial.h"

static int failures;

static void check(int cond, const char *what) {
    if (!cond) { printf("FAIL: %s\n", what); failures++; }
}

static void check_eq(long got, long want, const char *what) {
    if (got != want) {
        printf("FAIL: %s (got %ld, want %ld)\n", what, got, want);
        failures++;
    }
}

/* Little-endian, asserted against fixed positions rather than against
 * whatever this host happens to do.
 *
 * This is the test that a 68000 build must also pass. `make port-check`
 * compiles the core for the 68000 but does not run it, so what this really
 * buys is that the byte order is a property of the CODE and not of the host:
 * the expected values below are written out by hand, so a cast-based
 * implementation fails here on x86 and arm just as it would on a 68000. */
static void test_byte_order(void) {
    uint8_t buf[TREK_SAVE_SIZE];
    uint16_t n;

    trek_new_game(3, 12345);
    ship.energy   = 0x1234;
    ship.stardate = 0xABCD;

    n = trek_state_save(buf, sizeof buf);
    check_eq(n, TREK_SAVE_SIZE, "save writes exactly TREK_SAVE_SIZE bytes");

    check_eq(buf[0], 'E', "magic byte 0");
    check_eq(buf[1], 'T', "magic byte 1");
    check_eq(buf[2], TREK_SAVE_VERSION, "version byte");

    /* header 3 + quad_y, quad_x, sec_y, sec_x = offset 7 */
    check_eq(buf[7], 0x34, "energy low byte comes FIRST");
    check_eq(buf[8], 0x12, "energy high byte comes second");

    /* ... + energy 2 + impulse 2 + shields 2 + torps 1 + laser_eff 1
       + laser_heat 2 + warp 1 + shields_up 1 = offset 19 */
    check_eq(buf[19], 0xCD, "stardate low byte comes FIRST");
    check_eq(buf[20], 0xAB, "stardate high byte comes second");
}

static void test_round_trip(void) {
    uint8_t buf[TREK_SAVE_SIZE];
    uint8_t i;
    uint16_t hp_before[QUAD_CELLS];
    uint8_t  known_before[GAL_CELLS];
    Planet   planets_before[PLANET_MAX];
    uint8_t  inv_before[ITEM_COUNT];
    uint8_t  count_before;
    Ship before;

    trek_new_game(4, 999);

    /* Move it away from a fresh game in every direction, so a field the
       serialiser forgot cannot pass by still holding its starting value --
       which is how a missed field would otherwise sail through. */
    ship.energy      = 4321;
    ship.impulse     = 321;
    ship.shields     = 1234;
    ship.torps       = 5;
    ship.laser_eff   = 77;
    ship.laser_heat  = 1500;
    ship.warp        = 65;
    ship.shields_up  = 1;
    ship.stardate    = 35123;
    ship.casualties  = 42;
    ship.killed      = 7;
    ship.killed_cmd  = 2;
    ship.repair_focus = 3;
    ship.docked      = 0;
    for (i = 0; i < SYS_COUNT; i++) ship.sys[i] = (uint8_t)(40 + i);
    base_under_attack = 17;
    bases_lost = 2;

    /* The planet chain. Same trick as the ship fields: move every one of them
       off what a fresh galaxy would hold, so a field the serialiser forgot
       cannot pass by still looking right. */
    ship.orbiting = 2;
    ship.rescues  = 3;
    planets[2].flags = PF_SCANNED | PF_TAKEN;
    planets[2].find  = PFIND_SETTLERS;
    planets[2].sec   = 41;
    inventory[ITEM_RAW_ENERGIUM] = 2;

    before = ship;
    memcpy(planets_before, planets, sizeof planets_before);
    memcpy(inv_before, inventory, sizeof inv_before);
    count_before  = planet_count;
    memcpy(hp_before, enemy_hp, sizeof hp_before);
    memcpy(known_before, gal_known, sizeof known_before);

    check(trek_state_save(buf, sizeof buf) == TREK_SAVE_SIZE, "save succeeds");

    /* Wipe the board completely. A load that quietly leaves state alone would
       otherwise look identical to a load that restored it. */
    trek_new_game(1, 1);
    memset(enemy_hp, 0, sizeof enemy_hp);
    memset(gal_known, 0, sizeof gal_known);
    base_under_attack = 0;
    bases_lost = 0;
    memset(planets, 0, sizeof planets);
    memset(inventory, 0, sizeof inventory);
    planet_count = 0;

    check(trek_state_load(buf, TREK_SAVE_SIZE), "load succeeds");

    check_eq(ship.energy,     before.energy,     "energy");
    check_eq(ship.impulse,    before.impulse,    "impulse");
    check_eq(ship.shields,    before.shields,    "shields");
    check_eq(ship.torps,      before.torps,      "torps");
    check_eq(ship.laser_eff,  before.laser_eff,  "laser_eff");
    check_eq(ship.laser_heat, before.laser_heat, "laser_heat");
    check_eq(ship.warp,       before.warp,       "warp");
    check_eq(ship.shields_up, before.shields_up, "shields_up");
    check_eq(ship.stardate,   before.stardate,   "stardate");
    check_eq(ship.stardate_end, before.stardate_end, "stardate_end");
    check_eq(ship.level,      before.level,      "level");
    check_eq(ship.enemies_left, before.enemies_left, "enemies_left");
    check_eq(ship.casualties, before.casualties, "casualties");
    check_eq(ship.killed,     before.killed,     "killed");
    check_eq(ship.killed_cmd, before.killed_cmd, "killed_cmd");
    check_eq(ship.repair_focus, before.repair_focus, "repair_focus");
    check_eq(ship.quad_y,     before.quad_y,     "quad_y");
    check_eq(ship.quad_x,     before.quad_x,     "quad_x");
    check_eq(ship.sec_y,      before.sec_y,      "sec_y");
    check_eq(ship.sec_x,      before.sec_x,      "sec_x");

    for (i = 0; i < SYS_COUNT; i++)
        check_eq(ship.sys[i], before.sys[i], "system repair percentage");

    check(memcmp(enemy_hp, hp_before, sizeof hp_before) == 0,
          "enemy hit points survive, 16 bits at a time");
    check(memcmp(gal_known, known_before, sizeof known_before) == 0,
          "the scanned-quadrant map survives");

    check_eq(base_under_attack, 17, "base_under_attack");
    check_eq(bases_lost, 2, "bases_lost");

    check_eq(ship.orbiting, before.orbiting, "the planet being orbited");
    check_eq(ship.rescues,  before.rescues,  "settlements evacuated");
    check_eq(planet_count,  count_before,    "planet count");
    check(memcmp(planets, planets_before, sizeof planets_before) == 0,
          "the whole planet list survives, tail included");
    check(memcmp(inventory, inv_before, sizeof inv_before) == 0,
          "the inventory survives");
}

/* The RNG and the event queue are as much of the game as the galaxy is.
   Without the RNG a restored game replays the same rolls from a fixed seed;
   without the schedule, every deadline the COMMUNICATIONS panel promised the
   player quietly stops existing. */
static void test_rng_and_schedule(void) {
    uint8_t buf[TREK_SAVE_SIZE];
    uint16_t r1, r2, sched_before[SCHED_COUNT];
    uint8_t i;

    trek_new_game(3, 4242);
    for (i = 0; i < 20; i++) trek_rand();     /* walk the generator on */
    for (i = 0; i < SCHED_COUNT; i++) sched_before[i] = trek_scheduled(i);

    trek_state_save(buf, sizeof buf);
    r1 = trek_rand();

    trek_new_game(1, 7);                      /* a different everything */
    trek_state_load(buf, TREK_SAVE_SIZE);
    r2 = trek_rand();

    check_eq(r2, r1, "the RNG resumes where it left off, not from a seed");
    for (i = 0; i < SCHED_COUNT; i++)
        check_eq(trek_scheduled(i), sched_before[i], "scheduled event survives");
}

/* A refused load must not have touched anything. Half a galaxy from the file
   and half from the game in progress is worse than no load at all. */
static void test_refusals(void) {
    uint8_t buf[TREK_SAVE_SIZE];
    uint16_t energy_before;

    trek_new_game(3, 55);
    trek_state_save(buf, sizeof buf);

    trek_new_game(2, 66);
    energy_before = ship.energy;

    check(!trek_state_load(buf, TREK_SAVE_SIZE - 1), "a short buffer is refused");
    check_eq(ship.energy, energy_before, "and nothing was written");

    buf[0] = 'X';
    check(!trek_state_load(buf, TREK_SAVE_SIZE), "bad magic is refused");
    check_eq(ship.energy, energy_before, "and nothing was written");

    buf[0] = 'E';
    buf[2] = TREK_SAVE_VERSION + 1;
    check(!trek_state_load(buf, TREK_SAVE_SIZE), "a future version is refused");
    check_eq(ship.energy, energy_before, "and nothing was written");

    check(trek_state_save(buf, TREK_SAVE_SIZE - 1) == 0,
          "save refuses a buffer it would overrun");
}

int main(void) {
    test_byte_order();
    test_round_trip();
    test_rng_and_schedule();
    test_refusals();

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("state serialisation: all checks passed (%d bytes per save)\n",
           TREK_SAVE_SIZE);
    return 0;
}
