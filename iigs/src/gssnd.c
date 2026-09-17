/* Ensoniq 5503 sound for the Apple IIgs: two oscillators, music and effects.
 *
 * TWO VOICES IS THE POINT, and c128/src/sid.h says why: voice 1 carries music
 * and voice 2 effects, so a hit during the title track does not chop the tune.
 * The original had one PC speaker and could not. The DOC has THIRTY-TWO
 * oscillators, so two is not a constraint here -- it is the shape the shared
 * driver has and there is nothing to gain from a third.
 *
 * THE INTERFACE IS FOUR ADDRESSES and a mode byte:
 *
 *     $C03C  SOUNDCTL   bit 6 auto-increment, bit 5 picks the DOC's REGISTERS
 *                       (0) or its 64K of wavetable RAM (1), low nibble is the
 *                       master volume
 *     $C03D  DATA
 *     $C03E  address low      $C03F  address high
 *
 * and the registers are five files of 32, one entry per oscillator: frequency
 * low at $00, high at $20, volume at $40, wavetable pointer at $80, control at
 * $A0, size and resolution at $C0. $E1 says how many oscillators are enabled.
 *
 * THE FREQUENCY, CALIBRATED AT THREE POINTS, because this project shipped a
 * port an octave flat for four months off a single-point check and shipped the
 * GIME port's first build an octave SHARP for the same reason:
 *
 *     register    MEASURED
 *      $0400      1757.1 Hz      1.71592 Hz per unit
 *      $0800      3514.8 Hz      1.71621
 *      $1000      7026.9 Hz      1.71555
 *
 * Ratios 2.0003 and 1.9992 against a wanted 2.0, so the MODEL is right; the
 * three constants agree to 0.04%, so the NUMBER is right. `src/gssndp.c`
 * under MAME with `-wavwrite`, read back by coco3gime/tools/hearit.py, which
 * counts zero crossings -- which is exact for the square wavetable this port
 * uses and only approximate for anything else.
 *
 * Theory, for what it is worth AFTER the fact: with two oscillators enabled
 * the DOC's sample rate is 894886/(2+2) = 223,721 Hz and a 256-entry table at
 * resolution 0 gives f = reg x 223721/131072 = reg x 1.70686 -- 0.53% below
 * what was measured. The measured column is what ships. Arithmetic that
 * agrees with itself is not evidence.
 *
 * AND THE CALIBRATION IS TIED TO $E1. The sample rate depends on how many
 * oscillators are enabled, so the constant above is only this constant while
 * OSCEN says two. Changing it moves every note.
 *
 * NO ZERO BYTES IN THE WAVETABLE. The DOC halts an oscillator when it reads a
 * sample of zero -- that is how one-shot sounds end -- so a waveform that
 * swings through zero stops itself. $40 and $C0.
 */
#include "../../c128/src/sid.h"
#include "../../c128/src/sidfreq.h"
#include "../../c128/src/music_data.h"
#include "../../core/farmem.h"

#define SOUNDCTL (*(volatile unsigned char *)0xC03C)
#define SNDDATA  (*(volatile unsigned char *)0xC03D)
#define ADDRLO   (*(volatile unsigned char *)0xC03E)
#define ADDRHI   (*(volatile unsigned char *)0xC03F)
#define VBL      (*(volatile unsigned char *)0xC019)

#define DOC_FREQLO 0x00
#define DOC_FREQHI 0x20
#define DOC_VOL    0x40
#define DOC_PTR    0x80
#define DOC_CTL    0xA0
#define DOC_SIZE   0xC0
#define DOC_OSCEN  0xE1

#define MASTER_VOL 0x0F
#define OSC_VOL    0xC0           /* of $FF; the PC speaker had no envelope,
                                     and full scale on two oscillators at once
                                     clips in the mixer */
#define BEEP_TENS  44             /* 440Hz, the A every other port beeps */
#define BEEP_FRAMES 15            /* 250ms at 60Hz */

/* A IIgs IS NTSC OR PAL AND THIS DOES NOT DETECT WHICH. The DOC's clock is
   not derived from the video standard -- unlike a SID's -- so the PITCH does
   not move. Only the TEMPO does, and the machine's home market is NTSC. */
uint8_t snd_region = REGION_NTSC;

static uint8_t enabled = 1;
static unsigned int acc;
static unsigned char last_vbl;
static unsigned int mus, mus_head, sfx;
static unsigned char mus_on, mus_ok, note_left, sfx_on, sfx_left;
static unsigned char mus_buf[MUS_BYTES];

