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
#include <stdlib.h>

#include "../src/vdc.h"
#include "../src/layout.h"
#include "../src/strdata.h"   /* the ids; the text comes from strings.txt via pool_init */
#include "../../core/hof.h"
#include "../../core/storage.h"
#include "../src/ui.h"
#include "../src/egavdc.h"
#include "../src/input.h"
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

/* Stubs for the parts of the port the panel tests do not exercise. ui.c now
   references both because the setup screen lives there. */
uint16_t kb_entropy = 0;
void scr_clear(void) { screen_reset(); }

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

/* The string pool, natively.
 *
 * ui.c draws with S(id) now -- the prose lives in bank 1 on the real machine
 * (core/strpool.h). The tests must still be able to assert what a panel
 * actually printed, so this reads the SAME committed list the generator uses
 * and hands back the real text. A stub returning "" would turn every string
 * assertion below into a tautology. */
#include "../../core/strpool.h"

static char  pool_buf[16384];
static char *pool[512];
static int   pool_n = 0;

static void pool_init(void) {
    FILE *f = fopen("src/strings.txt", "r");
    char line[256];
    size_t used = 0;

    if (!f) { printf("FAIL: cannot open src/strings.txt\n"); exit(1); }
    while (fgets(line, sizeof line, f)) {
        char *tab;
        size_t n;
        if (line[0] == '#' || line[0] == '\n') continue;
        tab = strchr(line, '\t');
        if (!tab) continue;
        n = strlen(tab + 1);
        while (n && (tab[n] == '\n' || tab[n] == '\r')) n--;
        memcpy(pool_buf + used, tab + 1, n);
        pool_buf[used + n] = 0;
        pool[atoi(line)] = pool_buf + used;
        used += n + 1;
        if (atoi(line) + 1 > pool_n) pool_n = atoi(line) + 1;
    }
    fclose(f);
}

/* StrId, NOT uint8_t. This said uint8_t until 2026-08-29 and had not compiled
   since the pool passed 256 strings on 2026-08-27 and core/strpool.h widened
   the type. Two days of `make test` failing at the FIRST of its three
   binaries, so test_panels and test_sid never ran at all -- and nothing said
   so, because the failure was a compiler error scrolling past above a prompt
   nobody read. CHECK THE EXIT STATUS. */
const char *S(StrId id) {
    return (id < pool_n && pool[id]) ? pool[id] : "";
}
uint8_t str_load(void) { return 1; }

/* A fake disk.
 *
 * ui_hall_of_fame() reads TREK.SCR, offers the finished game to it and writes
 * it back, so the tests need somewhere for that to happen. An in-memory file
 * is not just a stub to satisfy the linker: it makes the whole
 * read-offer-write-display path testable on the build machine, which is where
 * the place-major record layout would otherwise only be checkable by playing
 * a game to the end on a real drive. */
#define COL_GRID_TEST EGA_TO_VDC(EGA_DKGRAY)

static unsigned char disk[512];
static uint16_t      disk_len = 0;
static int           disk_present = 1;   /* 0 simulates a missing file */
static int           disk_writes = 0;

uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got) {
    (void)name;
    *got = 0;
    if (!disk_present) return STOR_NOTFOUND;
    if (disk_len > max) return STOR_ERROR;
    memcpy(buf, disk, disk_len);
    *got = disk_len;
    return STOR_OK;
}

uint8_t plat_write_all(const char *name, const void *buf, uint16_t len) {
    (void)name;
    if (len > sizeof disk) return STOR_ERROR;
    memcpy(disk, buf, len);
    disk_len = len;
    disk_present = 1;
    disk_writes++;
    return STOR_OK;
}

/* The streaming seam. NOTFOUND by default -- most tests must not find a file
   -- but test_briefing_pages() turns it on and serves the REAL briefing.txt,
   so the page loop is exercised against the bytes that actually ship. */
static FILE *brief_fp   = 0;
static int   brief_mode = 0;

uint8_t plat_open(const char *name) {
    if (!brief_mode) { (void)name; return STOR_NOTFOUND; }
    brief_fp = fopen("src/briefing.txt", "rb");
    return brief_fp ? STOR_OK : STOR_NOTFOUND;
}
uint16_t plat_read(void *buf, uint16_t len) {
    if (!brief_fp) { (void)buf; (void)len; return 0; }
    return (uint16_t)fread(buf, 1, len, brief_fp);
}
void plat_close(void) {
    if (brief_fp) { fclose(brief_fp); brief_fp = 0; }
}

