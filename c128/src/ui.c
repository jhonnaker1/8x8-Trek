#include <string.h>

#include "vdc.h"
#include "layout.h"
#include "ui.h"
#include "../../core/strpool.h"
#include "strdata.h"
#include "input.h"
#include "egavdc.h"
#include "../../core/trek.h"
#include "../../core/hof.h"
#include "../../core/serial.h"
#include "../../core/storage.h"
#include "../../core/ega.h"

/* Raw screen codes. Letters are 1..26 in the C64/C128 set, digits 48..57 --
   these bypass scr_puts deliberately, since scr_put takes a raw code (see
   the split documented in vdc.h). */
#define SC_SPACE   32
#define SC_STAR    42    /* '*' */
#define SC_HASH    35    /* '#' */
#define SC_DOT     46
#define SC_DASH    45    /* '-' */
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
    unsigned char level = trek_srscan_level();

    clear_panel(P_SCAN);

    /* Dead scanners draw the grid and nothing in it, so the panel still
       reads as a scanner rather than as a blank hole in the console. */
    if (level == SCAN_DEAD) {
        scr_puts((unsigned char)(p->x + 3), (unsigned char)(p->y + 4),
                 "SCANNERS", EGA_TO_VDC(EGA_LTRED));
        scr_puts((unsigned char)(p->x + 3), (unsigned char)(p->y + 5),
                 S(S_43), EGA_TO_VDC(EGA_LTRED));
        return;
    }

    /* Column headers and row labels are 1-based, as the original presents
       them; the core is 0-based throughout. */
    for (col = 0; col < QUAD_DIM; col++)
        scr_put((unsigned char)(x0 + col * 2), (unsigned char)(p->y + 1),
                (unsigned char)(SC_DIGIT0 + col + 1), COL_LABEL);

    for (row = 0; row < QUAD_DIM; row++) {
        scr_put((unsigned char)(p->x + 2), (unsigned char)(y0 + row),
                (unsigned char)(SC_DIGIT0 + row + 1), COL_LABEL);

        for (col = 0; col < QUAD_DIM; col++) {
            unsigned char cell = sector[(row << 3) | col];

            /* MEASURED from the manual 2026-08-24: "Above 90% they are fully
               functional, but below 90% they are unable to detect anything
               smaller than a star. Below 50% they do not function at all."
               Our own ship stays visible either way -- the scanner is not
               how the captain knows where his ship is. */
            if (level == SCAN_COARSE && cell != SEC_STAR && cell != SEC_SHIP)
                cell = SEC_EMPTY;

            glyph = cell_glyph(cell, &color);
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
    unsigned char lr = trek_lrscan_level();

    clear_panel(P_CHART);

    /* Below 50% they are not functional at all. */
    if (lr == SCAN_DEAD) {
        scr_puts((unsigned char)(p->x + 2), (unsigned char)(p->y + 4),
                 S(S_47), EGA_TO_VDC(EGA_LTRED));
        return;
    }

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

            /* MEASURED from the manual 2026-08-24: long range scanners
               "when less than 100% repaired ... can no longer detect enemy
               ships", so the first digit becomes a dash rather than a zero.
               A zero would be a lie the player acts on; a dash says the
               scanner cannot tell. Bases and stars still read. */
            color = COL_VALUE;
            if (lr == SCAN_FULL && gal_enemies[q]) color = EGA_TO_VDC(EGA_CHART_MONGOL);
            else if (gal_base[q] != BASE_NONE) color = COL_BASE;

            if (lr == SCAN_FULL)
                scr_put(x, y, (unsigned char)(SC_DIGIT0 + gal_enemies[q]), color);
            else
                scr_put(x, y, SC_DASH, COL_UNKNOWN);
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
        scr_puts(bx, by, S(S_46), COL_LABEL);
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
    /* The original's own word, and its own panel label: "Mongols:". */
    scr_puts(lx, y, "MONGOLS", COL_LABEL);
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

    scr_puts((unsigned char)(p->x + 2), y, S(S_89), COL_VALUE);
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

    scr_puts((unsigned char)(p->x + 3), y, S(S_20), COL_LABEL);
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

/* The panel holds no message text of its own -- only which log entries are
   still showing. MEASURED 2026-08-23: the panel is a QUEUE of messages
   awaiting acknowledgement and MSGS is a permanent RECORD, and A1 removes a
   message from the first without touching the second. Two separate things,
   so the panel stores four indices and the text lives once, in the log.

   This also happens to be what made the build fit. Four copies of a message
   cost 220 bytes of a binary that had 395 left; four indices cost four. */
static unsigned char panel_slot[MSG_SLOTS];
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

/* ------------------------------------------------------ the message log */

/* MSGS reviews messages that have already scrolled off the four-box panel,
 * so it needs an archive deeper than the panel. The archive lives in **VDC
 * RAM**, not in the 8502's.
 *
 * That is not cleverness for its own sake. When this was written the linked
 * binary had 395 bytes left between the end of BSS and the top of MAIN --
 * NOT the 1,688 the briefing note recorded, which was measured before the
 * sound driver and six commands landed. Thirty-two entries of panel-shaped
 * message cost 1,792 bytes. There is no version of this that fits in 8502
 * RAM.
 *
 * The VDC has 16K of its own and this port uses three regions of it: the
 * screen at $0000, the attributes at $0800, and the character set the KERNAL
 * left at $2000. **$1000..$1FFF is unused**, which is 4K going spare on the
 * far side of a bus the CPU's memory map never sees.
 *
 * A message log is close to the ideal tenant for it. VDC RAM is slow -- every
 * byte is a register write, an address set-up and a ready poll -- but this is
 * written once per message and read only while MSGS is open. Nothing in the
 * turn loop touches it.
 *
 * The stride is 64 rather than the 55 an entry needs, so the offset is a
 * shift instead of a multiply. The nine wasted bytes an entry are free: they
 * are bytes of a region nothing else can use. */
#define LOG_BASE    0x1000
#define LOG_STRIDE  64          /* power of two on purpose -- see above */
#define LOG_SLOTS   32
#define LOG_DEPT    16
#define LOG_TEXT    (MSG_WIDTH + 1)

/* Entry layout, within the 64-byte stride:
     0..1    stardate in tenths, low byte first
     2..17   department, NUL padded
     18..54  text, NUL padded
     55..63  unused */
#define LOG_OFF_DEPT  2
#define LOG_OFF_TEXT  (LOG_OFF_DEPT + LOG_DEPT)

static unsigned char log_count = 0;   /* entries written, capped at LOG_SLOTS */
static unsigned char log_head  = 0;   /* next slot to write; wraps */

/* Where MSGS puts the entry it is currently drawing. Statics rather than out
   parameters: cc65 -O has miscompiled an out parameter in this port before
   (see read_field), and this build passes -O. */
static char     view_dept[LOG_DEPT + 1];
static char     view_text[LOG_TEXT + 1];
static uint16_t view_date;

static unsigned int log_addr(unsigned char slot) {
    return (unsigned int)LOG_BASE + ((unsigned int)slot << 6);
}

/* Copies a NUL-terminated string into `n` bytes, padding with NUL. Written as
   one pass because the VDC's update address auto-increments: seeking again
   per byte would cost an address set-up and a ready poll each time. */
static void log_put_str(const char *src, unsigned char n) {
    unsigned char i;
    unsigned char c = 1;             /* nonzero until the terminator is hit */

    for (i = 0; i < n; i++) {
        if (c) c = (unsigned char)src[i];
        vdc_data_write(c);
    }
}

static void log_append(const char *dept, const char *text, uint16_t date) {
    vdc_set_address(log_addr(log_head));
    vdc_data_write((unsigned char)(date & 0xFF));
    vdc_data_write((unsigned char)(date >> 8));
    log_put_str(dept, LOG_DEPT);
    log_put_str(text, LOG_TEXT);

    log_head = (unsigned char)((log_head + 1) % LOG_SLOTS);
    if (log_count < LOG_SLOTS) log_count++;
}

/* `n` counts from the OLDEST entry still held, so callers index a list rather
   than a ring. Once the log has wrapped, entry 0 is whatever log_head points
   at, because that is the slot about to be overwritten. */
/* By absolute slot. The panel indexes this way, because a panel entry has to
   keep pointing at the same message while newer ones arrive behind it. */
static void log_fetch_slot(unsigned char slot) {
    unsigned char i;

    vdc_set_address(log_addr(slot));
    view_date = vdc_data_read();
    view_date |= (uint16_t)vdc_data_read() << 8;
    for (i = 0; i < LOG_DEPT; i++) view_dept[i] = (char)vdc_data_read();
    view_dept[LOG_DEPT] = '\0';
    for (i = 0; i < LOG_TEXT; i++) view_text[i] = (char)vdc_data_read();
    view_text[LOG_TEXT] = '\0';
}

/* By position from the OLDEST entry still held, which is how MSGS reads it.
   Once the log has wrapped, entry 0 is whatever log_head points at, because
   that is the slot about to be overwritten. */
static void log_fetch(unsigned char n) {
    log_fetch_slot((log_count < LOG_SLOTS)
                   ? n
                   : (unsigned char)((log_head + n) % LOG_SLOTS));
}

void ui_clear_messages(void) {
    msg_count = 0;
    log_count = 0;
    log_head  = 0;
    msg_clear_region();
}

/* Draws one box. The stardate goes into the top border rather than costing a
   text row, which is where the original puts it too. */
static void msg_box(unsigned char slot) {
    unsigned char y = msg_top(slot);
    unsigned char right = (unsigned char)(MSG_X + MSG_W - 1);
    unsigned char bottom = (unsigned char)(y + msg_height(slot) - 1);
    unsigned char color;
    unsigned char dlen, rem;
    char tbuf[MSG_WIDTH + 1];

    /* Pulls the text out of VDC RAM into view_dept/view_text. Everything
       below reads those, not arrays of its own. C89 wants the declarations
       first, so the fetch comes after them rather than beside its comment. */
    log_fetch_slot(panel_slot[slot]);
    color = dept_color(view_dept);

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

    put_tenths((unsigned char)(right - 7), y, view_date, color);

    /* The VDC auto-increments its update address, so a line running past the
       right edge does not clip -- it wraps onto the next row and lands in
       whatever is there. Clamp here rather than trusting callers. */
    dlen = (unsigned char)strlen(view_dept);
    if (dlen > MSG_W - 2) dlen = (unsigned char)(MSG_W - 2);
    rem = (unsigned char)(MSG_W - 2 - dlen);
    if (rem > MSG_WIDTH) rem = MSG_WIDTH;

    scr_puts((unsigned char)(MSG_X + 1), (unsigned char)(y + 1),
             view_dept, COL_DEPT);
    strncpy(tbuf, view_text, rem);
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
    unsigned char slot = log_head;      /* where log_append is about to put it */

    log_append(dept, text, ship.stardate);

    if (msg_count == MSG_SLOTS) {
        /* Oldest scrolls off the panel, as the original's stack does. It
           stays in the log: falling off the panel is not acknowledgement. */
        for (i = 1; i < MSG_SLOTS; i++) panel_slot[i - 1] = panel_slot[i];
        msg_count--;
    }

    panel_slot[msg_count++] = slot;
    msg_redraw();
}

/* A#, MEASURED 2026-08-23. `n` is 1-based by position down the panel, which
 * is how the player counts them -- the original prints no numbers on the
 * boxes and this port does not either, because adding them would be a change
 * to the game rather than a port of it.
 *
 * Dismisses from the PANEL only. The message stays in the log and MSGS still
 * shows it afterwards; that was captured explicitly, because the natural
 * implementation deletes and the original does not.
 *
 * n == 0 means bare `A`, which the original treats as "clear them all".
 * Out of range is a silent no-op: `A5` against an empty panel drew nothing
 * and reported nothing. */
void ui_ack(uint8_t n) {
    unsigned char i;

    if (n == 0) {
        msg_count = 0;
    } else if (n <= msg_count) {
        for (i = n; i < msg_count; i++) panel_slot[i - 1] = panel_slot[i];
        msg_count--;
    } else {
        return;                        /* silent, as the original is */
    }
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

/* A yes/no question on the COMMAND line, where the original asks it.
 *
 * MEASURED: typing Q in the original does not quit. It puts "Quit <Y/N>? " in
 * the COMMAND panel and waits. Ours quit on the keystroke until now, which
 * made an accidental Q the most expensive typo in the game.
 *
 * Explicit Y or N and nothing else, RETURN included -- the same rule as
 * ui_play_again(), and for the same reason: ask_yes() on the setup screen
 * reads a bare RETURN as no, and a RETURN meaning yes on one screen and no on
 * another is how a session ends by accident. */
uint8_t ui_confirm(const char *prompt) {
    const Panel *p = &panels[P_COMMAND];
    unsigned char x0 = (unsigned char)(p->x + 2);
    unsigned char y  = (unsigned char)(p->y + 1);
    unsigned char len;
    char c;

    scr_hline(x0, y, (unsigned char)(p->w - 3), SC_SPACE, COL_LABEL);
    scr_puts(x0, y, prompt, COL_LABEL);
    for (len = 0; prompt[len]; len++) { }
    scr_put((unsigned char)(x0 + len + 1), y, 32 + 128, COL_VALUE);

    for (;;) {
        c = kb_waitkey();
        if (c == KB_Y || c == KB_N) break;
    }

    /* Leave the line as we found it: the caller either quits, in which case
       the console is about to go, or carries on and wants a clean prompt. */
    scr_hline(x0, y, (unsigned char)(p->w - 3), SC_SPACE, COL_LABEL);
    return (uint8_t)(c == KB_Y);
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
/* Returns 1 normally, 0 if the player pressed ESC. A RETURN VALUE and not an
   out-parameter on purpose: cc65 -O has crashed outright on an out-parameter
   in this codebase before, and it is recorded as a trap to avoid. */
static uint8_t read_field(unsigned char x0, unsigned char y, char *buf, uint8_t max) {
    unsigned char n = 0;
    char c;

    for (;;) {
        scr_put((unsigned char)(x0 + n), y, 32 + 128, COL_VALUE);   /* cursor */
        c = kb_waitkey();
        if (c == KB_ESC) {
            scr_put((unsigned char)(x0 + n), y, SC_SPACE, COL_VALUE);
            buf[0] = 0;
            return 0;
        }
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
    return 1;
}

/* Same, but ESC abandons the prompt and returns 0. Only self destruct wants
   this -- EGA Trek's own prompt says "Hit ESC to abort". */
uint8_t ui_dialog_ask_esc(const char *prompt, char *buf, uint8_t max) {
    unsigned char y;

    dlg_room();
    y = (unsigned char)(DLG_Y + dlg_row);
    scr_puts((unsigned char)(DLG_X + 2), y, prompt, COL_DEPT);
    dlg_row++;
    return read_field((unsigned char)(DLG_X + 2 + strlen(prompt) + 1), y, buf, max);
}

void ui_dialog_ask(const char *prompt, char *buf, uint8_t max) {
    (void)ui_dialog_ask_esc(prompt, buf, max);
}


/* Closes WITHOUT the "hit return" prompt.
 *
 * Split out 2026-08-24 for SAVE. Every dialog in this port ends by asking for
 * a RETURN, which is right for the ones reporting something the player must
 * read -- a weapons exchange, an energy transfer. It is wrong for SAVE:
 * MEASURED, the original's save box closes the moment it has a file name and
 * asks for nothing more. Jamie noticed the extra keystroke immediately. */
void ui_dialog_dismiss(void) {
    unsigned char x, y;

    for (y = 0; y < DLG_H; y++)
        for (x = 0; x < DLG_W; x++)
            scr_put((unsigned char)(DLG_X + x), (unsigned char)(DLG_Y + y),
                    SC_SPACE, COL_LABEL);
    ui_draw_all();
}

void ui_dialog_close(void) {
    unsigned char x, y;

    dlg_room();
    scr_puts((unsigned char)(DLG_X + 2), (unsigned char)(DLG_Y + dlg_row),
             S(S_40), COL_DEPT);
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

/* The same names, for anything outside this file that needs them -- F)ix lists
   them so the player can pick a number. */
const char *ui_sys_name(uint8_t i) {
    /* Written as an if rather than a ternary: cc65 rejects the conditional
       here, one arm being `const char *const` from the table and the other a
       plain string literal. */
    if (i < SYS_COUNT) return sys_name[i];
    return "";
}

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
    scr_puts((unsigned char)(REP_X + 14), REP_Y, S(S_75), COL_VALUE);

    scr_puts((unsigned char)(REP_X + 2),  (unsigned char)(REP_Y + 1), "SYSTEM", COL_LABEL);
    scr_puts((unsigned char)(REP_X + 26), (unsigned char)(REP_Y + 1), S(S_66), COL_LABEL);
    scr_puts((unsigned char)(REP_X + 24), (unsigned char)(REP_Y + 2), S(S_22), COL_LABEL);

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
             S(S_40), COL_DEPT);
}

/* ------------------------------------------------------------- MSGS */

/* The PREVIOUS MESSAGES overlay, MEASURED 2026-08-23 -- see MEASURED.md for
 * the capture. Four behaviours were captured rather than guessed, and three
 * of them are not what the obvious implementation does:
 *
 *   - it opens SCROLLED TO THE BOTTOM, newest visible, with the topmost
 *     entry routinely cut off mid-way. That cut is how it says there is more
 *     above, so it is a feature, not an accident of arithmetic.
 *   - it scrolls ONE LINE per keypress, not one entry.
 *   - it opens on an EMPTY log rather than refusing.
 *   - it costs no turn.
 *
 * Colours are the original's: cyan title and border, magenta StarDate line,
 * green message text, red footer. Note the department colour does NOT carry
 * in here -- COMMUNICATIONS is yellow and DAMAGE REPORT orange on the panel,
 * and both are green in this box. Measured on one of each in one capture.
 *
 * Two deliberate departures, both forced:
 *
 * GEOMETRY. The original's content lines sit on a 10-PIXEL pitch, so eleven
 * of them fit in about ten character rows. A character display cannot
 * compress like that, so the box is 15 rows here against the original's ~10.
 * The same trade the short range scan and chart panels already make. The
 * COLUMNS are the original's exactly: 25..65.
 *
 * FILL. The original's box is filled dark grey. This port's box() draws
 * spaces in the border colour, which on the VDC shows the screen background
 * through -- a per-cell background would mean reverse-video spaces. Left
 * alone so this box matches every other dialog in the port rather than
 * being the only filled one. */
#define MSGV_X      25
#define MSGV_Y       6
#define MSGV_W      41
#define MSGV_LINES  11
#define MSGV_H      (MSGV_LINES + 4)   /* border, title, lines, footer, border */

#define COL_MSGV_TITLE  EGA_TO_VDC(EGA_CYAN)
#define COL_MSGV_DATE   EGA_TO_VDC(EGA_MAGENTA)
#define COL_MSGV_TEXT   EGA_TO_VDC(EGA_GREEN)
#define COL_MSGV_FOOT   EGA_TO_VDC(EGA_RED)

/* Every log entry occupies two lines here -- the StarDate line and one line
   of text. The original wraps its text to as many as three; this port's
   messages are one line of 36 characters by construction (see MSG_WIDTH), so
   there is nothing to wrap. Scrolling still works in LINES, as measured,
   which is why this is a line count and not an entry count. */
#define MSGV_PER_ENTRY 2

static void msgv_draw(unsigned char top) {
    unsigned char r, line, entry, last = 0xFF;
    unsigned char x = (unsigned char)(MSGV_X + 1);
    unsigned char total = (unsigned char)(log_count * MSGV_PER_ENTRY);

    for (r = 0; r < MSGV_LINES; r++) {
        unsigned char y = (unsigned char)(MSGV_Y + 2 + r);

        scr_hline(x, y, (unsigned char)(MSGV_W - 2), SC_SPACE, COL_MSGV_TEXT);

        line = (unsigned char)(top + r);
        if (line >= total) continue;

        entry = (unsigned char)(line / MSGV_PER_ENTRY);
        /* Consecutive lines share an entry, so fetch only on the change.
           Halves the VDC traffic a redraw costs. */
        if (entry != last) { log_fetch(entry); last = entry; }

        if ((line % MSGV_PER_ENTRY) == 0) {
            scr_puts(x, y, "STARDATE:", COL_MSGV_DATE);
            put_tenths((unsigned char)(x + 10), y, view_date, COL_MSGV_DATE);
        } else {
            scr_puts((unsigned char)(x + 1), y, view_dept, COL_MSGV_TEXT);
            scr_puts((unsigned char)(x + 1 + strlen(view_dept)), y,
                     view_text, COL_MSGV_TEXT);
        }
    }
}

void ui_messages_view(void) {
    unsigned char total = (unsigned char)(log_count * MSGV_PER_ENTRY);
    unsigned char top;
    char c;

    box(MSGV_X, MSGV_Y, MSGV_W, MSGV_H, COL_MSGV_TITLE);
    scr_puts((unsigned char)(MSGV_X + 12), MSGV_Y, S(S_63),
             COL_MSGV_TITLE);
    /* 30 characters centred in a 41-wide box. The wording is this port's:
       the original draws real up and down arrow glyphs, which the C128's
       stock character set has no equivalent of at these screen codes. */
    scr_puts((unsigned char)(MSGV_X + 5), (unsigned char)(MSGV_Y + MSGV_H - 1),
             S(S_90), COL_MSGV_FOOT);

    /* Opens at the bottom. Measured, and it is the opposite of what a
       from-the-top viewer would do. */
    top = (total > MSGV_LINES) ? (unsigned char)(total - MSGV_LINES) : 0;

    for (;;) {
        msgv_draw(top);
        c = kb_waitkey();
        /* ESC only. The original's footer says "ESC to exit" where its other
           screens say "HIT RETURN TO CONTINUE", and it distinguishes the two
           deliberately -- so this one does too, rather than accepting both
           for convenience. */
        if (c == KB_ESC) break;
        if (c == KB_UP   && top > 0) top--;
        if (c == KB_DOWN && total > MSGV_LINES
                         && top < (unsigned char)(total - MSGV_LINES)) top++;
    }

    ui_draw_all();
}

/* ------------------------------------------------------------ SAVE/RESTORE */

/* The save file: a small UI header, then the core's own blob.
 *
 * `core/serial.c` serialises the GAME -- galaxy, ship, event queue, RNG. It
 * does not know the player's name or self-destruct password, and it should
 * not: those belong to this screen, not to the rules. So the file is
 *
 *     [13] name, NUL padded
 *     [ 9] self-destruct password, NUL padded
 *     [ 2] command level, low byte first (matches core/serial.c's rule)
 *     [  ] TREK_SAVE_SIZE bytes from trek_state_save()
 *
 * The core half already carries its own magic and version and refuses
 * anything it does not recognise, so this header needs neither.
 *
 * MEASURED 2026-08-24: the original's file is plain text and starts with the
 * banner "EGATrek 3.0" then the player's name -- the same two ideas, version
 * and name, arrived at independently. Ours is binary and deliberately NOT
 * interchangeable with it; nothing in this project ever claimed it would be.
 *
 * The default name is the original's: EGATREK.SAV, which is eleven characters
 * and so fits a CBM directory entry unchanged. */
#define SAVE_HDR   (13 + 9 + 2)
#define SAVE_BYTES (SAVE_HDR + TREK_SAVE_SIZE)
#define SAVE_DEFAULT "EGATREK.SAV"

/* ONE buffer, shared with the hall of fame.
 *
 * SAVE and the hall of fame can never be in flight at the same time -- one is
 * a command inside a game, the other runs after the game has ended -- and
 * neither holds anything in here across a call. Sharing costs a comment and
 * buys 384 bytes, which mattered: the port had about 2,800 bytes of RAM left
 * when SAVE was written and two separate buffers overflowed it by 319. */
/* ONE BYTE LARGER THAN THE BIGGEST FILE, deliberately.
 *
 * plat_read_all() treats "filled the buffer exactly" as a possible silent
 * truncation and returns STOR_ERROR, because it genuinely cannot tell a file
 * that fit from one that did not. That guard is right, and it means every
 * caller must offer more room than the file needs. A save is exactly
 * SAVE_BYTES long, so passing a SAVE_BYTES buffer made every restore fail
 * with NO SAVED GAME FOUND while the file sat on the disk, correctly written
 * and correctly closed. */
#define IO_BUF_SIZE (((SAVE_BYTES + 1) > HOF_BUF) ? (SAVE_BYTES + 1) : HOF_BUF)
static uint8_t io_buf[IO_BUF_SIZE];
#define save_buf io_buf

/* Copies a NUL-terminated string into a fixed field, padding with NUL. Same
   once-terminated-always-padded rule as core/hof.c, and for the same reason:
   the caller's buffer is shorter than the field and reading past it drags in
   whatever follows -- which put a self-destruct password in the hall of fame
   once already. */
static void save_put(unsigned char *dst, const char *src, unsigned char n) {
    unsigned char i;
    char c = 1;
    for (i = 0; i < n; i++) {
        if (c) c = src[i];
        dst[i] = (unsigned char)(c ? c : 0);
    }
}

/* Non-zero if it was written. */
static uint8_t save_write(const Setup *s, const char *name) {
    uint16_t n;

    save_put(save_buf,      s->name,     13);
    save_put(save_buf + 13, s->password,  9);
    save_buf[22] = s->level;
    save_buf[23] = 0;

    n = trek_state_save(save_buf + SAVE_HDR, TREK_SAVE_SIZE);
    if (n != TREK_SAVE_SIZE) return 0;

    return (uint8_t)(plat_write_all(name, save_buf, SAVE_BYTES) == STOR_OK);
}

/* Non-zero if a game was restored. Fills the caller's Setup from the file so
   the hall of fame still knows who was flying. */
static uint8_t save_read(Setup *s, const char *name) {
    uint16_t got = 0;
    unsigned char i;

    /* SAVE_BYTES + 1 so a full-length save does not look truncated -- see the
       note on IO_BUF_SIZE. The exact length is then checked here. */
    if (plat_read_all(name, save_buf, SAVE_BYTES + 1, &got) != STOR_OK) return 0;
    if (got != SAVE_BYTES) return 0;
    if (!trek_state_load(save_buf + SAVE_HDR, TREK_SAVE_SIZE)) return 0;

    for (i = 0; i < 13; i++) s->name[i] = (char)save_buf[i];
    s->name[12] = '\0';
    for (i = 0; i < 9; i++) s->password[i] = (char)save_buf[13 + i];
    s->password[8] = '\0';
    s->level = save_buf[22];
    return 1;
}

/* SAVE. MEASURED 2026-08-24: the original opens a SAVE GAME box asking for a
   file name and offering Enter for the default, and it COSTS NO TURN. */
void ui_save_game(const Setup *s) {
    char name[18];

    ui_dialog_open("SAVE GAME");
    ui_dialog_line(S(S_3));
    ui_dialog_ask(S(S_34), name, sizeof name);
    if (!name[0]) strcpy(name, SAVE_DEFAULT);

    /* MEASURED: on success the original just CLOSES -- no confirmation, no
       second keypress. An earlier version here printed "GAME SAVED." and
       waited for RETURN, which is one keystroke the original never asks for.
       Failure is different: a disk error that flashed past unread would be
       the worst outcome of the whole command, so that one stops and waits. */
    if (save_write(s, name)) {
        ui_dialog_dismiss();
        return;
    }

    ui_dialog_line("");
    ui_dialog_line(S(S_19));
    ui_dialog_ask(S(S_40), name, 2);
    ui_dialog_close();
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

    s->restored = 0;

    scr_clear();
    scr_puts(31, 1, S(S_89), COL_VALUE);
    scr_puts(36, 2, "RCB-92",           COL_VALUE);
    scr_puts(2,  5, S(S_96), COL_MSG);

    s->briefing = ask_yes(7, "WILL YOU REQUIRE A BRIEFING (Y/N)?");

    if (ask_yes(9, "RESTORE A SAVED GAME (Y/N)?")) {
        /* MEASURED 2026-08-24: the original asks for a file name here with a
           default and an ESC abort, and on success goes STRAIGHT to the
           console -- it does not go on to ask the name, level and password,
           because the save already holds them. */
        char fname[18];

        scr_puts(2, 10, S(S_33),
                 COL_LABEL);
        if (read_field(51, 10, fname, sizeof fname)) {
            if (!fname[0]) strcpy(fname, SAVE_DEFAULT);
            if (save_read(s, fname)) {
                s->restored = 1;
                return;                 /* nothing else to ask */
            }
            scr_puts(4, 11, S(S_56), EGA_TO_VDC(EGA_LTRED));
        }
        y = 12;
    } else {
        y = 10;
    }

    scr_puts(2, y, S(S_61), COL_LABEL);
    read_field(26, y, s->name, sizeof s->name);
    y++;

    /* Looped rather than clamped: the original asks again, and silently
       turning a typo into a different difficulty is worse than re-asking. */
    for (;;) {
        scr_puts(2, y, S(S_36), COL_LABEL);
        buf[0] = '\0';
        read_field(51, y, buf, 3);
        if (buf[0] >= '1' && buf[0] <= '5' && buf[1] == '\0') break;
        scr_puts(51, y, "  ", COL_VALUE);
    }
    s->level = (uint8_t)(buf[0] - '0');
    y++;

    scr_puts(2, y, S(S_13), COL_LABEL);
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
    scr_puts(32, 1, S(S_20),      COL_VALUE);
    scr_puts(31, 2, S(S_26),  COL_LABEL);
    scr_puts(31, 3, S(S_21), COL_VALUE);
    scr_puts(EV_ITEM,    5, "ITEM",  COL_LABEL);
    scr_puts(EV_DOTS_TO, 5, "SCORE", COL_LABEL);

    /* The original's line SET varies by ending: a lost ship's sheet carries
       the ship-loss penalty and omits RESCUES, a surviving ship's does the
       reverse. MEASURED 2026-08-24 off both. */
    if (sh.ship_lost_pts)
        ev_row(y++, NULL, "PENALTY FOR LOSS OF SHIP", sh.ship_lost_pts, COL_MSG);
    else {
        ev_count(c, sh.rescues);
        ev_row(y++, c, "RESCUES @ 200 EACH", sh.rescue_pts, COL_MSG);
    }
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

    scr_puts(29, 22, S(S_40), COL_DEPT);
    while (kb_waitkey() != KB_RETURN) { }
}

/* One slot per command level, named by rank -- MEASURED off the original's own
   screen, and TREK.SCR holds ten records, two per level. Only the current
   game's entry is shown: writing the file needs disk I/O, which is deliberately
   absent along with save and restore. */
static const char *const rank_name[5] = {
    "LT. COMMANDER", "COMMANDER", "CAPTAIN", "COMMODORE", "ADMIRAL"
};

/* The hall of fame, now persistent.
 *
 * MEASURED 2026-08-23: TWO entries per rank, a bright first place and a dim
 * second, and the file behind them is place-major -- records 0..4 are first
 * place for ranks 1..5, records 5..9 second. core/hof.c owns that mapping.
 *
 * The table is read, offered the finished game, and written back only if the
 * score earned a place. The original does the same: a scuttled ship scoring
 * -930 left TREK.SCR's timestamp untouched.
 *
 * A missing or corrupt file reads as an EMPTY hall of fame and the game
 * carries on. There is nowhere useful to report a disk fault at this point --
 * the player has just finished a game -- and refusing to show the screen
 * because a file is missing would be worse than showing a blank one. */
static HofEntry hof[HOF_ENTRIES];
/* Shares io_buf with SAVE -- see the note there. */
#define hof_buf io_buf

#define HOF_FILE "TREK.SCR"

void ui_hall_of_fame(const char *name, uint8_t level, int16_t score) {
    unsigned char i, y, j, idx;
    uint16_t got = 0;
    uint16_t n;

    if (plat_read_all(HOF_FILE, hof_buf, sizeof hof_buf, &got) != STOR_OK ||
        !hof_parse(hof_buf, got, hof))
        hof_clear(hof);

    if (name[0] && hof_offer(hof, level, name, score)) {
        n = hof_format(hof, hof_buf, sizeof hof_buf);
        if (n) plat_write_all(HOF_FILE, hof_buf, n);
    }

    scr_clear();
    scr_puts(33, 1, S(S_20), COL_VALUE);
    scr_puts(34, 2, S(S_38),   COL_VALUE);
    scr_puts(14, 4, S(S_35), COL_MSG);
    scr_puts(19, 5, S(S_59), COL_MSG);

    scr_puts(16, 8, "NAME", COL_LABEL);
    scr_puts(46, 8, "RANK", COL_LABEL);
    scr_puts(62, 8, "SCORE", COL_LABEL);

    for (i = 0; i < HOF_RANKS; i++) {
        y = (unsigned char)(10 + i * 2);
        scr_puts(46, y, rank_name[i], COL_VALUE);

        /* First place bright, second place dim on the row below -- which is
           how the original distinguishes them, and why ten records fit in
           five rank rows. */
        for (j = 0; j < HOF_PLACES; j++) {
            unsigned char row = (unsigned char)(y + j);
            unsigned char col = j ? COL_GRID : COL_MSG;

            idx = hof_index((uint8_t)(i + 1), j);
            scr_puts(16, row, hof[idx].name, col);
            put_signed(62, row, hof[idx].score, 6, col);
        }
    }

    scr_puts(29, 22, S(S_40), COL_DEPT);
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
    scr_puts(1, 0, S(S_0), EGA_TO_VDC(EGA_LTCYAN));

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

    scr_puts(30, 9, S(S_80), EGA_TO_VDC(EGA_LTCYAN));

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
    scr_puts(9,  17, S(S_89), COL_VALUE);
    scr_puts(13, 18, "RCB-92",           COL_VALUE);
    scr_puts(9,  20, S(S_20),   EGA_TO_VDC(EGA_LTCYAN));

    /* And the credit. The original earns this panel -- EGA Trek was shareware
       and Nels Anderson wrote it; this port exists because of his game, so his
       name goes on the front of it and not in a comment somewhere. */
    box(32, 16, 44, 7, COL_LABEL);
    scr_puts(34, 17, S(S_27),   COL_MSG);
    scr_puts(34, 18, S(S_8),      COL_MSG);
    scr_puts(34, 20, S(S_81), EGA_TO_VDC(EGA_LTCYAN));
    scr_puts(34, 21, S(S_82),                   EGA_TO_VDC(EGA_LTCYAN));

    scr_puts(28, 24, S(S_62), COL_DEPT);
    while (kb_waitkey() != KB_RETURN) { }
}

/* ------------------------------------------------------- play again */

/* MEASURED 2026-08-22 against the original, which does have this and does it
   as a dialog rather than a line of text: a grey box with a magenta border
   over the Hall of Fame, "Play Again?" above two raised buttons reading YES
   and NO. Its pixel extent is x 150..280, y 70..130 in the 640x350 frame,
   which on the 8x14 cell EGA laid the console out on is columns 19..35 and
   rows 5..9 -- so the box below is the same size and in the same place.

   The buttons are drawn in pixels there and cannot be here, so they are
   bracketed instead; their measured extents (cols 21..25 and 28..32) are what
   put [YES] and [NO] where they are.

   Answering YES in the original returns to the TITLE screen and re-runs the
   whole setup -- name, level and password are all asked again, verified by
   playing two games through. NO exits to DOS. */
#define PA_X  19
#define PA_Y   5
#define PA_W  17
#define PA_H   5

uint8_t ui_play_again(void) {
    char c;

    /* The hall of fame leaves "HIT RETURN TO CONTINUE" at the foot of the
       screen, and RETURN is not an answer to this prompt -- so it goes,
       rather than sitting there telling the player to press a key that does
       nothing. */
    scr_fill_rect(29, 22, 22, 1, SC_SPACE, COL_DEPT);

    box(PA_X, PA_Y, PA_W, PA_H, EGA_TO_VDC(EGA_LTMAGENTA));
    scr_puts(PA_X + 3, PA_Y + 1, S(S_60), COL_VALUE);
    scr_puts(PA_X + 2, PA_Y + 3, "[YES]",       EGA_TO_VDC(EGA_LTRED));
    scr_puts(PA_X + 10, PA_Y + 3, "[NO]",       EGA_TO_VDC(EGA_LTRED));

    /* An explicit Y or N, and nothing else. RETURN is deliberately not a
       shortcut for either: ask_yes() on the setup screen reads a bare RETURN
       as NO, and a RETURN that means YES here and NO there is the kind of
       inconsistency that ends a session by accident. */
    for (;;) {
        c = kb_waitkey();
        if (c == KB_Y) return 1;
        if (c == KB_N) return 0;
    }
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
                     S(S_54), COL_GRID);
            scr_puts((unsigned char)(INF_X + 6), (unsigned char)(INF_Y + INF_H - 2),
                     S(S_67), COL_DEPT);
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
                     S(S_73), COL_DEPT);
        else
            scr_puts((unsigned char)(INF_X + 10), (unsigned char)(INF_Y + INF_H - 2),
                     S(S_67), COL_DEPT);

        c = kb_waitkey();
        if (c == KB_RETURN) return;
        if (c == KB_SPACE && n > 1) sel = (unsigned char)((sel + 1) % n);
    }
}
