/* Atari + VBXE first light. One screenshot answers four questions:
 *   1. does llvm-mos produce an XEX this machine runs from $3000,
 *   2. does the MEMAC window open and land text where it is aimed,
 *   3. do all sixteen EGA colours come out on their own index,
 *   4. does the BUILT font have a glyph at every code the console uses --
 *      the box-drawing set is authored here and the ASCII half comes from
 *      the Atari OS ROM, so this is where a mismatch shows.
 *
 * Row 24 is drawn on purpose. Uno's driver is 24 rows and this port needs
 * 25; the measurement that said 25 work was made against a different
 * program, so it is re-made here against this one.
 */
#include "vbxevid.h"
#include "layout.h"

static const char *names[16] = {
    "BLACK","BLUE","GREEN","CYAN","RED","MAGENTA","BROWN","LTGRAY",
    "DKGRAY","LTBLUE","LTGREEN","LTCYAN","LTRED","LTMAGENTA","YELLOW","WHITE" };

int main(void) {
    unsigned char i;

    vdc_init();

    scr_puts(2, 0, "EGA TREK -- ATARI VBXE FIRST LIGHT", 15);

    /* Sixteen colours, each labelled in itself. If a name is unreadable its
       index is wrong, and index 0 is meant to be invisible. */
    for (i = 0; i < 16; i++) {
        scr_put(2, (unsigned char)(2 + i), (unsigned char)('0' + (i & 7)), 15);
        scr_puts(4, (unsigned char)(2 + i), names[i], i);
        scr_fill_rect(20, (unsigned char)(2 + i), 8, 1, (unsigned char)G_BLOCK, i);
    }

    /* The box-drawing set, drawn as a frame. If any of these comes out as a
       hollow box the font builder missed a code; as a letter, the screen-code
       mapping disagrees with layout.h. */
    scr_put(40, 2, G_TL, 11); scr_hline(41, 2, 20, G_HLINE, 11); scr_put(61, 2, G_TR, 11);
    scr_vline(40, 3, 6, G_VLINE, 11); scr_vline(61, 3, 6, G_VLINE, 11);
    scr_put(40, 9, G_BL, 11); scr_hline(41, 9, 20, G_HLINE, 11); scr_put(61, 9, G_BR, 11);
    scr_put(50, 2, G_TEE_D, 14); scr_put(50, 9, G_TEE_U, 14);
    scr_vline(50, 3, 6, G_VLINE, 14);
    scr_put(40, 6, G_TEE_L, 12); scr_put(61, 6, G_TEE_R, 12);
    scr_put(50, 6, G_CROSS, 12); scr_hline(41, 6, 9, G_HLINE, 12);
    scr_hline(51, 6, 10, G_HLINE, 12);
    /* Labels kept INSIDE their cells -- the divider is at column 50 and the
       cross at row 6, so a ten-character label starting at 42 runs straight
       through the frame it is meant to be describing. */
    scr_puts(44, 4, "BOX", 15);
    scr_puts(53, 4, "SCREEN", 15);
    scr_puts(42, 7, "GLYPHS", 15);
    scr_puts(53, 7, "CODES", 15);

    /* The two reverse-video codes the badge and the systems bar are made of.
       They exist only as the inversion rule applied to 98 and 100, so this is
       the check that the rule ran. */
    scr_puts(40, 11, "BADGE 98/226:", 15);
    scr_put(54, 11, 98, 14); scr_put(55, 11, 226, 14);
    scr_puts(40, 12, "SYSBAR 228:", 15);
    scr_fill_rect(54, 12, 6, 1, 228, 10);
    scr_puts(40, 13, "SAUCER 81:", 15);
    scr_put(54, 13, 81, 15);

    /* THE 25TH ROW, which uno's driver does not have. If this line is missing
       the XDL's row count is wrong and layout.c's bottom band is off-screen. */
    scr_puts(0, 24, "ROW 24 -- IF YOU CAN READ THIS, TWENTY-FIVE ROWS WORK.", 14);
    scr_puts(0, 23, "ROW 23", 7);

    for (;;) { }
    return 0;
}
