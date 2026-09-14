/* Does the VIC-IIe driver draw what it is told? A picture, to be LOOKED at.
 *
 * THREE THINGS CAN BE SILENTLY WRONG HERE and none of them reports an error:
 *
 *   1. THE CHARACTER SET BANK. $D018 picks uppercase/graphics or
 *      lowercase/uppercase. In the wrong one every box-drawing glyph renders
 *      as punctuation and every capital letter as a graphic -- a screen that
 *      is full of SOMETHING, which is why a return code cannot catch it.
 *   2. THE COLOUR TABLE. egavic.h maps sixteen EGA indices onto sixteen
 *      Commodore ones, and a transposed pair is a legal colour in the wrong
 *      place. check_colours.py proves the eight information colours stay
 *      DISTINCT; it cannot prove they are the RIGHT distinct colours.
 *   3. THE SCREEN CODES themselves. layout.h's G_* were verified against the
 *      C128 character ROM for the VDC, which loads that ROM into its own RAM.
 *      The VIC-IIe reads the same ROM directly -- so they SHOULD be identical
 *      and this is the cheapest place to find out they are not.
 *
 * So this draws the palette, the glyphs and a real panel border, and a person
 * looks at it. That is not a weaker test than an assertion; on this project a
 * person looking at the screen has beaten the instruments seventeen times.
 */
#include "vdc.h"
#include "vic.h"
#include "layout.h"
#include "egavic.h"

/* Eight of the sixteen are GAME RULES rather than decoration -- the Mongol
   classes, the chart markers, the Vandal, the department colours. Marked so
   the eye goes to them first: if one of these is wrong the game misinforms
   the player, and if a decorative one is wrong it merely looks odd. */
static const unsigned char informational[16] = {
    0,0,1,1,1,1,1,1, 0,1,1,0,0,0,0,0
};

static void palette(void) {
    unsigned char i, x;

    scr_puts(0, 2, "EGA PALETTE, MAPPED THROUGH EGAVIC.H", EGA_TO_VIC(EGA_WHITE));
    scr_puts(0, 3, "* = CARRIES INFORMATION, NOT DECORATION", EGA_TO_VIC(EGA_DKGRAY));

    for (i = 0; i < 16; i++) {
        x = (unsigned char)(3 + i * 2);
        /* A solid cell in the mapped colour. G_BLOCK is a reverse space, so
           what shows is the FOREGROUND -- which is the only colour a VIC-IIe
           cell has of its own. */
        scr_put(x, 5, G_BLOCK, EGA_TO_VIC(i));
        scr_put((unsigned char)(x + 1), 5, G_BLOCK, EGA_TO_VIC(i));
        /* The EGA index under it, in hex, so a transposition is readable
           rather than something to count along the row for. */
        scr_put(x, 6, (unsigned char)(i < 10 ? 48 + i : 1 + (i - 10)),
                EGA_TO_VIC(EGA_LTGRAY));
        if (informational[i])
            scr_put(x, 7, 42, EGA_TO_VIC(EGA_YELLOW));      /* '*' */
    }
}

/* A REAL PANEL, drawn with the real defines, not a sample of lines. If
   layout.h's glyphs are wrong for this chip the corners are where it shows --
   that is exactly the fault the X16 reported as "no bottom border" and which
   turned out to be four corner cells, not a missing line. */
static void panel(void) {
    unsigned char x0 = 2, y0 = 10, w = 21, h = 8;
    unsigned char right = (unsigned char)(x0 + w - 1);
    unsigned char bot = (unsigned char)(y0 + h - 1);
    unsigned char c = EGA_TO_VIC(EGA_LTCYAN);

    scr_hline((unsigned char)(x0 + 1), y0, (unsigned char)(w - 2), G_HLINE, c);
    scr_hline((unsigned char)(x0 + 1), bot, (unsigned char)(w - 2), G_HLINE, c);
    scr_vline(x0, (unsigned char)(y0 + 1), (unsigned char)(h - 2), G_VLINE, c);
    scr_vline(right, (unsigned char)(y0 + 1), (unsigned char)(h - 2), G_VLINE, c);
    scr_put(x0, y0, G_TL, c);
    scr_put(right, y0, G_TR, c);
    scr_put(x0, bot, G_BL, c);
    scr_put(right, bot, G_BR, c);
    scr_puts((unsigned char)(x0 + 2), y0, "SHORT RANGE SCAN", EGA_TO_VIC(EGA_YELLOW));

    /* The junctions, which is where two panels meet and the naive answer is
       wrong. Laid out in a row so each is identifiable. */
    scr_puts(2, 19, "JUNCTIONS:", EGA_TO_VIC(EGA_LTGRAY));
    scr_put(13, 19, G_TEE_L, c);
    scr_put(15, 19, G_TEE_R, c);
    scr_put(17, 19, G_TEE_D, c);
    scr_put(19, 19, G_TEE_U, c);
    scr_put(21, 19, G_CROSS, c);
}

int main(void) {
    vdc_init();

    scr_puts(0, 0, "EGA TREK  VIC-IIE 40 COLUMN SMOKE TEST",
             EGA_TO_VIC(EGA_WHITE));
    palette();
    panel();

    /* THE RIGHT EDGE, so a column-count error is visible rather than
       inferred. If the driver's stride is wrong this lands somewhere else
       entirely, and if it is right these sit flush against columns 0 and 39
       on every one of the 25 rows. */
    scr_put(0, 24, G_BLOCK, EGA_TO_VIC(EGA_LTGREEN));
    scr_put(39, 24, G_BLOCK, EGA_TO_VIC(EGA_LTGREEN));
    scr_put(39, 0, G_BLOCK, EGA_TO_VIC(EGA_LTGREEN));
    scr_puts(2, 22, "GREEN BLOCKS MARK 0,0-39,24 CORNERS",
             EGA_TO_VIC(EGA_DKGRAY));

    for (;;) {}
}
