/* Video for the CoCo 3 + SuperSprite FM+ -- the Yamaha V9958 in GRAPHIC6.
 *
 * The seam is c128/src/vdc.h: an 80x25 grid of cells, each a glyph plus a
 * foreground colour on black. On the four 6502 ports that is a character
 * cell the hardware draws for you. HERE IT IS A BITMAP AND WE DRAW IT --
 * the same shape as the Amiga and the Falcon, not the C128's.
 *
 *   $FF78  data          $FF79  address / status
 *   $FF7A  palette       $FF7B  register indirect
 *
 * the MSX $98/$99/$9A/$9B layout moved into the CoCo's slot window. Confirmed
 * by writing VRAM and reading it back, not inferred from a table.
 *
 * GRAPHIC6: 512x212, sixteen colours per pixel, FOUR BITS A PIXEL so a line
 * is 256 bytes and a screen is 54,272. A 6-pixel font puts 85 columns in that
 * 512 and 8-pixel rows give 26; the console takes 80x25 and the remainder
 * becomes a margin, 16 pixels each side and 6 lines top and bottom.
 *
 * SIX IS AN EVEN NUMBER AND THAT IS WHY THE FONT IS 6 WIDE. At four bits a
 * pixel a byte is exactly two pixels, so a 6-pixel cell is THREE WHOLE BYTES
 * and no cell ever has to read-modify-write a byte it shares with its
 * neighbour. A 5- or 7-pixel font would have made every other column a
 * different and slower routine.
 *
 * WRITTEN IN PLAIN C, AND THAT WAS MEASURED RATHER THAN ASSUMED. The note
 * this file used to carry said cmoc has no `volatile`, so repeated stores to
 * one port address would be dead-store-eliminated and the driver would have
 * to be assembly. THAT IS NOT TRUE OF cmoc 0.1.86 at this port's own flags:
 * three consecutive stores to $FF78, two consecutive loads of $FF79, and the
 * whole set-address-then-stream loop below all survive -O2
 * -fomit-frame-pointer intact, checked by reading the generated .s. The claim
 * came from the banking work, where the real fault was the U frame pointer
 * sitting in the paged window -- a wrong diagnosis that would have cost this
 * file in hand-written 6809.
 */
#include <stdint.h>

#include "../../c128/src/vdc.h"
#include "font6x8.h"

#define VDP_DATA   ((unsigned char *)0xFF78)
#define VDP_ADDR   ((unsigned char *)0xFF79)
#define VDP_PAL    ((unsigned char *)0xFF7A)
/* $FF7B is register-INDIRECT access (via R#17). This driver does not use
   it; it is named here so nobody reaches for it thinking it writes a
   register by number. See vdp_reg() below. */
#define VDP_INDIRECT ((unsigned char *)0xFF7B)

#define SCR_STRIDE  256             /* bytes per scanline, 512 pixels at 4bpp */
#define SCR_LINES   212
#define MARGIN_X    8               /* bytes -- 16 pixels, centring 80*6=480 */
#define MARGIN_Y    6               /* lines -- centring 25*8=200 in 212 */

/* THE MESSAGE LOG LIVES IN THE CARD'S VRAM, which is what this port has that
   the 6809's 64K does not. ui.c keeps 32 slots of 64 bytes of scrollback
   outside its own variables, because on the C128 it sits in spare VDC video
   RAM that the 8502 cannot address -- the same reason applies here, and it
   buys back 2,048 bytes of a 64K address space with 2,468 free. Parked well
   clear of the 54,272 bytes the display uses. */
#define LOG_ORIGIN     0x1000       /* what ui.c calls it */
#define LOG_BYTES      (32 * 64)
#define LOG_VRAM       0xE000U      /* what it is here */

/* EGA's sixteen, in EGA's order, so -DTREK_COLOUR_IS_EGA makes a colour name
   its own index. The V9958 wants three bits a channel: EGA's 0/85/170/255
   scale to 0/2/5/7. Two bytes a colour -- 0RRR0BBB then 00000GGG. */
static const unsigned char ega_pal[16][2] = {
    {0x00,0x00}, {0x05,0x00}, {0x00,0x05}, {0x05,0x05},
    {0x50,0x00}, {0x55,0x00}, {0x50,0x02}, {0x55,0x05},
    {0x22,0x02}, {0x27,0x02}, {0x22,0x07}, {0x27,0x07},
    {0x72,0x02}, {0x77,0x02}, {0x72,0x07}, {0x77,0x07}
};

/* A REGISTER WRITE GOES TO THE ADDRESS PORT, NOT TO $FF7B. Value first, then
   $80|n, both to $FF79 -- the same port that takes a VRAM address, and the
   high bit is what distinguishes the two.
   $FF7B IS "REGISTER INDIRECT": it writes to whichever register R#17 points
   at and auto-increments, which is a different feature entirely. Sending
   register writes there set a *sequence* of registers starting from R#17's
   default of 0, which by luck produced something screen-shaped -- so the
   display looked plausible while R#14 (VRAM A16-A14) took garbage, and reads
   landed in the wrong 16K bank. That is exactly the failure the readback
   found: identical reads giving different answers depending on which bank
   R#14 happened to be left holding. The stub this file replaced named
   $FF7B "register indirect" correctly and I read it as "register write". */
