/* PROBE 5: what memory does this machine actually have, and which of it can
 * hold the parts of the game that are running out of room?
 *
 * The question is NOT "how many kilobytes". It is which of them the port can
 * use, and for what -- and those are three different answers:
 *
 *   BANKS $02.. are STORAGE ONLY. llvm-mos emits 16-bit code: every JSR, every
 *   pointer and the whole soft stack are 16 bits, and it never emits JSL or
 *   RTL. So code cannot execute outside the bank it was linked for, however
 *   many banks exist. Another bank can hold DATA reached by long addressing --
 *   which this port has already proved works, because that is how it draws.
 *
 *   THE LANGUAGE CARD IS DIFFERENT, and that is why this probe exists.
 *   $D000..$FFFF in BANK 0 is RAM once the soft switches say so, and code
 *   there runs with ordinary 16-bit calls. It is the only memory on this
 *   machine that adds EXECUTABLE space rather than storage.
 *
 * So: walk the banks and find out which are real and which alias, then test
 * whether the language card gives writable, readable RAM in bank 0 -- both
 * $D000 banks and $E000..$FFFF.
 */
#define ASMVAR __attribute__((used, retain))

ASMVAR __attribute__((section(".zp.bss"))) unsigned char gs_p[3];
ASMVAR unsigned char gs_bank, gs_val, gs_lo, gs_hi;

/* The report, placed by the linker rather than at an address chosen by hand. */
ASMVAR unsigned char bank_ok[8];      /* $01, $E0, $E1 -- see main() */
ASMVAR unsigned char lc_result[8];
ASMVAR unsigned char gs_done;

__attribute__((naked, used, retain, section(".init.040")))
void gs_emul(void) { __asm__ volatile("sec\n\txce"); }
__attribute__((naked, used, retain, section(".init.050")))
void gs_setdb(void) { __asm__ volatile("phk\n\tplb"); }

static void bank_poke(void)
{
    __asm__ volatile(
        "lda gs_lo\n\t"   "sta gs_p+0\n\t"
        "lda gs_hi\n\t"   "sta gs_p+1\n\t"
        "lda gs_bank\n\t" "sta gs_p+2\n\t"
        "ldy #$00\n\t"
        "lda gs_val\n\t"  "sta [gs_p],y\n\t"
        : : : "a", "y", "memory");
}

static unsigned char bank_peek(void)
{
    __asm__ volatile(
        "lda gs_lo\n\t"   "sta gs_p+0\n\t"
        "lda gs_hi\n\t"   "sta gs_p+1\n\t"
        "lda gs_bank\n\t" "sta gs_p+2\n\t"
        "ldy #$00\n\t"
        "lda [gs_p],y\n\t" "sta gs_val\n\t"
        : : : "a", "y", "memory");
    return gs_val;
}

/* The language card soft switches. READ them; writing has different meaning.
   $C083 read TWICE selects: RAM readable, RAM writable, $D000 bank 2.
   $C08B read twice is the same with $D000 bank 1. */
#define LC_RD(a) ((void)*(volatile unsigned char *)(a))

int main(void)
{
    unsigned char i;

    __asm__ volatile("sei");

    /* ---- 1. THE BANKS A STOCK MACHINE HAS, AND ONLY THOSE.

       WALKING ALL 224 BANKS WAS TRIED THREE TIMES AND IS ABANDONED. The first
       version wrote the bank number everywhere and read it back: it reported
       222 banks -- 14 MB -- and reported THE SAME 222 at `-ramsize 1M` and at
       `-ramsize 8M`. An answer that does not move when the thing measured
       moves by a factor of eight is not an answer; a store to missing memory
       and the load after it can both come off the CPU's own data latch, so a
       phantom bank returns exactly what was written to it.

       The second version put a different value on the bus between the write
       and the read, which fixed the latch and made 1M differ from 2M -- and
       still could not tell 2M from 8M, and reported banks answering
       SCATTERED rather than in a run, which is not how memory is fitted.

       THE THIRD ATTEMPT WAS THE ONE NOT TO MAKE. Three instruments in a row
       for a question whose answer changes nothing: no number of banks makes
       code executable outside bank 0, because llvm-mos emits 16-bit calls and
       never JSL or RTL. Extra banks are STORAGE, and the storage this port
       could want fits in what a machine cannot be sold without.

       AND THE RIG CANNOT SEE THE MINIMUM MACHINE ANYWAY: MAME's smallest
       apple2gs is 1M. A stock 256K IIgs is not testable here, so relying on
       any bank above $01 would be relying on something this project has no
       way to check. That is the finding, and it is a constraint rather than a
       measurement.

       So test the three that every IIgs has: $01 is aux RAM, $E0 and $E1 are
       the Mega II's, and $E1 is where the screen already lives. */
    {
        static const unsigned char want[3] = { 0x01, 0xE0, 0xE1 };
        for (i = 0; i < 3; i++) {
            gs_lo = 0x00; gs_hi = 0x40;        /* clear of screens and vectors */
            gs_bank = want[i]; gs_val = 0xA5; bank_poke();
            gs_bank = 0xE1;    gs_val = 0x5A;  /* a bank known real: the screen
                                                  is drawn through it. Also
                                                  puts a different value on the
                                                  bus, so a latch cannot
                                                  impersonate RAM. */
            gs_lo = 0x00; gs_hi = 0x50;        /* ...but not on top of $E1/4000 */
            bank_poke();
            gs_lo = 0x00; gs_hi = 0x40;
            gs_bank = want[i];
            bank_ok[i] = bank_peek();
        }
    }

    /* ---- 2. THE LANGUAGE CARD, which is the only memory that adds
       EXECUTABLE space. Read the switch twice, write a signature, read it
       back, and do it for both $D000 banks and for $E000..$FFFF. */
    for (i = 0; i < 8; i++) lc_result[i] = 0;

    LC_RD(0xC083); LC_RD(0xC083);                 /* RAM r/w, $D000 bank 2 */
    *(volatile unsigned char *)0xD000 = 0xA1;
    *(volatile unsigned char *)0xE000 = 0xA2;
    *(volatile unsigned char *)0xFFF0 = 0xA3;
    lc_result[0] = *(volatile unsigned char *)0xD000;
    lc_result[1] = *(volatile unsigned char *)0xE000;
    lc_result[2] = *(volatile unsigned char *)0xFFF0;

    LC_RD(0xC08B); LC_RD(0xC08B);                 /* RAM r/w, $D000 bank 1 */
    *(volatile unsigned char *)0xD000 = 0xB1;
    lc_result[3] = *(volatile unsigned char *)0xD000;

    LC_RD(0xC083); LC_RD(0xC083);                 /* back to bank 2 */
    /* IF THE TWO $D000 BANKS ARE REALLY SEPARATE, this still reads $A1 --
       bank 1's $B1 went somewhere else. If they are the same memory it reads
       $B1, and the port has 4K less than it thought. */
    lc_result[4] = *(volatile unsigned char *)0xD000;
    lc_result[5] = *(volatile unsigned char *)0xE000;

    /* And put the ROM back, so whatever runs next is not surprised. */
    LC_RD(0xC082);
    lc_result[6] = *(volatile unsigned char *)0xE000;   /* should be ROM now */

    gs_done = 0x5A;
    for (;;) ;
    return 0;
}
