#include <string.h>

#include "vdc.h"
#include "layout.h"
#include "ui.h"
#include "input.h"
#include "egavdc.h"
#include "../../core/trek.h"
#include "../../core/ega.h"

/* Raw screen codes. Letters are 1..26 in the C64/C128 set, digits 48..57 --
   these bypass scr_puts deliberately, since scr_put takes a raw code (see
   the split documented in vdc.h). */
#define SC_SPACE   32
#define SC_STAR    42    /* '*' */
#define SC_HASH    35    /* '#' */
#define SC_DOT     46
#define SC_DIGIT0  48
#define SC_A        1
#define SC_LETTER(c) ((unsigned char)((c) - 'A' + 1))

#define COL_LABEL   EGA_TO_VDC(EGA_LTGRAY)
#define COL_VALUE   EGA_TO_VDC(EGA_WHITE)
#define COL_GRID    EGA_TO_VDC(EGA_DKGRAY)
#define COL_SHIP    EGA_TO_VDC(EGA_WHITE)
#define COL_STAR    EGA_TO_VDC(EGA_YELLOW)
#define COL_BASE    EGA_TO_VDC(EGA_CHART_BASE)
#define COL_MSG     EGA_TO_VDC(EGA_LTGREEN)
#define COL_DEPT    EGA_TO_VDC(EGA_LTCYAN)
#define COL_UNKNOWN EGA_TO_VDC(EGA_DKGRAY)

/* ----------------------------------------------------------- primitives */

/* Right-aligned unsigned decimal in a field `w` wide, space padded. Written
   here rather than with sprintf: cc65's stdio would pull a formatter into a
   2K binary for no reason, and this needs no allocation. */
static void put_num(unsigned char x, unsigned char y, uint16_t v,
                    unsigned char w, unsigned char color) {
    unsigned char buf[6];
    unsigned char n = 0;

    do { buf[n++] = (unsigned char)(SC_DIGIT0 + (v % 10)); v /= 10; } while (v && n < 6);

    while (w > n) { scr_put(x++, y, SC_SPACE, color); w--; }
    while (n) scr_put(x++, y, buf[--n], color);
}

/* A stardate as t.d -- the value is carried in tenths. */
static void put_tenths(unsigned char x, unsigned char y, uint16_t tenths,
                       unsigned char color) {
    put_num(x, y, (uint16_t)(tenths / 10), 4, color);
    scr_put((unsigned char)(x + 4), y, SC_DOT, color);
    scr_put((unsigned char)(x + 5), y,
            (unsigned char)(SC_DIGIT0 + (tenths % 10)), color);
}

/* Blanks a panel's interior, leaving its border and title intact. */
static void clear_panel(unsigned char p) {
    const Panel *pp = &panels[p];
    scr_fill_rect((unsigned char)(pp->x + 1), (unsigned char)(pp->y + 1),
                  (unsigned char)(pp->w - 2), (unsigned char)(pp->h - 2),
                  SC_SPACE, COL_LABEL);
}

/* --------------------------------------------------------- short range */

/* Glyph and colour for a sector cell. The colours are game rules, not
   decoration -- core/ega.h carries them because the manual (l.272) makes
   enemy type readable by hue. */
static unsigned char cell_glyph(unsigned char c, unsigned char *color) {
    switch (c) {
        case SEC_SHIP:       *color = COL_SHIP;
                             return SC_LETTER('E');
        case SEC_STAR:       *color = COL_STAR;
                             return SC_STAR;
        case SEC_BASE:       *color = COL_BASE;
                             return SC_HASH;
        case SEC_PLANET:     *color = EGA_TO_VDC(EGA_LTGREEN);
                             return SC_LETTER('O');
        case SEC_BATTLESHIP: *color = EGA_TO_VDC(EGA_MONGOL_BATTLESHIP);
                             return SC_LETTER('K');
        case SEC_COMMAND:    *color = EGA_TO_VDC(EGA_MONGOL_COMMAND);
                             return SC_LETTER('C');
        case SEC_SCOUT:      *color = EGA_TO_VDC(EGA_MONGOL_SCOUT);
                             return SC_LETTER('S');
        case SEC_SUPPLY:     *color = EGA_TO_VDC(EGA_MONGOL_SUPPLY);
                             return SC_LETTER('P');
        default:             *color = COL_GRID;
                             return SC_DOT;
    }
}

