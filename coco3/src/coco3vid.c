/* Video for the CoCo 3 + SuperSprite FM+ -- STUB.
 *
 * The seam is c128/src/vdc.h: an 80x25 grid of cells, each a glyph plus a
 * foreground colour on black.
 *
 * THIS FILE EXISTS SO THE WHOLE GAME CAN BE LINKED AND MEASURED before any of
 * it is real, which is how every port here opened. What the scope established
 * and this will implement (NOTES.md, "SCOPE: the CoCo 3 + SuperSprite FM+"):
 *
 *   - The V9958 is at $FF78 data / $FF79 address-status, $FF7A palette,
 *     $FF7B register indirect -- the MSX $98/$99/$9A/$9B layout moved into
 *     the CoCo's slot window. CONFIRMED by writing VRAM and reading it back,
 *     not inferred from a table.
 *   - SCREEN 7 / GRAPHIC6: 512x212, sixteen colours per pixel. A six-pixel
 *     font puts 85 columns in that 512 and eight-pixel rows give 26.
 *   - Blit cost is MEASURED: 10.625 cycles a byte in a linear fill, and 257
 *     cycles a cell when a whole text line is walked scanline-wise against
 *     512 blitting cell-at-a-time. The loop's shape is worth a factor of two,
 *     so a dirty-cell scheme is the design rather than an optimisation.
 *
 * THE MESSAGE LOG'S BACKING STORE IS NOT ONE OF THESE STUBS. ui.c calls
 * vdc_set_address/vdc_data_write/vdc_data_read every time it files a message,
 * and stubbing them is how the Falcon shipped a broken message panel under a
 * comment claiming nothing called them. Here they get the card's VRAM, which
 * is where 2K of scrollback belongs on a machine with 64K of address space --
 * but until the VDP driver is real they are a plain array, because a stub
 * that silently swallows writes is the bug that cost a play session.
 */
#include <stdint.h>

#include "../../c128/src/vdc.h"

void vdc_init(void) { }
void vdc_shutdown(void) { }
void plat_exit(void) { }
void wait_vsync(void) { }
void scr_clear(void) { }

void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color)
{ (void)x; (void)y; (void)ch; (void)color; }

void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color)
{ (void)x; (void)y; (void)s; (void)color; }

void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color)
{ (void)x; (void)y; (void)w; (void)h; (void)ch; (void)color; }

void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color)
{ (void)x; (void)y; (void)w; (void)ch; (void)color; }

void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color)
{ (void)x; (void)y; (void)h; (void)ch; (void)color; }

/* THE MESSAGE LOG, real from the first build. 32 slots of 64 bytes from
   ui.c's LOG_BASE. This is 2K of a 64K address space and it is the first
   thing the card's VRAM should take. */
#define LOG_ORIGIN  0x1000
#define LOG_BYTES   (32 * 64)

static unsigned char logstore[LOG_BYTES];
static unsigned int  log_cursor;

void vdc_set_address(unsigned int addr)
{ log_cursor = (addr >= LOG_ORIGIN) ? (addr - LOG_ORIGIN) : 0; }

void vdc_data_write(unsigned char value)
{ if (log_cursor < LOG_BYTES) logstore[log_cursor] = value; log_cursor++; }

unsigned char vdc_data_read(void)
{
    unsigned char v = (log_cursor < LOG_BYTES) ? logstore[log_cursor] : 0;
    log_cursor++;
    return v;
}

/* C128-only: that machine's CRTC registers. */
unsigned char vdc_reg_read(unsigned char reg) { (void)reg; return 0; }
void vdc_reg_write(unsigned char reg, unsigned char value) { (void)reg; (void)value; }
