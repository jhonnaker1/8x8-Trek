/* WHERE DOES IT DIE? -- markers on both sides of the bank switch.
 *
 * The bisection said `sta $ff3f` is the instruction: STEP 0..3 all reach
 * main() and STEP 4 does not. That is not yet a cause. "The bank switch kills
 * it" and "the bank switch is fine and what runs AFTER it dies" look
 * identical from one marker written by main.
 *
 *   M[0] = $A1   the hook started
 *   M[1] = $A2   the hook finished -- written AFTER $FF3F, with RAM in
 *   M[2] = $5A   main() was reached
 *
 * M is in .lowbss, which the linker places below $8000 -- so it is never in
 * the hidden half and a read of it means the same thing either way.
 */
#ifndef STEP
#define STEP 4
#endif

__attribute__((used, retain)) void p4_stray_irq(void) { __asm__ volatile("rti"); }

__attribute__((used, retain, section(".lowbss"))) volatile unsigned char M[4];

__attribute__((used, retain, naked, section(".init.010")))
void p4_ram_in(void)
{
    __asm__ volatile (
        "lda #$a1\n"
        "sta M+0\n"
        "sei\n"
#if STEP >= 2
        "lda #0\n"
        "sta $ff0a\n"
#endif
#if STEP >= 3
        "lda #<p4_stray_irq\n"
        "sta $fffe\n"
        "sta $fffa\n"
        "lda #>p4_stray_irq\n"
        "sta $ffff\n"
        "sta $fffb\n"
#endif
#if STEP >= 4
        "sta $ff3f\n"
#endif
        "lda #$a2\n"
        "sta M+1\n"
        ::: "a", "memory");
}

int main(void)
{
    M[2] = 0x5A;
    *(volatile unsigned char *)0xFF19 = 0x72;
    for (;;) { }
    return 0;
}