static void vdp_reg(unsigned char r, unsigned char v)
{
    *VDP_ADDR = v;
    *VDP_ADDR = (unsigned char)(0x80 | r);
}

/* SET THE WRITE ADDRESS. R#14 carries A16-A14 and the VDP's own counter
   carries into it as the address crosses 16K, so a run longer than a bank
   does not need touching again -- measured during the scope, and the reason
   a 54,272-byte clear is one loop rather than four. */
static void vdp_write_at(unsigned int addr)
{
    vdp_reg(14, (unsigned char)((addr >> 14) & 0x07));
    *VDP_ADDR = (unsigned char)(addr & 0xFF);
    *VDP_ADDR = (unsigned char)(((addr >> 8) & 0x3F) | 0x40);
}

static void vdp_read_at(unsigned int addr)
{
    vdp_reg(14, (unsigned char)((addr >> 14) & 0x07));
    *VDP_ADDR = (unsigned char)(addr & 0xFF);
    *VDP_ADDR = (unsigned char)((addr >> 8) & 0x3F);
}

/* The machine is taken at program_start, not here -- see place_stack() in
   coco3/tools/build_ovl.py. Doing it in this function was too late by a whole
   interrupt: the CoCo's 60Hz IRQ vectors through Disk BASIC, whose handler
   resets S to BASIC's stack inside this port's code, and that happened 20ms
   in while main() was still on its way here. */
void vdc_init(void)
{
    unsigned int i;


    /* GRAPHIC6. R#0 bit1=M3 and bit3=M5 give $0A; R#1 bit6 enables the
       display; R#9 bit7 selects 212 lines instead of 192. */
    vdp_reg(0, 0x0A);
    vdp_reg(1, 0x40);
    vdp_reg(2, 0x1F);               /* pattern base -- page 0 */
    vdp_reg(7, 0x00);               /* border black */
    /* R#8: bit3 sets 128K VRAM. BIT 1 DISABLES SPRITES, and that is a
       deliberate departure from the $08 the scope measured: the sprite
       attribute table is uninitialised VRAM, so leaving sprites on paints
       whatever happens to be in memory over the console. */
    vdp_reg(8, 0x0A);
    vdp_reg(9, 0x80);

    /* The palette. R#16 is the pointer and it auto-increments. */
    vdp_reg(16, 0x00);
    for (i = 0; i < 16; i++) {
        *VDP_PAL = ega_pal[i][0];
        *VDP_PAL = ega_pal[i][1];
    }

    scr_clear();
}

/* DELIBERATELY DOES NOT CLEAR, and does not blank the display. The farewell
   has to survive it -- the C128's rule, and the bug the X16 and the Amiga
   both shipped in v0.13.0, where the goodbye was erased one line after it was
   drawn. */
void vdc_shutdown(void) { }

/* Nothing to hand back: this port owns the machine from $1200 up, the way
   the Atari one does since it dropped DOS. */
void plat_exit(void) { }

void wait_vsync(void)
{
    /* S#0 bit 7 is the vblank flag and READING IT CLEARS IT, so this waits
       for the next one rather than testing a level. */
    vdp_reg(15, 0x00);
    while ((*VDP_ADDR & 0x80) == 0)
        ;
}

void scr_clear(void)
{
    unsigned char line;
    unsigned int  i;

    /* TWO 8/16-BIT LOOPS, NOT ONE 32-BIT ONE. The first version counted
       54,272 in an `unsigned long` and the clear took over TWELVE SECONDS of
       emulated time -- the program was still inside it when the test gave up,
       which read exactly like a hang. cmoc has no 32-bit registers to do it
       in; every compare and increment was a library-sized sequence. The
       address counter carries by itself, so the whole screen is still ONE
       run of writes with a single address set. */
    vdp_write_at(0);
    for (line = 0; line < SCR_LINES; line++)
        for (i = 0; i < SCR_STRIDE; i++)
            *VDP_DATA = 0x00;
}

/* Eight rows of six significant bits, bit 5 leftmost -- whatever the code
   turns out to mean. Screen codes 0..63 are the text set, 64..127 the box
   set, and bit 7 is reverse video, which is a RULE rather than data. */
