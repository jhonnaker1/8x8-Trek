/* VBXE text-mode driver for the Atari 800XL. The video seam, fifth port.
 *
 * WHAT IS BORROWED AND WHAT IS NOT. The register map, the XDL structure and
 * three traps below come from commodore-uno/atari/src/vbxevid.c (MIT, same
 * author), which found every one of them the hard way. What is new here is
 * the 4K window, the VRAM layout that keeps the whole hot path in one bank,
 * a 25th row, EGA's palette, and a font that is BUILT rather than copied.
 *
 * THE THREE TRAPS, kept verbatim in spirit because each cost a day:
 *
 *   1. The FX core exposes CSEL/PSEL/CR/CG/CB directly at $D644-$D648. The
 *      older VBXE manual documents those same addresses as an MSEL/MB0-3
 *      commit protocol; following the manual scrambles the palette so text
 *      renders in a colour indistinguishable from its background -- which
 *      looks like blank glyphs, not like a palette bug.
 *   2. MEMAC window A is $D65E/$D65F. The v1.0-beta manual's MA_CPU at $D64C
 *      DOES NOT EXIST on the FX core: Altirra's register-write switch has no
 *      case for it, so writes are silently dropped, the window never opens,
 *      and every "VRAM write" lands in plain Atari RAM. Same-window read-backs
 *      then look correct, which is what makes it expensive.
 *   3. CHBASE must be programmed by a PRIOR XDL entry, not by the one that
 *      turns text mode on. Set together, every glyph comes out blank with
 *      correct backgrounds -- which reads as a font problem and is a timing
 *      one.
 */
#include <stdint.h>

#include "vbxevid.h"
#include "../../core/ega.h"

#define VBXE_BASE 0xD640
#define VIDEO_CONTROL  (*(volatile unsigned char *)(VBXE_BASE + 0x00))
#define XDL_ADR0       (*(volatile unsigned char *)(VBXE_BASE + 0x01))
#define XDL_ADR1       (*(volatile unsigned char *)(VBXE_BASE + 0x02))
#define XDL_ADR2       (*(volatile unsigned char *)(VBXE_BASE + 0x03))
#define CSEL           (*(volatile unsigned char *)(VBXE_BASE + 0x04))
#define PSEL           (*(volatile unsigned char *)(VBXE_BASE + 0x05))
#define CR             (*(volatile unsigned char *)(VBXE_BASE + 0x06))
#define CG             (*(volatile unsigned char *)(VBXE_BASE + 0x07))
#define CB             (*(volatile unsigned char *)(VBXE_BASE + 0x08))
#define MEMAC_CONTROL  (*(volatile unsigned char *)(VBXE_BASE + 0x1E))
#define MEMAC_BANK_SEL (*(volatile unsigned char *)(VBXE_BASE + 0x1F))

#define ANTIC_VCOUNT (*(volatile unsigned char *)0xD40B)
#define OS_SDMCTL    (*(volatile unsigned char *)0x022F)  /* shadow of DMACTL */
#define OS_CHBAS     (*(volatile unsigned char *)0x02F4)  /* ROM font base page */

/* THE WINDOW IS 4K, NOT UNO'S 8K, AND THAT IS THE FIRST LEVER THIS PORT
   SPENT. MEMAC_CONTROL: high nibble = CPU window base page, bit3 = CPU
   access enable, bit2 = ANTIC access enable, bits0-1 = size (4K << n). So
   $28 is "$2000, CPU enabled, 4K" where uno's $29 was 8K -- which buys the
   program 4,096 bytes of address space and is what makes the budget on this
   target merely difficult. See atari.ld and tools/budget.py. */
#define MEMAC_OPEN 0x28

/* VRAM LAYOUT, and it is chosen so THE HOT PATH NEVER SWITCHES BANKS.
 *
 *   bank 0  $00000  screen, 80*25*2 = 4,000 bytes, ends $00F9F
 *           $00FA0  the XDL, 19 bytes, in the 96 the screen leaves over
 *   bank 1  $01000  font, 2KB (256 glyphs; VBXE wants a 2KB-aligned base,
 *                   so CHBASE = $1000 >> 11 = 2)
 *   bank 2+ $02000  the message log's backing store -- see vdc_set_address
 *   bank 4+ $04000  free for the overlay images and far memory
 *
 * Uno put its screen at $01000 and its font at $00800 and paid a bank
 * select on every write. Putting the screen at zero instead means every
 * scr_* call is a plain store into the window with no register write at
 * all, which on the tightest target in the project is both the fast answer
 * and the small one. */
