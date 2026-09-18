/* The nine-panel console, drawn by the REAL shared layout.c through the real
 * f256vid.c. First picture of the game on this machine.
 *
 * WHAT THIS SETTLES AND WHAT IT DOES NOT. It exercises four things that are
 * either all right or produce an obviously wrong picture: the DOUBLE_Y mode
 * and its 80x30 grid, the two-row console offset, the EGA palette in both
 * LUTs, and the rebuilt screen-code font -- the console is built almost
 * entirely out of box-drawing glyphs, so a font build that went wrong cannot
 * hide here.
 *
 * IT SETTLES NOTHING ABOUT THE GAME. Every defect Jamie found in the card-less
 * CoCo 3 lived in ui.c with live state, which no frame bench reaches. This is
 * the seam, not the port.
 *
 * layout.c wants panel titles through S(), which wants the string pool, which
 * wants far memory -- a seam this port does not have yet. A stub S() returning
 * the panel's own name is enough to see the frame.
 */
#include "../../c128/src/vdc.h"
#include "../../c128/src/layout.h"
#include "../../core/ega.h"
#include "f256kern.h"

TREK_SIGNATURE;
__attribute__((used, retain)) volatile unsigned char ran;
__attribute__((used, retain)) volatile unsigned int frames;

const char *S(unsigned int id)
{
    static const char *names[] = {
        "SHORT RANGE SCAN", "STATUS", "LONG RANGE CHART", "SYSTEMS STATUS",
        "LASERS", "COMMAND", "MAIN VIEWER", "BADGE"
    };
    return names[id & 7];
}

int main(void)
{
    unsigned char c;

    ran = 0x11;
    vdc_init();
    draw_console();

    /* The sixteen EGA colours along the bottom margin row, in order, so the
       palette can be READ OFF THE PICTURE rather than trusted. Row 24 is the
       console's last; this is row 25, in the margin the offset creates. */
    for (c = 0; c < 16; c++) {
        scr_put((unsigned char)(2 + c * 3), 25, 160, c);        /* solid cell */
        scr_put((unsigned char)(3 + c * 3), 25, (unsigned char)(c < 10 ? 48 + c : 1 + c - 10), EGA_WHITE);
    }
    ran = 0x5A;

    /* AND EXERCISE wait_vsync IN THE DRIVER, not just in the probe that
       calibrated it. The probe's frame_count and this one are the same eight
       lines, but they are compiled in different translation units with
       different neighbours -- and this project has a miscompile on record
       where llvm-mos hoisted a store above the comparison that needed the old
       value. The host reads `frames` against its own clock; 60 Hz here means
       the driver's copy works, not just the probe's. */
    for (;;) { wait_vsync(); frames = frames + 1; }
}
