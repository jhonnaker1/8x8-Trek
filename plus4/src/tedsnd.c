/* TED sound: two voices, and this machine can do what the CoCo 3 could not.
 *
 * TWO VOICES IS THE POINT. c128/src/sid.h says why it matters: voice 1 carries
 * music and voice 2 effects, so a hit during the title track does not chop the
 * tune. The original had one PC speaker and could not. The card-less CoCo 3
 * could not either -- a DAC holds no pitch, so music and effects had to share
 * one speaker there. TED has two tone generators, so the Plus/4 gets the same
 * arrangement as the SID ports.
 *
 * THE REGISTERS, and they are not laid out the way a SID's are:
 *
 *     $FF0E  voice 1 frequency, low eight bits
 *     $FF0F  voice 2 frequency, low eight bits
 *     $FF10  bits 1-0: voice 2 frequency, high two bits
 *     $FF11  bits 3-0 VOLUME (global, 0-8), bit 4 voice 1 on,
 *            bit 5 voice 2 on, bit 6 makes voice 2 noise instead of tone
 *     $FF12  bits 1-0: voice 1 frequency, high two bits -- AND BIT 2 IS THE
 *            CHARACTER GENERATOR'S ROM ENABLE, which has nothing to do with
 *            sound and must never be written as a side effect. See below.
 *
 * A TEN-BIT COUNTER THAT COUNTS UP TO 1024, so the frequency is
 *
 *     f = 111860.78 / (1024 - N)      PAL
 *
 * and N rises with pitch, which is the opposite of a SID's period and of the
 * AY's and of the GIME timer's. Getting that backwards gives a tune that plays
 * its intervals inside out, which sounds wrong without sounding obviously
 * broken -- worth the sentence.
 *
 * The music format carries pitch in TENS of Hz, so
 *
 *     N = 1024 - (11186 / tenths)
 *
 * and 11186 fits in sixteen bits, so no staging is needed -- unlike the GIME
 * driver, where the numerator did not fit and had to be quartered.
 *
 * CALIBRATED AT THREE POINTS, because this project shipped an octave flat for
 * four months off a single-point check, and shipped the GIME port's first
 * build an octave SHARP for the same reason:
 *
 *     wanted    predicted    MEASURED
 *        440       440.4        438.3 Hz    -0.38%
 *       1000       998.8        998.4 Hz    -0.16%
 *        200       200.1        198.7 Hz    -0.64%
 *        440       440.4        439.1 Hz    -0.20%   <- ON VOICE 2
 *
 * all inside 0.64%, against a semitone of 5.9%. The MEASURED column is what
 * counts: `src/sndtest.c` under xplus4 with `-sounddev wav`, read back by
 * `coco3gime/tools/hearit.py`, which counts zero crossings. An earlier draft
 * of this comment carried only the predicted column and said so in as many
 * words -- "a hypothesis with three decimal places" -- because arithmetic that
 * agrees with itself is not evidence.
 *
 * The fourth line is the one the CoCo 3 could not produce: a second tone from
 * a second generator, addressed independently.
 */
#include "../../c128/src/sid.h"
#include "../../c128/src/sidfreq.h"
#include "../../c128/src/music_data.h"
#include "../../core/farmem.h"

#define TED_V1FREQ (*(volatile unsigned char *)0xFF0E)
#define TED_V2FREQ (*(volatile unsigned char *)0xFF0F)
#define TED_V2HI   (*(volatile unsigned char *)0xFF10)
#define TED_CTRL   (*(volatile unsigned char *)0xFF11)
#define TED_V1HI   (*(volatile unsigned char *)0xFF12)
#define TED_RASTER (*(volatile unsigned char *)0xFF1D)

#define V1_ON 0x10
#define V2_ON 0x20
#define VOLUME 8                  /* of 8 -- the PC speaker had no envelope */
#define TED_DIV 11186U            /* 111860.78 / 10, for pitch in tens of Hz */

#define BEEP_TENTHS 44            /* 440Hz, the A every other port beeps */
#define BEEP_FRAMES 15            /* 250ms at 60Hz */

/* A PLUS/4 IS PAL OR NTSC AND THIS DOES NOT DETECT WHICH. snd_region decides
   the tempo divisor and, strictly, the 111860.78 above -- an NTSC TED clocks
   111840.45, which is 0.02% and inaudible. The tempo is the part that would be
   heard. Detection is unwritten; PAL is the machine's home market and the
   honest default. */
uint8_t snd_region = REGION_PAL;

static uint8_t enabled = 1;
static unsigned char ctrl = VOLUME;      /* the shadow: $FF11 does not read back
                                            usefully and both voices share it */
static unsigned int acc, last_raster;
static unsigned int mus, mus_head, sfx;
static unsigned char mus_on, mus_ok, note_left, sfx_on, sfx_left;
static unsigned char mus_buf[MUS_BYTES];

static void voice_off(unsigned char ch)
{
    ctrl &= (unsigned char)~(ch ? V2_ON : V1_ON);
    TED_CTRL = ctrl;
}

