/* The 40-column console, both pages, drawn for a person to judge.
 *
 * ITEM 57'S STEP TWO. The straight left/right split is implemented and
 * NEITHER of the three message-placement answers is -- so what this draws is
 * the question, not an answer to it. Page 1 is the instrument cluster, page 2
 * is the long range chart above the message region. Look at page 1 and ask
 * whether typing an order there and reading its result on page 2 is
 * acceptable; that is the whole decision.
 *
 * The four message boxes on page 2 are drawn as empty frames at their real
 * sizes (MSG_BOX_H = 3, the last one taking the remainder) because their
 * SHAPE is the thing being judged -- how much of the screen the narration
 * occupies, and therefore what it would cost to move two of them to page 1.
 *
 * Which page is drawn comes from `page40`, poked by tools/shot40.py, so both
 * pictures come out of one build and neither depends on timing.
 */
#include "vdc.h"
#include "vic.h"
#include "layout40.h"
#include "egavic.h"

/* Literals, not pooled strings: this test has no far memory and no disk, and
   the geometry is what is being checked. The real build passes S(p->title). */
static const char *const titles[PANEL_COUNT] = {
    "SHORT RANGE SCAN", "STATUS", "LONG RANGE CHART", "LASERS",
    "COMMAND", "MAIN VIEWER", 0, "SYSTEMS STATUS"
};

volatile unsigned char page40 = PAGE_TACTICAL;

#define MSG_SLOTS  4
#define MSG_BOX_H  3

/* The message region: a STACK OF SEPARATELY BORDERED BOXES, one per message,
   which is the original's design and not a simplification of it -- what looks
   like a COMMUNICATIONS panel above a DAMAGE REPORT panel is one region
   holding both, interleaved in time order. */
static void msg_boxes(void) {
    unsigned char slot, y, h, right, bot, c;
    for (slot = 0; slot < MSG_SLOTS; slot++) {
        y = (unsigned char)(MSG40_Y + slot * MSG_BOX_H);
        h = (unsigned char)(slot + 1 < MSG_SLOTS
                            ? MSG_BOX_H : MSG40_H - (MSG_SLOTS - 1) * MSG_BOX_H);
        right = (unsigned char)(MSG40_X + MSG40_W - 1);
        bot = (unsigned char)(y + h - 1);
        /* Coloured by the department that speaks, as the original's are. Two
           of the four shown in each so the region does not read as uniform. */
        c = (slot & 1) ? EGA_TO_VIC(EGA_CYAN) : EGA_TO_VIC(EGA_BROWN);

        scr_hline((unsigned char)(MSG40_X + 1), y,
                  (unsigned char)(MSG40_W - 2), G_HLINE, c);
        scr_hline((unsigned char)(MSG40_X + 1), bot,
                  (unsigned char)(MSG40_W - 2), G_HLINE, c);
        scr_vline(MSG40_X, (unsigned char)(y + 1), (unsigned char)(h - 2),
                  G_VLINE, c);
        scr_vline(right, (unsigned char)(y + 1), (unsigned char)(h - 2),
                  G_VLINE, c);
        scr_put(MSG40_X, y, G_TL, c);
        scr_put(right, y, G_TR, c);
        scr_put(MSG40_X, bot, G_BL, c);
        scr_put(right, bot, G_BR, c);
        scr_puts((unsigned char)(MSG40_X + 2), (unsigned char)(y + 1),
                 (slot & 1) ? "NAVIGATION" : "ENGINEERING", c);
    }
}

int main(void) {
    unsigned char shown = 0xFF;
    vdc_init();
    for (;;) {
        if (page40 != shown) {
            shown = page40;
            draw_console40(shown, titles);
            if (shown == PAGE_CHART) msg_boxes();
        }
    }
}
