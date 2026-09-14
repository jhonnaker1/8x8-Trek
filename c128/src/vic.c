#include "vdc.h"        /* the seam every caller sees -- see vic.h */
#include "vic.h"

#define VIC_RASTER  (*(volatile unsigned char *)0xD012)
#define VIC_BORDER  (*(unsigned char *)0xD020)
#define VIC_BGND    (*(unsigned char *)0xD021)
#define VIC_MEMPTR  (*(unsigned char *)0xD018)
#define C128_CLKRATE (*(unsigned char *)0xD030)

#define VIC_BLACK 0

/* The same converter vdc.c carries, and for the same reason: scr_puts takes
   text and everything else takes a RAW screen code. The box-drawing glyphs
   live at screen codes 64-127, which is precisely the range this rewrites, so
   routing a panel border through here would silently turn it into a letter.
   Copied rather than shared because the two drivers are never linked
   together; if a third target ever needs it, that is the moment to lift it
   out, not now. */
static unsigned char ascii_to_screencode(char c) {
    unsigned char u = (unsigned char)c;
    if (u >= 32 && u <= 63) return u;           /* space, digits, punctuation */
    if (u >= 64 && u <= 95) return u - 64;      /* ASCII @A-Z[\]^_ */
    if (u >= 97 && u <= 122) return u - 96;     /* ASCII lowercase */
    if (u >= 193 && u <= 218) return u - 192;   /* PETSCII A-Z */
    return 32;
}

void vdc_init(void) {
    /* C128 ONLY, AND IT IS THE OPPOSITE OF WHAT vdc.c DOES. The 80-column
       build sets $D030 bit 0 for 2MHz precisely BECAUSE nothing ever displays
       the VIC-IIe -- "the VIC-IIe's own picture becomes unwatchable at 2x, it
       cannot fetch coherently". This build IS the VIC-IIe, so the bit must be
       CLEAR, and clearing it is not optional tidiness: at 2MHz there is no
       picture to look at. A 40-column C128 therefore runs at 1MHz, half what
       the 80-column port does, which is also an honest preview of C64 timing.
       See NOTES.md item 57. */
    C128_CLKRATE &= (unsigned char)~0x01;

    /* Video matrix at $0400, character generator at $1000 -- the UPPERCASE/
       GRAPHICS half of the ROM, which is where the box-drawing glyphs and the
       capital letters live. $14 = (0x0400 >> 6) | (0x1000 >> 10).
       C128 ONLY, AND WRITING $D018 IS NOT ENOUGH -- MEASURED, not reasoned.
       The first version of this driver set $D018 = $14 and the smoke test came
       up with every letter LOWERCASE. Reading the register back gave $17
       (char base $1800, the lowercase/uppercase set), twice, with the machine
       running in between -- so the write landed and something put it back.
       That something is THE KERNAL'S OWN SCREEN EDITOR IRQ, which re-asserts
       $D018 every frame from its variable VM1 at $0A2C, and $0A2C read $16.
       So the KERNAL's copy is the one that has to change; $D018 is set as well
       only so the first frame is right rather than one frame late.
       vdc.c never met this because it deliberately leaves the character base
       alone -- "there is nothing for the KERNAL's cursor IRQ to undo". A
       40-column build has to own a register the KERNAL believes is its own.
       The screen codes were never in doubt: $0400 held 05 07 01 20 14 12 05 0B
       throughout, which is EGA TREK. Only the font bank was wrong, which is
       exactly the failure that cannot report itself -- a screen full of the
       wrong letters looks like a screen. */
    *((unsigned char *)0x0A2C) = 0x14;      /* VM1: what the IRQ re-asserts */
    VIC_MEMPTR = 0x14;                      /* and now, for this frame */

    /* The console is drawn on black, like the original. The VIC-IIe has ONE
       background for the whole screen -- unlike the VDC's per-cell attribute,
       this is a global register -- which is exactly the model scr_put already
       assumes: one foreground per cell, common background. */
    VIC_BGND   = VIC_BLACK;
    VIC_BORDER = VIC_BLACK;

    scr_clear();
}

void vdc_shutdown(void) {
    /* Nothing to undo. The 80-column driver has to drop back to 1MHz here;
       this one never left it, and the screen and charset pointers are the
       values the KERNAL itself uses. Deliberately does NOT clear: the
       farewell has to survive, which is the C128's own rule and the bug the
       X16 and the Amiga both shipped in v0.13.0. */
}

/* THE SAME ANSWER AS THE 80-COLUMN BUILD, and for the same measured reason:
   this program occupies BASIC's entire text area, so there is nothing to
   return to, and llvm-mos's own exit path BRKs into the monitor. Jump through
   the reset vector and let the KERNAL bring the machine up clean. The full
   disassembly of why is in vdc.c; it is not repeated here because one copy of
   a finding is enough and two copies drift. */
void plat_exit(void) {
    __asm__ volatile("jmp ($fffc)");
}

/* The raster register, read directly rather than through the VDC's ready
   poll. Unlike the 80-column build this IS the chip drawing the picture, so
   the wait is a real vertical blank rather than a borrowed timer. */
void wait_vsync(void) {
    while (VIC_RASTER != 0) {}
    while (VIC_RASTER == 0) {}
}

void scr_clear(void) {
    unsigned int i;
    for (i = 0; i < (unsigned int)VIC_COLS * VIC_ROWS; i++) {
        VIC_SCREEN[i] = 32;
        VIC_COLRAM[i] = VIC_BLACK;
    }
}

/* NO SEEK, WHICH IS THE WHOLE DIFFERENCE FROM THE VDC. The 80-column driver
   pays an address-register write, a data-register write and a ready poll per
   cell, and goes to some trouble to seek once per RUN instead of once per
   cell. Here a cell is two stores to memory the CPU can address, so hline,
   vline and fill_rect have nothing to optimise and are written as loops over
   scr_put. Half the cells, direct writes, half the megahertz -- whether that
   nets out faster than the VDC build is UNMEASURED. */
void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color) {
    unsigned int off = (unsigned int)y * VIC_COLS + x;
    VIC_SCREEN[off] = ch;
    VIC_COLRAM[off] = color & 0x0F;
}

void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color) {
    unsigned int off = (unsigned int)y * VIC_COLS + x;
    const char *p;
    for (p = s; *p; p++, off++) {
        VIC_SCREEN[off] = ascii_to_screencode(*p);
        VIC_COLRAM[off] = color & 0x0F;
    }
}

void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color) {
    unsigned int off = (unsigned int)y * VIC_COLS + x;
    unsigned char i;
    for (i = 0; i < w; i++, off++) {
        VIC_SCREEN[off] = ch;
        VIC_COLRAM[off] = color & 0x0F;
    }
}

void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color) {
    unsigned char i;
    for (i = 0; i < h; i++) scr_put(x, (unsigned char)(y + i), ch, color);
}

void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color) {
    unsigned char row;
    for (row = 0; row < h; row++) scr_hline(x, (unsigned char)(y + row), w, ch, color);
}
