/* Tiny Vicky: the F256K's video driver, against the shared c128/src/vdc.h.
 *
 * THREE THINGS ARE DIFFERENT FROM EVERY OTHER PORT HERE, all measured:
 *
 * 1. THE SCREEN IS BIGGER THAN THE GAME. 640x480 is 80x60 at an 8x8 cell --
 *    the console is 80x25, which would leave it stranded in the top third.
 *    $D001 bit 2 (DOUBLE_Y) makes the cell 8x16 and the grid 80x30, so the
 *    console fills the screen with a two-row margin. AND THE 8x16 CELL IS THE
 *    REAL PRIZE: EGA Trek runs at 640x350 in an 8x14 cell and every other
 *    8-bit port here draws it at 8x8. This one keeps the vertical detail.
 *    DOUBLE_X (40 columns) is deliberately left off -- the eighty is why this
 *    machine is worth porting to.
 *
 * 2. THE PALETTE IS PROGRAMMABLE, so EGA's own sixteen go into the LUTs and
 *    EGA_TO_VDC becomes the identity. Brown included -- the one colour the
 *    C128's fixed RGBI chip cannot have.
 *
 * 3. THE FONT IS RAM, so this port authors what it needs. The machine's own
 *    font has a correct ASCII half (measured: $41 is 3C 42 42 7E 42 42 42 00,
 *    a capital A) and dither patterns where a PC font would keep box drawing.
 *    So the ASCII half is MOVED into C64 screen-code order and the box set is
 *    drawn on top -- which leaves the shared layout.h's G_* constants working
 *    unchanged, and gets reverse video as a rule rather than 128 more glyphs.
 *
 * EVERY MATRIX WRITE COSTS sei/cli. The character and colour matrices share
 * the I/O window with Vicky's registers, and FoenixMCP's IRQ -- which fills
 * the keyboard queue -- expects page 0. An interrupt landing while the page
 * is 2 or 3 would have the kernel reading matrix RAM as its registers.
 */
#include "../../c128/src/vdc.h"
#include "f256vid.h"
#include "f256kern.h"

/* ---------------------------------------------------------------- colour */

/* EGA's own sixteen, two bits per gun: 0x00 0x55 0xAA 0xFF. Same table the
   MEGA65 port loads into the VIC-IV, and for the same reason. */
static const unsigned char ega_r[16] = {
    0x00,0x00,0x00,0x00,0xAA,0xAA,0xAA,0xAA,
    0x55,0x55,0x55,0x55,0xFF,0xFF,0xFF,0xFF };
static const unsigned char ega_g[16] = {
    0x00,0x00,0xAA,0xAA,0x00,0x00,0x55,0xAA,
    0x55,0x55,0xFF,0xFF,0x55,0x55,0xFF,0xFF };
static const unsigned char ega_b[16] = {
    0x00,0xAA,0x00,0xAA,0x00,0xAA,0x00,0xAA,
    0x55,0xFF,0x55,0xFF,0x55,0xFF,0x55,0xFF };

/* BOTH LUTS. A colour-matrix byte is (foreground << 4) | background, and the
   nibbles index two SEPARATE tables. Loading only the foreground one leaves
   every background reading out of whatever FoenixMCP left behind. */
static void load_ega_palette(void)
{
    unsigned char i;
    __asm__ volatile ("sei");
    F256_IO = F256_IO_REGS;
    for (i = 0; i < 16; i++) {
        unsigned int o = (unsigned int)i * 4;
        VKY_FG_LUT[o] = ega_b[i]; VKY_FG_LUT[o+1] = ega_g[i];
        VKY_FG_LUT[o+2] = ega_r[i]; VKY_FG_LUT[o+3] = 0;
        VKY_BG_LUT[o] = ega_b[i]; VKY_BG_LUT[o+1] = ega_g[i];
        VKY_BG_LUT[o+2] = ega_r[i]; VKY_BG_LUT[o+3] = 0;
    }
    __asm__ volatile ("cli");
}

