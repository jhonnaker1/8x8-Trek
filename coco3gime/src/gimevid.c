/* Video for a CoCo 3 with NO SuperSprite: the GIME's own 80x25 text mode.
 *
 * MEASURED BEFORE IT WAS WRITTEN, and the measurements changed the design
 * three times. src/gimeprobe.c drew a numbered ruler and the whole character
 * set on a bare machine under MAME; NOTES.md, "THE CARD-LESS COCO 3 IS AN
 * 80-COLUMN PORT", has the pictures' findings. The short version:
 *
 *   80 COLUMNS AND 25 ROWS, which is the console Anderson drew -- so this
 *   port links layout.c, not layout40.c. `WIDTH 80` from BASIC gives 24 rows;
 *   the 25th comes from LPF in $FF99, which is why this file programs the
 *   GIME itself rather than inheriting BASIC's setup.
 *
 *   EIGHT FOREGROUND COLOURS, and that is exactly the number the console
 *   needs -- tools/check_colours.py derives eight information-bearing colours
 *   and the GIME carries all eight distinctly. Seven decorative colours fold;
 *   no game rule does.
 *
 *   NO MMU, AND THAT IS THE POINT. $FF9D/$FF9E aim the video at PHYSICAL
 *   memory, so the screen is a buffer in this program's own 64K. Setting
 *   MMUEN permanently breaks disk access on this machine (NOTES.md item 30,
 *   unexplained, parked) and this port never sets it.
 *
 * THE FONT IS THE GIME'S AND CANNOT BE CHANGED. Text mode has no user
 * character generator, and the probe showed what is actually there: ASCII in
 * 32..127, accented characters in 0..31, and 128..255 MIRRORING 0..127 --
 * no reverse video, no solid block, and no box-drawing glyphs at all.
 *
 * SO THE BOXES ARE ASCII AND THE SOLIDS ARE BACKGROUND COLOUR. layout.h's
 * G_HLINE, G_VLINE and the corners are C128 screen codes that would render as
 * `a ] p n m }` here -- gibberish -- so they map to `-`, `|` and `+`. That is
 * a visible difference from every other port and it is the honest one.
 * G_BLOCK goes the other way and comes out BETTER: a solid cell is a SPACE
 * with the background set, which the attribute byte gives for free, so the
 * badge and the bars are true solid colour rather than a glyph.
 */
#include "../../c128/src/vdc.h"
#include "../../core/ega.h"
#include "../../c128/src/layout.h"
#include "egagime.h"        /* GIME_INIT0 -- gimesnd.c writes the same register */

#define INIT0     (*(unsigned char *)0xFF90)
#define VMODE     (*(unsigned char *)0xFF98)
#define VRES      (*(unsigned char *)0xFF99)
#define VSTART_HI (*(unsigned char *)0xFF9D)
#define VSTART_LO (*(unsigned char *)0xFF9E)
#define PALETTE   ((unsigned char *)0xFFB0)
#define CPU_SLOW  (*(unsigned char *)0xFFD8)

#ifdef TREK_40COL
#define COLS 40
#else
#define COLS 80
#endif
#define ROWS 25

/* THE SCREEN IS AT A FIXED ADDRESS, NOT A LINKER-PLACED ARRAY, and that is
   a correction rather than a preference. The first version declared
   `extern unsigned char gime_screen[]` and computed the video start from its
   address with 32-bit arithmetic; the screen filled with junk, because what
   $FF9D/$FF9E ended up holding was not where the array was. A literal is
   checkable by eye against the register value and cmoc cannot get it wrong.
   IN LOW RAM AT $1000, BELOW THE PROGRAM, and that is where the last 1,608
   bytes came from. The port loads at $2800 and everything under it looked
   like Disk BASIC's -- but this port runs in ALL-RAM MODE with cmoc's
   STANDALONE WD1773 driver, so once the game starts there is no ROM down
   there and no Disk BASIC workspace to protect. MEASURED, not assumed: every
   DSKCON variable -- DCOPC, DCBPT, DRGRAM, NMIFLG, DNMIVC -- resolves into
   the program's OWN BSS at $D498 and above, which the map says and a
   datasheet could not.
   4,000 bytes at $1000 is clear of the direct page, clear of the first-stage
   loader's stub at $2800, and clear of its body at $6000 -- and the buffer is
   not written until vdc_init(), by which time both have finished. */
