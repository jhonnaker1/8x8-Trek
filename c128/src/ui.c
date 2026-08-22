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
        case SEC_SHIP:
                             /* Yellow with the shields raised, white with
                                them down. Not decoration -- the manual makes
                                it the way the player reads shield state at a
                                glance: "the image of your ship on the short
                                range scanner will change to yellow" (l.580),
                                and SHDN "will return to white" (l.586). */
                             *color = ship.shields_up
                                      ? EGA_TO_VDC(EGA_YELLOW) : COL_SHIP;
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
    unsigned char y0 = (unsigned char)(p->y + 2);

    clear_panel(P_SCAN);

    /* Column headers and row labels are 1-based, as the original presents
       them; the core is 0-based throughout. */
    for (col = 0; col < QUAD_DIM; col++)
        scr_put((unsigned char)(x0 + col * 2), (unsigned char)(p->y + 1),
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
    unsigned char y0 = (unsigned char)(p->y + 2);

    clear_panel(P_CHART);

    for (col = 0; col < GAL_DIM; col++)
        scr_put((unsigned char)(x0 + col * 4 + 1), (unsigned char)(p->y + 1),
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

    /* "LEXINGTON IN QUAD y,x" goes in the BOTTOM BORDER, the way a title goes
       in the top one. The panel's nine interior rows are all spoken for -- one
       header and eight galaxy rows -- and the original prints this line right
       against its own bottom edge too. */
    {
        unsigned char by = (unsigned char)(p->y + p->h - 1);
        unsigned char bx = (unsigned char)(p->x + 2);
        /* Trailing space matters: this line is written INTO the bottom
           border, so a gap left between words shows the border rule through
           it and reads as a stray dash. */
        scr_puts(bx, by, "LEXINGTON IN QUAD ", COL_LABEL);
        put_num((unsigned char)(bx + 18), by, (uint16_t)(ship.quad_y + 1), 1, COL_VALUE);
        scr_put((unsigned char)(bx + 19), by, 44 /* ',' */, COL_VALUE);
        put_num((unsigned char)(bx + 20), by, (uint16_t)(ship.quad_x + 1), 1, COL_VALUE);
    }
}

/* -------------------------------------------------------------- status */

void ui_draw_status(void) {
    const Panel *p = &panels[P_STATUS];
    unsigned char lx = (unsigned char)(p->x + 2);
    unsigned char vx = (unsigned char)(p->x + 11);
    unsigned char y  = (unsigned char)(p->y + 2);

    clear_panel(P_STATUS);

    /* Seven rows. QUADRANT and SECTOR used to be here and are now on the
       COMMAND panel, which is where the original puts them -- it prints
       "Quad 8,5  Sec 2,5" under the command prompt. That was not a saving
       made up to fit: it is what the capture shows.

       Energy is three separate pools, as the original's ENGINEERING REPORT
       shows -- main banks, impulse engines, and shield charge. */
    scr_puts(lx, y, "STARDATE", COL_LABEL);
    put_tenths(vx, y, ship.stardate, COL_VALUE);
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

/* ---------------------------------------------------------------- badge */

/* The Department of Space crest. The original draws a blue ellipse outlined
   in cyan with one large white star and three small ones, under the ship name
   and hull number, over "Dept. of Space" -- six lines, which is exactly what
   the measured panel gives.

   The disc is built from half-block glyphs read out of the C128 chargen ROM:
   228 is a full block less its bottom pixel row, 98 fills the bottom half of a
   cell and 226 the top half. Rounding the disc that way costs nothing and is
   the difference between a crest and a rectangle. */
#define BADGE_DISC_TOP     98    /* lower half filled -- rounds the top edge */
#define BADGE_DISC_BOTTOM 226    /* upper half filled -- rounds the bottom   */
#define BADGE_DISC_BODY   160    /* solid                                    */

void ui_draw_badge(void) {
    const Panel *p = &panels[P_BADGE];
    unsigned char y = (unsigned char)(p->y + 1);
    unsigned char cx = (unsigned char)(p->x + p->w / 2);
    unsigned char disc = EGA_TO_VDC(EGA_BLUE);
    unsigned char i;

    clear_panel(P_BADGE);

    scr_puts((unsigned char)(p->x + 2), y, "U.S.S. LEXINGTON", COL_VALUE);
    y++;
    scr_puts((unsigned char)(cx - 3), y, "RCB-92", COL_VALUE);
    y++;

    /* All one colour. Outlining the rounded rows in cyan the way the original
       outlines its ellipse turned the crest into a cyan sandwich on screen:
       at this size the rim is the whole of the top and bottom rows, so it
       stops reading as an edge and starts reading as a stripe. */
    for (i = 0; i < 5; i++)
        scr_put((unsigned char)(cx - 2 + i), y, BADGE_DISC_TOP, disc);
    y++;
    for (i = 0; i < 7; i++)
        scr_put((unsigned char)(cx - 3 + i), y, BADGE_DISC_BODY, disc);
    /* The large star. A character cell has no background of its own on the
       VDC, so this reads as a white star in the disc rather than on it --
       which is as close as a text display gets. */
    scr_put((unsigned char)(cx + 1), y, SC_STAR, COL_VALUE);
    scr_put((unsigned char)(cx - 2), y, SC_DOT, COL_VALUE);
    y++;
    for (i = 0; i < 5; i++)
        scr_put((unsigned char)(cx - 2 + i), y, BADGE_DISC_BOTTOM, disc);
    y++;

    scr_puts((unsigned char)(p->x + 3), y, "DEPT. OF SPACE", COL_LABEL);
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
#define SYS_ENTRY_W   9    /* 3 label + 1 space + 5 bar */
#define SYS_BAR_W     5
#define SYS_COL_PITCH 9    /* two columns fill the 18-cell interior exactly */

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

/* MEASURED 2026-08-21 off the original's STATE OF REPAIR screen, which colours
   each system by the same rule the bars use: 100% green; 95, 65 and 55 all
   yellow; 40, 10 and 0 all red. So green means undamaged and nothing else, and
   the red boundary sits between 40 and 55. An earlier version guessed 90 and
   50 from three readings; the 95 seen yellow is what moved the green line. */
static unsigned char sys_color(unsigned char pct) {
    if (pct >= 100) return EGA_TO_VDC(EGA_LTGREEN);
    if (pct >= 50)  return EGA_TO_VDC(EGA_YELLOW);
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

/* ---------------------------------------------------------- main viewer */

/* What the original puts here, read off a capture: against a starfield, the
   enemy's class as a caption in the top left, a line drawing of the ship, and
   two readouts in the bottom left -- bearing in degrees after a slashed zero,
   distance in sectors after a triangle.

   Five character rows cannot hold a line drawing, so the ship becomes a small
   PETSCII silhouette and the caption, bearing and distance carry the rest.
   The nearest enemy is the subject, which is what the original tracks: its
   scanner reports follow whichever ship is closing. */
#define VIEW_EMPTY_MSG "NO CONTACT"

static const char *enemy_class(unsigned char c) {
    switch (c) {
        case SEC_COMMAND:    return "MONGOL COMMANDER";
        case SEC_SCOUT:      return "MONGOL SCOUT";
        case SEC_SUPPLY:     return "SUPPLY STATION";
        default:             return "MONGOL BATTLESHIP";
    }
}

/* Nearest enemy, or QUAD_CELLS if the quadrant is clear. */
static unsigned char nearest_enemy(uint16_t *dist) {
    unsigned char cell, best = QUAD_CELLS;
    uint16_t d, bd = 0xFFFF;

    for (cell = 0; cell < QUAD_CELLS; cell++) {
        unsigned char y, x, dy, dx;
        if (!SEC_IS_ENEMY(sector[cell])) continue;
        y = (unsigned char)(cell >> 3);
        x = (unsigned char)(cell & 7);
        dy = (unsigned char)(y > ship.sec_y ? y - ship.sec_y : ship.sec_y - y);
        dx = (unsigned char)(x > ship.sec_x ? x - ship.sec_x : ship.sec_x - x);
        d = trek_dist(dy, dx);
        if (d < bd) { bd = d; best = cell; }
    }
    if (dist) *dist = (best == QUAD_CELLS) ? 0 : bd;
    return best;
}

void ui_draw_viewer(void) {
    const Panel *p = &panels[P_VIEWER];
    unsigned char x = (unsigned char)(p->x + 1);
    unsigned char y = (unsigned char)(p->y + 1);
    unsigned char cell, color;
    uint16_t d;

    clear_panel(P_VIEWER);

    cell = nearest_enemy(&d);
    if (cell == QUAD_CELLS) {
        scr_puts((unsigned char)(x + 3), (unsigned char)(y + 2),
                 VIEW_EMPTY_MSG, COL_GRID);
        return;
    }

    /* The class colours are game rules, not decoration -- the original makes
       enemy type readable by hue and core/ega.h carries the mapping. */
    (void)cell_glyph(sector[cell], &color);
    scr_puts(x, y, enemy_class(sector[cell]), color);

    /* A silhouette, not a drawing: saucer, hull, nacelle. */
    scr_put((unsigned char)(x + 6), (unsigned char)(y + 2), 81 /* filled disc */, color);
    scr_hline((unsigned char)(x + 7), (unsigned char)(y + 2), 4, G_HLINE, color);
    scr_put((unsigned char)(x + 11), (unsigned char)(y + 2), 160, color);

    /* Bearing and range, in the corner the original uses. The slashed zero
       and triangle it prints have no PETSCII equivalent, so these are
       labelled rather than glyphed. */
    scr_puts(x, (unsigned char)(y + 3), "BRG", COL_LABEL);
    put_num((unsigned char)(x + 4), (unsigned char)(y + 3),
            trek_bearing((unsigned char)(cell >> 3), (unsigned char)(cell & 7)),
            3, COL_VALUE);

    scr_puts(x, (unsigned char)(y + 4), "RNG", COL_LABEL);
    /* trek_dist is 8.8 fixed point; print it as w.t by scaling the fraction
       to tenths. Staged: the fraction is at most 255, so 255*10 is safe. */
    put_num((unsigned char)(x + 4), (unsigned char)(y + 4),
            (uint16_t)(d >> 8), 2, COL_VALUE);
    scr_put((unsigned char)(x + 6), (unsigned char)(y + 4), SC_DOT, COL_VALUE);
    put_num((unsigned char)(x + 7), (unsigned char)(y + 4),
            (uint16_t)(((d & 0xFF) * 10) >> 8), 1, COL_VALUE);
}

/* ------------------------------------------------------------ messages */

/* One box per message, stacked oldest at the top, exactly as the original
   does it. There is no COMMUNICATIONS panel and no DAMAGE REPORT panel: a
   640x350 capture shows four separately bordered boxes sharing one region,
   with damage reports and communications interleaved in time order and each
   completed box stamped with its stardate in the right end of its top border.
   The department names that looked like panel titles are message prefixes.

   Three rows per box -- border, text, border -- except the last, which takes
   the slack. That is the original's shape too: its first three boxes are 43
   pixels tall and the bottom one 62, because the bottom box is the turn in
   progress and grows as the turn reports itself. The original also wraps each
   message to three lines; ours holds one, which is what fourteen rows and a
   38-cell interior will take. */
#define MSG_SLOTS   4
#define MSG_BOX_H   3
#define MSG_WIDTH  36

static unsigned char msg_top(unsigned char slot) {
    return (unsigned char)(MSG_Y + slot * MSG_BOX_H);
}

static unsigned char msg_height(unsigned char slot) {
    if (slot + 1 < MSG_SLOTS) return MSG_BOX_H;
    return (unsigned char)(MSG_H - (MSG_SLOTS - 1) * MSG_BOX_H);
}

static char     msg_dept[MSG_SLOTS][16];
static char     msg_text[MSG_SLOTS][MSG_WIDTH + 1];
static uint16_t msg_date[MSG_SLOTS];
static unsigned char msg_count = 0;

/* Department colours. Yellow and orange are measured -- they are the border
   and text colours of the COMMUNICATIONS and DAMAGE REPORT boxes in the
   capture. Everything else is UNMEASURED: no capture has been taken with a
   HELM or SCIENCE message on screen, so those keep the green this port has
   been using rather than a guess dressed up as a finding. */
static unsigned char dept_color(const char *dept) {
    if (dept[0] == 'D')                    /* DAMAGE */
        return EGA_TO_VDC(EGA_BROWN);
    if (dept[0] == 'C' && dept[1] == 'O' && dept[2] == 'M' && dept[3] == 'M')
        return EGA_TO_VDC(EGA_YELLOW);     /* COMMUNICATIONS, not COMPUTER */
    return COL_MSG;
}

static void msg_clear_region(void) {
    scr_fill_rect(MSG_X, MSG_Y, MSG_W, MSG_H, SC_SPACE, COL_LABEL);
}

void ui_clear_messages(void) {
    msg_count = 0;
    msg_clear_region();
}

/* Draws one box. The stardate goes into the top border rather than costing a
   text row, which is where the original puts it too. */
static void msg_box(unsigned char slot) {
    unsigned char y = msg_top(slot);
    unsigned char right = (unsigned char)(MSG_X + MSG_W - 1);
    unsigned char bottom = (unsigned char)(y + msg_height(slot) - 1);
    unsigned char color = dept_color(msg_dept[slot]);
    unsigned char dlen, rem;
    char tbuf[MSG_WIDTH + 1];

    scr_hline((unsigned char)(MSG_X + 1), y, (unsigned char)(MSG_W - 2),
              G_HLINE, color);
    scr_hline((unsigned char)(MSG_X + 1), bottom, (unsigned char)(MSG_W - 2),
              G_HLINE, color);
    scr_put(MSG_X, y, G_TL, color);
    scr_put(right, y, G_TR, color);
    scr_put(MSG_X, bottom, G_BL, color);
    scr_put(right, bottom, G_BR, color);
    scr_vline(MSG_X, (unsigned char)(y + 1),
              (unsigned char)(msg_height(slot) - 2), G_VLINE, color);
    scr_vline(right, (unsigned char)(y + 1),
              (unsigned char)(msg_height(slot) - 2), G_VLINE, color);

    put_tenths((unsigned char)(right - 7), y, msg_date[slot], color);

    /* The VDC auto-increments its update address, so a line running past the
       right edge does not clip -- it wraps onto the next row and lands in
       whatever is there. Clamp here rather than trusting callers. */
    dlen = (unsigned char)strlen(msg_dept[slot]);
    if (dlen > MSG_W - 2) dlen = (unsigned char)(MSG_W - 2);
    rem = (unsigned char)(MSG_W - 2 - dlen);
    if (rem > MSG_WIDTH) rem = MSG_WIDTH;

    scr_puts((unsigned char)(MSG_X + 1), (unsigned char)(y + 1),
             msg_dept[slot], COL_DEPT);
    strncpy(tbuf, msg_text[slot], rem);
    tbuf[rem] = '\0';
    scr_puts((unsigned char)(MSG_X + 1 + dlen), (unsigned char)(y + 1),
             tbuf, color);
}

static void msg_redraw(void) {
    unsigned char i;
    msg_clear_region();
    for (i = 0; i < msg_count; i++) msg_box(i);
}

void ui_message(const char *dept, const char *text) {
    unsigned char i;

    if (msg_count == MSG_SLOTS) {
        /* Oldest scrolls off the top, as the original's stack does. */
        for (i = 1; i < MSG_SLOTS; i++) {
            strcpy(msg_dept[i - 1], msg_dept[i]);
            strcpy(msg_text[i - 1], msg_text[i]);
            msg_date[i - 1] = msg_date[i];
        }
        msg_count--;
    }

    strncpy(msg_dept[msg_count], dept, 15);
    msg_dept[msg_count][15] = '\0';
    strncpy(msg_text[msg_count], text, MSG_WIDTH);
    msg_text[msg_count][MSG_WIDTH] = '\0';
    msg_date[msg_count] = ship.stardate;
    msg_count++;

    msg_redraw();
}

/* ------------------------------------------------------------- command */

/* "Quad 8,5  Sec 2,5" under the prompt, which is where the original keeps
   it -- these two were on the STATUS panel here until a capture showed
   otherwise. Drawn separately from ui_read_command so that reading a command
   does not wipe it. */
void ui_draw_position(void) {
    const Panel *p = &panels[P_COMMAND];
    unsigned char x = (unsigned char)(p->x + 2);
    unsigned char y = (unsigned char)(p->y + 2);

    scr_hline(x, y, (unsigned char)(p->w - 3), SC_SPACE, COL_LABEL);

    scr_puts(x, y, "QUAD", COL_LABEL);
    put_num((unsigned char)(x + 5), y, (uint16_t)(ship.quad_y + 1), 1, COL_VALUE);
    scr_put((unsigned char)(x + 6), y, 44, COL_VALUE);
    put_num((unsigned char)(x + 7), y, (uint16_t)(ship.quad_x + 1), 1, COL_VALUE);

    scr_puts((unsigned char)(x + 10), y, "SEC", COL_LABEL);
    put_num((unsigned char)(x + 14), y, (uint16_t)(ship.sec_y + 1), 1, COL_VALUE);
    scr_put((unsigned char)(x + 15), y, 44, COL_VALUE);
    put_num((unsigned char)(x + 16), y, (uint16_t)(ship.sec_x + 1), 1, COL_VALUE);
}

void ui_read_command(char *buf, uint8_t max) {
    const Panel *p = &panels[P_COMMAND];
    unsigned char x0 = (unsigned char)(p->x + 2);
    unsigned char y  = (unsigned char)(p->y + 1);
    unsigned char n = 0;
    char c;

    /* Clears its own row only. The row below carries the position readout. */
    scr_hline(x0, y, (unsigned char)(p->w - 3), SC_SPACE, COL_LABEL);
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

/* Reads a line at (x0,y), echoing as it goes and handling backspace. Shared by
   the modal dialog and the setup screen so both behave identically. */
static void read_field(unsigned char x0, unsigned char y, char *buf, uint8_t max) {
    unsigned char n = 0;
    char c;

    for (;;) {
        scr_put((unsigned char)(x0 + n), y, 32 + 128, COL_VALUE);   /* cursor */
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

void ui_dialog_ask(const char *prompt, char *buf, uint8_t max) {
    unsigned char y;

    dlg_room();
    y = (unsigned char)(DLG_Y + dlg_row);
    scr_puts((unsigned char)(DLG_X + 2), y, prompt, COL_DEPT);
    dlg_row++;
    read_field((unsigned char)(DLG_X + 2 + strlen(prompt) + 1), y, buf, max);
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
    ui_draw_badge();
    ui_draw_viewer();
    ui_draw_position();
    msg_redraw();
}

/* ------------------------------------------------ state of repair report */

#define SC_PCT   37            /* screen codes 32..63 are ASCII, so '%' is 37 */

/* Full names, as the original's report spells them. The SYSTEMS STATUS panel
   uses three-letter abbreviations because it has four columns to fit; this has
   the width for the real thing. Same order as SYS_*. */
static const char *const sys_name[SYS_COUNT] = {
    "ENERGYCONVERTER", "SHIELDS",       "LIFE SUPPORT",   "LASERS",
    "ENTORP TUBES",    "WARP ENGINES",  "IMPULSE ENGINE", "S.R. SCANNER",
    "L.R. SCANNER",    "COMPUTER",      "TRANSPORTER",    "SHUTTLECRAFT"
};

/* Stardates, in tenths, to mend `pts` points at `rate` points per stardate,
   rounded to the nearest tenth. Both rates are MEASURED -- see trek.h -- and
   these figures are computed from OUR constants rather than transcribed off
   the original's screen, so what the report promises is what the repair code
   will actually do. */
static uint16_t repair_tenths(unsigned char pts, unsigned char rate) {
    return (uint16_t)(((uint16_t)pts * 10u + rate / 2u) / rate);
}

/* n.n in three cells. The other put_tenths() pads to six for stardates. */
static void put_time(unsigned char x, unsigned char y, uint16_t tenths,
                     unsigned char color) {
    if (tenths > 99) { scr_puts(x, y, "***", color); return; }
    scr_put(x,                      y, (unsigned char)(SC_DIGIT0 + tenths / 10), color);
    scr_put((unsigned char)(x + 1), y, SC_DOT, color);
    scr_put((unsigned char)(x + 2), y, (unsigned char)(SC_DIGIT0 + tenths % 10), color);
}

/* A bordered, cleared box. The dialog draws its own because it also tracks a
   cursor row; the report and the title screen share this one.
 *
 * Drawn with scr_hline/scr_vline rather than a nested loop with a conditional
 * per cell, and that is not a style choice. The loop form WORKED NATIVELY and
 * passed its unit test while the cl65 build silently dropped the bottom edge
 * on screen -- both boxes lost their bottom border and nothing complained.
 * draw_panel() in layout.c has always used this shape and has always rendered
 * correctly on hardware, so the fix was to match it. The exact cause was not
 * isolated to -O versus codegen generally; what matters is that the native
 * test cannot be trusted alone here, which is the same lesson the PETSCII key
 * table taught and the reason `make verify` exists. */
static void box(unsigned char bx, unsigned char by, unsigned char w,
                unsigned char h, unsigned char color) {
    unsigned char right  = (unsigned char)(bx + w - 1);
    unsigned char bottom = (unsigned char)(by + h - 1);

    scr_fill_rect((unsigned char)(bx + 1), (unsigned char)(by + 1),
                  (unsigned char)(w - 2), (unsigned char)(h - 2), SC_SPACE, color);
    scr_hline((unsigned char)(bx + 1), by,     (unsigned char)(w - 2), G_HLINE, color);
    scr_hline((unsigned char)(bx + 1), bottom, (unsigned char)(w - 2), G_HLINE, color);
    scr_vline(bx,    (unsigned char)(by + 1), (unsigned char)(h - 2), G_VLINE, color);
    scr_vline(right, (unsigned char)(by + 1), (unsigned char)(h - 2), G_VLINE, color);
    scr_put(bx,    by,     G_TL, color);
    scr_put(right, by,     G_TR, color);
    scr_put(bx,    bottom, G_BL, color);
    scr_put(right, bottom, G_BR, color);
}

void ui_repair_report(void) {
    unsigned char y, i, pct, color;

    box(REP_X, REP_Y, REP_W, REP_H, COL_LABEL);
    scr_puts((unsigned char)(REP_X + 14), REP_Y, "STATE OF REPAIR", COL_VALUE);

    scr_puts((unsigned char)(REP_X + 2),  (unsigned char)(REP_Y + 1), "SYSTEM", COL_LABEL);
    scr_puts((unsigned char)(REP_X + 26), (unsigned char)(REP_Y + 1), "REPAIR TIME", COL_LABEL);
    scr_puts((unsigned char)(REP_X + 24), (unsigned char)(REP_Y + 2), "DOCKED UNDOCKED", COL_LABEL);

    for (i = 0; i < SYS_COUNT; i++) {
        y   = (unsigned char)(REP_Y + 4 + i);
        pct = ship.sys[i];
        if (pct > 100) pct = 100;
        color = sys_color(pct);

        scr_puts((unsigned char)(REP_X + 2), y, sys_name[i], color);
        put_num((unsigned char)(REP_X + 18), y, pct, 3, color);
        scr_put((unsigned char)(REP_X + 21), y, SC_PCT, color);

        /* Left blank against an undamaged system, as the original leaves it:
           a column of 0.0 down twelve rows reads as noise. */
        if (pct >= 100) continue;
        put_time((unsigned char)(REP_X + 25), y,
                 repair_tenths((unsigned char)(100 - pct),
                               REPAIR_PER_STARDATE_DOCKED), color);
        put_time((unsigned char)(REP_X + 34), y,
                 repair_tenths((unsigned char)(100 - pct),
                               REPAIR_PER_STARDATE), color);
    }

    scr_puts((unsigned char)(REP_X + 11), (unsigned char)(REP_Y + REP_H - 2),
             "HIT RETURN TO CONTINUE", COL_DEPT);
}

/* ---------------------------------------------------------- setup screen */

/* The prompts, in the original's order and its own words -- captured from a
   live run on 2026-08-21 and archived to reference/shots. The name is what the
   hall of fame records; the password is what S)elf will be gated on. */

static uint8_t ask_yes(unsigned char y, const char *prompt) {
    char buf[4];
    scr_puts(2, y, prompt, COL_LABEL);
    read_field((unsigned char)(2 + strlen(prompt) + 1), y, buf, sizeof buf);
    return (uint8_t)(buf[0] == 'Y' || buf[0] == 'y');
}

uint16_t setup_seed(uint16_t entropy, uint8_t level) {
    uint16_t v = (uint16_t)(entropy ^ (uint16_t)(level * 2749u));
    return v ? v : 1u;
}

void ui_setup(Setup *s) {
    char buf[8];
    unsigned char y;

    scr_clear();
    scr_puts(31, 1, "U.S.S. LEXINGTON", COL_VALUE);
    scr_puts(36, 2, "RCB-92",           COL_VALUE);
    scr_puts(2,  5, "WELCOME ABOARD CAPTAIN!", COL_MSG);

    s->briefing = ask_yes(7, "WILL YOU REQUIRE A BRIEFING (Y/N)?");

    if (ask_yes(9, "RESTORE A SAVED GAME (Y/N)?")) {
        /* Saving is deliberately not built yet. This is what the original
           does when the disk holds no game, so the prompt can stay honest
           rather than being left out of the sequence. */
        scr_puts(4, 10, "NO SAVED GAME FOUND.", EGA_TO_VDC(EGA_LTRED));
        y = 11;
    } else {
        y = 10;
    }

    scr_puts(2, y, "PLEASE ENTER YOUR NAME:", COL_LABEL);
    read_field(26, y, s->name, sizeof s->name);
    y++;

    /* Looped rather than clamped: the original asks again, and silently
       turning a typo into a different difficulty is worse than re-asking. */
    for (;;) {
        scr_puts(2, y, "FOR VERIFICATION, ENTER YOUR COMMAND LEVEL (1-5):", COL_LABEL);
        buf[0] = '\0';
        read_field(51, y, buf, 3);
        if (buf[0] >= '1' && buf[0] <= '5' && buf[1] == '\0') break;
        scr_puts(51, y, "  ", COL_VALUE);
    }
    s->level = (uint8_t)(buf[0] - '0');
    y++;

    scr_puts(2, y, "CAPTAIN, PLEASE ENTER SELF-DESTRUCT PASSWORD:", COL_LABEL);
    read_field(47, y, s->password, sizeof s->password);

    /* The whole point of the screen, mechanically: kb_entropy has been counting
       poll passes throughout, so it now holds something no two sittings will
       share. */
    s->seed = setup_seed(kb_entropy, s->level);
}

/* ------------------------------------------------------- end of game */

/* Right-aligned signed number, for the score column. put_num() is unsigned and
   pads on the left, which is what the counts want but not the points. */
static void put_signed(unsigned char x, unsigned char y, int16_t v,
                       unsigned char w, unsigned char color) {
    unsigned char buf[7], n = 0;
    uint16_t m = (uint16_t)(v < 0 ? -v : v);

    do { buf[n++] = (unsigned char)(SC_DIGIT0 + (m % 10)); m /= 10; } while (m && n < 6);
    if (v < 0) buf[n++] = 45;                      /* '-', numerically: PETSCII */
    while (w > n) { scr_put(x++, y, SC_SPACE, color); w--; }
    while (n) scr_put(x++, y, buf[--n], color);
}

/* One row: the count on the left, the item with dot leaders, the points on the
   right. The original's leaders run to a fixed column, which is what makes the
   sheet read as a column of figures rather than ragged text. */
#define EV_X       12
#define EV_ITEM    (EV_X + 6)
#define EV_DOTS_TO (EV_X + 46)

static void ev_row(unsigned char y, const char *count, const char *item,
                   int16_t pts, unsigned char color) {
    unsigned char i, x;

    if (count) scr_puts(EV_X, y, count, color);
    scr_puts(EV_ITEM, y, item, color);
    x = (unsigned char)(EV_ITEM + strlen(item));
    for (i = x; i < EV_DOTS_TO; i++) scr_put(i, y, SC_DOT, COL_GRID);
    put_signed(EV_DOTS_TO, y, pts, 5, color);
}

/* A count as text, so a row can print "0.00" for the rate and plain integers
   everywhere else through the same path. */
static void ev_count(char *buf, uint16_t v) {
    uint8_t n = 0, i;
    char tmp[6];
    do { tmp[n++] = (char)('0' + (v % 10)); v /= 10; } while (v && n < 5);
    for (i = 0; i < n; i++) buf[i] = tmp[n - 1 - i];
    buf[n] = '\0';
}

void ui_evaluation(void) {
    ScoreSheet sh;
    char c[8];
    unsigned char y = 7;

    trek_score_sheet(&sh);
    scr_clear();
    scr_puts(32, 1, "DEPT. OF SPACE",      COL_VALUE);
    scr_puts(31, 2, "EARTH HEADQUARTERS",  COL_LABEL);
    scr_puts(31, 3, "DETAILED EVALUATION", COL_VALUE);
    scr_puts(EV_ITEM,    5, "ITEM",  COL_LABEL);
    scr_puts(EV_DOTS_TO, 5, "SCORE", COL_LABEL);

    ev_count(c, sh.rescues);
    ev_row(y++, c, "RESCUES @ 200 EACH", sh.rescue_pts, COL_MSG);
    ev_row(y++, NULL, "PENALTY FOR INCOMPLETE MISSION", sh.incomplete_pts, COL_MSG);
    ev_count(c, sh.mongols);
    ev_row(y++, c, "MONGOLS KILLED @ 10 EACH", sh.mongol_pts, COL_MSG);
    ev_count(c, sh.commanders);
    ev_row(y++, c, "COMMANDERS KILLED @ 20 EACH", sh.commander_pts, COL_MSG);
    ev_count(c, sh.enemy_bases);
    ev_row(y++, c, "ENEMY BASES DESTROYED @ 50 EACH", sh.enemy_base_pts, COL_MSG);

    /* The rate prints with two decimals, as the original does -- 0.00 on an
       unfinished mission, which is the commonest sight on this screen. */
    {
        char r[8];
        uint8_t k = 0;
        ev_count(r, (uint16_t)(sh.rate_hundredths / 100));
        k = (uint8_t)strlen(r);
        r[k++] = '.';
        r[k++] = (char)('0' + (sh.rate_hundredths / 10) % 10);
        r[k++] = (char)('0' + sh.rate_hundredths % 10);
        r[k]   = '\0';
        ev_row(y++, r, "KILL/DAY RATIO @ 500 PER DAY", sh.rate_pts, COL_MSG);
    }

    ev_count(c, sh.casualties);
    ev_row(y++, c, "CASUALTIES ON BOARD LEXINGTON", sh.casualty_pts, COL_MSG);
    ev_count(c, sh.stars);
    ev_row(y++, c, "STARS DESTROYED @ -5 EACH", sh.star_pts, COL_MSG);
    ev_count(c, sh.bases_hit);
    ev_row(y++, c, "BASES HIT @ -200 EACH", sh.bases_hit_pts, COL_MSG);

    y++;
    ev_row(y, NULL, "TOTAL", sh.total, COL_VALUE);

    scr_puts(29, 22, "HIT RETURN TO CONTINUE", COL_DEPT);
    while (kb_waitkey() != KB_RETURN) { }
}

/* One slot per command level, named by rank -- MEASURED off the original's own
   screen, and TREK.SCR holds ten records, two per level. Only the current
   game's entry is shown: writing the file needs disk I/O, which is deliberately
   absent along with save and restore. */
static const char *const rank_name[5] = {
    "LT. COMMANDER", "COMMANDER", "CAPTAIN", "COMMODORE", "ADMIRAL"
};

void ui_hall_of_fame(const char *name, uint8_t level, int16_t score) {
    unsigned char i, y;

    scr_clear();
    scr_puts(33, 1, "DEPT. OF SPACE", COL_VALUE);
    scr_puts(34, 2, "HALL OF FAME",   COL_VALUE);
    scr_puts(14, 4, "FOR OUTSTANDING PERFORMANCE, THE FOLLOWING DEPT. OF SPACE", COL_MSG);
    scr_puts(19, 5, "OFFICERS HAVE BEEN INDUCTED INTO THE HALL OF FAME:", COL_MSG);

    scr_puts(16, 8, "NAME", COL_LABEL);
    scr_puts(46, 8, "RANK", COL_LABEL);
    scr_puts(62, 8, "SCORE", COL_LABEL);

    for (i = 0; i < 5; i++) {
        unsigned char j;
        y = (unsigned char)(10 + i * 2);
        scr_puts(46, y, rank_name[i], COL_VALUE);
        if (level == (uint8_t)(i + 1) && name[0]) {
            scr_puts(16, y, name, COL_MSG);
            put_signed(62, y, score, 6, COL_MSG);
        } else {
            for (j = 0; j < 20; j++) scr_put((unsigned char)(16 + j), y, SC_DOT, COL_GRID);
            put_signed(62, y, 0, 6, COL_GRID);
        }
    }

    scr_puts(29, 22, "HIT RETURN TO CONTINUE", COL_DEPT);
    while (kb_waitkey() != KB_RETURN) { }
}

/* ---------------------------------------------------------- title screen */

/* The original's logo is a drawn bitmap in EGA mode 10h. On a text VDC the
   nearest honest thing is a block-letter banner, so this carries a 5x5 font
   for exactly the six letters "EGA TREK" needs. Rows are bit patterns, high
   bit leftmost of five. */
static const unsigned char banner_font[6][5] = {
    { 0x1F, 0x10, 0x1E, 0x10, 0x1F },   /* E */
    { 0x0F, 0x10, 0x17, 0x11, 0x0E },   /* G */
    { 0x0E, 0x11, 0x1F, 0x11, 0x11 },   /* A */
    { 0x1F, 0x04, 0x04, 0x04, 0x04 },   /* T */
    { 0x1E, 0x11, 0x1E, 0x12, 0x11 },   /* R */
    { 0x11, 0x12, 0x1C, 0x12, 0x11 }    /* K */
};
static const char banner_text[] = "EGATREK";       /* index into the font */
static const unsigned char banner_gap = 3;         /* blank cols between EGA and TREK */

static void banner_letter(unsigned char bx, unsigned char by, unsigned char idx,
                          unsigned char color) {
    unsigned char r, c;
    for (r = 0; r < 5; r++) {
        for (c = 0; c < 5; c++) {
            if (banner_font[idx][r] & (unsigned char)(0x10 >> c))
                scr_put((unsigned char)(bx + c), (unsigned char)(by + r), G_BLOCK, color);
        }
    }
}

void ui_title(void) {
    unsigned char i, x;

    scr_clear();

    /* Version bar, where the original puts its "Revision 3.0". Ours names the
       port, because claiming the original's revision number would be a lie
       about what this program is. */
    scr_puts(1, 0, " C128-VDC PORT ", EGA_TO_VDC(EGA_LTCYAN));

    /* "EGA" then a gap then "TREK": 3*6 + gap + 4*6 - 1 columns wide. */
    x = 15;
    for (i = 0; i < 7; i++) {
        unsigned char idx = 0;
        switch (banner_text[i]) {
            case 'E': idx = 0; break;
            case 'G': idx = 1; break;
            case 'A': idx = 2; break;
            case 'T': idx = 3; break;
            case 'R': idx = 4; break;
            default:  idx = 5; break;          /* K */
        }
        if (i == 3) x = (unsigned char)(x + banner_gap);   /* the space */
        banner_letter(x, 3, idx, COL_VALUE);
        x = (unsigned char)(x + 6);
    }

    scr_puts(30, 9, "THE MONGOL INVASION", EGA_TO_VDC(EGA_LTCYAN));

    /* The Lexington, drawn in solid cells rather than punctuation. An earlier
       version used "/", "\\" and "_" and lost half of them: cc65 translates
       string literals to PETSCII, where those glyphs are not where ASCII put
       them. Blocks sidestep the character set entirely, and match the
       original's filled artwork better than line-drawing would. */
    {
        static const char *const ship_art[5] = {
            "....######....",
            "...########...",
            "....######....",
            "......##......",
            "..###########."
        };
        unsigned char r, c;
        for (r = 0; r < 5; r++)
            for (c = 0; ship_art[r][c]; c++)
                if (ship_art[r][c] == '#')
                    scr_put((unsigned char)(8 + c), (unsigned char)(11 + r),
                            G_BLOCK, EGA_TO_VDC(EGA_LTGRAY));
    }

    /* Ship plate, bottom left, matching the console's own badge panel. */
    box(4, 16, 24, 7, COL_LABEL);
    scr_puts(9,  17, "U.S.S. LEXINGTON", COL_VALUE);
    scr_puts(13, 18, "RCB-92",           COL_VALUE);
    scr_puts(9,  20, "DEPT. OF SPACE",   EGA_TO_VDC(EGA_LTCYAN));

    /* And the credit. The original earns this panel -- EGA Trek was shareware
       and Nels Anderson wrote it; this port exists because of his game, so his
       name goes on the front of it and not in a comment somewhere. */
    box(32, 16, 44, 7, COL_LABEL);
    scr_puts(34, 17, "EGA TREK WAS WRITTEN BY NELS ANDERSON",   COL_MSG);
    scr_puts(34, 18, "AND RELEASED AS SHAREWARE IN 1992.",      COL_MSG);
    scr_puts(34, 20, "THIS IS AN INDEPENDENT PORT OF HIS GAME", EGA_TO_VDC(EGA_LTCYAN));
    scr_puts(34, 21, "TO THE COMMODORE 128.",                   EGA_TO_VDC(EGA_LTCYAN));

    scr_puts(28, 24, "PRESS RETURN TO BEGIN", COL_DEPT);
    while (kb_waitkey() != KB_RETURN) { }
}

/* --------------------------------------------------------------- INFO */

#define INF_X   22
#define INF_Y    6
#define INF_W   36
#define INF_H   12

void ui_info_panel(void) {
    unsigned char cells[QUAD_CELLS];
    unsigned char n = 0, i, sel = 0;

    for (i = 0; i < QUAD_CELLS; i++)
        if (SEC_IS_ENEMY(sector[i])) cells[n++] = i;

    for (;;) {
        unsigned char cell, y, x, color;
        uint16_t d, full, pct;
        char c;

        box(INF_X, INF_Y, INF_W, INF_H, COL_LABEL);
        scr_puts((unsigned char)(INF_X + 2), INF_Y, "INFO", COL_VALUE);

        if (n == 0) {
            scr_puts((unsigned char)(INF_X + 10), (unsigned char)(INF_Y + 5),
                     "NO CONTACT", COL_GRID);
            scr_puts((unsigned char)(INF_X + 6), (unsigned char)(INF_Y + INF_H - 2),
                     "RETURN TO CLOSE", COL_DEPT);
            while (kb_waitkey() != KB_RETURN) { }
            return;
        }

        cell = cells[sel];
        y = (unsigned char)(cell >> 3);
        x = (unsigned char)(cell & 7);
        (void)cell_glyph(sector[cell], &color);

        /* Same silhouette the viewer draws, so a ship looks the same wherever
           it is shown. */
        scr_put((unsigned char)(INF_X + 14), (unsigned char)(INF_Y + 2), 81, color);
        scr_hline((unsigned char)(INF_X + 15), (unsigned char)(INF_Y + 2), 4, G_HLINE, color);
        scr_put((unsigned char)(INF_X + 19), (unsigned char)(INF_Y + 2), 160, color);

        scr_puts((unsigned char)(INF_X + 2), (unsigned char)(INF_Y + 4),
                 enemy_class(sector[cell]), color);

        scr_puts((unsigned char)(INF_X + 2), (unsigned char)(INF_Y + 6), "SECTOR:", COL_LABEL);
        put_num((unsigned char)(INF_X + 14), (unsigned char)(INF_Y + 6),
                (uint16_t)(y + 1), 1, COL_VALUE);
        scr_put((unsigned char)(INF_X + 15), (unsigned char)(INF_Y + 6), 45, COL_VALUE);
        put_num((unsigned char)(INF_X + 16), (unsigned char)(INF_Y + 6),
                (uint16_t)(x + 1), 1, COL_VALUE);

        d = trek_dist((unsigned char)(y > ship.sec_y ? y - ship.sec_y : ship.sec_y - y),
                      (unsigned char)(x > ship.sec_x ? x - ship.sec_x : ship.sec_x - x));
        scr_puts((unsigned char)(INF_X + 2), (unsigned char)(INF_Y + 7), "RANGE:", COL_LABEL);
        put_num((unsigned char)(INF_X + 14), (unsigned char)(INF_Y + 7),
                (uint16_t)(d >> 8), 1, COL_VALUE);
        scr_put((unsigned char)(INF_X + 15), (unsigned char)(INF_Y + 7), SC_DOT, COL_VALUE);
        put_num((unsigned char)(INF_X + 16), (unsigned char)(INF_Y + 7),
                (uint16_t)(((d & 0xFF) * 100) >> 8), 2, COL_VALUE);

        scr_puts((unsigned char)(INF_X + 2), (unsigned char)(INF_Y + 8), "BEARING:", COL_LABEL);
        put_num((unsigned char)(INF_X + 14), (unsigned char)(INF_Y + 8),
                trek_bearing(y, x), 3, COL_VALUE);

        /* The original reports strength as a percentage and calls it shields;
           the raw hit points never appear on its screen. */
        full = trek_enemy_full_hp(sector[cell]);
        /* hp * 100 would overflow at 695 hit points, so scale by ten on both
           sides: 6950 and 69 both fit, and the answer is the same percentage. */
        pct  = full ? (uint16_t)(((uint16_t)(enemy_hp[cell] * 10)) / (full / 10 ? full / 10 : 1)) : 0;
        if (pct > 100) pct = 100;
        scr_puts((unsigned char)(INF_X + 2), (unsigned char)(INF_Y + 9), "SHIELDS:", COL_LABEL);
        put_num((unsigned char)(INF_X + 14), (unsigned char)(INF_Y + 9), pct, 3,
                sys_color((unsigned char)pct));
        scr_put((unsigned char)(INF_X + 17), (unsigned char)(INF_Y + 9), SC_PCT,
                sys_color((unsigned char)pct));

        if (n > 1)
            scr_puts((unsigned char)(INF_X + 4), (unsigned char)(INF_Y + INF_H - 2),
                     "SPACE=NEXT  RETURN=CLOSE", COL_DEPT);
        else
            scr_puts((unsigned char)(INF_X + 10), (unsigned char)(INF_Y + INF_H - 2),
                     "RETURN TO CLOSE", COL_DEPT);

        c = kb_waitkey();
        if (c == KB_RETURN) return;
        if (c == KB_SPACE && n > 1) sel = (unsigned char)((sel + 1) % n);
    }
}
