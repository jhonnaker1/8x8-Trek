#include <cbm.h>

/* PUT RAM UNDER THE ROM BEFORE main() RUNS, AND PUT THE ROM BACK ON THE WAY
 * OUT. This is the Plus/4's `unmap-basic.o`, and its absence is the single
 * reason the port did not boot.
 *
 * llvm-mos's C64 platform ships unmap-basic.o: four instructions in
 * `.init.010` that set $01 = $3E so BASIC's 8K becomes RAM, and a `.fini.990`
 * that puts it back. There is no plus4 platform, so nothing supplied the
 * equivalent -- and the equivalent is not $01, it is a store to $FF3F.
 *
 * WHAT IT COST TO FIND. The program linked, fitted, verified and loaded, and
 * then answered `?SYNTAX ERROR` from BASIC. Setting PC to the C runtime entry
 * through the monitor -- taking BASIC out of the picture -- showed the CPU
 * landing at $D90A within a quarter of a second: inside the ROM, in BASIC's
 * own idle loop. With the ROM mapped, .text above $8000 IS the ROM, so main()
 * called into $8000+ and arrived in BASIC.
 *
 * THE ADDRESS IS THE WRITE. $FF3F selects RAM and $FF3E selects ROM; what is
 * stored is irrelevant, which is why these look like they are writing nothing
 * useful.
 *
 * .init.010 RUNS BEFORE .init.200's bss clear, and that ordering matters: bss
 * is at $A1CE, under the ROM, so zeroing it with the ROM still mapped would
 * write through to RAM the program cannot then read back consistently. Doing
 * the bank first makes every later init see one memory map.
 */

__attribute__((used, section(".init.010")))
void p4_ram_in(void)
{
    /* THE MACHINE HAS A DESIGNED ANSWER FOR THIS AND I DID NOT KNOW IT.
       Banking RAM in takes the 6502's vectors with it: the HI ROM is
       $C000..$FBFF **and $FF40..$FFFF**, so $FFFA..$FFFF become RAM. But
       $FC00..$FCFF is ALWAYS KERNAL ROM whatever is banked -- a permanent
       page put there for exactly this -- and it holds the interrupt entry at
       $FCB3, which saves the bank, does the KERNAL's IRQ work, and returns
       through $FCBE to restore it from the zero-page byte at $FB.

       So the RAM vectors are pointed at $FCB3 and interrupts KEEP RUNNING.
       That is not a nicety: input.c reads the keyboard with GETIN, which is
       fed by the KERNAL's IRQ-driven buffer, so an SEI here would have left
       the finished port unable to read a key. The first version did SEI and
       cleared TED's $FF0A, which was me defending against a problem the
       machine had already solved.

       Writes reach RAM through the ROM (src/wrprobe.c), so these land even
       though the ROM is still mapped as they execute. */
    *(volatile unsigned char *)0xFFFE = 0xB3;   /* IRQ/BRK -> $FCB3 */
    *(volatile unsigned char *)0xFFFF = 0xFC;
    *(volatile unsigned char *)0xFFFA = 0xB3;   /* NMI, which SEI never masked */
    *(volatile unsigned char *)0xFFFB = 0xFC;
    *(volatile unsigned char *)0xFF3F = 0;      /* and now the RAM is ours */
}

/* The way out, for a program that returns rather than resetting. plat_exit()
   banks in for itself before taking the reset vector, because it cannot rely
   on reaching here. */
__attribute__((used, section(".fini.990")))
void p4_rom_back(void)
{
    *(volatile unsigned char *)0xFF3E = 0;
}

/* ---------------------------------------------------------------------------
 * THE KERNAL WRAPPERS, BANKED. Every one of these OVERRIDES the identically
 * named routine in llvm-mos's commodore libc, which is an archive, so a strong
 * definition here wins the link.
 *
 * WHY THEY HAVE TO EXIST, and it is the difference between this machine and a
 * C64 in one line: on a C64 `$01 = $3E` pages out BASIC and LEAVES THE KERNAL
 * MAPPED, so `jsr $FFD2` works all game and c64mem.c only has to bank for its
 * own far store. On a Plus/4 `$FF3F` removes BOTH ROMS AT ONCE -- so with the
 * program's RAM in, $FFBA is RAM holding whatever the program put there, and
 * `cbm_k_setlfs` jumps into it.
 *
 * MEASURED, NOT REASONED. The game linked, fitted, verified and loaded, and
 * then sat with PC at $FD02 -- inside the I/O window, executing register
 * values as opcodes. Setting PC through the monitor is what showed it; the
 * screen only ever said `?SYNTAX ERROR`.
 *
 * Each is: ROM in, load the registers the KERNAL wants, JSR, RAM back, return
 * the result. In .lowtext, because the caller is above $8000 and must survive
 * being hidden -- the same rule kernal_load_raw follows, and the reason the
 * soft stack is never touched between the two stores.
 */

