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
/* $FBFF, NOT $FCFF. $FC00..$FCFF is ALWAYS KERNAL ROM on this machine --
   it cannot be banked at all -- so a store there is discarded and a read
   returns the ROM. The first limit was $FCFF, which would have let the pool
   grow into it and reported a load as fine while the last 256 bytes were
   somebody else's code. STRINGS.DAT and MUSIC.DAT together are 7,908 bytes
   from $DD00, ending $FC23 -- 36 bytes INTO that page. It was already over. */
#define FAR_LIMIT  0xFBFF

#define LFN_FAR  3
#define DEV      8

static uint16_t far_len = 0;

/* BELOW $8000, AND NOTHING IN THE BANKED WINDOW TOUCHES A C LOCAL.
 *
 * Two separate requirements, and the second is the one that nearly forced an
 * awkward memory map. Above $8000 the ROM hides the program during a KERNAL
 * call: harmless for code that is not executing and for a return address that
 * becomes visible again the moment the ROM goes out -- but NOT harmless for
 * llvm-mos's soft stack, which C locals spill to. Put the stack high and a
 * spill inside the window writes into ROM shadow.
 *
 * The alternative was splitting `ram` either side of a low stack, and that
 * does not work: code and rodata are about 37K on the C128's 40-column build
 * and neither half of any split is that big, so the general code must span
 * $8000 contiguously.
 *
 * SO THE WINDOW IS ASSEMBLY AND ITS OPERANDS ARE IN LOW RAM. Between the two
 * stores to $FF3E and $FF3F nothing is touched but the 6502's own hardware
 * stack at $0100 -- which no banking can hide -- and these eight bytes, which
 * .lowbss keeps below $8000 by the same first-in-the-script trick as
 * .lowtext. The soft stack is then free to live anywhere, and the memory map
 * question dissolves.
 *
 * NOT `static`, AND THAT IS NOT AN OVERSIGHT: the assembly below names these
 * symbols, and LTO renames or internalises a static that C code alone can see.
 * The link failed with "undefined symbol: k_namlen" until they were given
 * external linkage. Inline assembly is outside the compiler's view of who
 * uses what.
 *
 * ALL THREE CALLS ARE INSIDE THE WINDOW. SETLFS and SETNAM go through $FFBA
 * and $FFBD, which are as much KERNAL as LOAD is; an earlier version banked
 * in only around LOAD and left the other two jumping into this program.
 */
__attribute__((used, section(".lowbss"))) unsigned char k_namlen;
__attribute__((used, section(".lowbss"))) unsigned char k_namlo, k_namhi;
__attribute__((used, section(".lowbss"))) unsigned char k_dstlo, k_dsthi;
__attribute__((used, section(".lowbss"))) unsigned char k_endlo, k_endhi;
__attribute__((used, section(".lowbss"))) unsigned char k_err;

/* THE FILENAME HAS TO LIVE DOWN HERE TOO, and this is the fault that made the
   game draw a title screen and then a blank prompt with a cursor.
   
   SETNAM is called INSIDE the banked window, with the ROM mapped. The name
   this function is handed is a string literal in .rodata -- measured at
   $A2B3 for "STRINGS.DAT", with .rodata running $9F5E..$A817 -- so with the
   ROM in, the KERNAL read the filename out of BASIC ROM and the LOAD failed.
   
   p4bank.c's cbm_k_setnam has copied its names into .lowbss since it was
   written, and says why in a comment. This file makes the same three KERNAL
   calls in its own assembly and never got the same treatment -- so overlays
   arrived (they go through p4bank.c) and the string pool did not (it comes
   through here). The game played on with blank labels, which is exactly what
   main.c says a disk with no STRINGS.DAT should do. */
__attribute__((used, section(".lowbss"))) char k_name[20];

__attribute__((noinline, section(".lowtext")))
static void kernal_load_raw(void)
{
    __asm__ volatile (
        "sta $ff3e\n"                 /* ROM in -- the KERNAL becomes real   */
        "lda #3\n"                    /* SETLFS: logical file 3, device 8,   */
        "ldx #8\n"                    /*         secondary 0 -- a RAW load,  */
        "ldy #0\n"                    /*         not ,1 relocating           */
        "jsr $ffba\n"
        "lda k_namlen\n"              /* SETNAM: length in A, pointer in X/Y */
        "ldx k_namlo\n"
        "ldy k_namhi\n"
        "jsr $ffbd\n"
        "lda #0\n"                    /* LOAD: A=0 loads, X/Y is the address */
        "ldx k_dstlo\n"
        "ldy k_dsthi\n"
        "jsr $ffd5\n"
        "stx k_endlo\n"               /* one past the last byte, or an error */
        "sty k_endhi\n"
        "lda #0\n"
        "rol\n"                       /* carry set means the KERNAL failed   */
        "sta k_err\n"
        "sta $ff3f\n"                 /* RAM back, before anything returns   */
        ::: "a", "x", "y", "p", "memory");
}

uint16_t far_load(const char *name)
{
    uint16_t base = far_len;
    uint16_t dest = (uint16_t)(FAR_BASE + base);
    uint16_t end;

    if (base >= (uint16_t)(FAR_LIMIT - FAR_BASE)) return FAR_NONE;

    {   /* Set up outside the window, in ordinary C, with the ROM out. */
        unsigned char n = 0;
        while (name[n] && n < (unsigned char)(sizeof k_name - 1)) {
            k_name[n] = name[n];
            n++;
        }
        k_namlen = n;
        k_namlo = (unsigned char)((uint16_t)(uintptr_t)k_name & 0xFF);
        k_namhi = (unsigned char)((uint16_t)(uintptr_t)k_name >> 8);
        k_dstlo = (unsigned char)(dest & 0xFF);
        k_dsthi = (unsigned char)(dest >> 8);
        kernal_load_raw();
        if (k_err) return FAR_NONE;
        end = (uint16_t)(k_endlo | ((uint16_t)k_endhi << 8));
    }

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
