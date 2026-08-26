#include <cbm.h>
#include <stdint.h>

#include "../../core/farmem.h"

/* C128 far memory: RAM bank 1, through the KERNAL's FETCH and STASH.
 *
 * The MMU could map bank 1 in directly and let plain loads and stores reach
 * it, but the code doing the mapping would have to sit in the common RAM at
 * the bottom of memory, because switching banks changes what is visible at
 * every other address. FETCH and STASH are the KERNAL's supported way in and
 * need no relocation, at the cost of a subroutine call per byte. That is fine
 * for a store that is written once at startup and read a few times a second.
 *
 * $FF74 INDFET: A = the ZERO PAGE ADDRESS of a pointer, X = bank, Y = index
 *               -> A = the byte. Reading is all this needs now; the store is
 *               filled by the KERNAL's own LOAD, into bank 1 directly.
 *
 * $FB/$FC is free on the C128 and sits outside llvm-mos's imaginary registers
 * at $0A..$8F, so it is safe as that pointer.
 *
 * THE TRAP, and it cost a debugging round: an `__asm__` block with no "memory"
 * clobber may be REORDERED past the stores that set the pointer up. The first
 * version of this read back the loop index instead of the data. Every block
 * below declares "memory" and takes its operands from fixed globals rather
 * than from constraints, so there is nothing for the compiler to move.
 */

#define ZPPTR      0xFB
#define FAR_BANK   1

/* WHY $4000 AND NOT LOWER.
 *
 * The MMU can make the bottom of memory COMMON -- visible identically from
 * both banks -- and $D506 bits 1..0 pick the size: 00 = 1K, 01 = 4K, 10 = 8K,
 * 11 = 16K. The default is 1K at $0000..$03FF, so an earlier draft using
 * $2000 would have worked on a default machine and silently aliased to bank 0
 * if anything ever selected 16K common. $4000 is above every setting, so this
 * cannot be got wrong by something else changing the RCR.
 *
 * WHY IT IS SAFE TO USE AT ALL. Bank 1 is where BASIC keeps its VARIABLES --
 * that is what the second 64K is for. This is safe here because BASIC is not
 * running: the game is started with RUN, which clears variables anyway, and
 * BASIC only touches bank 1 while it is executing. When the game quits, BASIC
 * starts using bank 1 again and overwrites this, by which time it is dead.
 *
 * What this rules out is a future where the port returns to BASIC and expects
 * its far data to still be there. It will not be.
 *
 * Sources: oxyron.de MMU register reference, and the C64-Wiki C128 memory
 * model page; verified on the machine before use -- a pattern written at
 * $4000 in bank 1 read back correctly while bank 0 at $4000 was untouched. */
#define FAR_BASE   0x4000
#define FAR_LIMIT  0xC000        /* 32K of bank 1, $4000..$BFFF */

static uint16_t far_len = 0;

/* THE READ IS ONE ASSEMBLY LOOP.
 *
 * The first version wrapped FETCH in a C helper and let the compiler write
 * the loop. That cost 1,904 bytes -- more than the 1,082 of music data it was
 * displacing, so the seam lost space instead of freeing it. Every asm block
 * clobbers "memory", the compiler spills its imaginary registers around each
 * one, and doing that per byte is ruinous.
 *
 * One block, looping in assembly, pays for itself. Parameters come through
 * fixed globals because llvm-mos may place a local anywhere.
 *
 * THERE IS NO WRITE PATH ANY MORE. This used to carry a STASH half, used by
 * far_load() to push a file across 64 bytes at a time. far_load() now hands
 * the KERNAL bank 1 as its load target and the whole file lands there
 * directly -- see below -- so nothing writes to the store, which is what the
 * contract in core/farmem.h said all along ("read-only bulk data"). The
 * STASH branch, the direction flag and the $02B9 vector went with it.
 *
 * $FF74 INDFET: A = zero page address of a pointer, X = bank, Y = index -> A
 */
static volatile unsigned char far_lo, far_hi, far_n;

/* The RAM side of the transfer is reached with (zp),y, which REQUIRES a zero
   page pointer -- a .bss pointer gives "relocation R_MOS_ADDR8 out of range".
   $FD/$FE is free on the C128 and sits outside llvm-mos's imaginary registers
   at $0A..$8F, the same reasoning as $FB/$FC for the far side. */
#define ZPRAM 0xFD

