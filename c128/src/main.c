#include <stdint.h>

#include "vdc.h"
#include "layout.h"
#include "ui.h"
#include "input.h"
#include "../../core/trek.h"

/* 8x8 Trek -- C128 VDC port, milestone 2.
 *
 * Galaxy generation, movement and laser fire, driven from the shared core.
 * The console frame from milestone 1 shows live state: the short range
 * scan, the chart of known galaxy, and the status readouts all read core
 * arrays.
 *
 * Enemies do not shoot back yet, and there is no damage, docking, supply
 * or torpedo handling.
 */

/* Key values come from input.h (KB_*), the same symbols the scan table is
   built from. They were briefly duplicated here as separate numeric
   constants, which is how `q` came to never quit: input.c's table was
   written with character literals, cc65 translated them to PETSCII, and the
   two spellings drifted silently. One definition, included by both. */

/* PROVISIONAL: a fixed seed, so the same galaxy comes up every run. That is
   deliberate for now -- it makes a side-by-side against the original in
   DOSBox-X repeatable, and the core test asserts a seed reproduces a galaxy
   exactly. Wire this to something varying once there is a title screen to
   take the timing from. */
#define GAME_SEED  12345
#define GAME_LEVEL 3

static char cmd[16];

/* Collects the digits out of a command line, ignoring any separators the
   player chose to use. The original is equally relaxed: "you can use
   whatever is most convenient for separators" (manual l.538). */
static uint8_t grab_digits(const char *s, uint8_t *out, uint8_t max) {
    uint8_t n = 0;
    unsigned char u;

    while (*s && n < max) {
        u = (unsigned char)*s++;
        if (u >= KB_DIGIT0 && u <= KB_DIGIT9) out[n++] = (uint8_t)(u - KB_DIGIT0);
    }
    return n;
}

/* Decimal into buf, returning the length written. cc65 ships sprintf, but
   pulling it in drags a full formatter into a program that needs at most
   five digits. Digits are built from KB_DIGIT0 rather than a '0' literal,
   for the reason input.h spells out. */
static uint8_t put_u16(char *buf, uint16_t v) {
    char rev[6];
    uint8_t n = 0, i = 0;

    if (v == 0) { buf[0] = KB_DIGIT0; return 1; }
    while (v) { rev[n++] = (char)(KB_DIGIT0 + (v % 10)); v /= 10; }
    while (n) buf[i++] = rev[--n];
    return i;
}

/* String literals ARE translated to PETSCII by cc65, so this copies letters
   in the 193-218 range while put_u16 writes ASCII digits. Mixing the two in
   one buffer is fine: everything here goes to ui_message, and the screen
   code converter in vdc.c accepts both spellings. The translation only
   bites when a literal is COMPARED against a key value, never when it is
   displayed. */
static uint8_t put_str(char *buf, const char *s) {
    uint8_t n = 0;
    while (*s) buf[n++] = *s++;
    return n;
}

/* Whole number out of a command line, ignoring separators the same way
   grab_digits does. Saturates rather than wrapping: a captain who types
   nine digits gets everything the banks hold, not a 16-bit wrap to nearly
   nothing. */
static uint16_t grab_num(const char *s) {
    uint16_t v = 0;
    unsigned char u;
    uint8_t d;

    while (*s) {
        u = (unsigned char)*s++;
        if (u < KB_DIGIT0 || u > KB_DIGIT9) continue;
        d = (uint8_t)(u - KB_DIGIT0);
        /* Both halves matter: 6554 overflows on the multiply, and 6553
           overflows on the add for any digit above 5. Guarding only the
           first let "65536" wrap to 0, so a captain asking for everything
           fired nothing. */
        if (v > 6553 || (v == 6553 && d > 5)) return 65535U;
        v = (uint16_t)(v * 10 + d);
    }
    return v;
}

static const char *enemy_name(unsigned char c) {
    switch (c) {
        case SEC_COMMAND: return "COMMANDER";
        case SEC_SCOUT:   return "SCOUT";
        case SEC_SUPPLY:  return "SUPPLY SHIP";
        default:          return "MONGOL";
    }
}

