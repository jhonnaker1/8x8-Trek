/* First light on the MEGA65: the console frame, drawn from the SHARED layout
 * table with the SHARED glyph constants, on the shared EGA colour numbers.
 *
 * It draws and then spins. Xemu's -dumpscreen and -screenshot both flush on
 * SIGTERM, which is what makes a program with no exit path verifiable; see
 * the Makefile. */
#include "m65vid.h"
#include "../../c128/src/layout.h"
#include "../../core/ega.h"

static void frame(unsigned char x, unsigned char y, unsigned char w,
                  unsigned char h, unsigned char col) {
    scr_put(x, y, G_TL, col);
    scr_hline((unsigned char)(x + 1), y, (unsigned char)(w - 2), G_HLINE, col);
    scr_put((unsigned char)(x + w - 1), y, G_TR, col);
    scr_vline(x, (unsigned char)(y + 1), (unsigned char)(h - 2), G_VLINE, col);
    scr_vline((unsigned char)(x + w - 1), (unsigned char)(y + 1),
              (unsigned char)(h - 2), G_VLINE, col);
    scr_put(x, (unsigned char)(y + h - 1), G_BL, col);
    scr_hline((unsigned char)(x + 1), (unsigned char)(y + h - 1),
              (unsigned char)(w - 2), G_HLINE, col);
    scr_put((unsigned char)(x + w - 1), (unsigned char)(y + h - 1), G_BR, col);
}

int main(void) {
    unsigned char i;

    vdc_init();

    for (i = 0; i < PANEL_COUNT; i++)
        frame(panels[i].x, panels[i].y, panels[i].w, panels[i].h, EGA_LTCYAN);
    frame(MSG_X, MSG_Y, MSG_W, MSG_H, EGA_LTGREEN);

    /* The sixteen EGA colours, in order, so the palette can be read off the
       screen rather than trusted. Brown is index 6 -- the one the VDC cannot
       render and this machine can. */
    scr_puts(2, 1, "EGA PALETTE 0-15", EGA_WHITE);
    for (i = 0; i < 16; i++)
        scr_put((unsigned char)(2 + i), 2, 'A' - 64 + i > 26 ? 42 : (unsigned char)(1 + i), i);

    scr_puts(23, 12, "EGA TREK", EGA_YELLOW);
    scr_puts(23, 13, "MEGA65 NATIVE, 80 COLUMNS", EGA_LTGREEN);

    for (;;) { wait_vsync(); }
    return 0;
}
