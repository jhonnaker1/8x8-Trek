/* Video for the MSX2 -- the Yamaha V9938 in SCREEN 7 (GRAPHIC6).
 *
 * The seam is c128/src/vdc.h: an 80x25 grid of cells, each a glyph plus a
 * foreground colour on black. Like the CoCo 3 card port this is a BITMAP and
 * we draw it -- and it is the same mode on the same family of chip: the
 * SuperSprite FM+ carries the V9958, the V9938's successor, and
 * coco3/src/coco3vid.c already drives GRAPHIC6 at 80x25. THE LAYOUT BELOW IS
 * THAT FILE'S: 512x212 at four bits a pixel, a 6-pixel font so a cell is
 * THREE WHOLE BYTES, a 16-pixel/6-line margin, and the message log in VRAM.
 *
 * WHAT IS NOT THAT FILE'S IS EVERYTHING ABOUT SHARING THE CHIP, and every one
 * of these would have been a bug copied across. On the CoCo nothing else
 * touched the card's VDP. On an MSX THE BIOS INTERRUPT HANDLER READS THE VDP
 * EVERY FRAME, and commodore-uno's msx2 port paid for each of these once:
 *
 *   1. A register write or address set is TWO bytes to one port, and a
 *      VBLANK landing between them leaves the latch half-set for both us and
 *      the handler. Every such pair runs with interrupts off.
 *   2. coco3vid.c's wait_vsync() polls S#0 -- and READING S#0 IS HOW THE BIOS
 *      ACKNOWLEDGES THE INTERRUPT. Doing it here steals the acknowledgement
 *      and the machine loses its keyboard. We wait on JIFFY, which the
 *      handler bumps, instead.
 *   3. R#15 selects which status register port $99 returns, and the handler
 *      assumes S#0. Anything that selects S#2 puts it straight back, with
 *      interrupts off throughout.
 *   4. NEVER SDCC's __critical. It saves state with `ld a,i`, which has a
 *      documented Z80 erratum: an interrupt accepted during that instruction
 *      makes P/V read 0, the restore decides interrupts were already off, and
 *      they stay off for good. uno's first build survived seventeen seconds.
 *      Interrupts are on for the whole life of this program, so plain di/ei
 *      is both correct and all that is needed.
 *
 * Ports, not addresses:  $98 data   $99 address/register/status
 *                        $9A palette  $9B register-indirect
 */
#include <stdint.h>

#include "vdc.h"
#include "msx2.h"
/* BY PATH, not -I../coco3/src: that directory also holds the CoCo's own
   string.h, which shadows the system one and drags in cmoc.h. */
#include "../../coco3/src/font6x8.h"

__sfr __at 0x98 VDP_DATA;
__sfr __at 0x99 VDP_ADDR;
__sfr __at 0x9A VDP_PAL;
__sfr __at 0x9B VDP_REGI;

#define IRQ_OFF() __asm di __endasm
#define IRQ_ON()  __asm ei __endasm

/* The BIOS handler increments this every VBLANK. */
#define RG8SAV (*(volatile unsigned char *)0xFFE7)
#define JIFFY (*(volatile unsigned int *)0xFC9E)

extern void bios_chgmod(unsigned char mode);

#define SCR_STRIDE  256             /* bytes per scanline, 512 pixels at 4bpp */
#define SCR_LINES   212
#define MARGIN_X    8               /* bytes -- 16 pixels, centring 80*6=480 */
#define MARGIN_Y    6               /* lines -- centring 25*8=200 in 212 */

/* THE MESSAGE LOG IN VRAM, as on the C128 (VDC RAM) and the CoCo 3 card. ui.c
   keeps 32 slots of 64 bytes of scrollback behind vdc_set_address/_data_*,
   and here that costs no TPA at all. $E000 is above the 54,272 bytes the
   display uses and below SCREEN 7's sprite tables at $F000/$FA00. */
#define LOG_ORIGIN     0x1000       /* what ui.c calls it */
#define LOG_BYTES      (32 * 64)
#define LOG_VRAM       0xE000U      /* what it is here */

/* EGA's sixteen in EGA's order -- the V9938 wants 0RRR0BBB then 00000GGG,
   the same format as the V9958, so this is coco3vid.c's table unchanged. */
static const unsigned char ega_pal[16][2] = {
    {0x00,0x00}, {0x05,0x00}, {0x00,0x05}, {0x05,0x05},
    {0x50,0x00}, {0x55,0x00}, {0x50,0x02}, {0x55,0x05},
    {0x22,0x02}, {0x27,0x02}, {0x22,0x07}, {0x27,0x07},
    {0x72,0x02}, {0x77,0x02}, {0x72,0x07}, {0x77,0x07}
};

