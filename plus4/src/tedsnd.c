/* TED sound -- A STUB FOR NOW, AND SAYING SO.
 *
 * The Plus/4's TED has two voices where the C64 has a SID, so sid.c cannot be
 * linked here the way the C64 links it. This stub exists so the whole game
 * can be LINKED and the budget read before a driver is written against a fit
 * that might not exist -- the staging rule every port here starts with.
 *
 * snd_poll() IS CALLED FROM THE KEY LOOP FROM THE FIRST BUILD, stub or not:
 * the Atari 8-bit port shipped SILENT because it was missing from both loops
 * of kb_waitkey and the driver had been dead-stripped, with nothing pointing
 * at the cause.
 *
 * A STUB UNDERSTATES. [[seam-costs-more-than-driver]]: video once cost 4,636
 * bytes for a 1,559-byte driver because the CALLERS grow. Whatever this
 * measures, the real driver costs more.
 */
#include "../../c128/src/sid.h"

uint8_t snd_region = REGION_PAL;   /* a Plus/4 is PAL or NTSC; see the driver */

void snd_init(void) { }
void snd_off(void) { }
void snd_music_data(unsigned int base, unsigned char ok) { (void)base; (void)ok; }
void snd_music(uint8_t track) { (void)track; }
void snd_effect(uint8_t track) { (void)track; }
void snd_beep(void) { }
void snd_poll(void) { }
uint8_t snd_enabled(void) { return 0; }
void snd_toggle(void) { }
