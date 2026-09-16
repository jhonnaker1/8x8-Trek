#ifndef EGAGIME_H
#define EGAGIME_H

/* THE ONE INIT0 VALUE THIS PORT RUNS UNDER, in one place because two files
   write it and $FF90 DOES NOT READ BACK -- there is no way to OR a bit in
   safely, so both writers must write the whole thing.

       bit 7  COCO   0   the GIME's own text mode, not CoCo compatibility
       bit 6  MMUEN  0   the MMU is usable (see NOTES.md) but unused here
       bit 5  IEN    1   the GIME may assert /IRQ -- the sound timer needs it
       bit 4  FEN    0   nothing uses FIRQ; cmoc's `interrupt` saves no
                         registers, which FIRQ does not stack either
       bit 3  MC3    1   the RAM vector page at $FE00, where $FEF7 is the IRQ
                         slot the sound driver takes
       bit 2  MC2    1   **the standard SCS -- the WD1773's chip select.**
                         Clearing this is what "MMUEN breaks the disk" really
                         was, and what made a screen at $1000 look fatal.
       bits 1-0     00   ROM map; meaningless in all-RAM mode

   Measured against the disk: $04, $0C, $2C and $8C all read files; $00, $C8
   and $CA do not. src/lowinit.c and tools/coco3/bits3.c. */
#define GIME_INIT0 0x2C

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
