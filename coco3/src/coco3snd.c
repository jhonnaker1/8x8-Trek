/* Sound for the CoCo 3 + SuperSprite FM+ -- STUB.
 *
 * The card brings a YM2413 OPLL, which is FM and richer than anything else
 * this project drives; the original SuperSprite had an AY-3-8910 and the FM+
 * adds the OPLL. Neither has been touched, and the scope never measured this
 * seam on either card.
 *
 * snd_enabled() returns 0 so the SND command reports the truth rather than
 * claiming a driver that is not here.
 */
#include <stdint.h>

#include "../../c128/src/sid.h"

void snd_init(void) { }
void snd_off(void) { }
void snd_music_data(unsigned int base, unsigned char ok) { (void)base; (void)ok; }
void snd_music(uint8_t track) { (void)track; }
void snd_effect(uint8_t track) { (void)track; }
void snd_beep(void) { }
void snd_poll(void) { }
uint8_t snd_enabled(void) { return 0; }
void snd_toggle(void) { }
