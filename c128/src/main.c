#include <stdint.h>
#include <string.h>

#include "vdc.h"
#include "egavdc.h"
#include "layout.h"
#include "ui.h"
#include "../../core/strpool.h"
#include "strdata.h"
#include "input.h"
#include "sid.h"
#include "../../core/farmem.h"
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
static Setup setup;        /* the name and password the setup screen collected */

/* Whole-word command match, case-insensitive on the letters we care about.
   The command line is already upper case coming out of the keyboard scan, so
   this is a plain compare with the length checked -- "S" must not match
   "SHUP", which is exactly the bug that would put the ship's destruction one
   keystroke from raising its shields. */
static uint8_t word_is(const char *line, const char *word) {
    uint8_t i;
    for (i = 0; word[i]; i++)
        /* & 0x7F because `word` is a string literal and cl65 translates those
           to PETSCII, where a letter sits 0x80 above its ASCII code, while the
           keyboard scanner returns plain ASCII. Masking the high bit maps
           PETSCII letters back and leaves ASCII untouched, so the comparison
           is right under either compiler. This is the same mismatch that once
           put the key table in PETSCII and made "q" a no-op; the first version
           of this function had it too, and INFO silently fell through to the
           unknown-command help. */
        if (line[i] != (char)(word[i] & 0x7F)) return 0;
    return (uint8_t)(line[i] == 0);
}

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
/* SHUP and SHDN, as the reference card names them. This was one S key that
   toggled, which is neither what the card says nor compatible with S)elf --
   the card gives S to self destruct and spells shields out. */
static void do_shields_down(void) {
    trek_shields_down();
    ui_message(S(S_31), S(S_71));
}

static void do_shields_up(void) {
    switch (trek_shields_up()) {
        case SHIELD_OK:
            ui_message(S(S_31), S(S_72));
            break;
        case SHIELD_ALREADY:
            ui_message(S(S_31), S(S_7));
            break;
        default:
            ui_message(S(S_31), S(S_84));
            break;
    }
}

/* The number in A#, or 0 for a bare A -- which the original reads as "all",
   so 0 is a real answer here and not a failure value. Anything that is not a
   digit is treated as absent for the same reason: the original's out-of-range
   A5 was a silent no-op, so there is nothing to report. */
static uint8_t ack_arg(const char *cmd) {
    if (cmd[1] >= KB_DIGIT0 && cmd[1] <= KB_DIGIT9)
        return (uint8_t)(cmd[1] - KB_DIGIT0);
    return 0;
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
                ui_message(S(S_31), linebuf);
            } else {
                ui_message(S(S_31), S(S_87));
            }
            break;
        case DIVERT_SHORT:
            ui_message(S(S_31), S(S_94));
            break;
        default:
            ui_message(S(S_31), S(S_41));
            break;
    }
}

static void do_energy(void) {
    char line[8];
    uint16_t from, to, amount, lost = 0;

    ui_dialog_open(S(S_29));
    ui_dialog_line(S(S_2));

    ui_dialog_ask(S(S_37), line, sizeof line);
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
        ui_message(S(S_31), S(S_70));
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
                    ui_message("HELM: ", S(S_23));
                    break;
                case BASE_SUPPLY:
                    ui_message("HELM: ", S(S_25));
                    break;
                default:
                    ui_message("HELM: ", S(S_24));
                    break;
            }
            break;
        case DOCK_ALREADY:
            ui_message("HELM: ", S(S_92));
            break;
        default:
            ui_message("HELM: ", S(S_53));
            break;
    }
}

/* SFX_A and SFX_B are MEASURED, not matched by ear: the routine that starts
   0x099A in the original is the one that prints "Amount to fire at " and
   "Lasers overheat", and the routine that starts 0x09A0 prints "ENERGY TORPEDO
   CONTROL" and "Number to fire". Each effect belongs to the command whose code
   launches it. */
