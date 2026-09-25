/* FIRST LIGHT for the SCREEN 7 driver: every glyph class the console uses,
   in every colour, inside a frame drawn from the box set -- the corners have
   to MEET or the box glyphs are wrong. Stays up for the screenshot. */
#include "vdc.h"

void main(void)
{
    unsigned char i;

    vdc_init();

    scr_put(0, 0, 112, 11);  scr_hline(1, 0, 78, 64, 11);  scr_put(79, 0, 110, 11);
    scr_vline(0, 1, 23, 93, 11);                           scr_vline(79, 1, 23, 93, 11);
    scr_put(0, 24, 109, 11); scr_hline(1, 24, 78, 64, 11); scr_put(79, 24, 125, 11);

    scr_puts(3, 2, "8X8 TREK -- MSX2 FIRST LIGHT -- SCREEN 7, 80X25, 6X8 CELLS", 14);
    for (i = 1; i < 16; i++)
        scr_puts(3, (unsigned char)(3 + i),
                 "THE QUICK BROWN FOX 0123456789 ,.:;()+-*/=?  EGA COLOUR", i);
    for (i = 0; i < 26; i++)                       /* reverse video, a RULE */
        scr_put((unsigned char)(3 + i), 21, (unsigned char)(0x80 | (i + 1)), 15);

    for (;;) wait_vsync();
}
