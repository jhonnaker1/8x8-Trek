/* Four tones through the real driver, so the frequency arithmetic that ships
 * is the frequency arithmetic that was measured.
 *
 * The fourth is on VOICE 1, because "two voices" is a claim and a second
 * oscillator addressed independently is the evidence for it.
 */
#define ASMVAR __attribute__((used, retain))

#include "../../c128/src/sid.h"

void gs_test_note(unsigned char ch, unsigned char tens);
void gs_test_off(unsigned char ch);

ASMVAR unsigned char gs_done;

__attribute__((naked, used, retain, section(".init.040")))
void gs_emul(void) { __asm__ volatile("sec\n\txce"); }
__attribute__((naked, used, retain, section(".init.050")))
void gs_setdb(void) { __asm__ volatile("phk\n\tplb"); }

#define VBL (*(volatile unsigned char *)0xC019)

static void frames(unsigned int n)
{
    while (n--) {
        while (VBL & 0x80) ;
        while (!(VBL & 0x80)) ;
    }
}

static void play(unsigned char ch, unsigned char tens)
{
    gs_test_note(ch, tens);
    frames(90);
    gs_test_off(ch);
    frames(25);
}

int main(void)
{
    __asm__ volatile("sei");
    snd_init();
    frames(30);
    play(0, 44);      /* 440 Hz */
    play(0, 100);     /* 1000 Hz */
    play(0, 20);      /* 200 Hz */
    play(1, 44);      /* 440 Hz, on the effects voice */
    gs_done = 0x5A;
    for (;;) ;
    return 0;
}
