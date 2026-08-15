#include "vdc.h"
#include "layout.h"

/* 8x8 Trek -- C128 VDC port, milestone 1.
 *
 * Brings up the VDC and draws the nine-panel console frame. No game logic
 * yet; this exists to prove the chain end to end -- cl65 -> VDC driver ->
 * 80x25 attribute-colour output -> the panel layout -- and to put the EGA
 * palette mapping on screen where it can be checked by eye.
 */
int main(void) {
    vdc_init();
    draw_console();

    /* Nothing to drive yet. Hold the display rather than returning to BASIC,
       which would scroll the console away. */
    for (;;) wait_vsync();

    return 0;
}
