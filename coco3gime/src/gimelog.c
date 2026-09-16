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
/* THE LOG MOVES INTO LOW RAM, and it buys the overlay window 2,048 bytes.
   It sat at $F200, directly above the window, so every byte bss grew squeezed
   the window -- and when it reached 2,048 the 2,500-byte MSGS.OVL stopped
   loading and the game said so on screen. Jamie found that; build_ovl refuses
   it now.
   $2000..$27FF is below the load address, above the 4,000-byte screen at
   $1000, and src/lowbisect.c filled and restored every 512-byte block of
   $0200..$27FF with the disk reading STOR_OK throughout. It is pure data --
   nothing here is ever executed -- and the loader has finished with the whole
   region before the game starts.
   The boot report and the storage trace lived at $2000/$2010 and move to
   $1FA0/$1FB0, in the gap between the screen and here; see coco3boot.c. */
#define LOGSTORE    ((unsigned char *)0x2000)

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
