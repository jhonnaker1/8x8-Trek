/* Far memory for the IIgs: the string pool and the message log, in bank $01.
 *
 * WHERE, AND IT WAS MEASURED RATHER THAN REASONED ABOUT. Banks $01, $E0 and
 * $E1 are all real and writable -- but "writable" is not "safe to put the
 * string pool in" on a machine where $C035 shadows bank $00 and $01 into $E0
 * and $E1, the language card switches apply to bank $01 as well as bank $00,
 * and $0400..$07FF and $2000..$5FFF are text and hires pages in every bank
 * that has them. Working out which combination is live is exactly the kind of
 * question that produces a confident wrong answer.
 *
 * So src/gsfarp.c filled eight candidate regions, DREW A WHOLE SCREEN through
 * the real driver, and counted what survived:
 *
 *     $01/1000  16/16     $E0/6000  16/16
 *     $01/6000  16/16     $E0/A000  16/16
 *     $01/A000  16/16     $E1/A000  16/16
 *     $01/E000   0/16     $E1/B000  16/16
 *
 * Seven of eight untouched by drawing. The one that fails is under the AUX
 * LANGUAGE CARD, which is the one the documentation would have warned about
 * and the one a guess would most likely have picked, since it is the biggest
 * contiguous-looking run in the bank.
 *
 * $01/6000..$01/BFFF is therefore the home: 24,576 bytes, measured clean, and
 * it stops at $C000 because that is I/O in bank $01 as much as in bank $00.
 * STRINGS.DAT and MUSIC.DAT are about 7,900 bytes together, so this port has
 * roughly three times what it needs -- against the C64's far store, which had
 * 282 bytes left over.
 *
 * NOTHING EXECUTES HERE. llvm-mos emits 16-bit calls and never jsl or rtl, so
 * another bank can only ever hold DATA; see README.md, "Will more RAM help?".
 */
#include <stdint.h>

#include "../../core/storage.h"

#define ASMVAR __attribute__((used, retain))

#define FAR_BANK   0x01
#define LOG_BASE   0x6000        /* 2,048 bytes, ui.c's 32 entries of 64 */
#define POOL_BASE  0x6800
#define POOL_LIMIT 0xC000        /* $C000 is I/O in bank $01 too */
#define FAR_NONE   0xFFFF

/* ui.c addresses the log as though it were VDC RAM at $1000. */
#define LOG_ORIGIN 0x1000

ASMVAR __attribute__((section(".zp.bss"))) unsigned char fm_p[3];
ASMVAR unsigned char fm_lo, fm_hi, fm_val, fm_n;

static uint16_t far_len;          /* bytes used from POOL_BASE */
static unsigned int log_ptr;      /* bank $01 offset the log stream is at */

/* One byte to $01/fm_hi:fm_lo. */
static void far_poke(unsigned int off, unsigned char v)
{
    fm_lo = (unsigned char)(off & 0xFF);
    fm_hi = (unsigned char)(off >> 8);
    fm_val = v;
    __asm__ volatile(
        "lda fm_lo\n\t"  "sta fm_p+0\n\t"
        "lda fm_hi\n\t"  "sta fm_p+1\n\t"
        "lda #$01\n\t"   "sta fm_p+2\n\t"
        "ldy #$00\n\t"
        "lda fm_val\n\t" "sta [fm_p],y\n\t"
        : : : "a", "y", "memory");
}

static unsigned char far_peek(unsigned int off)
{
    fm_lo = (unsigned char)(off & 0xFF);
    fm_hi = (unsigned char)(off >> 8);
    __asm__ volatile(
        "lda fm_lo\n\t"  "sta fm_p+0\n\t"
        "lda fm_hi\n\t"  "sta fm_p+1\n\t"
        "lda #$01\n\t"   "sta fm_p+2\n\t"
        "ldy #$00\n\t"
        "lda [fm_p],y\n\t" "sta fm_val\n\t"
        : : : "a", "y", "memory");
    return fm_val;
}

/* ---------------------------------------------------------- the string pool

   THE FILE IS STREAMED THROUGH A SMALL BANK-0 BUFFER, because the block
   driver's buffer address is sixteen bits and lands in the bank the CPU is
   executing in -- it cannot be pointed at bank $01. So each chunk is read
   into `stage` and copied across with long addressing. 128 bytes is chosen
   against BSS rather than speed: 7,900 bytes is 62 chunks and the load
   happens once.

   far_load APPENDS, and that is the contract strpool.c relies on -- STRINGS
   then MUSIC, each getting back the offset where its own data starts. */
static unsigned char stage[128];

uint16_t far_load(const char *name)
{
    uint16_t base = far_len;
    unsigned int dest = (unsigned int)(POOL_BASE + base);
    uint16_t n;
    unsigned char i;

    if (plat_open(name) != STOR_OK) return FAR_NONE;

    for (;;) {
        n = plat_read(stage, sizeof stage);
        if (!n) break;
        if (dest + n > POOL_LIMIT) { plat_close(); return FAR_NONE; }
        for (i = 0; i < (unsigned char)n; i++)
            far_poke(dest + i, stage[i]);
        dest += n;
    }
    plat_close();

    /* LOADING NOTHING IS A FAILURE TOO. The C64 port's note: a missing
       MUSIC.DAT that returns an offset rather than FAR_NONE leaves the caller
       reading whatever was there. */
    if (dest <= (unsigned int)(POOL_BASE + base)) return FAR_NONE;

    far_len = (uint16_t)(dest - POOL_BASE);
    return base;
}

void far_read(uint16_t off, void *dst, uint8_t len)
{
    unsigned char *p = (unsigned char *)dst;
    unsigned char i;
    for (i = 0; i < len; i++)
        p[i] = far_peek(POOL_BASE + off + i);
}

/* ---------------------------------------------------------- the message log

   ui.c keeps a 32-entry log outside the program through three functions whose
   names are the C128's, because that is where the seam was first cut. Every
   port implements them against whatever spare memory it has: the C128 uses
   real VDC RAM, the C64 a plain array because it had the room. THIS PORT DOES
   NOT have the room -- the array cost 2,048 bytes of bank 0 out of 1,012
   remaining -- so it goes to bank $01 with the pool. */
void vdc_set_address(unsigned int addr)
{
    log_ptr = LOG_BASE + (addr - LOG_ORIGIN);
}

void vdc_data_write(unsigned char value)
{
    far_poke(log_ptr++, value);
}

unsigned char vdc_data_read(void)
{
    return far_peek(log_ptr++);
}