void ui_draw_scan(void) {
    const Panel *p = &panels[P_SCAN];
    unsigned char row, col, glyph, color;
    unsigned char x0 = (unsigned char)(p->x + 4);
    unsigned char y0 = (unsigned char)(p->y + 3);

    clear_panel(P_SCAN);

    /* Column headers and row labels are 1-based, as the original presents
       them; the core is 0-based throughout. */
    for (col = 0; col < QUAD_DIM; col++)
        scr_put((unsigned char)(x0 + col * 2), (unsigned char)(p->y + 2),
                (unsigned char)(SC_DIGIT0 + col + 1), COL_LABEL);

    for (row = 0; row < QUAD_DIM; row++) {
        scr_put((unsigned char)(p->x + 2), (unsigned char)(y0 + row),
                (unsigned char)(SC_DIGIT0 + row + 1), COL_LABEL);

        for (col = 0; col < QUAD_DIM; col++) {
            glyph = cell_glyph(sector[(row << 3) | col], &color);
            scr_put((unsigned char)(x0 + col * 2), (unsigned char)(y0 + row),
                    glyph, color);
        }
    }
}

/* --------------------------------------------------------------- chart */

/* The three-digit quadrant code the original's long range display uses:
   enemies, base type, stars (manual l.286-292). Unscanned quadrants show
   dots. Quadrants holding enemies are highlighted red, bases orange -- both
   from core/ega.h. */
void ui_draw_chart(void) {
    const Panel *p = &panels[P_CHART];
    unsigned char row, col, q, color;
    unsigned char x, y;
    unsigned char x0 = (unsigned char)(p->x + 2);
    unsigned char y0 = (unsigned char)(p->y + 3);

    clear_panel(P_CHART);

    for (col = 0; col < GAL_DIM; col++)
        scr_put((unsigned char)(x0 + col * 4 + 1), (unsigned char)(p->y + 2),
                (unsigned char)(SC_DIGIT0 + col + 1), COL_LABEL);

    for (row = 0; row < GAL_DIM; row++) {
        scr_put((unsigned char)(p->x + 1), (unsigned char)(y0 + row),
                (unsigned char)(SC_DIGIT0 + row + 1), COL_LABEL);

        for (col = 0; col < GAL_DIM; col++) {
            q = (unsigned char)((row << 3) | col);
            x = (unsigned char)(x0 + col * 4);
            y = (unsigned char)(y0 + row);

            if (!gal_known[q]) {
                scr_put(x, y, SC_DOT, COL_UNKNOWN);
                scr_put((unsigned char)(x + 1), y, SC_DOT, COL_UNKNOWN);
                scr_put((unsigned char)(x + 2), y, SC_DOT, COL_UNKNOWN);
                continue;
            }

            color = COL_VALUE;
            if (gal_enemies[q]) color = EGA_TO_VDC(EGA_CHART_MONGOL);
            else if (gal_base[q] != BASE_NONE) color = COL_BASE;

            scr_put(x, y, (unsigned char)(SC_DIGIT0 + gal_enemies[q]), color);
            scr_put((unsigned char)(x + 1), y,
                    (unsigned char)(SC_DIGIT0 + gal_base[q]), color);
            scr_put((unsigned char)(x + 2), y,
                    (unsigned char)(SC_DIGIT0 + gal_stars[q]), color);
        }
    }

    /* Mark where we are, in the row of the panel below the grid. */
    scr_puts((unsigned char)(p->x + 2), (unsigned char)(p->y + p->h - 2),
             "LEXINGTON IN QUAD", COL_LABEL);
    put_num((unsigned char)(p->x + 20), (unsigned char)(p->y + p->h - 2),
            (uint16_t)(ship.quad_y + 1), 1, COL_VALUE);
    scr_put((unsigned char)(p->x + 21), (unsigned char)(p->y + p->h - 2),
            44 /* ',' */, COL_VALUE);
    put_num((unsigned char)(p->x + 22), (unsigned char)(p->y + p->h - 2),
            (uint16_t)(ship.quad_x + 1), 1, COL_VALUE);
}

