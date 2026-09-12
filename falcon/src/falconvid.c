/* Video for the Atari Falcon030: the VIDEL in 640x480, sixteen colours.
 *
 * The seam is c128/src/vdc.h, exactly as on the MEGA65, the X16 and the
 * Amiga: an 80x25 grid of cells, each one a glyph plus a foreground colour on
 * black. What that header calls a "screen code" is a byte this file has to
 * turn into pixels, because this machine has no character generator -- same
 * position as the Amiga, and the shape of this driver follows it.
 *
 * THE MODE IS THE ONE THE MACHINE ALREADY BOOTS INTO. A Falcon on a VGA
 * monitor comes up in $001A = VGA|COL80|BPS4, which VgetSize reports as
 * 153,600 bytes: 640x480 at four planes. Measured, not assumed -- see
 * NOTES.md, "SCOPE: the ATARI FALCON", and tools/falcon/vidprobe.c. We still
 * set it explicitly, because a Falcon on an RGB monitor boots elsewhere.
 *
 * THE PLANES ARE WORD-INTERLEAVED, AND THIS IS THE ONE THING THAT DOES NOT
 * CARRY FROM THE AMIGA. There, four bitplanes are four separate regions and a
 * driver keeps four pointers. Here the four planes of the same sixteen pixels
 * sit in four consecutive words, so one cell's eight pixels are ONE BYTE in
 * each of four words eight bytes apart -- and which half of each word depends
 * on whether the cell's column is even or odd. That is the whole address
 * arithmetic, and it was confirmed by drawing before this file was written.
 *
 * THE FONT COMES OUT OF ROM AND IS NEVER SHIPPED. Line-A init hands back a
 * table of three system fonts; this port reads their headers and takes the
 * 8x16 one, which on EmuTOS 1.3.0 covers all 256 codes with form_width 256 --
 * so glyph row r of code c is simply dat_table[r * 256 + c]. Measured with
 * tools/falcon/fontprobe.c rather than assumed from the usual font trio.
 * Same principle as the Atari 8-bit port, which copies the OS ROM font at
 * runtime: the machine's glyphs stay the machine's.
 *
 * 8x16 IS WHY THE CELL IS NOT THE AMIGA'S 8x8. 640x480 with 8x8 cells is
 * 80x60, more than twice the console's rows. At 8x16 it is 80x30, the console
 * takes 25 of those, and the five spare rows become a margin split top and
 * bottom.
 */
#include <tos.h>
#include <string.h>

#include "../../c128/src/vdc.h"
#include "../../core/ega.h"

#define SCR_W    640
#define SCR_H    480
#define STRIDE   (SCR_W / 2)          /* 4 planes, 1 bit each = 320 bytes */
#define CELL_W   8
#define CELL_H   16
#define CON_H    (VDC_ROWS * CELL_H)  /* 25 rows of 16 = 400 lines */
#define MARGIN_Y ((SCR_H - CON_H) / 2)

/* Line-A init: d0/a0 = variable table, a1 = the three font headers, a2 = the
   routine table. Only a1 is wanted. d2 and a2 are saved because Line-A is
   documented to use them and vbcc must not assume otherwise. */
__regsused("d0/d1/a0/a1/a2") void *linea_fonts(void) =
  "\tmovem.l\td2/a2,-(sp)\n"
  "\tdc.w\t$a000\n"
  "\tmove.l\ta1,d0\n"
  "\tmovem.l\t(sp)+,d2/a2\n";

struct fnthdr {
    short id, point;
    char  name[32];
    unsigned short first_ade, last_ade;
    short top, ascent, half, descent, bottom;
    short max_char_width, max_cell_width;
    short left_offset, right_offset;
    short thicken, ul_size, lighten, skew;
    unsigned short flags;
    unsigned char  *hor_table;
    unsigned short *off_table;
    unsigned char  *dat_table;
    unsigned short form_width, form_height;
    struct fnthdr *next_font;
};

static unsigned char *scr;              /* Physbase(), once */
static const struct fnthdr *rom_font;   /* the 8x16 system font */
static short old_mode = -1;

/* EGA's palette in the Falcon's eight bits per gun. EGA's own four levels are
   0x00/0x55/0xAA/0xFF, which land exactly with no rounding at all. Index
   order is EGA's, so core/ega.h's names ARE the palette registers and
   EGA_TO_VDC stays the identity -- the same trick the MEGA65, the X16 and the
   Amiga use. */
static const long ega_rgb[16] = {
    0x00000000L, 0x000000AAL, 0x0000AA00L, 0x0000AAAAL,
    0x00AA0000L, 0x00AA00AAL, 0x00AA5500L, 0x00AAAAAAL,
    0x00555555L, 0x005555FFL, 0x0055FF55L, 0x0055FFFFL,
    0x00FF5555L, 0x00FF55FFL, 0x00FFFF55L, 0x00FFFFFFL
};

