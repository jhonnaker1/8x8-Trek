/* Sound for the Atari 800XL: POKEY in 16-bit mode, and the OS's own frame
 * counter.
 *
 * EVERY NUMBER IN THIS FILE WAS MEASURED ON THE MACHINE, with tools/pokey.py
 * and src/sndprobe.c, because the pacing and the pitch are the two things this
 * project has got wrong by reasoning. The MEGA65 ran its music at double speed
 * off a raster that wraps twice a frame. The X16 paced off a jiffy clock that
 * returns zero for ever once a program has taken the machine over, and paced
 * its beep off it too, which froze the machine with a voice sounding.
 *
 * WHY 16-BIT, WHICH COSTS A DIVIDE. POKEY's 8-bit modes cannot carry this
 * tune. Measured against the actual note set in MUSIC.DAT -- 90 Hz to 930 Hz,
 * 25 distinct values:
 *
 *     8-bit, 64kHz base    bottoms out at 124.8 Hz.  Cannot reach 90 Hz.
 *     8-bit, 15kHz base    reaches everything, but is 5.51% sharp at 930 Hz
 *                          and over 1.5% out on six of the 25. A semitone is
 *                          5.95%, so the top of the melody is a semitone off.
 *     16-bit, 1.79MHz      worst error 0.032% across the whole set.
 *
 * The cost is one 32-bit divide per NOTE -- a handful of times a second, not
 * per tick -- and that is a trade this target can afford in time even though
 * it is the tightest in the project for space.
 *
 * TWO VOICES OUT OF FOUR CHANNELS. Joining 1+2 and 3+4 uses all of POKEY, and
 * two voices is exactly what this seam exists for: music on one, effects on
 * the other, so a hit during the title track does not chop the tune. The
 * original had one PC speaker and could not do that.
 */
#include <stdint.h>

#include "../../c128/src/sid.h"
/* sidfreq.h mixes TWO things: the SID's frequency table and the ORIGINAL's
   tempo. Only the tempo is wanted here -- snd_tick_num()/SND_TICK_DEN are a
   property of Anderson's game, not of any machine -- so it is included whole
   and sid_freq() goes unused, exactly as x16snd.c does it. Copying the two
   tempo constants instead would be a second source of truth for a number the
   C128's test_sid.c already checks against double precision. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "../../c128/src/sidfreq.h"
#pragma clang diagnostic pop
#include "../../c128/src/music_data.h"
#include "../../core/farmem.h"

#define AUDF1  (*(volatile unsigned char *)0xD200)
#define AUDC1  (*(volatile unsigned char *)0xD201)
#define AUDF2  (*(volatile unsigned char *)0xD202)
#define AUDC2  (*(volatile unsigned char *)0xD203)
#define AUDF3  (*(volatile unsigned char *)0xD204)
#define AUDC3  (*(volatile unsigned char *)0xD205)
#define AUDF4  (*(volatile unsigned char *)0xD206)
#define AUDC4  (*(volatile unsigned char *)0xD207)
#define AUDCTL (*(volatile unsigned char *)0xD208)
#define SKCTL  (*(volatile unsigned char *)0xD20F)
#define GTIA_PAL (*(volatile unsigned char *)0xD014)
#define RTCLOK (*(volatile unsigned char *)0x0014)   /* OS VBI frame counter */

/* AUDCTL: join 1+2 (bit4) and 3+4 (bit3), clock channel 1 (bit6) and channel 3
   (bit5) at 1.79MHz. Measured working as a pair of 16-bit voices. */
#define AUDCTL_TWO_VOICE 0x78

/* AUDC: distortion 5 in bits 5-7 is POKEY's pure tone -- confirmed by Altirra
   reporting a frequency at all for $A8, which it does not for a noise
   setting. Volume in bits 0-3; 8 of 15 leaves headroom for two voices to sum
   without clipping, which is the same reason the other ports do not run
   theirs flat out. */
#define TONE_VOL 0xA8
#define SILENT   0x00

/* AUDF = C/n - 7, MEASURED IN BOTH REGIONS by switching the emulator's video
   standard and reading Altirra's own computed frequency back:
 *
 *     PAL   $D014 = $01   base 1,773,448 Hz   C = 88672
 *     NTSC  $D014 = $0F   base 1,789,772 Hz   C = 89489
 *
 * `n` is the note in TENS of Hz, the encoding all five ports read, so C is the
 * base over twenty rather than over two. */
#define C_PAL   88672UL
#define C_NTSC  89489UL
#define AUDF_BIAS 7

uint8_t snd_region = REGION_NTSC;