/* The log's cached position (see the bottom of this file) -- declared here
   because every draw below MOVES THE VDP'S ADDRESS COUNTER and must say so.
   coco3vid.c did not, and got away with it only because ui.c never reads the
   log mid-draw; one store per draw is cheaper than depending on that. */
#define MODE_NONE  0
#define MODE_READ  1
#define MODE_WRITE 2
static unsigned int  log_off;
static unsigned char log_mode;

/* Two pixels at a time out of a four-entry table, rebuilt only when the
   colour changes. */

static void vdp_reg(unsigned char r, unsigned char v)
{
    IRQ_OFF();
    VDP_ADDR = v;
    VDP_ADDR = (unsigned char)(0x80 | r);
    IRQ_ON();
}

static void vdp_at(unsigned int addr, unsigned char write)
{
    IRQ_OFF();
    VDP_ADDR = (unsigned char)((addr >> 14) & 0x07);
    VDP_ADDR = 0x80 | 14;
    VDP_ADDR = (unsigned char)(addr & 0xFF);
    VDP_ADDR = (unsigned char)(((addr >> 8) & 0x3F) | write);
    IRQ_ON();
}

/* The command engine and the CPU share the VRAM port: a blit returns when it
   STARTS, and a CPU write made while it is still running is overwritten
   behind it. Every CPU access to VRAM waits here first. S#2 bit 0 is CE. */
static void vdp_idle(void)
{
    unsigned char s;
    for (;;) {
        IRQ_OFF();
        VDP_ADDR = 2;
        VDP_ADDR = 0x80 | 15;
        s = VDP_ADDR;
        VDP_ADDR = 0;
        VDP_ADDR = 0x80 | 15;       /* S#0 back, for the BIOS */
        IRQ_ON();
        if (!(s & 0x01)) return;
    }
}

void vdc_init(void)
{
    unsigned char i;

    /* THROUGH THE BIOS, as uno does: CHGMOD sets the table bases and mode bits
       right on every MSX2 variant and hides the sprites. Under MSX-DOS page 0
       is RAM, so bios_chgmod() goes through CALSLT -- see msxbios.s. */
    bios_chgmod(7);

    /* SPRITES OFF. The game has none, and a V9938 with sprites enabled gives
       the CPU 31 VRAM access slots a line and the command engine about half
       its speed; with them disabled, 88 (Grauw's measurements). R#8 bit 1,
       SPD -- and the BIOS's own copy in RG8SAV too, which is what anything
       that rewrites R#8 from the BIOS would put back. */
    RG8SAV |= 0x02;
    vdp_reg(8, RG8SAV);

    vdp_reg(7, 0x00);               /* border black */
    vdp_reg(16, 0x00);              /* palette pointer, auto-increments */
    IRQ_OFF();
    for (i = 0; i < 16; i++) {
        VDP_PAL = ega_pal[i][0];
        VDP_PAL = ega_pal[i][1];
    }
    IRQ_ON();

    scr_clear();
}

/* DELIBERATELY DOES NOT CLEAR -- the farewell has to survive it. */
void vdc_shutdown(void) { }

void wait_vsync(void)
{
    unsigned int t = JIFFY;
    while (JIFFY == t) { }
}

/* ONE BLIT, NOT 54,272 CPU WRITES. HMMV fills a rectangle with R#44 (two
   pixels at a time); 15 register writes instead of a quarter-million cycles.
   NX = 0 MEANS THE WHOLE WIDTH -- the trap uno recorded, used on purpose here,
   because 512 does not fit NX's nine bits and "0" is how the V9938 spells it.
   Callers that write VRAM afterwards go through vdp_idle(). */
/* ONE HMMV: a rectangle of VRAM filled with one byte by the command engine.
   DX and NX are PIXELS, two to a byte, so both must be even here -- and NX=0
   means the whole 512, used deliberately by scr_clear. */
static void hmmv(unsigned int dx, unsigned char dy, unsigned int nx,
                 unsigned char ny, unsigned char fill)
{
    vdp_idle();
    vdp_reg(17, 36);                /* R#17: indirect pointer -> R#36 (DX) */
    IRQ_OFF();
    VDP_REGI = (unsigned char)dx;   VDP_REGI = (unsigned char)(dx >> 8);
    VDP_REGI = dy;                  VDP_REGI = 0;
    VDP_REGI = (unsigned char)nx;   VDP_REGI = (unsigned char)(nx >> 8);
    VDP_REGI = ny;                  VDP_REGI = 0;
    VDP_REGI = fill;                /* R#44: the byte, two pixels */
    VDP_REGI = 0;                   /* R#45 argument */
    VDP_REGI = 0xC0;                /* R#46 HMMV -- starts it */
    IRQ_ON();
    log_mode = MODE_NONE;
}

void scr_clear(void)
{
    hmmv(0, 0, 0, SCR_LINES, 0);    /* NX = 0: the whole 512 */
}