/* The shared UI hands a single EGA index and means the FOREGROUND; the
   background is black throughout, as on the C128 and the MEGA65. */
#define CELL_COLOUR(c) ((unsigned char)(((unsigned char)(c) & 0x0F) << 4))

/* ------------------------------------------------------------------ font */

/* THE BOX-DRAWING SET, AT C64 SCREEN CODES. This port's own artwork, shared
   with atari/src/vbxevid.c and amiga/src/amigagfx.c, which drew it first
   because the shared layout.h names these glyphs by screen code and neither
   topaz nor the Atari ROM has anything there. Geometry, not copying: the
   vertical is two pixels wide at the centre so it meets a horizontal cleanly
   and joins between adjacent cells.
   NOTE the cell is 8x16 on this machine but the FONT is still 8x8 -- DOUBLE_Y
   doubles each row on the way out, so these stay eight bytes. */
struct box_glyph { unsigned char code; unsigned char row[8]; };
static const struct box_glyph box[] = {
  /* 32  space, stated rather than inherited, so that 160 -- the solid cell
         every panel fill and the badge body use -- is solid whatever the
         machine's own font had at 32 */
                          { 32, {0,0,0,0,0,0,0,0}},
  /* 64  G_HLINE  ---- */ { 64, {0,0,0,0xFF,0xFF,0,0,0}},
  /* 93  G_VLINE  |    */ { 93, {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18}},
  /* 112 G_TL     ,-   */ {112, {0,0,0,0x1F,0x1F,0x18,0x18,0x18}},
  /* 110 G_TR     -.   */ {110, {0,0,0,0xF8,0xF8,0x18,0x18,0x18}},
  /* 109 G_BL     `-   */ {109, {0x18,0x18,0x18,0x1F,0x1F,0,0,0}},
  /* 125 G_BR     -'   */ {125, {0x18,0x18,0x18,0xF8,0xF8,0,0,0}},
  /* 107 G_TEE_L  |-   */ {107, {0x18,0x18,0x18,0x1F,0x1F,0x18,0x18,0x18}},
  /* 115 G_TEE_R  -|   */ {115, {0x18,0x18,0x18,0xF8,0xF8,0x18,0x18,0x18}},
  /* 114 G_TEE_D  T    */ {114, {0,0,0,0xFF,0xFF,0x18,0x18,0x18}},
  /* 113 G_TEE_U  _|_  */ {113, {0x18,0x18,0x18,0xFF,0xFF,0,0,0}},
  /* 91  G_CROSS  +    */ { 91, {0x18,0x18,0x18,0xFF,0xFF,0x18,0x18,0x18}},
  /* 98  G_HALF_LO -- the badge disc's top; its reverse, 226, is the bottom
         and needs no entry of its own */
                          { 98, {0,0,0,0,0xFF,0xFF,0xFF,0xFF}},
  /* 100 exists ONLY so that its reverse is 228, G_BAR: seven rows filled on
         an eight-pixel pitch, which leaves the hairline between stacked gauge
         bars that ui.c measured off the original */
                          {100, {0,0,0,0,0,0,0,0xFF}},
  /* 30, 31  the up and left arrows: the two of screen codes 27..31 with no
         ASCII to borrow. A code with a glyph cannot come out as a marker. */
                          { 30, {0x18,0x3C,0x7E,0x18,0x18,0x18,0x18,0}},
                          { 31, {0,0x10,0x30,0x7E,0x30,0x10,0,0}},
  /* 81  G_SHIP -- the enemy silhouette's saucer. Drawn next to four cells of
         G_HLINE and a solid block, so it has to read as a hull: wider than
         tall, clear of the top and bottom rows so it does not merge. */
                          { 81, {0,0x3C,0x7E,0xFF,0xFF,0x7E,0x3C,0}}
};
#define BOX_COUNT ((unsigned char)(sizeof box / sizeof box[0]))

