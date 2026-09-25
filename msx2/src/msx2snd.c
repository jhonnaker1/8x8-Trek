/* Sound for the MSX2: the AY-3-8910 PSG, three channels of square wave.
 *
 * THE SAME CHIP AS THE FALCON AND THE ST -- their YM2149 is a licensed
 * AY-3-8910 -- so this is falconsnd.c's player with three things changed:
 * the clock, the timer, and the latch discipline. Two voices as everywhere
 * (A music, B effects), and a PSG tone channel IS the square wave the PC
 * speaker made, with nothing in between.
 *
 * THE LATCH IS SHARED WITH THE BIOS, MEASURED 2026-09-24. The PSG is
 * addressed by writing a register number to $A0 and then the value to $A1.
 * An openMSX watchpoint on $A0, with nothing of ours running, logged 1,004
 * writes in five seconds -- the BIOS interrupt handler at $110E selects R#15
 * then R#14 every frame to read the joystick triggers. An interrupt between
 * our latch and our data therefore sends our value to R#14, or to R#15,
 * which is an OUTPUT port (joystick pins and the kana LED). So every write
 * is a di..ei pair, exactly as the VDP's latches are in msx2vid.c -- and not
 * SDCC's `__critical`, for the reason given there.
 *
 * THE PSG CLOCK IS 3.579545MHz / 2, the colourburst crystal every MSX has,
 * so period = 1789772.5 / (16 * Hz) = 111861 / Hz, and with the track
 * format's tens of Hz that is 11186 / tenths: one 16-bit divide and no long
 * arithmetic.
 *
 * TIMING COMES FROM JIFFY, the BIOS's frame counter at $FC9E, not from a
 * call count -- the lesson the first two ports paid for in tempo. It counts
 * 50 or 60 a second by region, and the region is the V9938's own NT bit as
 * the BIOS left it in RG9SAV.
 */
#include <stdint.h>

#include "sid.h"
#include "farmem.h"
#include "music_data.h"

__sfr __at 0xA0 PSG_ADDR;
__sfr __at 0xA1 PSG_DATA;

#define IRQ_OFF() __asm__("di")
#define IRQ_ON()  __asm__("ei")

#define JIFFY   (*(volatile unsigned int *)0xFC9E)
#define RG9SAV  (*(volatile unsigned char *)0xFFE8)
#define R9_NT   0x02                /* V9938 R#9 bit 1: 1 = 50Hz */

#define R_A_LO   0
#define R_B_LO   2
#define R_MIXER  7
#define R_A_VOL  8

#define V_MUS  0                    /* channel A */
#define V_SFX  1                    /* channel B */
#define VOLUME 12                   /* of 15, the Falcon's level */

/* MIXER BITS ARE ACTIVE LOW, AND THE TOP TWO ARE NOT THE MIXER AT ALL.
 * Bits 0-2 enable tones A/B/C and 3-5 noise, a 0 enabling. Bits 6 and 7 are
 * the PSG's port directions, and on an MSX they are the machine's: port A
 * (R#14) is the joystick INPUT and port B (R#15) an OUTPUT, so bit 6 must be
 * 0 and bit 7 must be 1 or the BIOS loses the joysticks. The Falcon's base
 * was 0xBC because its port A drives the floppy; this one is 0xBC too, by
 * the other route: B out, A in, noise off, tone C off. */
#define MIX_BASE  0xBC

#define PSG_TENTHS 11186U           /* 111861 / 10 */
#define PERIOD_MAX 4095U            /* 12 bits; 90Hz wants 1243 */

/* 18.2065 PC ticks a second, as a fraction of a jiffy in ten-thousandths.
 * A JIFFY IS NOT 1/50 OR 1/60 OF A SECOND. The V9938 runs 1368 clocks a line
 * at 21.477MHz and 313 lines a PAL frame, 262 an NTSC one: 50.159Hz and
 * 59.923Hz. Steps computed from round 50 and 60 played the title 0.35% FAST
 * in the first recording -- tools/listen.py measured it, the ratio is
 * exactly 50.159/50, and the check's tolerance had let it through.
 * 18.2065 / 50.159 = 0.36297 and / 59.923 = 0.30383. */
#define TICK_ONE   10000U
#define STEP_PAL   3630U
#define STEP_NTSC  3038U

uint8_t snd_region = REGION_NTSC;

static unsigned char enabled = 1;
static unsigned char mixer = MIX_BASE | 0x03;

static unsigned int  mus_base, mus_head, mus, sfx;
static unsigned char mus_ok, mus_on, sfx_on;
static unsigned char note_left, sfx_left;

static unsigned int  last_jiffy, acc, step;

static void psg_w(unsigned char reg, unsigned char val)
{
    IRQ_OFF();
    PSG_ADDR = reg;
    PSG_DATA = val;
    IRQ_ON();
}

