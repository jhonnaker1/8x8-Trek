/* PROBE 3: is byte-at-a-time CBDOS fast enough to replace the Hypervisor?
 *
 * The question behind it is a design one: if everything moved onto a D81 and
 * the SD card were dropped, there would be ONE I/O stack instead of two, no
 * two-filesystem problem for the save, and the 512-byte sector buffer would go
 * away. The cost is that OVERLAYS.BIN -- 45,056 bytes -- has to come in
 * through CHRIN at startup, because the C65 KERNAL's banked LOAD exists but
 * Xemu cannot run it (see "The C65 KERNAL HAS a banked LOAD").
 *
 * So: how long does 45K take? That number decides it, and nothing else here
 * is worth arguing about until it is known.
 *
 * TIMED ON CIA1's TOD, which is the only honest clock on this machine -- ten
 * ticks a second, while no ROM interrupt runs at all and the raster wraps
 * twice per frame. Read hours FIRST (that latches) and tenths LAST (that
 * releases), or the registers can tear.
 *
 * THE CHECKSUM IS NOT DECORATION. A read that returns zeros fast would look
 * like a wonderful result.
 */
#include <stdint.h>
#include <mega65/conio.h>

static const unsigned char fname[] = "OVL,S,R";
static volatile unsigned char nlo, nhi;
static volatile unsigned char stage;
volatile unsigned char open_p, chkin_p, inbyte, instat;

static void say(const char *s) { cputs((const unsigned char *)s); }

/* Config C from probe 2: BASIC stays out, ROMC in. */
#define MAP()   __asm__ volatile("sei\n\tlda #$3e\n\tsta $01\n\t"           \
                                 "lda #$64\n\tsta $d030" ::: "a", "memory")
#define UNMAP() __asm__ volatile("lda #$3e\n\tsta $01\n\t"                  \
                                 "lda #$44\n\tsta $d030\n\tcli" ::: "a", "memory")

/* One byte plus its status. The C loop around this costs a microsecond or so
   at 40MHz, which is counted IN the result on purpose -- a real implementation
   would have the same shape, so this is the honest number, not a floor. */
#define CHRIN() __asm__ volatile("jsr $ffcf\n\tsta inbyte\n\t"              \
                                 "jsr $ffb7\n\tsta instat"                  \
                                 ::: "a", "x", "y", "memory")

static unsigned char un_bcd(unsigned char v)
{
    return (unsigned char)((v >> 4) * 10 + (v & 0x0F));
}

static unsigned int tod_tenths(void)
{
    unsigned char m, s, t;
    (void)*(volatile unsigned char *)0xDC0B;   /* hours: LATCHES the set */
    m = *(volatile unsigned char *)0xDC0A;
    s = *(volatile unsigned char *)0xDC09;
    t = *(volatile unsigned char *)0xDC08;     /* tenths: RELEASES it    */
    return (unsigned int)(((unsigned int)un_bcd(m) * 60u
                         + (unsigned int)un_bcd(s)) * 10u + (t & 0x0F));
}

int main(void)
{
    unsigned long count = 0;
    unsigned char csum = 0;
    unsigned int t0, t1;

    conioinit();
    clrscr();
    gotoxy(2, 1); say("PROBE 3: HOW FAST IS BYTE-AT-A-TIME CBDOS?");
    gotoxy(2, 3); say("opening OVL on device 8 ...");

    nlo = (unsigned char)(unsigned)fname;
    nhi = (unsigned char)(((unsigned)fname) >> 8);
    stage = 1;

    MAP();
    __asm__ volatile("lda #7\n\t" "ldx nlo\n\t" "ldy nhi\n\t" "jsr $ffbd\n\t"
                     "lda #2\n\t" "ldx #8\n\t"  "ldy #2\n\t"  "jsr $ffba\n\t"
                     "jsr $ffc0\n\t" "php\n\t" "pla\n\t" "sta open_p"
                     ::: "a", "x", "y", "memory");
    stage = 2;
    __asm__ volatile("ldx #2\n\t" "jsr $ffc6\n\t"          /* CHKIN */
                     "php\n\t" "pla\n\t" "sta chkin_p"
                     ::: "a", "x", "memory");
    stage = 3;

    /* THE EOF BYTE IS VALID, and the first run of this probe threw it away:
       `if (instat) break;` before counting returned 45055 bytes of 45056 and a
       checksum of $C3 against the file's $57. ST bit 6 (EOF) is set TOGETHER
       WITH the last good byte; only bit 7 means the byte is junk. $C3 ^ $94 --
       the last byte of OVERLAYS.BIN, the high half of the build stamp -- is
       exactly $57, which is what identified the fault rather than a guess.
       Any real implementation has to get this right or every file it reads is
       one byte short. */
    t0 = tod_tenths();
    for (;;) {
        CHRIN();
        if (instat & 0x80) break;                  /* error: byte is junk */
        csum = (unsigned char)(csum ^ inbyte);
        count++;
        if (instat & 0x40) break;                  /* EOF: byte was good  */
    }
    t1 = tod_tenths();

    stage = 4;
    __asm__ volatile("jsr $ffcc\n\t" "lda #2\n\tjsr $ffc3" ::: "a","x","y","memory");
    stage = 5;
    UNMAP();

    gotoxy(2, 5);  say("stage (5 = finished) = "); cputhex(stage, 2);
    gotoxy(2, 6);  say("OPEN carry="); cputhex((unsigned char)(open_p & 1), 2);
    say("   CHKIN carry="); cputhex((unsigned char)(chkin_p & 1), 2);
    gotoxy(2, 7);  say("bytes read  = "); cputhex(count, 8);
    gotoxy(2, 8);  say("xor csum    = "); cputhex(csum, 2);
    gotoxy(2, 9);  say("tenths      = "); cputhex((unsigned int)(t1 - t0), 4);
    gotoxy(2, 11); say("45056 bytes is what OVERLAYS.BIN would cost at startup.");

    for (;;) { }
}
