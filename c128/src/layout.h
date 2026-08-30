#ifndef LAYOUT_H
#define LAYOUT_H

/* The nine-panel console, mapped onto the VDC's 80x25 grid.
 *
 * This is the whole reason C128-VDC is the first port: EGA Trek's console is
 * an 80x25 character grid at an 8x14 cell, and the VDC's 80x25 maps to it
 * 1:1. Only per-cell pixel detail is lost (8x8 vs 8x14), never layout.
 */

/* PETSCII box-drawing glyphs as RAW SCREEN CODES (not PETSCII codes, not
   ASCII -- see the API note in vdc.h). Screen code = PETSCII - 0x40 for the
   0xA0-0xBF block, PETSCII - 0x80 for 0xC0-0xDF.

   VERIFIED: these were derived by hand and once shipped marked UNVERIFIED.
   c128/test/test_panels.c now asserts their actual bitmaps against VICE's
   C128 character ROM (chargen-390059-01.bin, 8 bytes per glyph in screen
   code order), so an invented screen code fails on the build machine rather
   than on screen. See "Screen codes are checkable" in NOTES.md. */
#define G_HLINE    64    /* PETSCII 0xC0  --  */
#define G_VLINE    93    /* PETSCII 0xDD  |   */
#define G_TL      112    /* PETSCII 0xB0  ,-  */
#define G_TR      110    /* PETSCII 0xAE  -.  */
#define G_BL      109    /* PETSCII 0xAD  `-  */
#define G_BR      125    /* PETSCII 0xBD  -'  */
#define G_TEE_L   107    /* PETSCII 0xAB  |-  */
#define G_TEE_R   115    /* PETSCII 0xB3  -|  */
#define G_TEE_D   114    /* PETSCII 0xB2  T   */
#define G_TEE_U   113    /* PETSCII 0xB1  _|_ */
#define G_CROSS    91    /* PETSCII 0xDB  +   */
#define G_BLOCK   160    /* reverse space -- a solid cell in the attr colour */

/* A panel with no title in its border. Not an empty pooled string: that
   would still cost an id and a far read to draw nothing. */
#define PANEL_NO_TITLE 0xFF

typedef struct {
    unsigned char x, y, w, h;
    /* AN ID, NOT A POINTER -- see sys_name_id in ui.c for why. The eight
       titles were 88 bytes of the binary; they are eight bytes now.
       PANEL_NO_TITLE for a panel that carries none. */
    unsigned char title;
} Panel;

/* MEASURED off a 640x350 capture of the original (2026-08-19); the full
   pixel table is in MEASURED.md. Column bands 0..20 | 20..40 | 41..79 and row
   bands 0..10 | 11..17 | 17..24, with panels sharing border rows and columns
   the way the original's do.

   One deliberate departure: the top band is ELEVEN rows here, not the ten the
   original uses. EGA Trek runs in a 640x350 graphics mode and draws text at
   whatever pitch it likes inside a frame -- the short range scan fits a header
   and eight rows into eight character rows' worth of height, and the chart
   fits a header, eight rows and a footer into the same. A character display
   cannot compress like that, so those panels need one row more than the
   measurement gives them. MAIN VIEWER pays for it: six interior rows in the
   original, five here.

   Everything downstream reads this table. */
#define PANEL_COUNT 8
extern const Panel panels[PANEL_COUNT];

/* The message region is NOT a panel, and this is the original's design, not a
   simplification of it. What looked like a COMMUNICATIONS panel above a
   DAMAGE REPORT panel is one region holding a stack of separately bordered
   boxes, one per message, interleaved in time order: a damage report can sit
   between two communications. Measured off a 640x350 capture -- four boxes at
   x323..636 (columns 40..79), tops at y141/189/237/285, borders yellow for
   COMMUNICATIONS and orange for DAMAGE REPORT, each completed box carrying a
   stardate stamp in the right end of its top border.

   Columns 40..79 of rows 11..24, which is where the original puts it. */
#define MSG_X   40
#define MSG_Y   11
#define MSG_W   40
#define MSG_H   14

/* PREVIOUS MESSAGES, the modal log viewer. Here rather than in ui.c because a
   test has to know where the box is to prove nothing is drawn outside it --
   which is exactly the bug of 2026-08-29, when msgv_draw() wrote the log's
   department and text with no bound at all and a long line crossed the right
   border onto the console behind. */
#define MSGV_X      25
#define MSGV_Y       6
#define MSGV_W      41
#define MSGV_LINES  11
#define MSGV_H      (MSGV_LINES + 4)   /* border, title, lines, footer, border */

/* Indices into panels[]. The UI addresses panels by name so that correcting
   the geometry above stays a single-site edit. */
#define P_SCAN      0
#define P_STATUS    1
#define P_CHART     2
#define P_LASERS    3
#define P_COMMAND   4
#define P_VIEWER    5
#define P_BADGE     6
#define P_SYSTEMS   7

void draw_console(void);


/* The STATE OF REPAIR report draws its own box rather than using the modal
   dialog: twelve systems plus three header lines do not fit the dialog's
   thirteen, and the original gives the report a panel of its own. Here rather
   than in ui.c so the tests can address it. */
#define REP_X    18
#define REP_Y     3
#define REP_W    44
#define REP_H    19

#endif
