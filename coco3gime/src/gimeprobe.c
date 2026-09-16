/* Can the GIME give this console 80x25 text with eight per-cell colours, on a
 * machine with NO SuperSprite card?
 *
 * THIS IS A MEASUREMENT, NOT A DRIVER. Three things have to come back from the
 * machine before a driver is worth writing, and none of them can be read out
 * of a register: the GIME's control registers are WRITE-ONLY on real hardware
 * -- NOTES.md item 30 records $FF90 and $FF91 reading back $1B, the floating
 * bus, whatever was put there. So the screen is the instrument.
 *
 *   1. 80 columns and 25 ROWS. `WIDTH 80` from BASIC gives 24, and this
 *      console is 80x25. 200 scanlines at 8 per character row is exactly 25,
 *      and LPF in $FF99 is what selects 200.
 *   2. THE SCREEN CAN LIVE IN THE PORT'S OWN 64K. $FF9D/$FF9E point the video
 *      at PHYSICAL memory, so the port can aim it at a buffer it already
 *      owns and never touch the MMU -- which matters enormously, because
 *      MMUEN alone permanently breaks disk access on this machine and is a
 *      parked, unexplained blocker.
 *   3. Eight foreground colours, distinct, from the attribute byte.
 *
 * WHY NOT LET BASIC DO IT. Measured first: `WIDTH 80` works, but the screen it
 * sets up is NOT in the CPU's address space -- a search of all 64K for a
 * string BASIC had just printed found it only in the input buffer. BASIC pages
 * its screen in to write to it. This port cannot, so it brings its own.
 */
/* No <stdint.h>: cmoc hands the host cc the preprocessing, and on this Mac
   that pulls in arm64 system headers and fails with "architecture not
   supported". The CoCo 3 port carries its own shim for exactly that, and
   this probe needs no fixed-width types anyway. */

#define INIT0  (*(unsigned char *)0xFF90)
#define VMODE  (*(unsigned char *)0xFF98)
#define VRES   (*(unsigned char *)0xFF99)
#define VSTART_HI (*(unsigned char *)0xFF9D)
#define VSTART_LO (*(unsigned char *)0xFF9E)
#define PALETTE ((unsigned char *)0xFFB0)

/* The buffer the video will be pointed at. 80 x 25 cells of two bytes is
   4,000; this sits well clear of the probe's own code at $3000. */
#define SCREEN ((unsigned char *)0x4000)
#define COLS 80
#define ROWS 25

/* PHYSICAL, NOT CPU. $FF9D/$FF9E hold the video start in EIGHT-BYTE UNITS of
   a PHYSICAL address, and with the MMU off the CPU's 64K is the TOP 64K of
   whatever RAM the machine has. On a 128K CoCo 3 that is physical $10000, so
   CPU $4000 is physical $14000 and the register value is $14000/8 = $2800.
   IF THE MACHINE HAS 512K THIS IS WRONG and the screen will show it -- which
   is the point of looking rather than asserting. */
#define PHYS_BASE 0x70000UL
#define VSTART ((PHYS_BASE + 0x4000UL) >> 3)

int main(void)
{
    unsigned int i;
    unsigned char r, c;

    asm { orcc #$50 }

    /* COCO=0 selects the GIME's own modes; MMUEN stays 0 -- see above.
       MC3..MC0 = 0 leaves the ROM map alone in its power-up arrangement. */
    INIT0 = 0x00;

    /* Alphanumeric (BP=0), 8 scanlines per character row (LPR=011). */
    VMODE = 0x03;

    /* LPF=01 -> 200 lines -> 25 rows of 8. HRES=101 -> 80 characters.
       CRES=01 -> the attribute byte is live. */
    VRES = (0x01 << 5) | (0x05 << 2) | 0x01;

    VSTART_HI = (unsigned char)((VSTART >> 8) & 0xFF);
    VSTART_LO = (unsigned char)(VSTART & 0xFF);

    /* EGA's first eight, near enough to judge distinctness: the GIME's RGB
       palette is two bits a gun, so these are the closest it has. */
    PALETTE[0] = 0x00;  /* black   */
    PALETTE[1] = 0x09;  /* blue    */
    PALETTE[2] = 0x12;  /* green   */
    PALETTE[3] = 0x1B;  /* cyan    */
    PALETTE[4] = 0x24;  /* red     */
    PALETTE[5] = 0x2D;  /* magenta */
    PALETTE[6] = 0x26;  /* brown   */
    PALETTE[7] = 0x3F;  /* white   */
    /* 8..15 ARE THE FOREGROUNDS. The attribute byte's FGND field is three
       bits and indexes palette entries 8-15, BGND indexes 0-7 -- so the eight
       colours the console needs go here, not in 0-7. Putting white in all of
       them is what made the first successful picture uniformly white. */
    PALETTE[8]  = 0x12;  /* green    */
    PALETTE[9]  = 0x1B;  /* cyan     */
    PALETTE[10] = 0x24;  /* red      */
    PALETTE[11] = 0x2D;  /* magenta  */
    PALETTE[12] = 0x26;  /* brown    */
    PALETTE[13] = 0x38;  /* lt gray  */
    PALETTE[14] = 0x0B;  /* lt blue  */
    PALETTE[15] = 0x16;  /* lt green */

    /* EVERY CHARACTER THE GIME HAS, 0..255, sixteen to a row with its code
       in hex at the left. The console's borders are C128 SCREEN CODES in
       layout.h -- 64 is G_HLINE, 93 G_VLINE, 112/110/109/125 the corners --
       and the GIME's character generator is FIXED: there is no user font in
       text mode. So what is at those codes decides whether this port can draw
       the console's boxes at all, or has to fall back on - | and +. */
    {
        static const char hex[] = "0123456789ABCDEF";
        unsigned char code = 0;
        for (r = 0; r < 16; r++) {
            unsigned int base = (unsigned int)(r + 4) * COLS * 2;
            SCREEN[base + 0] = hex[r]; SCREEN[base + 1] = 0x38;
            SCREEN[base + 2] = '0';    SCREEN[base + 3] = 0x38;
            SCREEN[base + 4] = ':';    SCREEN[base + 5] = 0x38;
            for (c = 0; c < 16; c++) {
                unsigned int o = base + (unsigned int)(6 + c * 3) * 2;
                SCREEN[o]     = code++;
                SCREEN[o + 1] = 0x28;          /* fgnd 5, bgnd 0 */
            }
        }
        /* A row of the codes layout.h actually asks for, so they can be
           judged side by side rather than hunted for in the grid. */
        {
            static const unsigned char want[] =
                { 64, 93, 112, 110, 109, 125, 107, 115, 114, 113, 91, 160, 98, 100, 81 };
            unsigned int base = (unsigned int)21 * COLS * 2;
            for (c = 0; c < 15; c++) {
                SCREEN[base + (unsigned int)(c * 3) * 2]     = want[c];
                SCREEN[base + (unsigned int)(c * 3) * 2 + 1] = 0x10;
            }
        }
    }

    for (;;) { }
    return 0;
}
