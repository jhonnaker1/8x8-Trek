#ifndef VDC_H
#define VDC_H

/* Low-level VDC (8563/8568) access for the C128 80-column display.
 *
 * Derived from commodore-uno/c128/src/vdc.c (MIT, same author). The hard-won
 * parts kept verbatim in spirit: every register goes through the indexed
 * control/data port pair with a ready poll, and the VIC-IIe raster register
 * still makes a free frame timer even though the VDC drives the picture.
 *
 * Deliberate differences from the uno driver, both documented at their sites
 * in vdc.c: this one uploads no character set (it uses the one the C128
 * KERNAL already placed in VDC RAM), and it does not re-assert register 28
 * every frame, because it never moves the character base away from the
 * KERNAL's own default and so has nothing to defend.
 */

unsigned char vdc_reg_read(unsigned char reg);
void vdc_reg_write(unsigned char reg, unsigned char value);

void vdc_set_address(unsigned int addr);
void vdc_data_write(unsigned char value);
unsigned char vdc_data_read(void);

#define VDC_COLS 80
#define VDC_ROWS 25
#define VDC_SCREEN_BASE 0x0000
#define VDC_ATTR_BASE 0x0800

/* VDC colour indices. The bit order is R G B I -- bit0 is intensity, so even
   entries are the dark half and odd the light. The core never uses these
   directly; it speaks EGA indices and the UI converts with EGA_TO_VDC (see
   egavdc.h). Named here only so the driver itself has a black to clear to. */
#define VDC_BLACK 0
#define VDC_WHITE 15

void vdc_init(void);
/* Puts back what vdc_init() changed on the machine's behalf, so returning to
   BASIC leaves a usable C128 rather than one still running at 2MHz. */
void vdc_shutdown(void);
void wait_vsync(void);
void scr_clear(void);

/* NOTE the split: scr_puts takes ASCII and converts; every other routine
   here takes a RAW screen code. The console's box-drawing glyphs sit at
   screen codes 64-127, which is precisely the range the ASCII converter
   rewrites -- so panel borders must not go through it. See vdc.c. */
void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color);
void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color);
void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color);
/* Writes `ch` with `color` across a run without re-seeking per cell. */
void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color);
void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color);

#endif