static unsigned int jiffy(void)
{
    unsigned int j;
    IRQ_OFF();                      /* one `ld de,(nn)` today, which cannot
                                       tear; this keeps it so if SDCC ever
                                       splits it -- a torn read jumps 256 */
    j = JIFFY;
    IRQ_ON();
    return j;
}

static void voice_off(unsigned char v)
{
    mixer |= (unsigned char)(1 << v);
    psg_w(R_MIXER, mixer);
    psg_w((unsigned char)(R_A_VOL + v), 0);
}

/* `tenths` is the track format's frequency: tens of Hz, 0 for a rest. */
static void voice_note(unsigned char v, unsigned char tenths)
{
    unsigned int period;

    if (!enabled || tenths == 0) {
        voice_off(v);
        return;
    }
    period = PSG_TENTHS / tenths;
    if (period > PERIOD_MAX) period = PERIOD_MAX;

    psg_w((unsigned char)(R_A_LO + 2 * v), (unsigned char)period);
    psg_w((unsigned char)(R_A_LO + 2 * v + 1), (unsigned char)(period >> 8));
    mixer &= (unsigned char)~(1 << v);
    psg_w(R_MIXER, mixer);
    psg_w((unsigned char)(R_A_VOL + v), VOLUME);
}

void snd_init(void)
{
    snd_region = (RG9SAV & R9_NT) ? REGION_PAL : REGION_NTSC;
    step = snd_region == REGION_PAL ? STEP_PAL : STEP_NTSC;
    mixer = MIX_BASE | 0x03;
    psg_w(R_MIXER, mixer);
    psg_w(R_A_VOL, 0);
    psg_w(R_A_VOL + 1, 0);
    last_jiffy = jiffy();
    acc = 0;
}

void snd_off(void)
{
    voice_off(V_MUS);
    voice_off(V_SFX);
    mus_on = sfx_on = 0;
}

void snd_music_data(unsigned int base, unsigned char ok)
{
    mus_base = base;
    mus_ok = ok;
}

void snd_music(uint8_t track)
{
    if (!mus_ok || track == MUS_NONE || track >= MUS_COUNT) {
        mus_on = 0;
        voice_off(V_MUS);
        return;
    }
    mus_head = mus_base + mus_offset[track];
    mus = mus_head;
    mus_on = 1;
    note_left = 0;                  /* zero forces the first note on the next tick */
}

void snd_effect(uint8_t track)
{
    if (!enabled || !mus_ok || track >= MUS_COUNT)
        return;
    sfx = mus_base + mus_offset[track];
    sfx_on = 1;
    sfx_left = 0;
}

/* 440Hz for a quarter of a second, blocking, as in sid.c. 250ms is 15 frames
   at 60Hz and 12.5 at 50, and the C128 settled on 13 there; so does this. */
#define BEEP_TENTHS      44
#define BEEP_FRAMES_NTSC 15
#define BEEP_FRAMES_PAL  13

void snd_beep(void)
{
    unsigned int t0;
    unsigned char n;

    if (!enabled)
        return;
    sfx_on = 0;                     /* a refusal cancels whatever else was playing */
    voice_note(V_SFX, BEEP_TENTHS);
    n = snd_region == REGION_PAL ? BEEP_FRAMES_PAL : BEEP_FRAMES_NTSC;
    t0 = jiffy();
    while ((unsigned int)(jiffy() - t0) < n)
        ;
    voice_off(V_SFX);
}

/* One 18.2065Hz tick -- the same function as falconsnd.c's, reading the
   same bytes out of the same file. */
static void music_tick(void)
{
    unsigned char note[2];

    if (mus_on) {
        if (note_left) note_left--;
        if (!note_left) {
            far_read(mus, note, 2);
            if (note[0] == 0) {          /* zero duration ends the track */
                mus = mus_head;          /* and this port loops until stopped */
                far_read(mus, note, 2);
                if (note[0] == 0) { mus_on = 0; voice_off(V_MUS); return; }
            }
            note_left = note[0];
            voice_note(V_MUS, note[1]);
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

/* Catching up rather than dropping what was missed, so a slow redraw does
   not lose the beat -- but capped at half a second, because a disk load that
   took ten should not then play ten seconds of music at once. One step is
   under a whole tick, so each jiffy yields at most one. */
void snd_poll(void)
{
    unsigned int now, elapsed;

    if (!enabled || !mus_ok)
        return;
    now = jiffy();
    elapsed = now - last_jiffy;
    if (!elapsed)
        return;
    last_jiffy = now;
    if (elapsed > 30) elapsed = 30;
    while (elapsed--) {
        acc += step;
        if (acc >= TICK_ONE) {
            acc -= TICK_ONE;
            music_tick();
        }
    }
}

uint8_t snd_enabled(void) { return enabled; }

void snd_toggle(void)
{
    enabled = (unsigned char)!enabled;
    if (!enabled) {
        voice_off(V_MUS);
        voice_off(V_SFX);
    }
}