/* -------------------------------------------------------------- status */

void ui_draw_status(void) {
    const Panel *p = &panels[P_STATUS];
    unsigned char lx = (unsigned char)(p->x + 2);
    unsigned char vx = (unsigned char)(p->x + 11);
    unsigned char y  = (unsigned char)(p->y + 2);

    clear_panel(P_STATUS);

    /* Nine rows, no blank spacers. The previous layout used spacers and ran
       ENEMIES onto the panel's bottom border -- visible in the first VICE
       capture, where the border line ran straight through the word. The
       mission-deadline row is gone with them: the original's panel shows no
       such figure, and ours was invented.

       Energy is three separate pools, as the original's ENGINEERING REPORT
       shows -- main banks, impulse engines, and shield charge. */
    scr_puts(lx, y, "STARDATE", COL_LABEL);
    put_tenths(vx, y, ship.stardate, COL_VALUE);
    y++;

    scr_puts(lx, y, "QUADRANT", COL_LABEL);
    put_num(vx, y, (uint16_t)(ship.quad_y + 1), 1, COL_VALUE);
    scr_put((unsigned char)(vx + 1), y, 44, COL_VALUE);
    put_num((unsigned char)(vx + 2), y, (uint16_t)(ship.quad_x + 1), 1, COL_VALUE);
    y++;

    scr_puts(lx, y, "SECTOR", COL_LABEL);
    put_num(vx, y, (uint16_t)(ship.sec_y + 1), 1, COL_VALUE);
    scr_put((unsigned char)(vx + 1), y, 44, COL_VALUE);
    put_num((unsigned char)(vx + 2), y, (uint16_t)(ship.sec_x + 1), 1, COL_VALUE);
    y++;

    scr_puts(lx, y, "ENERGY", COL_LABEL);
    put_num(vx, y, ship.energy, 5, COL_VALUE);
    y++;
    scr_puts(lx, y, "IMPULSE", COL_LABEL);
    put_num(vx, y, ship.impulse, 5, COL_VALUE);
    y++;
    /* Green when raised: the charge figure alone does not say whether the
       shields are actually up. */
    scr_puts(lx, y, "SHIELDS", COL_LABEL);
    put_num(vx, y, ship.shields, 5,
            ship.shields_up ? EGA_TO_VDC(EGA_LTGREEN) : COL_VALUE);
    y++;
    scr_puts(lx, y, "TORPS", COL_LABEL);
    put_num(vx, y, (uint16_t)ship.torps, 5, COL_VALUE);
    y++;
    scr_puts(lx, y, "WARP", COL_LABEL);
    put_tenths((unsigned char)(vx - 1), y, (uint16_t)ship.warp, COL_VALUE);
    y++;
    scr_puts(lx, y, "ENEMIES", COL_LABEL);
    put_num(vx, y, ship.enemies_left, 5, EGA_TO_VDC(EGA_LTRED));
}

/* ------------------------------------------------------- systems status */

/* Twelve systems, six rows of two columns. The original prints the full name
   ("EnergyConverter", "Impulse Engine") against a bar, but it does so in a
   graphics mode with an 8-pixel line pitch inside a 14-pixel character cell,
   which is how it fits ten names into six rows' worth of height. A character
   display cannot compress like that, so the names shorten to three letters
   and the freed width goes into the bar.

   Twelve, not the original console's ten: the Transporter and Shuttlecraft
   are in the repair array (DS:235A holds twelve words) and in the original's
   own STATE OF REPAIR dialog, and only the console panel leaves them out.
   Showing them costs nothing here and the game already tracks them. */