/* THE BOX GLYPHS WERE A QUARTER OF A REPAINT. After the fills became a blit,
   `make profile` put glyph_ptr at 25-35% of the console draw: fourteen box
   glyphs searched linearly and then COPIED, for every border cell -- and a
   border is the same glyph forty times running. So the last one found is
   remembered, and a plain glyph is returned where it lies; only reverse
   video still needs the scratch copy, to invert it. */
static unsigned char box_code = 0xFF;
static const unsigned char *box_row;

static const unsigned char *glyph_ptr(unsigned char ch, unsigned char *scratch)
{
    unsigned char base = (unsigned char)(ch & 0x7F);
    const unsigned char *g;
    unsigned char j, i;

    if (base < FONT_CODES) {
        g = font6x8[base];
    } else if (base == box_code) {
        g = box_row;
    } else {
        for (i = 0; i < (unsigned char)FONT_BOX_COUNT; i++)
            if (font_box[i].code == base) break;
        if (i < (unsigned char)FONT_BOX_COUNT) {
            box_code = base;
            g = box_row = font_box[i].row;
        } else {
            /* The missing-glyph marker, loud on purpose: a hollow box. */
            for (j = 0; j < FONT_CELL_H; j++)
                scratch[j] = (unsigned char)((j == 0 || j == FONT_CELL_H - 1) ? 0x3F : 0x21);
            g = scratch;
        }
    }
    if (!(ch & 0x80))
        return g;
    for (j = 0; j < FONT_CELL_H; j++)
        scratch[j] = (unsigned char)(~g[j] & 0x3F);
    return scratch;
}

/* ONE HMMC A CHARACTER, the shape MSX2ANSI uses (read, not copied -- it is
   GPL-3.0). The old path set the VRAM address for each of a cell's 8 rows --
   eight latch sequences under di -- and pushed 3 bytes after each, in C:
   ~2.1ms a character in `make profile`. HMMC is ONE command for the 6x8 cell
   (3 bytes by 8 lines); its first byte rides in R#44 as the command starts
   and the other 23 follow through the indirect port, and the VDP does every
   address. The ISR cannot disturb the stream: it touches port $99, and R#17
   and port $9B are ours.

   Each byte is two pixels from two glyph bits, tested IN PLACE with
   `bit n,(hl)`: bits 5/4, 3/2, 1/0 of a row, the higher one the left pixel
   (high nibble). D = fg << 4 and E = fg, so there is no table. ~60 cycles a
   byte, far above the V9938's worst slot gap with sprites off -- the first
   byte cannot be outrun.

   hc_* are this function's arguments, as globals, so the calling convention
   is not in question. */
static const unsigned char *hc_g;
static unsigned int  hc_dx;
static unsigned char hc_dy, hc_fg;

static void hmmc_cell(void) __naked
{
    __asm
        ld   hl, (_hc_g)
        ld   a, (_hc_fg)
        ld   e, a
        add  a, a
        add  a, a
        add  a, a
        add  a, a
        ld   d, a               ; D = fg << 4, E = fg
        ld   c, #0x9B
        di
        ld   a, #36             ; R#17 = 36, auto-increment
        out  (0x99), a
        ld   a, #0x80+17
        out  (0x99), a
        ld   a, (_hc_dx)
        out  (c), a             ; R#36 DX
        ld   a, (_hc_dx+1)
        out  (c), a             ; R#37
        ld   a, (_hc_dy)
        out  (c), a             ; R#38 DY
        xor  a
        out  (c), a             ; R#39
        ld   a, #6
        out  (c), a             ; R#40 NX = 6 pixels
        xor  a
        out  (c), a             ; R#41
        ld   a, #8
        out  (c), a             ; R#42 NY = 8 lines
        xor  a
        out  (c), a             ; R#43
        bit  5, (hl)            ; the first byte: row 0, bits 5/4
        jr   z, 1$
        or   d
    1$: bit  4, (hl)
        jr   z, 2$
        or   e
    2$: out  (c), a             ; R#44 CLR, the first two pixels
        xor  a
        out  (c), a             ; R#45 ARG
        ld   a, #0xF0
        out  (c), a             ; R#46 HMMC -- starts
        ld   a, #0x80+44        ; R#17 = 44, NO auto-increment
        out  (0x99), a
        ld   a, #0x80+17
        out  (0x99), a
        ei
        ld   b, #8
        jr   5$                 ; row 0 first byte is already gone
    3$: xor  a                  ; bits 5/4
        bit  5, (hl)
        jr   z, 4$
        or   d
    4$: bit  4, (hl)
        jr   z, 11$
        or   e
    11$: out (c), a
    5$: xor  a                  ; bits 3/2
        bit  3, (hl)
        jr   z, 6$
        or   d
    6$: bit  2, (hl)
        jr   z, 7$
        or   e
    7$: out  (c), a
        xor  a                  ; bits 1/0
        bit  1, (hl)
        jr   z, 8$
        or   d
    8$: bit  0, (hl)
        jr   z, 9$
        or   e
    9$: out  (c), a
        inc  hl
        djnz 3$
        ret
    __endasm;
}

