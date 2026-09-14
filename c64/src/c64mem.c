#include <cbm.h>
#include <stdint.h>

#include "../../core/farmem.h"

/* C64 far memory: the 8K of RAM that lives under the KERNAL ROM.
 *
 * THIS IS THE CHEAPEST FAR STORE OF ANY 6502 PORT, and the reason is a
 * property of the machine rather than anything clever here: ON A C64, WRITES
 * ALWAYS GO TO RAM. Only reads are affected by what $01 maps. So the KERNAL's
 * own LOAD routine, running out of the KERNAL ROM at $F4A5, stores the file
 * through (EAL),Y into the RAM underneath itself -- and the ROM it is
 * executing from stays mapped the whole time. The file lands at $E000 in one
 * call with no banking, no window and no chunking.
 *
 * Reading it back is the only part that banks. $01 bits 2..0 pick the map:
 *
 *     110  (the state llvm-mos's unmap-basic.o leaves)  KERNAL + I/O, RAM at $A000
 *     101  (what this file selects to read)             I/O only, RAM at $A000 AND $E000
 *
 * so the change is "set LORAM, clear HIRAM", NOT "clear HIRAM" -- 100 is RAM
 * ONLY and takes the I/O page away with it, which would be a machine with no
 * VIC, no SID and no CIAs for the duration of a string fetch. The value is
 * computed from whatever $01 already holds rather than written as a constant,
 * so the datasette bits above 2 are preserved and nothing here depends on
 * llvm-mos choosing $3E over $36.
 *
 * Compare c128/src/farmem.c, which needs an INDFET call PER BYTE through
 * $FF74 and an assembly loop to make that affordable. Here it is a memcpy.
 */

#define PORT   (*(volatile unsigned char *)0x0001)
#define MAP_IO_ONLY 0x05          /* bits 2..0: I/O in, both ROMs out */
#define MAP_MASK    0x07

/* $E000..$FFF9. THE TOP SIX BYTES ARE NOT THE STORE'S, and that is the whole
 * hazard of this design in one line: while $01 selects map 101 the 6502 takes
 * its NMI, RESET and IRQ vectors from RAM at $FFFA..$FFFF, because the KERNAL
 * ROM that normally answers those addresses is not there.
 *
 * SEI DOES NOT COVER THIS. The IRQ is masked; the NMI is not maskable at all,
 * and on this machine it is wired to the RESTORE key -- so a player leaning on
 * RESTORE during a string fetch would send the CPU to whatever two bytes of
 * prose happened to be sitting at $FFFA. Nothing about that failure would ever
 * point back here.
 *
 * So the vectors are written into RAM once, pointing at an RTI, and the store
 * stops six bytes short of the top to leave room for them. An NMI in the
 * window is then survivable and invisible, which is the correct behaviour for
 * a game that has taken the machine.
 */
#define FAR_BASE   0xE000
#define FAR_LIMIT  0xFFFA

static uint16_t far_len = 0;

/* A REAL RTI, not a C function -- a C function returns with RTS, which would
   leave the processor status and the return address on the stack and run off
   into whatever called last. Module-level asm so it has an address the vectors
   can point at. */
__asm__(".globl c64_nmi_rti\nc64_nmi_rti:\n\trti\n");
extern void c64_nmi_rti(void);

static uint8_t vectors_done = 0;

static void arm_ram_vectors(void) {
    unsigned char *v = (unsigned char *)0xFFFA;
    uint16_t a = (uint16_t)(uintptr_t)&c64_nmi_rti;
    if (vectors_done) return;
    /* Stores reach RAM whatever $01 says, so this needs no banking of its
       own -- that is the same property far_load relies on. */
    v[0] = (unsigned char)(a & 0xFF); v[1] = (unsigned char)(a >> 8);  /* NMI   */
    v[2] = (unsigned char)(a & 0xFF); v[3] = (unsigned char)(a >> 8);  /* RESET */
    v[4] = (unsigned char)(a & 0xFF); v[5] = (unsigned char)(a >> 8);  /* IRQ   */
    vectors_done = 1;
}

