#ifndef M65VID_H
#define M65VID_H

/* MEGA65 native-mode video, presenting the SAME API as c128/src/vdc.h.
 *
 * The two ports share `core/` and the console layout, so the seam between
 * them is this handful of names. Anything above it -- layout.c, and in time
 * ui.c and main.c -- should not know which machine it is drawing on.
 *
 * WHY NATIVE MODE. A $0801 binary runs the MEGA65 in C64 mode, where 80
 * columns do not exist and VIC-IV's H640 bit does nothing. Linking at $2001
 * with a BASIC 65 header is what actually selects C65/native mode; there H640
 * is real, and colour RAM is the full 2000 cells at $FF80000 through the
 * 45GS02's 32-bit addressing rather than the 1K window at $D800.
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

#endif
