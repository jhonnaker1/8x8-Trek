#include "vdc.h"

/* The VDC's registers and its RAM -- SHARED BY BOTH C128 SCREEN DRIVERS.
 *
 * WHY THIS IS ITS OWN FILE. These five functions are not part of the
 * 80-column picture; they are the seam ui.c uses to keep its 2K MESSAGE LOG
 * (LOG_SLOTS 32 x LOG_STRIDE 64, based at $1000) in memory that is not the
 * program's. EVERY PORT IMPLEMENTS THEM -- amigagfx.c, falconvid.c, m65mem.c,
 * x16vera.c, vbxevid.c, coco3vid.c all define vdc_set_address -- so the name
 * is the C128's but the seam is the project's.
 *
 * THE 40-COLUMN BUILD KEEPS USING VDC RAM, and that is not a hack: the VDC
 * chip is still in the machine, it simply is not driving the monitor, and its
 * sixteen kilobytes are sitting idle. The accessors work whether or not the
 * VDC is displaying, because they go through registers 18/19/31.
 *
 * A C64 HAS NO SUCH CHIP AND THIS IS A REAL GAP IN THE C64 SCOPE -- see
 * NOTES.md item 57. The scope said far memory was the one new seam; it is
 * two, and the second is 2K of READ/WRITE scratch that has been invisible
 * because every port so far had a video chip with RAM to spare.
 */

#define VDC_CTRL (*(volatile unsigned char *)0xD600)
#define VDC_DATA (*(volatile unsigned char *)0xD601)
#define VIC_RASTER (*(unsigned char *)0xD012)
#define C128_CLKRATE (*(unsigned char *)0xD030)

static void wait_ready(void) {
    while (!(VDC_CTRL & 0x80)) {}
}

unsigned char vdc_reg_read(unsigned char reg) {
    wait_ready();
    VDC_CTRL = reg;
    wait_ready();
    return VDC_DATA;
}

void vdc_reg_write(unsigned char reg, unsigned char value) {
    wait_ready();
    VDC_CTRL = reg;
    wait_ready();
    VDC_DATA = value;
}

void vdc_set_address(unsigned int addr) {
    vdc_reg_write(18, (unsigned char)(addr >> 8));
    vdc_reg_write(19, (unsigned char)(addr & 0xFF));
}

void vdc_data_write(unsigned char value) {
    vdc_reg_write(31, value);
}

unsigned char vdc_data_read(void) {
    return vdc_reg_read(31);
}
