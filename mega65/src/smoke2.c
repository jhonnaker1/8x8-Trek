/* Bisect: add the game's startup steps one at a time and see which one blacks
 * the screen. Each stage draws a marker, so the screenshot says how far it
 * got. */
#include <stdint.h>
#include "m65vid.h"
#include "../../c128/src/layout.h"
#include "../../core/ega.h"
#include "../../core/strpool.h"
#include "../../core/farmem.h"
#include "../../core/overlay.h"
#include "../../c128/src/strdata.h"
#include "m65snd.h"
#include "music_data.h"
#include "../../c128/src/ui.h"

int main(void) {
    unsigned char row = 1;

    vdc_init();
    scr_puts(2, row++, "STAGE 1: VIDEO UP", EGA_WHITE);

    if (str_load()) scr_puts(2, row++, "STAGE 2: STRINGS LOADED", EGA_LTGREEN);
    else            scr_puts(2, row++, "STAGE 2: STRINGS FAILED", EGA_LTRED);

    /* Anything at all drawn AFTER the first Hypervisor call. */
    scr_puts(2, row++, "STAGE 3: DREW AFTER FILE I/O", EGA_YELLOW);

    /* And a pooled string, which proves the pool as well as the drawing. */
    scr_puts(2, row++, S(S_89), EGA_LTCYAN);

    ovl_load(OVL_TITLE);
    /* DID IT ACTUALLY LOAD? ovl_load returns void, so the old marker here
       printed whether or not anything happened. Read the window back: all
       zeroes means ovl_init() failed and any call into an overlay is running
       BRKs. */
    {
        const unsigned char *w = (const unsigned char *)0xC000;
        const char *H = "0123456789ABCDEF";
        unsigned char i;
        static char hex[20];
        for (i = 0; i < 8; i++) { hex[i*2] = H[w[i]>>4]; hex[i*2+1] = H[w[i]&15]; }
        hex[16] = 0;
        scr_puts(2, row, "STAGE 4: WINDOW=", EGA_LTMAGENTA);
        scr_puts(19, row, hex, EGA_WHITE);
        row++;
    }

    /* The two things the full game does that this did not. */
    snd_init();
    {
        uint16_t mb = far_load("MUSIC.DAT");
        snd_music_data(mb, (unsigned char)(mb != FAR_NONE));
        scr_puts(2, row++, mb == FAR_NONE ? "STAGE 5: NO MUSIC FILE"
                                          : "STAGE 5: MUSIC LOADED", EGA_LTCYAN);
    }
    snd_music(MUS_TITLE);
    scr_puts(2, row++, "STAGE 6: MUSIC STARTED", EGA_WHITE);

    /* And the frame loop, which is where snd_poll() lives -- the one caller
       that DMAs into a stack local every single frame. */
    {
        unsigned int i;
        for (i = 0; i < 2000; i++) { wait_vsync(); snd_poll(); }
    }
    scr_puts(2, row++, "STAGE 7: SURVIVED 2000 POLLED FRAMES", EGA_LTGREEN);

    /* STAGE 8: the real thing. ui_title() is the first screen the full game
       draws and the last place the black screen can be hiding. It blocks on a
       key, so the screenshot catches exactly what it painted. */
    ui_title();

    for (;;) { wait_vsync(); }
    return 0;
}
