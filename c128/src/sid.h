#ifndef SID_H
#define SID_H

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

void snd_music(uint8_t track);   /* start a looping track, or MUS_NONE */
void snd_effect(uint8_t track);  /* fire a one-shot on the effects voice */

/* Advances the music. Call it from anywhere that waits -- it is cheap, it
   times itself off the VIC raster, and calling it more often than once per
   frame costs nothing but a compare. kb_waitkey() is the important caller:
   that is where this port spends every second it is not drawing. */
void snd_poll(void);

uint8_t snd_enabled(void);
void    snd_toggle(void);   /* what the original's SND command does */

extern uint8_t snd_region;  /* REGION_PAL or REGION_NTSC, set by snd_init */

#endif
