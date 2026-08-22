/* Host-side tests for the console panels drawn in src/ui.c.
 *
 * Runs natively (cc, not cl65) against a fake VDC that records every cell
 * written. The point is not the arithmetic -- it is the bounds. The real VDC
 * auto-increments its update address, so a routine that writes one cell past
 * a panel's right edge does not clip: it wraps onto the next row and silently
 * overwrites whichever panel is there. That failure mode has already cost
 * this port one bug (message truncation in COMMUNICATIONS), and it cannot be
 * seen in a screenshot of the panel that caused it.
 *
 *     cc -Wall -o build/test_panels test/test_panels.c src/ui.c \
 *        src/layout.c ../core/trek.c && ./build/test_panels
 */

#include <stdio.h>
#include <string.h>

#include "../src/vdc.h"
#include "../src/layout.h"
#include "../src/ui.h"
#include "../src/egavdc.h"
#include "../../core/trek.h"

/* ------------------------------------------------------------ fake VDC */

#define SENTINEL 0xFF

static unsigned char cell[VDC_ROWS][VDC_COLS];
static unsigned char attr[VDC_ROWS][VDC_COLS];
static int off_screen;      /* writes outside the 80x25 grid */

static void screen_reset(void) {
    memset(cell, SENTINEL, sizeof cell);
    memset(attr, SENTINEL, sizeof attr);
    off_screen = 0;
}

void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color) {
    if (x >= VDC_COLS || y >= VDC_ROWS) { off_screen++; return; }
    cell[y][x] = ch;
    attr[y][x] = color;
}

void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color) {
    while (*s) scr_put(x++, y, (unsigned char)*s++, color);
}

void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color) {
    unsigned char i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) scr_put((unsigned char)(x + i), (unsigned char)(y + j), ch, color);
}

void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color) {
    while (w--) scr_put(x++, y, ch, color);
}

void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color) {
    while (h--) scr_put(x, y++, ch, color);
}

/* ui.c's dialog code references these; nothing here calls them. */
void vdc_reg_write(unsigned char r, unsigned char v) { (void)r; (void)v; }
char kb_waitkey(void) { return 13; }

/* ------------------------------------------------------------- harness */

/* Derived from the panel rather than restated, so that moving the panel moves
   the test with it. Two columns of (3 label + 1 space + bar) fill the
   interior; a hard-coded 7 here is what broke when SYSTEMS STATUS shrank from
   24 interior columns to the measured 18. */
#define SYS_PITCH  ((panels[P_SYSTEMS].w - 2) / 2)
#define SYS_BAR    (SYS_PITCH - 4)

static int failures;

static void check(int cond, const char *what) {
    if (!cond) { printf("FAIL: %s\n", what); failures++; }
}

/* Every panel's interior must be disjoint from every other panel's, or one
   redraw silently erases part of another. This is what caught the old
   SYSTEMS/VIEWER overlap when the panel was moved up to row 17. */
static void test_panels_disjoint(void) {
    unsigned char owner[VDC_ROWS][VDC_COLS];
    unsigned char i, x, y;
    int clash = 0;

    memset(owner, 0xFF, sizeof owner);

    /* The message region is not in panels[], so claim it first. Without this
       a panel could grow into it and nothing here would notice -- the region
       stopped being a panel when COMMUNICATIONS and DAMAGE REPORT turned out
       to be one stack of boxes rather than two frames. */
    for (y = MSG_Y; y < MSG_Y + MSG_H; y++)
        for (x = MSG_X; x < MSG_X + MSG_W; x++) owner[y][x] = 0xF0;

    for (i = 0; i < PANEL_COUNT; i++) {
        const Panel *p = &panels[i];
        check(p->x + p->w <= VDC_COLS, "panel fits horizontally");
        check(p->y + p->h <= VDC_ROWS, "panel fits vertically");
        for (y = (unsigned char)(p->y + 1); y < p->y + p->h - 1; y++)
            for (x = (unsigned char)(p->x + 1); x < p->x + p->w - 1; x++) {
                if (owner[y][x] != 0xFF) clash++;
                owner[y][x] = i;
            }
    }
    check(clash == 0, "no panel overlaps another, or the message region");
}

