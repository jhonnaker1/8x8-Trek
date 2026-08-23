#include "sid.h"
#include "sidfreq.h"

#define SID       ((volatile unsigned char *)0xD400)
#define VIC_RASTER (*(volatile unsigned char *)0xD012)
#define VIC_CTRL1  (*(volatile unsigned char *)0xD011)

/* Voice register blocks. Voice 1 is music, voice 2 effects. */
#define V1 0
#define V2 7

/* A plain gate with no envelope shaping: attack 0, decay 0, sustain full,
   release 0. That is what a PC speaker does -- on or off -- and the point here
   is the original's tunes, not a synth patch. */
#define AD_FLAT 0x00
#define SR_FLAT 0xF0

/* 50% pulse, the square wave the original actually made. */
#define PW_LO   0x00
#define PW_HI   0x08
#define GATE_ON  0x41
#define GATE_OFF 0x40

uint8_t snd_region = REGION_NTSC;

static uint8_t enabled = 1;
static uint8_t last_raster;

/* Music voice state. */
static const unsigned char *mus;      /* NULL when nothing is playing */
static const unsigned char *mus_head; /* for the loop back */
static unsigned int  acc;             /* tempo accumulator, see sidfreq.h */
static unsigned char note_left;       /* original ticks left on this note */

/* Effects voice state -- same shape, no looping. */
static const unsigned char *sfx;
static unsigned char sfx_left;

static void voice_off(unsigned char v) {
    SID[v + 4] = GATE_OFF;
}

static void voice_note(unsigned char v, unsigned char tenths) {
    unsigned int f;
    if (tenths == 0) {          /* a rest: gate down, no new pitch */
        SID[v + 4] = GATE_OFF;
        return;
    }
    f = sid_freq(tenths, snd_region);
    SID[v + 0] = (unsigned char)(f & 0xFF);
    SID[v + 1] = (unsigned char)(f >> 8);
    SID[v + 4] = GATE_ON;
}

/* PAL or NTSC, off the VIC raster.
 *
 * The obvious version of this was wrong and it is worth saying how, because
 * the failure was silent and the symptom was three semitones away from the
 * cause. Reconstructing the full raster line means reading $D012 for the low
 * byte and $D011 for bit 8 -- two separate reads, with the raster moving
 * between them. When it crosses 255 to 256 in that gap, a stale low byte gets
 * 256 added to it and the routine sees line 510 on a machine that has 263.
 * Anything above 300 read as PAL, so it answered PAL on everything, and the
 * only evidence was that every note played 3.8% sharp -- which is exactly the
 * ratio of the two SID clocks.
 *
 * So this never reconstructs the line number at all. Lines 256..311 exist only
 * on PAL and 256..262 only on NTSC, so the LOW BYTE while bit 8 is set reaches
 * 55 on one and 6 on the other. MEASURED at both clock speeds on both
 * machines: PAL 55 and 55, NTSC 0 and 6. Nothing lands near the threshold.
 *
 * The sandwich read is what makes the sample trustworthy: $D011 either side of
 * $D012, and the sample is discarded if bit 8 moved across it. */
#define PAL_LOW_MIN 32

static unsigned char detect_region(void) {
    unsigned char c1, c2, r;
    unsigned char hi = 0;
    unsigned int spins;

    for (spins = 0; spins < 30000u; spins++) {
        c1 = VIC_CTRL1;
        r  = VIC_RASTER;
        c2 = VIC_CTRL1;
        if (c1 != c2) continue;          /* bit 8 moved -- the pair is unsafe */
        if (!(c1 & 0x80)) continue;      /* below line 256, tells us nothing */
        if (r > hi) hi = r;
        if (hi >= PAL_LOW_MIN) return REGION_PAL;
    }
    return REGION_NTSC;
}

void snd_init(void) {
    unsigned char i;

    for (i = 0; i < 25; i++) SID[i] = 0;
    SID[24] = 0x0F;                    /* volume, full */

    SID[V1 + 2] = PW_LO; SID[V1 + 3] = PW_HI;
    SID[V1 + 5] = AD_FLAT; SID[V1 + 6] = SR_FLAT;
    SID[V2 + 2] = PW_LO; SID[V2 + 3] = PW_HI;
    SID[V2 + 5] = AD_FLAT; SID[V2 + 6] = SR_FLAT;

    snd_region = detect_region();
    mus = 0;
    sfx = 0;
    acc = 0;
    last_raster = (unsigned char)VIC_RASTER;
}

void snd_off(void) {
    unsigned char i;
    mus = 0;
    sfx = 0;
    for (i = 0; i < 25; i++) SID[i] = 0;
}

uint8_t snd_enabled(void) { return enabled; }

void snd_toggle(void) {
    enabled = (uint8_t)!enabled;
    if (!enabled) {
        voice_off(V1);
        voice_off(V2);
    }
}

void snd_music(uint8_t track) {
    if (track == MUS_NONE || track >= MUS_COUNT) {
        mus = 0;
        voice_off(V1);
        return;
    }
    mus_head = mus_tracks[track];
    mus = mus_head;
    note_left = 0;               /* zero forces the first note on next tick */
    acc = 0;
}

void snd_effect(uint8_t track) {
    if (!enabled || track >= MUS_COUNT) return;
    sfx = mus_tracks[track];
    sfx_left = 0;
}

/* One original 18.2065Hz tick. */
static void music_tick(void) {
    if (mus) {
        if (note_left) note_left--;
        if (!note_left) {
            unsigned char dur = mus[0];
            if (dur == 0) {                 /* zero duration ends the track */
                mus = mus_head;             /* the original loops 99 times;
                                               this loops until told to stop */
                dur = mus[0];
                if (dur == 0) { mus = 0; voice_off(V1); return; }
            }
            note_left = dur;
            voice_note(V1, mus[1]);
            mus += 2;
        }
    }

    if (sfx) {
        if (sfx_left) sfx_left--;
        if (!sfx_left) {
            unsigned char dur = sfx[0];
            if (dur == 0) { sfx = 0; voice_off(V2); return; }
            sfx_left = dur;
            voice_note(V2, sfx[1]);
            sfx += 2;
        }
    }
}

void snd_poll(void) {
    unsigned char r;

    if (!enabled || (!mus && !sfx)) return;

    /* A frame has passed when the raster counter goes backwards. That works
       whatever rate this is called at, so long as it is more than twice a
       frame -- which the key poll loop manages by orders of magnitude at 2MHz
       -- and it needs no interrupt of our own. Hooking one would have meant
       taking the IRQ away from a KERNAL this port already goes behind the back
       of in kb_waitkey(). */
    r = (unsigned char)VIC_RASTER;
    if (r >= last_raster) { last_raster = r; return; }
    last_raster = r;

    acc += snd_tick_num(snd_region);
    while (acc >= SND_TICK_DEN) {
        acc -= SND_TICK_DEN;
        music_tick();
    }
}
