#ifndef M65SND_H
#define M65SND_H

#include <stdint.h>

/* Sound, C128 SID. Platform layer only -- core/ has no audio call and must
   never gain one. What the core offers is its event list, and the platform
   decides what noise an event makes.

   Two voices, deliberately: voice 1 carries music, voice 2 carries effects, so
   a hit during the title track does not chop the tune. The original had one PC
   speaker and could not do that. This is the one area NOTES item 12 says every
   port can beat the original outright rather than approximate it, and two
   voices is the cheapest possible way to take it up. */

/* Track ids come from the generated header -- MUS_TITLE, MUS_END, SFX_A.. */
#include "music_data.h"

#define MUS_NONE 0xFF

void snd_init(void);        /* silence the SID, detect PAL/NTSC */
void snd_off(void);         /* full stop, for the way out */

/* Tells the driver where MUSIC.DAT landed in far memory, and whether it
   loaded at all. Without it every track is refused and the game is silent. */
void snd_music_data(unsigned int base, unsigned char ok);

void snd_music(uint8_t track);   /* start a looping track, or MUS_NONE */
void snd_effect(uint8_t track);  /* fire a one-shot on the effects voice */

/* The original's refusal beep: 440Hz for 250ms, MEASURED, and the sound it
   makes when the ship declines an order. Blocking, as it is there. */
void snd_beep(void);

/* Advances the music. Call it from anywhere that waits -- it is cheap, it
   times itself off the VIC raster, and calling it more often than once per
   frame costs nothing but a compare. kb_waitkey() is the important caller:
   that is where this port spends every second it is not drawing. */
void snd_poll(void);

uint8_t snd_enabled(void);
void    snd_toggle(void);   /* what the original's SND command does */

extern uint8_t snd_region;  /* REGION_PAL or REGION_NTSC, set by snd_init */

#endif
