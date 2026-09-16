/* WHAT DOES $FF3F ACTUALLY BANK, AND IS THE PROGRAM STILL THERE?
 *
 * Everything about this port's layout depends on the answer. Writing a 60K
 * memory map first and discovering it afterwards is how three CoCo 3 budgets
 * went wrong.
 *
 * THE REPORT IS A LINKER-PLACED ARRAY, NOT AN ADDRESS I CHOSE. The first
 * version used $0500 by analogy with the C64's cassette buffer; on a Plus/4
 * that holds 4C 1C 99, a live system jump vector, and the probe reported the
 * KERNAL's own bytes back at me. The linker knows what is free here and I do
 * not, so it picks -- tools/run_p4.py reads the address out of the map.
 */
#define TED_COLOR  (*(volatile unsigned char *)0xFF15)   /* background */
#define ROM_IN     (*(volatile unsigned char *)0xFF3E)
#define RAM_IN     (*(volatile unsigned char *)0xFF3F)
volatile unsigned char report[16];
#define R          report

int main(void)
{
    volatile unsigned char *a000 = (volatile unsigned char *)0xA000;
    volatile unsigned char *e000 = (volatile unsigned char *)0xE000;

    R[0] = 0xA1;

    /* What is visible where, with the ROM in -- which is how we start. */
    R[1] = a000[0];
    R[2] = e000[0];

    /* Put RAM everywhere, write a marker into both, read it back. */
    RAM_IN = 0;
    a000[0] = 0x5A;
    e000[0] = 0xA5;
    R[3] = a000[0];
    R[4] = e000[0];

    /* Back to ROM. If these differ from R[3]/R[4] the switch is real and the
       ROM genuinely covers both. */
    ROM_IN = 0;
    R[5] = a000[0];
    R[6] = e000[0];

    /* And RAM again -- did the writes survive being hidden? */
    RAM_IN = 0;
    R[7] = a000[0];
    R[8] = e000[0];

    ROM_IN = 0;
    TED_COLOR = 0x21;              /* visible proof it ran at all */
    R[9] = 0x5A;
    for (;;) { }
    return 0;
}
