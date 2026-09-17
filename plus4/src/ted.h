#ifndef TED_H
#define TED_H

/* The Plus/4's TED, as this port uses it.
 *
 * WHAT MAKES IT NOT A VIC-II, which is the whole reason vic.c cannot be
 * linked here the way the C64 links it:
 *
 *   screen memory   $0C00, not $0400
 *   colour memory   $0800, not $D800 -- and it is PLAIN RAM, not a nybble
 *                   port, so a colour cell holds a whole byte
 *   a colour byte   LUMINANCE in bits 6-4, HUE in bits 3-0. Sixteen hues at
 *                   eight brightnesses is the 121 distinct colours this
 *                   machine is known for, and it means an EGA colour maps to
 *                   a byte rather than to an index.
 *
 * Both defaults sit below $1001, in the space a BASIC program never occupies,
 * so the console's 2,000 bytes of screen and colour cost this port NOTHING
 * out of its own region -- the same accident that makes the C64's $0400 free.
 */

#define TED_COLS 40
#define TED_ROWS 25

#define TED_SCREEN ((unsigned char *)0x0C00)
#define TED_COLRAM ((unsigned char *)0x0800)

/* $FF06 bit 4 blanks the display; bit 3 picks 25 rows over 24. $FF07 bit 7 is
   the reverse-video flag for the whole screen and must be CLEAR or every cell
   inverts. $FF15 is background 0, $FF19 the border. */
/* $FF12 bit 2: TED enables ROM for its CHARACTER GENERATOR fetch. Clear it
   and TED reads the charset out of DRAM -- which, with this port's RAM banked
   in under the ROM, is the program itself. The other bits belong to the sound
   driver (voice 1's frequency high bits), so both files touch this register
   and both must read-modify-write. */
#define TED_CHGEN  (*(volatile unsigned char *)0xFF12)
#define TED_CHGEN_ROM 0x04

/* Pause/resume the voices across a blocking disk load -- see tedsnd.c. Kept
   in this port's own header rather than c128/src/sid.h, because no other port
   needs it: they either advance the tune from an interrupt or do not block
   this long. */
void snd_hush(void);
void snd_unhush(void);

#define TED_CTRL1  (*(volatile unsigned char *)0xFF06)
#define TED_CTRL2  (*(volatile unsigned char *)0xFF07)
#define TED_BGND   (*(volatile unsigned char *)0xFF15)
#define TED_BORDER (*(volatile unsigned char *)0xFF19)
#define TED_RASTER (*(volatile unsigned char *)0xFF1D)

/* A TED colour byte from a luminance and a hue. Black is hue 0 and ignores
   luminance, which is why the palette below never spends a level on it. */
#define TED_COL(lum, hue) ((unsigned char)(((lum) << 4) | ((hue) & 0x0F)))
#define TED_BLACK  0x00

#endif
