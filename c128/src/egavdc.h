#ifndef EGAVDC_H
#define EGAVDC_H

#include "../../core/ega.h"

/* EGA colour index -> VDC colour index.
 *
 * Both chips carry the same sixteen colours -- eight hues at two intensities
 * -- but they order the index bits differently:
 *
 *     EGA/CGA :  I R G B   (bit3 = intensity)
 *     VDC     :  R G B I   (bit0 = intensity)
 *
 * which makes the conversion a 4-bit rotate left. Verified entry by entry
 * against the VDC table documented in commodore-uno/c128/src/vdc.h:
 *
 *     EGA  0 black      -> VDC  0 black
 *     EGA  1 blue       -> VDC  2 dark blue
 *     EGA  2 green      -> VDC  4 dark green
 *     EGA  3 cyan       -> VDC  6 dark cyan
 *     EGA  4 red        -> VDC  8 dark red
 *     EGA  5 magenta    -> VDC 10 dark purple
 *     EGA  6 brown      -> VDC 12 dark yellow   <-- only inexact entry
 *     EGA  7 lt gray    -> VDC 14 light gray
 *     EGA  8 dk gray    -> VDC  1 dark gray
 *     EGA  9 lt blue    -> VDC  3 light blue
 *     EGA 10 lt green   -> VDC  5 light green
 *     EGA 11 lt cyan    -> VDC  7 light cyan
 *     EGA 12 lt red     -> VDC  9 light red
 *     EGA 13 lt magenta -> VDC 11 light purple
 *     EGA 14 yellow     -> VDC 13 light yellow
 *     EGA 15 white      -> VDC 15 white
 *
 * Fifteen of sixteen are exact. EGA 6 is brown only because the CGA/EGA
 * palette special-cases it; the VDC has no such quirk and renders olive.
 * That affects the galaxy chart's base highlight, which the manual calls
 * orange (l.289).
 *
 * NOTE: NOTES.md said the VDC "shares EGA's RGBI palette so the original
 * colours reproduce exactly". True for the colours, but not for the indices
 * -- passing an EGA number straight to the VDC gives the wrong hue. That is
 * exactly the bug commodore-uno's vdc.h documents having shipped once.
 */
#define EGA_TO_VDC(e) ((unsigned char)(((((unsigned char)(e)) << 1) | \
                                        (((unsigned char)(e)) >> 3)) & 0x0F))

#endif