#define SYS_COLS      2
#define SYS_ROWS      6
#define SYS_ENTRY_W  11    /* 3 label + 1 space + 7 bar */
#define SYS_BAR_W     7
#define SYS_COL_PITCH 13   /* leaves a two-column gutter between the halves */

/* NOT the solid block. G_BLOCK fills all eight pixel rows of its cell, so six
   bars stacked on consecutive rows fuse into one green slab and the panel
   stops showing six systems -- confirmed on screen in VICE before this was
   changed. Screen code 228 is the reverse of the bottom-line glyph: solid
   across the top seven rows, clear on the eighth. That leaves a one-pixel
   separator between rows, which is the same 7-of-8 pitch the original draws
   its bars at (measured: 7px bars on an 8px pitch, MEASURED.md).

   Read out of VICE's own C128 chargen ROM rather than guessed -- the port has
   already lost time to invented screen codes, and the charset is right there
   on disk to check against. */
#define SYS_BAR_GLYPH 228

/* Order matches core/trek.h's SYS_* indices, which in turn match the order
   the original's console and repair dialog list them in. */
static const char *const sys_abbrev[SYS_COUNT] = {
    "CNV", "SHD", "LIF", "LAS", "TUB", "WRP",
    "IMP", "SRS", "LRS", "CMP", "TRN", "SHT"
};

/* FITTED. Poking the repair array in the original showed a full green bar at
   100, a short yellow one at 70 and a short red one at 40 (MEASURED.md), so
   the green threshold is somewhere in (70,100] and the yellow one in (40,70].
   90 and 50 are chosen because they are the two numbers the game already
   uses for the scanners -- the manual states them and the redraw code at
   0x01C37C compares against exactly those constants. Consistent with every
   reading, but not pinned down by one. */
static unsigned char sys_color(unsigned char pct) {
    if (pct >= 90) return EGA_TO_VDC(EGA_LTGREEN);
    if (pct >= 50) return EGA_TO_VDC(EGA_YELLOW);
    return EGA_TO_VDC(EGA_LTRED);
}

void ui_draw_systems(void) {
    const Panel *p = &panels[P_SYSTEMS];
    unsigned char i, c, r, b, x, y, pct, fill, color;

    clear_panel(P_SYSTEMS);

    for (i = 0; i < SYS_COUNT; i++) {
        /* Column-major: the left half is systems 1-6 reading down, which is
           how the original's single column reads. */
        c = (unsigned char)(i / SYS_ROWS);
        r = (unsigned char)(i % SYS_ROWS);
        if (c >= SYS_COLS) break;

        x = (unsigned char)(p->x + 1 + c * SYS_COL_PITCH);
        y = (unsigned char)(p->y + 1 + r);

        pct = ship.sys[i];
        if (pct > 100) pct = 100;
        color = sys_color(pct);

        scr_puts(x, y, sys_abbrev[i], COL_LABEL);

        /* Round to nearest cell, but never round a system that is still
           partly alive down to an empty bar -- empty has to mean destroyed. */
        fill = (unsigned char)(((uint16_t)pct * SYS_BAR_W + 50) / 100);
        if (fill == 0 && pct != 0) fill = 1;

        /* The unlit part of the bar is drawn, not left blank, so the bar's
           full extent reads and a short bar is visibly short rather than
           just absent. */
        for (b = 0; b < SYS_BAR_W; b++)
            scr_put((unsigned char)(x + 4 + b), y, SYS_BAR_GLYPH,
                    b < fill ? color : COL_GRID);
    }
}

/* -------------------------------------------------------- laser gauges */

/* Two gauges, as the original: efficiency over temperature. The original
   prints a numbered scale above each bar -- "0  50  100" and
   "0  1000 1500" -- which needs five lines of a graphics-mode panel and is
   not going to fit in two character rows. The scale becomes a number printed
   beside the bar instead, which says more per cell.

   Widths add to the eighteen cells of the panel interior:
   four label, one space, eight bar, one space, four value. */