/* ui.c's dialog code references this; nothing here calls it. */
void vdc_reg_write(unsigned char r, unsigned char v) { (void)r; (void)v; }

/* A fake 16K of VDC RAM.
 *
 * Not a stub that swallows writes -- a real array with a real auto-incrementing
 * update address, because the message log lives out there now and a stub would
 * make every log assertion below pass vacuously. The auto-increment is the
 * part worth modelling: log_append and log_fetch_slot both set the address
 * once and then stream, so an increment that did not happen would corrupt
 * every entry after the first and a swallowing stub would never show it. */
static unsigned char vdc_ram[16384];
static unsigned int  vdc_addr = 0;

void vdc_set_address(unsigned int addr) { vdc_addr = addr & 0x3FFF; }

void vdc_data_write(unsigned char v) {
    vdc_ram[vdc_addr] = v;
    vdc_addr = (vdc_addr + 1) & 0x3FFF;
}

unsigned char vdc_data_read(void) {
    unsigned char v = vdc_ram[vdc_addr];
    vdc_addr = (vdc_addr + 1) & 0x3FFF;
    return v;
}

/* Scripted keyboard. The default of RETURN is what lets the blocking waits in
   ui_title() and the repair report fall straight through; a test that has to
   answer a specific prompt points kb_script at the keys it wants first. */
static const char *kb_script = 0;

/* ui_messages_view() redraws the whole console on its way out, so by the time
   it returns there is nothing of the overlay left to assert on. This copies
   the screen the moment the viewer blocks for a key -- which is exactly the
   state a player would be looking at. */
static unsigned char snap[VDC_ROWS][VDC_COLS];
static int snap_armed = 0;

/* The briefing blocks once per page and clears the screen straight after, so
   the ONLY moment its last page can be inspected is while it is waiting.
   wait_snap keeps the most recent one; wait_count says how many there were. */
static unsigned char wait_snap[VDC_ROWS][VDC_COLS];
static int wait_count = 0;

char kb_waitkey(void) {
    if (snap_armed) { memcpy(snap, cell, sizeof snap); snap_armed = 0; }
    wait_count++;
    memcpy(wait_snap, cell, sizeof wait_snap);
    if (kb_script && *kb_script) return *kb_script++;
    return KB_RETURN;
}

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

/* Reads a run of cells back as text, so a test can assert what a screen
   actually printed rather than that it printed something. */
static void row_text(unsigned char y, unsigned char x, unsigned char n, char *out) {
    unsigned char i;
    for (i = 0; i < n; i++) out[i] = (char)cell[y][x + i];
    out[n] = 0;
}


/* Is `needle` anywhere on the screen? MSGS scrolls, so a test that asserted a
   fixed row would be asserting the scroll offset by accident. */
static int in_snapshot(const char *needle) {
    unsigned char y, x, i, n = (unsigned char)strlen(needle);
    char buf[VDC_COLS + 1];

    for (y = 0; y < VDC_ROWS; y++) {
        for (x = 0; x + n <= VDC_COLS; x++) {
            for (i = 0; i < n; i++) buf[i] = (char)snap[y][x + i];
            buf[n] = 0;
            if (strcmp(buf, needle) == 0) return 1;
        }
    }
    return 0;
}

/* Runs the viewer, capturing what it put on screen. ESC because that is the
   only key it accepts -- see ui.c. A test that forgot it would hang. */