/* THIS PORT'S OWN GLYPHS, at 8x16 -- every code the shared UI can pass that
 * the ROM font does not have. The codes are C128 screen codes: the
 * box-drawing set lives at 64..127, which in the ROM font is '@'..'_', so
 * every one of these MUST be overridden or panel borders come out as letters.
 *
 * The set and its codes are the Amiga's, re-derived there by sweeping every
 * scr_put/hline/vline/fill_rect argument in ui.c, layout.c and main.c -- that
 * sweep found FIFTEEN where an eyeball count gives eleven, and the two extra
 * live in panels nothing had drawn yet. Taking the same list rather than
 * repeating the sweep is deliberate; taking the same BITMAPS is not possible,
 * because these cells are twice as tall.
 *
 * DRAWN FOR 8x16, NOT DOUBLED FROM 8x8. Doubling would put a four-pixel
 * horizontal rule in a cell whose vertical rule stayed two pixels wide, and
 * the corners would not meet. So the rule is restated for this cell: a line
 * sits on rows 7 and 8 and in columns 3 and 4, which is what makes a vertical
 * meet a horizontal in the middle and two adjacent cells join.
 */
#define HL 0xFF                       /* a full horizontal run  */
#define VL 0x18                       /* columns 3 and 4        */

struct box_glyph { unsigned char code; unsigned char row[CELL_H]; };

static const struct box_glyph box[] = {
  /* 32 space -- stated rather than left to the font, so that its reverse
        (160, the cursor and the badge's body) is solid whatever the ROM has */
  { 32, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
  /* 64  G_HLINE  ---- */
  { 64, {0,0,0,0,0,0,0,HL,HL,0,0,0,0,0,0,0}},
  /* 93  G_VLINE  |    */
  { 93, {VL,VL,VL,VL,VL,VL,VL,VL,VL,VL,VL,VL,VL,VL,VL,VL}},
  /* 112 G_TL     ,-   */
  {112, {0,0,0,0,0,0,0,0x1F,0x1F,VL,VL,VL,VL,VL,VL,VL}},
  /* 110 G_TR     -.   */
  {110, {0,0,0,0,0,0,0,0xF8,0xF8,VL,VL,VL,VL,VL,VL,VL}},
  /* 109 G_BL     `-   */
  {109, {VL,VL,VL,VL,VL,VL,VL,0x1F,0x1F,0,0,0,0,0,0,0}},
  /* 125 G_BR     -'   */
  {125, {VL,VL,VL,VL,VL,VL,VL,0xF8,0xF8,0,0,0,0,0,0,0}},
  /* 107 G_TEE_L  |-   */
  {107, {VL,VL,VL,VL,VL,VL,VL,0x1F,0x1F,VL,VL,VL,VL,VL,VL,VL}},
  /* 115 G_TEE_R  -|   */
  {115, {VL,VL,VL,VL,VL,VL,VL,0xF8,0xF8,VL,VL,VL,VL,VL,VL,VL}},
  /* 114 G_TEE_D  T    */
  {114, {0,0,0,0,0,0,0,HL,HL,VL,VL,VL,VL,VL,VL,VL}},
  /* 113 G_TEE_U  _|_  */
  {113, {VL,VL,VL,VL,VL,VL,VL,HL,HL,0,0,0,0,0,0,0}},
  /* 91  G_CROSS  +    */
  { 91, {VL,VL,VL,VL,VL,VL,VL,HL,HL,VL,VL,VL,VL,VL,VL,VL}},
  /* 98  BADGE_DISC_TOP -- the lower half filled, rounding the disc's top
        edge. Its reverse, 226, is the bottom half and needs no entry. */
  { 98, {0,0,0,0,0,0,0,0,HL,HL,HL,HL,HL,HL,HL,HL}},
  /* 100 the bottom row alone, and the ONLY reason it is here is that its
        REVERSE is 228, the systems-status bar: filled except for one row, so
        adjacent bars keep a hairline between them instead of merging. ui.c
        measured that off the original as "7px bars on an 8px pitch"; at this
        cell height it is 15 on 16, which preserves the hairline and not the
        ratio -- the hairline is the point. */
  {100, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,HL}},
  /* 30, 31 the up and left arrows, the only two of 27..31 with no ASCII to
        borrow. Not used by the game as it stands; here so that a code with a
        glyph cannot come out as a hollow box later. */
  { 30, {0,0,0x18,0x3C,0x7E,0xFF,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0,0,0}},
  { 31, {0,0,0,0x10,0x30,0x7E,0xFF,0xFF,0x7E,0x30,0x10,0,0,0,0,0}},
  /* 81 the ship's saucer -- drawn, not copied. The badge and the info panel
        both put this beside four cells of G_HLINE and a solid block, so what
        it has to be is a round body that reads as a hull. Wider than tall,
        clear of the top and bottom rows so it does not merge with the cell
        above or below. */
  { 81, {0,0,0,0,0x3C,0x7E,0xFF,0xFF,0xFF,0x7E,0x3C,0,0,0,0,0}},
};