#define LAS_BAR_W    8
#define LAS_VAL_W    4

/* The efficiency gauge is the one that matters: it is a direct multiplier on
   damage, and the same colours as SYSTEMS STATUS keep that readable. */

/* PROVISIONAL. Only 1500 has evidence behind it -- see LASER_HEAT_MAX. The
   original prints a tick at 1000 as well, which suggests it means something,
   but nothing has been measured there, so the amber band is a guess and is
   marked as one. Red means the ancestor would be rolling for a burn. */
static unsigned char heat_color(uint16_t heat) {
    if (heat > LASER_HEAT_MAX)  return EGA_TO_VDC(EGA_LTRED);
    if (heat >= 1000)           return EGA_TO_VDC(EGA_YELLOW);
    return EGA_TO_VDC(EGA_LTGREEN);
}

/* Rounds to nearest cell, and never lets a nonzero value draw an empty bar:
   empty has to mean nothing, not merely a little.

   The saturating case is handled by clamping FIRST rather than by widening
   the arithmetic. Heat saturates at 65535 and 65535 * 8 does not fit in the
   16 bits cc65 gives an int; after the early return, value < full <= 1500, so
   value * 8 tops out at 12000 and there is nothing to overflow. */
static unsigned char gauge_fill(uint16_t value, uint16_t full) {
    if (value == 0) return 0;
    if (value >= full) return LAS_BAR_W;
    {
        uint16_t n = (uint16_t)((value * LAS_BAR_W + full / 2) / full);
        return (unsigned char)(n == 0 ? 1 : n);
    }
}

/* put_num pads a short number but does NOT truncate a long one -- it writes
   every digit it has. Heat saturates at 65535, which is five digits in a
   four-cell field, and the fifth lands on the panel's right border. A real
   gauge reading off its scale shows that rather than a wrong number, so the
   field fills with stars. Caught by test_panels.c, not by looking. */
static void put_gauge_num(unsigned char x, unsigned char y, uint16_t v,
                          unsigned char color) {
    unsigned char i;
    if (v > 9999) {
        for (i = 0; i < LAS_VAL_W; i++)
            scr_put((unsigned char)(x + i), y, SC_STAR, color);
        return;
    }
    put_num(x, y, v, LAS_VAL_W, color);
}

static void gauge_row(unsigned char x, unsigned char y, const char *label,
                      uint16_t value, uint16_t full, unsigned char color) {
    unsigned char fill = gauge_fill(value, full);
    unsigned char b;

    scr_puts(x, y, label, COL_LABEL);
    for (b = 0; b < LAS_BAR_W; b++)
        scr_put((unsigned char)(x + 5 + b), y, SYS_BAR_GLYPH,
                b < fill ? color : COL_GRID);
    put_gauge_num((unsigned char)(x + 5 + LAS_BAR_W + 1), y, value, color);
}

void ui_draw_lasers(void) {
    const Panel *p = &panels[P_LASERS];
    unsigned char x = (unsigned char)(p->x + 1);
    unsigned char y = (unsigned char)(p->y + 1);

    clear_panel(P_LASERS);
    gauge_row(x, y, "EFF", (uint16_t)ship.laser_eff, 100,
              sys_color(ship.laser_eff));
    gauge_row(x, (unsigned char)(y + 1), "TEMP", ship.laser_heat,
              LASER_HEAT_MAX, heat_color(ship.laser_heat));
}

/* ------------------------------------------------------------ messages */

#define MSG_LINES 4
#define MSG_WIDTH 30

/* 16, not 12: "NAVIGATION: " is twelve characters and was losing its
   trailing space, so messages ran straight into the department name. */
static char msg_dept[MSG_LINES][16];
static char msg_text[MSG_LINES][MSG_WIDTH + 1];
static unsigned char msg_count = 0;