static void view_messages(void) {
    snap_armed = 1;
    kb_script = "\x1b";
    ui_messages_view();
    kb_script = 0;
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

/* LIFE SUPPORT IS HELD AT 100, and that is the whole point of this helper.
   Since 2026-08-27 ui_draw_systems() swaps SYSTEMS STATUS for the RESERVE
   panel the moment sys[SYS_LIFE] is not perfect -- faithfully, the original
   does the same. These tests are about the bar gauge, so a set_all() that
   damaged life support along with everything else was quietly measuring a
   panel with no bars in it: every case except 100% drew RESERVE, and the one
   that passed passed because it was the only one still drawing a gauge.

   Nobody saw it for two days because the file did not COMPILE (see the S()
   stub above), so all three of `make test`'s binaries died at the first.
   Two blind spots stacked: a suite that could not run, and inside it a fixture
   that would have been testing the wrong screen if it had. */
static void set_all(unsigned char pct) {
    unsigned char i;
    for (i = 0; i < SYS_COUNT; i++) ship.sys[i] = pct;
    ship.sys[SYS_LIFE] = 100;
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

/* THE PANEL SWAP ITSELF, which is the discriminator the gauge tests cannot be.
 *
 * Every fixture above now pins sys[SYS_LIFE] at 100 so that SYSTEMS STATUS is
 * what gets drawn -- which means none of them can notice if the swap stops
 * happening, or starts happening at the wrong threshold. This one runs the
 * other way: it asserts that damaged life support draws RESERVE and NOT the
 * twelve bars, so the two tests fail on opposite mistakes.
 *
 * LIFE_PANEL_BELOW is 100 and read from the binary -- `cmp [0x235E], 0x64`
 * at 0x01FDC9 -- so 99 must swap and 100 must not. The boundary is asserted
 * against the literals, not against the constant, so a wrong LIFE_PANEL_BELOW
 * cannot satisfy this by agreeing with itself. */
static void test_reserve_panel_swap(void) {
    const Panel *p = &panels[P_SYSTEMS];
    unsigned char x = (unsigned char)(p->x + 1);
    unsigned char y = (unsigned char)(p->y + 1);
    char got[VDC_COLS + 1];

    /* Perfect: the bars are there, and the panel is NOT headed LIFE SUPPORT.
       Both halves matter -- a reserve panel drawn by accident would still put
       something in row 0, so the assertion has to name what. */
    set_all(100);
    screen_reset();
    ui_draw_systems();
    check(bar_len(0) == SYS_BAR, "life at 100 draws the twelve bars");
    row_text(y, x, (unsigned char)strlen(S(S_126)), got);
    check(strcmp(got, S(S_126)) != 0, "life at 100 is not the reserve panel");

    /* One point of damage, and the whole panel is a different panel. Its two
       labels are the ones read off the original at cs:0x403A and cs:0x4047. */
    set_all(100);
    ship.sys[SYS_LIFE] = 99;
    screen_reset();
    ui_draw_systems();
    row_text(y, x, (unsigned char)strlen(S(S_126)), got);
    check(strcmp(got, S(S_126)) == 0,
          "life at 99 swaps SYSTEMS STATUS for RESERVE");
    row_text((unsigned char)(y + 1), x, (unsigned char)strlen(S(S_253)), got);
    check(strcmp(got, S(S_253)) == 0, "and the reserve panel says RESERVE, DAYS");
}

/* Each system is drawn from its own slot, not from a shared one. */
static void test_independent(void) {
    unsigned char i;

    set_all(100);
    for (i = 0; i < SYS_COUNT; i++)
        if (i != SYS_LIFE) ship.sys[i] = (unsigned char)(i * 9);
    screen_reset();
    ui_draw_systems();

    for (i = 0; i < SYS_COUNT; i++) {
        unsigned char pct = (i == SYS_LIFE) ? 100 : (unsigned char)(i * 9);
        unsigned char want = (unsigned char)((pct * SYS_BAR + 50) / 100);
        char msg[64];
        if (want == 0 && pct != 0) want = 1;
        sprintf(msg, "system %u at %u%% draws %u cells", i, pct, want);
        check(bar_len(i) == want, msg);
    }
}



/* The setup screen's only real invariant: the core's xorshift is dead at zero,
   so a seed of zero would hand every such sitting the same galaxy -- which is
   the exact bug the screen exists to fix. */
static void test_setup_seed(void) {
    unsigned char lv;
    uint16_t a, b;

    for (lv = 1; lv <= 5; lv++) {
        char msg[64];
        sprintf(msg, "level %u never seeds zero", lv);
        /* The entropy that would cancel this level's mix, if any. */
        check(setup_seed((uint16_t)(lv * 2749u), lv) != 0, msg);
    }
    check(setup_seed(0, 3) != 0, "zero entropy still seeds non-zero");

    a = setup_seed(1234, 3);
    b = setup_seed(1235, 3);
    check(a != b, "one more poll pass gives a different galaxy");

    a = setup_seed(1234, 2);
    b = setup_seed(1234, 4);
    check(a != b, "same timing at a different level gives a different galaxy");
}

/* The title screen, rendered into the fake VDC. kb_waitkey() is stubbed to
   return RETURN, so the blocking wait falls straight through. */
static void test_title_screen(void) {
    screen_reset();
    ui_title();

    /* Both plates must be complete boxes. The bottom edge is the one that went
       missing on screen and is invisible in a screenshot at this size. */
    check(cell[16][4]  == G_TL, "title: left plate top left");
    check(cell[16][27] == G_TR, "title: left plate top right");
    check(cell[22][4]  == G_BL, "title: left plate bottom left");
    check(cell[22][27] == G_BR, "title: left plate bottom right");
    check(cell[22][10] == G_HLINE, "title: left plate bottom edge");
    check(cell[22][55] == G_HLINE, "title: credit plate bottom edge");

    check(off_screen == 0, "title: nothing drawn off screen");

    /* And the credit is on it, because that is the point of the screen. */
    {
        char buf[40];
        row_text(17, 34, 37, buf);
        check(strstr(buf, "NELS ANDERSON") != NULL,
              "title: Nels Anderson is credited");
    }
}

/* ------------------------------------------------------ the ship's word */

/* EGA Trek calls them Mongols, everywhere: the STATUS panel reads "Mongols:",
   the enemy classes are Mongol Commander and Mongol Battleship, and the score
   sheet counts "Mongols killed". This port said ENEMIES on the status panel,
   which is the ancestor's word and nobody else's. Asserted rather than just
   fixed, because it is the kind of thing that drifts back. */
static void test_calls_them_mongols(void) {
    const Panel *p = &panels[P_STATUS];
    char buf[16];
    unsigned char y;

    screen_reset();
    set_all(100);
    ui_draw_status();

    for (y = (unsigned char)(p->y + 1); y < (unsigned char)(p->y + p->h - 1); y++) {
        row_text(y, (unsigned char)(p->x + 2), 7, buf);   /* labels sit at x+2 */
        if (strcmp(buf, "MONGOLS") == 0) break;
    }
    check(y < (unsigned char)(p->y + p->h - 1),
          "status panel calls them MONGOLS, as the original does");
}

/* --------------------------------------------------- quit confirmation */

/* MEASURED: the original answers Q with "Quit <Y/N>?" on the COMMAND line
   rather than quitting on the keystroke. This checks the prompt lands there,
   that Y and N mean what they say, and that the line is left clean -- a stale
   "QUIT <Y/N>?" sitting under the next command would be worse than no prompt
   at all. */
static void test_quit_confirm(void) {
    char buf[24];
    char yes[2], no[3];
    const Panel *p = &panels[P_COMMAND];
    unsigned char x0 = (unsigned char)(p->x + 2);
    unsigned char y  = (unsigned char)(p->y + 1);

    yes[0] = KB_Y; yes[1] = 0;
    no[0] = KB_RETURN; no[1] = KB_N; no[2] = 0;   /* RETURN must not answer */

    screen_reset();
    kb_script = yes;
    check(ui_confirm("QUIT <Y/N>?") == 1, "quit: Y confirms");

    screen_reset();
    kb_script = no;
    check(ui_confirm("QUIT <Y/N>?") == 0,
          "quit: RETURN is ignored and N declines");

    /* Drawn where the command line is, and cleared afterwards. */
    screen_reset();
    kb_script = yes;
    ui_confirm("QUIT <Y/N>?");
    row_text(y, x0, 11, buf);
    check(strcmp(buf, "           ") == 0,
          "quit: the prompt does not outlive the answer");
    check(off_screen == 0, "quit: nothing drawn off screen");

    kb_script = 0;
}

/* ------------------------------------------------------- play again */

/* The prompt is MEASURED off the original -- columns 19..35, rows 5..9. What
   this guards is the box being whole and the answer mapping to the right key.
   The bottom edge gets its own check for the reason the title screen's does:
   cl65 -O once dropped exactly that row while the native build was fine, so
   the corner alone is not enough. */
static void test_play_again(void) {
    char buf[16];
    char buf2[32];
    char yes[2], no[2], junk[3];

    yes[0] = KB_Y; yes[1] = 0;
    no[0]  = KB_N; no[1]  = 0;

    screen_reset();
    kb_script = yes;
    check(ui_play_again() == 1, "play again: Y means yes");

    check(cell[5][19] == G_TL, "play again: box top left");
    check(cell[5][35] == G_TR, "play again: box top right");
    check(cell[9][19] == G_BL, "play again: box bottom left");
    check(cell[9][35] == G_BR, "play again: box bottom right");
    check(cell[9][27] == G_HLINE, "play again: box bottom edge");
    check(off_screen == 0, "play again: nothing drawn off screen");

    row_text(6, 22, 11, buf);
    check(strcmp(buf, "PLAY AGAIN?") == 0, "play again: the original's wording");
    row_text(8, 21, 5, buf);
    check(strcmp(buf, "[YES]") == 0, "play again: YES button where measured");
    row_text(8, 29, 4, buf);
    check(strcmp(buf, "[NO]") == 0, "play again: NO button where measured");

    /* The hall of fame's own prompt must not survive under the dialog: RETURN
       does nothing here, so leaving it on screen is an instruction to press a
       dead key. Drawn for real first -- checking a blank row on a blank screen
       would pass without the clearing code existing at all. */
    {
        char seq[3];
        seq[0] = KB_RETURN;   /* dismisses the hall of fame */
        seq[1] = KB_Y;        /* answers the prompt */
        seq[2] = 0;
        screen_reset();
        kb_script = seq;
        ui_hall_of_fame("KIRK", 3, -300);
        row_text(22, 29, 22, buf2);
        check(strcmp(buf2, "HIT RETURN TO CONTINUE") == 0,
              "play again: the hall of fame really does print that prompt");
        check(ui_play_again() == 1, "play again: still answers after the hall of fame");
        row_text(22, 29, 22, buf2);
        check(strcmp(buf2, "                      ") == 0,
              "play again: the hall of fame's RETURN prompt is cleared");
    }

    screen_reset();
    kb_script = no;
    check(ui_play_again() == 0, "play again: N means no");

    /* RETURN is deliberately NOT a shortcut here, and neither is anything
       else -- the setup screen reads a bare RETURN as no, and one key meaning
       opposite things on two screens is how a session ends by accident. */
    junk[0] = KB_RETURN; junk[1] = KB_Y; junk[2] = 0;
    screen_reset();
    kb_script = junk;
    check(ui_play_again() == 1, "play again: RETURN is ignored, not an answer");

    kb_script = 0;
}

/* ------------------------------------------------ state of repair report */

static void test_repair_report(void) {
    char buf[32];
    unsigned char i;

    set_all(100);
    ship.sys[SYS_CONVERTER] = 40;    /* 60 points to mend */
    ship.sys[SYS_LASERS]    = 65;
    screen_reset();
    ui_repair_report();

    /* Ours is points/rate: 60 points at the docked 50 a stardate is 1.2.
       The ORIGINAL prints 1.3 here, and 2026-08-24 established that this is
       not a rate error -- the four rates are measured exactly, on real repair,
       and the dialog is a separate ESTIMATE running about a tenth high.
       ceil((points + 1) / rate) fits all ten docked samples and eight of ten
       undocked: close enough to be suspicious, not close enough to ship.
       Until it is pinned we print the honest quotient. See MEASURED.md,
       "Run 1 of the measurement plan". */
    row_text((unsigned char)(REP_Y + 4), (unsigned char)(REP_X + 25), 3, buf);
    check(strcmp(buf, "1.2") == 0, "40% mends in 1.2 stardates docked");
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

    /* The box itself: all four corners and a mid-span of each edge. Drawn by
       the shared box() helper, so this covers the title screen's boxes too. */
    check(cell[REP_Y][REP_X]                 == G_TL, "box: top left corner");
    check(cell[REP_Y][REP_X + REP_W - 1]     == G_TR, "box: top right corner");
    check(cell[REP_Y + REP_H - 1][REP_X]     == G_BL, "box: bottom left corner");
    check(cell[REP_Y + REP_H - 1][REP_X + REP_W - 1] == G_BR, "box: bottom right corner");
    check(cell[REP_Y][REP_X + 5]             == G_HLINE, "box: top edge");
    check(cell[REP_Y + REP_H - 1][REP_X + 5] == G_HLINE, "box: bottom edge");
    check(cell[REP_Y + 3][REP_X]             == G_VLINE, "box: left edge");

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

/* BOTH GAUGES CHANGED 2026-08-26 and these tests changed with them.
 *
 * EFF used to draw ship.laser_eff, which is set to 100 at the start of a game
 * and never altered by anything, so the bar read full on a wrecked laser
 * bank. It draws trek_laser_eff() now -- the Lasers repair percentage, which
 * is MEASURED to be exactly what the damage uses. So the test drives
 * ship.sys[SYS_LASERS], not ship.laser_eff.
 *
 * TEMP used to draw the raw fired energy against a 0..1500 scale. The
 * original's word is capped at 100 and drawn TIMES TEN against that scale, so
 * a fully heated bank fills five of the eight cells and the bar can never
 * reach its own 1500. The expectations below are that arithmetic. */
static void test_laser_gauges(void) {
    static const struct { unsigned char las; uint16_t heat;
                          unsigned char ec, hc; } t[] = {
        { 100,   0, 8, 0 },
        {  50,  50, 4, 3 },   /* heat 50 draws 500 of 1500 */
        {   1,   1, 1, 1 },   /* alive at all must never draw empty */
        {   0,   0, 0, 0 },
        {   7, 100, 1, 5 },   /* the CAP fills five cells, not eight */
        { 100, 200, 8, 8 },   /* past the cap only a poke can reach; pins */
    };
    unsigned char i;
    char msg[80];

    ship.laser_eff = 100;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        ship.sys[SYS_LASERS] = t[i].las;
        ship.laser_heat = t[i].heat;
        screen_reset();
        ui_draw_lasers();
        sprintf(msg, "lasers at %u%% draws %u of 8", t[i].las, t[i].ec);
        check(las_bar_len(0) == t[i].ec, msg);
        sprintf(msg, "heat word %u draws %u of 8", t[i].heat, t[i].hc);
        check(las_bar_len(1) == t[i].hc, msg);
    }
    ship.sys[SYS_LASERS] = 100;
}

/* The colour bands are on the DRAWN value, so these are heat words times ten.
   Red is unreachable in play now -- the game caps the word at 100, which
   draws at 1000 -- and it is kept and tested anyway: a bar in the red would
   mean the cap had failed, which is worth being able to see. */
static void test_heat_colours(void) {
    const Panel *p = &panels[P_LASERS];
    unsigned char x = (unsigned char)(p->x + 1 + 5);
    unsigned char y = (unsigned char)(p->y + 2);
    static const struct { uint16_t heat; unsigned char ega; const char *what; } t[] = {
        {  99, EGA_LTGREEN, "word 99 draws 990 and is green"  },
        { 100, EGA_YELLOW,  "the cap draws 1000 and is amber" },
        { 150, EGA_YELLOW,  "1500 is still amber -- the threshold is not crossed" },
        { 151, EGA_LTRED,   "past 1500 is red, which only a broken cap reaches" },
    };
    unsigned char i;

    ship.laser_eff = 100;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        ship.laser_heat = t[i].heat;
        screen_reset();
        ui_draw_lasers();
        check(attr[y][x] == EGA_TO_VDC(t[i].ega), t[i].what);
    }
    ship.laser_heat = 0;
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

/* NOTHING THE LOG VIEWER DRAWS MAY LEAVE ITS BOX.
 *
 * msgv_draw() had no bound at all until 2026-08-29: it wrote view_dept then
 * view_text at the box's left edge, and the log holds up to LOG_DEPT +
 * LOG_TEXT = 53 characters against a MSGV_W - 2 = 39 interior. A long message
 * drew across the right border and over the console behind it. Jamie saw it
 * on the VDC; no test could have, because test_messages_stay_inside() above
 * exercises ui_message() -- the small panel -- and never opens the viewer.
 *
 * THE FIXTURE MUST USE AN OVER-LONG MESSAGE. A test written with realistic
 * text passes against the unclamped draw, which is how this survived: the
 * composed lines were all short enough until one was not. */
static void test_viewer_stays_inside(void) {
    unsigned char x, y;
    int escaped = 0;

    screen_reset();
    ui_clear_messages();
    ship.stardate = 35149;
    ui_message("COMMUNICATIONS: ",
               "A MESSAGE FAR LONGER THAN THIS BOX CAN EVER HOLD");
    ui_message("DAMAGE: ", "SHORT ONE");

    /* A CLEAN SCREEN FIRST, so what is counted is what the VIEWER drew. The
       ui_message() calls above painted the small message panel, which is
       outside this box and would count as an escape. The log itself lives in
       VDC memory, not in the fake screen, so clearing the screen does not
       clear the log. */
    screen_reset();

    /* SNAPSHOT, not the final screen. ui_messages_view() calls ui_draw_all()
       on its way out to restore the console, which writes over the whole
       grid -- so checking `cell` afterwards checks the console and not the
       viewer. snap_armed makes kb_waitkey() copy the screen as the player
       sees it, with the box still up. */
    snap_armed = 1;
    kb_script = "\033";              /* ESC: draw once, then leave */
    ui_messages_view();

    for (y = 0; y < VDC_ROWS; y++)
        for (x = 0; x < VDC_COLS; x++) {
            if (snap[y][x] == SENTINEL) continue;
            if (x >= MSGV_X && x < MSGV_X + MSGV_W &&
                y >= MSGV_Y && y < MSGV_Y + MSGV_H) continue;
            escaped++;
        }

    check(off_screen == 0, "the log viewer writes nothing off the grid");
    check(escaped == 0, "and nothing outside its own box, however long the text");
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

/* A#, against the capture of 2026-08-23. Every assertion here is a measured
   behaviour, not a design choice, and three of them are the opposite of what
   the natural implementation does. */
static void test_ack(void) {
    char buf[16];

    screen_reset();
    ui_clear_messages();
    ship.stardate = 35149;
    ui_message("COMMUNICATIONS: ", "ONE");
    ui_message("DAMAGE: ", "TWO");
    ui_message("HELM: ", "THREE");

    /* A1 takes the FIRST box and the rest move up. */
    ui_ack(1);
    row_text((unsigned char)(MSG_Y + 1), (unsigned char)(MSG_X + 1), 8, buf);
    check(strcmp(buf, "DAMAGE: ") == 0, "A1 removes the first box");
    row_text((unsigned char)(MSG_Y + 4), (unsigned char)(MSG_X + 1), 6, buf);
    check(strcmp(buf, "HELM: ") == 0, "and the rest move up");

    /* Out of range does nothing at all, and says nothing. */
    ui_ack(9);
    row_text((unsigned char)(MSG_Y + 1), (unsigned char)(MSG_X + 1), 8, buf);
    check(strcmp(buf, "DAMAGE: ") == 0, "A9 is a silent no-op");

    /* Bare A clears the lot. */
    ui_ack(0);
    check(cell[MSG_Y + 1][MSG_X + 1] == 32, "bare A clears every box");
}

/* The panel is a queue and the log is a record: acknowledging empties the
   first without touching the second. This is the one that would have been
   got wrong -- the obvious implementation deletes. */
static void test_log_outlives_the_panel(void) {
    screen_reset();
    ui_clear_messages();
    ship.stardate = 35149;
    ui_message("COMMUNICATIONS: ", "KEEPME");
    ui_message("DAMAGE: ", "TWO");
    ui_ack(0);                       /* clear the panel entirely */

    view_messages();
    check(in_snapshot("KEEPME"),
          "an acknowledged message is still in the MSGS log");
}

/* MEASURED: MSGS opens scrolled to the BOTTOM, newest visible, and the log
   goes deeper than the panel's four. Eight messages means the first ones are
   long gone from the panel and must still be reachable here. */
static void test_msgs_opens_at_the_bottom(void) {
    unsigned char i;
    char text[8];

    screen_reset();
    ui_clear_messages();
    ship.stardate = 35149;
    for (i = 0; i < 8; i++) {
        sprintf(text, "MSG%u", i);
        ui_message("HELM: ", text);
    }

    view_messages();
    check(in_snapshot("MSG7"),  "MSGS opens showing the newest message");
    check(!in_snapshot("MSG0"), "and the oldest is scrolled off the top");
}

/* The hall of fame, end to end: read the file, offer the game, write it back,
 * and put it on screen in the right row.
 *
 * The row placement is the part worth asserting. MEASURED 2026-08-23: two
 * entries per rank, first place bright on the rank's own row and second place
 * dim on the row below -- which is why ten records fit in five rank rows, and
 * why the file is place-major rather than rank-major. Getting that wrong puts
 * an Admiral's score on the Captain's line and nothing else complains. */
static void test_hall_of_fame_persists(void) {
    char buf[32];

    /* No file at all. The screen must still draw. */
    disk_present = 0; disk_len = 0; disk_writes = 0;
    screen_reset();
    ui_hall_of_fame("", 3, 0);
    check(disk_writes == 0, "an empty game writes nothing");
    row_text(14, 16, 5, buf);
    check(strcmp(buf, ".....") == 0, "a missing file draws a blank table");

    /* A Captain scores. Level 3 is the third rank row, y = 10 + 2*2. */
    screen_reset();
    ui_hall_of_fame("PICARD", 3, 500);
    check(disk_writes == 1, "a qualifying score is written back");
    row_text(14, 16, 6, buf);
    check(strcmp(buf, "PICARD") == 0, "first place sits on the rank's own row");

    /* A second Captain, scoring less, takes the dim row below. */
    screen_reset();
    ui_hall_of_fame("KIRK", 3, 200);
    row_text(14, 16, 6, buf);
    check(strcmp(buf, "PICARD") == 0, "the better score keeps first place");
    row_text(15, 16, 4, buf);
    check(strcmp(buf, "KIRK") == 0, "and the lesser one takes the row below");
    check(attr[15][16] == COL_GRID_TEST, "second place is drawn dim");

    /* Another rank is a separate table that happens to share a file. */
    screen_reset();
    ui_hall_of_fame("ADAMA", 5, 50);
    row_text(18, 16, 5, buf);
    check(strcmp(buf, "ADAMA") == 0, "an Admiral lands on the Admiral row");
    row_text(14, 16, 6, buf);
    check(strcmp(buf, "PICARD") == 0, "without disturbing the Captain's");

    /* And it survives a reload, which is the whole point of the file. */
    screen_reset();
    ui_hall_of_fame("", 1, 0);
    row_text(14, 16, 6, buf);
    check(strcmp(buf, "PICARD") == 0, "the table survives a round trip to disk");
}


/* THE BRIEFING WAITS ON EVERY PAGE, THE LAST ONE INCLUDED.
 *
 * Twelve pages are separated by ELEVEN form feeds, and the wait used to live
 * inside the form-feed branch alone. Page 12 therefore fell out of the read
 * loop and the screen cleared under the reader (Jamie, 2026-09-02). The page
 * count is derived from the shipping file, not restated, so adding a page to
 * briefing.txt cannot quietly leave this test asserting the old number.
 *
 * Two assertions, and the second is the one that matters: a loop that waited
 * twelve times but did it AFTER clearing the screen would pass the first. */
static void test_briefing_pages(void) {
    int pages = 1, ch, r, found = 0;
    FILE *f = fopen("src/briefing.txt", "rb");

    if (!f) { printf("FAIL: cannot open src/briefing.txt\n"); failures++; return; }
    while ((ch = fgetc(f)) != EOF) if (ch == '\f') pages++;
    fclose(f);

    screen_reset();
    brief_mode = 1;
    wait_count = 0;
    ui_briefing();
    brief_mode = 0;

    {
        char msg[96];
        sprintf(msg, "briefing waits once per page (%d waits, %d pages)",
                wait_count, pages);
        check(wait_count == pages, msg);
    }

    /* The last wait happened with page 12 still drawn. Its closing line is the
       thing a reader is left looking at. */
    for (r = 0; r < VDC_ROWS && !found; r++) {
        char line[VDC_COLS + 1];
        int c;
        for (c = 0; c < VDC_COLS; c++) {
            unsigned char v = wait_snap[r][c];
            line[c] = (v == SENTINEL) ? ' ' : (char)v;
        }
        line[VDC_COLS] = 0;
        if (strstr(line, "Good hunting.")) found = 1;
    }
    check(found, "the last page is still on screen when the briefing waits");

    /* Q QUITS, AND QUITTING ASKS FOR NOTHING MORE. The end-of-file footer
       added above must not fire on the way out of an abandoned briefing --
       that would make Q take two presses. */
    screen_reset();
    brief_mode = 1;
    wait_count = 0;
    kb_script = "Q";
    ui_briefing();
    kb_script = 0;
    brief_mode = 0;
    check(wait_count == 1, "Q on page 1 ends the briefing with one press");
}

int main(void) {
    pool_init();
    trek_new_game(3, 12345);

    test_panels_disjoint();
    test_panel_holds_twelve();
    test_stays_inside();
    test_placement();
    test_bar_glyph();
    test_bar_length();
    test_colours();
    test_repair_report();
    test_setup_seed();
    test_title_screen();
    test_play_again();
    test_quit_confirm();
    test_calls_them_mongols();
    test_independent();
    test_reserve_panel_swap();

    test_badge_stays_inside();
    test_lasers_stay_inside();
    test_laser_gauges();
    test_heat_colours();

    test_messages_stay_inside();
    test_message_stack();
    test_message_stamps();
    test_message_scroll();
    test_viewer_stays_inside();
    test_ack();
    test_log_outlives_the_panel();
    test_msgs_opens_at_the_bottom();
    test_hall_of_fame_persists();
    test_briefing_pages();

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("console panels: all checks passed\n");
    return 0;
}