static uint8_t enabled = 1;
static unsigned int  acc;
static unsigned int  mus, mus_head, mus_base;
static unsigned char mus_on, mus_ok, note_left;
static unsigned int  sfx;
static unsigned char sfx_on, sfx_left;
static unsigned char last_frame;

/* Voice 0 is channels 1+2 (music), voice 1 is channels 3+4 (effects). In a
   joined pair the LOW byte goes in the odd channel's AUDF and the output --
   and so the volume -- belongs to the even one. */
static void voice_off(unsigned char v) {
    if (v) AUDC4 = SILENT; else AUDC2 = SILENT;
}

static void voice_note(unsigned char v, unsigned char tenths) {
    unsigned long c;
    unsigned int f;

    if (tenths == 0) { voice_off(v); return; }      /* a rest */
    c = (snd_region == REGION_PAL) ? C_PAL : C_NTSC;
    c = c / tenths;
    /* A note so low the divisor will not fit is played at the lowest pitch
       POKEY has rather than wrapping to a shriek. MUSIC.DAT's lowest is 90 Hz
       and the limit is about 13.6 Hz, so nothing composed for this game gets
       here -- but a note that wrapped would be the loudest possible way to
       find that out. */
    if (c > 65535UL + AUDF_BIAS) c = 65535UL + AUDF_BIAS;
    if (c < AUDF_BIAS) c = AUDF_BIAS;
    f = (unsigned int)(c - AUDF_BIAS);

    if (v) {
        AUDF3 = (unsigned char)(f & 0xFF);
        AUDF4 = (unsigned char)(f >> 8);
        AUDC3 = SILENT;
        AUDC4 = TONE_VOL;
    } else {
        AUDF1 = (unsigned char)(f & 0xFF);
        AUDF2 = (unsigned char)(f >> 8);
        AUDC1 = SILENT;
        AUDC2 = TONE_VOL;
    }
}

/* THE FRAME TICK, and the Atari has the clock the X16 did not.
 *
 * RTCLOK ($0012-$0014) is the OS vertical-blank counter and $0014 is its
 * fastest byte, incrementing once per frame. MEASURED still running for a
 * program that has taken the machine over: 180 changes in 200 frames, the
 * shortfall being the frames spent booting. The OS VBI survives because this
 * port only clears SDMCTL -- it never takes the interrupt vectors.
 *
 * A DIFFERENCE, NOT A TEST FOR CHANGE, so a poll that misses a frame while a
 * panel is being drawn catches up instead of losing tempo.
 *
 * AND THE COUNTER MUST BE READ EVEN WHEN THERE IS NOTHING TO PLAY. The first
 * version returned early from snd_poll before sampling, so `last_frame` went
 * stale for as long as the game was silent -- and the first poll after the
 * music started saw HUNDREDS of frames and burned the whole track in one
 * call. That single fault produced three of the six failures in
 * tools/sndtest.py's first run: the track was already on its second note four
 * frames in, and snd_beep's `done += frames_since()` was satisfied
 * immediately, so the refusal beep ended before it was audible while its
 * bounded loop reported that it had terminated correctly.
 *
 * The catch-up is CAPPED for the same reason. Honouring a two-second gap
 * exactly would keep wall-clock tempo at the price of a burst of far-memory
 * note reads and a flurry of notes nobody hears; four frames is enough to
 * absorb a slow draw and not enough to sound like a fault. */
#define CATCH_UP_MAX 4
static unsigned char frames_since(void) {
    unsigned char now = RTCLOK;
    unsigned char n = (unsigned char)(now - last_frame);
    last_frame = now;
    return n;
}

/* $D014 is $01 on PAL and $0F on NTSC -- SETTLED BY MEASURING BOTH, after the
   first reading of $0F sat next to a base clock that was plainly NTSC's and
   one of the two had to be being misread. Switching the emulator's video
   standard and looking again is what said which. */
void snd_init(void) {
    SKCTL  = 0x03;                 /* POKEY out of reset */
    AUDCTL = AUDCTL_TWO_VOICE;
    AUDC1 = AUDC2 = AUDC3 = AUDC4 = SILENT;
    snd_region = (GTIA_PAL & 0x0E) ? REGION_NTSC : REGION_PAL;
    last_frame = RTCLOK;
    acc = 0;
}

void snd_off(void) {
    voice_off(0); voice_off(1);
    mus_on = 0; sfx_on = 0;
}

uint8_t snd_enabled(void) { return enabled; }
void snd_toggle(void) { enabled = (uint8_t)!enabled; if (!enabled) snd_off(); }

