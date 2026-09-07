/* PROBE 1: what stops being our RAM while the C65 ROM is mapped?
 *
 * Two sources disagree and the answer decides how much work MEGA65 saving is.
 * The 2026-09-04 note says "$8000..$BFFF is not our RAM"; reading $D030=$64
 * bit by bit says ROM8 (bit 3) is CLEAR, so $8000..$9FFF should stay RAM and
 * only $A000..$BFFF (BASIC, via $01) plus $C000..$CFFF (ROMC, bit 5) go away.
 *
 * It matters because plat_read_all sits at $913E in the game. Under the first
 * reading a write routine there kills itself and has to be relocated below
 * $8000; under the second it can stay where it is.
 *
 * NOTHING HERE CAN HANG: it maps, READS, and unmaps. No DOS, no device 8.
 * That is deliberate -- probe 2 does the part that can wedge, and keeping the
 * two apart means a hang there cannot be mistaken for a banking answer.
 *
 * THE PROBE IS TINY ON PURPOSE. Its own .bss lands just past its code, around
 * $2xxx, so every variable it touches is below $8000 no matter which reading
 * is right. A probe with its results parked at $A784 -- where the game's
 * io_buf actually is -- would be measuring itself.
 */
#include <stdint.h>
#include <mega65/conio.h>

/* Outside this probe's own image, so writing them disturbs nothing. */
#define M8 ((volatile uint8_t *)0x8000)
#define M9 ((volatile uint8_t *)0x9000)
#define MA ((volatile uint8_t *)0xA000)
#define MB ((volatile uint8_t *)0xB000)
#define MC ((volatile uint8_t *)0xCF00)

static uint8_t res[4][5];

/* conio wants unsigned char*; keep the casts in one place. */
static void say(const char *s) { cputs((const unsigned char *)s); }

#define MAP(P01, D030)                                                      \
    __asm__ volatile("sei\n\tlda #" #P01 "\n\tsta $01\n\t"                  \
                     "lda #" #D030 "\n\tsta $d030" ::: "a", "memory")
/* Back to what unmap-basic.o's .init leaves: ROM out, our RAM back. */
#define UNMAP()                                                             \
    __asm__ volatile("lda #$3e\n\tsta $01\n\t"                              \
                     "lda #$44\n\tsta $d030\n\tcli" ::: "a", "memory")

#define TEST(i, P01, D030)                                                  \
    do { MAP(P01, D030);                                                    \
         res[i][0] = *M8; res[i][1] = *M9;                                  \
         res[i][2] = *MA; res[i][3] = *MB; res[i][4] = *MC;                 \
         UNMAP(); } while (0)

/* gotoxy per row rather than newlines: conio printed my \n\r as glyphs and the
   first run's table wrapped across three lines. A table you have to decode is
   a table you can misread. */
static void row(uint8_t y, const char *label, uint8_t i)
{
    uint8_t k;
    static const uint8_t want[5] = { 0x88, 0x99, 0xAA, 0xBB, 0xCC };

    gotoxy(2, y);
    say(label);
    for (k = 0; k < 5; k++) {
        gotoxy((uint8_t)(18 + k * 10), y);
        cputhex(res[i][k], 2);
        say(res[i][k] == want[k] ? " RAM" : " rom");
    }
}

int main(void)
{
    conioinit();
    clrscr();
    gotoxy(2, 1);
    say("PROBE 1: WHAT STOPS BEING OUR RAM WHILE THE C65 ROM IS MAPPED");

    /* The markers, written with the ROM out -- these are plain RAM now. */
    *M8 = 0x88; *M9 = 0x99; *MA = 0xAA; *MB = 0xBB; *MC = 0xCC;

    /* READ THEM BACK UNMAPPED FIRST. Without this the run is unfalsifiable:
       if the stores never landed, every config below reads "rom" and the
       probe reports a confident wrong answer. */
    res[0][0] = *M8; res[0][1] = *M9;
    res[0][2] = *MA; res[0][3] = *MB; res[0][4] = *MC;

    /* $01 BITS, AND THE FIRST RUN OF THIS PROBE GOT THEM WRONG:
         bit 0 LORAM  1 = BASIC ROM at $A000..$BFFF
         bit 1 HIRAM  1 = KERNAL at $E000..$FFFF
         bit 2 CHAREN 1 = I/O at $D000, 0 = character ROM
       So $3E -- what unmap-basic.o leaves -- is ALREADY KERNAL + I/O with
       BASIC out, and $3F only adds BASIC. The first attempt tested $3B, which
       sets LORAM and CLEARS CHAREN: BASIC in and I/O gone, the opposite of the
       question. It read as "BASIC cannot be paged out", which is false.
       $D030 bit 4 is ROMA and bit 5 ROMC; $44 has neither, $64 has ROMC. */
    TEST(1, $3f, $64);   /* what llvm-mos's .fini restores -- known good for DOS */
    TEST(2, $3f, $44);   /* BASIC in, ROMC OUT -- does the $C000 window survive? */
    TEST(3, $3e, $64);   /* BASIC OUT, ROMC in -- does $A000 survive?            */

    gotoxy(2, 4);
    say("cfg   $01 $D030");
    gotoxy(18, 4); say("$8000");
    gotoxy(28, 4); say("$9000");
    gotoxy(38, 4); say("$A000");
    gotoxy(48, 4); say("$B000");
    gotoxy(58, 4); say("$CF00");
    row(6,  "none   3e  44", 0);
    row(7,  "A      3f  64", 1);
    row(8,  "B      3f  44", 2);
    row(9,  "C      3e  64", 3);
    gotoxy(2, 11);
    say("RAM = our marker survived.  rom = shadowed.");

    for (;;) { }
}