#define NBOX ((int)(sizeof box / sizeof box[0]))

/* SCREEN CODE -> ASCII, for everything the ROM font can supply.
 *
 * scr_put takes a C128 SCREEN CODE, not a character -- that is the seam's
 * contract on every port, and getting it wrong here cost the first build a
 * visible bug: "WILL YOU REQUIRE A BRIEFING" drew the Q as the ship's saucer,
 * because ASCII 'Q' is 81 and SCREEN CODE 81 is the saucer. The raw code was
 * being handed to the font as though it were a character.
 *
 * The C64 unshifted set puts '@' at 0 and A..Z at 1..26, then 32..63 are
 * ASCII already. Anything else is a graphic and must come from box[]. */
static int code_to_ascii(unsigned char c)
{
    if (c == 0) return '@';
    if (c <= 26) return 'A' + c - 1;
    /* 27..31 are [ POUND ] UP-ARROW LEFT-ARROW, and two of them are real: the
       play-again box is drawn as "[YES]" and "[NO]", which scr_puts turns
       into screen codes 27 and 29. Without these that prompt reads as a
       hollow box either side of the word. */
    if (c == 27) return '[';
    if (c == 29) return ']';
    if (c >= 32 && c <= 63) return c;
    return -1;
}

/* Code -> 16 rows of bits. REVERSE VIDEO IS A RULE, NOT A TABLE: codes
   128..255 are their base glyph inverted, so 160 (cursor, badge body) falls
   out of 32, 226 out of 98 and 228 out of 100 without three more hand-written
   entries to disagree with the rule. */
static void glyph_rows(unsigned char ch, unsigned char *out)
{
    unsigned char base = ch & 0x7F;
    int rev = (ch & 0x80) != 0;
    int i, j, a;

    for (i = 0; i < NBOX; i++)
        if (box[i].code == base) {
            for (j = 0; j < CELL_H; j++)
                out[j] = rev ? (unsigned char)~box[i].row[j] : box[i].row[j];
            return;
        }

    a = code_to_ascii(base);
    if (a >= 0 && rom_font) {
        const unsigned char *d = rom_font->dat_table + a;
        unsigned short w = rom_font->form_width;
        for (j = 0; j < CELL_H; j++)
            out[j] = rev ? (unsigned char)~d[j * w] : d[j * w];
        return;
    }

    /* THE MISSING-GLYPH MARKER, and it is deliberately loud. A code with no
       box entry and no ASCII is a bug in this file, not in the caller, and a
       blank cell would hide it -- on the Amiga this marker is what caught two
       missing bracket glyphs. A hollow box is visible and is not a letter. */
    for (j = 0; j < CELL_H; j++)
        out[j] = (j == 0 || j == CELL_H - 1) ? 0xFF : 0x81;
    if (rev)
        for (j = 0; j < CELL_H; j++)
            out[j] = (unsigned char)~out[j];
}

/* One cell's byte, in one plane, on one scanline. Eight pixels are a single
   byte in each of four words that sit eight bytes apart; an even column is
   the high byte of each word and an odd column the low one. */
static unsigned char *cell_byte(unsigned char x, int py, int plane)
{
    return scr + (long)py * STRIDE + ((long)(x >> 1) * 8) + (plane * 2) + (x & 1);
}

void vdc_init(void)
{
    void **fonts = (void **)linea_fonts();
    const struct fnthdr *f;
    int i;

    /* Take the 8x16 font by MEASURING the headers, not by taking index 2.
       EmuTOS 1.3.0 puts 6x6, 8x8 and 8x16 in that order; a different ROM need
       not, and the cell height is the thing this driver actually depends on. */
    rom_font = 0;
    for (i = 0; i < 3; i++) {
        f = (const struct fnthdr *)fonts[i];
        if (f && f->form_height == CELL_H && f->max_cell_width == CELL_W) {
            rom_font = f;
            break;
        }
    }

    old_mode = VsetMode(-1);
    VsetMode(VGA | COL80 | BPS4);
    scr = (unsigned char *)Physbase();
    VsetRGB(0, 16, (RGB *)ega_rgb);
    scr_clear();
}

/* Deliberately does NOT clear: the farewell has to survive it. That is the
   C128's rule and the reason the teardown could safely move after the
   keypress in v0.13.1 -- the X16 and the Amiga were erasing the goodbye
   before anyone could read it. */
void vdc_shutdown(void)
{
    if (old_mode >= 0)
        VsetMode(old_mode);
}