static void do_lasers(void) {
    uint8_t cell, y, x, found = 0;
    unsigned char what;
    uint16_t energy, dealt;
    char line[8];

    for (cell = 0; cell < QUAD_CELLS; cell++)
        if (SEC_IS_ENEMY(sector[cell])) found++;

    if (!found) {
        snd_beep();
        ui_message("SCIENCE: ", S(S_55));
        return;
    }

    /* Heat is per-command, so it is cleared where the command begins. The
       core cannot see a volley boundary; only this can. */
    trek_laser_begin_volley();

    ui_dialog_open(S(S_95));

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

        snd_effect(SFX_A);
        switch (trek_fire_laser(y, x, energy, &dealt)) {
            case FIRE_OK:
            case FIRE_KILL: {
                uint8_t killed = (sector[cell] == SEC_EMPTY);
                uint8_t n = put_u16(linebuf, dealt);
                n += put_str(linebuf + n, " UNIT HIT ON ");
                n += put_str(linebuf + n, enemy_name(what));
                linebuf[n] = 0;
                ui_dialog_line(linebuf);
                if (killed) ui_dialog_line(S(S_50));
                break;
            }
            case FIRE_NO_ENERGY:
                snd_beep();
                ui_dialog_line(S(S_44));
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
            ui_message(S(S_52), S(S_11));
            break;
        case MOVE_NO_ENERGY:
            ui_message(S(S_31), S(S_83));
            break;
        case MOVE_SAME_PLACE:
            /* MEASURED: "Captain, that is our current location!" */
            ui_message(S(S_52), S(S_78));
            break;
        case MOVE_NO_IMPULSE:
            /* MEASURED wording, from the string table: the original says
               "Move aborted; impulse engines are too damaged to use" -- a
               hard refusal, not a slower move. */
            ui_message(S(S_31), S(S_42));
            break;
        default:
            ui_message(S(S_52), S(S_57));
            break;
    }
}

/* Coordinates arrive 1-based, as the original presents them, and are
   converted to the core's 0-based indexing here -- the boundary between
   presentation and core is exactly the right place for it. */
/* Manual movement: DeltaX then DeltaY, relative and signed.
 *
 * MEASURED from the manual (l.515-529) and confirmed on a running original.
 * The number format is the part that surprises: **the digit before the
 * decimal point is QUADRANTS and the digit after it is SECTORS**, each 0..7.
 * Moving from 1,8,1,8 to 2,6,1,6 is DeltaX 1.0 and DeltaY -2.2.
 *
 * DeltaX is VERTICAL and DeltaY is horizontal, which is the opposite of the
 * usual convention and matches the original's vertical-first coordinates.
 *
 * This is not a convenience. The manual: "Automatic navigation requires the
 * computer to be 100% repaired." Below that it is the ONLY way to move, so a
 * port without it strands a player it has just damaged -- which is why
 * trek_autonav_ok() was written on 2026-08-24 and deliberately left unwired
 * until this existed.
 *
 * Returns 1 if the field parsed, 0 on ESC or nonsense. The parsed value is a
 * signed count of SECTORS, since a quadrant is eight of them. */
static uint8_t read_delta(const char *prompt, int16_t *out) {
    char buf[10];
    const char *p;
    uint8_t neg = 0, q = 0, sec = 0;

    if (!ui_dialog_ask_esc(prompt, buf, sizeof buf)) return 0;

    p = buf;
    while (*p == ' ') p++;
    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') p++;

    if (*p < KB_DIGIT0 || *p > KB_DIGIT9) return 0;
    q = (uint8_t)(*p++ - KB_DIGIT0);

    if (*p == '.' || *p == ',') {
        p++;
        if (*p < KB_DIGIT0 || *p > KB_DIGIT9) return 0;
        sec = (uint8_t)(*p - KB_DIGIT0);
    }
    if (q > 7 || sec > 7) return 0;

    *out = (int16_t)(q * QUAD_DIM + sec);
    if (neg) *out = (int16_t)(-*out);
    return 1;
}

/* Absolute coordinates, 1-based as the player types them. Two numbers is an
   impulse hop inside the quadrant, four is a warp jump; anything else is not
   a location. Shared by the command line and the NAVIGATION dialog so the
   two cannot drift. */
