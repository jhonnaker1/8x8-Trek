/* The V9958 driver, checked on the card. MAME renders this card's screen
   black whatever the VDP is doing, so this draws a known pattern, reads VRAM
   back THROUGH the card and leaves the answers in CPU RAM at $2F00.
   The readback itself was checked first -- write two values, read the first
   back and get the first -- because an instrument returning a latch would
   have "confirmed" every one of these. */
#include <stdint.h>
#include "../../c128/src/vdc.h"

#define R ((unsigned char *)0x2F00)
#define VDP_DATA ((unsigned char *)0xFF78)
#define VDP_ADDR ((unsigned char *)0xFF79)

static void vreg(unsigned char r, unsigned char v)
{ *VDP_ADDR = v; *VDP_ADDR = (unsigned char)(0x80 | r); }

static unsigned char vpeek(unsigned int a)
{
    vreg(14, (unsigned char)((a >> 14) & 0x07));
    *VDP_ADDR = (unsigned char)(a & 0xFF);
    *VDP_ADDR = (unsigned char)((a >> 8) & 0x3F);
    return *VDP_DATA;
}

int main(void)
{
    unsigned char i;

    asm { orcc #$50 }
    asm { lds #$3F00 }
    for (i = 0; i < 32; i++) R[i] = 0xEE;

    vdc_init();

    scr_put(0, 0, 1, 15);                       /* 'A', white, top-left */
    R[0] = vpeek(6U * 256 + 8);
    R[1] = vpeek(6U * 256 + 9);
    R[2] = vpeek(6U * 256 + 10);
    R[3] = vpeek(9U * 256 + 8);                 /* row 3 of the same cell */
    R[4] = vpeek(9U * 256 + 9);
    R[5] = vpeek(9U * 256 + 10);

    scr_put(79, 24, 8, 2);                      /* the far corner */
    R[6] = vpeek((unsigned int)(6 + 192) * 256 + 8 + 237);
    R[7] = vpeek((unsigned int)(6 + 192) * 256 + 9 + 237);
    R[8] = vpeek((unsigned int)(6 + 192) * 256 + 10 + 237);

    scr_put(1, 0, (unsigned char)(1 | 0x80), 15);   /* reverse video */
    R[9]  = vpeek(6U * 256 + 11);
    R[10] = vpeek(6U * 256 + 12);
    R[11] = vpeek(6U * 256 + 13);

    scr_put(2, 0, 64, 1);                       /* a box glyph */
    R[12] = vpeek(9U * 256 + 14);
    R[13] = vpeek(9U * 256 + 15);
    R[14] = vpeek(9U * 256 + 16);

    /* THE MESSAGE LOG in the card's VRAM -- the seam the Falcon shipped
       broken because ui.c calls these three and a comment I invented said
       nothing did. */
    vdc_set_address(0x1000);
    vdc_data_write(0x11); vdc_data_write(0x22); vdc_data_write(0x33);
    vdc_set_address(0x1000);
    R[15] = vdc_data_read(); R[16] = vdc_data_read(); R[17] = vdc_data_read();
    vdc_set_address(0x1000 + 100);
    vdc_data_write(0xA5);
    vdc_set_address(0x1000 + 100);
    R[18] = vdc_data_read();
    /* ...and the FIRST slot again, to prove the second write did not move it */
    vdc_set_address(0x1000);
    R[19] = vdc_data_read();

    scr_put(5, 5, 1, 15);
    R[20] = vpeek((unsigned int)(6 + 40) * 256 + 8 + 15 + 1);
    scr_clear();
    R[21] = vpeek((unsigned int)(6 + 40) * 256 + 8 + 15 + 1);
    /* and the log must SURVIVE a screen clear -- it lives at $E000 */
    vdc_set_address(0x1000);
    R[22] = vdc_data_read();

    R[31] = 0x5A;
    for (;;) ;
}