void ui_clear_messages(void) {
    msg_count = 0;
    clear_panel(P_COMMS);
}

/* Department and text together must fit the panel's interior. The VDC
   auto-increments its update address, so a line that runs past column 79
   does not clip -- it wraps onto the next row and overwrites whatever panel
   is there. Clamp rather than trusting callers to keep messages short. */
static void msg_redraw(void) {
    const Panel *p = &panels[P_COMMS];
    const unsigned char avail = (unsigned char)(p->w - 4);
    char tbuf[MSG_WIDTH + 1];
    unsigned char i, y, dlen, rem;

    clear_panel(P_COMMS);

    for (i = 0; i < msg_count; i++) {
        y = (unsigned char)(p->y + 2 + i);

        dlen = (unsigned char)strlen(msg_dept[i]);
        if (dlen > avail) dlen = avail;
        rem = (unsigned char)(avail - dlen);
        if (rem > MSG_WIDTH) rem = MSG_WIDTH;

        scr_puts((unsigned char)(p->x + 2), y, msg_dept[i], COL_DEPT);

        strncpy(tbuf, msg_text[i], rem);
        tbuf[rem] = '\0';
        scr_puts((unsigned char)(p->x + 2 + dlen), y, tbuf, COL_MSG);
    }
}

void ui_message(const char *dept, const char *text) {
    unsigned char i;

    if (msg_count == MSG_LINES) {
        /* Oldest scrolls off the top, as the original's four-line console
           does (manual l.312-314). */
        for (i = 1; i < MSG_LINES; i++) {
            strcpy(msg_dept[i - 1], msg_dept[i]);
            strcpy(msg_text[i - 1], msg_text[i]);
        }
        msg_count--;
    }

    strncpy(msg_dept[msg_count], dept, 15);
    msg_dept[msg_count][15] = '\0';
    strncpy(msg_text[msg_count], text, MSG_WIDTH);
    msg_text[msg_count][MSG_WIDTH] = '\0';
    msg_count++;

    msg_redraw();
}

/* ------------------------------------------------------------- command */

void ui_read_command(char *buf, uint8_t max) {
    const Panel *p = &panels[P_COMMAND];
    unsigned char x0 = (unsigned char)(p->x + 2);
    unsigned char y  = (unsigned char)(p->y + 2);
    unsigned char n = 0;
    char c;

    clear_panel(P_COMMAND);
    scr_puts(x0, y, "CMD:", COL_LABEL);

    for (;;) {
        /* Block cursor drawn by hand -- the KERNAL's own cursor belongs to
           the 40-column screen, which this port does not use. */
        scr_put((unsigned char)(x0 + 5 + n), y, 32 + 128, COL_VALUE);

        c = kb_waitkey();

        if (c == KB_RETURN) {
            scr_put((unsigned char)(x0 + 5 + n), y, SC_SPACE, COL_VALUE);
            break;
        }
        if (c == KB_DELETE) {
            if (n) {
                scr_put((unsigned char)(x0 + 5 + n), y, SC_SPACE, COL_VALUE);
                n--;
                scr_put((unsigned char)(x0 + 5 + n), y, SC_SPACE, COL_VALUE);
            }
            continue;
        }
        if (n + 1 >= max) continue;
        if (c < 32) continue;

        buf[n] = c;
        /* Echo through the same conversion scr_puts uses, one cell at a
           time. kb_waitkey returns ASCII, which is what that converter's
           64-95 branch expects: 'M' (77) becomes screen code 13. */
        {
            char one[2];
            one[0] = c;
            one[1] = '\0';
            scr_puts((unsigned char)(x0 + 5 + n), y, one, COL_VALUE);
        }
        n++;
    }

    buf[n] = '\0';
}

/* ----------------------------------------------------------------- all */

/* ------------------------------------------------------------- dialogues */

/* Centred over the viewer and communications panels, which is roughly where
   the original puts its WEAPONS CONTROL box. Sized to leave the scan, the
   status readout and the chart visible above it. */
