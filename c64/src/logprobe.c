/* WHERE DOES A C64 PUT THE 2K MESSAGE LOG? Item 59, measured rather than argued.
 *
 * THE PROBLEM. `c128/src/ui.c` keeps its message log -- LOG_SLOTS 32 x
 * LOG_STRIDE 64, so 2,048 bytes of READ/WRITE scratch -- in VDC RAM, reached
 * through vdc_set_address/vdc_data_read/vdc_data_write. Every port implements
 * those three, so the seam is portable; it is only NAMED after the C128's
 * chip. The 40-column C128 build gets away with it because the VDC is still
 * in the machine, merely not driving the monitor.
 *
 * A C64 HAS NO SUCH CHIP, and its address space is already spoken for: the
 * program wants ~38K at $0801, the 7,380-byte string pool wants the 8K under
 * BASIC at $A000 (leaving about 800 bytes, so not both), and the overlay
 * window wants the 4K at $C000. The remaining candidate is THE 8K UNDER THE
 * KERNAL ROM at $E000-$FFFF.
 *
 * AND THE QUESTION THAT DECIDES IT IS NOT "can I write there". Of course a
 * store to $E000 lands in RAM -- writes go to RAM whatever is banked in. The
 * question is whether it SURVIVES, because the port needs the KERNAL for
 * every disk operation, and if any KERNAL routine stores into the RAM beneath
 * itself the log is corrupted by the act of loading a file. Reasoning says it
 * does not: the KERNAL's variables live at $0200-$03FF. Reasoning has been
 * wrong on this project often enough to be worth ten minutes.
 *
 * So: fill 2K under the KERNAL, do REAL KERNAL DISK I/O, read it back.
 *
 * INTERRUPTS OFF WHILE THE ROM IS OUT, and that is not caution. With bit 1 of
 * $01 clear the CPU fetches its IRQ vector from $FFFE in RAM, which holds our
 * pattern -- an interrupt there jumps into the message log.
 */
#include <stdint.h>
#include <string.h>
#include <cbm.h>

#define PORT   (*(volatile unsigned char *)0x0001)
#define BANK_KERNAL_OUT 0x35      /* BASIC in, KERNAL out, I/O in */
#define BANK_NORMAL     0x37      /* the value the machine boots with */

#define LOG      ((unsigned char *)0xE000)
#define LOG_SIZE 2048

/* The report, in low RAM where the monitor can read it without banking. */
#define R ((volatile unsigned char *)0x0334)

/* A pattern with no run of equal bytes, so a region that is half written or
   written twice cannot pass by looking plausible. */
static unsigned char want(unsigned int i)
{
    return (unsigned char)(((i * 7u) + (i >> 3)) ^ 0x5A);
}

static void fill(void)
{
    unsigned int i;
    __asm__ volatile("sei" ::: "memory");
    PORT = BANK_KERNAL_OUT;
    for (i = 0; i < LOG_SIZE; i++) LOG[i] = want(i);
    PORT = BANK_NORMAL;
    __asm__ volatile("cli" ::: "memory");
}

/* Returns 0 if every byte survived, else 1 + the first bad offset's high byte
   in R[3]/R[4]. Compared UNDER the bank, so nothing is copied out first --
   a copy would need 2K of somewhere else, which is the thing we have not got. */
static unsigned char verify(void)
{
    unsigned int i;
    unsigned char bad = 0;
    __asm__ volatile("sei" ::: "memory");
    PORT = BANK_KERNAL_OUT;
    for (i = 0; i < LOG_SIZE; i++) {
        if (LOG[i] != want(i)) { bad = 1; break; }
    }
    PORT = BANK_NORMAL;
    __asm__ volatile("cli" ::: "memory");
    if (bad) { R[3] = (unsigned char)(i >> 8); R[4] = (unsigned char)i; }
    return bad;
}

int main(void)
{
    unsigned char i;
    void *end;

    for (i = 0; i < 8; i++) R[i] = 0;
    R[0] = 0xA1;

    fill();

    /* IT SURVIVES ITS OWN BANKING -- check before blaming the KERNAL. If this
       already fails, the disk has nothing to do with it. */
    R[1] = verify() ? 0xE1 : 0xC1;

    /* NOW REAL KERNAL DISK I/O, the thing the port cannot do without. A LOAD
       is the heaviest of them: it opens the drive, reads, and runs the whole
       serial routine, all of it ROM code executing at $E000-$FFFF while our
       pattern sits underneath. */
    /* A SMALL FILE INTO THE FREE 4K AT $C000, not a big one into a small
       buffer. The first version loaded the 7,380-byte string pool into a
       256-byte array, which would have smashed its way through memory and
       measured MY bug instead of the machine's behaviour. $C000-$CFFF is the
       one block of C64 RAM that never has a ROM over it. */
    cbm_k_setlfs(2, 8, 0);
    cbm_k_setnam("MUSIC.DAT");
    end = cbm_k_load(0, (void *)0xC000);
    R[5] = (unsigned char)(((unsigned int)end) >> 8);
    R[6] = (unsigned char)((unsigned int)end);

    R[2] = verify() ? 0xE2 : 0xC2;
    R[7] = 0x5A;
    for (;;) { }
}