#define gime_screen ((unsigned char *)0x1000)
#define SCREEN_CPU   0x1000UL

/* PHYSICAL, AND THE MACHINE'S RAM SIZE DECIDES IT. With the MMU off the CPU's
   64K is the TOP 64K of whatever is fitted, so a CPU address is physical
   (RAMTOP - 64K + addr) and $FF9D/$FF9E want that in EIGHT-BYTE UNITS. The
   first probe assumed 128K on a 512K machine and painted a white screen out
   of memory nothing had written -- one constant, two machines, and only the
   picture can tell you which. Detected at run time below rather than baked in. */
static unsigned long phys_base = 0x70000UL;

/* THE ATTRIBUTE BYTE. Bit 7 blink, bit 6 underline, bits 5-3 FOREGROUND and
   bits 2-0 BACKGROUND -- and the two fields index DIFFERENT HALVES of the
   palette: foreground selects entries 8-15, background 0-7. The first picture
   that worked came out uniformly white because the colours had been loaded
   into 0-7. */
#define ATTR(fg, bg) ((unsigned char)((((fg) & 7) << 3) | ((bg) & 7)))

/* Background entry 0 is black, so colour 0 cannot be a solid. Light green is
   the nearest thing to green this palette has; see vdc_init. */
#define SOLID_BG(c) ((unsigned char)(((c) & 7) ? ((c) & 7) : 7))



/* EGA's eight information-bearing colours in the GIME's two-bits-a-gun RGB,
   in the order core/ega.h numbers them after EGA_TO_VDC folds the other
   seven onto them. See egagime.h. */
static const unsigned char pal_fg[8] = {
    0x12,  /* 0 green    */
    0x1B,  /* 1 cyan     */
    0x24,  /* 2 red      */
    0x2D,  /* 3 magenta  */
    0x26,  /* 4 brown    */
    0x38,  /* 5 lt gray  */
    0x0B,  /* 6 lt blue  */
    0x16   /* 7 lt green */
};

static unsigned char ascii_of(unsigned char code);

void vdc_init(void)
{
    unsigned int i;

    /* $70000 IS CORRECT ON BOTH A 512K AND A STOCK 128K COCO 3, and that was
       MEASURED, not reasoned. On a 512K machine it is the top 64K -- the
       window the CPU sees with the MMU off. On a 128K machine it is past the
       end of RAM and the GIME ALIASES it down to $10000, which is that
       machine's top 64K. One constant, both machines, and no RAM upgrade
       required.
       The asymmetry is worth knowing because it is not obvious: the reverse
       does NOT work. $10000 on a 512K machine is real, addressable RAM that
       the CPU is not looking at, so the video faithfully displays whatever is
       in it -- which is how an earlier build painted a white screen out of
       memory nothing had written. A too-HIGH address wraps; a too-LOW one
       silently shows the wrong memory.
       Verified by running the console test under MAME with `-ramsize 128k`:
       identical picture. */
    phys_base = 0x70000UL;

    /* $04, NOT $00, AND THE DIFFERENCE IS THE WHOLE DISK. Bit 2 is MC2, the
       GIME's standard SCS -- the chip select that decodes $FF40..$FF5F, where
       the WD1773 is. `$00` switches the disk controller off the bus, and this
       line did that from the day it was written; the comment it used to carry
       said "never set MMUEN", which is this file organised around the wrong
       bit. It is what made lowram.c report that a screen at $1000 breaks the
       disk -- two variables changed between that probe's control and its test,
       and the fill took the blame for vdc_init.
       Measured, src/lowinit.c: $00 NOT FOUND, $04/$0C/$8C all STOR_OK with
       this same mode set and 4,000 bytes written at $1000. The value itself
       lives in egagime.h, because gimesnd.c writes the same register and it
       cannot be read back to OR a bit into. */
    INIT0 = GIME_INIT0;
    VMODE = 0x03;                 /* alphanumeric, 8 scanlines a row */
    #ifdef TREK_40COL
    /* HRES=001 is 40 characters; CRES=01 keeps the attribute byte. */
    VRES  = (unsigned char)((0x01 << 5) | (0x01 << 2) | 0x01);
#else
    VRES  = (unsigned char)((0x01 << 5) | (0x05 << 2) | 0x01);
#endif

    /* (physical >> 3), and BOTH halves are written out so the value can be
       read straight off the page and compared with the picture. 512K machine:
       $70000 + $E000 = $7E000, >> 3 = $FC00. */
    {
        unsigned long v = (phys_base + SCREEN_CPU) >> 3;
        VSTART_HI = (unsigned char)((v >> 8) & 0xFF);
        VSTART_LO = (unsigned char)(v & 0xFF);
    }

    /* THE BACKGROUND HALF OF THE PALETTE CARRIES COLOURS, NOT EIGHT BLACKS.
       It held black in all eight entries, which made every SOLID CELL on this
       port black-on-black -- and a solid cell here is a space with its
       background set, so that is the badge disc and, worse, the entire
       "EGA TREK" banner: ui.c draws those block letters out of G_BLOCK and
       nothing else. The title screen had no title on it and I had not
       noticed, because the badge was rendering the WRONG GLYPH in the
       foreground colour and looked like something.
       Entry 0 stays black -- it is the background of every ordinary text cell
       and of scr_clear -- so seven of the eight colours are reachable as a
       background and green, index 0, is not. SOLID_BG substitutes light green
       for it: a visible near-miss rather than an invisible cell. */
    for (i = 0; i < 8; i++) PALETTE[i] = pal_fg[i];
    PALETTE[0] = 0x00;
    for (i = 0; i < 8; i++) PALETTE[8 + i] = pal_fg[i];

    scr_clear();
}

