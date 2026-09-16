#include <cbm.h>
#include <stdint.h>

#include "../../core/farmem.h"

/* Plus/4 far memory -- and it is CHEAPER THAN THE C64'S, which is not what
 * the survey expected.
 *
 * WHY THERE IS NO BANKING ON THE READ PATH AT ALL. This port runs with RAM
 * mapped everywhere ($FF3F), exactly as the C64 port runs with BASIC out. The
 * ROM comes back only around a KERNAL call. So for the whole time the game is
 * drawing, `$8000..$FCFF` is plain RAM the CPU can address, and a string fetch
 * is a memcpy -- where c64mem.c must set map 101, copy, and restore it with
 * interrupts off because the 6502's vectors move while it does.
 *
 * WHAT STILL NEEDS THE ROM IS THE LOAD, and that works for the same measured
 * reason the C64's does: WRITES PASS THROUGH. src/wrprobe.c wrote $BE/$EF to
 * $A000/$E000 with the ROM mapped and read them back after banking, so the
 * KERNAL's own LOAD, running out of ROM, stores the file into the RAM beneath
 * itself. One call, no chunking.
 *
 * THE HAZARD IS NOT THE VECTORS, IT IS THE CALLER. On a C64 the danger of
 * unmapping the KERNAL is that $FFFA..$FFFF become RAM and an NMI goes
 * somewhere arbitrary. Here the opposite applies: while the ROM is IN, every
 * byte of the program above $8000 is invisible -- so the code that performs
 * the call, and the return address it goes back to, must be BELOW $8000.
 * kernal_load() below is that code and says so in its section attribute.
 */

/* $8000..$FCFF is 32,000 bytes of RAM under the ROM. The store takes the TOP
 * of it so the resident image can grow upward into the rest, and stops at
 * $FCFF because $FD00..$FF3F is I/O on this machine whatever is banked. */
#define FAR_BASE   0xDD00
#define FAR_LIMIT  0xFCFF

#define LFN_FAR  3
#define DEV      8

static uint16_t far_len = 0;

/* BELOW $8000, AND THE ATTRIBUTE IS LOAD-BEARING. Everything above $8000 is
 * hidden while the ROM is mapped, so a KERNAL call issued from up there would
 * return into ROM shadow. This function banks the ROM in, calls, banks it out,
 * and only then returns -- so its own address is the one thing that must stay
 * visible throughout.
 *
 * A FUNCTION ATTRIBUTE AND NOT A PER-FILE PLACEMENT, because this build uses
 * LTO: every translation unit becomes one `.lto.o` and a linker script that
 * matches `*p4mem.o(.text*)` would match nothing. Section attributes survive
 * LTO; file names do not.
 */
__attribute__((noinline, section(".lowtext")))
static uint16_t kernal_load(const char *fname, uint16_t dest)
{
    uint16_t end;
    /* ROM IN FIRST, BEFORE SETLFS -- these are all KERNAL calls. The first
       version banked in only around cbm_k_load and left setlfs and setnam
       jumping through $FFBA/$FFBD into RAM that holds this program. They are
       as much KERNAL as LOAD is; the window has to cover all three. */
    *(volatile unsigned char *)0xFF3E = 0;
    cbm_k_setlfs(LFN_FAR, DEV, 0);
    cbm_k_setnam(fname);
    end = (uint16_t)(uintptr_t)cbm_k_load(0, (void *)dest);
    *(volatile unsigned char *)0xFF3F = 0;      /* RAM back before returning */
    return end;
}

uint16_t far_load(const char *name)
{
    uint16_t base = far_len;
    uint16_t dest = (uint16_t)(FAR_BASE + base);
    uint16_t end;

    if (base >= (uint16_t)(FAR_LIMIT - FAR_BASE)) return FAR_NONE;

    end = kernal_load(name, dest);

    /* cbm_k_load returns one past the last byte, or a KERNAL ERROR CODE in the
       low byte with carry set -- which is indistinguishable here, so the range
       is checked instead. c64mem.c learned that the same way. */
    if (end <= dest || end > FAR_LIMIT) return FAR_NONE;

    far_len = (uint16_t)(end - FAR_BASE);
    return base;
}

uint16_t far_size(void) { return far_len; }

/* A memcpy. No bank switch, no interrupts-off window, no per-byte KERNAL call
   -- compare c128/src/farmem.c, which needs an INDFET through $FF74 for every
   single byte and an assembly loop to make that affordable. */
void far_read(uint16_t off, void *dst, uint8_t len)
{
    const unsigned char *src = (const unsigned char *)(uintptr_t)(FAR_BASE + off);
    unsigned char *d = (unsigned char *)dst;
    uint8_t i;
    for (i = 0; i < len; i++) d[i] = src[i];
}
