/* IT WAS NEVER THE LOW-RAM SCREEN EITHER. IT IS vdc_init's INIT0 = $00.
 *
 * lowram.c concluded "low RAM breaks the disk" and re-confirmed it under the
 * real loader, and it is wrong for the same reason item 30 was wrong. Between
 * its control read and its failing read it changed TWO things:
 *
 *     before = plat_raw_open(...)     <- STOR_OK
 *     vdc_init();                     <- WRITES INIT0 = $00
 *     fill 4,000 bytes at $1000
 *     first  = plat_raw_open(...)     <- NOT FOUND
 *
 * and blamed the fill. `$00` clears MC2, the GIME's standard SCS, which is the
 * chip select for $FF40-$FF5F where the WD1773 lives -- so vdc_init() switches
 * the disk controller off the bus, and has done since the day it was written.
 * Its own comment says "COCO=0, MMUEN=0 -- never set MMUEN", which is the
 * whole file organised around the wrong bit.
 *
 * lowbisect.c is the other half of the proof: every 512-byte block of
 * $0200..$27FF can be filled with $AA and restored with the disk reading
 * STOR_OK throughout. Low RAM was never the suspect.
 *
 * So this varies ONE thing -- the value vdc_init puts in INIT0 -- and after
 * each one sets up the rest of the video mode, fills the whole 4,000-byte
 * screen at $1000, and reads a file. Two nibbles per candidate:
 *
 *     high   the read after the mode change and the fill
 *     low    the read after INIT0 is put back to a value known to work
 *
 * STOR_OK is 0, STOR_NOTFOUND 1, STOR_ERROR 2.
 */
#include "../../core/storage.h"

#define INIT0     (*(unsigned char *)0xFF90)
#define VMODE     (*(unsigned char *)0xFF98)
#define VRES      (*(unsigned char *)0xFF99)
#define VSTART_HI (*(unsigned char *)0xFF9D)
#define VSTART_LO (*(unsigned char *)0xFF9E)
#define SCREEN    ((unsigned char *)0x1000)
#define NCELLS    (80 * 25)
#define r         ((unsigned char *)0x5F00)
#define DONE      0x5A

/* $00 is what vdc_init ships. The rest keep MC2 (bit 2) and differ in MC3
   (bit 3, the $FE00 vector page) and COCO (bit 7). $8C is the CoCo-mode
   value closest to what Disk BASIC itself runs under. */
static const unsigned char cand[4] = { 0x00, 0x04, 0x0C, 0x8C };

static unsigned int got;
static unsigned char buf[512];

static void video(void)
{
    unsigned long v;
    unsigned int i;

    VMODE = 0x03;
    VRES  = (unsigned char)((0x01 << 5) | (0x05 << 2) | 0x01);
    v = (0x70000UL + 0x1000UL) >> 3;
    VSTART_HI = (unsigned char)((v >> 8) & 0xFF);
    VSTART_LO = (unsigned char)(v & 0xFF);

    for (i = 0; i < NCELLS; i++) {
        SCREEN[i * 2]     = (unsigned char)('A' + (i % 26));
        SCREEN[i * 2 + 1] = (unsigned char)(((i % 8) << 3));
    }
}

int main(void)
{
    unsigned char c, hit, back;
    unsigned int i;

    for (i = 0; i < 32; i++) r[i] = 0xFF;
    r[0] = 0xA5;
    asm { orcc #$50 }
    asm { lds #$5E00 }
    asm { sta $FFDF }

    r[1] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);   /* control */

    for (c = 0; c < 4; c++) {
        INIT0 = cand[c];
        video();
        hit = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

        INIT0 = 0x8C;                    /* MC2 back on */
        back = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);

        r[2 + c] = (unsigned char)((hit << 4) | (back & 0x0F));
    }

    /* Did the 4,000 bytes at $1000 survive all of that? */
    r[6] = 1;
    for (i = 0; i < NCELLS; i++)
        if (SCREEN[i * 2] != (unsigned char)('A' + (i % 26))) { r[6] = 0; break; }

    r[31] = DONE;
    for (;;) ;
}
