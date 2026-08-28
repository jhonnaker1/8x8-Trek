#include <stdint.h>
#include <string.h>

#include "vdc.h"
#include "egavdc.h"
#include "layout.h"
#include "ui.h"
#include "../../core/strpool.h"
#include "../../core/overlay.h"
#include "strdata.h"
#include "input.h"
#include "sid.h"
#include "../../core/farmem.h"
#include "../../core/trek.h"
#include "../../core/planet.h"

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
        case SEC_COMMAND: return S(S_137);
        case SEC_SCOUT:   return S(S_157);
        case SEC_SUPPLY:  return S(S_158);
        default:          return S(S_159);
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
        case SHIELD_BOARDED:
            ui_message(S(S_262), S(S_256));
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
                uint8_t k = put_str(linebuf, S(S_168));
                k += put_u16(linebuf + k, lost);
                k += put_str(linebuf + k, S(S_104));
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

OVL_CODE("cmds") static void do_energy(void) {
    char line[8];
    uint16_t from, to, amount, lost = 0;

    ui_dialog_open(S(S_29));
    ui_dialog_line(S(S_2));

    ui_dialog_ask(S(S_37), line, sizeof line);
    from = grab_num(line);
    ui_dialog_ask(S(S_186), line, sizeof line);
    to = grab_num(line);
    ui_dialog_ask(S(S_164), line, sizeof line);
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

OVL_CODE("cmds") static void do_dock(void) {
    switch (trek_dock()) {
        case DOCK_OK:
            /* Name what this base actually gave us -- a research station
               resupplies nothing this core models, and saying "DOCKED" alone
               would leave the player wondering why the tubes are still
               empty. */
            switch (ship.docked) {
                case BASE_STARBASE:
                    ui_message(S(S_170), S(S_23));
                    break;
                case BASE_SUPPLY:
                    ui_message(S(S_170), S(S_25));
                    break;
                default:
                    ui_message(S(S_170), S(S_24));
                    break;
            }
            break;
        case DOCK_ALREADY:
            ui_message(S(S_170), S(S_92));
            break;
        default:
            /* MEASURED, and it is the ORIGINAL'S OWN WORDING, planet and all:
               "NAVIGATION: Not adjacent to planet." -- a base, in the game
               that says it. This port used to answer HELM: NO BASE ALONGSIDE,
               which is our prose for a line we had already read off the
               screen. Department NAVIGATION, not HELM, for the same reason. */
            ui_message(S(S_52), S(S_53));
            break;
    }
}

/* ------------------------------------------------- the planet chain

   ORBIT, LAND and USE. The model and every constant behind it are in
   core/planet.h; this is only the wording and the dialogs. The original's
   own strings are in reference/strings.txt and are followed where they fit
   -- the panel gives a message 38 cells minus its department, and several of
   Anderson's lines are twice that, so the long ones are said shorter rather
   than truncated. See check_message_widths() in tools/verify_prg.py for why
   truncation is not an option. */

/* The planet's name and class, "GAMMA REGULA-8, TYPE O". The digit is the
   1-based quadrant ROW and is not stored -- see core/planet.h. */
static uint8_t put_planet(char *buf, const Planet *p) {
    uint8_t k = put_str(buf, PLANET_NAME_OF(p->quad));
    buf[k++] = '-';
    k += put_u16(buf + k, PLANET_DIGIT_OF(p->quad));
    return k;
}

/* What the orbit scan says. SCIENCE has 29 cells to say it in, so these are
   the original's four findings in this port's own words rather than its
   sentences: "SCIENCE: Scanners indicate the presence of energium on planet."
   is 53 characters before the department is counted. */
static const char *scan_line(const Planet *p) {
    switch (p->find) {
        case PFIND_ENERGIUM: return "ENERGIUM PRESENT ON PLANET.";
        case PFIND_MONGOL:   return "A MONGOL SUPPLY STATION.";
        default:             return "NOTHING OF INTEREST.";
    }
}

/* The settlement line is SEPARATE from the find, and the original prints both
   for the one settled world -- its ORBIT tests the quadrant before it decodes
   the per-quadrant byte and falls straight through into it. "Destroyed" is
   computed from the deadline rather than stored, so this is a call and not a
   flag test. */
static const char *settle_line(void) {
    return planet_settlement_lost() ? "A DESTROYED SETTLEMENT."
                                    : "A SETTLEMENT ON THE PLANET.";
}

OVL_CODE("planet") static void do_orbit(void) {
    const Planet *p;
    uint8_t k;

    switch (trek_orbit()) {
        case ORBIT_OK:
            p = &planets[ship.orbiting];
            ui_message(S(S_52), S(S_204));
            /* The original's second line reads "Planet Sigma-7, Type O." and
               will not fit behind NAVIGATION: with the longest of the seven
               names. The word PLANET goes, since the line above just said
               we are orbiting one. */
            k = put_planet(linebuf, p);
            k += put_str(linebuf + k, S(S_193));
            linebuf[k++] = planet_class_letter[p->cls];
            linebuf[k] = 0;
            ui_message(S(S_52), linebuf);
            if (p->flags & PF_SETTLED) ui_message(S(S_178), settle_line());
            ui_message(S(S_178), scan_line(p));
            break;
        case ORBIT_ALREADY:
            ui_message(S(S_52), S(S_240));
            break;
        default:
            /* The original's own words, and the string this port already
               had -- see the note in do_dock about which command it was
               measured on. */
            ui_message(S(S_52), S(S_53));
            break;
    }
}

/* LAND and USE both live in the PLANET overlay -- they are dialogs that run
   only when asked, and between them they are most of a kilobyte. do_orbit
   stays resident: it is three messages, and it is on the path of every turn
   that ends in one.

   LAND. The original's four-beat sequence per route, then the finding.

   The manual is the authority on the choice: the transporter "is the better
   choice since the shuttlecraft takes 0.2 stardays to make the round trip
   whereas the transporter is virtually instantaneous". Both are 100%-or-
   nothing, and the menu marks a damaged one the way the original does, with
   a "(damaged)" suffix rather than by hiding the option. */
OVL_CODE("planet") static void do_land(void) {
    char buf[8];
    uint16_t cas = 0;
    uint8_t how, r, k;

    if (ship.orbiting == PLANET_NONE) {
        /* Anderson's spelling, two t's and all -- the same rule the DOCK
           refusal follows. */
        ui_message(S(S_52), S(S_220));
        return;
    }
    if (ship.shields_up) {
        /* "ENGINEERING: Cannot use transporters or shuttlecraft with shields
           up." is 62 characters against the 25 the panel gives ENGINEERING. */
        ui_message(S(S_31), S(S_229));
        return;
    }

    ui_dialog_open(S(S_209));
    ui_dialog_line(S(S_208));

    k = put_str(linebuf, S(S_188));
    if (!trek_shuttle_ok()) k += put_str(linebuf + k, S(S_191));
    linebuf[k] = 0;
    ui_dialog_line(linebuf);

    k = put_str(linebuf, S(S_189));
    if (!trek_transporter_ok()) k += put_str(linebuf + k, S(S_191));
    linebuf[k] = 0;
    ui_dialog_line(linebuf);

    ui_dialog_line(S(S_190));
    ui_dialog_ask(S(S_242), buf, sizeof buf);

    how = (uint8_t)grab_num(buf);
    if (how != 1 && how != 2) {
        ui_dialog_line(S(S_194));
        ui_dialog_close();
        return;
    }
    how = (uint8_t)(how == 1 ? LAND_BY_SHUTTLE : LAND_BY_TRANSPORTER);

    /* A FRESH PAGE FOR THE TRIP. The menu is five rows of the dialog's nine,
       and the four beats plus the finding plus the closing prompt is six
       more -- so on one page the box reframes mid-sequence and the player is
       left looking at an empty dialog with HIT RETURN in it. The landing had
       worked; it just said nothing.

       Caught on the machine and not by a test: the inventory held the
       crystal while the screen held nothing, which is the same silent shape
       the F)ix system list failed in. */
    ui_dialog_open(S(S_209));

    /* The first two beats go out BEFORE the core is asked, because they are
       the trip and the trip happens whatever is waiting on the surface. */
    if (how == LAND_BY_SHUTTLE) {
        if (!trek_shuttle_ok()) {
            ui_dialog_line(S(S_230));
            ui_dialog_close();
            return;
        }
        ui_dialog_line(S(S_215));
        ui_dialog_line(S(S_212));
    } else {
        if (!trek_transporter_ok()) {
            ui_dialog_line(S(S_236));
            ui_dialog_close();
            return;
        }
        ui_dialog_line(S(S_214));
        ui_dialog_line(S(S_213));
    }

    r = trek_land(how, &cas);
    switch (r) {
        case LAND_ENERGIUM:
            ui_dialog_line(S(S_201));
            break;
        case LAND_SETTLERS:
            ui_dialog_line(S(S_226));
            ui_dialog_line(S(S_205));
            break;
        case LAND_ATTACKED:
            k = put_str(linebuf, S(S_210));
            k += put_u16(linebuf + k, cas);
            k += put_str(linebuf + k, S(S_192));
            linebuf[k] = 0;
            ui_dialog_line(linebuf);
            break;
        case LAND_ALREADY:
            ui_dialog_line(S(S_223));
            break;
        default:
            ui_dialog_line(S(S_221));
            break;
    }

    ui_dialog_line(how == LAND_BY_SHUTTLE ? S(S_231)
                                          : S(S_211));
    ui_dialog_close();
}

/* USE. One item has a source in this core -- raw energium -- but the dialog
   is built off the inventory array rather than around that one item, because
   the original's is: it prints a numbered list of the types held, with the
   quantity in brackets, and the four other types are already named in
   core/planet.h waiting for something to produce them. */
OVL_CODE("planet") static void do_use(void) {
    char buf[8];
    uint8_t i, k, n = 0, pick;
    uint8_t slot[ITEM_COUNT];

    for (i = 0; i < ITEM_COUNT; i++)
        if (inventory[i]) slot[n++] = i;

    if (!n) {
        /* "ENGINEERING: No energium crystals are available to load." */
        ui_message(S(S_31), S(S_224));
        return;
    }

    ui_dialog_open(S(S_239));
    ui_dialog_line(S(S_241));
    for (i = 0; i < n; i++) {
        k = put_str(linebuf, "  ");
        k += put_u16(linebuf + k, (uint16_t)(i + 1));
        k += put_str(linebuf + k, ". ");
        k += put_str(linebuf + k, item_name[slot[i]]);
        k += put_str(linebuf + k, " (");
        k += put_u16(linebuf + k, inventory[slot[i]]);
        linebuf[k++] = ')';
        linebuf[k] = 0;
        ui_dialog_line(linebuf);
    }
    ui_dialog_ask(S(S_242), buf, sizeof buf);

    pick = (uint8_t)grab_num(buf);
    if (pick < 1 || pick > n) { ui_dialog_close(); return; }

    /* The reserve life support canister. BINARY 0x009861: a whole stardate,
       clamped at the same 2.0 the dock refills to, and refused unless the
       ship is actually on reserve -- the original answers "Not on reserve
       life support." and does NOT spend the item. */
    if (slot[pick - 1] == ITEM_LIFE_SUPPORT) {
        if (!trek_life_replenish()) {
            ui_dialog_line(S(S_252));
        } else {
            inventory[ITEM_LIFE_SUPPORT]--;
            ui_dialog_line(S(S_254));
        }
        ui_dialog_close();
        ui_draw_all();
        return;
    }

    if (slot[pick - 1] != ITEM_RAW_ENERGIUM) {
        /* Named, held, and with nothing yet that uses it. Saying so is
           better than a silent no-op. */
        ui_dialog_line(S(S_219));
        ui_dialog_close();
        return;
    }

    /* The gate is the manual's, and refusing here rather than after the
       warning is the original's order too: the First Officer speaks up
       before Engineering is ever asked. */
    if (!trek_energium_allowed()) {
        ui_dialog_open(S(S_239));
        ui_dialog_line(S(S_207));
        ui_dialog_line(S(S_195));
        ui_dialog_line(S(S_238));
        ui_dialog_close();
        return;
    }

    /* A fresh page, for the reason LAND takes one: the item list plus the
       warning plus the outcome does not fit nine rows. */
    ui_dialog_open(S(S_239));
    ui_dialog_line(S(S_203));
    ui_dialog_line(S(S_234));
    ui_dialog_ask(S(S_227), buf, sizeof buf);
    if (buf[0] != KB_Y) { ui_dialog_close(); return; }

    ui_dialog_line(S(S_196));

    {
        /* No event array: a defective crystal costs ENERGY and damages no
           system, so there is nothing for the core to report. It used to be
           TrekEvent ev[4] against a trek_wreck_system() that the binary says
           does not happen. */
        uint8_t r = trek_use_energium(0, 0);
        switch (r) {
            case USE_GOOD:
                ui_dialog_line(S(S_198));
                ui_dialog_line(S(S_202));
                break;
            case USE_DEFECTIVE:
                ui_dialog_line(S(S_237));
                ui_dialog_line(S(S_233));
                ui_dialog_line(S(S_199));
                break;
            case USE_DUD:
                ui_dialog_line(S(S_235));
                ui_dialog_line(S(S_217));
                break;
            default:
                ui_dialog_line(S(S_222));
                break;
        }
    }
    ui_dialog_close();
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

    /* Before the target sweep, as the original tests it at 0x009CD0. */
    if (ship.boarders == BOARD_LASERS) {
        snd_beep();
        ui_message(S(S_262), S(S_257));
        return;
    }

    for (cell = 0; cell < QUAD_CELLS; cell++)
        if (SEC_IS_ENEMY(sector[cell])) found++;

    if (!found) {
        snd_beep();
        ui_message(S(S_178), S(S_55));
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
            uint8_t n = put_str(linebuf, S(S_105));
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
                n += put_str(linebuf + n, S(S_103));
                n += put_str(linebuf + n, enemy_name(what));
                linebuf[n] = 0;
                ui_dialog_line(linebuf);
                if (killed) ui_dialog_line(S(S_50));
                /* Past 90 the banks damage themselves and the original says
                   so, with the new percentage. */
                if (laser_overheated) {
                    uint8_t m = put_str(linebuf, S(S_248));
                    m += put_u16(linebuf + m, ship.sys[SYS_LASERS]);
                    m += put_str(linebuf + m, S(S_247));
                    linebuf[m] = 0;
                    ui_dialog_line(linebuf);
                }
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
    /* Caught in flight. The core sets this on a warp move that ended
       somewhere the captain did not ask for. */
    if (tractored) {
        uint8_t k = put_str(linebuf, S(S_249));
        k += put_u16(linebuf + k, (uint16_t)(ship.quad_y + 1));
        linebuf[k++] = '-';
        k += put_u16(linebuf + k, (uint16_t)(ship.quad_x + 1));
        linebuf[k++] = '.';
        linebuf[k] = 0;
        tractored = 0;
        ui_message(S(S_167), linebuf);
    }
    switch (r) {
        case MOVE_OK:
            break;
        case MOVE_BLOCKED: {
            /* MEASURED: the original names the cell, and the ship has already
               moved -- it is standing in the sector before this one. Its
               wording is "NAVIGATION: Move blocked by object at 7-6.", so the
               coordinates are 1-based and joined by a hyphen. */
            uint8_t k = put_str(linebuf, S(S_11));
            k += put_str(linebuf + k, " AT ");
            /* Sectors are 0..7, so the digits are written straight rather
               than through put_u16 -- two calls to a general 16-bit
               formatter cost a couple of hundred bytes here. */
            linebuf[k++] = (char)('1' + trek_block_y);
            linebuf[k++] = '-';
            linebuf[k++] = (char)('1' + trek_block_x);
            linebuf[k] = 0;
            ui_message(S(S_52), linebuf);
            break;
        }
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
    ovl_load(OVL_REPAIR);
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
OVL_CODE("cmds") static void do_self(void) {
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
      k += put_str(linebuf + k, S(S_100));
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
   a different repair RATE for the chosen system, 60 points a stardate adrift
   and 100 docked, straight off the manual's own table. See trek.h, including
   why those two are not the undocked rates multiplied by anything. */
OVL_CODE("cmds") static void do_fix(void) {
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
        { uint8_t k = put_str(row, S(S_111));
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

/* HAIL. The empty COMMUNICATIONS box the 2026-08-23 session saw was the
   original's "No response." -- there was no StarBase in range, which is
   exactly the case fn 0x207FD prints it for. All three replies are read now;
   see trek.h. In OVL_CMDS because a hail is an occasional command and its
   prose has no business resident. */
OVL_CODE("cmds") static void do_hail(void) {
    uint8_t qy = 0, qx = 0, k;

    switch (trek_hail(&qy, &qx)) {
        case HAIL_BLOCKED:
            ui_message(S(S_16), S(S_274));
            return;
        case HAIL_RESPONDS:
            k  = put_str(linebuf, S(S_273));
            k += put_sector(linebuf + k, qy, qx);
            k += put_str(linebuf + k, S(S_271));
            linebuf[k] = 0;
            ui_message(S(S_16), linebuf);
            return;
        default:
            ui_message(S(S_16), S(S_272));
            return;
    }
}

/* SND, from the reference card. The original keeps a sound on/off byte that
   every one of its sound sites tests first -- [0x1cc8] in its data segment,
   found while extracting the music -- so this is its mechanic, not an
   invention. */
static void do_sound(void) {
    snd_toggle();
    ui_message(S(S_18), snd_enabled() ? S(S_182) : S(S_181));
}

static void do_info(void) {
    ovl_load(OVL_INFO);
    ui_info_panel();
    ui_draw_all();
}

/* PLANET LIST, in the same shape as INFO and STATE OF REPAIR: an overlay is
   loaded, the report paints over the console, and the console is redrawn
   when the player is done with it. */
static void do_planets(void) {
    ovl_load(OVL_PLANET);
    ui_planet_list();
    while (kb_waitkey() != KB_RETURN) { }
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

/* THE RARE EVENTS' PROSE, in OVL_MSGS.
 *
 * Second overlay pass, 2026-08-27. enemy_turn() was 6,103 resident bytes and
 * most of the growth was here: nine event kinds whose message formatting is
 * resident and whose events fire once in dozens of turns. A base falling, a
 * tractor beam, a boarding party, a supernova -- none of them is a hot path,
 * and every one of them was paying rent next to the enemy fire loop.
 *
 * The four COMMON kinds stay resident and keep the shared tail: a hit, a
 * shield hold, a system knocked out, an enemy moving happen most turns.
 *
 * OVL_MSGS rather than a tenth overlay because it is already the message
 * viewer and had 3,554 bytes spare. Nothing here calls into another overlay;
 * put_str, put_u16, put_sector, ui_message and linebuf are all resident. */
OVL_CODE("msgs") static void report_rare_event(const TrekEvent *e, uint8_t *sfx) {
    uint8_t k = 0;
    switch (e->kind) {
        case EV_BASE_ATTACKED:
            /* The deadline is the point of the message, as it is in the
               original: "They can last until 3517.8." */
            k  = put_str(linebuf, "BASE ");
            k += put_sector(linebuf + k, e->y, e->x);
            k += put_str(linebuf + k, S(S_160));
            k += put_tenths_str(linebuf + k, e->amount);
            linebuf[k] = 0;
            ui_message(S(S_166), linebuf);
            return;
        case EV_BASE_LOST:
            k  = put_str(linebuf, "BASE ");
            k += put_sector(linebuf + k, e->y, e->x);
            k += put_str(linebuf + k, S(S_98));
            linebuf[k] = 0;
            ui_message(S(S_166), linebuf);
            return;
        case EV_TRACTORED:
            k  = put_str(linebuf, S(S_113));
            k += put_sector(linebuf + k, e->y, e->x);
            linebuf[k] = 0;
            ui_message(S(S_170), linebuf);
            return;
        case EV_POD_HIT:
            *sfx = SFX_D;      /* MEASURED: the pod has its own sound */
            k  = put_str(linebuf, S(S_112));
            k += put_u16(linebuf + k, e->amount);
            k += put_str(linebuf + k, S(S_162));
            linebuf[k] = 0;
            ui_message(S(S_167), linebuf);
            return;
        case EV_SHIP_LOST:
            ui_message(S(S_170), S(S_93));
            return;
        case EV_NOVA:
            /* "COMMUNICATIONS: Dept. of Space warns of a star having
               gone supernova in quadrant R-C" and, when it took Mongols
               with it, ";  N Mongols reported destroyed." -- cs:0x2F28
               and cs:0x2F83, cut to the 26 this panel fits. */
            k  = put_str(linebuf, S(S_264));
            k += put_sector(linebuf + k, e->y, e->x);
            if (e->amount) {
                k += put_str(linebuf + k, "; ");
                k += put_u16(linebuf + k, e->amount);
                k += put_str(linebuf + k, " lost");
            }
            linebuf[k++] = '.';
            linebuf[k] = 0;
            ui_message(S(S_263), linebuf);
            return;
        case EV_BOARDED:
            /* "SECURITY: A Mongol boarding party has transported into
               <department>" -- cs:0x0C4A and the three names after it,
               trimmed to the panel. */
            ui_message(S(S_262),
                   e->y == BOARD_ENGINEERING ? S(S_259)
                 : e->y == BOARD_LASERS      ? S(S_260)
                                        : S(S_258));
            return;
        case EV_BOARDERS_GONE:
            ui_message(S(S_262), S(S_261));
            return;
        case EV_LIFE_GONE:
            /* The original's own line, from cs:0x43AA -- its dialog
               truncates it with an ellipsis and this keeps that. */
            ui_message(S(S_251),
                   S(S_250));
            return;
        default:
            return;
    }
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
                k += put_str(linebuf + k, S(S_97));
                k += put_sector(linebuf + k, ev[i].y, ev[i].x);
                break;
            case EV_HIT:
                if (turn_sfx == 0xFF) turn_sfx = SFX_E;
                k  = put_u16(linebuf, ev[i].amount);
                k += put_str(linebuf + k, S(S_99));
                k += put_sector(linebuf + k, ev[i].y, ev[i].x);
                break;
            case EV_SYSTEM_HIT:
                /* ui_sys_name(), not a second copy of the same twelve
                   names -- this file carried its own switch, which is 143
                   bytes of duplicate prose and one more place to get the
                   wording wrong. It also said CONVERTER where the original
                   says EnergyConverter; the shared table is the right one. */
                k  = put_str(linebuf, ui_sys_name(ev[i].y));
                k += put_str(linebuf + k, " AT ");
                k += put_u16(linebuf + k, ev[i].amount);
                break;
            case EV_ENEMY_MOVED:
                k  = put_str(linebuf, S(S_117));
                k += put_sector(linebuf + k, ev[i].y, ev[i].x);
                linebuf[k] = 0;
                ui_message(S(S_176), linebuf);
                continue;
            /* EV_NONE first: it should never reach here, and if it did the
               default would spend a DISK LOAD on nothing. */
            case EV_NONE:
                continue;
            default:
                ovl_load(OVL_MSGS);
                report_rare_event(&ev[i], &turn_sfx);
                continue;
        }
        linebuf[k] = 0;
        ui_message(S(S_167), linebuf);
    }

    if (turn_sfx != 0xFF) snd_effect(turn_sfx);

    /* The banks shed twenty points a turn whatever the turn was -- the
       original does it here, at the bottom of its main loop. */
    trek_turn_end();
}

/* A supernova's four lines. In OVL_CMDS -- the rare-and-modal overlay --
   because it is 4% of a star hit and star hits are themselves uncommon,
   while fire_one_torpedo around it is the hottest code in the game and must
   stay resident. Splitting it here took resident free from 178 back to a
   workable margin. */
OVL_CODE("cmds") static void report_nova(uint16_t dmg) {
            /* The original's four lines, cut to the dialog's width:
               "Star at R-C goes supernova!", "Lexington blown to quad Q.",
               "N Mongols destroyed.", and the damage. */
            uint8_t m;
            ui_dialog_line(S(S_270));
            if (nova_kills) {
                m  = put_u16(linebuf, nova_kills);
                m += put_str(linebuf + m, S(S_265));
                linebuf[m] = 0;
                ui_dialog_line(linebuf);
            }
            m  = put_u16(linebuf, dmg);
            m += put_str(linebuf + m, ship.shields_up ? S(S_266)
                                                      : S(S_267));
            linebuf[m] = 0;
            ui_dialog_line(linebuf);
            if (ship.lost) {
                ui_dialog_line(S(S_269));
            } else {
                m  = put_str(linebuf, S(S_268));
                m += put_sector(linebuf + m, (uint8_t)(nova_quad >> 3),
                                             (uint8_t)(nova_quad & 7));
                linebuf[m++] = '.';
                linebuf[m] = 0;
                ui_dialog_line(linebuf);
            }
            snd_effect(SFX_D);
}

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
        /* The port DOES ray-march now (2026-08-26), so this fires on a star
           anywhere in the flight path, not only on the aimed-at cell. The
           SUPERNOVA branch is built (2026-08-27); the third outcome, the star
           destroyed outright leaving a nova cell, still is not -- it wants a
           sector code this port does not have. See core/trek.h. */
        case TORP_ABSORBED:
            ui_dialog_line(S(S_85));
            break;
        case TORP_NOVA:
            ovl_load(OVL_CMDS);
            report_nova(dmg);
            break;
        case TORP_PLANET:
            ui_dialog_line(S(S_245));
            break;
        case TORP_BASE_HIT:
            ui_dialog_line(S(S_243));
            break;
        case TORP_DUD:
            ui_dialog_line(S(S_244));
            break;
        case TORP_THROUGH:
            ui_dialog_line(S(S_246));
            break;
        case TORP_OK: {
            uint8_t k = put_str(linebuf, S(S_116));
            k += put_u16(linebuf + k, dmg);
            k += put_str(linebuf + k, S(S_163));
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

    /* Refused BEFORE the salvo is asked for, which is where the original
       tests it -- 0x00B60B, near the top of its torpedo routine. */
    if (ship.boarders == BOARD_TUBES) {
        snd_beep();
        ui_message(S(S_262), S(S_255));
        return;
    }

    if (ship.torps == 0) {
        snd_beep();
        ui_message(S(S_187), S(S_14));
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
            uint8_t k = put_str(linebuf, S(S_107));
            k += put_u16(linebuf + k, (uint16_t)tubes);
            k += put_str(linebuf + k, tubes == 1 ? S(S_101) : S(S_102));
            linebuf[k] = 0;
            snd_beep();
            ui_dialog_line(linebuf);
            salvo = tubes;
        }
    }
    if (salvo > ship.torps) {
        uint8_t k = put_str(linebuf, S(S_108));
        k += put_u16(linebuf + k, (uint16_t)ship.torps);
        k += put_str(linebuf + k, S(S_161));
        linebuf[k] = 0;
        snd_beep();
        ui_dialog_line(linebuf);
        salvo = ship.torps;
    }

    for (shot = 1; shot <= salvo; shot++) {
        uint8_t k = put_str(linebuf, S(S_122));
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
        ovl_load(OVL_TITLE);
        ui_title();
        snd_music(MUS_NONE);
        ovl_load(OVL_FRONT);
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
        ui_message(S(S_170), S(S_9));

        for (;;) {
            ui_read_command(cmd, sizeof cmd);

            c = (unsigned char)cmd[0];

            /* Only L and T pass 1: enemies manoeuvre when we shoot at them, not
               merely when a turn passes. MEASURED -- see trek.h. */
            /* Word commands first: SHUP and SHDN both begin with S, which the
               card gives to self destruct. */
            if      (word_is(cmd, "SAVE")) { ovl_load(OVL_FRONT); ui_save_game(&setup); }
            else if (word_is(cmd, "MSGS")) { ovl_load(OVL_MSGS); ui_messages_view(); }
            else if (word_is(cmd, "SND"))  do_sound();
            else if (word_is(cmd, "HAIL")) { ovl_load(OVL_CMDS); do_hail();
                                             enemy_turn(0); }
            else if (word_is(cmd, "INFO")) do_info();
            /* LAND and USE are WORDS on the original's reference card --
               "LAND      send Landing party to planet", "USE       Use a
               miscellaneous item" -- and they have to be, because L is the
               lasers and U is nothing. O)rbit is the single key the card
               gives it, and sits with the other letters below. */
            else if (word_is(cmd, "LAND")) { ovl_load(OVL_PLANET); do_land();
                                             enemy_turn(0); }
            else if (word_is(cmd, "USE"))  { ovl_load(OVL_PLANET); do_use();
                                             enemy_turn(0); }
            /* PLAN is this port's own, for a report the original reaches
               some other way -- see ui_planet_list. A report, so no turn. */
            else if (word_is(cmd, "PLAN")) do_planets();
            else if (word_is(cmd, "SHUP")) { do_shields_up();   enemy_turn(0); }
            else if (word_is(cmd, "SHDN")) { do_shields_down(); enemy_turn(0); }
            else if (word_is(cmd, "MAX"))  { do_max_energy();   enemy_turn(0); }
            else if (c == KB_M)      { do_move(cmd);    enemy_turn(0); }
            else if (c == KB_L) { do_lasers();     enemy_turn(1); }
            else if (c == KB_T) { do_torpedo(cmd); enemy_turn(1); }
            else if (c == KB_D) { ovl_load(OVL_CMDS); do_dock();
                                  enemy_turn(0); }
            else if (c == KB_O) { ovl_load(OVL_PLANET); do_orbit();
                                  enemy_turn(0); }
            else if (c == KB_S) { ovl_load(OVL_CMDS); do_self(); }  /* S)elf */
            else if (c == KB_E) { ovl_load(OVL_CMDS); do_energy();
                                  enemy_turn(0); }
            else if (c == KB_X) { do_max_energy(); enemy_turn(0); }
            else if (c == KB_R) do_repair();   /* a report, not a turn */
            /* MEASURED 2026-08-24: the arrows are the ORIGINAL's primary
               binding for shields -- EGATREK.REF says "Shields Up (use up
               arrow)" and the in-game F1 help lists only the arrows. SHUP
               and SHDN stay as the spoken names, as the card gives both. */
            else if (c == KB_UP)   { do_shields_up();   enemy_turn(0); }
            else if (c == KB_DOWN) { do_shields_down(); enemy_turn(0); }
            else if (c == KB_C) do_chart();    /* MEASURED: costs no turn */
            else if (c == KB_F) { ovl_load(OVL_CMDS); do_fix(); }   /* no turn */
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
        /* THE ONLY WAY INTO THE EVALUATION OVERLAY. Loading and calling
           together is what stops the two ever being separated -- see the
           three rules in core/overlay.h. */
        ovl_load(OVL_EVAL);
        ui_evaluation();
        ovl_load(OVL_HOF);
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
