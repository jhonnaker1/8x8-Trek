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
 * same encoding all five ports read.
 *
 * ~~VERA wants Hz * 2^25 / 25e6, i.e. tens-of-Hz * 13.42177.~~ **IT WANTS
 * 2^26, AND THIS PORT WAS AN OCTAVE FLAT FROM v0.10.0 TO 2026-09-10.** VERA's
 * PSG accumulator is SEVENTEEN bits, clocked at 25MHz/512 = 48828.125Hz, so
 * the law is Hz * 2^26 / 25e6 = tens-of-Hz * 26.84355. The old constant put
 * every note -- music, effects and the refusal beep -- an octave below every
 * other port, playing the same MUSIC.DAT.
 *
 * MEASURED, not re-derived from the same documentation that produced the
 * error: x16emu records a WAV, and two reference tones the driver believed
 * were 440Hz and 880Hz came back at 219.9 and 439.9, both exactly x0.500. TWO
 * references and not one, because one cannot separate a SCALE from an OFFSET.
 * With the constant below they read 439.9 at x1.000. See `make run-sndtest`,
 * `tools/sndtest.py`, and NOTES.md.
 *
 * Staged as whole and fractional parts so no intermediate leaves 16 bits,
 * exactly as sidfreq.h does for the SID: the widest case is 255 * 216 =
 * 55,080, and the widest word 6,845. The music's own top note is 93 tenths,
 * giving 2,496.
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

/* tens-of-Hz -> VERA frequency word. 26 + 216/256 = 26.8438, against the
   exact 26.84355. Was 13 + 108/256, which is half of it -- see the header. */
#define VERA_WHOLE 26
#define VERA_FRAC 216

#define VOL_ON  0xC0            /* pan = both speakers, volume in bits 0-5 */
/* PSG register 3 is [waveform:2][pulse width:6], and duty is (width+1)/64.
   ~~0x00: pulse, 50%~~ -- **0x00 SELECTS THE NARROWEST PULSE THERE IS, and
   this port played a 1/64 spike from v0.10.0 to 2026-09-10.** Measured off
   x16emu's recording at 0.9% duty, against the 50% this line claimed.

   AND THE DUTY LAW IS (width+1)/128, NOT /64, which is why 0x1F went in first
   and measured 24.9%. Swept it instead of arguing with the documentation:
   width 31 -> 24.9%, 47 -> 37.2%, 63 -> 49.7%, all three matching (w+1)/128 to
   a third of a percent. So 0x3F is the square a PC speaker actually makes --
   which is what the rest of this comment was always about. */
#define WAVE    0x3F            /* pulse, 50% -- the PC speaker the original
                                   used had no envelope either */
#define VOLUME  40

static uint8_t enabled = 1;
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

/* THE FRAME TICK, AND THE JIFFY CLOCK WAS THE WRONG CHOICE.
 *
 * This paced off RDTIM ($FFDE) on the reasoning that a jiffy clock is
 * unambiguous where a raster counter is not. MEASURED ON THE MACHINE after
 * Jamie reported no music and a freeze: RDTIM returns A=X=Y=00 forever, before
 * or after a CLI. The X16 KERNAL's jiffy is not maintained for a program that
 * has taken the machine over, and VERA's VSYNC flag never sets either -- 0
 * times in 150,000 polls.
 *
 * That one dead clock explains both faults. snd_poll() saw zero elapsed frames
 * so the music never ticked, and snd_beep() spun waiting for a jiffy that
 * would never move -- with the voice already ON, which is the buzzing.
 *
 * What DOES tick without interrupts is VERA's LINE flag, ISR bit 1: it sets
 * when the raster reaches IRQ_LINE, once a frame, and stays set until written
 * back. Measured re-arming 7 times across the same loop that saw VSYNC zero.
 * The raw scanline at $9F28 also moves, but its low byte wraps TWICE per frame
 * at 480 lines -- which is precisely the bug that cost the MEGA65 a day, so it
 * is not used. */
#define VERA_ISR (*(volatile unsigned char *)0x9F27)
#define ISR_LINE 0x02

static unsigned char frame_tick(void) {
    if (VERA_ISR & ISR_LINE) { VERA_ISR = ISR_LINE; return 1; }
    return 0;
}

