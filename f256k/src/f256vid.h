#ifndef F256VID_H
#define F256VID_H

/* Tiny Vicky, the F256K's video. The shared UI reaches all of this through
   c128/src/vdc.h; what is here is the machine, not the interface. */

/* THE I/O WINDOW IS PAGED, AND THE PAGE IS PART OF THE ADDRESS.
   $C000-$DFFF is four different things depending on $0001:

       0   Vicky's registers, the colour LUTs, the PSGs, the SD card
       1   the text FONT: 2K at $C000, 8 bytes per glyph, plus a second set
       2   the CHARACTER matrix
       3   the COLOUR matrix

   Every access below states its page. The PSG tone probe was silent for a
   whole run because $D608 is the sound chip only on page 0. */
#define F256_IO       (*(volatile unsigned char *)0x0001)
#define F256_IO_REGS  0
#define F256_IO_FONT  1
#define F256_IO_CHAR  2
#define F256_IO_COLOR 3

#define F256_MATRIX ((volatile unsigned char *)0xC000)   /* pages 2 and 3 */
#define F256_FONT   ((volatile unsigned char *)0xC000)   /* page 1 */

/* Vicky registers, page 0. */
#define VKY_MCR_L  (*(volatile unsigned char *)0xD000)
#define VKY_MCR_H  (*(volatile unsigned char *)0xD001)
#define VKY_BG_B   (*(volatile unsigned char *)0xD00D)
#define VKY_BG_G   (*(volatile unsigned char *)0xD00E)
#define VKY_BG_R   (*(volatile unsigned char *)0xD00F)
#define VKY_FG_LUT ((volatile unsigned char *)0xD800)    /* 16 x 4, B G R x */
#define VKY_BG_LUT ((volatile unsigned char *)0xD840)

#define MCR_TEXT     0x01    /* $D000: text mode on */
#define MCR_GRAPHICS 0x04
#define MCR_H_400    0x01    /* $D001 bit 0: 400 lines instead of 480 */
#define MCR_H_DBL_X  0x02    /* 40 columns -- NOT wanted, see f256vid.c */
#define MCR_H_DBL_Y  0x04    /* 8x16 cell: 80x30 instead of 80x60 */
#define MCR_H_FONT1  0x20    /* the second font set at $C800 */

/* THE SCREEN IS 80x30 AND THE CONSOLE IS 80x25. Everything the shared UI
   draws is in console coordinates; the driver adds the offset so that no
   shared code and no layout table has to know this machine is taller. */
#define F256_SCREEN_ROWS 30
#define F256_ROW_OFFSET  2

/* The physical matrix stride. It is 80 whether or not the rows are doubled --
   DOUBLE_Y changes the cell height, not the row length. */
#define F256_STRIDE 80

/* The machine ID at $D6A7: $02 for an F256 Jr, $12 for an F256K. */
#define VKY_MACHINE_ID (*(volatile unsigned char *)0xD6A7)

void f256_font_build(void);

#endif