static void doc_reg(unsigned char reg, unsigned char val)
{
    SOUNDCTL = MASTER_VOL;        /* registers, no auto-increment */
    ADDRLO = reg;
    ADDRHI = 0;
    SNDDATA = val;
}

static void voice_off(unsigned char ch)
{
    doc_reg((unsigned char)(DOC_CTL + ch), 0x01);        /* halt */
}

static void voice_note(unsigned char ch, unsigned char tens)
{
    unsigned int n;

    if (tens == 0) { voice_off(ch); return; }            /* a rest */

    /* tens is the pitch in TENS of Hz, so the register is tens x 5.82787.
       Done as tens*5 + (tens*212 + 128)/256 rather than through a 32-bit
       divide: 5 + 212/256 is 5.82813, which is 0.005% from the measured
       constant and inside sixteen bits for every pitch the music uses
       (tens = 200 gives 1,166). Rounded, not truncated -- one add, and free
       of the systematic flatness truncation gives across a whole tune. */
    n = (unsigned int)tens * 5U;
    n = (unsigned int)(n + (((unsigned int)tens * 212U + 128U) >> 8));

    doc_reg((unsigned char)(DOC_FREQLO + ch), (unsigned char)(n & 0xFF));
    doc_reg((unsigned char)(DOC_FREQHI + ch), (unsigned char)(n >> 8));
    doc_reg((unsigned char)(DOC_VOL + ch), OSC_VOL);
    doc_reg((unsigned char)(DOC_CTL + ch), 0x00);        /* free-run, running */
}

void snd_init(void)
{
    unsigned int i;

    /* A 256-byte square at DOC RAM $0000, which both oscillators point at. */
    SOUNDCTL = 0x60 | MASTER_VOL;            /* RAM, auto-increment */
    ADDRLO = 0; ADDRHI = 0;
    for (i = 0; i < 128; i++) SNDDATA = 0xC0;
    for (i = 0; i < 128; i++) SNDDATA = 0x40;

    for (i = 0; i < 2; i++) {
        doc_reg((unsigned char)(DOC_PTR + i), 0x00);     /* table at page 0 */
        doc_reg((unsigned char)(DOC_SIZE + i), 0x00);    /* 256, resolution 0 */
        doc_reg((unsigned char)(DOC_VOL + i), 0x00);
        doc_reg((unsigned char)(DOC_CTL + i), 0x01);     /* halted */
    }
    /* TWO OSCILLATORS, and this is the value the frequency constant was
       measured at. See the head of this file. */
    doc_reg(DOC_OSCEN, 0x02);

    acc = 0;
    last_vbl = 0;
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

/* Lifted off the far store ONCE rather than two bytes a note, because the
   shared driver shape assumes it -- and here far_read is a long-addressed
   read from bank $01, so per-note fetches would cost real cycles. */
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
    unsigned char frames = 0, v, last;
    unsigned int guard = 0;

    if (!enabled) return;
    sfx_on = 0;
    voice_note(1, BEEP_TENS);
    last = (unsigned char)(VBL & 0x80);
    while (frames < BEEP_FRAMES) {
        v = (unsigned char)(VBL & 0x80);
        if (v && !last) { frames++; guard = 0; }
        else if (++guard == 0) break;
        last = v;
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
    unsigned char v;

    if (!enabled || (!mus_on && !sfx_on)) return;

    /* AN EDGE, NOT A LEVEL. There is no raster counter to watch going
       backwards here, so the frame source is $C019 bit 7 and what counts is
       the transition -- a level test would tick every call for the whole of
       the blanking interval. Like the raster trick it works whatever rate
       this is called at, so long as that is more than twice a frame, and it
       needs no interrupt of our own. */
    v = (unsigned char)(VBL & 0x80);
    if (!(v && !last_vbl)) { last_vbl = v; return; }
    last_vbl = v;

    acc = (unsigned int)(acc + snd_tick_num(snd_region));
    while (acc >= SND_TICK_DEN) {
        acc = (unsigned int)(acc - SND_TICK_DEN);
        music_tick();
    }
}

#ifdef GS_SNDTEST
/* THROUGH THE SAME voice_note THE MUSIC USES, which is the whole point: a test
   that reimplements the frequency arithmetic proves the test. Compiled out of
   every shipping build. */
void gs_test_note(unsigned char ch, unsigned char tens) { voice_note(ch, tens); }
void gs_test_off(unsigned char ch) { voice_off(ch); }
#endif
