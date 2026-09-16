/* IS LOW RAM SAFE FOR THE SCREEN WHILE THE DISK IS IN USE?
 *
 * The map says every DSKCON variable resolves into the program's own bss at
 * $D498 and above, so nothing the driver needs lives under $2800. THAT IS AN
 * ARGUMENT, NOT A MEASUREMENT, and on this machine the difference has
 * mattered twice: MMUEN broke the disk for reasons still unexplained, and a
 * 6809 frame pointer sitting in a paged window broke it before that.
 *
 * So this writes a pattern into the whole 4,000-byte screen buffer at $1000,
 * READS A FILE OFF THE DISK, and then checks the pattern survived AND the
 * read worked. Both halves matter: a driver that scribbles on low RAM and a
 * screen that stops the driver working are different faults with the same
 * symptom.
 */
#include "../../c128/src/vdc.h"
#include "../../core/ega.h"
#include "../../coco3/src/coco3storage.h"
#include "egagime.h"

#define SCREEN ((unsigned char *)0x1000)
#define NCELLS (80 * 25)

static unsigned char sector[256];

/* layout.c is not linked here -- this probe draws its own text and wants
   nothing from the panel table -- but scr_puts is in gimevid.c and that is
   all it needs. */

static void say(unsigned char row, const char *s, unsigned char col)
{
    scr_puts(1, row, s, col);
}

static void num(unsigned char row, unsigned char x, unsigned long v)
{
    static char b[12];
    signed char i;
    for (i = 9; i >= 0; i--) { b[i] = (char)('0' + (unsigned char)(v % 10)); v /= 10; }
    b[10] = '\0';
    scr_puts(x, row, b, EGA_LTGRAY);
}

int main(void)
{
    unsigned int i;
    unsigned long len = 0;
    unsigned char first, ok, kept, before;

    asm { orcc #$50 }

    /* THE DISK FIRST, BEFORE THE SCREEN EXISTS -- this is the CONTROL. The
       first run of this probe filled low RAM and then failed to find the
       file, which reads as "low RAM broke the disk" and is only one of two
       possibilities: the other is that the disk was never going to work in
       this probe's setup at all. Doing it in this order separates them. If
       `before` is NO, low RAM is innocent. */
    before = plat_raw_open("STRINGS.DAT", &len);

    vdc_init();

    /* Fill every cell with a known pattern -- character AND attribute -- so a
       single byte disturbed anywhere in the 4,000 shows up. */
    for (i = 0; i < NCELLS; i++) {
        SCREEN[i * 2]     = (unsigned char)('A' + (i % 26));
        SCREEN[i * 2 + 1] = (unsigned char)(((i % 8) << 3));
    }

    /* NOW USE THE DISK AGAIN, with that pattern sitting under $2800. */
    first = plat_raw_open("STRINGS.DAT", &len);
    ok = (first != 0xFF) ? plat_raw_sector(first, 0, sector) : 0;

    /* Did the pattern survive? */
    kept = 1;
    for (i = 0; i < NCELLS; i++) {
        if (SCREEN[i * 2] != (unsigned char)('A' + (i % 26))) { kept = 0; break; }
    }

    scr_clear();
    say(1, "LOW RAM SCREEN AT $1000, WITH THE DISK IN USE", EGA_LTGREEN);

    say(2, before != 0xFF ? "BEFORE THE SCREEN EXISTED  FOUND"
                          : "BEFORE THE SCREEN EXISTED  NOT FOUND",
        before != 0xFF ? EGA_LTGREEN : EGA_LTRED);
    say(3, first != 0xFF ? "AFTER 4,000 BYTES AT $1000 FOUND"
                         : "AFTER 4,000 BYTES AT $1000 NOT FOUND",
        first != 0xFF ? EGA_LTGREEN : EGA_LTRED);
    say(4, "   ITS LENGTH", EGA_LTGRAY);
    num(4, 20, len);

    say(5, ok ? "SECTOR 0 READ          YES" : "SECTOR 0 READ          NO",
        ok ? EGA_LTGREEN : EGA_LTRED);
    say(6, "   FIRST TWO BYTES ARE THE POOL'S COUNT", EGA_LTGRAY);
    num(6, 46, (unsigned long)(sector[0] | ((unsigned int)sector[1] << 8)));

    say(8, kept ? "THE 4,000-BYTE PATTERN SURVIVED   YES"
                : "THE 4,000-BYTE PATTERN SURVIVED   NO",
        kept ? EGA_LTGREEN : EGA_LTRED);

    if (before == 0xFF)
        say(10, "THE DISK NEVER WORKED HERE -- LOW RAM IS NOT THE SUSPECT",
            EGA_BROWN);
    else if (first != 0xFF && ok && kept)
        say(10, "LOW RAM IS SAFE FOR THE SCREEN", EGA_LTGREEN);
    else
        say(10, "LOW RAM BROKE THE DISK -- THE CONTROL PASSED AND THIS DID NOT",
            EGA_LTRED);

    for (;;) { }
    return 0;
}
