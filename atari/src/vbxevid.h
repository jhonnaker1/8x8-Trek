#ifndef VBXEVID_H
#define VBXEVID_H

/* The video seam for the Atari 800XL + VBXE.
 *
 * SAME CONTRACT AS c128/src/vdc.h, function for function -- this port shares
 * ui.c, layout.c and main.c with the C128 and the MEGA65, and those call
 * scr_put/scr_puts/scr_fill_rect/scr_hline/scr_vline and nothing else. What
 * changes is everything underneath.
 *
 * VBXE (Video Board XE) is a video coprocessor for the Atari 8-bit line with
 * its own 512KB of VRAM and an "Overlay" display that rides on top of the
 * stock ANTIC/GTIA picture. Its text mode is a real char+attribute grid --
 * 80 columns, an independent foreground and background palette index per
 * cell, from a 1024-colour palette -- which is what puts this target in the
 * same cost class as the MEGA65 and the X16 rather than with the Amiga.
 *
 * THREE THINGS ARE DIFFERENT FROM EVERY OTHER 6502 PORT HERE:
 *
 *   1. VRAM IS REACHED THROUGH A WINDOW IN THE 6502 ADDRESS SPACE. MEMAC
 *      window A maps 4K of VBXE VRAM at $2000..$2FFF, and the program starts
 *      at $3000 for exactly that reason -- see atari.ld, and the crash that
 *      taught it.
 *   2. THERE IS NO CHARACTER GENERATOR TO INHERIT. The C128 uses the one its
 *      KERNAL already placed in VDC RAM and the MEGA65 the C65's; here the
 *      font is BUILT at init from the Atari OS ROM's own glyphs plus this
 *      port's authored box-drawing set, and uploaded to VRAM. See font_build.
 *   3. THE PALETTE IS PROGRAMMABLE, so EGA's own sixteen go into entries
 *      0..15 and TREK_COLOUR_IS_EGA makes the mapping the identity, exactly
 *      as on the MEGA65.
 */

#define VDC_COLS 80
#define VDC_ROWS 25

void vdc_init(void);
void vdc_shutdown(void);
void plat_exit(void);
void wait_vsync(void);
void scr_clear(void);

/* NOTE the split, and it is the same one vdc.h documents: scr_puts takes
   ASCII and converts, every other routine takes a RAW C64/C128 SCREEN CODE.
   The console's box-drawing glyphs are screen codes 64..127 in shared
   layout.h, which is precisely the range the ASCII converter rewrites. */
void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color);
void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color);
void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color);
void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color);
void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color);

/* THE MESSAGE LOG'S BACKING STORE. Named for the C128's VDC because ui.c is
   shared and speaks that API: a cursor into spare video memory, set once and
   then read or written a byte at a time. Here it is VBXE VRAM above the
   screen and the font, which is 512K rather than the VDC's spare 4K.
   The address is the C128's, unchanged -- this maps it. */
void vdc_set_address(unsigned int addr);
void vdc_data_write(unsigned char value);
unsigned char vdc_data_read(void);

/* VRAM BEYOND THE SEAM, for the overlay loader and far memory, both of which
   live in VBXE VRAM on this port rather than on disk or in banked RAM.
   `bank` is in 4K units -- the MEMAC window's granularity -- and `win` is
   the CPU pointer the window appears at. */
#define VBXE_WIN ((unsigned char *)0x2000)
#define VBXE_WIN_SIZE 0x1000
void vbxe_bank(unsigned char bank);

#endif
