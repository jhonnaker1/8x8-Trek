/* PROBE 3: write the PAGE NUMBER into every page of SHR, then read it back.
 *
 * Probe 1's own log says it targeted pages $20..$8F, once each, in order --
 * 112 calls, and bankfill() is exact when tested alone. The screen memory
 * nonetheless reads back as nine values fourteen pages wide. The program and
 * the memory disagree, so one of the two instruments is wrong, and no amount
 * of staring at the loop can say which.
 *
 * This removes every other variable. No palette, no bands, no arithmetic: page
 * $20+n gets the byte $20+n, all 125 of them. If the readback is the identity
 * the memory model is fine and the fault is in probe 1; if it is not, the
 * readback is telling us what the machine actually does with bank $E1.
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
        "lda #$00\n\t"       "sta shrp+0\n\t"
        "lda gs_hi\n\t"      "sta shrp+1\n\t"
        "lda #$e1\n\t"       "sta shrp+2\n\t"
        "ldx gs_pages\n\t"
        "1:\n\t" "ldy #$00\n\t" "lda gs_val\n\t"
        "2:\n\t" "sta [shrp],y\n\t" "iny\n\t" "bne 2b\n\t"
        "inc shrp+1\n\t" "dex\n\t" "bne 1b\n\t"
        : : : "a", "x", "y", "memory");
}

/* THE ONE VARIABLE. Probe 1 wrote 112 pages and its own log proves it wrote
   the right value to the right page every time -- yet the readback shows
   fourteen-page runs of nine values, which is exactly "read page $20+k
   returns written page $20+k/2". The only thing probe 1 does that probe 3
   does not is TURN SHR ON. So build this probe both ways and change nothing
   else. */
int main(void)
{
    unsigned char p;
    for (p = 0x20; p < 0x9D; p++) {
        gs_hi = p; gs_val = p; gs_pages = 1;
        bankfill();
    }
#ifdef GS_SHR_ON
    *(volatile unsigned char *)0xC029 = 0xC1;
#endif
    for (;;) ;
    return 0;
}