/* A GRAPHICS CODE NOBODY DREW must render as a hollow box, not as nothing.
   The Amiga missed two of fifteen on its first pass, and an invisible glyph
   leaves a panel looking merely empty -- the bug that does not get reported. */
static const unsigned char marker[8] = {0,0x7E,0x42,0x42,0x42,0x42,0x7E,0};

/* ONE GLYPH PER CRITICAL SECTION. The whole build touches 2K of font RAM and
   doing it inside a single sei would hold the kernel's IRQ off for tens of
   milliseconds. Per glyph it is a few dozen cycles at a time. */
static void glyph_copy(unsigned char dst, unsigned char src)
{
    unsigned int d = (unsigned int)dst * 8, s = (unsigned int)src * 8;
    unsigned char i, tmp[8];
    __asm__ volatile ("sei");
    F256_IO = F256_IO_FONT;
    for (i = 0; i < 8; i++) tmp[i] = F256_FONT[s + i];
    for (i = 0; i < 8; i++) F256_FONT[d + i] = tmp[i];
    F256_IO = F256_IO_REGS;
    __asm__ volatile ("cli");
}

static void glyph_invert(unsigned char dst, unsigned char src)
{
    unsigned int d = (unsigned int)dst * 8, s = (unsigned int)src * 8;
    unsigned char i, tmp[8];
    __asm__ volatile ("sei");
    F256_IO = F256_IO_FONT;
    for (i = 0; i < 8; i++) tmp[i] = (unsigned char)~F256_FONT[s + i];
    for (i = 0; i < 8; i++) F256_FONT[d + i] = tmp[i];
    F256_IO = F256_IO_REGS;
    __asm__ volatile ("cli");
}

static void glyph_set(unsigned char code, const unsigned char *rows)
{
    unsigned int d = (unsigned int)code * 8;
    unsigned char i;
    __asm__ volatile ("sei");
    F256_IO = F256_IO_FONT;
    for (i = 0; i < 8; i++) F256_FONT[d + i] = rows[i];
    F256_IO = F256_IO_REGS;
    __asm__ volatile ("cli");
}

/* Rebuild the machine's ASCII font as a C64 screen-code font.
 *
 * THE ORDER IS THE FUNCTION, and it is what makes this need no scratch
 * buffer. Screen codes 1..26 are copied FROM ASCII 65..90 -- font offsets
 * $208..$2D7 -- down to $08..$D7, which do not overlap. The box set then
 * lands at codes 64..127, $200..$3FF, ON TOP of the letters it was just
 * copied from: correct, but only because the copy happened first. The
 * reverse half is built last, from $000..$3FF up to $400..$7FF. Three
 * passes, none of them overlapping, no 2K of scratch on a machine that has
 * about forty. */
void f256_font_build(void)
{
    unsigned char c;

    glyph_copy(0, '@');
    for (c = 1; c <= 26; c++) glyph_copy(c, (unsigned char)('A' + c - 1));
    glyph_copy(27, '[');        /* the play-again box draws "[YES]" and */
    glyph_copy(29, ']');        /* "[NO]" -- the Amiga found these needed */
    /* 32..63 are ASCII already and stay where they are. */

    /* Everything from 64 up is graphics: draw the marker across the range
       first, then the real glyphs on top. A code that gets neither would
       otherwise keep a letter's bitmap and look deliberate. */
    for (c = 64; c < 128; c++) glyph_set(c, marker);
    for (c = 0; c < BOX_COUNT; c++) glyph_set(box[c].code, box[c].row);

    /* REVERSE VIDEO IS A RULE, NOT ENTRIES. 160 (G_BLOCK) falls out of space,
       226 (the badge disc's bottom) out of 98, 228 (G_BAR) out of 100.
       Writing those three by hand would be three chances to disagree. */
    for (c = 0; c < 128; c++) glyph_invert((unsigned char)(c + 128), c);
}