#define DEV      8
#define LFN_FAR  1        /* clear of c64storage.c's 2 and 15 */

/* Long enough for "0:" plus a 16-character CBM name and a terminator. */
static char fname[20];

uint16_t far_load(const char *name) {
    uint16_t base = far_len;
    uint16_t dest = (uint16_t)(FAR_BASE + base);
    uint16_t end;
    uint8_t i = 0, j = 0;

    arm_ram_vectors();

    if (base >= (uint16_t)(FAR_LIMIT - FAR_BASE)) return FAR_NONE;

    /* "0:" selects drive 0 of the unit -- a 1541 has only one and wants the
       prefix anyway, and an SD2IEC needs it. */
    fname[i++] = '0'; fname[i++] = ':';
    while (name[j] && i < (uint8_t)(sizeof fname - 1)) fname[i++] = name[j++];
    fname[i] = '\0';

    /* NO SETBNK. That is a C128 KERNAL call ($FF68) and this machine has no
       such entry point -- the address is inside the KERNAL's own code here.
       c128/src/farmem.c needs it because the C128 KERNAL defaults to putting
       loaded data in bank 15. */
    cbm_k_setlfs(LFN_FAR, DEV, 0);
    cbm_k_setnam(fname);
    end = (uint16_t)(uintptr_t)cbm_k_load(0, (void *)dest);

    /* cbm_k_load returns one past the last byte, or a KERNAL ERROR CODE --
       a small number, never a plausible address in the store. A file that
       loaded nothing is a failure too, which is what the missing-MUSIC.DAT
       case relies on to play silently rather than crash. */
    if (end <= dest || end > FAR_LIMIT) return FAR_NONE;

    far_len = (uint16_t)(end - FAR_BASE);
    return base;
}

uint16_t far_size(void) { return far_len; }

/* Parameters through fixed globals and "memory" on every block, for the same
   reason c128/src/farmem.c gives at length: an __asm__ with no memory clobber
   may be REORDERED past the stores around it, and the first version of the
   C128's read came back with the loop index instead of the data. Here the
   stakes are higher -- a reordered bank switch reads the KERNAL ROM and hands
   the string pool six kilobytes of 6502 opcodes, which would draw as a screen
   full of plausible-looking rubbish. */
static volatile unsigned char saved_port;
/* `used`, because the ONLY reference to this is inside an __asm__ string and
   the compiler cannot see into one -- without it, -Werror kills the build on
   -Wunused-variable and dropping the attribute instead would let the optimiser
   delete the storage the assembly writes to. Same trick, same reason as the
   fixed globals in c128/src/farmem.c. */
static volatile unsigned char saved_flags __attribute__((used));

void far_read(uint16_t off, void *dst, uint8_t len) {
    const unsigned char *src = (const unsigned char *)(uintptr_t)(FAR_BASE + off);
    unsigned char *d = (unsigned char *)dst;
    uint8_t i;

    if (len == 0) return;

    /* THE I FLAG IS SAVED, NOT ASSUMED. A bare sei/cli pair would re-enable
       interrupts on return whatever the caller had them set to -- which is
       correct for the game (it runs from BASIC with the KERNAL IRQ live) and
       wrong the first time anything calls this from inside a critical
       section. php/pla into a byte keeps it honest and costs four bytes. */
    __asm__ volatile("php\n\tpla\n\tsta saved_flags\n\tsei"
                     ::: "a", "memory");

    saved_port = PORT;
    PORT = (unsigned char)((saved_port & (unsigned char)~MAP_MASK) | MAP_IO_ONLY);

    for (i = 0; i < len; i++) d[i] = src[i];

    PORT = saved_port;

    __asm__ volatile("lda saved_flags\n\tpha\n\tplp"
                     ::: "a", "memory", "p");
}