#define VRAM_SCREEN_BANK 0
#define VRAM_XDL_OFF     0x0FA0
#define VRAM_XDL_ADDR    0x00FA0L
#define VRAM_FONT_BANK   1
#define VRAM_FONT_CHBASE 2
#define VRAM_LOG_BANK    2

static unsigned char cur_bank = 0xFF;   /* forces the first select */

void vbxe_bank(unsigned char bank) {
    if (bank != cur_bank) {
        MEMAC_BANK_SEL = (unsigned char)(0x80 | bank);
        cur_bank = bank;
    }
}

/* ---- the palette ---------------------------------------------------- */

/* EGA's own sixteen, so TREK_COLOUR_IS_EGA can make the mapping the
   identity exactly as it does on the MEGA65. Levels are 0x00, 0x55, 0xAA,
   0xFF -- every one a duplicated nibble, which is what makes them safe
   against a core that takes the high bits of the register rather than all
   eight. */
static const unsigned char ega_r[16] = {
    0x00,0x00,0x00,0x00,0xAA,0xAA,0xAA,0xAA,
    0x55,0x55,0x55,0x55,0xFF,0xFF,0xFF,0xFF };
static const unsigned char ega_g[16] = {
    0x00,0x00,0xAA,0xAA,0x00,0x00,0x55,0xAA,
    0x55,0x55,0xFF,0xFF,0x55,0x55,0xFF,0xFF };
static const unsigned char ega_b[16] = {
    0x00,0xAA,0x00,0xAA,0x00,0xAA,0x00,0xAA,
    0x55,0xFF,0x55,0xFF,0x55,0xFF,0x55,0xFF };

/* THE ATTRIBUTE BYTE pairs a foreground index with a background index that
   is its own palette entry at fg+128 -- so every one of the sixteen
   backgrounds this driver can produce has to be explicitly programmed to
   black, or the cell shows whatever was in that unprogrammed slot. Uno saw
   that as an all-white screen.
 *
 * Set PSEL and CSEL ONCE and then stream CR/CG/CB: the FX core auto-advances
 * the colour index after each triple, and re-setting CSEL per entry is what
 * appears to scramble the palette. */
static void load_ega_palette(void) {
    unsigned char i;

    PSEL = 1;
    CSEL = 0;
    for (i = 0; i < 16; i++) { CR = ega_r[i]; CG = ega_g[i]; CB = ega_b[i]; }

    PSEL = 1;
    CSEL = 128;
    for (i = 0; i < 16; i++) { CR = 0; CG = 0; CB = 0; }
}

/* ---- the font ------------------------------------------------------- */

/* THIS PORT HAS NO CHARACTER GENERATOR TO INHERIT, which is the one real
 * difference from the other three 6502 targets. The C128 uses the set its
 * KERNAL already put in VDC RAM and the MEGA65 the C65's; the Atari's ROM
 * font has letters and digits but no box-drawing glyphs at all, and the
 * console is built out of box-drawing glyphs.
 *
 * So the font is BUILT at init, in C64/C128 SCREEN CODE order, from two
 * sources:
 *
 *   letters, digits, punctuation   the Atari OS ROM's own set, read through
 *                                  CHBAS the way the OS documents
 *   the box-drawing set            authored here -- thirteen glyphs, plus a
 *                                  saucer and two arrows
 *
 * THE BOX GLYPHS ARE THIS PORT'S OWN ARTWORK and shared with the Amiga's
 * amigagfx.c, which drew them first for the same reason: the shared
 * layout.h names them by C64 screen code, and neither topaz nor the Atari
 * ROM has anything at those code points. Geometry, not copying -- a
 * vertical is two pixels wide at the centre so it meets a horizontal
 * cleanly and joins between adjacent cells.
 *
 * REVERSE VIDEO IS A RULE, NOT ENTRIES. Codes 128..255 are their base glyph
 * inverted, so G_BLOCK (160) falls out of space, the badge disc's bottom
 * (226) out of 98, and the systems bar (228) out of 100. Writing those out
 * by hand would be three more chances to disagree with the rule. */
struct box_glyph { unsigned char code; unsigned char row[8]; };

