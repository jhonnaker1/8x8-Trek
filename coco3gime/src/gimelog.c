#include "../../c128/src/vdc.h"

/* The message log: 2K, IN THE GAP ABOVE THE SCREEN.
 *
 * ui.c keeps a 32-entry log outside the program through three functions whose
 * names are the C128's. Every port implements them; on the roomy machines
 * they are a plain array, and on the C128 they are real VDC RAM.
 *
 * HERE THEY ARE A FIXED ADDRESS, and the reason is that this port has a gap
 * nothing else wants. The screen is 4,000 bytes at $E000, so $EFA0..$FEFF --
 * 3,936 bytes below the I/O page -- is RAM the address space has already
 * spent. Putting the log at $F000 costs the program NOTHING, where a `static
 * unsigned char logstore[2048]` would cost 2K of the budget this port is
 * tight on. Same trick as the screen: a literal, checkable by eye.
 */
#define LOG_ORIGIN  0x1000                    /* ui.c's LOG_BASE */
#define LOG_BYTES   (32 * 64)
#define LOGSTORE    ((unsigned char *)0xF000) /* $F000..$F7FF, in the gap */

static unsigned int log_cursor = 0;

void vdc_set_address(unsigned int addr)
{
    log_cursor = (addr >= LOG_ORIGIN) ? (unsigned int)(addr - LOG_ORIGIN) : 0;
}

void vdc_data_write(unsigned char value)
{
    if (log_cursor < LOG_BYTES) LOGSTORE[log_cursor] = value;
    log_cursor++;
}

unsigned char vdc_data_read(void)
{
    unsigned char v = (log_cursor < LOG_BYTES) ? LOGSTORE[log_cursor] : 0;
    log_cursor++;
    return v;
}
