#include "vdc.h"
#include "layout40.h"
#include "egavic.h"
#include "strdata.h"
#include "../../core/strpool.h"

/* Six of these are the 80-column coordinates VERBATIM. Only SCAN (21 wide ->
   20, losing one column of padding) and STATUS (x 20 -> 19) move, and only to
   close the single column the top band was over. See layout40.h. */
const Panel panels[PANEL_COUNT] = {
    {  0,  0, 20, 11, S_145 },      /* SCAN    -- one narrower than at 80 */
    { 19,  0, 21, 11, S_146 },      /* STATUS  -- one left     */
    {  0,  0, 40, 11, S_147 },      /* CHART   -- PAGE 2, full width      */
    {  0, 11, 21,  4, S_127 },      /* LASERS  -- unchanged    */
    {  0, 14, 21,  4, S_148 },      /* COMMAND -- unchanged    */
    { 21, 11, 19,  7, S_149 },      /* VIEWER  -- unchanged    */
    {  0, 17, 20,  8, PANEL_NO_TITLE },  /* BADGE   -- unchanged    */
    { 20, 17, 20,  8, S_150 }       /* SYSTEMS -- unchanged    */
};

const unsigned char panel40_page[PANEL_COUNT] = {
    PAGE_TACTICAL, PAGE_TACTICAL, PAGE_CHART, PAGE_TACTICAL,
    PAGE_TACTICAL, PAGE_TACTICAL, PAGE_TACTICAL, PAGE_TACTICAL
};

#define BORDER_COL  EGA_TO_VIC(EGA_LTCYAN)
#define TITLE_COL   EGA_TO_VIC(EGA_YELLOW)

unsigned char layout40_page = PAGE_TACTICAL;

static void draw_panel40(const Panel *p) {
    unsigned char right = (unsigned char)(p->x + p->w - 1);
    unsigned char bottom = (unsigned char)(p->y + p->h - 1);
    unsigned char inner = (unsigned char)(p->w - 2);

    scr_hline((unsigned char)(p->x + 1), p->y, inner, G_HLINE, BORDER_COL);
    scr_hline((unsigned char)(p->x + 1), bottom, inner, G_HLINE, BORDER_COL);
    scr_vline(p->x, (unsigned char)(p->y + 1), (unsigned char)(p->h - 2),
              G_VLINE, BORDER_COL);
    scr_vline(right, (unsigned char)(p->y + 1), (unsigned char)(p->h - 2),
              G_VLINE, BORDER_COL);

    scr_put(p->x, p->y, G_TL, BORDER_COL);
    scr_put(right, p->y, G_TR, BORDER_COL);
    scr_put(p->x, bottom, G_BL, BORDER_COL);
    scr_put(right, bottom, G_BR, BORDER_COL);

    /* Title in the top border, one cell in, like the original -- and through
       S() exactly as layout.c does, so the 40-column build pays no extra
       string cost and a test that has no pool supplies its own S(). */
    if (p->title != PANEL_NO_TITLE)
        scr_puts((unsigned char)(p->x + 2), p->y, S(p->title), TITLE_COL);
}

/* WHERE TWO PANELS MEET. Seven of these nine are the 80-column values
   unchanged, because the panels either side of them did not move. The two
   that DID move are SCAN and STATUS, which share column 20 at eighty and
   column 19 here -- and getting that wrong is not a missing line, it is four
   corner cells, which is the fault the X16 reported as "the lasers and
   command windows have no bottom border". */
static const unsigned char junctions_tactical[] = {
    /* x   y   glyph */
    19,  0, G_TEE_D,    /* SCAN and STATUS share column 19 ...      */
    19, 10, G_TEE_U,    /* ... at both ends of the top band         */
     0, 14, G_TEE_L,    /* LASERS bottom meets COMMAND top          */
    20, 14, G_TEE_R,
     0, 17, G_TEE_L,    /* COMMAND bottom meets the badge top       */
    19, 17, G_TEE_D,    /* the badge's top right, mid COMMAND's line */
    20, 17, G_CROSS,    /* COMMAND, the badge, SYSTEMS: four ways   */
    21, 17, G_TEE_U,    /* VIEWER's bottom left                     */
    39, 17, G_TEE_R     /* VIEWER meets SYSTEMS STATUS              */
};

/* The chart page has NONE. The chart ends at row 10 and the message region
   begins at row 11, so no border is shared -- and the message region has no
   frame of its own at all, which is the original's design: a stack of
   separately bordered boxes, one per message. */

void draw_console(void) {
    unsigned char i;

    /* CLEARS, unlike the 80-column draw_console. It has to: the other page's
       panels are still on the screen and none of them will be overwritten by
       what this page draws. */
    scr_clear();

    for (i = 0; i < PANEL_COUNT; i++) {
        if (panel40_page[i] != layout40_page) continue;
        draw_panel40(&panels[i]);
    }

    /* AFTER every panel, never inside draw_panel40 -- a panel drawn later
       runs its border straight through an earlier one's corner, so nothing
       written per panel survives. The 80-column file learned this the hard
       way and the comment is worth carrying. */
    if (layout40_page == PAGE_TACTICAL)
        for (i = 0; i < sizeof junctions_tactical; i += 3)
            scr_put(junctions_tactical[i], junctions_tactical[i + 1],
                    junctions_tactical[i + 2], BORDER_COL);
}