static void move_absolute(const uint8_t *d, uint8_t n) {
    uint8_t i;

    for (i = 0; i < n; i++)
        if (d[i] < 1 || d[i] > 8) { ui_message(S(S_52), S(S_57)); return; }

    if (n == 2)
        report_move(trek_move_impulse((uint8_t)(d[0] - 1), (uint8_t)(d[1] - 1)));
    else if (n == 4)
        report_move(trek_move_warp((uint8_t)(d[0] - 1), (uint8_t)(d[1] - 1),
                                   (uint8_t)(d[2] - 1), (uint8_t)(d[3] - 1)));
    else
        ui_message(S(S_52), S(S_57));
}

static void do_move_manual(void) {
    int16_t dy, dx;

    if (!read_delta("DELTAX:", &dy)) { ui_dialog_close(); return; }
    if (!read_delta("DELTAY:", &dx)) { ui_dialog_close(); return; }
    ui_dialog_dismiss();
    report_move(trek_move_delta(dy, dx));
}

/* The NAVIGATION dialog, which the original opens when M arrives with no
   coordinates on it. Automatic entry takes "6,2,3,5" or a bare "3,5" for an
   impulse hop; typing just M switches to manual entry, as the manual says. */
static void do_move_prompt(void) {
    char buf[16];
    uint8_t d[8], n;

    ui_dialog_open(S(S_51));

    /* A damaged computer gives no choice -- straight to manual. */
    if (!trek_autonav_ok()) {
        ui_dialog_line(S(S_17));
        do_move_manual();
        return;
    }

    if (!ui_dialog_ask_esc(S(S_64), buf, sizeof buf)) {
        ui_dialog_close();
        return;
    }

    /* MEASURED: "enter just an M when asked for the coordinates and the
       computer will switch movement entry to manual". */
    if (buf[0] == KB_M || buf[0] == KB_M + 32) { do_move_manual(); return; }

    n = grab_digits(buf, d, 8);
    ui_dialog_dismiss();
    move_absolute(d, n);
}

static void do_move(const char *line) {
    uint8_t d[8];
    uint8_t n = grab_digits(line, d, 8);

    /* "M" on its own opens the dialog; "m6235" and "m35" carry their
       coordinates on the command line, which the manual documents as the
       abbreviated form.

       Coordinates ARE automatic navigation, though, so a damaged computer
       ignores them and goes to manual entry -- otherwise the shorthand would
       be a way round a restriction the long form obeys. */
    if (n == 0 || !trek_autonav_ok()) { do_move_prompt(); return; }

    move_absolute(d, n);
}

