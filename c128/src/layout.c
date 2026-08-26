#include "vdc.h"
#include "layout.h"
#include "egavdc.h"
#include "../../core/strpool.h"
#include "strdata.h"

/* See layout.h for where these numbers come from and what the one deliberate
   departure from the original is. */
const Panel panels[PANEL_COUNT] = {
    /* Top band, rows 0..10. The scan panel carries no title because the
       original's does not -- its column headers are the top line. */
    {  0,  0, 21, 11, S_145 },
    { 20,  0, 21, 11, S_146 },
    { 41,  0, 39, 11, S_147 },

    /* Middle band, rows 11..17. LASERS and COMMAND share row 14. */
    {  0, 11, 21,  4, S_127 },
    {  0, 14, 21,  4, S_148 },
    { 21, 11, 19,  7, S_149 },

    /* Bottom band, rows 17..24. The badge has no title in its border: the
       ship name is the first line inside it. */
    {  0, 17, 20,  8, PANEL_NO_TITLE },
    { 20, 17, 20,  8, S_150 },

    /* Columns 40..79 of rows 11..24 are deliberately absent from this table.
       That is the message region, and it has no frame of its own -- see
       MSG_X and friends in layout.h. */
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

    /* Title sits in the top border, one cell in, like the original. The
       badge panel carries none, which is PANEL_NO_TITLE rather than an empty
       string -- an empty string would still cost a pool id and a fetch. */
    if (p->title != PANEL_NO_TITLE)
        scr_puts((unsigned char)(p->x + 2), p->y, S(p->title), TITLE_COL);
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