static const struct box_glyph box[] = {
  /* 32  space, stated rather than left to the ROM, so that 160 -- the solid
         cell every panel fill and the badge's body use -- is solid even if
         CHBAS points somewhere unexpected */
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
  /* 98  the badge disc's top: lower half filled, which rounds the top edge.
         Its reverse, 226, is the bottom and needs no entry. */
                          { 98, {0,0,0,0,0xFF,0xFF,0xFF,0xFF}},
  /* 100 the bottom row alone, and its ONLY reason for existing is that its
         reverse is 228, the systems bar: seven rows filled on an eight-pixel
         pitch, which leaves the hairline between bars that ui.c measured off
         the original. */
                          {100, {0,0,0,0,0,0,0,0xFF}},
  /* 30, 31  the up and left arrows -- the two of screen codes 27..31 with no
         ASCII to borrow. Unused by the game as it stands; a screen code with
         a glyph is one that cannot come out as a marker later. */
                          { 30, {0x18,0x3C,0x7E,0x18,0x18,0x18,0x18,0}},
                          { 31, {0,0x10,0x30,0x7E,0x30,0x10,0,0}},
  /* 81  the ship's saucer. The badge and the info panel put this next to
         four cells of G_HLINE and a solid block, so what it has to be is a
         round body that reads as a hull at 8x8 -- wider than tall, clear of
         the top and bottom rows so it does not merge with its neighbours. */
                          { 81, {0,0x3C,0x7E,0xFF,0xFF,0x7E,0x3C,0}}
};
#define BOX_COUNT ((unsigned char)(sizeof box / sizeof box[0]))

/* A GRAPHICS CODE NOBODY DREW renders as a hollow box, not as nothing.
   The Amiga port missed two of fifteen on its first pass, and an invisible
   glyph leaves a panel looking merely empty -- which is the bug that does
   not get reported. */
static const unsigned char marker[8] = {0,0x7E,0x42,0x42,0x42,0x42,0x7E,0};

/* Screen code -> ASCII, for everything the Atari ROM can supply. The C64
   unshifted set puts '@' at 0 and A..Z at 1..26; 32..63 are ASCII already.
   27 and 29 are the brackets, and they are real: the play-again prompt is
   drawn as "[YES]" and "[NO]". */
static int code_to_ascii(unsigned char c) {
    if (c == 0) return '@';
    if (c <= 26) return 'A' + c - 1;
    if (c == 27) return '[';
    if (c == 29) return ']';
    if (c >= 32 && c <= 63) return c;
    return -1;
}

static void font_build(void) {
    /* The ROM set, found the documented way rather than hardcoded at $E000:
       CHBAS holds its base PAGE. 128 glyphs of 8 bytes, indexed in Atari's
       own internal order, where ASCII $20..$5F maps to $00..$3F -- which is
       every character code_to_ascii can return. */
    const unsigned char *rom =
        (const unsigned char *)((unsigned int)OS_CHBAS << 8);
    unsigned char *win = VBXE_WIN;
    unsigned int code;

    vbxe_bank(VRAM_FONT_BANK);

    for (code = 0; code < 128; code++) {
        const unsigned char *g;
        unsigned char i;
        int ascii;

        g = (const unsigned char *)0;
        for (i = 0; i < BOX_COUNT; i++) {
            if (box[i].code == (unsigned char)code) { g = box[i].row; break; }
        }
        if (!g) {
            ascii = code_to_ascii((unsigned char)code);
            g = (ascii >= 0x20 && ascii <= 0x5F)
                    ? rom + ((ascii - 0x20) << 3)
                    : marker;
        }

        for (i = 0; i < 8; i++) {
            win[(code << 3) + i] = g[i];
            /* Reverse video, as a rule: 128..255 are 0..127 inverted. */
            win[1024 + (code << 3) + i] = (unsigned char)~g[i];
        }
    }
}

/* ---- the display list ------------------------------------------------ */

/* TWO ENTRIES, and the split between them is trap 3.
 *
 * Entry 1 is an 8-scanline top border with the overlay OFF, and its whole
 * job is to program OVADR, CHBASE and OVATT so they are already in effect
 * when entry 2 runs. CHBASE set in the SAME entry as text-mode-on produces
 * entirely blank glyphs with correct backgrounds -- the overlay fetches
 * glyphs using the CHBASE a PRIOR entry established.
 *
 * Entry 2 turns text mode on for ROWS*8 scanlines, re-sets OVADR (it kept
 * auto-advancing through the border) and ends the list. 25 rows x 8 = 200
 * scanlines, plus the 8-line border = 208, inside the Atari raster.
 *
 * TWENTY-FIVE ROWS, NOT UNO'S TWENTY-FOUR. Measured before this port
 * existed: a test filling every row rendered ROW 00 through ROW 24, 80
 * columns wide, on AltirraSDL. layout.c puts the bottom band on rows 17..24
 * and needs all of them. */