static void do_warp(const char *line) {
    uint8_t d[4];
    uint8_t n = grab_digits(line, d, 4);
    uint8_t tenths;

    if (n == 0) {
        ui_message(S(S_31), S(S_74));
        return;
    }
    /* "w5" is warp 5.0, "w5.2" is warp 5.2 (manual l.632). */
    tenths = (uint8_t)(n == 1 ? d[0] * 10 : d[0] * 10 + d[1]);

    if (trek_set_warp(tenths)) ui_message(S(S_31), S(S_91));
    else ui_message(S(S_31), S(S_45));
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

/* S)elf destruct. Gated on the password the setup screen collected, which is
   the whole reason that prompt exists. The core is never told the password --
   it formats no text and compares no strings -- so the check is here.
 *
 * RECONCILED 2026-08-22. This used to speak the ANCESTOR: "PASSWORD-REJECTED;
 * CONTINUITY-EFFECTED", "PASSWORD-ACCEPTED", "ENTROPY MAXIMIZED. n TAKEN WITH
 * US." The comment justifying it said the ancestor's wording was "too good to
 * replace", which is the wrong test entirely -- this is a port of EGA Trek, and
 * EGA Trek has its own sequence, extracted from its binary:
 *
 *     "Enter self-destruct password: "   "Hit ESC to abort"
 *     ">>SELF-DESTRUCT SEQUENCE COMMENCING<<"
 *     ">>DESTRUCT ABORTED<<"
 *     ">>WRONG PASSWORD, DESTRUCT ABORTED<<"
 *
 * The abort path came with it. A destruct prompt with no way out is not the
 * same command, and ESC had to be added to the key matrix to offer it.
 *
 * STILL DERIVED: the blast itself is the ancestor's kaboom() -- see
 * SELFDESTRUCT_FACTOR in trek.h. That is the last piece of an implemented
 * command still taken from the wrong game. */
static void do_self(void) {
    char buf[9];
    uint8_t n;

    ui_dialog_open(S(S_69));
    ui_dialog_line(S(S_39));

    if (!ui_dialog_ask_esc(S(S_32), buf, sizeof buf)) {
        ui_dialog_line(S(S_4));
        ui_dialog_close();
        return;
    }

    if (strcmp(buf, setup.password) != 0) {
        ui_dialog_line(S(S_6));
        ui_dialog_close();
        return;
    }

    ui_dialog_line(S(S_5));
    n = trek_self_destruct();
    /* "Mongol ship(s) destroyed" is EGA Trek's own phrasing, from the death
       pod's damage report. */
    { uint8_t k = put_u16(linebuf, n);
      k += put_str(linebuf + k, " MONGOL SHIP(S) DESTROYED.");
      linebuf[k] = 0;
      ui_dialog_line(linebuf); }
    ui_dialog_close();
}

/* F)ix. MEASURED 2026-08-23 off the original's own dialog:

       ENGINEERING
       System to concentrate repairs on:
       0 to abort, L for list: _

   NOT a repair-priority list, which is what the command table assumed for
   weeks -- one system, by number, with L listing the numbering. The effect is
   REPAIR_FOCUS_FACTOR in trek.h and is DERIVED, not measured; see the note
   there for the experiment that would settle it. */
static void do_fix(void) {
    char buf[8];
    /* Its own buffer, NOT the shared linebuf, which is 32 bytes -- the
       two-column list is 40 characters wide and overran it, and the overflow
       landed in the message panel's department field. It showed up on screen
       as "LECRAFTAWAITING ORDERS CAPTAIN": the tail of SHUTTLECRAFT. */
    char row[48];
    uint8_t i;

    ui_dialog_open(S(S_30));
    for (;;) {
        ui_dialog_line(S(S_76));
        ui_dialog_ask(S(S_1), buf, sizeof buf);

        if (buf[0] == KB_L || buf[0] == 'l') {
            /* TWO COLUMNS, on a fresh page. Twelve systems one per line
               overflows the dialog's eleven rows, and the scroll is a redraw
               -- so the player asked for the list and got its last five. */
            ui_dialog_open(S(S_30));
            for (i = 0; i < SYS_COUNT / 2; i++) {
                uint8_t k = put_u16(row, (uint16_t)(i + 1));
                k += put_str(row + k, ") ");
                k += put_str(row + k, ui_sys_name(i));
                while (k < 24) row[k++] = ' ';
                k += put_u16(row + k, (uint16_t)(i + 1 + SYS_COUNT / 2));
                k += put_str(row + k, ") ");
                k += put_str(row + k, ui_sys_name((uint8_t)(i + SYS_COUNT / 2)));
                row[k] = 0;
                ui_dialog_line(row);
            }
            continue;
        }

        i = (uint8_t)grab_num(buf);
        if (i == 0) { ui_dialog_close(); return; }
        if (i > SYS_COUNT) { snd_beep(); continue; }

        ship.repair_focus = i;
        { uint8_t k = put_str(row, "CONCENTRATING ON ");
          k += put_str(row + k, ui_sys_name((uint8_t)(i - 1)));
          row[k] = 0;
          ui_dialog_line(row); }
        ui_dialog_close();
        return;
    }
}

/* C)hart. MEASURED 2026-08-23: on a console already showing the chart this is
   a NO-OP in the original -- nothing on screen changed. The manual says the
   chart is "displayed at all times unless overridden", so redrawing the panel
   is exactly what the command is for, and it costs no turn. */
static void do_chart(void) {
    ui_draw_chart();
}

/* HAIL. MEASURED 2026-08-23: it costs a turn -- the stardate moved 3500.1 to
   3500.2 -- and opens a COMMUNICATIONS box which was EMPTY, there being no
   StarBase in range. What it says when a base IS in range has not been
   captured, so nothing is invented here: the turn is spent and the department
   answers with nothing, which is what was seen. */
static void do_hail(void) {
    ui_message(S(S_16), "");
}

/* SND, from the reference card. The original keeps a sound on/off byte that
   every one of its sound sites tests first -- [0x1cc8] in its data segment,
   found while extracting the music -- so this is its mechanic, not an
   invention. */
static void do_sound(void) {
    snd_toggle();
    ui_message(S(S_18), snd_enabled() ? "SOUND ON" : "SOUND OFF");
}

static void do_info(void) {
    ui_info_panel();
    ui_draw_all();
}

/* The alert.
 *
 * MEASURED: the original starts 0x09AE from the routine whose strings are
 * "Status", "Green", "Yellow", "Alert" and ">>ALERT<<" -- so it is the sound
 * of the status panel going to Alert, which is what happens on arriving in a
 * quadrant that has Mongols in it. Jamie described it exactly that way before
 * the strings confirmed it.
 *
 * Only on ARRIVING: a quadrant does not re-alert every turn you sit in it. */
static uint8_t alert_quad = 0xFF;

static void alert_check(void) {
    uint8_t q = (uint8_t)((ship.quad_y << 3) | ship.quad_x);
    if (q == alert_quad) return;
    alert_quad = q;
    if (gal_enemies[q]) snd_effect(SFX_C);
}

/* Which noise a turn makes. The effects voice is monophonic, so a turn with
   three hits in it plays one sound, not three restarts of the same one -- and
   the death pod outranks ordinary fire because in the original it has its own
   effect and its own line in the damage report. */
static uint8_t turn_sfx;

static void enemy_turn(uint8_t player_fired) {
    TrekEvent ev[12];
    uint8_t n, i, k;

    turn_sfx = 0xFF;

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
                if (turn_sfx == 0xFF) turn_sfx = SFX_E;
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
                turn_sfx = SFX_D;      /* MEASURED: the pod has its own sound */
                k  = put_str(linebuf, "DEATH POD ");
                k += put_u16(linebuf + k, ev[i].amount);
                k += put_str(linebuf + k, " ON ALL");
                linebuf[k] = 0;
                ui_message("DAMAGE: ", linebuf);
                continue;
            case EV_SHIP_LOST:
                ui_message("HELM: ", S(S_93));
                continue;
            default:
                continue;
        }
        linebuf[k] = 0;
        ui_message("DAMAGE: ", linebuf);
    }

    if (turn_sfx != 0xFF) snd_effect(turn_sfx);
}

