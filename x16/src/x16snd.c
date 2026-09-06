/* Sound for the Commander X16: VERA's PSG, and a jiffy clock instead of a
 * raster.
 *
 * THE PACING IS THE PART THAT ALWAYS GOES WRONG, so it is the part done
 * differently. Both existing ports detect a frame by watching a raster counter
 * go BACKWARDS, and the MEGA65 lost a day to exactly that: its VIC-II
 * compatible raster wraps 0..311 TWICE per frame on a 625-line display, so
 * every frame counted as two and the music ran at double speed.
 *
 * The X16 KERNAL keeps a jiffy clock, and RDTIM ($FFDE) reads it: A=high,
 * X=mid, Y=low, incrementing 60 times a second. Comparing the low byte gives
 * an unambiguous frame count with no wrap to misread -- and taking the
 * DIFFERENCE rather than testing for change means a poll that misses a frame
 * catches up instead of losing tempo.
 *
 * NO REGION DETECTION, AND THAT IS A PLATFORM FACT RATHER THAN A SHORTCUT --
 * which is worth saying plainly, because m65snd.c's header records that this
 * same file once claimed "NO REGION DETECTION" for the MEGA65 on reasoning
 * that turned out to be wrong. There the argument was about the SID clock and
 * so about PITCH, while snd_tick_num() converts FRAMES to the original's
 * 18.2Hz ticks -- and frames are 50 a second on PAL against 60 on NTSC. Using
 * the NTSC numerator on a PAL machine ran the music 19% fast.
 *
 * The X16 cannot be in that position: VERA drives VGA or NTSC composite and
 * has NO 50Hz mode at all -- x16emu's own video modes are `@vga` and `@ntsc`
 * and nothing else. So 60 frames a second is the only case, and REGION_NTSC
 * below is the right constant rather than a default nobody checked. If VERA
 * ever gains a 50Hz output, this is the line that breaks, and the fix is to
 * measure the rate rather than to add a region flag.
 *
 * VERA'S PSG lives in VRAM at $1F9C0, four bytes per voice: frequency low,
 * frequency high, volume with pan in the top two bits, then waveform with
 * pulse width. A note byte in MUSIC.DAT is a frequency in TENS OF HZ -- the
 * same encoding all three ports read -- and VERA wants Hz * 2^25 / 25e6, i.e.
 * tens-of-Hz * 13.42177. Staged as whole and fractional parts so no
 * intermediate leaves 16 bits, exactly as sidfreq.h does for the SID: the
 * widest case is 255 * 108 = 27,540.
 */
#include <stdint.h>
#include "../../c128/src/sid.h"
/* sidfreq.h mixes TWO things: the SID's frequency table and the ORIGINAL's
   tempo. This port needs only the tempo -- snd_tick_num()/SND_TICK_DEN are a
   property of Anderson's game, not of any machine -- so it is included whole
   and sid_freq() goes unused here. Copying the two tempo constants instead
   would be a second source of truth for a number the C128's test_sid.c already
   checks against double precision, which is the worse trade. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "../../c128/src/sidfreq.h"
#pragma clang diagnostic pop
#include "../../c128/src/music_data.h"
#include "../../core/farmem.h"

#define VERA_ADDR_L (*(volatile unsigned char *)0x9F20)
#define VERA_ADDR_M (*(volatile unsigned char *)0x9F21)
#define VERA_ADDR_H (*(volatile unsigned char *)0x9F22)
#define VERA_DATA0  (*(volatile unsigned char *)0x9F23)
#define VERA_CTRL   (*(volatile unsigned char *)0x9F25)

#define PSG_BASE 0x1F9C0UL
#define V_MUSIC  0              /* voice 0 carries the tune   */
#define V_SFX    1              /* voice 1 carries the effect */

/* tens-of-Hz -> VERA frequency word. 13 + 108/256 = 13.4219, against the
   exact 13.42177. */
#define VERA_WHOLE 13
#define VERA_FRAC 108

#define VOL_ON  0xC0            /* pan = both speakers, volume in bits 0-5 */
#define WAVE    0x00            /* pulse, 50% -- the PC speaker the original
                                   used had no envelope either */
#define VOLUME  40

static uint8_t enabled = 1;
static unsigned char last_jiffy;
static unsigned int  acc;
static unsigned int  mus, mus_head, mus_base;
static unsigned char mus_on, mus_ok, note_left;
static unsigned int  sfx;
static unsigned char sfx_on, sfx_left;

