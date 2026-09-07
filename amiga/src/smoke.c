/* Amiga first light. One screenshot answers four questions:
 *   1. does the toolchain produce a binary this machine runs,
 *   2. does a 640x200 four-bitplane screen give 80x25 cells exactly,
 *   3. do all sixteen EGA colours come out on their own palette index,
 *   4. do the box-drawing glyphs -- which are this port's OWN artwork here,
 *      because topaz has letters at those screen codes -- actually join up.
 *
 * The console frame is drawn through the SHARED c128/src/layout.c, so this
 * also proves that file compiles and runs for 68000 against a video layer
 * that is nothing like a VDC.
 */
#include <stdio.h>
#include "../../c128/src/vdc.h"
#include "../../c128/src/layout.h"
#include "../../core/strpool.h"

/* A STUB STRING POOL, and it exists so this can draw the REAL console frame.
   layout.c fetches every panel title through S(), which on a finished port
   reads STRINGS.DAT out of far memory -- machinery this milestone has not
   built. These seven titles are the pool's own text for the ids layout.c
   asks for; anything else answers with its number so a wrong id is visible
   rather than blank. Replaced by c128/src/strpool.c once the file seam is in. */
const char *S(StrId id) {
    switch (id) {
        case 127: return "LASERS";
        case 145: return "SHORT RANGE SCAN";
        case 146: return "STATUS";
        case 147: return "CHART OF KNOWN GALAXY";
        case 148: return "COMMAND";
        case 149: return "MAIN VIEWER";
        case 150: return "SYSTEMS STATUS";
        default:  return "?";
    }
}

static const char *names[16] = {
    "BLACK","BLUE","GREEN","CYAN","RED","MAGENTA","BROWN","LTGRAY",
    "DKGRAY","LTBLUE","LTGREEN","LTCYAN","LTRED","LTMAGENTA","YELLOW","WHITE" };

int main(void) {
    int i;

    vdc_init();

    /* Sixteen colours, each label written in its own colour. An unreadable
       name means that index is wrong, and index 0 is meant to be invisible. */
    for (i = 0; i < 16; i++)
        scr_puts((unsigned char)(42 + (i / 8) * 19),
                 (unsigned char)(12 + (i % 8)), names[i], (unsigned char)i);

    /* THE REAL CONSOLE FRAME, through the shared layout.c -- nine panels, the
       titles in their borders, and the junctions where panels share a row.
       Those junctions shipped broken on three ports until 2026-09-06, and
       here they are drawn from GLYPHS THIS PORT AUTHORED rather than from a
       character ROM, so whether they join is a real question. */
    draw_console();

    scr_puts(42, 21, "640X200, FOUR PLANES, 80X25 CELLS", 10);
    scr_puts(42, 22, "SIXTEEN EGA COLOURS ON THEIR OWN INDEX", 10);
    scr_puts(42, 23, "PRESS RETURN TO LEAVE", 12);

    /* Nothing to poll yet -- the input seam is not built. AmigaDOS gives us a
       blocking read for free, and the screen stays up until it returns. */
    getchar();

    vdc_shutdown();
    plat_exit();
    return 0;
}