/* The panel has to be tall enough for what it draws. Six interior rows and
   two columns is exactly twelve systems; one row fewer and systems fall off
   the bottom border without any visible complaint. */
static void test_panel_holds_twelve(void) {
    const Panel *p = &panels[P_SYSTEMS];
    check(p->h - 2 >= 6, "SYSTEMS STATUS has six interior rows");
    check(p->w - 2 >= 14, "SYSTEMS STATUS has room for two labelled bars");
    check((p->w - 2) % 2 == 0, "its interior splits evenly into two columns");
}

static void set_all(unsigned char pct) {
    unsigned char i;
    for (i = 0; i < SYS_COUNT; i++) ship.sys[i] = pct;
}

/* Nothing may land outside the panel's interior rectangle. */
static void test_stays_inside(void) {
    const Panel *p = &panels[P_SYSTEMS];
    unsigned char x, y;
    int escaped = 0;

    set_all(100);
    screen_reset();
    ui_draw_systems();

    for (y = 0; y < VDC_ROWS; y++)
        for (x = 0; x < VDC_COLS; x++) {
            if (cell[y][x] == SENTINEL) continue;
            if (x > p->x && x < p->x + p->w - 1 &&
                y > p->y && y < p->y + p->h - 1) continue;
            escaped++;
        }

    check(off_screen == 0, "no write leaves the 80x25 grid");
    check(escaped == 0, "no write leaves the panel interior");
}

/* Where each entry goes: column-major, six rows, second column 13 cells over.
   Checked against the labels rather than against the geometry constants, so
   this fails if the layout moves without the test being updated. */
static void test_placement(void) {
    const Panel *p = &panels[P_SYSTEMS];
    static const char *want[SYS_COUNT] = {
        "CNV", "SHD", "LIF", "LAS", "TUB", "WRP",
        "IMP", "SRS", "LRS", "CMP", "TRN", "SHT"
    };
    unsigned char i, k, x, y;
    char buf[4];

    set_all(100);
    screen_reset();
    ui_draw_systems();

    for (i = 0; i < SYS_COUNT; i++) {
        x = (unsigned char)(p->x + 1 + (i / 6) * SYS_PITCH);
        y = (unsigned char)(p->y + 1 + (i % 6));
        for (k = 0; k < 3; k++) buf[k] = (char)cell[y][x + k];
        buf[3] = '\0';
        if (strcmp(buf, want[i]) != 0) {
            printf("FAIL: system %u label at %u,%u is \"%s\", want \"%s\"\n",
                   i, x, y, buf, want[i]);
            failures++;
        }
    }
}

/* The bar glyph must leave a separator row, or six bars on six consecutive
   rows fuse into one block and the panel shows a slab instead of six systems.
   Checked against the real C128 character ROM where one can be found: an
   invented screen code is exactly the kind of mistake that looks fine in the
   source and wrong on the screen, and this port has made it before. */
static void test_bar_glyph(void) {
    static const char *roms[] = {
        "/usr/local/share/vice/C128/chargen-390059-01.bin",
        "/opt/homebrew/share/vice/C128/chargen-390059-01.bin",
        "/usr/share/vice/C128/chargen-390059-01.bin",
        NULL
    };
    unsigned char want[8] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00 };
    unsigned char got[8];
    const Panel *p = &panels[P_SYSTEMS];
    unsigned char glyph;
    int i;
    FILE *f = NULL;

    set_all(100);
    screen_reset();
    ui_draw_systems();

    glyph = cell[p->y + 1][p->x + 5];
    check(glyph != 160, "bar glyph is not the solid block");

    for (i = 0; roms[i]; i++)
        if ((f = fopen(roms[i], "rb")) != NULL) break;
    if (!f) { printf("  (no C128 chargen ROM found -- bitmap check skipped)\n"); return; }

    if (fseek(f, glyph * 8, SEEK_SET) != 0 || fread(got, 1, 8, f) != 8) {
        printf("FAIL: could not read glyph %u from %s\n", glyph, roms[i]);
        failures++;
        fclose(f);
        return;
    }
    fclose(f);

    if (memcmp(got, want, 8) != 0) {
        printf("FAIL: glyph %u is not a block with a clear bottom row:", glyph);
        for (i = 0; i < 8; i++) printf(" %02X", got[i]);
        printf("\n");
        failures++;
    }
}

