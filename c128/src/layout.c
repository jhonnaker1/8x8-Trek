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
/* WHERE TWO PANELS MEET, the corner is wrong until this fixes it.
 *
 * REPORTED OFF THE X16 2026-09-06: "the lasers and command windows have no
 * bottom border ... main viewer too". The border was there -- reading the
 * pixels of the frame shows a full G_HLINE across every one of those rows.
 * What was missing was the CORNERS. LASERS occupies rows 11..14 and COMMAND
 * rows 14..17: they SHARE row 14 on purpose, so LASERS wrote G_BL and G_BR
 * there and COMMAND then wrote G_TL and G_TR straight over them. A G_TL has
 * a stroke going right and one going down and NOTHING going up, so the side
 * of the LASERS box stopped six pixels short of its own bottom line and the
 * box read as open. Row 17 is worse: COMMAND, the badge, MAIN VIEWER and
 * SYSTEMS STATUS all end or begin there.
 *
 * NOT AN X16 FAULT. This file is shared, so the C128 and the MEGA65 drew it
 * the same way and still do until this ships; the X16 is where somebody
 * looked. The three fonts were dumped and carry identical bitmaps at every
 * code used here -- VICE's chargen-390059-01, the charset in MEGA65.ROM, and
 * bank 6 of the X16's rom.bin -- so one table serves all three.
 *
 * A TABLE, NOT A RULE, AND THAT IS A BUDGET DECISION. The general form of
 * this -- ask every panel what strokes it puts through a cell, index a glyph
 * by the four bits -- was written, measured at 209 bytes of resident code,
 * and dropped: the X16 has 154 bytes between its last variable and its
 * stack. Nine cells at three bytes is 27. What a hand table costs instead is
 * the risk of going stale, so `make -C c128 verify` recomputes all nine from
 * the panel geometry and fails if this list disagrees.
 *
 * The last two of these were never reported: SHORT RANGE SCAN and STATUS
 * share column 20 for their whole height, and their corners were wrong in
 * exactly the same way at the top and bottom of the band. */
static const unsigned char junctions[] = {
    /* x   y   glyph */
    20,  0, G_TEE_D,    /* SCAN and STATUS share column 20 ...      */
    20, 10, G_TEE_U,    /* ... at both ends of the top band         */
     0, 14, G_TEE_L,    /* LASERS bottom meets COMMAND top          */
    20, 14, G_TEE_R,
     0, 17, G_TEE_L,    /* COMMAND bottom meets the badge top       */
    19, 17, G_TEE_D,    /* the badge's top right, mid COMMAND's line */
    20, 17, G_CROSS,    /* COMMAND, the badge, SYSTEMS: four ways   */
    21, 17, G_TEE_U,    /* MAIN VIEWER's bottom left               */
    39, 17, G_TEE_R     /* MAIN VIEWER meets SYSTEMS STATUS        */
};

void draw_console(void) {
    unsigned char i;

    for (i = 0; i < PANEL_COUNT; i++) draw_panel(&panels[i]);

    /* AFTER every panel, not inside draw_panel(). A panel drawn later runs
       its border straight through an earlier panel's corner -- SYSTEMS
       STATUS's top line covers MAIN VIEWER's bottom-left cell -- so nothing
       written per panel can survive. */
    for (i = 0; i < sizeof junctions; i += 3)
        scr_put(junctions[i], junctions[i + 1], junctions[i + 2], BORDER_COL);
}
