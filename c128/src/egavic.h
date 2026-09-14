#ifndef EGAVIC_H
#define EGAVIC_H

#include "../../core/ega.h"

/* EGA colour index -> VIC-II colour index.
 *
 * THIS FILE IS WRITTEN TO BE SHARED WITH A C64 PORT, not to serve the C128
 * alone. The C128's VIC-IIe and the C64's VIC-II carry the SAME sixteen
 * Commodore colours in the same order, so one table serves both -- which is
 * the first concrete thing the 40-column work hands the C64 (NOTES.md item
 * 57). `tools/check_colours.py` holds the same mapping and asserts the eight
 * information-bearing colours stay distinct through it.
 *
 * IT IS A TABLE AND NOT A ROTATE. egavdc.h can convert with a 4-bit rotate
 * because EGA and the VDC are both IRGB orderings of the same sixteen
 * colours. The Commodore palette is NOT the EGA palette -- it is a different
 * set of sixteen, chosen by different people -- so this is a lookup and there
 * is no arithmetic shortcut to find.
 *
 *   EGA 0 black      -> VIC  0 black
 *   EGA 1 blue       -> VIC  6 blue
 *   EGA 2 green      -> VIC  5 green
 *   EGA 3 cyan       -> VIC  3 cyan
 *   EGA 4 red        -> VIC  2 red
 *   EGA 5 magenta    -> VIC  4 purple
 *   EGA 6 brown      -> VIC  9 brown      <-- see the note below
 *   EGA 7 lt gray    -> VIC 15 light grey
 *   EGA 8 dk gray    -> VIC 11 dark grey
 *   EGA 9 lt blue    -> VIC 14 light blue
 *   EGA 10 lt green  -> VIC 13 light green
 *   EGA 11 lt cyan   -> VIC  3 cyan       <-- COLLIDES, deliberately
 *   EGA 12 lt red    -> VIC 10 light red
 *   EGA 13 lt magenta-> VIC  4 purple     <-- COLLIDES, deliberately
 *   EGA 14 yellow    -> VIC  7 yellow
 *   EGA 15 white     -> VIC  1 white
 *
 * TWO ENTRIES COLLIDE AND IT IS CHECKED THAT THEY DO NOT MATTER. The
 * Commodore sixteen have no light cyan and no light magenta; they spend those
 * slots on orange and medium grey instead. So both fold onto their dark
 * siblings. NEITHER CARRIES INFORMATION -- EGA_LTCYAN is the panel border and
 * EGA_LTMAGENTA is unused by this game -- while all eight colours that ARE
 * game rules map uniquely. That is not an opinion: `make ports` runs
 * check_colours.py, which derives the eight out of core/ega.h and ui.c and
 * fails if any two of them land on the same VIC index.
 *
 * The visible consequence, and it is the honest cost: panel borders become
 * the same cyan as NAVIGATION's message text. On an 80-column VDC they differ.
 *
 * EGA 6 IS THE MANUAL'S "ORANGE". core/ega.h notes that CGA/EGA special-case
 * index 6 to brown rather than olive, and EGA_CHART_BASE uses it for a
 * starbase which the manual calls orange. The C64 HAS a true orange at 8.
 * Brown (9) is chosen here because it is what EGA actually renders and this
 * port's job is to be the same game, not a better-looking one -- but 8 is
 * defensible and the check passes either way. Changing it is a one-line
 * decision, recorded so nobody has to re-derive that it was a decision.
 */
static const unsigned char ega_to_vic[16] = {
    0,  6,  5,  3,  2,  4,  9, 15,
    11, 14, 13,  3, 10,  4,  7,  1
};

#define EGA_TO_VIC(e) (ega_to_vic[(e) & 0x0F])

#endif