/* Bar length rounds to nearest cell out of seven, and a system that is alive
   at all keeps at least one cell -- an empty bar has to mean destroyed. */
static unsigned char bar_len(unsigned char sys) {
    const Panel *p = &panels[P_SYSTEMS];
    unsigned char x = (unsigned char)(p->x + 1 + (sys / 6) * SYS_PITCH + 4);
    unsigned char y = (unsigned char)(p->y + 1 + (sys % 6));
    unsigned char n = 0, k;
    for (k = 0; k < SYS_BAR; k++)
        if (attr[y][x + k] != EGA_TO_VDC(EGA_DKGRAY)) n++;
    return n;
}

static void test_bar_length(void) {
    static const unsigned char pct[] = { 0, 1, 7, 8, 14, 40, 50, 70, 90, 93, 100 };
    unsigned char i, want;
    char msg[72];

    for (i = 0; i < sizeof pct / sizeof pct[0]; i++) {
        want = (unsigned char)((pct[i] * SYS_BAR + 50) / 100);
        if (want == 0 && pct[i] != 0) want = 1;   /* alive is never empty */
        if (want > SYS_BAR) want = SYS_BAR;
        set_all(pct[i]);
        screen_reset();
        ui_draw_systems();
        sprintf(msg, "%u%% draws %u of %u cells", pct[i], want, SYS_BAR);
        check(bar_len(0) == want, msg);
    }

    /* The rounding is to nearest, so the midpoint of a cell must land on the
       higher count -- checked explicitly because "close enough" would pass
       every case above by accident. */
    set_all((unsigned char)(100 / SYS_BAR / 2 + 1));
    screen_reset();
    ui_draw_systems();
    check(bar_len(0) == 1, "half a cell rounds up to one, not down to none");
}

/* The three colours that were read off the original by poking its repair
   array: 100 full green, 70 short yellow, 40 short red. */
static void test_colours(void) {
    const Panel *p = &panels[P_SYSTEMS];
    unsigned char x = (unsigned char)(p->x + 1 + 4);
    unsigned char y = (unsigned char)(p->y + 1);

    set_all(100); screen_reset(); ui_draw_systems();
    check(attr[y][x] == EGA_TO_VDC(EGA_LTGREEN), "100% is green");

    set_all(95); screen_reset(); ui_draw_systems();
    check(attr[y][x] == EGA_TO_VDC(EGA_YELLOW), "95% is yellow, not green");

    set_all(70); screen_reset(); ui_draw_systems();
    check(attr[y][x] == EGA_TO_VDC(EGA_YELLOW), "70% is yellow");

    set_all(55); screen_reset(); ui_draw_systems();
    check(attr[y][x] == EGA_TO_VDC(EGA_YELLOW), "55% is yellow");

    set_all(40); screen_reset(); ui_draw_systems();
    check(attr[y][x] == EGA_TO_VDC(EGA_LTRED), "40% is red");
}

/* Each system is drawn from its own slot, not from a shared one. */
static void test_independent(void) {
    unsigned char i;

    set_all(100);
    for (i = 0; i < SYS_COUNT; i++) ship.sys[i] = (unsigned char)(i * 9);
    screen_reset();
    ui_draw_systems();

    for (i = 0; i < SYS_COUNT; i++) {
        unsigned char pct = (unsigned char)(i * 9);
        unsigned char want = (unsigned char)((pct * SYS_BAR + 50) / 100);
        char msg[64];
        if (want == 0 && pct != 0) want = 1;
        sprintf(msg, "system %u at %u%% draws %u cells", i, pct, want);
        check(bar_len(i) == want, msg);
    }
}


/* ------------------------------------------------ state of repair report */

/* Reads a run of cells back as text, so a test can assert what the report
   actually printed rather than that it printed something. */
static void row_text(unsigned char y, unsigned char x, unsigned char n, char *out) {
    unsigned char i;
    for (i = 0; i < n; i++) out[i] = (char)cell[y][x + i];
    out[n] = 0;
}