static void glyph_rows(unsigned char ch, unsigned char *out)
{
    unsigned char base = (unsigned char)(ch & 0x7F);
    unsigned char rev  = (unsigned char)(ch & 0x80);
    int i, j;

    if (base < FONT_CODES) {
        for (j = 0; j < FONT_CELL_H; j++)
            out[j] = font6x8[base][j];
    } else {
        for (i = 0; i < FONT_BOX_COUNT; i++)
            if (font_box[i].code == base) {
                for (j = 0; j < FONT_CELL_H; j++)
                    out[j] = font_box[i].row[j];
                goto shade;
            }
        /* THE MISSING-GLYPH MARKER, and it is loud on purpose. A code with no
           entry is a bug in THIS file, not in the caller, and a blank cell
           hides it -- on the Amiga this marker is what caught two missing
           bracket glyphs. A hollow box is visible and is not a letter. */
        for (j = 0; j < FONT_CELL_H; j++)
            out[j] = (unsigned char)((j == 0 || j == FONT_CELL_H - 1) ? 0x3F : 0x21);
    }

shade:
    if (rev)
        for (j = 0; j < FONT_CELL_H; j++)
            out[j] = (unsigned char)(~out[j] & 0x3F);
}

void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color)
{
    unsigned char rows[FONT_CELL_H];
    unsigned int  addr;
    unsigned char r, bits, fg;

    if (x >= VDC_COLS || y >= VDC_ROWS) return;

    glyph_rows(ch, rows);
    fg   = (unsigned char)(color & 0x0F);
    /* 16-BIT THROUGHOUT. The whole of VRAM this driver touches -- 54,272
       bytes of display and 2K of message log at $E000 -- fits under 65,536,
       so R#14's A16-A14 are just bits 14-15 of an ordinary unsigned int.
       There is no 32-bit arithmetic anywhere in this file and there must not
       be: on a 6809 it is what turned a screen clear into a hang. */
    addr = (unsigned int)(MARGIN_Y + (unsigned int)y * FONT_CELL_H) * SCR_STRIDE
         + MARGIN_X + (unsigned int)x * 3;

    for (r = 0; r < FONT_CELL_H; r++) {
        bits = rows[r];
        /* Three whole bytes, two pixels each, high nibble left. */
        vdp_write_at(addr);
        *VDP_DATA = (unsigned char)(((bits & 0x20) ? fg << 4 : 0) | ((bits & 0x10) ? fg : 0));
        *VDP_DATA = (unsigned char)(((bits & 0x08) ? fg << 4 : 0) | ((bits & 0x04) ? fg : 0));
        *VDP_DATA = (unsigned char)(((bits & 0x02) ? fg << 4 : 0) | ((bits & 0x01) ? fg : 0));
        addr += SCR_STRIDE;
    }
}

/* ASCII IN, SCREEN CODES OUT -- the same conversion every port does, and the
   same 64..95 and 97..122 branches. scr_put takes a SCREEN code, so a caller
   holding a C string has to come through here. Getting this wrong is how the
   Falcon drew the Q in "WILL YOU REQUIRE A BRIEFING" as the ship's saucer. */
void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color)
{
    while (*s && x < VDC_COLS) {
        unsigned char u = (unsigned char)*s++;
        unsigned char c;

        if (u >= 32 && u <= 63)       c = u;
        else if (u >= 64 && u <= 95)  c = (unsigned char)(u - 64);
        else if (u >= 97 && u <= 122) c = (unsigned char)(u - 96);
        else                          c = 32;

        scr_put(x, y, c, color);
        x++;
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
    for (i = 0; i < w; i++) scr_put((unsigned char)(x + i), y, ch, color);
}

void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color)
{
    unsigned char i;
    for (i = 0; i < h; i++) scr_put(x, (unsigned char)(y + i), ch, color);
}

/* THE MESSAGE LOG, now in VRAM where the header always said it belonged.
   ui.c sets an address and then does a RUN of writes or a RUN of reads, so
   the direction latch is set once per run and each access is a single port
   write; `mode` is what makes that safe when the two interleave. */
#define MODE_NONE 0
#define MODE_READ 1
#define MODE_WRITE 2

static unsigned int  log_off;
static unsigned char log_mode;

void vdc_set_address(unsigned int addr)
{
    log_off  = (addr >= LOG_ORIGIN) ? (unsigned int)(addr - LOG_ORIGIN) : 0;
    log_mode = MODE_NONE;
}

void vdc_data_write(unsigned char value)
{
    if (log_off >= LOG_BYTES) return;
    if (log_mode != MODE_WRITE) {
        vdp_write_at(LOG_VRAM + log_off);
        log_mode = MODE_WRITE;
    }
    *VDP_DATA = value;
    log_off++;
}

unsigned char vdc_data_read(void)
{
    unsigned char v;
    if (log_off >= LOG_BYTES) { log_off++; return 0; }
    if (log_mode != MODE_READ) {
        vdp_read_at(LOG_VRAM + log_off);
        log_mode = MODE_READ;
    }
    v = *VDP_DATA;
    log_off++;
    return v;
}

/* C128-only: that machine's CRTC registers. Nothing here has them. */
unsigned char vdc_reg_read(unsigned char reg) { (void)reg; return 0; }
void vdc_reg_write(unsigned char reg, unsigned char value) { (void)reg; (void)value; }
