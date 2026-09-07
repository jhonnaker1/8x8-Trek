/* PROBE 2: which banking does the C65 DOS actually need to write a file?
 *
 * Probe 1 established that the two shadows are INDEPENDENT and each has its
 * own control:
 *
 *     $01 bit 0 (LORAM)   BASIC ROM over $A000..$BFFF -- where io_buf, the
 *                         hall of fame and the soft stack live
 *     $D030 bit 5 (ROMC)  C65 ROM over $C000..$CFFF   -- the overlay window
 *
 * So there is a config for every combination, and the only question left is
 * which of them the DOS will accept. That decides the size of the job:
 *
 *     C ($3E,$64) works -> BASIC stays out, io_buf/hof/stack stay visible,
 *                          and the write path is a straight port of the C128's
 *     B ($3F,$44) works -> the window survives but the data does not
 *     only A       ($3F,$64) -> both shadowed, and everything needs relocating
 *
 * ONE CONFIG PER RUN, chosen with -DCFG=n, because a config the DOS rejects
 * HANGS -- and a hang inside a loop over configs would be indistinguishable
 * from the next config failing. `stage` says how far it got.
 *
 * THE ANSWER IS THE D81, NOT THE SCREEN. What this prints is a status byte;
 * whether a file actually landed is settled by reading the image back with
 * c1541 afterwards. A KERNAL call can report success and write nothing.
 */
#include <stdint.h>
#include <mega65/conio.h>

#ifndef CFG
#define CFG 1
#endif

/* Spelled out per config rather than stringified: `#` only stringifies a MACRO
   PARAMETER, and MAP() takes none. */
#if CFG == 1
#define MAPASM  "sei\n\tlda #$3f\n\tsta $01\n\tlda #$64\n\tsta $d030"
#define CFGNAME "A  $01=3F $D030=64  BASIC in, ROMC in"
#elif CFG == 2
#define MAPASM  "sei\n\tlda #$3f\n\tsta $01\n\tlda #$44\n\tsta $d030"
#define CFGNAME "B  $01=3F $D030=44  BASIC in, ROMC OUT"
#else
#define MAPASM  "sei\n\tlda #$3e\n\tsta $01\n\tlda #$64\n\tsta $d030"
#define CFGNAME "C  $01=3E $D030=64  BASIC OUT, ROMC in"
#endif

/* Every one of these lives just past this probe's code, around $2xxx, which
   probe 1 showed stays RAM under all three configs. */
static const unsigned char fname[] = "PROBE,S,W";
/* volatile: the compiler cannot see the inline asm read these, and without it
   -Wunused-but-set-global rejects the build. */
static volatile unsigned char nlo, nhi;
static volatile unsigned char stage;
/* NON-STATIC AND VOLATILE. These are written only by the inline asm, so LTO
   saw no writer, folded them to zero and dropped the symbols -- the link then
   failed on the asm's own references to them. External linkage guarantees the
   symbol exists; volatile stops the value being invented. */
volatile unsigned char open_a, open_p, ckout_p, st_after;

/* DOES THE DOS SCRIBBLE THE OVERLAY WINDOW? $C000..$CFFF is shadowed by ROMC
   while mapped, and ROM is a READ overlay -- stores fall through to the RAM
   underneath. If the C65 DOS keeps workspace there, every write would silently
   corrupt whichever overlay is loaded, and the game would have to reload it
   afterwards. Filling only to $CBFF because this probe's own soft stack is at
   $D000 growing down; the game's largest overlay ends at $CEBC, so 3K is
   enough to answer "does it touch this region at all". */
#define WIN     ((volatile unsigned char *)0xC000)
#define WIN_LEN 0x0C00
static unsigned int  win_bad;
static unsigned int  win_first;

static void say(const char *s) { cputs((const unsigned char *)s); }

#define MAP()  __asm__ volatile(MAPASM ::: "a", "memory")
#define UNMAP() __asm__ volatile("lda #$3e\n\tsta $01\n\t"                  \
                                 "lda #$44\n\tsta $d030\n\tcli" ::: "a", "memory")

int main(void)
{
    conioinit();
    clrscr();
    gotoxy(2, 1); say("PROBE 2: CAN THE C65 DOS WRITE UNDER THIS BANKING?");
    gotoxy(2, 2); say(CFGNAME);
    /* PRINTED BEFORE THE RISKY PART, so a hang still says which config hung.
       The screenshot is taken on SIGTERM whether or not the program got
       anywhere. */
    gotoxy(2, 4); say("mapping and opening device 8 ...");

    {   unsigned int i;
        for (i = 0; i < WIN_LEN; i++) WIN[i] = (unsigned char)((i ^ 0x5A) & 0xFF);
    }

    nlo = (unsigned char)(unsigned)fname;
    nhi = (unsigned char)(((unsigned)fname) >> 8);
    stage = 1;

    MAP();
    /* SETNAM(len, name), SETLFS(lfn=2, dev=8, sa=2), OPEN. php/pla rather than
       a branch: no labels in inline asm, and the P byte carries the carry. */
    __asm__ volatile("lda #9\n\t" "ldx nlo\n\t" "ldy nhi\n\t" "jsr $ffbd\n\t"
                     "lda #2\n\t" "ldx #8\n\t"  "ldy #2\n\t"  "jsr $ffba\n\t"
                     "jsr $ffc0\n\t"
                     "php\n\t" "sta open_a\n\t" "pla\n\t" "sta open_p"
                     ::: "a", "x", "y", "memory");
    stage = 2;
    __asm__ volatile("ldx #2\n\t" "jsr $ffc9\n\t"          /* CHKOUT */
                     "php\n\t" "pla\n\t" "sta ckout_p"
                     ::: "a", "x", "memory");
    stage = 3;
    __asm__ volatile("lda #$48\n\tjsr $ffd2\n\t"           /* 'H' */
                     "lda #$49\n\tjsr $ffd2\n\t"           /* 'I' */
                     "lda #$0d\n\tjsr $ffd2"               /* CR  */
                     ::: "a", "memory");
    stage = 4;
    __asm__ volatile("jsr $ffcc\n\t"                       /* CLRCHN */
                     "lda #2\n\tjsr $ffc3\n\t"             /* CLOSE 2 */
                     "jsr $ffb7\n\tsta st_after"           /* READST */
                     ::: "a", "x", "y", "memory");
    stage = 5;
    UNMAP();

    gotoxy(2, 6);  say("stage reached (5 = finished): "); cputhex(stage, 2);
    gotoxy(2, 7);  say("OPEN   A="); cputhex(open_a, 2);
    say("  P="); cputhex(open_p, 2);
    say("  carry="); cputhex((unsigned char)(open_p & 1), 2);
    gotoxy(2, 8);  say("CHKOUT P="); cputhex(ckout_p, 2);
    say("  carry="); cputhex((unsigned char)(ckout_p & 1), 2);
    gotoxy(2, 9);  say("READST after close = "); cputhex(st_after, 2);
    {   unsigned int i;
        win_bad = 0; win_first = 0xFFFF;
        for (i = 0; i < WIN_LEN; i++)
            if (WIN[i] != (unsigned char)((i ^ 0x5A) & 0xFF)) {
                if (win_first == 0xFFFF) win_first = i;
                win_bad++;
            }
    }
    gotoxy(2, 10); say("window $C000-$CBFF bytes changed = ");
    cputhex(win_bad, 4);
    say("  first +"); cputhex(win_first, 4);

    gotoxy(2, 12); say("now read the D81 back -- that is the real answer.");

    for (;;) { }
}