void snd_music_data(unsigned int base, unsigned char ok) {
    mus_base = base;
    mus_ok = ok;
}

void snd_music(uint8_t track) {
    if (!mus_ok || track == MUS_NONE || track >= MUS_COUNT) {
        mus_on = 0; voice_off(0); return;
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

/* The original's refusal beep: 440Hz for 250ms, MEASURED off the DOS game and
   carried here from c128/src/sid.c unchanged. Blocking, as it is there.
 *
 * BOUNDED, and not as defensive programming for its own sake: the unbounded
 * version of this loop is what froze the X16 with a voice sounding. A beep
 * that ends early is a blemish; one that never ends is the bug Jamie hit. */
#define BEEP_TENTHS      44
#define BEEP_FRAMES_NTSC 15    /* 250.6ms at 59.826Hz */
#define BEEP_FRAMES_PAL  13    /* 259.4ms at 50.125Hz */

void snd_beep(void) {
    unsigned char want = (snd_region == REGION_PAL) ? BEEP_FRAMES_PAL
                                                    : BEEP_FRAMES_NTSC;
    unsigned char done = 0;
    unsigned int guard = 0;

    if (!enabled) return;
    sfx_on = 0;                    /* a refusal cancels whatever was playing */
    (void)frames_since();          /* discard the idle gap, or the beep is over
                                      before it starts -- see frames_since */
    voice_note(1, BEEP_TENTHS);
    while (done < want && ++guard) done = (unsigned char)(done + frames_since());
    voice_off(1);
}

/* Two bytes out of far memory, and ONLY when a note ends -- a handful of times
   a second, not once per tick. Each read crosses the MEMAC window, which is
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
                if (note[0] == 0) { mus_on = 0; voice_off(0); return; }
            }
            note_left = note[0];
            voice_note(0, note[1]);
            mus += 2;
        }
    }

    if (sfx_on) {
        if (sfx_left) sfx_left--;
        if (!sfx_left) {
            far_read(sfx, note, 2);
            if (note[0] == 0) { sfx_on = 0; voice_off(1); return; }
            sfx_left = note[0];
            voice_note(1, note[1]);
            sfx += 2;
        }
    }
}

/* RE-ASSERTED EVERY POLL, BECAUSE THE OS TAKES POKEY BACK ON EVERY DISK READ.
 *
 * SIO drives the serial port from POKEY: it joins channels 3+4 as the baud
 * generator and clocks channel 3 at 1.79MHz, which is AUDCTL = $28 -- and it
 * writes the WHOLE register, so bit 4 (join 1+2) and bit 6 (clock channel 1
 * fast) come back CLEAR. Those two are the music voice. Without them channel
 * 2 stops being the high half of a 16-bit divisor and becomes an ordinary
 * 8-bit channel on the 64kHz clock, which is about three octaves up.
 *
 * MEASURED 2026-09-11, at the title screen, by asking Altirra for POKEY's
 * write-side state: AUDCTL $28 where snd_init wrote $78, AUDF1 6 and AUDF2 12
 * -- a divisor of 3078, which is the 290 Hz note the driver intended -- while
 * the machine sounded 2458 Hz, exactly 63921/(2*(12+1)). Jamie heard it on
 * the first play with sound: "the pitch is way too high."
 *
 * AND THIS IS WHY `make run-sndtest` PASSED, twelve checks and 0.05% worst
 * error. It links src/sndtest.c with NO storage seam at all -- the rule names
 * five sources and none of them can touch a disk -- so SIO never ran, AUDCTL
 * stayed $78 and every pitch was right. The game reads an overlay off the
 * disk on the hot path. TWO tests measured this driver in a world it does not
 * run in: one never called snd_poll, this one never touched a disk.
 *
 * Here rather than in voice_note() because a note that starts before an
 * overlay load and is still sounding after it would otherwise stay wrong for
 * its whole length. Here it is repaired within a frame of the read finishing.
 * Not in atarisio.c: SIO is synchronous, so nothing polls during a transfer,
 * and the disk seam has no business knowing this port has a sound chip. */
void snd_poll(void) {
    unsigned char n = frames_since();   /* ALWAYS, even when idle */

    AUDCTL = AUDCTL_TWO_VOICE;

    if (!enabled || (!mus_on && !sfx_on)) return;
    if (n > CATCH_UP_MAX) n = CATCH_UP_MAX;
    while (n--) {
        acc += snd_tick_num(snd_region);
        while (acc >= SND_TICK_DEN) { acc -= SND_TICK_DEN; music_tick(); }
    }
}