static void far_move(uint16_t addr, unsigned char *ram, uint8_t len) {
    if (len == 0) return;
    far_lo = (unsigned char)(addr & 0xFF);
    far_hi = (unsigned char)(addr >> 8);
    *(volatile unsigned char *)ZPRAM       = (unsigned char)((uint16_t)ram & 0xFF);
    *(volatile unsigned char *)(ZPRAM + 1) = (unsigned char)((uint16_t)ram >> 8);
    far_n = len;

    __asm__ volatile(
        "        lda far_lo\n"
        "        sta $fb\n"
        "        lda far_hi\n"
        "        sta $fc\n"
        "        ldy #0\n"
        "1:      ldx #1\n"
        "        lda #$fb\n"
        "        jsr $ff74\n"           /* FETCH -> A */
        "        sta ($fd),y\n"
        "        iny\n"
        "        cpy far_n\n"
        "        bne 1b\n"
        ::: "a", "x", "y", "memory");
}

/* ONE KERNAL LOAD, STRAIGHT INTO BANK 1, and it is worth the explanation.
 *
 * This used to open the file through the disk seam and read it with
 * plat_read() in 64-byte chunks, STASHing each chunk across. MEASURED
 * 2026-08-26 (c128/test/loadbench.c): a byte-at-a-time CHRIN read runs at
 * about 2,300 bytes a second on this machine, where one KERNAL LOAD of the
 * same 4,096 bytes takes 6 to 14 jiffies -- between eight and eighteen times
 * faster. The port was spending seconds of every startup on it.
 *
 * The part that makes it simple as well as fast: SETBNK ($FF68) takes the
 * bank the DATA is to land in, so asking for bank 1 puts the file in the far
 * store with no bank-0 buffer, no STASH and no chunking. Verified byte for
 * byte against the payload through the monitor before this was written.
 *
 * Secondary address 0 means "put it where I say", so the file's first two
 * bytes -- its PRG load address -- are read and discarded. That is why the
 * disk build writes STRINGS.DAT and MUSIC.DAT as PRG with a two-byte header
 * rather than as SEQ; see the d64 rule in c128/Makefile.
 *
 * THE DEVICE NUMBER LIVES IN TWO PLACES NOW, here and in storage.c. That is
 * deliberate: this is not the disk seam. storage.c serves core/storage.h,
 * which must never learn about banks; far memory owns bank 1, so loading it
 * is far memory's business. core/ still knows none of it.
 */
#define DEV      8
#define LFN_FAR  1        /* clear of storage.c's 2 and 15 */

/* Long enough for "0:" plus a 16-character CBM name and a terminator. */
static char fname[20];

/* SETBNK: A = the bank the data goes to, X = the bank the FILENAME is in.
   The name is always here in bank 0; only the data moves. */
static void bank_for_data(unsigned char b) {
    far_n = b;                       /* reuse the parameter global, see above */
    __asm__ volatile(
        "        lda far_n\n"
        "        ldx #0\n"
        "        jsr $ff68\n"
        ::: "a", "x", "memory");
}

uint16_t far_load(const char *name) {
    uint16_t base = far_len;
    uint16_t dest = (uint16_t)(FAR_BASE + base);
    uint16_t end;
    uint8_t i = 0, j = 0;

    if (base >= (uint16_t)(FAR_LIMIT - FAR_BASE)) return FAR_NONE;

    /* "0:" selects drive 0 of the unit. A 1541 has only one drive but wants
       the prefix anyway, and a 1571 or an SD2IEC needs it. */
    fname[i++] = '0'; fname[i++] = ':';
    while (name[j] && i < (uint8_t)(sizeof fname - 1)) fname[i++] = name[j++];
    fname[i] = '\0';

    bank_for_data(FAR_BANK);
    cbm_k_setlfs(LFN_FAR, DEV, 0);
    cbm_k_setnam(fname);
    end = (uint16_t)(uintptr_t)cbm_k_load(0, (void *)dest);
    bank_for_data(0);                /* back to bank 0 before anything else */

    /* cbm_k_load returns one past the last byte, or a KERNAL ERROR CODE --
       a small number, never a plausible address in the store. A file that
       loaded nothing is a failure too, which is what the missing-MUSIC.DAT
       case relies on to play silently rather than crash. */
    if (end <= dest || end > FAR_LIMIT) return FAR_NONE;

    far_len = (uint16_t)(end - FAR_BASE);
    return base;
}

uint16_t far_size(void) { return far_len; }

void far_read(uint16_t off, void *dst, uint8_t len) {
    far_move((uint16_t)(FAR_BASE + off), (unsigned char *)dst, len);
}