static void test_repair_report(void) {
    char buf[32];
    unsigned char i;

    set_all(100);
    ship.sys[SYS_CONVERTER] = 40;    /* 60 points to mend */
    ship.sys[SYS_LASERS]    = 65;
    screen_reset();
    ui_repair_report();

    /* MEASURED off the original at these very percentages: 40% took 1.3
       docked and 3.0 adrift. Ours is computed from REPAIR_PER_STARDATE and
       its docked twin, so this asserts the two agree. */
    row_text((unsigned char)(REP_Y + 4), (unsigned char)(REP_X + 25), 3, buf);
    check(strcmp(buf, "1.3") == 0, "40% mends in 1.3 stardates docked");
    row_text((unsigned char)(REP_Y + 4), (unsigned char)(REP_X + 34), 3, buf);
    check(strcmp(buf, "3.0") == 0, "and 3.0 adrift, as the original printed");

    /* An undamaged system prints no time at all -- twelve rows of 0.0 would
       be noise, and the original leaves them blank. */
    row_text((unsigned char)(REP_Y + 4 + SYS_SHIELDS), (unsigned char)(REP_X + 25), 3, buf);
    check(strcmp(buf, "   ") == 0, "an undamaged system shows no repair time");

    /* Colour follows the same measured rule as the bars. */
    check(attr[REP_Y + 4][REP_X + 2] == EGA_TO_VDC(EGA_LTRED),
          "40% names the system in red");
    check(attr[REP_Y + 4 + SYS_LASERS][REP_X + 2] == EGA_TO_VDC(EGA_YELLOW),
          "65% in yellow");
    check(attr[REP_Y + 4 + SYS_SHIELDS][REP_X + 2] == EGA_TO_VDC(EGA_LTGREEN),
          "100% in green");

    /* Every row inside the box, every system named. */
    for (i = 0; i < SYS_COUNT; i++) {
        char msg[64];
        unsigned char yy = (unsigned char)(REP_Y + 4 + i);
        sprintf(msg, "row %u sits inside the report box", i);
        check(yy < (unsigned char)(REP_Y + REP_H - 2), msg);
        sprintf(msg, "row %u names a system", i);
        check(cell[yy][REP_X + 2] != 32, msg);
    }
}

/* ------------------------------------------------------------- lasers */

/* Same bounds argument as SYSTEMS STATUS, and the LASERS panel is the tighter
   fit: label, bar and a four-digit readout come to exactly the eighteen cells
   of the interior, with nothing spare to absorb an off-by-one. */
static void test_lasers_stay_inside(void) {
    const Panel *p = &panels[P_LASERS];
    unsigned char x, y;
    int escaped = 0;

    ship.laser_eff = 100;
    ship.laser_heat = 65535U;      /* the widest readout there can be */
    screen_reset();
    ui_draw_lasers();

    for (y = 0; y < VDC_ROWS; y++)
        for (x = 0; x < VDC_COLS; x++) {
            if (cell[y][x] == SENTINEL) continue;
            if (x > p->x && x < p->x + p->w - 1 &&
                y > p->y && y < p->y + p->h - 1) continue;
            escaped++;
        }

    check(off_screen == 0, "lasers: no write leaves the 80x25 grid");
    check(escaped == 0,    "lasers: no write leaves the panel interior");

    /* And what it shows instead of a truncated number. */
    check(cell[p->y + 2][p->x + 15] == 42 &&
          cell[p->y + 2][p->x + 18] == 42,
          "a readout wider than its field shows stars, not a wrong number");
}

static unsigned char las_bar_len(unsigned char row) {
    const Panel *p = &panels[P_LASERS];
    unsigned char x = (unsigned char)(p->x + 1 + 5);
    unsigned char y = (unsigned char)(p->y + 1 + row);
    unsigned char n = 0, k;
    for (k = 0; k < 8; k++)
        if (attr[y][x + k] != EGA_TO_VDC(EGA_DKGRAY)) n++;
    return n;
}

