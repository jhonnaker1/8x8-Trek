/* PROBE 7: WHERE can far memory live?
 *
 * Banks $01, $E0 and $E1 are real and writable -- measured. That is not the
 * same as "safe to put the string pool in", because on this machine memory in
 * those banks has other tenants:
 *
 *   SHADOWING ($C035) MIRRORS BANK $00 AND $01 INTO $E0 AND $E1. Which ranges
 *   depends on bits nobody here has set, and the SHR bit covers $2000..$9FFF
 *   of bank $01 -- so a string pool there could be overwritten by the act of
 *   drawing, or could overwrite the screen.
 *
 *   THE LANGUAGE CARD SWITCHES APPLY TO BANK $01 TOO, so $01/D000..$FFFF is
 *   whatever the soft switches last said.
 *
 *   AND $0400..$07FF AND $2000..$5FFF ARE THE TEXT AND HIRES PAGES in every
 *   bank that has them.
 *
 * Reasoning about which combination is live on this machine is exactly the
 * kind of thing that produces a confident wrong answer. So: fill eight
 * candidate regions with a pattern keyed to their own address, DRAW A WHOLE
 * SCREEN through the real video driver, and then check what survived. A region
 * the picture disturbs is not far memory, whatever the documentation says.
 */
#include "../../c128/src/vdc.h"
#include "../../core/ega.h"

#define ASMVAR __attribute__((used, retain))

ASMVAR __attribute__((section(".zp.bss"))) unsigned char fp[3];
ASMVAR unsigned char f_bank, f_lo, f_hi, f_val, f_pages;
ASMVAR unsigned char result[16];
ASMVAR unsigned char gs_done;

__attribute__((naked, used, retain, section(".init.040")))
void gs_emul(void) { __asm__ volatile("sec\n\txce"); }
__attribute__((naked, used, retain, section(".init.050")))
void gs_setdb(void) { __asm__ volatile("phk\n\tplb"); }

/* Fill f_pages pages from f_bank/f_hi:00, writing the PAGE NUMBER in every
   byte -- so a region that comes back holding another region's value says so,
   and a region that comes back holding zero says something different again. */
static void farfill(void)
{
    __asm__ volatile(
        "lda #$00\n\t"   "sta fp+0\n\t"
        "lda f_hi\n\t"   "sta fp+1\n\t"
        "lda f_bank\n\t" "sta fp+2\n\t"
        "ldx f_pages\n\t"
        "1:\n\t" "ldy #$00\n\t" "lda fp+1\n\t"
        "2:\n\t" "sta [fp],y\n\t" "iny\n\t" "bne 2b\n\t"
        "inc fp+1\n\t" "dex\n\t" "bne 1b\n\t"
        : : : "a", "x", "y", "memory");
}

/* Count how many of f_pages pages still hold their own page number. */
static unsigned char farcheck(void)
{
    __asm__ volatile(
        "lda #$00\n\t"   "sta fp+0\n\t"
        "lda f_hi\n\t"   "sta fp+1\n\t"
        "lda f_bank\n\t" "sta fp+2\n\t"
        "ldx f_pages\n\t"
        "stz f_val\n\t"
        "1:\n\t" "ldy #$00\n\t"
        "2:\n\t" "lda [fp],y\n\t" "cmp fp+1\n\t" "bne 3f\n\t"
        "iny\n\t" "bne 2b\n\t"
        "inc f_val\n\t"          /* the whole page held its own number */
        "3:\n\t"
        "inc fp+1\n\t" "dex\n\t" "bne 1b\n\t"
        : : : "a", "x", "y", "memory");
    return f_val;
}

/* Eight candidates: bank, first page, how many pages. Sixteen pages each --
   4K, which is a realistic unit and small enough that eight of them fit in
   the report. */
static const unsigned char cand[8][2] = {
    { 0x01, 0x10 },   /* $01/1000 -- below the hires page */
    { 0x01, 0x60 },   /* $01/6000 -- inside SHR's shadow range */
    { 0x01, 0xA0 },   /* $01/A000 -- above it, below the language card */
    { 0x01, 0xE0 },   /* $01/E000 -- under the aux language card */
    { 0xE0, 0x60 },   /* $E0/6000 */
    { 0xE0, 0xA0 },   /* $E0/A000 */
    { 0xE1, 0xA0 },   /* $E1/A000 -- above SHR's palettes */
    { 0xE1, 0xB0 },   /* $E1/B000 */
};

int main(void)
{
    unsigned char i, x, y;

    for (i = 0; i < 16; i++) result[i] = 0xEE;

    /* 1. Fill every candidate. */
    for (i = 0; i < 8; i++) {
        f_bank = cand[i][0]; f_hi = cand[i][1]; f_pages = 16;
        farfill();
    }

    /* 2. DRAW A WHOLE SCREEN, through the real driver, the way the game will.
       vdc_init turns SHR on and clears; then fill every one of the 1,000
       cells, because a shadow that only bites while the beam is somewhere
       particular will not show in a single glyph. */
    vdc_init();
    for (y = 0; y < 25; y++)
        for (x = 0; x < 40; x++)
            scr_put(x, y, (unsigned char)(33 + ((x + y) & 31)),
                    (unsigned char)(1 + ((x + y) & 14)));

    /* 3. Check what survived. */
    for (i = 0; i < 8; i++) {
        f_bank = cand[i][0]; f_hi = cand[i][1]; f_pages = 16;
        result[i] = farcheck();
    }

    gs_done = 0x5A;
    for (;;) ;
    return 0;
}