/* One torpedo destroys a standard Mongol outright, confirmed against the
   original -- nothing has ever survived one to report a damage figure. */
/* One torpedo, fired and reported. Split out because the original fires a
   SALVO -- it asks how many first, then a sector per tube -- and the reporting
   is identical for each. */
static void fire_one_torpedo(uint8_t sy, uint8_t sx) {
    uint16_t dmg = 0;
    uint8_t  r;

    ui_dialog_line(S(S_86));
    snd_effect(SFX_B);
    r = trek_fire_torpedo(sy, sx, &dmg);
    switch (r) {
        case TORP_KILL:
            ui_dialog_line(S(S_50));
            break;
        /* MEASURED 2026-08-23: aiming at a star spends the torpedo and does
           nothing -- the star survives, and the original says so rather than
           calling it a miss. A star in the FLIGHT PATH is the other case, a
           supernova that takes the quadrant, and this port does not ray-march
           yet, so that one is still unmodelled. */
        case TORP_ABSORBED:
            ui_dialog_line(S(S_85));
            break;
        case TORP_OK: {
            uint8_t k = put_str(linebuf, "MONGOL DAMAGED -- ");
            k += put_u16(linebuf + k, dmg);
            k += put_str(linebuf + k, " UNIT HIT");
            linebuf[k] = 0;
            ui_dialog_line(linebuf);
            break;
        }
        case TORP_MISS:
            /* The original's own wording, read off its screen. */
            ui_dialog_line(S(S_15));
            break;
        default:
            ui_dialog_line(S(S_88));
            break;
    }
}