/* Same converter vic.c and ted.c carry, for the same reason: scr_puts takes
   text and everything else takes a RAW screen code, and the box glyphs live
   at 64..127 -- precisely the range this rewrites. Routing a border through
   here would silently turn it into a letter. */
static unsigned char ascii_to_screencode(char c)
{
    unsigned char u = (unsigned char)c;
    if (u >= 32 && u <= 63) return u;           /* space, digits, punctuation */
    if (u >= 64 && u <= 95) return (unsigned char)(u - 64);
    if (u >= 97 && u <= 122) return (unsigned char)(u - 96);
    if (u >= 193 && u <= 218) return (unsigned char)(u - 192);
    return 32;
}

/* ----------------------------------------------------------------- cells */

/* THE OFFSET IS THE DRIVER'S, NOT THE LAYOUT'S. panels[] is measured off the
   original and shared with eleven other ports; this machine being five rows
   taller than the console is this file's problem. */
#define CELL_OFF(x, y) \
    ((unsigned int)((unsigned char)(y) + F256_ROW_OFFSET) * F256_STRIDE + (x))

__attribute__((noinline))
void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color)
{
    unsigned int off = CELL_OFF(x, y);
    __asm__ volatile ("sei");
    F256_IO = F256_IO_CHAR;  F256_MATRIX[off] = ch;
    F256_IO = F256_IO_COLOR; F256_MATRIX[off] = CELL_COLOUR(color);
    F256_IO = F256_IO_REGS;
    __asm__ volatile ("cli");
}

/* CLIPS AT THE RIGHT EDGE, as vic.c's note says it must: screen memory is
   linear, so an unbounded write lands on the start of the next row. Clipping
   does not make an over-long string right; it confines the damage. */
__attribute__((noinline))
void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color)
{
    unsigned int off = CELL_OFF(x, y);
    unsigned char col = x;
    const char *p;
    unsigned int o;
    unsigned char n = 0;

    /* ONE PAGE SWITCH PER RUN, not per cell. The characters go down in one
       pass and the colours in a second, which is two switches for a whole
       string instead of three per character. */
    __asm__ volatile ("sei");
    F256_IO = F256_IO_CHAR;
    for (p = s, o = off; *p && col < VDC_COLS; p++, o++, col++, n++)
        F256_MATRIX[o] = ascii_to_screencode(*p);
    F256_IO = F256_IO_COLOR;
    for (o = off; n; o++, n--) F256_MATRIX[o] = CELL_COLOUR(color);
    F256_IO = F256_IO_REGS;
    __asm__ volatile ("cli");
}

__attribute__((noinline))
void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color)
{
    unsigned int off = CELL_OFF(x, y);
    unsigned char col = x;
    unsigned char i, n = 0;
    unsigned int o;

    for (i = 0, col = x; i < w && col < VDC_COLS; i++, col++) n++;
    __asm__ volatile ("sei");
    F256_IO = F256_IO_CHAR;
    for (i = 0, o = off; i < n; i++, o++) F256_MATRIX[o] = ch;
    F256_IO = F256_IO_COLOR;
    for (i = 0, o = off; i < n; i++, o++) F256_MATRIX[o] = CELL_COLOUR(color);
    F256_IO = F256_IO_REGS;
    __asm__ volatile ("cli");
}

__attribute__((noinline))
void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color)
{
    unsigned char i;
    for (i = 0; i < h; i++) scr_put(x, (unsigned char)(y + i), ch, color);
}

__attribute__((noinline))
void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color)
{
    unsigned char row;
    for (row = 0; row < h; row++) scr_hline(x, (unsigned char)(y + row), w, ch, color);
}

