/* Sound for the Falcon: the YM2149 PSG, three channels of square wave.
 *
 * THE CLOSEST TARGET TO THE SID SINCE THE C128, and the easiest sound seam on
 * the project as a result. The original made one square wave out of a PC
 * speaker; sid.c says "50% pulse, the square wave the original actually made",
 * and a PSG tone channel IS that with nothing in between. No sample buffer to
 * allocate like Paula, no waveform table, no duty cycle to get wrong -- write
 * a 12-bit period and an amplitude.
 *
 * THE MACHINE CAN ALSO DO 8-BIT STEREO DMA AUDIO, and this does not use it.
 * The music is seven tracks of square-wave note data; playing it through a
 * CODEC would mean synthesising the waveform this chip generates for free.
 * The DMA audio is the better instrument for sampled sound, which this game
 * does not have.
 *
 * THE PSG CLOCK IS 2 MHz, AND THAT WAS MEASURED RATHER THAN LOOKED UP.
 * Hatari recorded four tones at periods spanning what the music actually uses
 * -- 90Hz to 930Hz, not round numbers -- and the clock came back 2,000,160 /
 * 2,008,460 / 2,005,520 / 2,004,640 Hz, every point inside 0.42%. So
 * period = 2000000 / (16 * f), and with the track format's tens of Hz that is
 * 12500/tenths with no scaling constant to get wrong. FOUR POINTS BECAUSE ONE
 * CANNOT TELL A WRONG SCALE FROM A WRONG INTERCEPT: the X16's beep was an
 * octave flat AND had a free first tick, and a single sample would have
 * "confirmed" either explanation. See tools/falcon/psgcal.c.
 *
 * TIMING COMES FROM A CLOCK, NOT FROM A CALL COUNT. The tracks are written in
 * ticks of the PC's original 18.2065Hz timer. Ticking once per snd_poll()
 * would be easy and wrong: the key loop calls it about sixty times a second
 * but other callers run at no fixed rate, so the tempo would wander with
 * whatever the game happened to be doing. `_hz_200` at $4BA is the machine's
 * 200Hz counter and the tick is derived from it. THE FIRST TWO PORTS BOTH
 * LOST TIME TO A TEMPO BUG -- the C128 to a driver three semitones from its
 * cause, the MEGA65 to a raster that wraps twice a frame -- which is why this
 * one reads a clock, exactly as the Amiga does.
 */
#include <stdint.h>
#include <tos.h>

#include "../../c128/src/sid.h"
#include "../../core/farmem.h"
#include "music_data.h"

/* Giaccess is XBIOS 28 and is callable from user mode, so the PSG needs no
   Supexec of its own: register | 0x80 writes, plain register reads. */
#define PSG_W(r, v)  Giaccess((WORD)(v), (WORD)((r) | 0x80))

#define R_A_LO   0
#define R_A_HI   1
#define R_B_LO   2
#define R_B_HI   3
#define R_MIXER  7
#define R_A_VOL  8
#define R_B_VOL  9

/* Channel A is music and channel B is effects, the same split as the SID
   port's V1 and V2 -- so a laser does not cut the music off. */
#define V_MUS  0
#define V_SFX  1
#define VOLUME 12                  /* of 15; loud enough, short of harsh */

/* MIXER BITS ARE ACTIVE LOW, AND TWO OF THEM ARE NOT THE MIXER AT ALL.
 * Bits 0-2 enable tones A/B/C and 3-5 the noise generators, a 0 enabling in
 * both cases. Bits 6 and 7 are the PSG's PORT DIRECTION, and on this machine
 * port A drives floppy select, side select and the printer strobe -- so bit 7
 * MUST stay 1 (port A output) or the drive stops answering. Writing 0x00 here
 * to "turn everything off" is the classic way to lose the floppy.
 *
 * 0xBC: port A out, port B in, all three noise channels off, tone C off. Then
 * clearing bit 0 enables the music voice and bit 1 the effects voice. */
#define MIX_BASE  0xBC

/* The PSG clock, MEASURED (see the header). A note's period is
   clock / (16 * Hz), and the track format is tens of Hz. */
#define PSG_CLOCK   2000000UL
#define PERIOD_MAX  4095U          /* 12 bits, and 90Hz wants 1389 */

static unsigned char enabled = 1;
static unsigned char mixer = MIX_BASE | 0x03;   /* both voices silent */

static unsigned int  mus_base, mus_head, mus, sfx;
static unsigned char mus_ok, mus_on, sfx_on;
static unsigned char note_left, sfx_left;

static long last_200;
static unsigned long acc;

static long hz200_raw(void) { return *(volatile long *)0x4BAL; }

/* $4BA is system RAM, so the read goes through Supexec. A trap per poll is
   some tens of microseconds on a 16MHz 030 against a loop that runs sixty
   times a second, and it is correct under MiNT's memory protection where a
   direct read is not. */
static long now_200(void) { return Supexec(hz200_raw); }

