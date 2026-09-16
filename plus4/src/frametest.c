/* The console frame, drawn by the REAL layout40.c on real TED hardware.
 *
 * The same bench the CoCo 3 port used, and the same caution about it: this
 * proves the VIDEO SEAM and nothing else. Every defect Jamie found in the
 * card-less CoCo 3 lived in ui.c with live game state, which no frame bench
 * reaches -- see [[jamie-plays-and-instruments-miss]]. What it does settle is
 * whether TED's screen and colour addresses, the 25-row setting, the reverse
 * flag and the colour byte encoding are right, which is four things that are
 * either all correct or produce an obviously wrong picture.
 *
 * Not the whole game: layout40.c wants panel titles through S(), which wants
 * the string pool, which wants far memory -- a seam this port does not have
 * yet. A stub S() returning the panel's own name is enough to see the frame.
 */
#include "../../c128/src/vdc.h"
#include "../../c128/src/layout40.h"
#include "../../core/ega.h"
#include "egated.h"

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
    vdc_init();
    draw_console();
    for (;;) { }
    return 0;
}