static char linebuf[32];

/* MEASURED wording, from the original's own strings: it prints " unit hit
   on " followed by the ship, and reports a kill on its own line. */
static void report_hit(uint16_t dmg, const char *who, uint8_t killed) {
    uint8_t n = put_u16(linebuf, dmg);

    n += put_str(linebuf + n, " UNIT HIT ON ");
    n += put_str(linebuf + n, who);
    linebuf[n] = 0;
    ui_message("WEAPONS: ", linebuf);

    if (killed) ui_message("WEAPONS: ", "MONGOL DESTROYED!");
}

/* The laser officer asks for an amount against each enemy vessel in the
   quadrant (manual l.563-566) rather than one figure split between them,
   and 0 skips a target. Energy comes out of the main banks, confirmed on
   the original by its "insufficient energy" refusal firing when main was
   below the amount requested. */
static void do_lasers(void) {
    uint8_t cell, y, x, found = 0;
    unsigned char what;
    uint16_t energy, dealt;
    char line[8];

    for (cell = 0; cell < QUAD_CELLS; cell++)
        if (SEC_IS_ENEMY(sector[cell])) found++;

    if (!found) {
        ui_message("SCIENCE: ", "NO ENEMY SHIPS HERE");
        return;
    }

    for (cell = 0; cell < QUAD_CELLS; cell++) {
        if (!SEC_IS_ENEMY(sector[cell])) continue;

        y = (uint8_t)(cell >> 3);
        x = (uint8_t)(cell & 7);

        /* Name the target before firing: a kill clears the cell, so reading
           the class afterwards would report an empty sector. */
        what = sector[cell];

        {
            uint8_t n = put_str(linebuf, "FIRE AT ");
            n += put_u16(linebuf + n, (uint16_t)(y + 1));
            n += put_str(linebuf + n, ",");
            n += put_u16(linebuf + n, (uint16_t)(x + 1));
            n += put_str(linebuf + n, " AMOUNT?");
            linebuf[n] = 0;
            ui_message("WEAPONS: ", linebuf);
        }

        ui_read_command(line, sizeof line);
        energy = grab_num(line);
        if (energy == 0) continue;

        switch (trek_fire_laser(y, x, energy, &dealt)) {
            case FIRE_OK:
                report_hit(dealt, enemy_name(what), 0);
                break;
            case FIRE_KILL:
                report_hit(dealt, enemy_name(what), 1);
                break;
            case FIRE_NO_ENERGY:
                /* The original refuses the shot outright and charges
                   nothing for it, so there is no partial volley to carry
                   on with. */
                ui_message("ENGINEERING: ", "INSUFFICIENT ENERGY!");
                return;
            default:
                break;
        }
    }
}

static void report_move(uint8_t r) {
    switch (r) {
        case MOVE_OK:
            break;
        case MOVE_BLOCKED:
            ui_message("NAVIGATION: ", "BLOCKED BY OBJECT");
            break;
        case MOVE_NO_ENERGY:
            ui_message("ENGINEERING: ", "TOO LITTLE ENERGY");
            break;
        case MOVE_SAME_PLACE:
            ui_message("NAVIGATION: ", "THAT IS OUR LOCATION");
            break;
        default:
            ui_message("NAVIGATION: ", "NO SUCH LOCATION");
            break;
    }
}

/* Coordinates arrive 1-based, as the original presents them, and are
   converted to the core's 0-based indexing here -- the boundary between
   presentation and core is exactly the right place for it. */
static void do_move(const char *line) {
    uint8_t d[8];
    uint8_t n = grab_digits(line, d, 8);
    uint8_t i;

    for (i = 0; i < n; i++) {
        if (d[i] < 1 || d[i] > 8) {
            ui_message("NAVIGATION: ", "NO SUCH LOCATION");
            return;
        }
    }

    if (n == 2) {
        report_move(trek_move_impulse((uint8_t)(d[0] - 1), (uint8_t)(d[1] - 1)));
    } else if (n == 4) {
        report_move(trek_move_warp((uint8_t)(d[0] - 1), (uint8_t)(d[1] - 1),
                                   (uint8_t)(d[2] - 1), (uint8_t)(d[3] - 1)));
    } else {
        ui_message("NAVIGATION: ", "GIVE SECTOR OR QUAD+SEC");
    }
}