static void voice_note(unsigned char ch, unsigned char tenths)
{
    unsigned int n;

    if (tenths == 0) { voice_off(ch); return; }          /* a rest */

    /* Rounded, not truncated: one add, and free of the systematic error
       truncation gives across a whole tune. */
    n = (unsigned int)((TED_DIV + (tenths >> 1)) / tenths);
    n = (n >= 1024) ? 0 : (unsigned int)(1024 - n);      /* N RISES with pitch */

    if (ch) {
        TED_V2FREQ = (unsigned char)(n & 0xFF);
        TED_V2HI   = (unsigned char)((TED_V2HI & 0xFC) | ((n >> 8) & 0x03));
        ctrl |= V2_ON;
    } else {
        TED_V1FREQ = (unsigned char)(n & 0xFF);
        TED_V1HI   = (unsigned char)((TED_V1HI & 0xFC) | ((n >> 8) & 0x03));
        ctrl |= V1_ON;
    }
    ctrl = (unsigned char)((ctrl & 0xF0) | VOLUME);
    TED_CTRL = ctrl;
}

void snd_init(void)
{
    ctrl = VOLUME;                        /* volume up, both voices gated off */
    TED_CTRL = ctrl;
    /* READ-MODIFY-WRITE, AND `= 0` HERE COST THE WHOLE DISPLAY. $FF12 bit 2
       is TED's CHARACTER GENERATOR ROM ENABLE: set, TED enables ROM for its
       charset fetch; clear, it reads DRAM. Only bits 1-0 are voice 1's
       frequency. `TED_V1HI = 0` cleared bit 2, so from snd_init() onwards TED
       drew every glyph out of this program's own RAM -- the screen codes were
       perfect and the pixels were horizontal bars.
       It took a SCREENSHOT to see. Reading screen memory says what the
       characters ARE, never what they LOOK like, and this port had only ever
       been checked by reading $0C00. */
    TED_V1HI = (unsigned char)(TED_V1HI & 0xFC);
    TED_V2HI = (unsigned char)(TED_V2HI & 0xFC);
    acc = 0;
    last_raster = 0;
}

void snd_off(void)
{
    voice_off(0);
    voice_off(1);
    mus_on = 0;
    sfx_on = 0;
}

uint8_t snd_enabled(void) { return enabled; }

void snd_toggle(void)
{
    enabled = (uint8_t)!enabled;
    if (!enabled) snd_off();
}

/* Lifted off the disk ONCE. far_read here is a memcpy -- this port's far store
   is plain RAM, not a drive -- so this is cheap, but it is still done once
   rather than two bytes a note, because the shared driver shape assumes it. */
void snd_music_data(unsigned int base, unsigned char ok)
{
    unsigned int off;
    unsigned char n;

    mus_ok = 0;
    if (!ok) return;
    for (off = 0; off < MUS_BYTES; off = (unsigned int)(off + n)) {
        n = (unsigned char)((MUS_BYTES - off) > 128 ? 128 : (MUS_BYTES - off));
        far_read((unsigned int)(base + off), &mus_buf[off], n);
    }
    mus_ok = 1;
}

void snd_music(uint8_t track)
{
    if (!enabled || !mus_ok || track == MUS_NONE || track >= MUS_COUNT) {
        mus_on = 0;
        voice_off(0);
        return;
    }
    mus_head = mus_offset[track];
    mus = mus_head;
    mus_on = 1;
    note_left = 0;
    acc = 0;
}

void snd_effect(uint8_t track)
{
    if (!enabled || !mus_ok || track >= MUS_COUNT) return;
    sfx = mus_offset[track];
    sfx_on = 1;
    sfx_left = 0;
}

/* THE REFUSAL BEEP, blocking as it is in the original, and BOUNDED -- an
   unbounded wait on a frame source that has died is a machine that hangs with
   a voice sounding, which the X16 shipped once. */
void snd_beep(void)
{
    unsigned char frames = 0;
    unsigned int guard = 0, r, last;

    if (!enabled) return;
    sfx_on = 0;
    voice_note(1, BEEP_TENTHS);
    last = TED_RASTER;
    while (frames < BEEP_FRAMES) {
        r = TED_RASTER;
        if (r < last) { frames++; guard = 0; }
        else if (++guard == 0) break;
        last = r;
    }
    voice_off(1);
}

static void music_tick(void)
{
    if (mus_on) {
        if (note_left) note_left--;
        if (!note_left) {
            if (mus_buf[mus] == 0) {          /* a zero duration ends a track */
                mus = mus_head;               /* and it loops until stopped   */
                if (mus_buf[mus] == 0) { mus_on = 0; voice_off(0); return; }
            }
            note_left = mus_buf[mus];
            voice_note(0, mus_buf[mus + 1]);
            mus = (unsigned int)(mus + 2);
        }
    }
    if (sfx_on) {
        if (sfx_left) sfx_left--;
        if (!sfx_left) {
            if (mus_buf[sfx] == 0) { sfx_on = 0; voice_off(1); return; }
            sfx_left = mus_buf[sfx];
            voice_note(1, mus_buf[sfx + 1]);
            sfx = (unsigned int)(sfx + 2);
        }
    }
}

void snd_poll(void)
{
    unsigned int r;

    if (!enabled || (!mus_on && !sfx_on)) return;

    /* A frame has passed when the raster counter goes backwards -- sid.c's
       trick, and for its reason: it works whatever rate this is called at so
       long as that is more than twice a frame, and it needs no interrupt of
       our own. Taking the IRQ would mean going further behind a KERNAL this
       port already banks away. */
    r = TED_RASTER;
    if (r >= last_raster) { last_raster = r; return; }
    last_raster = r;

    acc = (unsigned int)(acc + snd_tick_num(snd_region));
    while (acc >= SND_TICK_DEN) {
        acc = (unsigned int)(acc - SND_TICK_DEN);
        music_tick();
    }
}