static void psg(unsigned char voice, unsigned char reg, unsigned char val) {
    unsigned long a = PSG_BASE + (unsigned long)voice * 4 + reg;
    VERA_CTRL   = 0;
    VERA_ADDR_L = (unsigned char)(a & 0xFF);
    VERA_ADDR_M = (unsigned char)((a >> 8) & 0xFF);
    VERA_ADDR_H = (unsigned char)(((a >> 16) & 1) | 0x10);
    VERA_DATA0  = val;
}

static void voice_off(unsigned char v) { psg(v, 2, 0); }   /* volume to zero */

static void voice_note(unsigned char v, unsigned char tenths) {
    unsigned int f;
    if (tenths == 0) { voice_off(v); return; }             /* a rest */
    f = (unsigned int)((unsigned int)tenths * VERA_WHOLE)
      + (unsigned int)((((unsigned int)tenths * VERA_FRAC) + 128) >> 8);
    psg(v, 0, (unsigned char)(f & 0xFF));
    psg(v, 1, (unsigned char)(f >> 8));
    psg(v, 3, WAVE);
    psg(v, 2, (unsigned char)(VOL_ON | VOLUME));
}

/* RDTIM: A=high, X=mid, Y=low. The low byte is enough -- it wraps every 256
   jiffies, about 4.3 seconds, and this is polled far faster than that. */
static unsigned char jiffy(void) {
    unsigned char y;
    __asm__ volatile("jsr $FFDE\n sty %0\n" : "=r"(y) :: "a", "x", "y");
    return y;
}

void snd_init(void) {
    unsigned char v;
    for (v = 0; v < 16; v++) { psg(v, 2, 0); }   /* silence every voice */
    last_jiffy = jiffy();
    acc = 0;
}

void snd_off(void) { voice_off(V_MUSIC); voice_off(V_SFX); mus_on = 0; sfx_on = 0; }
uint8_t snd_enabled(void) { return enabled; }
void snd_toggle(void) { enabled = (uint8_t)!enabled; if (!enabled) snd_off(); }

void snd_music_data(unsigned int base, unsigned char ok) {
    mus_base = base;
    mus_ok = ok;
}

void snd_music(uint8_t track) {
    if (!mus_ok || track == MUS_NONE || track >= MUS_COUNT) {
        mus_on = 0; voice_off(V_MUSIC); return;
    }
    mus_head = (unsigned int)(mus_base + mus_offset[track]);
    mus = mus_head;
    mus_on = 1;
    note_left = 0;                 /* zero forces the first note next tick */
    acc = 0;
}

void snd_effect(uint8_t track) {
    if (!enabled || !mus_ok || track >= MUS_COUNT) return;
    sfx = (unsigned int)(mus_base + mus_offset[track]);
    sfx_on = 1;
    sfx_left = 0;
}

void snd_beep(void) {
    unsigned char start;
    if (!enabled) return;
    sfx_on = 0;                    /* a refusal cancels whatever was playing */
    voice_note(V_SFX, 20);
    start = jiffy();
    while ((unsigned char)(jiffy() - start) < 6) { }
    voice_off(V_SFX);
}

/* Two bytes out of far memory, and ONLY when a note ends -- a handful of times
   a second, not once per tick. Each read crosses the bank window, which is
   ruinous in a loop and nothing at this rate. */
static void music_tick(void) {
    unsigned char note[2];

    if (mus_on) {
        if (note_left) note_left--;
        if (!note_left) {
            far_read(mus, note, 2);
            if (note[0] == 0) {                /* zero duration ends a track */
                mus = mus_head;                /* and it loops until stopped */
                far_read(mus, note, 2);
                if (note[0] == 0) { mus_on = 0; voice_off(V_MUSIC); return; }
            }
            note_left = note[0];
            voice_note(V_MUSIC, note[1]);
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
    unsigned char now, elapsed;

    if (!enabled || (!mus_on && !sfx_on)) return;

    now = jiffy();
    elapsed = (unsigned char)(now - last_jiffy);
    if (!elapsed) return;
    last_jiffy = now;

    /* Every elapsed frame is accounted for, not just the fact that one was.
       A poll that arrives late catches up rather than dropping tempo. */
    while (elapsed--) {
        acc += snd_tick_num(REGION_NTSC);
        while (acc >= SND_TICK_DEN) { acc -= SND_TICK_DEN; music_tick(); }
    }
}