static void do_warp(const char *line) {
    uint8_t d[4];
    uint8_t n = grab_digits(line, d, 4);
    uint8_t tenths;

    if (n == 0) {
        ui_message("ENGINEERING: ", "SPEED CAPTAIN?");
        return;
    }
    /* "w5" is warp 5.0, "w5.2" is warp 5.2 (manual l.632). */
    tenths = (uint8_t)(n == 1 ? d[0] * 10 : d[0] * 10 + d[1]);

    if (trek_set_warp(tenths)) ui_message("ENGINEERING: ", "WARP SPEED SET");
    else ui_message("ENGINEERING: ", "INVALID WARP FACTOR");
}

static const char *sys_name(uint8_t i) {
    switch (i) {
        case SYS_CONVERTER:   return "CONVERTER";
        case SYS_SHIELDS:     return "SHIELDS";
        case SYS_LIFE:        return "LIFE SUPPORT";
        case SYS_LASERS:      return "LASERS";
        case SYS_TUBES:       return "ENTORP TUBES";
        case SYS_WARP:        return "WARP ENGINES";
        case SYS_IMPULSE:     return "IMPULSE ENGINE";
        case SYS_SRSCAN:      return "S.R. SCANNER";
        case SYS_LRSCAN:      return "L.R. SCANNER";
        case SYS_COMPUTER:    return "COMPUTER";
        case SYS_TRANSPORTER: return "TRANSPORTER";
        default:              return "SHUTTLECRAFT";
    }
}

/* Append " y,x" in the original's 1-based presentation. */
static uint8_t put_sector(char *buf, uint8_t y, uint8_t x) {
    uint8_t n = put_u16(buf, (uint16_t)(y + 1));
    n += put_str(buf + n, ",");
    n += put_u16(buf + n, (uint16_t)(x + 1));
    return n;
}

/* Every command that costs a turn ends here: the enemy shoots back and the
   console reports it. Without this there is no game, only a shooting
   gallery. */
static void enemy_turn(void) {
    TrekEvent ev[12];
    uint8_t n, i, k;

    n = trek_enemy_turn(ev, 12);

    for (i = 0; i < n; i++) {
        k = 0;
        switch (ev[i].kind) {
            case EV_SHIELD_HOLD:
                k  = put_str(linebuf, "SHIELDS ABSORB ");
                k += put_u16(linebuf + k, ev[i].amount);
                k += put_str(linebuf + k, " FROM ");
                k += put_sector(linebuf + k, ev[i].y, ev[i].x);
                break;
            case EV_HIT:
                k  = put_u16(linebuf, ev[i].amount);
                k += put_str(linebuf + k, " UNIT HIT FROM ");
                k += put_sector(linebuf + k, ev[i].y, ev[i].x);
                break;
            case EV_SYSTEM_HIT:
                k  = put_str(linebuf, sys_name(ev[i].y));
                k += put_str(linebuf + k, " NOW AT ");
                k += put_u16(linebuf + k, ev[i].amount);
                break;
            case EV_SHIP_LOST:
                ui_message("HELM: ", "WE ARE LOST, CAPTAIN.");
                continue;
            default:
                continue;
        }
        linebuf[k] = 0;
        ui_message("DAMAGE: ", linebuf);
    }
}

/* One torpedo destroys a standard Mongol outright, confirmed against the
   original -- nothing has ever survived one to report a damage figure. */
