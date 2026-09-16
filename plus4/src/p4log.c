#include "../../c128/src/vdc.h"

/* The Plus/4's message log: 2K of plain RAM, and the C64's reasoning applies
 * here with more room to spare rather than less.
 *
 * ui.c keeps a 32-entry log outside the program's address space through three
 * functions whose names are the C128's -- vdc_set_address, vdc_data_write,
 * vdc_data_read -- because that is where the seam was first cut on a machine
 * that had an 8563 with spare VRAM. Every port since implements them somehow:
 * amigagfx.c, falconvid.c, m65mem.c, x16vera.c, vbxevid.c, coco3vid.c,
 * gimelog.c and c64log.c.
 *
 * A PLUS/4 HAS NO SUCH CHIP EITHER, and the C64's answer is the right one
 * again: a normal array, because this port has the room. $1001..$FCFF is
 * 60,671 bytes against the C64's 46,847, so 2,048 of them buy a log with no
 * banking and no seam at all.
 *
 * It does NOT go above $8000 with the far store, and that is a real reason
 * rather than symmetry with the C64: everything up there is hidden whenever
 * the ROM is banked in for a KERNAL call, and a message written during a disk
 * operation would land in ROM shadow. The log is written from ui.c at
 * unpredictable moments. Low RAM costs nothing here and has no such rule.
 *
 * Copied from c64/src/c64log.c rather than shared -- three functions, eight
 * lines, and the two ports are never linked together. The alternative is a
 * header that exists to hold one array. c64log.c says the same of amigagfx.c.
 */

/* ui.c's LOG_BASE, and its stride and slot count. Written out rather than
 * rounded to 2048 so a bigger log is a BOUNDS FAILURE here rather than a quiet
 * corruption of whatever the linker put next. */
#define LOG_ORIGIN  0x1000
#define LOG_BYTES   (32 * 64)

/* .noinit, not .bss: two kilobytes of zeroes at startup is two kilobytes the
   C runtime has to walk, and log_count in ui.c already knows how many entries
   are real. Nothing reads a slot that has not been written. */
__attribute__((section(".noinit")))
static unsigned char logstore[LOG_BYTES];

static unsigned int log_cursor = 0;

void vdc_set_address(unsigned int addr) {
    log_cursor = (addr >= LOG_ORIGIN) ? (addr - LOG_ORIGIN) : 0;
}

void vdc_data_write(unsigned char value) {
    if (log_cursor < LOG_BYTES) logstore[log_cursor] = value;
    log_cursor++;
}

unsigned char vdc_data_read(void) {
    unsigned char v = (log_cursor < LOG_BYTES) ? logstore[log_cursor] : 0;
    log_cursor++;
    return v;
}
