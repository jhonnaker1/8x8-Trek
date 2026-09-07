/* Sound for the Amiga: Paula, four channels of sampled audio.
 *
 * THE FURTHEST FROM THE SID OF ANY TARGET, and the reason this went last.
 * Paula has no tone generator: a channel plays back a buffer in CHIP RAM on a
 * loop, so "play a note" means having a waveform and choosing a PERIOD -- the
 * number of clock ticks between samples -- rather than writing a frequency.
 *
 * A 16-STEP SQUARE WAVE, because that is what the original made. EGA Trek on
 * a PC is one square wave out of the speaker, sid.c says "50% pulse, the
 * square wave the original actually made", and the MEGA65 and X16 follow it.
 * Sixteen steps rather than two so the period stays in Paula's usable range:
 * with a 2-byte wave, period = clock / (2 * freq), which for the middle of the
 * music is a number the hardware handles badly. See the clamp in voice_note().
 *
 * THE DATA IS THE SAME ON EVERY PORT and needs no byte-swapping, unlike
 * STRINGS.DAT: MUSIC.DAT is PAIRS OF BYTES -- duration in ticks, frequency in
 * tens of Hz, (0,0) ending a track -- and the track index is a generated C
 * array in the binary rather than a header in the file. Checked, not assumed,
 * because this is the second file this big-endian machine reads.
 *
 * TIMING COMES FROM A CLOCK, NOT FROM A CALL COUNT. The tracks are written in
 * ticks of the PC's original 18.2065Hz timer. It would be easy to tick once
 * per snd_poll() -- the key loop calls it about fifty times a second -- but
 * snd_poll() is also called from places that run at no fixed rate, so the
 * tempo would wander with what the game happened to be doing. DateStamp()
 * counts fiftieths of a second and is what the tick is derived from. THE
 * PREVIOUS TWO PORTS BOTH LOST TIME TO A TEMPO BUG (the C128 to a driver
 * three semitones from its cause, the MEGA65 to a raster that wraps twice a
 * frame), which is why this one reads a clock.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <graphics/gfxbase.h>
#include <hardware/custom.h>
#include <hardware/dmabits.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>

#include "../../c128/src/sid.h"
#include "../../core/farmem.h"
#include "music_data.h"

extern volatile struct Custom custom;
extern struct GfxBase *GfxBase;

/* Voice 0 is music and voice 1 is effects, the same split as the SID port's
   V1 and V2 -- so a laser does not cut the music off. */
#define V_MUS  0
#define V_SFX  1
#define VOLUME 48                  /* of 64; loud enough, short of harsh */

/* Paula's colour-clock rates. The music is written in Hz, so which machine
   this is decides the period for a given note. */
#define CLK_PAL   3546895UL
#define CLK_NTSC  3579545UL

/* SIXTEEN BYTES, one cycle of a square wave. Paula's minimum period is 124;
   a shorter one asks the DMA for samples faster than it can fetch them. With
   16 samples the highest note this can reach is clock/(124*16), about 1.8kHz,
   and the highest the music actually uses is 1480Hz -- so the range fits,
   with the clamp in voice_note() as the guard rather than the plan. */
#define WAVE_LEN 16
static const BYTE square[WAVE_LEN] = {
     64, 64, 64, 64, 64, 64, 64, 64,
    -64,-64,-64,-64,-64,-64,-64,-64
};
static BYTE *wave;                 /* the copy in CHIP RAM, which is the only
                                      memory the custom chips' DMA can reach */

uint8_t snd_region = REGION_PAL;   /* corrected by snd_init() */

static unsigned char enabled = 1;
static unsigned char mus_ok;
static unsigned int  mus_base;

static unsigned char mus_on, sfx_on;
static unsigned int  mus, mus_head, sfx;
static unsigned char note_left, sfx_left;

/* Fiftieths of a second, from DateStamp, and the accumulator that turns them
   into 18.2065Hz ticks. */
static ULONG last_50;
static ULONG acc;

static ULONG now_50(void) {
    struct DateStamp ds;
    DateStamp(&ds);
    /* Minutes and ticks together, so the count does not go backwards when
       ds_Tick wraps at the top of a minute. */
    return (ULONG)ds.ds_Minute * 3000UL + (ULONG)ds.ds_Tick;
}

