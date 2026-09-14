#include "vdc.h"

/* The C64's message log: 2K of plain RAM, and the shape of that answer is the
 * point of this file.
 *
 * ui.c keeps a 32-entry log outside the program's address space through five
 * functions whose names are the C128's -- vdc_set_address, vdc_data_write,
 * vdc_data_read -- because that is where the seam was first cut. EVERY PORT
 * IMPLEMENTS THEM: amigagfx.c, falconvid.c, m65mem.c, x16vera.c, vbxevid.c,
 * coco3vid.c and c128/src/vdcram.c all do. The C128's 40-column build keeps
 * using real VDC RAM, because the 8563 is still in the machine even when it is
 * not driving the monitor.
 *
 * A C64 HAS NO SUCH CHIP. NOTES.md item 57 called that a real gap in the C64
 * scope -- "the scope said far memory was the one new seam; it is two" -- and
 * the honest answer turned out to be the cheap one: the log goes in a normal
 * array, because THIS PORT HAS THE ROOM. c64.ld gives the program $0801..$BEFF
 * against the C128's 39,935 bytes, since BASIC's 8K is RAM here and the
 * overlay window moved out to the 4K at $C000 that nothing ever covers. The
 * 40-column C128 build has 304 bytes spare; this one starts with about 7,200,
 * so 2,048 of them buy a log with no banking, no chip and no seam at all.
 *
 * It could have gone under the KERNAL beside the string pool instead. It does
 * not, for two reasons: the store at $E000 has only 282 bytes left once
 * STRINGS.DAT and MUSIC.DAT are in it, and every read from there costs an
 * interrupts-off bank switch (see c64mem.c) which this does not.
 *
 * Copied from amiga/src/amigagfx.c rather than shared. The three functions are
 * eight lines and the two ports are never linked together; the alternative is
 * a header that exists to hold one array.
 */

/* ui.c's LOG_BASE, and its stride and slot count. Written out rather than
 * rounded up to 2048, so that a bigger log is a BOUNDS FAILURE here rather
 * than a quiet corruption of whatever the linker put next. */
#define LOG_ORIGIN  0x1000
#define LOG_BYTES   (32 * 64)

/* .noinit, not .bss: two kilobytes of zeroes at startup is two kilobytes the
   C runtime has to walk, and log_count in ui.c already knows how many entries
   are real. Nothing reads a slot that has not been written. */
__attribute__((section(".noinit")))
static unsigned char logstore[LOG_BYTES];

static unsigned int log_cursor = 0;

/* Write-and-advance: ui.c sets an address once and streams a whole 55-byte
   record through it, which is why the cursor is a file static and not a
   parameter. */
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