static void test_laser_gauges(void) {
    static const struct { uint16_t eff, heat; unsigned char ec, hc; } t[] = {
        { 100,     0, 8, 0 },
        {  50,   750, 4, 4 },
        {   1,     1, 1, 1 },   /* alive at all must never draw empty */
        {   0,     0, 0, 0 },
        {   7,  1500, 1, 8 },
        { 100, 65535U, 8, 8 },  /* saturated heat pins the bar, not wraps it */
    };
    unsigned char i;
    char msg[72];

    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        ship.laser_eff  = (unsigned char)t[i].eff;
        ship.laser_heat = t[i].heat;
        screen_reset();
        ui_draw_lasers();
        sprintf(msg, "eff %u draws %u of 8", t[i].eff, t[i].ec);
        check(las_bar_len(0) == t[i].ec, msg);
        sprintf(msg, "heat %u draws %u of 8", t[i].heat, t[i].hc);
        check(las_bar_len(1) == t[i].hc, msg);
    }
}

/* 1500 is the only number here with evidence behind it, so the boundary that
   matters is the one at 1500: at or below it the ancestor does nothing, above
   it rolls for a burn. The amber band is a guess and is tested only so that
   changing it is a deliberate act. */
static void test_heat_colours(void) {
    const Panel *p = &panels[P_LASERS];
    unsigned char x = (unsigned char)(p->x + 1 + 5);
    unsigned char y = (unsigned char)(p->y + 2);
    static const struct { uint16_t heat; unsigned char ega; const char *what; } t[] = {
        {  999, EGA_LTGREEN, "999 is green"       },
        { 1000, EGA_YELLOW,  "1000 is amber"      },
        { 1500, EGA_YELLOW,  "1500 is still amber -- the threshold is not yet crossed" },
        { 1501, EGA_LTRED,   "1501 is red"        },
    };
    unsigned char i;

    ship.laser_eff = 100;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        ship.laser_heat = t[i].heat;
        screen_reset();
        ui_draw_lasers();
        check(attr[y][x] == EGA_TO_VDC(t[i].ega), t[i].what);
    }
}

/* ------------------------------------------------------------ messages */

/* Four boxes of three rows fill the region exactly, so an off-by-one in the
   stacking writes onto the row below the console. */
static void test_messages_stay_inside(void) {
    unsigned char x, y;
    int escaped = 0;

    screen_reset();
    ui_clear_messages();
    ship.stardate = 35149;
    ui_message("COMMUNICATIONS: ", "STARBASE 6-6 UNDER ATTACK");
    ui_message("DAMAGE: ", "ENERGYCONVERTER AT 47%");
    ui_message("HELM: ", "AWAITING ORDERS CAPTAIN");
    ui_message("DAMAGE: ", "87 UNIT HIT FROM 4,6");

    for (y = 0; y < VDC_ROWS; y++)
        for (x = 0; x < VDC_COLS; x++) {
            if (cell[y][x] == SENTINEL) continue;
            if (x >= MSG_X && x < MSG_X + MSG_W &&
                y >= MSG_Y && y < MSG_Y + MSG_H) continue;
            escaped++;
        }

    check(off_screen == 0, "messages: no write leaves the 80x25 grid");
    check(escaped == 0,    "messages: no write leaves the message region");
}

/* Departments are interleaved in one stack, which is the whole point of the
   correction: a damage report sits between two other messages rather than
   being routed to a panel of its own. */
static void test_message_stack(void) {
    unsigned char i;
    char msg[64];
    static const unsigned char want[4] = { 0, 1, 2, 1 };  /* index into cols[] */
    unsigned char cols[3];

    cols[0] = EGA_TO_VDC(EGA_YELLOW);   /* COMMUNICATIONS */
    cols[1] = EGA_TO_VDC(EGA_BROWN);    /* DAMAGE         */
    cols[2] = EGA_TO_VDC(EGA_LTGREEN);  /* everything else, unmeasured    */

    screen_reset();
    ui_clear_messages();
    ship.stardate = 35149;
    ui_message("COMMUNICATIONS: ", "ONE");
    ui_message("DAMAGE: ", "TWO");
    ui_message("HELM: ", "THREE");
    ui_message("DAMAGE: ", "FOUR");

    for (i = 0; i < 4; i++) {
        unsigned char y = (unsigned char)(MSG_Y + i * 3);
        sprintf(msg, "box %u has its department's border colour", i);
        check(attr[y][MSG_X] == cols[want[i]], msg);
    }

    /* COMPUTER must not be mistaken for COMMUNICATIONS -- both start with C,
       and matching on one letter would colour every computer message yellow. */
    screen_reset();
    ui_clear_messages();
    ui_message("COMPUTER: ", "M)OVE W)ARP");
    check(attr[MSG_Y][MSG_X] == cols[2],
          "COMPUTER is not mistaken for COMMUNICATIONS");
}

