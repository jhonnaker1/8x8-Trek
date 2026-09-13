/* Sound for the CoCo 3 + SuperSprite FM+: the card's YM2149.
 *
 * TWO VOICES, as every other port does it -- channel A carries the music and
 * channel B the effects, so a hit during the title track does not chop the
 * tune. c128/src/sid.h says why that is the one place these ports beat the
 * original outright.
 *
 * THE CARD HAS TWO SOUND CHIPS AND THIS DRIVES THE SIMPLER ONE. A YM2413
 * OPLL sits at $FF76-$FF77 and is richer than anything else this project
 * touches; the YM2149 at $FF7C/$FF7D is an AY-3-8910, which is the shape
 * every 8-bit port here already plays. The OPLL is a better instrument and a
 * second project; getting the game to make the right NOTES comes first.
 *
 * THE CLOCK IS NOT GUESSED. MAME's dragon_msx2.cpp builds both chips at
 * `21.477272_MHz_XTAL / 6` = 3,579,545 Hz, and the AY divides by 16:
 *
 *     f = clock / (16 * period)   ->   period = 3579545 / (16 * f)
 *
 * The music format carries pitch in TENS of Hz, so period = 22372 / tenths.
 * CALIBRATED AT THREE POINTS, not one, because this project shipped an octave
 * flat for four months on a single-point check ([[x16-octave-flat]]):
 *
 *     tenths  44 -> period 508  -> 440.3 Hz  (wanted 440)
 *     tenths 100 -> period 223  -> 1003.2 Hz (wanted 1000)
 *     tenths  20 -> period 1118 -> 200.1 Hz  (wanted 200)
 *
 * All inside 0.35%, against a semitone of 5.9%.
 *
 * THE FRAME TICK IS THE VDP'S, not a timer: S#0 bit 7 is the vblank flag and
 * READING $FF79 CLEARS IT, which is exactly the edge-detector this needs.
 * It is the same flag `wait_vsync()` waits on, so a poll here can occasionally
 * take a frame that the video driver then waits an extra one for -- harmless,
 * and cheaper than a second timing source.
 */
#include <stdint.h>

#include "../../c128/src/sid.h"
#include "../../c128/src/sidfreq.h"      /* the ORIGINAL's tempo, not the SID's */
#include "../../c128/src/music_data.h"
#include "../../core/farmem.h"

#define AY_ADDR  (*(unsigned char *)0xFF7C)   /* register select */
#define AY_DATA  (*(unsigned char *)0xFF7D)   /* register data   */
#define VDP_PORT1 (*(unsigned char *)0xFF79)  /* write: address/register
                                                 read: status */
#define CH_MUSIC 0
#define CH_SFX   1
#define AY_DIV   22372U                  /* 3579545 / 16 / 10 */
#define LEVEL    12                      /* of 15; the PC speaker had no
                                            envelope and neither does this */
#define BEEP_TENTHS 44                   /* 440Hz, the A the other ports beep */
#define BEEP_FRAMES 15                   /* 250ms at 60Hz */

uint8_t snd_region = REGION_NTSC;         /* R#9 bit 1 is clear: 60Hz */

static uint8_t enabled = 1;
/* R7 is a MIXER AND ITS BITS ARE ACTIVE LOW: a 1 disables. $3F is everything
   off, which is where this starts and returns to. */
static unsigned char mix = 0x3F;
static unsigned int  acc;
static unsigned int  mus, mus_head, mus_base;
static unsigned char mus_on, mus_ok, note_left;
static unsigned int  sfx;
static unsigned char sfx_on, sfx_left;

static void ay(unsigned char r, unsigned char v)
{
    AY_ADDR = r;
    AY_DATA = v;
}

static void voice_off(unsigned char ch)
{
    ay((unsigned char)(8 + ch), 0);             /* amplitude to zero */
    mix |= (unsigned char)(1 << ch);            /* and the tone gate shut */
    ay(7, mix);
}

static void voice_note(unsigned char ch, unsigned char tenths)
{
    unsigned int period;

    if (tenths == 0) { voice_off(ch); return; }          /* a rest */

    /* Rounded, not truncated -- it costs one add and it is free of the
       systematic flatness truncation gives across a whole tune. */
    period = (unsigned int)((AY_DIV + (tenths >> 1)) / tenths);
    if (period > 4095) period = 4095;                    /* 12 bits */
    if (period == 0) period = 1;

    ay((unsigned char)(ch * 2), (unsigned char)(period & 0xFF));
    ay((unsigned char)(ch * 2 + 1), (unsigned char)((period >> 8) & 0x0F));
    mix &= (unsigned char)~(1 << ch);                    /* tone on (active low) */
    ay(7, mix);
    ay((unsigned char)(8 + ch), LEVEL);
}

