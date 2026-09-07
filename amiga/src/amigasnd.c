/* Sound for the Amiga: NOT BUILT YET, and these are the stubs that say so.
 *
 * Deliberately last. Paula is four channels of SAMPLED audio, not a
 * synthesiser -- the furthest from the SID of any target so far -- so the
 * nine-function seam has to be re-fitted around a waveform and a period per
 * note rather than a frequency. Both previous ports lost time to a TEMPO bug
 * (the C128 to a driver three semitones from its cause, the MEGA65 to a
 * raster that wraps twice a frame), so it goes after everything that can be
 * checked without it.
 *
 * A SILENT GAME IS A PLAYABLE GAME, which is the whole reason the seam has an
 * `enabled` flag and a MUSIC.DAT that may be absent: main.c already treats
 * missing music as a luxury it can do without. These stubs put this port in
 * exactly that state rather than in a broken one.
 *
 * snd_poll() IS THE ONE THAT MATTERS. Every port calls it from inside the
 * keyboard wait, because that is where the program spends its idle time and
 * the driver's only chance to run. It must exist and must be cheap; when the
 * driver lands, this is where it hooks in.
 */
#include <stdint.h>

#include "../../c128/src/sid.h"

/* REGION_PAL, which is 1 -- see c128/src/sidfreq.h. Written as the value
   rather than the name because that header also carries two static helpers
   for the SID's frequency table, and pulling them into a file with no driver
   makes them unused-function errors under -Werror. When the Paula driver
   lands it will include the header properly and use the name. */
uint8_t snd_region = 1;

void snd_init(void) { }
void snd_off(void) { }

void snd_music_data(unsigned int base, unsigned char ok) {
    (void)base; (void)ok;
}

void snd_music(uint8_t track)  { (void)track; }
void snd_effect(uint8_t track) { (void)track; }
void snd_beep(void) { }
void snd_poll(void) { }

/* The SND command toggles sound and reports the state. Answering "off" and
   staying off is honest; answering "on" and staying silent is not. */
uint8_t snd_enabled(void) { return 0; }
void    snd_toggle(void) { }
