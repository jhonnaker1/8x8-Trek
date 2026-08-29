#ifndef EGA_H
#define EGA_H

/* The core speaks EGA colour indices, because the original DOS game is the
   reference and every constant we verify in DOSBox-X is quoted in these
   numbers. Each platform layer translates to its own hardware palette; see
   c128/src/egavdc.h for the VDC mapping.

   EGA mode 10h's default 16 are the CGA-compatible set, and the index bits
   are I R G B -- bit3 intensity, then red, green, blue. */

#define EGA_BLACK         0   /* 0000 */
#define EGA_BLUE          1   /* 0001 */
#define EGA_GREEN         2   /* 0010 */
#define EGA_CYAN          3   /* 0011 */
#define EGA_RED           4   /* 0100 */
#define EGA_MAGENTA       5   /* 0101 */
#define EGA_BROWN         6   /* 0110 -- NOT dark yellow; see note below */
#define EGA_LTGRAY        7   /* 0111 */
#define EGA_DKGRAY        8   /* 1000 */
#define EGA_LTBLUE        9   /* 1001 */
#define EGA_LTGREEN      10   /* 1010 */
#define EGA_LTCYAN       11   /* 1011 */
#define EGA_LTRED        12   /* 1100 */
#define EGA_LTMAGENTA    13   /* 1101 */
#define EGA_YELLOW       14   /* 1110 */
#define EGA_WHITE        15   /* 1111 */

/* Index 6 is the one entry that is not simply "dark yellow": the CGA/EGA
   default palette special-cases it to brown (roughly AA5500) rather than the
   olive (AAAA00) the bit pattern would imply. Hardware without that quirk
   renders it olive. Avoid relying on index 6 where the difference would
   matter, or accept the shift. */

/* Colour as information -- from the manual, l.272 and l.286-292. These are
   game rules, not decoration, so they live in the core rather than in a
   platform's UI layer. */
#define EGA_MONGOL_BATTLESHIP  EGA_LTBLUE
#define EGA_MONGOL_COMMAND     EGA_RED
#define EGA_MONGOL_SCOUT       EGA_MAGENTA
#define EGA_MONGOL_SUPPLY      EGA_GREEN
#define EGA_CHART_MONGOL       EGA_RED     /* quadrants holding Mongols */
#define EGA_CHART_BASE         EGA_BROWN   /* "orange" in the manual */

/* The Vandal Death Pod, BINARY 2026-08-28. Every other short-range shape
   takes its colour as an argument -- the Mongol drawer at 0x02668C passes
   [bp+6] straight to SetColor -- but the Vandal's own drawer at 0x0266CB
   pushes a literal 7 and has no colour parameter at all. It is the one
   object on the scanner that is always the same colour. */
#define EGA_VANDAL             EGA_LTGRAY

#endif