static void xdl_build(void) {
    unsigned char *win = VBXE_WIN;
    unsigned int step = VDC_COLS * 2;          /* bytes per text row */
    unsigned int n = VRAM_XDL_OFF;

    vbxe_bank(VRAM_SCREEN_BANK);

    /* Entry 1. Low control: OVOFF $04 | MAPOFF $10 | RPTL $20 | OVADR $40.
       High control: CHBASE $01 | OVATT $08. */
    win[n++] = 0x04 | 0x10 | 0x20 | 0x40;
    win[n++] = 0x01 | 0x08;
    win[n++] = 7;                              /* RPTL: 7 more lines, 8 total */
    win[n++] = 0; win[n++] = 0; win[n++] = 0;  /* OVADR = VRAM $00000 */
    win[n++] = (unsigned char)(step & 0xFF);   /* OVADR step */
    win[n++] = (unsigned char)((step >> 8) & 0x0F);
    win[n++] = VRAM_FONT_CHBASE;
    /* OVATT byte 1: bits0-1 = width (01 = NORMAL, 640px / 80 cols), bit4 =
       overlay palette select. PALETTE 1 IS PROGRAMMED ABOVE, so bit 4 MUST
       be set -- with $01 the overlay reads the unprogrammed palette 0 and
       the foreground is indistinguishable from the background, which looks
       like blank text. */
    win[n++] = 0x11;
    win[n++] = 255;                            /* OVATT byte 2: on top */

    /* Entry 2. Low control: TMON $01 | MAPOFF $10 | RPTL $20 | OVADR $40.
       High control: END $80. */
    win[n++] = 0x01 | 0x10 | 0x20 | 0x40;
    win[n++] = 0x80;
    win[n++] = (unsigned char)(VDC_ROWS * 8 - 1);
    win[n++] = 0; win[n++] = 0; win[n++] = 0;
    win[n++] = (unsigned char)(step & 0xFF);
    win[n++] = (unsigned char)((step >> 8) & 0x0F);

    XDL_ADR0 = (unsigned char)(VRAM_XDL_ADDR & 0xFF);
    XDL_ADR1 = (unsigned char)((VRAM_XDL_ADDR >> 8) & 0xFF);
    XDL_ADR2 = (unsigned char)((VRAM_XDL_ADDR >> 16) & 0x07);
}

/* ---- the seam -------------------------------------------------------- */

void vdc_init(void) {
    MEMAC_CONTROL = MEMAC_OPEN;
    cur_bank = 0xFF;

    load_ega_palette();
    font_build();
    scr_clear();
    xdl_build();

    /* xdl_enabled (bit0) and no_trans (bit2, so foreground index 0 is a real
       black rather than "transparent" -- simpler than tracking which indices
       are safe to fill with). */
    VIDEO_CONTROL = 0x01 | 0x04;

    /* Stop ANTIC fetching a playfield. The overlay covers the whole screen,
       so the stock display underneath is only noise bleeding through the
       border. SDMCTL is the OS shadow the vertical-blank routine copies into
       DMACTL, so writing DMACTL directly would be undone one frame later. */
    OS_SDMCTL = 0;
}

void vdc_shutdown(void) {
    /* Give ANTIC its playfield back and close the window, so whatever runs
       next sees a normal machine. Deliberately does NOT clear the screen:
       the final console staying up is more useful and more honest than a
       black screen, which reads as a crash. Same call the C128 makes. */
    VIDEO_CONTROL = 0;
    MEMAC_CONTROL = 0;
    OS_SDMCTL = 0x22;
}

/* RESET, because there is nothing to return to -- this program occupies
   $3000..$BFFF, which is everything between DOS and the OS ROM. COLDSV at
   $E477 is the XL OS's own cold-start entry; the ROM is mapped there
   throughout, since nothing here ever banks it out. */
void plat_exit(void) {
    vdc_shutdown();
    __asm__ volatile("jmp $e477");
}

/* ANTIC's VCOUNT keeps running under VBXE -- the overlay rides on top of the
   normal ANTIC/GTIA timing rather than replacing it, the same way the C128's
   wait_vsync still reads the VIC-IIe raster while the VDC drives the
   picture. VCOUNT counts half-scanlines and wraps every field. */
void wait_vsync(void) {
    while (ANTIC_VCOUNT != 0) {}
    while (ANTIC_VCOUNT == 0) {}
}