#define DLG_X   14
#define DLG_Y    9
#define DLG_W   52
#define DLG_H   13

static unsigned char dlg_row;      /* next free line inside the box */

static void dlg_frame(const char *title) {
    unsigned char x, y;

    for (y = 0; y < DLG_H; y++) {
        for (x = 0; x < DLG_W; x++) {
            unsigned char c = SC_SPACE;
            if (y == 0)             c = (x == 0) ? G_TL : (x == DLG_W - 1) ? G_TR : G_HLINE;
            else if (y == DLG_H - 1) c = (x == 0) ? G_BL : (x == DLG_W - 1) ? G_BR : G_HLINE;
            else if (x == 0 || x == DLG_W - 1) c = G_VLINE;
            scr_put((unsigned char)(DLG_X + x), (unsigned char)(DLG_Y + y), c, COL_LABEL);
        }
    }
    scr_puts((unsigned char)(DLG_X + 2), DLG_Y, title, COL_VALUE);
    dlg_row = 2;
}

void ui_dialog_open(const char *title) {
    dlg_frame(title);
}

/* Scrolls by redrawing the frame when it fills, rather than moving cells
   about: the box is small and the VDC write is cheap enough. */
static void dlg_room(void) {
    if (dlg_row < DLG_H - 2) return;
    dlg_frame("");
}

void ui_dialog_line(const char *text) {
    dlg_room();
    scr_puts((unsigned char)(DLG_X + 2), (unsigned char)(DLG_Y + dlg_row),
             text, COL_MSG);
    dlg_row++;
}

void ui_dialog_ask(const char *prompt, char *buf, uint8_t max) {
    unsigned char x0, y, n = 0;
    char c;

    dlg_room();
    y  = (unsigned char)(DLG_Y + dlg_row);
    scr_puts((unsigned char)(DLG_X + 2), y, prompt, COL_DEPT);
    x0 = (unsigned char)(DLG_X + 2 + strlen(prompt) + 1);
    dlg_row++;

    for (;;) {
        scr_put((unsigned char)(x0 + n), y, 32 + 128, COL_VALUE);
        c = kb_waitkey();
        if (c == KB_RETURN) { scr_put((unsigned char)(x0 + n), y, SC_SPACE, COL_VALUE); break; }
        if (c == KB_DELETE) {
            if (n) {
                scr_put((unsigned char)(x0 + n), y, SC_SPACE, COL_VALUE);
                n--;
                scr_put((unsigned char)(x0 + n), y, SC_SPACE, COL_VALUE);
            }
            continue;
        }
        if (n + 1 >= max || c < 32) continue;
        buf[n] = c;
        { char one[2]; one[0] = c; one[1] = '\0';
          scr_puts((unsigned char)(x0 + n), y, one, COL_VALUE); }
        n++;
    }
    buf[n] = '\0';
}

void ui_dialog_close(void) {
    unsigned char x, y;

    dlg_room();
    scr_puts((unsigned char)(DLG_X + 2), (unsigned char)(DLG_Y + dlg_row),
             "HIT RETURN TO CONTINUE", COL_DEPT);
    for (;;) if (kb_waitkey() == KB_RETURN) break;

    /* Blank the box before repainting. ui_draw_all() redraws the frame and
       the four panels that carry live state, but nothing clears the interior
       of the panels this port leaves empty -- so dialog text sitting inside
       LASERS or MAIN VIEWER survived the redraw and stayed on screen. */
    for (y = 0; y < DLG_H; y++)
        for (x = 0; x < DLG_W; x++)
            scr_put((unsigned char)(DLG_X + x), (unsigned char)(DLG_Y + y),
                    SC_SPACE, COL_LABEL);

    ui_draw_all();
}

void ui_draw_all(void) {
    draw_console();
    ui_draw_scan();
    ui_draw_chart();
    ui_draw_status();
    ui_draw_systems();
    ui_draw_lasers();
    msg_redraw();
}
