#ifndef EGAGIME_H
#define EGAGIME_H

/* EGA colour -> the GIME's eight text foregrounds.
 *
 * THE MAP IS A FOLD, AND WHICH EIGHT SURVIVE IS DERIVED RATHER THAN CHOSEN.
 * tools/check_colours.py reads core/ega.h and ui.c every run and reports the
 * colours that carry GAME RULES -- the four Mongol ship types, the chart's
 * Mongol and base markers, the department colours and the message line. There
 * are eight of them and the GIME has exactly eight foregrounds, so every rule
 * survives and nothing is a judgement call.
 *
 * What folds is decoration, and the tool prints that too:
 *     BLUE=LTBLUE  CYAN=LTCYAN  RED=LTRED  MAGENTA=LTMAGENTA
 *     BROWN=YELLOW  DKGRAY=LTGRAY=WHITE
 * Seven of the sixteen. The C64 folds two; this machine folds seven; both
 * play the same game.
 *
 * ADD A NEW EGA_CHART_* OR DEPARTMENT COLOUR AND THIS MAP MAY STOP BEING
 * ENOUGH -- check_colours will say so on the next `make ports` rather than
 * the screen saying it later.
 */

#define GIME_GREEN    0
#define GIME_CYAN     1
#define GIME_RED      2
#define GIME_MAGENTA  3
#define GIME_BROWN    4
#define GIME_LTGRAY   5
#define GIME_LTBLUE   6
#define GIME_LTGREEN  7

/* Indexed by the EGA number in core/ega.h. Black maps to light grey because
   this is a FOREGROUND table and nothing draws in the background colour --
   a cell that wants to be blank is a space, not a black glyph. */
static const unsigned char ega_to_gime[16] = {
    GIME_LTGRAY,   /*  0 BLACK      -- see above */
    GIME_LTBLUE,   /*  1 BLUE       */
    GIME_GREEN,    /*  2 GREEN      */
    GIME_CYAN,     /*  3 CYAN       */
    GIME_RED,      /*  4 RED        */
    GIME_MAGENTA,  /*  5 MAGENTA    */
    GIME_BROWN,    /*  6 BROWN      */
    GIME_LTGRAY,   /*  7 LTGRAY     */
    GIME_LTGRAY,   /*  8 DKGRAY     */
    GIME_LTBLUE,   /*  9 LTBLUE     */
    GIME_LTGREEN,  /* 10 LTGREEN    */
    GIME_CYAN,     /* 11 LTCYAN     */
    GIME_RED,      /* 12 LTRED      */
    GIME_MAGENTA,  /* 13 LTMAGENTA  */
    GIME_BROWN,    /* 14 YELLOW     */
    GIME_LTGRAY    /* 15 WHITE      */
};

#define EGA_TO_VDC(c) (ega_to_gime[(c) & 15])

#endif