static void voice_off(unsigned char v) {
    custom.dmacon = (UWORD)(v == V_MUS ? DMAF_AUD0 : DMAF_AUD1);
    custom.aud[v].ac_vol = 0;
}

/* `tenths` is the track format's frequency: tens of Hz, 0 for a rest. */
static void voice_note(unsigned char v, unsigned char tenths) {
    ULONG clk = (snd_region == REGION_PAL) ? CLK_PAL : CLK_NTSC;
    ULONG period;

    if (!enabled || !wave || tenths == 0) { voice_off(v); return; }

    period = clk / ((ULONG)tenths * 10UL * WAVE_LEN);
    if (period < 124) period = 124;        /* Paula's floor -- see above */
    if (period > 65535UL) period = 65535UL;

    custom.aud[v].ac_ptr = (UWORD *)wave;
    custom.aud[v].ac_len = WAVE_LEN / 2;   /* Paula counts WORDS */
    custom.aud[v].ac_per = (UWORD)period;
    custom.aud[v].ac_vol = VOLUME;
    custom.dmacon = (UWORD)(DMAF_SETCLR | (v == V_MUS ? DMAF_AUD0 : DMAF_AUD1));
}

void snd_init(void) {
    unsigned char i;

    /* REGION IS READ, NOT ASSUMED. graphics.library knows which display this
       machine came up in, and it decides both the period for a given note and
       how many frames a quarter-second beep is. */
    snd_region = (GfxBase->DisplayFlags & PAL) ? REGION_PAL : REGION_NTSC;

    wave = (BYTE *)AllocMem(WAVE_LEN, MEMF_CHIP);
    if (wave)
        for (i = 0; i < WAVE_LEN; i++) wave[i] = square[i];

    custom.dmacon = DMAF_SETCLR | DMAF_MASTER;
    voice_off(V_MUS);
    voice_off(V_SFX);
    last_50 = now_50();
}

void snd_off(void) {
    voice_off(V_MUS);
    voice_off(V_SFX);
    mus_on = sfx_on = 0;
    if (wave) { FreeMem(wave, WAVE_LEN); wave = NULL; }
}

void snd_music_data(unsigned int base, unsigned char ok) {
    mus_base = base;
    mus_ok = ok;
}

void snd_music(uint8_t track) {
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

void snd_effect(uint8_t track) {
    if (!enabled || !mus_ok || track >= MUS_COUNT) return;
    sfx = (unsigned int)(mus_base + mus_offset[track]);
    sfx_on = 1;
    sfx_left = 0;
}

/* Blocking, and a quarter of a second, exactly as sid.c does it: a player
   reading the refusal message will not notice, and making it non-blocking
   would mean the effects voice carrying state across commands. 440Hz is 44 in
   the tenths the track format uses, so it needs no arithmetic of its own. */
#define BEEP_TENTHS 44
void snd_beep(void) {
    unsigned char n = (snd_region == REGION_PAL) ? 13 : 15;

    if (!enabled) return;
    sfx_on = 0;                 /* a refusal cancels whatever else was playing */
    voice_note(V_SFX, BEEP_TENTHS);
    while (n--) WaitTOF();
    voice_off(V_SFX);
}

/* One 18.2065Hz tick -- the same function as sid.c's, reading the same bytes
   out of the same file. */
static void music_tick(void) {
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

void snd_poll(void) {
    ULONG now, elapsed;

    if (!enabled || !mus_ok) return;

    now = now_50();
    if (now < last_50) last_50 = now;      /* midnight, or a clock set back */
    elapsed = now - last_50;
    if (!elapsed) return;
    last_50 = now;

    /* 18.2065 ticks per second against 50 of these, in integers: every
       fiftieth is 18207/50000 of a tick. Catching up rather than dropping
       what was missed, so a slow disk load does not lose the beat -- but
       capped, because a briefing that took ten seconds should not then play
       ten seconds of music at once. */
    if (elapsed > 25) elapsed = 25;
    acc += elapsed * 18207UL;
    while (acc >= 50000UL) {
        acc -= 50000UL;
        music_tick();
    }
}

uint8_t snd_enabled(void) { return enabled; }

void snd_toggle(void) {
    enabled = (unsigned char)!enabled;
    if (!enabled) {
        voice_off(V_MUS);
        voice_off(V_SFX);
    }
}