/* Nothing to do: TOS gets its process back and the desktop redraws itself.
   No machine reset, unlike the three Commodore ports, whose exit path would
   otherwise drop into a monitor or leave BASIC pointing at our code. */
void plat_exit(void) { }

void wait_vsync(void) { Vsync(); }

void scr_clear(void)
{
    if (scr)
        memset(scr, 0, (long)STRIDE * SCR_H);
}

void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color)
{
    unsigned char rows[CELL_H];
    int py, r, p;

    if (!scr || x >= VDC_COLS || y >= VDC_ROWS)
        return;

    glyph_rows(ch, rows);
    py = MARGIN_Y + y * CELL_H;

    for (r = 0; r < CELL_H; r++) {
        unsigned char bits = rows[r];
        for (p = 0; p < 4; p++)
            *cell_byte(x, py + r, p) = (color >> p) & 1 ? bits : 0;
    }
}

/* ASCII IN, SCREEN CODES OUT -- the same conversion every port does, and the
   same 64..95 and 97..122 branches. scr_put takes a SCREEN code, so a caller
   holding a C string has to come through here. */
void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color)
{
    while (*s && x < VDC_COLS) {
        unsigned char u = (unsigned char)*s++;
        unsigned char c;

        if (u >= 32 && u <= 63)       c = u;
        else if (u >= 64 && u <= 95)  c = (unsigned char)(u - 64);
        else if (u >= 97 && u <= 122) c = (unsigned char)(u - 96);
        else                          c = 32;
        scr_put(x++, y, c, color);
    }
}

void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color)
{
    unsigned char i, j;

    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            scr_put((unsigned char)(x + i), (unsigned char)(y + j), ch, color);
}

void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color)
{
    unsigned char i;

    for (i = 0; i < w; i++)
        scr_put((unsigned char)(x + i), y, ch, color);
}

void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color)
{
    unsigned char i;

    for (i = 0; i < h; i++)
        scr_put(x, (unsigned char)(y + i), ch, color);
}

/* THE MESSAGE LOG'S BACKING STORE, and on this machine it is just an array.
 *
 * ui.c keeps the scrollback -- 32 entries of a date, a department and a line
 * of text -- OUTSIDE its own variables, reached through vdc_set_address /
 * vdc_data_write / vdc_data_read. That is not an abstraction for its own
 * sake: on the C128 it lives in spare VDC video RAM, which the 8502 cannot
 * address at all, because 2K of scrollback will not fit in a machine whose
 * writable data has a hundred bytes left. The X16 puts it in VERA's VRAM for
 * the same reason. Here there is no shortage, so it is a plain array.
 *
 * THESE THREE WERE STUBS UNTIL JAMIE PLAYED THE PORT, and the stub carried a
 * comment claiming "nothing outside the C128's own driver calls these" --
 * WHICH I INVENTED AND NEVER CHECKED. ui.c calls all three, every time it
 * files a message. So every message went into a black hole and read back as
 * zeros: the message panel drew empty boxes and MSGS showed three entries of
 * "STARDATE: 0.0". Two bugs, one false claim, and no instrument here saw it
 * because the screen was otherwise perfect. A NEGATIVE CLAIM ABOUT OUR OWN
 * PORT, WRITTEN BESIDE THE CODE THAT DISPROVES IT -- the oldest shape in
 * NOTES.md, and it shipped in the first build that drew anything.
 *
 * Only vdc_reg_read/vdc_reg_write are genuinely C128-only.
 *
 * SIZED FROM ui.c's OWN CONSTANTS: LOG_BASE is 0x1000 and 32 slots of 64
 * bytes follow it. Written out rather than rounded up, so a bigger log is a
 * bounds failure here rather than a quiet corruption of whatever follows. */
#define LOG_ORIGIN  0x1000
#define LOG_BYTES   (32 * 64)

static unsigned char logstore[LOG_BYTES];
static unsigned int  log_cursor;

void vdc_set_address(unsigned int addr)
{
    log_cursor = (addr >= LOG_ORIGIN) ? (addr - LOG_ORIGIN) : 0;
}

/* Write-and-advance: ui.c sets an address once and streams a whole record
   through it, so the cursor is a file static exactly as on the X16. */
void vdc_data_write(unsigned char value)
{
    if (log_cursor < LOG_BYTES) logstore[log_cursor] = value;
    log_cursor++;
}

unsigned char vdc_data_read(void)
{
    unsigned char v = (log_cursor < LOG_BYTES) ? logstore[log_cursor] : 0;
    log_cursor++;
    return v;
}

/* These two ARE C128-only: they drive that machine's CRTC registers, and
   every other port defines them away. */
unsigned char vdc_reg_read(unsigned char reg) { (void)reg; return 0; }
void vdc_reg_write(unsigned char reg, unsigned char value) { (void)reg; (void)value; }