/* Non-zero once per frame. Reading the status register clears the flag, so
   this is an edge and not a level. */
static unsigned char frame_tick(void)
{
    return (unsigned char)((VDP_PORT1 & 0x80) ? 1 : 0);
}

static void status_select(void)
{
    VDP_PORT1 = 0x00;                 /* value first */
    VDP_PORT1 = 0x8F;                 /* then $80|15: R#15 = 0, status 0 */
}

void snd_init(void)
{
    unsigned char r;

    for (r = 0; r < 14; r++) ay(r, 0);
    mix = 0x3F;
    ay(7, mix);                        /* everything gated off */
    status_select();
    (void)frame_tick();                /* start from a known state */
    acc = 0;
}

void snd_off(void)
{
    voice_off(CH_MUSIC);
    voice_off(CH_SFX);
    mus_on = 0;
    sfx_on = 0;
}

uint8_t snd_enabled(void) { return enabled; }

void snd_toggle(void)
{
    enabled = (uint8_t)!enabled;
    if (!enabled) snd_off();
}

void snd_music_data(unsigned int base, unsigned char ok)
{
    mus_base = base;
    mus_ok = ok;
}

void snd_music(uint8_t track)
{
    if (!enabled || !mus_ok || track == MUS_NONE || track >= MUS_COUNT) {
        mus_on = 0;
        voice_off(CH_MUSIC);
        return;
    }
    mus_head = (unsigned int)(mus_base + mus_offset[track]);
    mus = mus_head;
    mus_on = 1;
    note_left = 0;                     /* zero forces the first note next tick */
    acc = 0;
}

void snd_effect(uint8_t track)
{
    if (!enabled || !mus_ok || track >= MUS_COUNT) return;
    sfx = (unsigned int)(mus_base + mus_offset[track]);
    sfx_on = 1;
    sfx_left = 0;
}

/* THE REFUSAL BEEP, blocking as it is in the original, and BOUNDED. An
   unbounded wait on a frame source that has died is a machine that hangs with
   a voice sounding -- the X16 shipped that once. */
void snd_beep(void)
{
    unsigned char frames = 0;
    unsigned int guard = 0;

    if (!enabled) return;
    sfx_on = 0;                        /* a refusal cancels what was playing */
    status_select();
    (void)frame_tick();                /* or the first tick is free */
    voice_note(CH_SFX, BEEP_TENTHS);
    while (frames < BEEP_FRAMES) {
        if (frame_tick())      { frames++; guard = 0; }
        else if (++guard == 0) { break; }
    }
    voice_off(CH_SFX);
}

/* Two bytes out of far memory, and ONLY when a note ends. Each read crosses
   the VDP's address counter -- see coco3vdp.h, three tenants share it -- which
   is ruinous in a loop and nothing at a handful of times a second. */
static void music_tick(void)
{
    unsigned char note[2];

    if (mus_on) {
        if (note_left) note_left--;
        if (!note_left) {
            far_read(mus, note, 2);
            if (note[0] == 0) {                 /* zero duration ends a track */
                mus = mus_head;                 /* and it loops until stopped */
                far_read(mus, note, 2);
                if (note[0] == 0) { mus_on = 0; voice_off(CH_MUSIC); return; }
            }
            note_left = note[0];
            voice_note(CH_MUSIC, note[1]);
            mus = (unsigned int)(mus + 2);
        }
    }
    if (sfx_on) {
        if (sfx_left) sfx_left--;
        if (!sfx_left) {
            far_read(sfx, note, 2);
            if (note[0] == 0) { sfx_on = 0; voice_off(CH_SFX); return; }
            sfx_left = note[0];
            voice_note(CH_SFX, note[1]);
            sfx = (unsigned int)(sfx + 2);
        }
    }
}

void snd_poll(void)
{
    if (!enabled) return;
    if (!mus_on && !sfx_on) return;
    if (!frame_tick()) return;

    acc = (unsigned int)(acc + snd_tick_num(snd_region));
    while (acc >= SND_TICK_DEN) {
        acc = (unsigned int)(acc - SND_TICK_DEN);
        music_tick();
    }
}
