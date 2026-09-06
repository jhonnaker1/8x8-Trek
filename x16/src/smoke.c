/* X16 first light. One screenshot answers three questions:
 *   1. does the toolchain produce a binary the machine runs,
 *   2. does VERA text output land where it is aimed,
 *   3. do all sixteen EGA colours come out on their own index.
 * It also draws the console's box-drawing glyphs, because those are C128
 * SCREEN CODES baked into shared layout.h and whether the X16's charset
 * agrees is a real question, not an assumption. */
#include <stdint.h>
#include "x16vera.h"
#include "layout.h"

/* NO CHROUT HERE. It writes at the KERNAL's cursor, which is the top-left of
   the very text map this program draws into -- progress markers added to
   diagnose a capture problem overwrote the title they were meant to help
   verify. If this needs instrumenting again, use `-dump V` and read the map. */

static const char *names[16] = {
    "BLACK","BLUE","GREEN","CYAN","RED","MAGENTA","BROWN","LTGRAY",
    "DKGRAY","LTBLUE","LTGREEN","LTCYAN","LTRED","LTMAGENTA","YELLOW","WHITE" };

int main(void) {
    unsigned char i;

    vdc_init();

    scr_puts(2, 0, "EGA TREK -- X16 FIRST LIGHT", 15);

    /* Sixteen colours, each labelled in itself. If a name is unreadable its
       index is wrong, and index 0 is meant to be invisible. */
    for (i = 0; i < 16; i++) {
        scr_put(2, (unsigned char)(2 + i), (unsigned char)('0' + (i & 7)), 15);
        scr_puts(4, (unsigned char)(2 + i), names[i], i);
        scr_fill_rect(20, (unsigned char)(2 + i), 8, 1, (unsigned char)G_BLOCK, i);
    }

    /* The box-drawing set, drawn as a frame. C128 screen codes; if the X16's
       charset disagrees these come out as letters. */
    scr_put(40, 2, G_TL, 11); scr_hline(41, 2, 20, G_HLINE, 11); scr_put(61, 2, G_TR, 11);
    scr_vline(40, 3, 6, G_VLINE, 11); scr_vline(61, 3, 6, G_VLINE, 11);
    scr_put(40, 9, G_BL, 11); scr_hline(41, 9, 20, G_HLINE, 11); scr_put(61, 9, G_BR, 11);
    scr_put(50, 2, G_TEE_D, 14); scr_put(50, 9, G_TEE_U, 14);
    scr_vline(50, 3, 6, G_VLINE, 14);
    scr_put(40, 6, G_TEE_L, 12); scr_put(61, 6, G_TEE_R, 12);
    scr_put(50, 6, G_CROSS, 12); scr_hline(41, 6, 9, G_HLINE, 12);
    scr_hline(51, 6, 10, G_HLINE, 12);
    scr_puts(42, 4, "BOX GLYPHS", 15);
    scr_puts(52, 4, "SCREEN", 15);
    scr_puts(52, 7, "CODES", 15);

    scr_puts(2, 20, "IF THE FRAME IS LETTERS, THE CHARSET DISAGREES.", 14);

    /* x16emu records a GIF when the program asks: $9FB5 = 1 captures one
       frame. That is how this port gets a screenshot without a harness. */
    *(volatile unsigned char *)0x9FB5 = 1;

    for (;;) { }
    return 0;
}
