#include "serial.h"
#include "trek.h"

/* A cursor rather than an index passed around. Cheapest thing on a 6502, and
   it keeps every field to one line so the save and load orders can be read
   side by side and checked against each other -- which is the only way this
   file is ever verified by eye. */
static uint16_t pos;
static uint8_t *out;
static const uint8_t *in;

static void put8(uint8_t v)  { out[pos++] = v; }

/* Low byte first, always. See the byte-order rule in serial.h. */
static void put16(uint16_t v) {
    out[pos++] = (uint8_t)(v & 0xFF);
    out[pos++] = (uint8_t)(v >> 8);
}

static uint8_t get8(void) { return in[pos++]; }

/* Shifts, never a cast to uint16_t*. A cast would read the host's own order
   and work perfectly on the machine that wrote the file. */
static uint16_t get16(void) {
    uint16_t lo = in[pos++];
    return (uint16_t)(lo | ((uint16_t)in[pos++] << 8));
}

/* The field list appears TWICE, once to save and once to load, and that is
   deliberate rather than lazy.
 *
 * The obvious version writes it once as a macro taking P8/P16 as parameters
 * and expands it two ways. That compiles clean under native cc and FAILS
 * UNDER cc65: its preprocessor does not expand a function-like macro that
 * arrived as a macro argument, so `SHIP_FIELDS(SAVE8, SAVE16)` reports
 * `Undefined symbol: SAVE8`. Discovered here on 2026-08-23, and it is the
 * same shape as every other trap in this port -- the build machine is happy
 * and the target toolchain is not.
 *
 * So: two lists, in the same order, and the round-trip test in
 * core/test/test_serial.c is what keeps them honest. Any field that appears
 * in one and not the other fails there, on the build machine, immediately. */

uint16_t trek_state_save(uint8_t *buf, uint16_t max) {
    uint8_t i;

    if (max < TREK_SAVE_SIZE) return 0;
    out = buf;
    pos = 0;

    put8('E'); put8('T'); put8(TREK_SAVE_VERSION);

    put8(ship.quad_y);   put8(ship.quad_x);
    put8(ship.sec_y);    put8(ship.sec_x);
    put16(ship.energy);  put16(ship.impulse);
    put16(ship.shields); put8(ship.torps);
    put8(ship.laser_eff); put16(ship.laser_heat);
    put8(ship.warp);     put8(ship.shields_up);
    put16(ship.stardate); put16(ship.stardate_end);
    put8(ship.level);    put16(ship.enemies_left);
    put8(ship.repair_focus);
    put16(ship.killed);  put16(ship.killed_cmd);
    put16(ship.casualties);
    put8(ship.lost);     put8(ship.docked);
    for (i = 0; i < SYS_COUNT; i++) put8(ship.sys[i]);

    for (i = 0; i < GAL_CELLS; i++) put8(gal_enemies[i]);
    for (i = 0; i < GAL_CELLS; i++) put8(gal_base[i]);
    for (i = 0; i < GAL_CELLS; i++) put8(gal_stars[i]);
    for (i = 0; i < GAL_CELLS; i++) put8(gal_known[i]);
    for (i = 0; i < QUAD_CELLS; i++) put8(sector[i]);
    for (i = 0; i < QUAD_CELLS; i++) put16(enemy_hp[i]);

    /* The event queue and the RNG are as much of the game as the galaxy is.
       Leave the RNG out and a reloaded game replays the same "random" rolls
       from a fixed seed; leave the schedule out and every deadline the
       COMMUNICATIONS panel promised silently stops existing. */
    for (i = 0; i < SCHED_COUNT; i++) put16(trek_scheduled(i));
    put16(trek_rng_state());

    put8(base_under_attack);
    put8(bases_lost);

    return pos;
}

uint8_t trek_state_load(const uint8_t *buf, uint16_t len) {
    uint8_t i;

    /* Every check before any store. A load that fails halfway would leave a
       galaxy half from the file and half from the game being played, which
       is a worse outcome than refusing. */
    if (len < TREK_SAVE_SIZE) return 0;
    if (buf[0] != 'E' || buf[1] != 'T') return 0;
    if (buf[2] != TREK_SAVE_VERSION) return 0;

    in = buf;
    pos = 3;

    ship.quad_y = get8();   ship.quad_x = get8();
    ship.sec_y = get8();    ship.sec_x = get8();
    ship.energy = get16();  ship.impulse = get16();
    ship.shields = get16(); ship.torps = get8();
    ship.laser_eff = get8(); ship.laser_heat = get16();
    ship.warp = get8();     ship.shields_up = get8();
    ship.stardate = get16(); ship.stardate_end = get16();
    ship.level = get8();    ship.enemies_left = get16();
    ship.repair_focus = get8();
    ship.killed = get16();  ship.killed_cmd = get16();
    ship.casualties = get16();
    ship.lost = get8();     ship.docked = get8();
    for (i = 0; i < SYS_COUNT; i++) ship.sys[i] = get8();

    for (i = 0; i < GAL_CELLS; i++) gal_enemies[i] = get8();
    for (i = 0; i < GAL_CELLS; i++) gal_base[i] = get8();
    for (i = 0; i < GAL_CELLS; i++) gal_stars[i] = get8();
    for (i = 0; i < GAL_CELLS; i++) gal_known[i] = get8();
    for (i = 0; i < QUAD_CELLS; i++) sector[i] = get8();
    for (i = 0; i < QUAD_CELLS; i++) enemy_hp[i] = get16();

    for (i = 0; i < SCHED_COUNT; i++) trek_sched_restore(i, get16());
    trek_rng_restore(get16());

    base_under_attack = get8();
    bases_lost = get8();

    return 1;
}
