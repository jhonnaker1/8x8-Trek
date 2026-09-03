/* Sound. The MEGA65 has real SIDs at the C64 addresses, so the C128 port's
 * driver is the model and the arithmetic in c128/src/sidfreq.h is reused
 * unchanged -- the note format, the tempo accumulator and the Hz/10 encoding
 * are all platform-independent.
 *
 * TWO DIFFERENCES, both simplifications:
 *
 *   - PITCH does not need region detection here, and TEMPO does. The two were
 *     conflated: this file used to say "NO REGION DETECTION" outright, on the
 *     grounds that the MEGA65 clocks its SIDs from a fixed 1MHz-equivalent
 *     regardless of video mode. That argument is about the SID clock and so
 *     about pitch, and snd_tick_num() is neither -- it converts FRAMES to the
 *     original's 18.2Hz ticks, and frames are 50 a second on PAL against 60 on
 *     NTSC. Using the NTSC numerator on a PAL machine ran the music 19% fast,
 *     which is a whole tone of tempo and plainly audible. Xemu boots PAL.
 *
 *     The pitch constant is still NTSC's and is still a GUESS, now labelled as
 *     one: 1.0MHz sits 2.3% below NTSC's 1.0227 and 1.5% above PAL's 0.9852,
 *     so if anything PAL is the closer pair and the old comment had it
 *     backwards. Under half a semitone either way. NOBODY HAS LISTENED YET;
 *     an ear settles this, not a comment.
 *   - NO FAR MEMORY. Notes are read straight out of the pool array.
 */
#include <stdint.h>
#include <string.h>
#include "../../core/farmem.h"
#include "m65snd.h"
#include "../../c128/src/sidfreq.h"
#include "music_data.h"
#include <mega65/memory.h>

#define SID ((volatile unsigned char *)0xD400)
#define VIC_CTRL1  (*(volatile unsigned char *)0xD011)
#define VIC_RASTER (*(volatile unsigned char *)0xD012)
/* The KERNAL's jiffy counter, ordinary RAM bumped once a frame by its IRQ. */
#define JIFFY_LO   (*(volatile unsigned char *)0x00A2)
#define V1 0
#define V2 7

#define GATE_ON  0x41      /* pulse + gate */
#define GATE_OFF 0x40
#define PW_LO 0x00
#define PW_HI 0x08
#define AD_FLAT 0x00
#define SR_FLAT 0xF0
#define BEEP_TENTHS 44      /* 440Hz, the same A the C128 port beeps */

static uint16_t mus, sfx, mus_base;
static uint8_t  mus_on, sfx_on, mus_track, mus_ok;
static uint8_t  note_left, sfx_left;
static uint8_t  enabled = 1;
static uint16_t acc;
uint8_t snd_region = REGION_NTSC;   /* declared in m65snd.h -- the header promised this before the .c had it */
static unsigned int last_raster;

/* Bit 8 of the raster lives in $D011, so the two reads have to agree about
   which half of the frame they are in -- the same guard c128/src/sid.c uses,
   for the same reason. */
static unsigned int raster_line(void) {
    unsigned char c1, r, c2;
    /* WITHOUT THIS THE REGISTERS READ AS A CONSTANT. The Hypervisor's file
       calls leave the I/O context changed (see m65storage.c's after_hyppo),
       and nothing re-enables it between then and here, so $D011/$D012 were
       reading plain RAM -- detect_region() called every machine NTSC and
       snd_poll() never saw a frame go by. */
    mega65_io_enable();
    for (;;) {
        c1 = VIC_CTRL1;
        r  = VIC_RASTER;
        c2 = VIC_CTRL1;
        if ((c1 & 0x80) == (c2 & 0x80))
            return (unsigned int)r + ((c1 & 0x80) ? 256u : 0u);
    }
}

/* PAL has 312 raster lines and NTSC 263, so the highest line seen over a few
   frames tells them apart with a threshold anywhere between. Lifted from the
   C128 port, where the numbers were measured on real hardware. */
#define PAL_LINE_MIN 300

static uint8_t detect_region(void) {
    unsigned int spins;
    for (spins = 0; spins < 30000u; spins++)
        if (raster_line() >= PAL_LINE_MIN) return REGION_PAL;
    return REGION_NTSC;
}

static void voice_off(uint8_t v) { SID[v + 4] = GATE_OFF; }

static void voice_note(uint8_t v, uint8_t tenths) {
    unsigned int f;
    if (tenths == 0) { SID[v + 4] = GATE_OFF; return; }
    f = sid_freq(tenths, REGION_NTSC);
    SID[v + 0] = (unsigned char)(f & 0xFF);
    SID[v + 1] = (unsigned char)(f >> 8);
    SID[v + 4] = GATE_ON;
}

