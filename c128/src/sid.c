#include "sid.h"
#include "../../core/farmem.h"
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
/* Offsets into far memory, not pointers: the notes live in bank 1 (see
   core/farmem.h) and are not addressable. A separate flag says whether a
   track is playing, because offset 0 is a fine place for one to start. */
static unsigned int  mus;             /* far offset of the next note pair */
static unsigned int  mus_head;        /* for the loop back */
static unsigned char mus_on;
static unsigned int  mus_base;        /* where MUSIC.DAT landed */
static unsigned char mus_ok;
static unsigned int  acc;             /* tempo accumulator, see sidfreq.h */
static unsigned char note_left;       /* original ticks left on this note */

/* Effects voice state -- same shape, no looping. */
static unsigned int  sfx;
static unsigned char sfx_on;
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

/* The raster line, whole and trustworthy.
 *
 * $D012 is only the LOW BYTE. Bit 8 lives in $D011, and they are separate
 * reads with the raster moving between them, so the naive combination sees
 * line 510 on a machine that has 263 whenever it straddles the 255-to-256
 * crossing. Reading $D011 either side and retrying if bit 8 moved costs an
 * occasional sample and can never return a wrong number.
 *
 * Everything below is built on this, because using the bare low byte is wrong
 * in two different ways and both of them bit this file:
 *
 *   - As a LINE NUMBER it can be 256 too high. That made region detection
 *     answer PAL on every machine and every note played 3.8% sharp.
 *   - As a FRAME MARKER it goes backwards TWICE per frame -- once at the
 *     255-to-256 crossing and again at the real wrap -- so "it decreased"
 *     means half a frame, sometimes. Whether a caller sees one or both
 *     depends on how fast it polls, which made the music tempo right by luck
 *     in the key loop and the beep half-length in a tight one.
 */
static unsigned int raster_line(void) {
    unsigned char c1, c2, r;
    for (;;) {
        c1 = VIC_CTRL1;
        r  = VIC_RASTER;
        c2 = VIC_CTRL1;
        if ((c1 & 0x80) == (c2 & 0x80))
            return (unsigned int)r + ((c1 & 0x80) ? 256u : 0u);
    }
}

/* PAL has 312 raster lines and NTSC 263, so the highest line seen over a few
   frames tells them apart with a threshold anywhere in between. MEASURED at
   both clock speeds on both machines before raster_line() existed, by the low
   byte while bit 8 was set: PAL 55 and 55, NTSC 0 and 6. Nothing lands near
   the middle. */
#define PAL_LINE_MIN 300

static unsigned char detect_region(void) {
    unsigned int spins;
    for (spins = 0; spins < 30000u; spins++)
        if (raster_line() >= PAL_LINE_MIN) return REGION_PAL;
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
    mus_on = 0;
    sfx_on = 0;
    acc = 0;
    last_raster = raster_line();
}

void snd_off(void) {
    unsigned char i;
    mus_on = 0;
    sfx_on = 0;
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

void snd_music_data(unsigned int base, unsigned char ok) {
    mus_base = base;
    mus_ok = ok;
}

void snd_music(uint8_t track) {
    if (!mus_ok || track == MUS_NONE || track >= MUS_COUNT) {
        mus_on = 0;
        voice_off(V1);
        return;
    }
    mus_head = (unsigned int)(mus_base + mus_offset[track]);
    mus = mus_head;
    mus_on = 1;
    note_left = 0;               /* zero forces the first note on next tick */
    acc = 0;
}

void snd_effect(uint8_t track) {
    if (!enabled || !mus_ok || track >= MUS_COUNT) return;
    sfx = (unsigned int)(mus_base + mus_offset[track]);
    sfx_on = 1;
    sfx_left = 0;
}

/* Wait n frames. A frame has passed when the whole line number goes backwards,
   which happens exactly once per frame however fast this is called. */
static void wait_frames(unsigned char n) {
    unsigned int last = raster_line();
    unsigned int r;
    while (n) {
        r = raster_line();
        if (r < last) n--;
        last = r;
    }
}

/* The refusal beep.
 *
 * MEASURED: Sound(440); Delay(250); NoSound, called from seven places in the
 * original -- two in the laser dialog and five in the torpedo dialog, all of
 * them refusals: no torpedoes, tubes damaged, not enough energy. It is the
 * sound of the ship declining an order.
 *
 * 440Hz is 44 in the tenths the track data uses, so this needs no arithmetic
 * of its own -- it goes through the same sid_freq() every note does.
 *
 * Blocking, like the original's Delay(). A quarter second is short enough that
 * a player reading the refusal message will not notice, and making it
 * non-blocking would mean the effects voice carrying state across commands. */
#define BEEP_TENTHS      44
#define BEEP_FRAMES_NTSC 15    /* 250.6ms at 59.826Hz */
#define BEEP_FRAMES_PAL  13    /* 259.4ms at 50.125Hz -- 13 is the nearer whole
                                  frame; 12 would be 239ms */

void snd_beep(void) {
    if (!enabled) return;
    sfx_on = 0;                /* a refusal cancels whatever else was playing */
    voice_note(V2, BEEP_TENTHS);
    wait_frames(snd_region == REGION_PAL ? BEEP_FRAMES_PAL : BEEP_FRAMES_NTSC);
    voice_off(V2);
}

/* One original 18.2065Hz tick. */
static void music_tick(void) {
    if (mus_on) {
        if (note_left) note_left--;
        if (!note_left) {
            /* Two bytes out of far memory, and ONLY when a note ends -- a
               handful of times a second, not once per tick. That is what
               makes the notes affordable in bank 1: each read is a pair of
               KERNAL calls, ruinous in a loop and nothing at this rate. */
            unsigned char note[2];

            far_read(mus, note, 2);
            if (note[0] == 0) {             /* zero duration ends the track */
                mus = mus_head;             /* the original loops 99 times;
                                               this loops until told to stop */
                far_read(mus, note, 2);
                if (note[0] == 0) { mus_on = 0; voice_off(V1); return; }
            }
            note_left = note[0];
            voice_note(V1, note[1]);
            mus += 2;
        }
    }

    if (sfx_on) {
        if (sfx_left) sfx_left--;
        if (!sfx_left) {
            unsigned char note[2];

            far_read(sfx, note, 2);
            if (note[0] == 0) { sfx_on = 0; voice_off(V2); return; }
            sfx_left = note[0];
            voice_note(V2, note[1]);
            sfx += 2;
        }
    }
}

void snd_poll(void) {
    unsigned int r;

    if (!enabled || (!mus && !sfx)) return;

    /* A frame has passed when the raster counter goes backwards. That works
       whatever rate this is called at, so long as it is more than twice a
       frame -- which the key poll loop manages by orders of magnitude at 2MHz
       -- and it needs no interrupt of our own. Hooking one would have meant
       taking the IRQ away from a KERNAL this port already goes behind the back
       of in kb_waitkey(). */
    r = raster_line();
    if (r >= last_raster) { last_raster = r; return; }
    last_raster = r;

    acc += snd_tick_num(snd_region);
    while (acc >= SND_TICK_DEN) {
        acc -= SND_TICK_DEN;
        music_tick();
    }
}
