#ifndef EGATED_H
#define EGATED_H

#include "../../core/ega.h"
#include "ted.h"

/* EGA colour index -> a TED colour BYTE.
 *
 * NOT AN INDEX, WHICH IS THE DIFFERENCE FROM egavic.h. A VIC-II colour is one
 * of sixteen palette entries and the C64 port's table is a permutation. TED
 * has no palette: a colour byte IS its own definition -- LUMINANCE in bits
 * 6-4, HUE in bits 3-0 -- so sixteen hues at eight brightnesses give 121
 * distinct colours and light/dark are the SAME hue at different luminances.
 *
 * THAT IS WHY THIS PORT FOLDS NOTHING. The C64 collapses two pairs because
 * its sixteen fixed entries do not contain everything EGA has; here light
 * blue is simply blue turned up, so all sixteen survive distinctly and
 * `tools/check_colours.py` reports the cost as none.
 *
 * THE VALUES ARE THE CHECKER'S, NOT MINE. `check_colours.py` has carried a
 * PLUS4 table since before this port existed, built from the same
 * `lum * 16 + hue` encoding, and it is the authority -- it is what `make
 * ports` runs. If the two ever disagree, that tool is right and this file is
 * the copy that drifted.
 *
 *   EGA 0 black      -> hue  0 black          (luminance means nothing here)
 *   EGA 1 blue       -> hue  6 lum 3
 *   EGA 2 green      -> hue  5 lum 3
 *   EGA 3 cyan       -> hue  3 lum 3
 *   EGA 4 red        -> hue  2 lum 3
 *   EGA 5 magenta    -> hue  4 lum 3          (TED hue 4 is purple)
 *   EGA 6 brown      -> hue  9 lum 2          TED hue 9 is orange; brown is
 *                                             it, dim -- the same judgement
 *                                             egavic.h records for the C64
 *   EGA 7 lt gray    -> hue  1 lum 4          hue 1 is white; grey is white,
 *                                             turned down
 *   EGA 8 dk gray    -> hue  1 lum 2
 *   EGA 9 lt blue    -> hue  6 lum 6          blue, turned up
 *   EGA 10 lt green  -> hue  5 lum 6
 *   EGA 11 lt cyan   -> hue  3 lum 6
 *   EGA 12 lt red    -> hue  2 lum 6
 *   EGA 13 lt magenta-> hue  4 lum 6
 *   EGA 14 yellow    -> hue  7 lum 7
 *   EGA 15 white     -> hue  1 lum 7
 */
static const unsigned char ega_to_ted[16] = {
    TED_COL(0, 0),   /*  0 black     */
    TED_COL(3, 6),   /*  1 blue      */
    TED_COL(3, 5),   /*  2 green     */
    TED_COL(3, 3),   /*  3 cyan      */
    TED_COL(3, 2),   /*  4 red       */
    TED_COL(3, 4),   /*  5 magenta   */
    TED_COL(2, 9),   /*  6 brown     */
    TED_COL(4, 1),   /*  7 lt gray   */
    TED_COL(2, 1),   /*  8 dk gray   */
    TED_COL(6, 6),   /*  9 lt blue   */
    TED_COL(6, 5),   /* 10 lt green  */
    TED_COL(6, 3),   /* 11 lt cyan   */
    TED_COL(6, 2),   /* 12 lt red    */
    TED_COL(6, 4),   /* 13 lt magenta*/
    TED_COL(7, 7),   /* 14 yellow    */
    TED_COL(7, 1)    /* 15 white     */
};

#define EGA_TO_VDC(c) (ega_to_ted[(c) & 0x0F])

#endif
