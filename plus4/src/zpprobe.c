/* DOES THE PLUS/4 KERNAL CLOBBER llvm-mos's IMAGINARY REGISTERS?
 *
 * plus4.ld carries `__basic_zp_start = 0x0002` verbatim from c64.ld, so
 * __rc0..__rc31 live at $0002..$0021 and every C spill goes through them.
 * That range is free on a C64 with BASIC paged out. Nobody checked whether it
 * is free on a PLUS/4 with the KERNAL being called into.
 *
 * ALL OF IT IS ASSEMBLY AND THAT IS FORCED. The test fills the very registers
 * the compiler uses for temporaries, so a single C statement between the fill
 * and the read would corrupt the measurement with its own spills. Absolute
 * addressing only, no soft stack, no C locals.
 *
 * zp_after[] holds $02..$21 as the KERNAL left them. Anything that is not the
 * $A5 fill is a byte the KERNAL wrote, and every one of those is a C temporary
 * this port would lose across a disk call.
 */
#include <stdint.h>

__attribute__((used, section(".lowbss"))) unsigned char zp_after[32];
__attribute__((used, section(".lowbss"))) unsigned char zp_done;

__attribute__((noinline, section(".lowtext")))
static void probe(void)
{
    __asm__ volatile (
        "sei\n"
        "sta $ff3f\n"              /* RAM in, the state the game runs in    */
        /* Fill $02..$21 with $A5. X counts 32 down to 1; $01,x reaches
           $02..$21. */
        "ldx #32\n"
        "lda #$a5\n"
        "1: sta $01,x\n"
        "dex\n"
        "bne 1b\n"
        /* ONE KERNAL CALL, banked exactly as the port's wrappers do it. */
        "sta $ff3e\n"
        "lda #3\n"
        "ldx #8\n"
        "ldy #0\n"
        "jsr $ffba\n"              /* SETLFS -- the simplest one there is   */
        "sta $ff3f\n"
        /* And read them straight back out, before anything else can run. */
        "ldx #32\n"
        "2: lda $01,x\n"
        "sta zp_after-1,x\n"
        "dex\n"
        "bne 2b\n"
        "lda #$5a\n"
        "sta zp_done\n"
        ::: "a", "x", "y", "p", "memory");
}

int main(void)
{
    probe();
    for (;;) { }
    return 0;
}