static void voice_off(unsigned char v)
{
    mixer |= (unsigned char)(v == V_MUS ? 0x01 : 0x02);
    PSG_W(R_MIXER, mixer);
    PSG_W(v == V_MUS ? R_A_VOL : R_B_VOL, 0);
}

/* `tenths` is the track format's frequency: tens of Hz, 0 for a rest. */
static void voice_note(unsigned char v, unsigned char tenths)
{
    unsigned long period;

    if (!enabled || tenths == 0) {
        voice_off(v);
        return;
    }

    period = PSG_CLOCK / ((unsigned long)tenths * 10UL * 16UL);
    if (period < 1) period = 1;
    if (period > PERIOD_MAX) period = PERIOD_MAX;

    PSG_W(v == V_MUS ? R_A_LO : R_B_LO, (unsigned char)(period & 0xFF));
    PSG_W(v == V_MUS ? R_A_HI : R_B_HI, (unsigned char)((period >> 8) & 0x0F));
    mixer &= (unsigned char)~(v == V_MUS ? 0x01 : 0x02);
    PSG_W(R_MIXER, mixer);
    PSG_W(v == V_MUS ? R_A_VOL : R_B_VOL, VOLUME);
}

void snd_init(void)
{
    mixer = MIX_BASE | 0x03;
    PSG_W(R_MIXER, mixer);
    PSG_W(R_A_VOL, 0);
    PSG_W(R_B_VOL, 0);
    last_200 = now_200();
    acc = 0;
}

void snd_off(void)
{
    voice_off(V_MUS);
    voice_off(V_SFX);
    mus_on = sfx_on = 0;
}

void snd_music_data(unsigned int base, unsigned char ok)
{
    mus_base = base;
    mus_ok = ok;
}

void snd_music(uint8_t track)
{
    if (!mus_ok || track == MUS_NONE || track >= MUS_COUNT) {
        mus_on = 0;
        voice_off(V_MUS);
        return;
    }
    mus_head = (unsigned int)(mus_base + mus_offset[track]);
    mus = mus_head;
    mus_on = 1;
    note_left = 0;              /* zero forces the first note on the next tick */
}

void snd_effect(uint8_t track)
{
    if (!enabled || !mus_ok || track >= MUS_COUNT)
        return;
    sfx = (unsigned int)(mus_base + mus_offset[track]);
    sfx_on = 1;
    sfx_left = 0;
}

/* Blocking, and a quarter of a second, exactly as sid.c does it: a player
   reading the refusal message will not notice, and making it non-blocking
   would mean the effects voice carrying state across commands. 440Hz is 44 in
   the tenths the track format uses, so it needs no arithmetic of its own, and
   250ms is 50 ticks of the 200Hz counter with no region to ask about -- which
   the Amiga and the C128 both have to. */
#define BEEP_TENTHS 44
#define BEEP_TICKS  50

void snd_beep(void)
{
    long t0;

    if (!enabled)
        return;
    sfx_on = 0;                 /* a refusal cancels whatever else was playing */
    voice_note(V_SFX, BEEP_TENTHS);
    t0 = now_200();
    while (now_200() - t0 < BEEP_TICKS)
        ;
    voice_off(V_SFX);
}

/* One 18.2065Hz tick -- the same function as sid.c's, reading the same bytes
   out of the same file. */
static void music_tick(void)
{
    unsigned char note[2];

    if (mus_on) {
        if (note_left) note_left--;
        if (!note_left) {
            far_read(mus, note, 2);
            if (note[0] == 0) {          /* zero duration ends the track */
                mus = mus_head;          /* and this port loops until stopped */
                far_read(mus, note, 2);
                if (note[0] == 0) { mus_on = 0; voice_off(V_MUS); return; }
            }
            note_left = note[0];
            voice_note(V_MUS, note[1]);
            mus += 2;
        }
    }

    if (sfx_on) {
        if (sfx_left) sfx_left--;
        if (!sfx_left) {
            far_read(sfx, note, 2);
            if (note[0] == 0) { sfx_on = 0; voice_off(V_SFX); return; }
            sfx_left = note[0];
            voice_note(V_SFX, note[1]);
            sfx += 2;
        }
    }
}

void snd_poll(void)
{
    long now, elapsed;

    if (!enabled || !mus_ok)
        return;

    now = now_200();
    if (now < last_200) last_200 = now;    /* the counter was set back */
    elapsed = now - last_200;
    if (!elapsed)
        return;
    last_200 = now;

    /* 18.2065 ticks a second against 200 of these, in integers: every 200th
       is 18207/200000 of a tick. Catching up rather than dropping what was
       missed, so a slow disk load does not lose the beat -- but capped,
       because a briefing that took ten seconds should not then play ten
       seconds of music at once. */
    if (elapsed > 100) elapsed = 100;
    acc += (unsigned long)elapsed * 18207UL;
    while (acc >= 200000UL) {
        acc -= 200000UL;
        music_tick();
    }
}

uint8_t snd_enabled(void) { return enabled; }

void snd_toggle(void)
{
    enabled = (unsigned char)!enabled;
    if (!enabled) {
        voice_off(V_MUS);
        voice_off(V_SFX);
    }
}
