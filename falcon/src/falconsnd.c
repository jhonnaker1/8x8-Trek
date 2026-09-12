/* Sound for the Falcon -- STUB, and it is the ONE seam the scope did not
 * measure. See NOTES.md, "SCOPE: the ATARI FALCON": the machine has a YM2149
 * for ST compatibility, which is the closest analogue to what the 8-bit ports
 * already drive, and an 8-bit stereo DMA CODEC that is richer than anything
 * this project has used. Neither has been touched.
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