static unsigned char ascii_to_screencode(char c) {
    unsigned char u = (unsigned char)c;
    if (u >= 32 && u <= 63) return u;           /* space, digits, punctuation */
    if (u >= 64 && u <= 95) return (unsigned char)(u - 64);
    if (u >= 97 && u <= 122) return (unsigned char)(u - 96);
    if (u >= 193 && u <= 218) return (unsigned char)(u - 192);
    return 32;
}

/* NOINLINE ON EVERY scr_* ENTRY POINT, AND IT WAS MEASURED, NOT ASSUMED.
 * ui.c calls these from hundreds of sites and -Oz still inlined the address
 * arithmetic into a good many of them; pinning them down is worth 242 bytes
 * of resident space on the tightest target in the project. It is not worth
 * anything on a roomy one, which is why no other port does it.
 *
 * WHAT IT DOES NOT BUY BACK is the other 3,000. Replacing the stubs with
 * this driver cost 4,636 bytes in the early link against the driver's own
 * 1,559 -- the rest is main() and the eight ui_draw_* routines growing,
 * because a stub that folds to one volatile write lets the optimiser
 * collapse the argument setup at every call site and a real one does not.
 * The X16's "4,539 bytes of driver layer" is a measurement of DRIVERS; the
 * seam costs more than the driver. See tools/budget.py.
 */

/* THE ATTRIBUTE BYTE: bit7 = opaque, bits0-6 = foreground palette index.
   The background is that index plus 128, and load_ega_palette programmed
   128..143 black -- so a colour in 0..15 draws on black, which is the whole
   of what this console asks for. */
#define ATTR(c) ((unsigned char)(0x80 | ((c) & 0x0F)))

__attribute__((noinline)) void scr_clear(void) {
    unsigned char *p = VBXE_WIN;
    unsigned int i;

    vbxe_bank(VRAM_SCREEN_BANK);
    for (i = 0; i < (unsigned int)VDC_COLS * VDC_ROWS; i++) {
        *p++ = 32;
        *p++ = ATTR(EGA_BLACK);
    }
}

__attribute__((noinline)) void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color) {
    unsigned char *p = VBXE_WIN + (((unsigned int)y * VDC_COLS + x) << 1);

    vbxe_bank(VRAM_SCREEN_BANK);
    p[0] = ch;
    p[1] = ATTR(color);
}

__attribute__((noinline)) void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color) {
    unsigned char *p = VBXE_WIN + (((unsigned int)y * VDC_COLS + x) << 1);
    unsigned char attr = ATTR(color);

    vbxe_bank(VRAM_SCREEN_BANK);
    while (*s) {
        *p++ = ascii_to_screencode(*s++);
        *p++ = attr;
    }
}

__attribute__((noinline)) void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color) {
    unsigned char attr = ATTR(color);
    unsigned char r, i;

    vbxe_bank(VRAM_SCREEN_BANK);
    for (r = 0; r < h; r++) {
        unsigned char *p = VBXE_WIN + ((((unsigned int)(y + r)) * VDC_COLS + x) << 1);
        for (i = 0; i < w; i++) { *p++ = ch; *p++ = attr; }
    }
}

__attribute__((noinline)) void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color) {
    scr_fill_rect(x, y, w, 1, ch, color);
}

__attribute__((noinline)) void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color) {
    scr_fill_rect(x, y, 1, h, ch, color);
}

/* ---- the message log's backing store --------------------------------- */

/* ui.c speaks the C128 VDC's cursor API because ui.c is shared, and on that
   machine these reach spare VDC RAM at $1000. Here they reach VBXE VRAM
   above the font -- 512K of it rather than 4K, so nothing has to be
   rationed. The address arrives as the C128's own, and the bank is folded
   out of its high bits.

   A BANK SELECT PER BYTE, deliberately: the log is written a record at a
   time and read once when the player opens the message view, so the cost is
   nothing and the generality is worth more than the bytes it saves. */
static unsigned int log_addr;

__attribute__((noinline)) void vdc_set_address(unsigned int addr) {
    log_addr = addr;
}

__attribute__((noinline)) void vdc_data_write(unsigned char value) {
    vbxe_bank((unsigned char)(VRAM_LOG_BANK + (log_addr >> 12)));
    VBXE_WIN[log_addr & 0x0FFF] = value;
    log_addr++;
}

__attribute__((noinline)) unsigned char vdc_data_read(void) {
    unsigned char v;
    vbxe_bank((unsigned char)(VRAM_LOG_BANK + (log_addr >> 12)));
    v = VBXE_WIN[log_addr & 0x0FFF];
    log_addr++;
    return v;
}