/* T)orps. MEASURED 2026-08-23: the original asks HOW MANY first -- "Number to
   fire:" -- then a sector per torpedo, "Sector to fire #1 at:". It has three
   tubes and refuses more. This port asked only for a single sector.

   "t35" still fires one straight at 3,5. That shortcut is ours, not the
   original's, and costs nothing to keep. */
static void do_torpedo(const char *line) {
    uint8_t d[8];
    uint8_t n = grab_digits(line, d, 8);
    uint8_t salvo = 1, shot;
    char buf[8];

    if (ship.torps == 0) {
        snd_beep();
        ui_message("WEAPONS: ", S(S_14));
        return;
    }

    ui_dialog_open(S(S_28));

    if (n == 2) {                      /* the "t35" shortcut: one, right now */
        if (d[0] < 1 || d[0] > 8 || d[1] < 1 || d[1] > 8) {
            snd_beep();
            ui_dialog_line(S(S_77));
            ui_dialog_close();
            return;
        }
        fire_one_torpedo((uint8_t)(d[0] - 1), (uint8_t)(d[1] - 1));
        ui_dialog_close();
        return;
    }

    ui_dialog_ask(S(S_58), buf, sizeof buf);
    salvo = (uint8_t)grab_num(buf);
    if (salvo == 0) { ui_dialog_close(); return; }
    {
        /* MEASURED from the manual: 100% gives three tubes, 67-99% two,
           34-66% one. The original says "Captain, all tubes are damaged!"
           when none work and "Captain, only N tubes..." otherwise. */
        uint8_t tubes = trek_tubes_available();

        if (tubes == 0) {
            snd_beep();
            ui_dialog_line(S(S_12));
            ui_dialog_close();
            return;
        }
        if (salvo > tubes) {
            uint8_t k = put_str(linebuf, "CAPTAIN, ONLY ");
            k += put_u16(linebuf + k, (uint16_t)tubes);
            k += put_str(linebuf + k, tubes == 1 ? " TUBE WORKS." : " TUBES WORK.");
            linebuf[k] = 0;
            snd_beep();
            ui_dialog_line(linebuf);
            salvo = tubes;
        }
    }
    if (salvo > ship.torps) {
        uint8_t k = put_str(linebuf, "CAPTAIN, THERE ARE ONLY ");
        k += put_u16(linebuf + k, (uint16_t)ship.torps);
        k += put_str(linebuf + k, " LEFT.");
        linebuf[k] = 0;
        snd_beep();
        ui_dialog_line(linebuf);
        salvo = ship.torps;
    }

    for (shot = 1; shot <= salvo; shot++) {
        uint8_t k = put_str(linebuf, "SECTOR TO FIRE #");
        k += put_u16(linebuf + k, (uint16_t)shot);
        k += put_str(linebuf + k, " AT:");
        linebuf[k] = 0;
        ui_dialog_ask(linebuf, buf, sizeof buf);
        n = grab_digits(buf, d, 8);
        if (n != 2 || d[0] < 1 || d[0] > 8 || d[1] < 1 || d[1] > 8) {
            snd_beep();
            ui_dialog_line(S(S_77));
            continue;
        }
        fire_one_torpedo((uint8_t)(d[0] - 1), (uint8_t)(d[1] - 1));
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
    /* Bulk data lives on the disk, not in the binary -- see core/farmem.h.
       The prose goes first because everything draws with it; the music is a
       luxury by comparison. Neither is fatal if missing: a disk without
       STRINGS.DAT plays with blank labels, one without MUSIC.DAT plays
       silently, and both beat refusing to start. */
    str_load();

    snd_init();
    {
        uint16_t mb = far_load("MUSIC.DAT");
        snd_music_data(mb, (unsigned char)(mb != FAR_NONE));
    }

    /* One pass per game. The original loops the whole cycle -- title, setup,
       game, evaluation, hall of fame, "Play Again?" -- and answering YES puts
       you back on the TITLE screen with every setup question asked again,
       which is what this reproduces. Verified by playing two games through
       the original end to end. */
    for (;;) {
        /* The title track belongs to the title screen and nothing else. It
           stops the moment the briefing question appears -- Jamie played this
           game and said so, and the original's own player state confirms it:
           the track is running on the title screen and stopped by the time the
           setup screen is up. An earlier version of this let it run through
           setup, which was a guess this file admitted to at the time. */
        snd_music(MUS_TITLE);
        ui_title();
        snd_music(MUS_NONE);
        ui_setup(&setup);
        /* The seed comes out of how long the player took to answer, so no two
           sittings get the same galaxy. Before this, GAME_SEED was a constant
           and every game was identical -- NOTES item 3, blocked on this screen
           since the start. kb_entropy keeps counting across games, so a second
           game is not a replay of the first even at the same command level. */
        /* A restored game already HAS a galaxy, a ship and an event queue --
           see ui_setup(). Calling trek_new_game() here would generate a fresh
           one straight over the top of it, which is the whole reason Setup
           carries the flag rather than main guessing from the level. */
        if (!setup.restored)
            trek_new_game(setup.level, setup.seed);

        /* The message log is the one piece of UI state that outlives a game:
           msg_count is a file static in ui.c, and without this the second
           game opens with the first one's damage reports still in the boxes. */
        ui_clear_messages();
        alert_quad = 0xFF;      /* a new galaxy alerts on its first quadrant */

        ui_draw_all();
        ui_message("HELM: ", S(S_9));

        for (;;) {
            ui_read_command(cmd, sizeof cmd);

            c = (unsigned char)cmd[0];

            /* Only L and T pass 1: enemies manoeuvre when we shoot at them, not
               merely when a turn passes. MEASURED -- see trek.h. */
            /* Word commands first: SHUP and SHDN both begin with S, which the
               card gives to self destruct. */
            if      (word_is(cmd, "SAVE")) ui_save_game(&setup);
            else if (word_is(cmd, "MSGS")) ui_messages_view();
            else if (word_is(cmd, "SND"))  do_sound();
            else if (word_is(cmd, "HAIL")) { do_hail(); enemy_turn(0); }
            else if (word_is(cmd, "INFO")) do_info();
            else if (word_is(cmd, "SHUP")) { do_shields_up();   enemy_turn(0); }
            else if (word_is(cmd, "SHDN")) { do_shields_down(); enemy_turn(0); }
            else if (word_is(cmd, "MAX"))  { do_max_energy();   enemy_turn(0); }
            else if (c == KB_M)      { do_move(cmd);    enemy_turn(0); }
            else if (c == KB_L) { do_lasers();     enemy_turn(1); }
            else if (c == KB_T) { do_torpedo(cmd); enemy_turn(1); }
            else if (c == KB_D) { do_dock();       enemy_turn(0); }
            else if (c == KB_S) do_self();     /* S)elf, per the card */
            else if (c == KB_E) { do_energy();     enemy_turn(0); }
            else if (c == KB_X) { do_max_energy(); enemy_turn(0); }
            else if (c == KB_R) do_repair();   /* a report, not a turn */
            /* MEASURED 2026-08-24: the arrows are the ORIGINAL's primary
               binding for shields -- EGATREK.REF says "Shields Up (use up
               arrow)" and the in-game F1 help lists only the arrows. SHUP
               and SHDN stay as the spoken names, as the card gives both. */
            else if (c == KB_UP)   { do_shields_up();   enemy_turn(0); }
            else if (c == KB_DOWN) { do_shields_down(); enemy_turn(0); }
            else if (c == KB_C) do_chart();    /* MEASURED: costs no turn */
            else if (c == KB_F) do_fix();      /* choosing does not cost one either */
            else if (c == KB_W) do_warp(cmd);   /* setting speed is not a turn */
            /* A#, MEASURED: dismisses one message from the panel by its
               position, and a bare A dismisses all of them. The digit is
               optional, which is why this is not a word_is() -- and it must
               be tested AFTER the word commands, or "A" would swallow
               nothing but is cheap enough to sit here with the other
               single-key ones. Costs no turn. */
            else if (c == KB_A) ui_ack(ack_arg(cmd));
            /* MEASURED: the original answers Q with "Quit <Y/N>?" in the
               COMMAND panel rather than quitting on the keystroke. */
            else if (c == KB_Q) { if (ui_confirm(S(S_65))) break; }
            /* DERIVED, not measured: the original beeps at a field its
               parser refuses, not at an unknown command. A refused order is
               near enough the same thing, but the distinction is real and
               belongs in the comment rather than being quietly lost. */
            else if (c) {
                snd_beep();
                ui_message(S(S_18), S(S_48));
            }

            /* After the enemy turn, not just after a move: a tractor beam
               drags the ship to another quadrant on someone else's turn, and
               arriving that way deserves the alert every bit as much. */
            alert_check();

            ui_draw_scan();
            ui_draw_chart();
            ui_draw_status();
            ui_draw_systems();
            ui_draw_lasers();
            ui_draw_viewer();
            ui_draw_position();

            if (trek_game_state() != GAME_ON) break;
        }

        /* The original ends with two full screens, not a line in the log: the
           Detailed Evaluation and then the Hall of Fame. Both layouts are MEASURED
           -- see MEASURED.md -- and the evaluation is filled straight from the
           core's own score sheet, so what it prints is what was scored. */
        if (trek_game_state() == GAME_WON)       ui_message("HQ: ", S(S_68));
        else if (trek_game_state() == GAME_LOST) ui_message("HQ: ", S(S_79));

        /* And 0x071A at the end, read the same way at the evaluation screen. */
        snd_music(MUS_END);
        ui_evaluation();
        ui_hall_of_fame(setup.name, setup.level, trek_score());

        if (!ui_play_again()) break;
    }

    /* Quit has to visibly end the game. Milestone 1 parked here in an
       endless wait_vsync() so the console would stay on screen, which made
       Q indistinguishable from a hang once there was a command loop to quit
       out of. Acknowledge, wait for the captain, then hand the machine back
       in the state we found it. */
    snd_music(MUS_NONE);

    scr_clear();
    scr_puts(28, 10, S(S_49), EGA_TO_VDC(EGA_LTGREEN));
    scr_puts(23, 12, S(S_10), EGA_TO_VDC(EGA_LTCYAN));

    /* Drop back to 1MHz first, so the VIC-IIe screen is live again and the
       machine does not look dead on the other window. */
    vdc_shutdown();
    snd_off();      /* a SID still gated would howl at BASIC forever */

    /* NOTES.md open item 2 -- "returning to BASIC wedges the C128" -- was
     * never true of this program, or stopped being true before anyone could
     * measure it. Settled 2026-08-22 and the park loop is gone.
     *
     * The note it replaces named three suspects and a bisect: the 2MHz
     * switch, the direct VDC register writes, and scanning CIA1 behind the
     * KERNAL's back. c128/test/exit_bisect.c runs that bisect for real, one
     * suspect per stage, and all four stages hand the machine back to a BASIC
     * that then evaluates arithmetic. Then tools/exit_real.py played THIS
     * binary through to a quit and did the same: BASIC came back and answered
     * PRINT 6*7 with 42.
     *
     * Why it stood for so long is the part worth keeping. The original note
     * says there was "no way to observe the machine from a session on this
     * host" -- a claim NOTES.md now records as simply wrong; the binary
     * monitor was there all along. So the wedge was never diagnosed, only
     * assumed, and the assumption cost every player an exit.
     *
     * The trap is real, though, and it caught this session too. A program
     * still blocked in kb_waitkey() is indistinguishable from a wedged
     * machine: both ignore the keyboard, and the C128 still shows READY
     * because BASIC printed it before the program ran. The first attempt at
     * exit_real.py desynced its scripted keys, never reached the quit, and
     * reported WEDGED. That is the same false positive the original note
     * almost certainly recorded. Deciding it needs a question only a live
     * BASIC can answer -- hence the arithmetic. */
    return 0;
}