/* CLEARS ALL THIRTY ROWS, not the console's twenty-five. The margin rows are
   not ours to leave as we found them: FoenixMCP's boot text is up there, and
   a console that starts with the shell's last line still showing above it
   looks like a bug in the game. */
void scr_clear(void)
{
    unsigned int i;
    __asm__ volatile ("sei");
    F256_IO = F256_IO_CHAR;
    for (i = 0; i < (unsigned int)F256_STRIDE * F256_SCREEN_ROWS; i++)
        F256_MATRIX[i] = 32;
    F256_IO = F256_IO_COLOR;
    for (i = 0; i < (unsigned int)F256_STRIDE * F256_SCREEN_ROWS; i++)
        F256_MATRIX[i] = CELL_COLOUR(VDC_BLACK);
    F256_IO = F256_IO_REGS;
    __asm__ volatile ("cli");
}

/* ----------------------------------------------------------------- frame */

/* THE FRAME COUNTER IS A KERNEL CALL, NOT A RASTER READ, and that is a
   decision with a measurement behind it. MAME's f256k returns the horizontal
   dot position from $D01A/$D01B -- the scan-line registers -- so pacing off
   the raster gives sampling noise in this emulator however correct the code
   is. SetTimer with the QUERY bit returns the kernel's own frame count, which
   measured 60.00 Hz against MAME's clock (300 frames in 5.000 seconds).
   It queues nothing, so unlike an event it cannot eat a keystroke. */
static volatile unsigned char frame_lo;

static unsigned char frame_count(void)
{
    __asm__ volatile(
        "        lda #$80\n"     /* TIMER_FRAMES | TIMER_QUERY */
        "        sta $f3\n"      /* timer.units -- THE UNION STARTS AT $F3 */
        "        jsr $fff0\n"    /* SetTimer */
        "        sta frame_lo\n"
        ::: "a", "x", "y", "memory", "p");
    return frame_lo;
}

void wait_vsync(void)
{
    unsigned char t = frame_count();
    while (frame_count() == t) { }
}

/* ------------------------------------------------------------------ mode */

void vdc_init(void)
{
    __asm__ volatile ("sei");
    F256_IO = F256_IO_REGS;
    VKY_MCR_L = MCR_TEXT;                    /* text only, no graphics layers */
    VKY_MCR_H = MCR_H_DBL_Y;                 /* 8x16 cell: 80x30, 480 lines */
    VKY_BG_B = 0; VKY_BG_G = 0; VKY_BG_R = 0;
    __asm__ volatile ("cli");

    load_ega_palette();
    f256_font_build();
    scr_clear();
}

/* Put back what the game changed, so whatever runs next gets the machine the
   way FoenixMCP hands it over: 80x60 at 8x8, and the kernel's own font. THE
   FONT CANNOT BE PUT BACK -- it is RAM and the original is gone the moment
   f256_font_build runs -- which is why plat_exit resets rather than
   returning, and why this says so instead of pretending. */
void vdc_shutdown(void)
{
    __asm__ volatile ("sei");
    F256_IO = F256_IO_REGS;
    VKY_MCR_L = MCR_TEXT;
    VKY_MCR_H = 0;                           /* back to 80x60 */
    __asm__ volatile ("cli");
    scr_clear();
}

/* THE LAST THING THE PROGRAM DOES, and it is a per-machine question -- the
   C128 BRKed into its monitor for a whole release because main() just
   returned. This machine's answer is a system reset: the font is RAM and the
   game has overwritten the kernel's, so there is nothing to hand back to. The
   two unlock bytes are the hardware's interlock against doing it by accident.
   PROVISIONAL: RunNamed back to the shell would be tidier and is not yet
   measured. */
void plat_exit(void)
{
    vdc_shutdown();
    *(volatile unsigned char *)0xD6A2 = 0xDE;
    *(volatile unsigned char *)0xD6A3 = 0xAD;
    *(volatile unsigned char *)0xD6A0 = 0x00;
    for (;;) { }
}
