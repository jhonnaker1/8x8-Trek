#ifndef LAYOUT40_H
#define LAYOUT40_H

#include "layout.h"

/* The nine-panel console on a 40x25 grid, as TWO PAGES of the same console.
 *
 * NOT A REDESIGN, AND THAT IS THE FINDING (NOTES.md item 57). The 80-column
 * console is two 40-column screens side by side:
 *
 *     rows 11..17  LASERS/COMMAND + VIEWER   cols 0..39   ALREADY EXACTLY 40
 *     rows 17..24  BADGE + SYSTEMS           cols 0..39   ALREADY EXACTLY 40
 *     rows  0..10  SCAN + STATUS             cols 0..40   one over
 *     rows  0..10  CHART                     cols 41..79  } together
 *     rows 11..24  the message region        cols 40..79  } EXACTLY 40x25
 *
 * So six of the eight panels and the whole message region are used at their
 * 80-column coordinates, unchanged. Only two panels move at all.
 *
 * AND THE "ONE OVER" IS ONE BLANK COLUMN, not a squeeze. SCAN puts its row
 * labels at x+2 and its grid at x+4 + col*2, so with eight cells the last
 * lands at x+18 and the right border sits at x+20 -- COLUMN x+19 IS PADDING.
 * Narrowing SCAN from 21 to 20 and sliding STATUS one left fits the band in
 * forty columns WITH NO CHANGE TO THE CELL PITCH. The scope budgeted for
 * redrawing the scanner at one column per cell; it is not needed, and the
 * scan looks exactly as it does at 80 columns.
 *
 * WHAT THIS DOES NOT DECIDE. Splitting down the middle puts COMMAND on the
 * tactical page and the messages on the other one, so an order is typed on
 * one page and its result read on the other. Item 57 records three answers to
 * that and recommends trading the BADGE for a two-box message strip. THIS
 * FILE DELIBERATELY IMPLEMENTS NEITHER: it is the straight split, so the
 * decision can be made against a picture rather than against arithmetic.
 */

#define PAGE_TACTICAL  0    /* scanner, status, lasers, command, badge, systems */
#define PAGE_CHART     1    /* long range chart, and the message region */

#define SCR40_COLS    40
#define SCR40_ROWS    25

/* The message region on the chart page. Same size as the 80-column one --
   MSG_W is already 40 and MSG_WIDTH already 36, so the game's narration is
   ALREADY a 40-column design and reflows not at all. Only x moves. */
#define MSG40_X    0
#define MSG40_Y   11
#define MSG40_W   40
#define MSG40_H   14

/* THE TABLE IS NAMED `panels`, NOT `panels40`, AND THAT IS THE WHOLE TRICK.
   layout.h already declares `extern const Panel panels[PANEL_COUNT]` and
   `void draw_console(void)`; ui.c reads the table thirteen times and calls
   draw_console once, and uses NO width constant of its own. So a 40-column
   build swaps WHICH layout .c is linked and the 2,501-line shared UI is not
   touched at all. The scope feared a second axis of #if through ui.c; the
   measurement says the conditional belongs in layout.h, which is 112 lines. */
extern const unsigned char panel40_page[PANEL_COUNT];

/* Which half of the console is on screen. A plain variable rather than an
   accessor: ui.c's paging test reads it once per redraw and the C64 will read
   it in an inner loop eventually. */
extern unsigned char layout40_page;

#endif
