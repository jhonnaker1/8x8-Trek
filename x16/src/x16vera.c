/* VERA text output for the Commander X16.
 *
 * VERA is reached through a register block at $9F20: point the address port
 * at a VRAM address, choose an auto-increment, then read or write the data
 * port. The address is 17 bits -- the low 16 in ADDR_L/ADDR_M, bit 16 in
 * ADDR_H bit 0, with ADDR_H bits 4-7 holding the increment code (1 = +1).
 */
#include <stdint.h>
#include <string.h>
#include "x16vera.h"

#define VERA_ADDR_L  (*(volatile unsigned char *)0x9F20)
#define VERA_ADDR_M  (*(volatile unsigned char *)0x9F21)
#define VERA_ADDR_H  (*(volatile unsigned char *)0x9F22)
#define VERA_DATA0   (*(volatile unsigned char *)0x9F23)
#define VERA_CTRL    (*(volatile unsigned char *)0x9F25)

/* DCSEL=0 display-composer registers. DC_VSCALE is a fractional vertical
   zoom: 128 is 1:1, which is what gives the default 480 lines and so 60 text
   rows of 8 pixels. */
#define VERA_DC_VSCALE (*(volatile unsigned char *)0x9F2B)
#define VSCALE_2X 64

#define VRAM_TEXT    0x1B000UL   /* KERNAL's text map: cell = char, colour */
#define VRAM_PAL     0x1FA00UL   /* 256 entries x 2 bytes */
#define MAP_STRIDE   128         /* CELLS per row, not columns -- see the .h */

/* The message log's home: spare VRAM well clear of the text map, the palette
   and the sprite tables. The C128 puts it in spare VDC RAM for the same
   reason -- storage that costs no main memory. */
#define VRAM_LOG     0x14000UL

static unsigned long log_cursor;

/* EGA's palette, the same table the MEGA65 loads. EGA's two-bits-per-gun
   levels are 0x00, 0x55, 0xAA and 0xFF -- every one a DUPLICATED NIBBLE, so
   taking the high nibble for VERA's 4-bit guns is exact rather than a
   rounding. That property is why this table ports without a conversion. */
static const unsigned char ega_r[16] = {
    0x00,0x00,0x00,0x00,0xAA,0xAA,0xAA,0xAA,
    0x55,0x55,0x55,0x55,0xFF,0xFF,0xFF,0xFF };
static const unsigned char ega_g[16] = {
    0x00,0x00,0xAA,0xAA,0x00,0x00,0x55,0xAA,
    0x55,0x55,0xFF,0xFF,0x55,0x55,0xFF,0xFF };
static const unsigned char ega_b[16] = {
    0x00,0xAA,0x00,0xAA,0x00,0xAA,0x00,0xAA,
    0x55,0xFF,0x55,0xFF,0x55,0xFF,0x55,0xFF };

/* THE X16 BOOTS IN ISO-8859-1, AND THAT HAS NO BOX-DRAWING GLYPHS.
 *
 * This cost four wrong attempts at the charset, and the rule was right all
 * along: the machine was in the wrong mode. Measured from the font bitmaps in
 * VRAM -- tile $41 is 18 3C 24 66 7E 66 66 00, an unmistakable `A`, so the
 * read was sound; tile $C0 is 30 18 3C 24 66 7E 66 00, the SAME `A` with an
 * accent stroke over it. $C0 is `A-grave`, not a horizontal line. Under ISO
 * the port's ASCII text renders perfectly and every graphics code lands on a
 * letter, which is exactly the symptom seen.
 *
 * screen_set_charset ($FF62) takes 1 = PETSCII upper-case/graphics, which is
 * where the box-drawing set lives -- and where c128/src/layout.h's PETSCII
 * values point. The console is upper-case throughout, as on the C128, so
 * losing lower case costs nothing. */
static void set_screencode_charset(void) {
    __asm__ volatile("lda #2\n jsr $FF62\n" ::: "a", "x", "y", "memory");
}

static void vera_seek(unsigned long addr, unsigned char inc) {
    VERA_CTRL   = 0;                      /* ADDRSEL 0 -> data port 0 */
    VERA_ADDR_L = (unsigned char)(addr & 0xFF);
    VERA_ADDR_M = (unsigned char)((addr >> 8) & 0xFF);
    VERA_ADDR_H = (unsigned char)(((addr >> 16) & 1) | (inc << 4));
}

/* A VERA palette entry is two bytes: (Green<<4)|Blue, then Red. */
static void load_ega_palette(void) {
    unsigned char i;
    vera_seek(VRAM_PAL, 1);
    for (i = 0; i < 16; i++) {
        VERA_DATA0 = (unsigned char)(((ega_g[i] >> 4) << 4) | (ega_b[i] >> 4));
        VERA_DATA0 = (unsigned char)(ega_r[i] >> 4);
    }
}

/* CHARSET 2 INDEXES BY C64 SCREEN CODE, which is the currency shared ui.c and
 * layout.c already speak -- so glyphs need NO translation at all. Measured
 * from the font bitmaps rather than reasoned: under charset 2, tile $B0 is a
 * reversed digit `0` and tile $C0 is a reversed horizontal line, i.e. $30 is
 * `0` and $40 is the line, exactly the C64 screen-code layout. The X16 boots
 * in ISO-8859-1 instead, where $C0 is `A-grave`, which is why the first
 * attempt drew the console frame as letters.
 *
 * THE SPLIT IS THE WHOLE POINT, and m65vid.c states it: STRINGS are converted,
 * GLYPHS ARE NOT. The box-drawing set lives at 64..127, exactly the range an
 * ASCII conversion rewrites, so putting that conversion in scr_put -- as this
 * file did for three attempts -- turns every panel border into letters. It
 * belongs in scr_puts.
 */
