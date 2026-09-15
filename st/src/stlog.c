#include "../../c128/src/vdc.h"

/* The ST's message log: 2K of plain RAM.
 *
 * ui.c keeps a 32-entry log outside the program's address space through three
 * functions whose names are the C128's -- that is where the seam was first
 * cut, against a real 8563. EVERY PORT IMPLEMENTS THEM. On a machine with
 * half a megabyte the honest answer is an array, exactly as on the Amiga, the
 * Falcon and the C64.
 *
 * It is its own file rather than eight lines at the foot of stvid.c because
 * the Falcon keeps its copy inside falconvid.c, and this port does not use
 * falconvid.c -- so a reader looking for the log would find it in a file this
 * directory does not compile. Same shape as c64/src/c64log.c.
 */

/* ui.c's LOG_BASE, and its stride and slot count. Written out rather than
   rounded to 2048, so a bigger log is a BOUNDS FAILURE here rather than a
   quiet corruption of whatever the linker put next. */
#define LOG_ORIGIN  0x1000
#define LOG_BYTES   (32 * 64)

static unsigned char logstore[LOG_BYTES];
static unsigned int  log_cursor = 0;

/* Write-and-advance: ui.c sets an address once and streams a whole 55-byte
   record through it, which is why the cursor is a file static. */
void vdc_set_address(unsigned int addr)
{
    log_cursor = (addr >= LOG_ORIGIN) ? (addr - LOG_ORIGIN) : 0;
}

void vdc_data_write(unsigned char value)
{
    if (log_cursor < LOG_BYTES) logstore[log_cursor] = value;
    log_cursor++;
}

unsigned char vdc_data_read(void)
{
    unsigned char v = (log_cursor < LOG_BYTES) ? logstore[log_cursor] : 0;
    log_cursor++;
    return v;
}
