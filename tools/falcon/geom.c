/* Set the Falcon's VGA 640x480x16 mode ($001A) and draw a figure whose
   shape proves the geometry: an 8-row band at the very top, an 8-row band
   at rows 472..479, and a 16-pixel column down the left edge, all in
   colour 15 on a colour-1 ground.  If the stride really is 320 bytes and
   the screen really is 640x480, that draws a clean bracket; any other
   geometry skews it into diagonals. */
#include <tos.h>
#include <stdio.h>

#define W       640
#define H       480
#define STRIDE  (W / 2)          /* 4 planes, 1 bit each = 320 bytes/row */

int main(void)
{
    int  old = VsetMode(-1);
    unsigned char *scr;
    long r, c;

    VsetMode(VGA | COL80 | BPS4);
    scr = (unsigned char *)Physbase();

    for (r = 0; r < H; r++) {
        unsigned short *row = (unsigned short *)(scr + r * STRIDE);
        for (c = 0; c < W / 16; c++) {       /* 16 px per 4-word group */
            int top    = (r < 8);
            int bottom = (r >= H - 8);
            int left   = (c == 0);
            int on     = top || bottom || left;
            row[c * 4 + 0] = 0xFFFF;                 /* plane 0: ground = 1 */
            row[c * 4 + 1] = on ? 0xFFFF : 0x0000;
            row[c * 4 + 2] = on ? 0xFFFF : 0x0000;
            row[c * 4 + 3] = on ? 0xFFFF : 0x0000;
        }
    }

    Crawcin();                    /* hold the picture for the screenshot */
    VsetMode(old);
    printf("restored $%04x\r\n", old & 0xffff);
    return 0;
}
