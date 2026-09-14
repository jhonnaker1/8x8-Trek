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

extern const Panel panels40[PANEL_COUNT];
extern const unsigned char panel40_page[PANEL_COUNT];

/* Titles are passed in rather than fetched. The real build hands pooled
   strings through S(); a test hands literals and needs no string pool, no far
   memory and no disk. NULL, or a NULL entry, draws borders only. */
void draw_console40(unsigned char page, const char *const *titles);

#endif