void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color)
{
    unsigned char rows[FONT_CELL_H];

    if (x >= VDC_COLS || y >= VDC_ROWS) return;

    hc_g  = glyph_ptr(ch, rows);
    hc_fg = (unsigned char)(color & 0x0F);
    hc_dx = (unsigned int)(MARGIN_X + x + x + x) * 2;
    hc_dy = (unsigned char)(MARGIN_Y + (y << 3));
    vdp_idle();                     /* a new command waits for the last */
    hmmc_cell();
    log_mode = MODE_NONE;
}

/* INDEXED, NOT A WALKING POINTER. SDCC 4.6 can put a string cursor in IY and
   then reload IY for a global in the same expression -- uno's gfx_text walked
   off through memory that way. An index keeps the cursor out of IY. */
void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color)
{
    unsigned char i;
    for (i = 0; s[i] && x < VDC_COLS; i++, x++) {
        unsigned char u = (unsigned char)s[i];
        unsigned char c;
        if (u >= 32 && u <= 63)       c = u;
        else if (u >= 64 && u <= 95)  c = (unsigned char)(u - 64);
        else if (u >= 97 && u <= 122) c = (unsigned char)(u - 96);
        else                          c = 32;
        scr_put(x, y, c, color);
    }
}

/* TWO-THIRDS OF A CONSOLE REPAINT WAS THIS LOOP. `make profile` sampled the
   first console draw: 8.5 seconds, 3,970 scr_put calls, and 2,667 of them
   from nine calls here -- every one filling a panel with SPACES, each blank
   cell drawn as eight address setups and 24 bytes of zero through the CPU.
   A blank cell is one colour, so a rectangle of them is ONE HMMV: the
   background byte for a space, the foreground pair for a reverse space.
   Anything else still goes a cell at a time. */
#define SC_BLANK 32

void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color)
{
    unsigned char i, j;

    if ((ch & 0x7F) == SC_BLANK && w && h) {
        unsigned char fg = (unsigned char)(color & 0x0F);
        hmmv((unsigned int)(MARGIN_X + x + x + x) * 2,
             (unsigned char)(MARGIN_Y + (y << 3)),
             (unsigned int)w * 6, (unsigned char)(h << 3),
             (ch & 0x80) ? (unsigned char)((fg << 4) | fg) : 0);
        return;
    }
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            scr_put((unsigned char)(x + i), (unsigned char)(y + j), ch, color);
}

void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color)
{
    unsigned char i;
    for (i = 0; i < w; i++) scr_put((unsigned char)(x + i), y, ch, color);
}

void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color)
{
    unsigned char i;
    for (i = 0; i < h; i++) scr_put(x, (unsigned char)(y + i), ch, color);
}

/* THE MESSAGE LOG. ui.c sets an address, then does a RUN of writes or reads;
   the direction is latched once per run. */
void vdc_set_address(unsigned int addr)
{
    log_off  = (addr >= LOG_ORIGIN) ? (unsigned int)(addr - LOG_ORIGIN) : 0;
    log_mode = MODE_NONE;
}

void vdc_data_write(unsigned char value)
{
    if (log_off >= LOG_BYTES) return;
    if (log_mode != MODE_WRITE) {
        vdp_idle();
        vdp_at(LOG_VRAM + log_off, 0x40);
        log_mode = MODE_WRITE;
    }
    VDP_DATA = value;
    log_off++;
}

unsigned char vdc_data_read(void)
{
    unsigned char v;
    if (log_off >= LOG_BYTES) { log_off++; return 0; }
    if (log_mode != MODE_READ) {
        vdp_idle();
        vdp_at(LOG_VRAM + log_off, 0x00);
        log_mode = MODE_READ;
    }
    v = VDP_DATA;
    log_off++;
    return v;
}

void vram_far_at(unsigned int off, unsigned char write)
{
    vdp_idle();
    IRQ_OFF();
    VDP_ADDR = (unsigned char)(0x04 | (off >> 14));     /* A16 = 1: page 1 */
    VDP_ADDR = 0x80 | 14;
    VDP_ADDR = (unsigned char)(off & 0xFF);
    VDP_ADDR = (unsigned char)(((off >> 8) & 0x3F) | write);
    IRQ_ON();
    log_mode = MODE_NONE;
}

/* C128-only: that machine's CRTC registers. */
unsigned char vdc_reg_read(unsigned char reg) { (void)reg; return 0; }
void vdc_reg_write(unsigned char reg, unsigned char value) { (void)reg; (void)value; }