/* The stardate stamp sits in the top border, and it is the date the message
   was made, not the date it is drawn -- so an older box keeps its own. */
static void test_message_stamps(void) {
    unsigned char x = (unsigned char)(MSG_X + MSG_W - 8);

    screen_reset();
    ui_clear_messages();
    ship.stardate = 35149;
    ui_message("HELM: ", "FIRST");
    ship.stardate = 35162;
    ui_message("HELM: ", "SECOND");

    check(cell[MSG_Y][x] == '3' - '0' + 48, "stamp starts with the stardate");
    /* 3514.9 then 3516.2 -- compare the digit that differs. */
    check(cell[MSG_Y][x + 3] == 52 && cell[MSG_Y][x + 5] == 57,
          "the first box keeps 3514.9");
    check(cell[MSG_Y + 3][x + 3] == 54 && cell[MSG_Y + 3][x + 5] == 50,
          "the second box carries 3516.2, not the first box's date");
}

/* A fifth message scrolls the oldest off the top rather than growing past the
   region, and the surviving boxes keep their own dates. */
static void test_message_scroll(void) {
    unsigned char x = (unsigned char)(MSG_X + MSG_W - 8);

    screen_reset();
    ui_clear_messages();
    ship.stardate = 35149;
    ui_message("HELM: ", "ONE");
    ship.stardate = 35150;
    ui_message("HELM: ", "TWO");
    ship.stardate = 35151;
    ui_message("HELM: ", "THREE");
    ship.stardate = 35152;
    ui_message("HELM: ", "FOUR");
    ship.stardate = 35153;
    ui_message("HELM: ", "FIVE");

    /* Top box should now be the one stamped 3515.0, not 3514.9. */
    check(cell[MSG_Y][x + 3] == 53 && cell[MSG_Y][x + 5] == 48,
          "the oldest box scrolls off and 3515.0 is now on top");
    check(cell[MSG_Y + 9][x + 5] == 51,
          "the newest box is at the bottom, stamped 3515.3");
}

/* The badge fills its panel and nothing more. It is the one panel whose
   contents are drawn by position rather than by a loop, so an edit to the
   layout is exactly what would push the crest through a border. */
static void test_badge_stays_inside(void) {
    const Panel *p = &panels[P_BADGE];
    unsigned char x, y;
    int escaped = 0, painted = 0;

    screen_reset();
    ui_draw_badge();

    for (y = 0; y < VDC_ROWS; y++)
        for (x = 0; x < VDC_COLS; x++) {
            if (cell[y][x] == SENTINEL) continue;
            painted++;
            if (x > p->x && x < p->x + p->w - 1 &&
                y > p->y && y < p->y + p->h - 1) continue;
            escaped++;
        }

    check(off_screen == 0, "badge: no write leaves the 80x25 grid");
    check(escaped == 0,    "badge: no write leaves the panel interior");
    check(painted > 0,     "badge: something is actually drawn");
}

int main(void) {
    trek_new_game(3, 12345);

    test_panels_disjoint();
    test_panel_holds_twelve();
    test_stays_inside();
    test_placement();
    test_bar_glyph();
    test_bar_length();
    test_colours();
    test_repair_report();
    test_independent();

    test_badge_stays_inside();
    test_lasers_stay_inside();
    test_laser_gauges();
    test_heat_colours();

    test_messages_stay_inside();
    test_message_stack();
    test_message_stamps();
    test_message_scroll();

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("console panels: all checks passed\n");
    return 0;
}
