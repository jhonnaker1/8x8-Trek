#include <string.h>

#include "../../core/farmem.h"
#include "../../core/storage.h"

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
 *               -> A = the byte.
 * $FF77 INDSTA: A = the byte, X = bank, Y = index, and $02B9 (STAVEC) holds
 *               the zero page address of the pointer.
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
#define STAVEC     0x02B9
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

/* THE WHOLE TRANSFER IS ONE ASSEMBLY LOOP.
 *
 * The first version wrapped FETCH and STASH in C helpers and let the compiler
 * write the loop. That cost 1,904 bytes -- more than the 1,082 of music data
 * it was displacing, so the seam lost space instead of freeing it. Every asm
 * block clobbers "memory", the compiler spills its imaginary registers around
 * each one, and doing that per byte is ruinous.
 *
 * One block, looping in assembly, pays for itself. Parameters come through
 * fixed globals because llvm-mos may place a local anywhere.
 *
 * $FF74 INDFET: A = zero page address of a pointer, X = bank, Y = index -> A
 * $FF77 INDSTA: A = data, X = bank, Y = index, $02B9 holds the ZP address
 */
static volatile unsigned char far_lo, far_hi, far_n, far_wr;

/* The RAM side of the transfer is reached with (zp),y, which REQUIRES a zero
   page pointer -- a .bss pointer gives "relocation R_MOS_ADDR8 out of range".
   $FD/$FE is free on the C128 and sits outside llvm-mos's imaginary registers
   at $0A..$8F, the same reasoning as $FB/$FC for the far side. */
#define ZPRAM 0xFD

static void far_move(uint16_t addr, unsigned char *ram, uint8_t len, uint8_t writing) {
    if (len == 0) return;
    far_lo = (unsigned char)(addr & 0xFF);
    far_hi = (unsigned char)(addr >> 8);
    *(volatile unsigned char *)ZPRAM       = (unsigned char)((uint16_t)ram & 0xFF);
    *(volatile unsigned char *)(ZPRAM + 1) = (unsigned char)((uint16_t)ram >> 8);
    far_n = len;
    far_wr = writing;
    *(volatile unsigned char *)STAVEC = ZPPTR;

    __asm__ volatile(
        "        lda far_lo\n"
        "        sta $fb\n"
        "        lda far_hi\n"
        "        sta $fc\n"
        "        ldy #0\n"
        "1:      lda far_wr\n"
        "        beq 2f\n"
        "        tya\n"
        "        tax\n"                 /* X = our RAM index, saved */
        "        lda ($fd),y\n"
        "        pha\n"
        "        ldx #1\n"
        "        pla\n"
        "        jsr $ff77\n"           /* STASH: A=data X=bank Y=index */
        "        jmp 3f\n"
        "2:      ldx #1\n"
        "        lda #$fb\n"
        "        jsr $ff74\n"           /* FETCH -> A */
        "        sta ($fd),y\n"
        "3:      iny\n"
        "        cpy far_n\n"
        "        bne 1b\n"
        ::: "a", "x", "y", "memory");
}

/* One chunk at a time, so the RAM cost is the chunk and not the file. Kept to
   a size that never straddles a page from a page-aligned base. */
#define CHUNK 64
static unsigned char chunk[CHUNK];

uint16_t far_load(const char *name) {
    uint16_t got, base = far_len, pos = far_len;

    /* Appends. See the note in core/farmem.h about the second tenant. */
    if (plat_open(name) != STOR_OK) return FAR_NONE;

    for (;;) {
        got = plat_read(chunk, CHUNK);
        if (got == 0) break;
        /* No 32-bit arithmetic -- the core forbids `long`. `pos` cannot pass
           the limit because this runs before every append. */
        if (pos > (uint16_t)(FAR_LIMIT - FAR_BASE) - got) {
            plat_close(); return FAR_NONE;
        }
        far_move((uint16_t)(FAR_BASE + pos), chunk, (uint8_t)got, 1);
        pos = (uint16_t)(pos + got);
    }
    plat_close();

    if (pos == base) return FAR_NONE;      /* the file was empty */
    far_len = pos;
    return base;
}

uint16_t far_size(void) { return far_len; }

void far_read(uint16_t off, void *dst, uint8_t len) {
    far_move((uint16_t)(FAR_BASE + off), (unsigned char *)dst, len, 0);
}