__attribute__((used, section(".lowbss"))) unsigned char kb_a, kb_x, kb_y, kb_st;

#define BANKED_CALL(vec)                    \
    __asm__ volatile (                      \
        "sta $ff3e\n"                       \
        "lda kb_a\n"                        \
        "ldx kb_x\n"                        \
        "ldy kb_y\n"                        \
        "jsr " vec "\n"                     \
        "sta kb_a\n"                        \
        "stx kb_x\n"                        \
        "lda #0\n"                          \
        "rol\n"                             \
        "sta kb_st\n"                       \
        "sta $ff3f\n"                       \
        ::: "a", "x", "y", "p", "memory")

__attribute__((noinline, section(".lowtext"))) static void k_setlfs(void) { BANKED_CALL("$ffba"); }
__attribute__((noinline, section(".lowtext"))) static void k_setnam(void) { BANKED_CALL("$ffbd"); }
__attribute__((noinline, section(".lowtext"))) static void k_open(void)   { BANKED_CALL("$ffc0"); }
__attribute__((noinline, section(".lowtext"))) static void k_close(void)  { BANKED_CALL("$ffc3"); }
__attribute__((noinline, section(".lowtext"))) static void k_chkin(void)  { BANKED_CALL("$ffc6"); }
__attribute__((noinline, section(".lowtext"))) static void k_ckout(void)  { BANKED_CALL("$ffc9"); }
__attribute__((noinline, section(".lowtext"))) static void k_clrch(void)  { BANKED_CALL("$ffcc"); }
__attribute__((noinline, section(".lowtext"))) static void k_chrin(void)  { BANKED_CALL("$ffcf"); }
__attribute__((noinline, section(".lowtext"))) static void k_chrout(void) { BANKED_CALL("$ffd2"); }
__attribute__((noinline, section(".lowtext"))) static void k_readst(void) { BANKED_CALL("$ffb7"); }
__attribute__((noinline, section(".lowtext"))) static void k_getin(void)  { BANKED_CALL("$ffe4"); }

void cbm_k_setlfs(unsigned char lfn, unsigned char dev, unsigned char sec)
{ kb_a = lfn; kb_x = dev; kb_y = sec; k_setlfs(); }

/* SETNAM WANTS A LENGTH AND A POINTER, not a C string -- the length goes in A
   and the pointer in X/Y. The caller's string may live above $8000, which is
   fine: the KERNAL only reads it during OPEN or LOAD, and those bank the ROM
   in themselves... which would hide it. So SETNAM's pointer must address
   memory the KERNAL can see WITH THE ROM MAPPED -- below $8000. Every name
   this game passes is a string literal in .rodata, which is at $9909. */
void cbm_k_setnam(const char *name)
{
    static __attribute__((section(".lowbss"))) char lowname[20];
    unsigned char n = 0;
    while (name[n] && n < sizeof lowname - 1) { lowname[n] = name[n]; n++; }
    kb_a = n;
    kb_x = (unsigned char)((unsigned int)(unsigned long)lowname & 0xFF);
    kb_y = (unsigned char)((unsigned int)(unsigned long)lowname >> 8);
    k_setnam();
}

unsigned char cbm_k_open(void)   { kb_a = kb_x = kb_y = 0; k_open();  return kb_st ? kb_a : 0; }
void cbm_k_close(unsigned char f){ kb_a = f; k_close(); }
unsigned char cbm_k_chkin(unsigned char f) { kb_a = 0; kb_x = f; kb_y = 0; k_chkin(); return kb_st ? kb_a : 0; }
unsigned char cbm_k_ckout(unsigned char f) { kb_a = 0; kb_x = f; kb_y = 0; k_ckout(); return kb_st ? kb_a : 0; }
void cbm_k_clrch(void)           { k_clrch(); }
unsigned char cbm_k_chrin(void)  { k_chrin();  return kb_a; }
void cbm_k_chrout(unsigned char c){ kb_a = c;  k_chrout(); }
unsigned char cbm_k_readst(void) { k_readst(); return kb_a; }
unsigned char cbm_k_getin(void)  { k_getin();  return kb_a; }