#ifdef TREK_SND_TEST
/* A REFERENCE TONE WITH A KNOWN ANSWER, for src/sndtest.c only.
 *
 * Measuring the beep is worthless if the instrument is not known good: a
 * number out of a WAV is only evidence once the same path has produced a
 * frequency somebody can predict. This plays any note through the SAME
 * voice_note() the beep uses, so `44` must come out at 440Hz -- and if it does
 * not, the analysis is wrong and the beep reading means nothing. Never
 * compiled into the game; see the sndtest target. */
void snd_test_note(unsigned char tenths) { voice_note(V_SFX, tenths); }
void snd_test_off(void) { voice_off(V_SFX); }

/* HOLD A NOTE FOR N FRAME TICKS, to calibrate what a frame tick is worth.
 *
 * snd_beep() says six ticks and the recording says 80ms, which is neither
 * 6/60 nor 6/50. Two candidates: the ISR LINE flag is already set when the
 * loop starts, so N ticks span N-1 intervals; or the shipping guard runs out
 * first. This separates them -- the guard here is four million, far past
 * either, so what comes back is the TICK. */
void snd_test_frames(unsigned char tenths, unsigned char n) {
    unsigned long guard = 0;
    unsigned char f = 0;
    voice_note(V_SFX, tenths);
    while (f < n && ++guard < 4000000UL) { if (frame_tick()) f++; }
    voice_off(V_SFX);
}
#endif

void snd_init(void) {
    unsigned char v;
    for (v = 0; v < 16; v++) { psg(v, 2, 0); }   /* silence every voice */
    VERA_ISR = ISR_LINE;                         /* start from a known state */
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

/* BOUNDED, and that is not defensive programming for its own sake: the
   unbounded version of this loop is what froze the machine with a voice
   sounding. A beep that ends early is a blemish; one that never ends is the
   bug Jamie hit. The spin cap is generous -- far longer than six frames -- and
   only reached if the frame source dies again. */
/* THE REFUSAL BEEP. Three things were wrong with this and only one was the
 * pitch; all three were measured on 2026-09-10 rather than reasoned about.
 *
 * IT PLAYED `voice_note(V_SFX, 20)` -- 200Hz, where the original is MEASURED
 * at 440Hz and the other four ports play that. With the octave error still in
 * place it came out of VERA at 99.9Hz.
 *
 * IT COUNTED SIX TICKS AND GOT FIVE FRAMES. `snd_init` clears the ISR LINE
 * flag, but by the time a refusal happens it is set again, so the first
 * `frame_tick()` returned immediately and N ticks spanned N-1 intervals.
 * Measured as an INTERCEPT: holds of 6, 15 and 30 ticks came back at 85.2,
 * 235.9 and 487.6ms, a slope of 16.77ms (59.6Hz, so the frame rate was never
 * in doubt) and an intercept of -15.4ms -- one free tick. Clearing the flag
 * here is what makes fifteen ticks mean fifteen frames.
 *
 * AND THE GUARD WOULD HAVE TRUNCATED THE FIX. It bounded the WHOLE beep at
 * 65,536 spins, and 65,536 spins is about 185ms on this machine -- measured
 * from the silences between bursts in the same recording, 739ms for four of
 * them. That is fine for a 84ms beep and short of a 251ms one, so changing 6
 * to 15 alone would have produced a beep cut off by its own guard. It bounds
 * ONE FRAME now and resets on every tick: ~6,000 iterations needed against
 * 65,536 allowed, and a beep of any length is safe. */
#define BEEP_TENTHS 44          /* 440Hz, the A the other four ports beep */
#define BEEP_FRAMES 15          /* 251.6ms at the measured 16.77ms/frame,
                                   against the original's measured 250.6 */

void snd_beep(void) {
    unsigned char frames = 0;
    unsigned int guard = 0;
    if (!enabled) return;
    sfx_on = 0;                    /* a refusal cancels whatever was playing */
    VERA_ISR = ISR_LINE;           /* or the first tick is free -- see above */
    voice_note(V_SFX, BEEP_TENTHS);
    while (frames < BEEP_FRAMES) {
        if (frame_tick())      { frames++; guard = 0; }
        else if (++guard == 0) { break; }   /* no frame in 65,536 spins */
    }
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
    if (!enabled || (!mus_on && !sfx_on)) return;
    if (!frame_tick()) return;

    acc += snd_tick_num(REGION_NTSC);
    while (acc >= SND_TICK_DEN) { acc -= SND_TICK_DEN; music_tick(); }
}
