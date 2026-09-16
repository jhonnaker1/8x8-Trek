/* PROBE 2: bankfill(), alone, with no loop around it.
 *
 * Probe 1 drew bands fourteen pages wide where seven were asked for, and every
 * other byte inside a page was left at zero. Two candidate explanations fit
 * that (a 16-bit register inherited from the ROM; the C loops around the
 * primitive) and forcing emulation mode changed NOTHING, byte for byte -- so
 * the mode is ruled out and the primitive itself has never been tested on its
 * own. This is that test: one call, one page, one known value.
 */
#define ASMVAR __attribute__((used, retain))
ASMVAR __attribute__((section(".zp.bss"))) unsigned char shrp[3];
ASMVAR unsigned char gs_hi, gs_val, gs_pages;

__attribute__((naked, used, retain, section(".init.040")))
void gs_emul(void) { __asm__ volatile("sec\n\txce"); }
__attribute__((naked, used, retain, section(".init.050")))
void gs_setdb(void) { __asm__ volatile("phk\n\tplb"); }

static void bankfill(void)
{
    __asm__ volatile(
        "lda #$00\n\t"
        "sta shrp+0\n\t"
        "lda gs_hi\n\t"
        "sta shrp+1\n\t"
        "lda #$e1\n\t"
        "sta shrp+2\n\t"
        "ldx gs_pages\n\t"
        "1:\n\t"
        "ldy #$00\n\t"
        "lda gs_val\n\t"
        "2:\n\t"
        "sta [shrp],y\n\t"
        "iny\n\t"
        "bne 2b\n\t"
        "inc shrp+1\n\t"
        "dex\n\t"
        "bne 1b\n\t"
        : : : "a", "x", "y", "memory");
}

int main(void)
{
    gs_hi = 0x30; gs_val = 0xA5; gs_pages = 1;
    bankfill();
    for (;;) ;
    return 0;
}
