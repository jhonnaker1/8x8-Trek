/* THE PROOF SHEET, DRAWN ON THE MACHINE.
 *
 * coco3/tools/gen_font.py renders its artwork to a PNG on the host, which is
 * the only way the pictures can be reviewed while they are being authored.
 * This is the other half: the same glyphs put through THIS port's widening,
 * this port's palette and this port's long-addressed store, on a real IIgs.
 * A font that looks right in gen_font.py's sheet and wrong here is a bug in
 * gsvid.c, and there is no way to tell those apart without both pictures.
 *
 * What it draws, and every row is a question:
 *   - all 64 text screen codes, so a missing or shifted glyph is visible
 *   - the box set, and BELOW IT a drawn panel, because a box glyph that looks
 *     right alone can still fail to CONNECT to the cell beside it -- which is
 *     the whole reason they are authored at eight wide instead of shifted
 *   - all sixteen colours as solid cells, which is the console's real
 *     requirement (it needs fifteen) rather than a palette dump
 *   - reverse video, which is a RULE rather than data and so has never been
 *     looked at
 */
#include "../../c128/src/vdc.h"
#include "../../core/ega.h"

__attribute__((naked, used, retain, section(".init.040")))
void gs_emul(void) { __asm__ volatile("sec\n\txce"); }
__attribute__((naked, used, retain, section(".init.050")))
void gs_setdb(void) { __asm__ volatile("phk\n\tplb"); }

/* Raw screen codes, not ASCII: scr_puts would rewrite 64..127. */
static void row_of_codes(unsigned char y, unsigned char first,
                         unsigned char n, unsigned char colour)
{
    unsigned char i;
    for (i = 0; i < n; i++)
        scr_put((unsigned char)(i + 2), y, (unsigned char)(first + i), colour);
}

int main(void)
{
    unsigned char i;

    vdc_init();

    scr_puts(0, 0, "IIGS SHR PROOF SHEET", EGA_WHITE);

    /* 64 text codes, sixteen to a row. */
    scr_puts(0, 2, "CODES", EGA_LTCYAN);
    row_of_codes(3,  0, 16, EGA_WHITE);
    row_of_codes(4, 16, 16, EGA_WHITE);
    row_of_codes(5, 32, 16, EGA_WHITE);
    row_of_codes(6, 48, 16, EGA_WHITE);

    /* The box set, by code, in the order gs_box lists them. */
    scr_puts(0, 8, "BOX", EGA_LTCYAN);
    scr_put(2,  9,  64, EGA_YELLOW);  scr_put(3,  9,  81, EGA_YELLOW);
    scr_put(4,  9,  91, EGA_YELLOW);  scr_put(5,  9,  93, EGA_YELLOW);
    scr_put(6,  9,  98, EGA_YELLOW);  scr_put(7,  9, 100, EGA_YELLOW);
    scr_put(8,  9, 107, EGA_YELLOW);  scr_put(9,  9, 109, EGA_YELLOW);
    scr_put(10, 9, 110, EGA_YELLOW);  scr_put(11, 9, 112, EGA_YELLOW);
    scr_put(12, 9, 113, EGA_YELLOW);  scr_put(13, 9, 114, EGA_YELLOW);
    scr_put(14, 9, 115, EGA_YELLOW);  scr_put(15, 9, 125, EGA_YELLOW);
    /* AND ONE THE TABLE DOES NOT HAVE, on purpose: the missing-glyph marker
       has to be visible or it is not a marker. */
    scr_put(17, 9, 126, EGA_LTRED);

    /* A DRAWN PANEL. The question is not whether each glyph is right but
       whether the rules JOIN -- a six-wide rule in an eight-wide cell draws a
       dashed line, and that is only visible in a run. */
    scr_hline(2, 11, 20,  64, EGA_LTGRAY);
    scr_vline(2, 12,  3,  93, EGA_LTGRAY);
    scr_vline(21, 12, 3,  93, EGA_LTGRAY);
    scr_hline(2, 15, 20,  64, EGA_LTGRAY);
    scr_puts(4, 13, "PANEL BORDER JOINS", EGA_LTGREEN);

    /* Sixteen colours as solid cells: screen code 160 is reverse space. */
    scr_puts(0, 17, "COLOURS", EGA_LTCYAN);
    for (i = 0; i < 16; i++)
        scr_put((unsigned char)(2 + i), 18, 160, i);
    for (i = 0; i < 16; i++)
        scr_put((unsigned char)(2 + i), 19, 160, (unsigned char)(i));

    /* Reverse video on text, which is a rule and has never been looked at. */
    scr_puts(0, 21, "REVERSE", EGA_LTCYAN);
    for (i = 0; i < 16; i++)
        scr_put((unsigned char)(2 + i), 22,
                (unsigned char)(0x80 | (1 + i)), EGA_WHITE);

    /* The last row, because a console that uses row 24 and a driver that
       clips at 24 is a bug nothing else on this sheet would show. */
    scr_puts(0, 24, "ROW 24 IS DRAWN", EGA_LTMAGENTA);

    for (;;) ;
    return 0;
}
