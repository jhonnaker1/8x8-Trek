/* The console frame, drawn by the REAL layout.c on a real GIME.
 *
 * The question this answers is a taste one and a picture settles it: this
 * font has no box-drawing glyphs, so every border comes out as ASCII `-`,
 * `|` and `+`. That is a visible difference from every other port and it is
 * worth LOOKING at before the rest of the port is built around it.
 *
 * Deliberately not the whole game: layout.c wants panel titles through S(),
 * which wants the string pool, which wants far memory -- a seam this port
 * does not have yet. A stub S() returning the panel's own name is enough to
 * see the frame, and nothing about the frame depends on the words.
 */
#include "../../c128/src/vdc.h"
#include "../../c128/src/layout.h"
#include "../../core/ega.h"
#include "egagime.h"



/* The pool is not here yet; the frame does not need it. */
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
    asm { orcc #$50 }
    vdc_init();
    draw_console();
    for (;;) { }
    return 0;
}
