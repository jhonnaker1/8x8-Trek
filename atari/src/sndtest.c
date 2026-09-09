/* Does POKEY play what the seam was told to play?
 *
 * THREE THINGS, and each is one this project has got wrong on another port:
 *   1. the REGION, which the MEGA65 got from reasoning and ran 19% fast on,
 *   2. the PITCH, which is a divide here rather than a table,
 *   3. the TEMPO -- that notes advance at all, which the X16's dead jiffy
 *      clock silently prevented.
 *
 * DRIVEN FROM OUTSIDE, like src/sndprobe.c: the harness raises a command byte
 * and the 6502 calls the seam, so what is exercised is the shipping driver and
 * not a copy of it. tools/sndtest.py then asks Altirra what frequency each
 * voice is actually producing.
 *
 *      $0600  command: 1 = snd_beep, 2 = start the planted track, 3 = snd_off
 *      $0601  ack, incremented when a command is taken
 *      $0602  snd_region as snd_init decided it
 */
#include <stdint.h>

#include "vbxevid.h"
#include "../../c128/src/sid.h"
#include "../../c128/src/music_data.h"
#include "atarimem.h"

#define CMD ((volatile unsigned char *)0x0600)

/* A TRACK PLANTED BY HAND, because the storage seam is still stubbed and
   MUSIC.DAT cannot be read yet. Two notes at the extremes of what the real
   music uses -- 930 Hz is its highest and 90 Hz its lowest -- so the pitch
   check covers the whole range the divide has to serve, and a zero pair ends
   it so the loop-back path is exercised too. Durations are player ticks at
   18.2Hz: 9 is about half a second. */
static const unsigned char track[] = { 9, 93,  9, 9,  0, 0 };

int main(void) {
    unsigned char row = 2;

    vdc_init();
    snd_init();

    CMD[0] = 0;
    CMD[1] = 0;
    CMD[2] = snd_region;

    scr_puts(2, 0, "POKEY -- REGION, PITCH, TEMPO", 15);
    scr_puts(2, row, snd_region == REGION_PAL ? "REGION: PAL" : "REGION: NTSC", 14);

    /* Straight into far memory at the offset snd_music will look for track 0,
       and then tell the seam the store starts at zero. */
    far_write((uint16_t)mus_offset[0], track, (uint16_t)sizeof track);
    snd_music_data(0, 1);

    for (;;) {
        snd_poll();
        if (CMD[0]) {
            unsigned char c = CMD[0];
            CMD[0] = 0;
            if (c == 1) snd_beep();
            else if (c == 2) snd_music(0);
            else if (c == 3) snd_off();
            CMD[1]++;
        }
    }
    return 0;
}
