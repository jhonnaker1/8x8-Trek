#include "vdc.h"
#include "layout.h"
#include "egavdc.h"

/* Column bands: 0..26 | 27..45 | 46..79   (27 + 19 + 34 = 80)
   Row bands:    0..12 | 13..20 | 21..24   (13 +  8 +  4 = 25)
   The left middle band splits again into LASERS over COMMAND.

   The middle column breaks that row banding on purpose. SYSTEMS STATUS runs
   rows 17..24, which is where a 640x350 capture of the original puts it
   (measured 2026-08-19: the panel's grey frame spans x161..318, y251..348,
   i.e. columns 20..39 and rows 17..24). Ten systems will not fit in the two
   interior rows the old 21..24 band gave it. MAIN VIEWER gives up the height:
   the original's viewer is taller than this, but this port draws nothing in
   it yet, and an empty panel is the right thing to spend. */
const Panel panels[PANEL_COUNT] = {
    {  0,  0, 27, 13, "SHORT RANGE SCAN" },
    { 27,  0, 19, 13, "STATUS" },
    { 46,  0, 34, 13, "CHART OF KNOWN GALAXY" },

    {  0, 13, 20,  4, "LASERS" },
    {  0, 17, 20,  4, "COMMAND" },
    { 20, 13, 26,  4, "MAIN VIEWER" },
    { 46, 13, 34,  8, "COMMUNICATIONS" },

    {  0, 21, 20,  4, "U.S.S. LEXINGTON" },
    { 20, 17, 26,  8, "SYSTEMS STATUS" },
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

/* The milestone-1 EGA palette swatch lived in MAIN VIEWER and has been
   retired: the EGA->VDC mapping it existed to check is now proven natively
   by core `make test` and by c128 `make test`, and was confirmed on screen
   in VICE. Leaving it in would have meant a debug aid squatting in a panel
   that the original uses for the external view and graphical readouts
   (manual l.294-306). The panel stays empty until there is something real
   to put in it, rather than filled with something invented. */
void draw_console(void) {
    unsigned char i;
    for (i = 0; i < PANEL_COUNT; i++) draw_panel(&panels[i]);
}