/* Deliberately does NOT clear: the farewell has to survive it, which is the
   C128's rule and the bug the X16 and Amiga both shipped in v0.13.0. */
void vdc_shutdown(void)
{
    CPU_SLOW = 0;                 /* Disk BASIC expects its own speed back */
}

void plat_exit(void)
{
    asm { orcc #$50 }             /* no interrupts while the map changes */
    asm { sta $FFDE }             /* ROM/RAM mode: Disk BASIC comes back */
    asm { jmp [$FFFE] }           /* the machine's own reset vector */
}

/* The GIME's vertical border interrupt would be the honest vsync, but it
   needs IEN and an interrupt handler this port does not otherwise want. The
   console is event-driven and nothing shared calls this -- the same position
   the Amiga is in. */
void wait_vsync(void) { }

void scr_clear(void)
{
    unsigned int i;
    for (i = 0; i < COLS * ROWS; i++) {
        gime_screen[i * 2]     = ' ';
        gime_screen[i * 2 + 1] = ATTR(5, 0);
    }
}

void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color)
{
    unsigned int o;

    if (x >= COLS || y >= ROWS) return;
    o = ((unsigned int)y * COLS + x) * 2;

    /* A SOLID CELL IS A SPACE WITH A BACKGROUND, not a glyph. This font has
       no block, and doing it this way is better than one would be: the badge
       and the laser bars come out as true flat colour. */
    /* THE BADGE DISC'S ROUNDED ENDS BECOME SQUARE ONES. G_HALF_LO and
       G_HALF_HI are half-filled cells on a C128, and they round the disc's top
       and bottom; this font has no half block and no overline, so there is
       nothing to round with. They draw solid, which makes the badge a filled
       rectangle with its star in the middle rather than a filled disc -- a
       visible difference from the other ports, and the honest one. */
    if (ch == G_BLOCK || ch == G_HALF_LO || ch == G_HALF_HI) {
        gime_screen[o]     = ' ';
        gime_screen[o + 1] = ATTR(0, SOLID_BG(color));
        return;
    }

    /* A HORIZONTAL RULE IS AN UNDERLINED SPACE, NOT A DASH. Attribute bit 6
       is UNDERLINE and it draws across the WHOLE cell, so consecutive cells
       join into one continuous line -- where `-` leaves a gap at every cell
       boundary and the console's long borders come out dotted. The line sits
       at the bottom of the cell rather than the middle, which is where a box
       rule belongs anyway. */
    if (ch == G_HLINE) {
        gime_screen[o]     = ' ';
        gime_screen[o + 1] = (unsigned char)(ATTR(color, 0) | 0x40);
        return;
    }
    /* The junctions that have a horizontal arm get the same underline, with
       a vertical bar in the cell for the arm that goes up or down. A corner
       is then a real corner rather than a plus sign. */
    if (ch == G_TL || ch == G_TR || ch == G_TEE_D ||
        ch == G_BL || ch == G_BR || ch == G_TEE_U ||
        ch == G_TEE_L || ch == G_TEE_R || ch == G_CROSS) {
        gime_screen[o]     = '|';
        gime_screen[o + 1] = (unsigned char)(ATTR(color, 0) | 0x40);
        return;
    }
    /* THE GAUGE BAR, AND EVERY ? JAMIE SAW WAS THIS ONE CODE. ui.c's
       G_BAR is 228 -- a C128 partial-height block, chosen there
       BECAUSE it is 7 pixels on an 8-pixel pitch, so bars on consecutive rows
       stay separate instead of fusing into one slab. This font has no partial
       block, and a solid background cell would fuse exactly as ui.c warns:
       SYSTEMS STATUS is six rows of paired bars and they would read as two
       vertical slabs.
       An UNDERLINED SPACE is the honest equivalent here. It is a rule at the
       bottom of the cell, continuous across cells, with the row's whole height
       of gap above it -- so a bar reads as a bar and six of them stacked stay
       six. Same mechanism as G_HLINE above; the colour still carries lit
       against unlit, which is what the gauge is actually saying.
       It drives the SYSTEMS STATUS bars, both laser gauges and the badge. */
    if (ch == G_BAR) {
        gime_screen[o]     = ' ';
        gime_screen[o + 1] = (unsigned char)(ATTR(color, 0) | 0x40);
        return;
    }

    /* THE SHIP'S NOSE. ui.c draws an enemy silhouette as a saucer, a hull and
       an engine block -- code 81, a filled circle on a C128, then G_HLINE then
       G_BLOCK. `O` is the nearest thing this font has and it keeps the
       silhouette reading left-to-right as a ship. */
    if (ch == G_SHIP) {
        gime_screen[o]     = 'O';
        gime_screen[o + 1] = ATTR(color, 0);
        return;
    }

    gime_screen[o]     = ascii_of(ch);
    gime_screen[o + 1] = ATTR(color, 0);
}

