#include "vdc.h"
#include "layout.h"
#include "egavdc.h"

/* Column bands: 0..26 | 27..45 | 46..79   (27 + 19 + 34 = 80)
   Row bands:    0..12 | 13..20 | 21..24   (13 +  8 +  4 = 25)
   The left middle band splits again into LASERS over COMMAND. */
const Panel panels[PANEL_COUNT] = {
    {  0,  0, 27, 13, "SHORT RANGE SCAN" },
    { 27,  0, 19, 13, "STATUS" },
    { 46,  0, 34, 13, "CHART OF KNOWN GALAXY" },

    {  0, 13, 20,  4, "LASERS" },
    {  0, 17, 20,  4, "COMMAND" },
    { 20, 13, 26,  8, "MAIN VIEWER" },
    { 46, 13, 34,  8, "COMMUNICATORS" },

    {  0, 21, 20,  4, "U.S.S. LEXINGTON" },
    { 20, 21, 26,  4, "SYSTEMS STATUS" },
    { 46, 21, 34,  4, "DAMAGE REPORT" },
};

#define BORDER_COL  EGA_TO_VDC(EGA_LTCYAN)
#define TITLE_COL   EGA_TO_VDC(EGA_YELLOW)

static void draw_panel(const Panel *p) {
    unsigned char right = (unsigned char)(p->x + p->w - 1);
    unsigned char bottom = (unsigned char)(p->y + p->h - 1);
    unsigned char inner = (unsigned char)(p->w - 2);

    scr_hline((unsigned char)(p->x + 1), p->y, inner, G_HLINE, BORDER_COL);
    scr_hline((unsigned char)(p->x + 1), bottom, inner, G_HLINE, BORDER_COL);
    scr_vline(p->x, (unsigned char)(p->y + 1), (unsigned char)(p->h - 2), G_VLINE, BORDER_COL);
    scr_vline(right, (unsigned char)(p->y + 1), (unsigned char)(p->h - 2), G_VLINE, BORDER_COL);

    scr_put(p->x, p->y, G_TL, BORDER_COL);
    scr_put(right, p->y, G_TR, BORDER_COL);
    scr_put(p->x, bottom, G_BL, BORDER_COL);
    scr_put(right, bottom, G_BR, BORDER_COL);

    /* Title sits in the top border, one cell in, like the original. */
    scr_puts((unsigned char)(p->x + 2), p->y, p->title, TITLE_COL);
}

/* Milestone 1 check, drawn into the otherwise-empty MAIN VIEWER panel: the
   sixteen EGA colours in EGA index order, each run through EGA_TO_VDC. If the
   rotate in egavdc.h is right this reads black, blue, green, cyan, red,
   magenta, brown, light grey, then the same eight brightened -- i.e. the
   standard CGA/EGA ramp, directly comparable against the original running in
   DOSBox-X. A wrong mapping shows the hues out of order, which is the exact
   failure commodore-uno's vdc.h records having shipped once.

   Index 6 is expected to differ: EGA renders it brown, the VDC olive. */
static void draw_palette_check(const Panel *p) {
    unsigned char i;
    unsigned char x = (unsigned char)(p->x + 2);
    unsigned char y = (unsigned char)(p->y + 3);

    scr_puts(x, (unsigned char)(p->y + 2), "EGA PALETTE 0-15", EGA_TO_VDC(EGA_LTGRAY));
    for (i = 0; i < 16; i++) {
        scr_put((unsigned char)(x + i), y, G_BLOCK, EGA_TO_VDC(i));
    }
    scr_puts(x, (unsigned char)(y + 2), "6=BROWN READS OLIVE", EGA_TO_VDC(EGA_DKGRAY));
}

void draw_console(void) {
    unsigned char i;
    for (i = 0; i < PANEL_COUNT; i++) draw_panel(&panels[i]);
    draw_palette_check(&panels[5]);   /* MAIN VIEWER */
}