static void do_torpedo(const char *line) {
    uint8_t d[8];
    uint8_t n = grab_digits(line, d, 8);

    if (ship.torps == 0) {
        ui_message("WEAPONS: ", "CAPTAIN, WE HAVE NO TORPEDOS!");
        return;
    }
    if (n != 2 || d[0] < 1 || d[0] > 8 || d[1] < 1 || d[1] > 8) {
        ui_message("WEAPONS: ", "GIVE A SECTOR, E.G. T35");
        return;
    }

    switch (trek_fire_torpedo((uint8_t)(d[0] - 1), (uint8_t)(d[1] - 1))) {
        case TORP_KILL: ui_message("WEAPONS: ", "MONGOL DESTROYED!"); break;
        case TORP_MISS: ui_message("WEAPONS: ", "TORPEDO MISSED.");   break;
        default:        ui_message("WEAPONS: ", "TUBES CANNOT FIRE."); break;
    }
}

int main(void) {
    unsigned char c;

    vdc_init();
    trek_new_game(GAME_LEVEL, GAME_SEED);
    ui_draw_all();
    ui_message("HELM: ", "AWAITING ORDERS CAPTAIN");

    for (;;) {
        ui_read_command(cmd, sizeof cmd);

        c = (unsigned char)cmd[0];

        if (c == KB_M)      { do_move(cmd);    enemy_turn(); }
        else if (c == KB_L) { do_lasers();     enemy_turn(); }
        else if (c == KB_T) { do_torpedo(cmd); enemy_turn(); }
        else if (c == KB_W) do_warp(cmd);   /* setting speed is not a turn */
        else if (c == KB_Q) break;
        else if (c)          ui_message("COMPUTER: ", "M)OVE W)ARP L)ASERS T)ORP Q)UIT");

        ui_draw_scan();
        ui_draw_chart();
        ui_draw_status();

        if (trek_game_state() != GAME_ON) break;
    }

    if (trek_game_state() == GAME_WON)       ui_message("HQ: ", "SECTOR SECURED. WELL DONE.");
    else if (trek_game_state() == GAME_LOST) ui_message("HQ: ", "THE LEXINGTON IS LOST.");
    {
        int16_t sc = trek_score();
        uint8_t k = put_str(linebuf, "SCORE ");
        /* 45 is '-', written numerically for the reason input.h gives: a
           character literal here would be translated to PETSCII. */
        if (sc < 0) { linebuf[k++] = 45; sc = (int16_t)(-sc); }
        k += put_u16(linebuf + k, (uint16_t)sc);
        linebuf[k] = 0;
        ui_message("HQ: ", linebuf);
    }

    /* Quit has to visibly end the game. Milestone 1 parked here in an
       endless wait_vsync() so the console would stay on screen, which made
       Q indistinguishable from a hang once there was a command loop to quit
       out of. Acknowledge, wait for the captain, then hand the machine back
       in the state we found it. */
    ui_clear_messages();
    ui_message("HELM: ", "MISSION ENDED, CAPTAIN.");
    ui_message("COMPUTER: ", "RUN/STOP+RESTORE FOR");
    ui_message("COMPUTER: ", "BASIC.");

    /* Drop back to 1MHz first, so the VIC-IIe screen is live again and the
       machine does not look dead on the other window. */
    vdc_shutdown();

    /* KNOWN ISSUE -- returning to BASIC wedges the C128.
     *
     * `return 0` here hands control to cc65's exit path, and the machine
     * stops responding on both screens. Not diagnosed: this program runs at
     * 2MHz, drives the VDC directly, and scans CIA1 behind the KERNAL's
     * back, so there are several candidates and no way to observe the
     * machine from a session on this host (see NOTES.md on why VICE cannot
     * be screenshotted or single-stepped from here).
     *
     * Parking is the honest behaviour rather than a pretend one: the final
     * console stays readable, and because interrupts are left enabled the
     * KERNAL's NMI still works, so RUN/STOP+RESTORE gets the player a BASIC
     * prompt. Plenty of 8-bit games never returned to BASIC at all.
     *
     * To diagnose properly, bisect it: return immediately from main() at the
     * top with no vdc_init() at all and confirm a clean exit, then add back
     * the 2MHz switch, then the VDC register writes, then the CIA scanning.
     * The first one that wedges it is the culprit. */
    for (;;) { }

    return 0;
}
