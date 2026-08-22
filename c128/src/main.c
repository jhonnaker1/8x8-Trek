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

/* A stardate as t.d into a buffer, for messages that carry a deadline. */
static uint8_t put_tenths_str(char *p, uint16_t tenths) {
    uint8_t k = put_u16(p, (uint16_t)(tenths / 10));
    p[k++] = '.';
    p[k++] = (char)('0' + (tenths % 10));
    return k;
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


/* The laser officer asks for an amount against each enemy vessel in the
   quadrant (manual l.563-566) rather than one figure split between them,
   and 0 skips a target. Energy comes out of the main banks, confirmed on
   the original by its "insufficient energy" refusal firing when main was
   below the amount requested. */
/* SHUP and SHDN on one key. The original gives them separate commands and
   binds them to the arrow keys as well; this port has neither the arrow keys
   (the C128's dedicated cursor keys sit outside the 8x8 matrix input.c scans
   -- see the note there) nor spare letters, so S toggles. The message says
   which way it went, so the state is never ambiguous. */
static void do_shields(void) {
    if (ship.shields_up) {
        trek_shields_down();
        ui_message("ENGINEERING: ", "SHIELDS DOWN.");
        return;
    }
    switch (trek_shields_up()) {
        case SHIELD_OK:
            ui_message("ENGINEERING: ", "SHIELDS UP.");
            break;
        default:
            ui_message("ENGINEERING: ", "TOO LITTLE ENERGY.");
            break;
    }
}

/* Names for the three pools, in the 1/2/3 order trek_divert uses. */
static const char *pool_name(unsigned char p) {
    switch (p) {
        case POOL_MAIN:    return "MAIN";
        case POOL_IMPULSE: return "IMPULSE";
        default:           return "SHIELDS";
    }
}

static void report_divert(uint8_t r, uint16_t lost) {
    switch (r) {
        case DIVERT_OK:
            if (lost) {
                /* CONFIRMED on the original: transferring into a full pool
                   destroys the surplus. Saying so matters -- the player
                   otherwise sees energy vanish with no explanation. */
                uint8_t k = put_str(linebuf, "DONE. ");
                k += put_u16(linebuf + k, lost);
                k += put_str(linebuf + k, " UNITS LOST");
                linebuf[k] = 0;
                ui_message("ENGINEERING: ", linebuf);
            } else {
                ui_message("ENGINEERING: ", "TRANSFER COMPLETE.");
            }
            break;
        case DIVERT_SHORT:
            ui_message("ENGINEERING: ", "WE HAVE NOT GOT IT.");
            break;
        default:
            ui_message("ENGINEERING: ", "ILLOGICAL, CAPTAIN.");
            break;
    }
}

static void do_energy(void) {
    char line[8];
    uint16_t from, to, amount, lost = 0;

    ui_dialog_open("ENERGY TRANSFER");
    ui_dialog_line("1 MAIN  2 IMPULSE  3 SHIELDS");

    ui_dialog_ask("FROM (1-3):", line, sizeof line);
    from = grab_num(line);
    ui_dialog_ask("TO (1-3):", line, sizeof line);
    to = grab_num(line);
    ui_dialog_ask("AMOUNT:", line, sizeof line);
    amount = grab_num(line);

    {
        uint8_t k = put_str(linebuf, pool_name((unsigned char)from));
        k += put_str(linebuf + k, " TO ");
        k += put_str(linebuf + k, pool_name((unsigned char)to));
        linebuf[k] = 0;
        ui_dialog_line(linebuf);
    }
    ui_dialog_close();

    report_divert(trek_divert((uint8_t)from, (uint8_t)to, amount, &lost), lost);
}

/* MAX -- everything the main banks hold, into the shields. The original lists
   it separately from ENERGY because it is the thing you want in a hurry. */
static void do_max_energy(void) {
    uint16_t lost = 0;
    uint16_t room = (uint16_t)(SHIELD_MAX > ship.shields
                               ? SHIELD_MAX - ship.shields : 0);
    uint16_t amount = ship.energy < room ? ship.energy : room;

    if (amount == 0) {
        ui_message("ENGINEERING: ", "SHIELDS ALREADY FULL.");
        return;
    }
    report_divert(trek_divert(POOL_MAIN, POOL_SHIELDS, amount, &lost), lost);
}

static void do_dock(void) {
    switch (trek_dock()) {
        case DOCK_OK:
            /* Name what this base actually gave us -- a research station
               resupplies nothing this core models, and saying "DOCKED" alone
               would leave the player wondering why the tubes are still
               empty. */
            switch (ship.docked) {
                case BASE_STARBASE:
                    ui_message("HELM: ", "DOCKED. ALL STORES FULL.");
                    break;
                case BASE_SUPPLY:
                    ui_message("HELM: ", "DOCKED. TORPEDOES ONLY.");
                    break;
                default:
                    ui_message("HELM: ", "DOCKED. NO STORES HERE.");
                    break;
            }
            break;
        case DOCK_ALREADY:
            ui_message("HELM: ", "WE ARE ALREADY DOCKED.");
            break;
        default:
            ui_message("HELM: ", "NO BASE ALONGSIDE.");
            break;
    }
}

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

    /* Heat is per-command, so it is cleared where the command begins. The
       core cannot see a volley boundary; only this can. */
    trek_laser_begin_volley();

    ui_dialog_open("WEAPONS CONTROL");

    for (cell = 0; cell < QUAD_CELLS; cell++) {
        if (!SEC_IS_ENEMY(sector[cell])) continue;

        y = (uint8_t)(cell >> 3);
        x = (uint8_t)(cell & 7);
        what = sector[cell];   /* a kill clears the cell; name it first */

        {
            uint8_t n = put_str(linebuf, "AMOUNT TO FIRE AT ");
            n += put_u16(linebuf + n, (uint16_t)(y + 1));
            n += put_str(linebuf + n, ",");
            n += put_u16(linebuf + n, (uint16_t)(x + 1));
            n += put_str(linebuf + n, ":");
            linebuf[n] = 0;
        }
        ui_dialog_ask(linebuf, line, sizeof line);

        energy = grab_num(line);
        if (energy == 0) continue;

        switch (trek_fire_laser(y, x, energy, &dealt)) {
            case FIRE_OK:
            case FIRE_KILL: {
                uint8_t killed = (sector[cell] == SEC_EMPTY);
                uint8_t n = put_u16(linebuf, dealt);
                n += put_str(linebuf + n, " UNIT HIT ON ");
                n += put_str(linebuf + n, enemy_name(what));
                linebuf[n] = 0;
                ui_dialog_line(linebuf);
                if (killed) ui_dialog_line("MONGOL DESTROYED!");
                break;
            }
            case FIRE_NO_ENERGY:
                ui_dialog_line("INSUFFICIENT ENERGY, CAPTAIN!");
                ui_dialog_close();
                return;
            default:
                break;
        }
    }

    ui_dialog_close();
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
/* R)epair. A report, so it costs no time and draws no fire -- the same as the
   permanently visible panels, and the command card calls it a "state of Repair
   report". Blocks on Return, then repaints the console it drew over. */
static void do_repair(void) {
    ui_repair_report();
    while (kb_waitkey() != KB_RETURN) { }
    ui_draw_all();
}

static void enemy_turn(uint8_t player_fired) {
    TrekEvent ev[12];
    uint8_t n, i, k;

    /* Scheduled events first, then the enemy. A base falling or a tractor
       beam is something that happened during the time the turn consumed, so
       it belongs before this turn's shooting. Note the tractor beam can move
       us to another quadrant, which is exactly why the enemy turn is
       evaluated after it and not before. */
    n = trek_run_events(ev, 12);
    if (n < 12)
        n = (uint8_t)(n + trek_enemy_turn(ev + n, (uint8_t)(12 - n),
                                          player_fired));

    for (i = 0; i < n; i++) {
        k = 0;
        switch (ev[i].kind) {
            case EV_SHIELD_HOLD:
                k  = put_u16(linebuf, ev[i].amount);
                k += put_str(linebuf + k, " ABSORBED FROM ");
                k += put_sector(linebuf + k, ev[i].y, ev[i].x);
                break;
            case EV_HIT:
                k  = put_u16(linebuf, ev[i].amount);
                k += put_str(linebuf + k, " HIT FROM ");
                k += put_sector(linebuf + k, ev[i].y, ev[i].x);
                break;
            case EV_SYSTEM_HIT:
                k  = put_str(linebuf, sys_name(ev[i].y));
                k += put_str(linebuf + k, " AT ");
                k += put_u16(linebuf + k, ev[i].amount);
                break;
            case EV_ENEMY_MOVED:
                k  = put_str(linebuf, "MONGOL NOW AT ");
                k += put_sector(linebuf + k, ev[i].y, ev[i].x);
                linebuf[k] = 0;
                ui_message("SCANNER: ", linebuf);
                continue;
            case EV_BASE_ATTACKED:
                /* The deadline is the point of the message, as it is in the
                   original: "They can last until 3517.8." */
                k  = put_str(linebuf, "BASE ");
                k += put_sector(linebuf + k, ev[i].y, ev[i].x);
                k += put_str(linebuf + k, " HIT TIL ");
                k += put_tenths_str(linebuf + k, ev[i].amount);
                linebuf[k] = 0;
                ui_message("COMMS: ", linebuf);
                continue;
            case EV_BASE_LOST:
                k  = put_str(linebuf, "BASE ");
                k += put_sector(linebuf + k, ev[i].y, ev[i].x);
                k += put_str(linebuf + k, " DESTROYED");
                linebuf[k] = 0;
                ui_message("COMMS: ", linebuf);
                continue;
            case EV_TRACTORED:
                k  = put_str(linebuf, "DRAGGED TO QUAD ");
                k += put_sector(linebuf + k, ev[i].y, ev[i].x);
                linebuf[k] = 0;
                ui_message("HELM: ", linebuf);
                continue;
            case EV_POD_HIT:
                k  = put_str(linebuf, "DEATH POD ");
                k += put_u16(linebuf + k, ev[i].amount);
                k += put_str(linebuf + k, " ON ALL");
                linebuf[k] = 0;
                ui_message("DAMAGE: ", linebuf);
                continue;
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
    char buf[8];

    if (ship.torps == 0) {
        ui_message("WEAPONS: ", "CAPTAIN, WE HAVE NO TORPEDOS!");
        return;
    }

    ui_dialog_open("ENERGY TORPEDO CONTROL");

    /* "t35" fires straight at 3,5; a bare "t" asks, as the original does. */
    if (n != 2) {
        ui_dialog_ask("SECTOR TO FIRE AT:", buf, sizeof buf);
        n = grab_digits(buf, d, 8);
    }
    if (n != 2 || d[0] < 1 || d[0] > 8 || d[1] < 1 || d[1] > 8) {
        ui_dialog_line("THAT IS NOT A SECTOR, CAPTAIN.");
        ui_dialog_close();
        return;
    }

    ui_dialog_line("TRACKING...");
    {
        uint16_t dmg = 0;
        uint8_t  r = trek_fire_torpedo((uint8_t)(d[0] - 1), (uint8_t)(d[1] - 1),
                                       &dmg);
        switch (r) {
            case TORP_KILL:
                ui_dialog_line("MONGOL DESTROYED!");
                break;
            case TORP_OK: {
                /* New: a Commander survives one. MEASURED -- torpedo damage
                   caps at 355 and a Commander has 695, so this branch was
                   unreachable before today. */
                uint8_t k = put_str(linebuf, "MONGOL DAMAGED -- ");
                k += put_u16(linebuf + k, dmg);
                k += put_str(linebuf + k, " UNIT HIT");
                linebuf[k] = 0;
                ui_dialog_line(linebuf);
                break;
            }
            case TORP_MISS:
                /* The original's own wording, read off its screen. */
                ui_dialog_line("CLEAN MISS, SIR!");
                break;
            default:
                ui_dialog_line("TUBES CANNOT FIRE.");
                break;
        }
    }
    ui_dialog_close();
}

int main(void) {
    unsigned char c;

#ifdef TREK_DEBUG_INPUT
    /* This assignment exists to make kb_inject appear in the link map.
       cc65 lists a symbol in the map's exports only if another module
       imports it, and input.c is the only place that otherwise touches this
       one -- so without a reference here tools/vice_mon.py has no address to
       poke. Compiled out of release builds along with everything else behind
       this flag.

       And it goes AFTER the declaration, because cc65 is C89. That is the
       third time this session; native cc is C99 and will not warn. */
    kb_inject = 0;
#endif

    vdc_init();
    {
        Setup setup;
        ui_setup(&setup);
        /* The seed comes out of how long the player took to answer, so no two
           sittings get the same galaxy. Before this, GAME_SEED was a constant
           and every game was identical -- NOTES item 3, blocked on this screen
           since the start. */
        trek_new_game(setup.level, setup.seed);
    }
    ui_draw_all();
    ui_message("HELM: ", "AWAITING ORDERS CAPTAIN");

    for (;;) {
        ui_read_command(cmd, sizeof cmd);

        c = (unsigned char)cmd[0];

        /* Only L and T pass 1: enemies manoeuvre when we shoot at them, not
           merely when a turn passes. MEASURED -- see trek.h. */
        if (c == KB_M)      { do_move(cmd);    enemy_turn(0); }
        else if (c == KB_L) { do_lasers();     enemy_turn(1); }
        else if (c == KB_T) { do_torpedo(cmd); enemy_turn(1); }
        else if (c == KB_D) { do_dock();       enemy_turn(0); }
        else if (c == KB_S) { do_shields();    enemy_turn(0); }
        else if (c == KB_E) { do_energy();     enemy_turn(0); }
        else if (c == KB_X) { do_max_energy(); enemy_turn(0); }
        else if (c == KB_R) do_repair();   /* a report, not a turn */
        else if (c == KB_W) do_warp(cmd);   /* setting speed is not a turn */
        else if (c == KB_Q) break;
        else if (c)          ui_message("COMPUTER: ", "M W L T D S)HLD E X)MAX Q");

        ui_draw_scan();
        ui_draw_chart();
        ui_draw_status();
        ui_draw_systems();
        ui_draw_lasers();
        ui_draw_viewer();
        ui_draw_position();

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

        /* Bases lost are the one item worth calling out separately: they are
           the only score line that says whether the deadlines were met. */
        if (bases_lost) {
            k  = put_u16(linebuf, (uint16_t)bases_lost);
            k += put_str(linebuf + k, " BASES LOST TO SIEGE");
            linebuf[k] = 0;
            ui_message("HQ: ", linebuf);
        }
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
