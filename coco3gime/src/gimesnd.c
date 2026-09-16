/* Sound for a CoCo 3 with no SuperSprite -- A STUB, AND SAYING SO.
 *
 * The YM2149 was ON THE CARD. This machine has the 6-bit DAC at $FF20 driven
 * through the PIA, which is a different driver entirely: a square wave needs
 * the CPU or a GIME timer interrupt to toggle it, where the PSG just held a
 * period in a register.
 *
 * It is a stub so that `make early` can measure the REAL budget question --
 * whether the string pool fits in 64K beside a 45K program and a 4,000-byte
 * screen -- before any effort goes into a driver that a failed budget would
 * make pointless.
 *
 * snd_poll() IS CALLED FROM THE KEY LOOP FROM THE FIRST BUILD, stub or not:
 * the Atari 8-bit port shipped SILENT because it was missing from both loops
 * of kb_waitkey and the driver had been dead-stripped, with nothing pointing
 * at the cause.
 */
#include "../../c128/src/sid.h"

unsigned char snd_region = REGION_NTSC;

void snd_init(void) { }
void snd_off(void) { }
void snd_music_data(unsigned int base, unsigned char ok) { (void)base; (void)ok; }
void snd_music(unsigned char track) { (void)track; }
void snd_effect(unsigned char track) { (void)track; }
void snd_beep(void) { }
void snd_poll(void) { }
unsigned char snd_enabled(void) { return 0; }
void snd_toggle(void) { }
