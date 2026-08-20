/* Host-side test for the SYSTEMS STATUS panel in src/ui.c.
 *
 * Runs natively (cc, not cl65) against a fake VDC that records every cell
 * written. The point is not the arithmetic -- it is the bounds. The real VDC
 * auto-increments its update address, so a routine that writes one cell past
 * a panel's right edge does not clip: it wraps onto the next row and silently
 * overwrites whichever panel is there. That failure mode has already cost
 * this port one bug (message truncation in COMMUNICATIONS), and it cannot be
 * seen in a screenshot of the panel that caused it.
 *
 *     cc -Wall -o build/test_systems test/test_systems.c src/ui.c \
 *        src/layout.c ../core/trek.c && ./build/test_systems
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
    check(clash == 0, "no two panel interiors overlap");
}

/* The panel has to be tall enough for what it draws. Six interior rows and
   two columns is exactly twelve systems; one row fewer and systems fall off
   the bottom border without any visible complaint. */
static void test_panel_holds_twelve(void) {
    const Panel *p = &panels[P_SYSTEMS];
    check(p->h - 2 >= 6, "SYSTEMS STATUS has six interior rows");
    check(p->w - 2 >= 24, "SYSTEMS STATUS has room for two 11-wide columns");
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
        x = (unsigned char)(p->x + 1 + (i / 6) * 13);
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
    unsigned char x = (unsigned char)(p->x + 1 + (sys / 6) * 13 + 4);
    unsigned char y = (unsigned char)(p->y + 1 + (sys % 6));
    unsigned char n = 0, k;
    for (k = 0; k < 7; k++)
        if (attr[y][x + k] != EGA_TO_VDC(EGA_DKGRAY)) n++;
    return n;
}

static void test_bar_length(void) {
    static const struct { unsigned char pct, cells; } t[] = {
        {   0, 0 }, {   1, 1 }, {   7, 1 }, {   8, 1 },
        {  14, 1 }, {  40, 3 }, {  50, 4 }, {  70, 5 },
        {  90, 6 }, {  93, 7 }, { 100, 7 },
    };
    unsigned char i;
    char msg[64];

    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        set_all(t[i].pct);
        screen_reset();
        ui_draw_systems();
        sprintf(msg, "%u%% draws %u of 7 cells", t[i].pct, t[i].cells);
        check(bar_len(0) == t[i].cells, msg);
    }
}

/* The three colours that were read off the original by poking its repair
   array: 100 full green, 70 short yellow, 40 short red. */
static void test_colours(void) {
    const Panel *p = &panels[P_SYSTEMS];
    unsigned char x = (unsigned char)(p->x + 1 + 4);
    unsigned char y = (unsigned char)(p->y + 1);

    set_all(100); screen_reset(); ui_draw_systems();
    check(attr[y][x] == EGA_TO_VDC(EGA_LTGREEN), "100% is green");

    set_all(70); screen_reset(); ui_draw_systems();
    check(attr[y][x] == EGA_TO_VDC(EGA_YELLOW), "70% is yellow");

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
        unsigned char want = (unsigned char)((pct * 7 + 50) / 100);
        char msg[64];
        if (want == 0 && pct != 0) want = 1;
        sprintf(msg, "system %u at %u%% draws %u cells", i, pct, want);
        check(bar_len(i) == want, msg);
    }
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
    test_independent();

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("systems status panel: all checks passed\n");
    return 0;
}