void snd_init(void) {
    uint8_t i;
    for (i = 0; i < 25; i++) SID[i] = 0;
    SID[24] = 0x0F;
    SID[V1 + 2] = PW_LO; SID[V1 + 3] = PW_HI;
    SID[V1 + 5] = AD_FLAT; SID[V1 + 6] = SR_FLAT;
    SID[V2 + 2] = PW_LO; SID[V2 + 3] = PW_HI;
    SID[V2 + 5] = AD_FLAT; SID[V2 + 6] = SR_FLAT;
    mus_on = sfx_on = 0; acc = 0;
    snd_region = detect_region();
    last_raster = 0;
}

void snd_off(void) { voice_off(V1); voice_off(V2); mus_on = sfx_on = 0; }
void snd_toggle(void) { enabled = !enabled; if (!enabled) snd_off(); }
uint8_t snd_enabled(void) { return enabled; }

void snd_music(uint8_t track) {
    if (!enabled || !mus_ok || track >= MUS_COUNT) {
        mus_on = 0; voice_off(V1); return;
    }
    mus_track = track;
    mus = (uint16_t)(mus_base + mus_offset[track]);
    mus_on = 1; note_left = 0;
}
void snd_music_off(void) { mus_on = 0; voice_off(V1); }

/* WHERE THE NOTES LANDED IN THE POOL, and this is not optional.
 *
 * far_load APPENDS, so STRINGS.DAT goes in first and MUSIC.DAT starts after
 * it -- around offset 7,284, not zero. The first version of this function
 * threw the base away on the reasoning that "here they are already in the pool
 * and the base is always zero", which is true of neither. snd_music() then
 * read its notes from the START OF THE STRING POOL: the first byte pair there
 * is a zero duration, so every track ended before its first note and the title
 * screen came up silent. Jamie heard it. */
void snd_music_data(unsigned int base, unsigned char ok) {
    mus_base = (uint16_t)base;
    mus_ok = ok;
}

void snd_effect(uint8_t which) {
    if (!enabled || !mus_ok || which >= MUS_COUNT) return;
    sfx = (uint16_t)(mus_base + mus_offset[which]);
    sfx_on = 1; sfx_left = 0;
}
void snd_beep(void) { voice_note(V2, BEEP_TENTHS); sfx_on = 0; sfx_left = 0; }

/* One original tick. Called from the frame loop through snd_poll(). */
static void tick(void) {
    if (mus_on) {
        if (note_left) note_left--;
        if (!note_left) {
            uint8_t note[2];
            far_read(mus, note, 2);
            if (note[0] == 0) {                 /* end of track: loop it */
                mus = (uint16_t)(mus_base + mus_offset[mus_track]);
                far_read(mus, note, 2);
                if (note[0] == 0) { mus_on = 0; voice_off(V1); return; }
            }
            mus += 2;
            note_left = note[0];
            voice_note(V1, note[1]);
        }
    }
    if (sfx_on) {
        if (sfx_left) sfx_left--;
        if (!sfx_left) {
            uint8_t note[2];
            far_read(sfx, note, 2);
            if (note[0] == 0) { sfx_on = 0; voice_off(V2); return; }
            sfx += 2;
            sfx_left = note[0];
            voice_note(V2, note[1]);
        }
    }
}

/* CALLED AS OFTEN AS THE CALLER LIKES, and it must be: the only caller is the
   keyboard wait loop, which spins thousands of times a second. This used to
   assume exactly one call per frame -- true of smoke2.c's `wait_vsync();
   snd_poll();` loop and of nothing in the game, which is why the game was
   silent: NOTHING CALLED IT AT ALL. A frame has passed when the raster counter
   goes backwards, which works at any rate above two samples a frame. */
void snd_poll(void) {
    unsigned int r;

    if (!enabled) return;

    /* A frame has passed when the raster counter goes backwards -- correct at
       any call rate above two samples a frame, which this loop clears by
       orders of magnitude. Same mechanism as c128/src/sid.c.

       READING $D012 FROM HERE USED TO WEDGE THE MACHINE within ten seconds,
       into a DMA whose descriptor had two bytes wrong. That was never about
       the raster: the DMA job lived in ZERO PAGE and something overwrote it.
       With the job out of zero page (see m65mem.c) this is fine. The KERNAL
       jiffy at $A0..$A2 was tried as a safer source and is useless here --
       nothing increments it, because no interrupt of the ROM's is running. */
    r = raster_line();
    if (r >= last_raster) { last_raster = r; return; }
    last_raster = r;

    acc = (uint16_t)(acc + snd_tick_num(snd_region));
    while (acc >= SND_TICK_DEN) { acc = (uint16_t)(acc - SND_TICK_DEN); tick(); }
}
