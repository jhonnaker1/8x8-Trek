#ifndef X16VERA_H
#define X16VERA_H

/* Commander X16 video, presenting the SAME API as c128/src/vdc.h.
 *
 * The ports share core/ and the console layout, so the seam between them is
 * this handful of names -- layout.c, ui.c and main.c must not know which
 * machine they are drawing on. m65vid.h says the same thing for the MEGA65.
 *
 * VERA gives an 80x60 text map; the console is 80x25 and uses the top rows.
 * The map is 128 CELLS WIDE regardless of the visible width, which is the one
 * number that will bite anyone computing an address by hand: a row step is
 * 128 * 2 bytes, not 80 * 2.
 *
 * Colour needs no mapping. VERA's palette is 256 programmable entries, so
 * entries 0-15 are loaded with EGA's own values and TREK_COLOUR_IS_EGA makes
 * EGA_TO_VDC the identity -- exactly as on the MEGA65.
 */

#define VDC_COLS 80
#define VDC_ROWS 25

void vdc_init(void);
void vdc_shutdown(void);
void wait_vsync(void);
void scr_clear(void);
void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color);
void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color);
void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color);
void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color);
void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color);

/* The message log lives in spare VRAM, the way the C128's lives in spare VDC
   RAM and the MEGA65's in banked RAM. ui.c reaches it through these three
   names; they are a byte-stream cursor, not VDC registers, despite the names
   it inherited. */
void vdc_set_address(unsigned int addr);
void vdc_data_write(unsigned char value);
unsigned char vdc_data_read(void);

#endif
