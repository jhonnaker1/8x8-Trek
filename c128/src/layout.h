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

   UNVERIFIED: derived by hand, not yet confirmed against a running VDC.
   Milestone 1 renders the frame precisely so these can be checked by eye. */
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

typedef struct {
    unsigned char x, y, w, h;
    const char *title;
} Panel;

/* PROVISIONAL. Traced by eye off reference/console_screenshot.jpg, which is a
   320x175 half-scale capture of the real 640x350 -- so one character cell is
   4x7 pixels there, too coarse to read exact column boundaries from.
   Open step in NOTES.md: run EGATREK_unpacked.exe in DOSBox-X, capture at
   full 640x350, and correct these numbers. Everything downstream reads this
   table, so that correction is a single-site edit. */
#define PANEL_COUNT 10
extern const Panel panels[PANEL_COUNT];

/* Indices into panels[]. The UI addresses panels by name so that correcting
   the geometry above stays a single-site edit. */
#define P_SCAN      0
#define P_STATUS    1
#define P_CHART     2
#define P_LASERS    3
#define P_COMMAND   4
#define P_VIEWER    5
#define P_COMMS     6
#define P_BADGE     7
#define P_SYSTEMS   8
#define P_DAMAGE    9

void draw_console(void);

#endif
