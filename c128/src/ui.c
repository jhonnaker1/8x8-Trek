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

    scr_puts(lx, y, "STARDATE", COL_LABEL);
    put_tenths(vx, y, ship.stardate, COL_VALUE);
    y++;
    scr_puts(lx, y, "ENDS", COL_LABEL);
    put_tenths(vx, y, ship.stardate_end, COL_VALUE);
    y += 2;

    scr_puts(lx, y, "QUADRANT", COL_LABEL);
    put_num(vx, y, (uint16_t)(ship.quad_y + 1), 1, COL_VALUE);
    scr_put((unsigned char)(vx + 1), y, 44, COL_VALUE);
    put_num((unsigned char)(vx + 2), y, (uint16_t)(ship.quad_x + 1), 1, COL_VALUE);
    y++;

    scr_puts(lx, y, "SECTOR", COL_LABEL);
    put_num(vx, y, (uint16_t)(ship.sec_y + 1), 1, COL_VALUE);
    scr_put((unsigned char)(vx + 1), y, 44, COL_VALUE);
    put_num((unsigned char)(vx + 2), y, (uint16_t)(ship.sec_x + 1), 1, COL_VALUE);
    y += 2;

    scr_puts(lx, y, "ENERGY", COL_LABEL);
    put_num(vx, y, ship.energy, 5, COL_VALUE);
    y++;
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

void ui_draw_all(void) {
    draw_console();
    ui_draw_scan();
    ui_draw_chart();
    ui_draw_status();
    msg_redraw();
}
