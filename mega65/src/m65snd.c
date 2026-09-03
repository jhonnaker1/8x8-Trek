/* Sound. The MEGA65 has real SIDs at the C64 addresses, so the C128 port's
 * driver is the model and the arithmetic in c128/src/sidfreq.h is reused
 * unchanged -- the note format, the tempo accumulator and the Hz/10 encoding
 * are all platform-independent.
 *
 * TWO DIFFERENCES, both simplifications:
 *
 *   - NO REGION DETECTION. The C128 driver reads the raster to tell PAL from
 *     NTSC because the SID's clock differs by 3.8% and it is audible. The
 *     MEGA65 clocks its SIDs from a fixed 1MHz-equivalent regardless of video
 *     mode, so there is one set of numbers. NTSC's are the closer pair.
 *   - NO FAR MEMORY. Notes are read straight out of the pool array.
 */
#include <stdint.h>
#include <string.h>
#include "../../core/farmem.h"
#include "m65snd.h"
#include "../../c128/src/sidfreq.h"
#include "music_data.h"

#define SID ((volatile unsigned char *)0xD400)
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

void snd_poll(void) {
    if (!enabled) return;
    acc = (uint16_t)(acc + snd_tick_num(REGION_NTSC));
    while (acc >= SND_TICK_DEN) { acc = (uint16_t)(acc - SND_TICK_DEN); tick(); }
}
