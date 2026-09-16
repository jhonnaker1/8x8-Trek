/* DOES IT PLAY THE RIGHT NOTES, AND DOES THE DRIVE STILL WORK WHILE IT DOES?
 *
 * Two questions that "I can hear something" answers neither of.
 *
 * THE PITCH is read off a WAV recorded from the emulator and measured by
 * tools/hearit.py, at THREE points -- 440, 1000 and 200 Hz -- because a
 * single point cannot see an octave error and this project shipped one for
 * four months.
 *
 * THE DRIVE matters more. This is the first thing in the port to enable
 * interrupts, and the standalone DSKCON underneath it takes an NMI per
 * sector. So a file is read WHILE a tone is sounding, three times, and the
 * results go in the report. A driver that silences the drive is worse than no
 * driver at all.
 *
 * It includes the driver rather than linking it, so it can reach voice_note
 * and hold a known pitch. Everything else goes through the public seam.
 */
#include "gimesnd.c"
#include "../../core/storage.h"

#define r    ((unsigned char *)0x5F00)
#define DONE 0x5A

static unsigned int got;
static unsigned char buf[512];

/* Waits n fields on the polled V-BORD flag, bounded so a dead source cannot
   hang the probe. */
static void fields(unsigned int n)
{
    unsigned int guard;
    while (n) {
        guard = 0;
        while (!frame_tick()) if (++guard == 0) return;
        n--;
    }
}

int main(void)
{
    unsigned int i;

    for (i = 0; i < 32; i++) r[i] = 0xFF;
    r[0] = 0xA5;
    /* MASK BEFORE THE ROM GOES AWAY. `sta $FFDF` deletes Disk BASIC from the
       address space, and BASIC leaves PIA0's 60 Hz field-sync interrupt
       enabled -- so the very next field vectors through $FEF7 into an address
       that is no longer code. The loader's own stub does this first for the
       same reason; this probe did not, and wedged before snd_init ran. */
    asm { orcc #$50 }
    asm { lds #$5E00 }
    asm { sta $FFDF }

    INIT0 = GIME_INIT0;
    r[1] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);   /* control */

    /* BREADCRUMBS, because "it wedged somewhere in the driver" is not a
       diagnosis and guessing which line cost a run already. */
    r[10] = 0x01;  snd_init();
    r[10] = 0x02;  fields(5);
    r[10] = 0x03;

    /* Three tones, a second each, with a clear gap between so the analysis
       can segment them without guessing. */
    voice_note(44);  r[10] = 0x04;  fields(60);  voice_off();  fields(20);  r[10] = 0x05;
    voice_note(100);  fields(60);  voice_off();  fields(20);
    voice_note(20);   fields(60);  voice_off();  fields(20);

    /* THE DISK, WITH A TONE SOUNDING AND THE TIMER INTERRUPT LIVE. */
    voice_note(44);
    r[2] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);
    r[3] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);
    r[4] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);
    voice_off();

    /* And the refusal beep through its public entry point. */
    fields(20);
    snd_beep();

    fields(20);
    r[5] = plat_read_all("OVLDEMO.BIN", buf, sizeof buf, &got);   /* after */

    snd_off();
    r[31] = DONE;
    for (;;) ;
}