static unsigned char ascii_to_screencode(char c) {
    unsigned char u = (unsigned char)c;
    if (u >= 32 && u <= 63) return u;           /* space, digits, punctuation */
    if (u >= 64 && u <= 95) return (unsigned char)(u - 64);
    if (u >= 97 && u <= 122) return (unsigned char)(u - 96);
    if (u >= 193 && u <= 218) return (unsigned char)(u - 192);
    return 32;
}

/* VERA's LINE flag, the same frame source x16snd.c uses -- see the long note
   there. The KERNAL's jiffy clock does NOT run for a program that has taken
   the machine over: RDTIM returns zero forever, which is what left the music
   silent and froze snd_beep. */
#define VERA_ISR (*(volatile unsigned char *)0x9F27)
#define ISR_LINE 0x02

static unsigned long cell_addr(unsigned char x, unsigned char y) {
    return VRAM_TEXT + ((unsigned long)y * MAP_STRIDE + x) * 2UL;
}

/* THE CONSOLE IS 80x25 AND VERA'S TEXT MODE IS 80x60, so at 1:1 the game drew
   in the top 25 rows and left the bottom 35 empty -- correct, and a waste of
   the screen. Halving the vertical scale doubles each row's height: 240 lines,
   30 rows of 16 pixels, of which the console uses 25. Columns are untouched at
   80. This is display-side only; the text map, its 128-cell stride and every
   address in this file are unaffected. */
static void stretch_rows(void) { VERA_DC_VSCALE = VSCALE_2X; }

void vdc_init(void) {
    /* GOLDEN RAM IS OUTSIDE .bss, SO THE CRT DOES NOT ZERO IT. x16.ld moves
       io_buf and the hall-of-fame table to $0400..$07FF to reclaim 906 bytes
       of the main region; that puts them outside __do_zero_bss's range, and
       the hall of fame reads as garbage rather than an empty table if this is
       skipped. First statement in the first function the game calls. */
    memset((void *)0x0400, 0, 0x0400);

    stretch_rows();
    set_screencode_charset();
    load_ega_palette();
    scr_clear();
}

/* NOTHING TO DO. The X16 loads at $0801 in plain, unbanked RAM and its
   _fini is a bare RTS, so returning from main hands the machine back to BASIC
   the ordinary way. The C128 needs a reset because its code sits under the
   ROMs; this one does not. See the long note in c128/src/vdc.c. */
void plat_exit(void) { }

void vdc_shutdown(void) {
    scr_clear();
}

/* Waits one frame, off the KERNAL's jiffy clock.
 *
 * IT USED TO SPIN ON $9F28 as if that were a scanline counter. It is not --
 * with DCSEL=0 that address is IRQ_LINE_L, so the loop could never terminate.
 * Nothing in the shared UI calls wait_vsync (the console is entirely
 * event-driven; the only grep hit anywhere is a comment), so the hang was
 * latent rather than live -- but an API function that cannot return is a trap
 * left for whoever calls it first.
 *
 * RDTIM is the same source x16snd.c paces the music from, and it is
 * unambiguous where a raster counter is not: 60 ticks a second, no wrap to
 * misread. */
void wait_vsync(void) {
    unsigned int guard = 0;
    VERA_ISR = ISR_LINE;
    while (!(VERA_ISR & ISR_LINE) && ++guard) { }   /* bounded: never hang */
    VERA_ISR = ISR_LINE;
}

/* ALL SIXTY ROWS, not VDC_ROWS. VERA's text mode is 80x60 and the console
   uses the top 25; clearing only those left the KERNAL's remaining rows on
   screen, and loading the EGA palette over the top turned them brown. First
   light showed it as a brown band under the console. */
#define VERA_ROWS 60

void scr_clear(void) {
    unsigned char y, x;
    for (y = 0; y < VERA_ROWS; y++) {
        vera_seek(cell_addr(0, y), 1);
        for (x = 0; x < VDC_COLS; x++) {
            VERA_DATA0 = ' ';
            VERA_DATA0 = 0;               /* black on black */
        }
    }
}

/* colour is an EGA index 0-15. VERA's cell colour byte is
   (background << 4) | foreground, and the console draws on black. */
void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color) {
    vera_seek(cell_addr(x, y), 1);
    VERA_DATA0 = ch;                       /* raw screen code -- see above */
    VERA_DATA0 = (unsigned char)(color & 0x0F);
}

void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color) {
    while (*s) {
        if (x >= VDC_COLS) return;
        scr_put(x++, y, ascii_to_screencode(*s++), color);
    }
}

void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color) {
    unsigned char r, i;
    unsigned char c = (unsigned char)(color & 0x0F);
    for (r = 0; r < h; r++) {
        vera_seek(cell_addr(x, (unsigned char)(y + r)), 1);
        for (i = 0; i < w; i++) { VERA_DATA0 = ch; VERA_DATA0 = c; }
    }
}

void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color) {
    scr_fill_rect(x, y, w, 1, ch, color);
}

void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color) {
    unsigned char i;
    for (i = 0; i < h; i++) scr_put(x, (unsigned char)(y + i), ch, color);
}

/* The message-log byte stream. Not VERA registers despite the names: ui.c
   inherited them from the C128, where they really were the VDC's ports. */
void vdc_set_address(unsigned int addr) {
    log_cursor = VRAM_LOG + addr;
    vera_seek(log_cursor, 1);
}

void vdc_data_write(unsigned char value) {
    VERA_DATA0 = value;
    log_cursor++;
}

unsigned char vdc_data_read(void) {
    log_cursor++;
    return VERA_DATA0;
}
