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
 * .init.260, AND IT USED TO BE .init.010 -- WHICH IS WHY THIS PORT DID NOT
 * BOOT. This file was blamed as a whole for months ("the same hello.c ran
 * without this file and did not run with it"), and a whole file is not a
 * cause. Bisected on the machine, one instruction at a time:
 *
 *     sei                                main() reached
 *     + $FF0A = 0  (TED mask off)        main() reached
 *     + the CPU's IRQ/NMI vectors        main() reached
 *     + $FF3F, RAM in                    main() NEVER REACHED
 *
 * and then, with markers either side of that store, the hook was seen to
 * COMPLETE -- so the bank switch works and something between it and main()
 * dies. The disassembly names it:
 *
 *     .init.250 <shift>:  lda #$0e
 *                         jsr $ffd2      ; BSOUT -- second charset
 *
 * llvm-mos's commodore libc puts its own hook in the init chain, AFTER ours,
 * and it calls the KERNAL. With both ROMs gone `jsr $FFD2` lands in
 * uninitialised RAM. Jamie, watching the emulator, described it better than
 * the marker bytes did: "seemed like the tape drive pressed play" -- garbage
 * executing into $01, whose bit 3 is the cassette motor.
 *
 * cc65's own plus4 crt0.s does the identical `lda #14 / jsr $FFD2`, and does
 * it BEFORE banking. So does this now: the hook moved to .init.260, after
 * shift, and main() is reached.
 *
 * AND THE ORDERING THIS FILE USED TO CLAIM WAS WRONG. It said .init.010 had
 * to run before .init.200's bss clear, "because bss is at $A1CE, under the
 * ROM, so zeroing it with the ROM still mapped would write through to RAM the
 * program cannot then read back consistently." src/vecprobe.c measured that:
 * a write made with the ROM mapped LANDS IN THE RAM BENEATH and reads back
 * correctly once RAM is banked in -- $BE $EF at $FFFE, in the tightest corner
 * of the map. So zero-bss with the ROM in is fine, and the constraint that
 * forced the hook to the front of the chain never existed.
 */

/* Somewhere harmless for an interrupt this program did not ask for. In
   .lowtext because it must be visible whatever is banked. */
__attribute__((used, noinline, section(".lowtext")))
void p4_stray_irq(void) { __asm__ volatile ("rti"); }

/* NAKED, AND THAT IS THE WHOLE REASON THIS FILE DID NOT WORK. The .init.NNN
   sections are CONCATENATED AND FALLEN THROUGH -- llvm-mos's own
   unmap-basic.o is four instructions with NO RTS, and .fini.990 is two. A
   normal C function ends in RTS, so putting one here RETURNED OUT OF THE INIT
   CHAIN and main() was never called. Measured by bisection: the same hello.c
   ran with this file left out and did not run with it linked. */
__attribute__((used, naked, section(".init.260")))
void p4_ram_in(void)
{
    /* ALL ASSEMBLY, because a naked function may contain nothing else -- and
       naked is required because .init.NNN sections are concatenated and
       FALLEN THROUGH. llvm-mos's own unmap-basic.o is four instructions with
       no RTS. A normal C function ends in RTS, which returns out of the init
       chain, and main() is never called. Measured by bisection: the same
       hello.c ran without this file and did not run with it.

       $FCB3 IS NOT USABLE HERE -- the ROM's own code says so. It is
       PHA/TXA/PHA/TYA/PHA, STA $FDD0, JMP $CE00, and the exit at $FCBE is
       LDX $FB / STA $FDD0,X. $FDD0,X picks WHICH ROM is mapped and no value
       of X means RAM, so it is a trampoline for cartridge programs switching
       ROM banks, not for a program running with RAM under the ROM.

       So this program owns the interrupt: TED's enable off, the CPU's flag
       down, and both vectors pointed at an RTI so a stray NMI -- which SEI
       never masks -- lands somewhere harmless. THE COST IS THE KEYBOARD:
       input.c reads through GETIN, which the KERNAL's IRQ fills, so this port
       needs its own keyboard seam. Written down rather than discovered later.

       The vector stores happen with the ROM still mapped and reach the RAM
       beneath it -- writes pass through, which src/wrprobe.c measured. */
    __asm__ volatile (
        "sei\n"
        "lda #0\n"
        "sta $ff0a\n"                  /* TED: no raster interrupt        */
        "lda #<p4_stray_irq\n"
        "sta $fffe\n"
        "sta $fffa\n"
        "lda #>p4_stray_irq\n"
        "sta $ffff\n"
        "sta $fffb\n"
        "sta $ff3f\n"                  /* and now the RAM is ours         */
        ::: "a", "memory");
}

/* The way out, for a program that returns rather than resetting. plat_exit()
   banks in for itself before taking the reset vector, because it cannot rely
   on reaching here. */
__attribute__((used, naked, section(".fini.990")))
void p4_rom_back(void)
{
    __asm__ volatile ("sta $ff3e" ::: "memory");
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

/* LOAD, AND ITS ABSENCE IS WHY THE TITLE OVERLAY NEVER ARRIVED.
 *
 * The eleven wrappers above cover every KERNAL call storage.c makes, and the
 * string pool arrives through p4mem.c's own kernal_load_raw -- so the game
 * reached main(), cleared the screen and had STRINGS.DAT in the far store at
 * $DD00, byte for byte. The one call nobody wrapped is the one the SHARED
 * overlay.c makes: `cbm_k_load(0, __ovl_start)`, which llvm-mos's commodore
 * libc compiles to a bare `jsr $FFD5`.
 *
 * With both ROMs banked out, $FFD5 is uninitialised RAM. Exactly the fault
 * that `.init.250 <shift>` had -- a KERNAL call made with the KERNAL gone --
 * in a file this port does not own and therefore never read.
 *
 * cc65's libsrc/plus4/kload.s is this, in four instructions, in a segment
 * commented "Must go into low memory".
 *
 * IT NEEDS ITS OWN WRAPPER RATHER THAN BANKED_CALL, because LOAD returns the
 * END ADDRESS IN X AND Y and BANKED_CALL preserves only A, X and the carry --
 * that is all the other eleven need, and losing Y here would give overlay.c an
 * end address with a random high byte. Its own guard compares that end against
 * the window, so a lost Y would read as a corrupt overlay rather than as this.
 */
/* BISECTED, because "adding this breaks startup" is not a cause either.
 * P4LOAD picks how much of the wrapper exists:
 *
 *   0   neither -- the baseline that reaches main() and loads STRINGS.DAT
 *   1   k_load defined in .lowtext, never called
 *   2   + cbm_k_load OVERRIDING the libc symbol, but NOT calling k_load
 *   3   + cbm_k_load calling k_load -- the full wrapper
 *
 * 1 separates "a function exists in .lowtext" from "the libc symbol is
 * overridden"; 2 separates the override from the banked call itself.
 */
#ifndef P4LOAD
#define P4LOAD 3
#endif

#if P4LOAD >= 1
/* `used, retain` AND NOT static: with only `noinline` this function VANISHED.
   The image afterwards held exactly ONE `jsr $ffd5` -- p4mem.c's own
   kernal_load_raw -- where it should now hold two, so LTO folded this into
   that one or dropped it outright. The count of a distinctive instruction is
   a cheap thing to check. */
__attribute__((used, retain, noinline, section(".lowtext"))) void k_load(void)
{
    __asm__ volatile (
        "sta $ff3e\n"
        "lda kb_a\n"
        "ldx kb_x\n"
        "ldy kb_y\n"
        "jsr $ffd5\n"
        "sta kb_a\n"
        "stx kb_x\n"
        "sty kb_y\n"
        "lda #0\n"
        "rol\n"
        "sta kb_st\n"
        "sta $ff3f\n"
        ::: "a", "x", "y", "p", "memory");
}
#endif

#if P4LOAD >= 2
void *cbm_k_load(unsigned char flag, void *load_addr)
{
    kb_a = flag;
    kb_x = (unsigned char)((unsigned int)load_addr & 0xFF);
    kb_y = (unsigned char)(((unsigned int)load_addr) >> 8);
#if P4LOAD >= 3
    k_load();
    if (kb_st) return load_addr;
    return (void *)(unsigned int)(kb_x | ((unsigned int)kb_y << 8));
#else
    /* The override WITHOUT the call: every overlay "fails" the way a short
       load does, which overlay.c already handles, and startup is the only
       thing under test. */
    return load_addr;
#endif
}
#endif

#if P4LOAD == 9
/* Thirty-four bytes of NOTHING ANYBODY CAN CALL, in .lowtext, the same size
   k_load takes. .lowtext is a code section, so this is a naked function of
   nops rather than an array -- clang rejects const data there outright. */
__attribute__((used, retain, naked, section(".lowtext")))
void p4_pad(void) { __asm__ volatile(
    "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop"); }
#endif