/* SCREEN CODE -> WHAT THIS FONT ACTUALLY HAS.
 *
 * The shared UI speaks C128 screen codes: 0 is '@', 1-26 are A-Z, 32-63 are
 * ASCII already, and 64-127 are the box-drawing set. This font is ASCII, so
 * the letters are a subtraction and the boxes are a SUBSTITUTION -- every
 * corner and tee becomes '+', horizontals '-', verticals '|'. Measured: at
 * those codes the GIME's own font shows `a ] p n m } k`, which is what the
 * console would have drawn without this table. */
static unsigned char ascii_of(unsigned char code)
{
    unsigned char c = code & 0x7F;      /* 128-255 mirror 0-127 in this font */

    switch (c) {
    case G_HLINE:  return '-';
    case G_VLINE:  return '|';
    case G_TL: case G_TR: case G_BL: case G_BR:
    case G_TEE_L: case G_TEE_R: case G_TEE_D: case G_TEE_U:
    case G_CROSS:  return '+';
    default: break;
    }
    if (c == 0) return '@';
    if (c <= 26) return (unsigned char)('A' + c - 1);
    if (c == 27) return '[';
    if (c == 29) return ']';
    if (c >= 32 && c <= 63) return c;
    /* Anything else has no glyph here. A visible marker beats a blank, which
       is the rule the Amiga's missing-glyph box established. */
    return '?';
}

void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color)
{
    const char *p;
    unsigned char u;

    for (p = s; *p && x < COLS; p++, x++) {
        u = (unsigned char)*p;
        if (u >= 64 && u <= 95)        u -= 64;
        else if (u >= 97 && u <= 122)  u -= 96;
        else if (u >= 193 && u <= 218) u -= 192;
        else if (!(u >= 32 && u <= 63)) u = 32;
        scr_put(x, y, u, color);
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
    for (i = 0; i < w && (unsigned char)(x + i) < COLS; i++)
        scr_put((unsigned char)(x + i), y, ch, color);
}

void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color)
{
    unsigned char i;
    for (i = 0; i < h && (unsigned char)(y + i) < ROWS; i++)
        scr_put(x, (unsigned char)(y + i), ch, color);
}
