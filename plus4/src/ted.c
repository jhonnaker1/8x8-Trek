/* The TED video driver: 40x25 text, one colour byte a cell.
 *
 * MODELLED ON vic.c AND DELIBERATELY THE SAME SHAPE, because everything above
 * this file is shared and the C64 port proved the shape. What differs is the
 * three things ted.h lists -- where the screen is, where the colour is, and
 * that a colour is a byte rather than a nybble.
 *
 * NOINLINE ON EVERY PRIMITIVE, COPIED FROM vic.c WITH ITS REASON. There, LTO
 * inlining these into every call site made the 40-column game 1,817 bytes
 * BIGGER and overflowed its region by 358; out of line it was 249 bytes
 * smaller. A TED cell is two indexed stores exactly as a VIC-II cell is, so
 * the same trade applies and is taken up front rather than rediscovered.
 * See [[seam-costs-more-than-driver]].
 */
#include "../../c128/src/vdc.h"
#include "ted.h"

/* The same converter vic.c carries, for the same reason: scr_puts takes text
   and everything else takes a RAW screen code, and the box-drawing glyphs live
   at codes 64-127 -- precisely the range this rewrites. Routing a border
   through here would silently turn it into a letter. */
static unsigned char ascii_to_screencode(char c) {
    unsigned char u = (unsigned char)c;
    if (u >= 32 && u <= 63) return u;           /* space, digits, punctuation */
    if (u >= 64 && u <= 95) return u - 64;      /* ASCII @A-Z[\]^_ */
    if (u >= 97 && u <= 122) return u - 96;     /* ASCII lowercase */
    if (u >= 193 && u <= 218) return u - 192;   /* PETSCII A-Z */
    return 32;
}

void vdc_init(void) {
    /* 25 rows, display on, no vertical scroll. */
    TED_CTRL1 = 0x1B;
    /* REVERSE OFF. $FF07 bit 7 inverts the WHOLE screen and BASIC leaves it
       however it pleases; a port that does not clear it draws a correct
       console in negative. */
    TED_CTRL2 = (unsigned char)(TED_CTRL2 & 0x7F);
    TED_BGND   = TED_BLACK;
    TED_BORDER = TED_BLACK;
    scr_clear();
}

void vdc_shutdown(void) {
    TED_BGND   = TED_BLACK;
    TED_BORDER = TED_BLACK;
    scr_clear();
}

void wait_vsync(void) {
    /* $FF1D is the low eight bits of the raster. Waiting for it to leave the
       visible area and come back is one frame, near enough, and this is only
       ever used to pace a redraw. */
    while (TED_RASTER >= 200) { }
    while (TED_RASTER <  200) { }
}

void scr_clear(void) {
    unsigned int i;
    for (i = 0; i < (unsigned int)TED_COLS * TED_ROWS; i++) {
        TED_SCREEN[i] = 32;
        TED_COLRAM[i] = TED_BLACK;
    }
}

__attribute__((noinline))
void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color) {
    unsigned int off = (unsigned int)y * TED_COLS + x;
    TED_SCREEN[off] = ch;
    TED_COLRAM[off] = color;            /* a WHOLE byte -- see ted.h */
}

/* CLIPS AT THE RIGHT EDGE, and vic.c's note says why it must: at forty columns
   thirteen strings in the pool are longer than the row they are written to,
   and screen memory is linear, so an unbounded write lands on the START OF THE
   NEXT ROW. Clipping does not make a 49-character prompt right; it confines
   the damage to the row that owns it. */
__attribute__((noinline))
void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color) {
    unsigned int off = (unsigned int)y * TED_COLS + x;
    unsigned char col = x;
    const char *p;
    for (p = s; *p && col < TED_COLS; p++, off++, col++) {
        TED_SCREEN[off] = ascii_to_screencode(*p);
        TED_COLRAM[off] = color;
    }
}

__attribute__((noinline))
void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color) {
    unsigned int off = (unsigned int)y * TED_COLS + x;
    unsigned char col = x;
    unsigned char i;
    for (i = 0; i < w && col < TED_COLS; i++, off++, col++) {
        TED_SCREEN[off] = ch;
        TED_COLRAM[off] = color;
    }
}

__attribute__((noinline))
void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color) {
    unsigned char i;
    for (i = 0; i < h; i++) scr_put(x, (unsigned char)(y + i), ch, color);
}

__attribute__((noinline))
void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color) {
    unsigned char row;
    for (row = 0; row < h; row++) scr_hline(x, (unsigned char)(y + row), w, ch, color);
}
