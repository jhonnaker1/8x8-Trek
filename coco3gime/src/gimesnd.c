/* Sound for a bare CoCo 3: the 6-bit DAC, paced by the GIME's timer.
 *
 * ONE VOICE, AND THAT IS THE MACHINE, NOT A SHORTCUT. Every other port here
 * has a sound chip that holds a pitch in a register, so music and effects get
 * a voice each and a hit does not chop the tune -- c128/src/sid.h calls that
 * the one place these ports beat the original outright. This machine has a
 * DAC and nothing else: a tone exists only while something toggles it, and
 * two tones at once would mean summing them in the interrupt at a fixed high
 * rate. At the ~16 kHz that needs, the handler costs better than a third of a
 * 1.79 MHz 6809, and this is already the slowest port to repaint. So the DAC
 * carries one note and an effect preempts the music, which is exactly what
 * the original's single PC speaker did.
 *
 * THE TIMER, AND THE CLOCK IS 3.58 MHz, WHICH IS NOT WHAT I ASSUMED. INIT1
 * bit 5 (TINS) selects 14.31818 MHz / 4 = 3,579,545 Hz. I built this against
 * 14.31818 / 8 and every note came out an OCTAVE SHARP -- 880, 2000 and 400
 * where 440, 1000 and 200 were wanted. Measured off a recording, not guessed
 * back from a datasheet, and it is the fourth time on this target that a
 * plausible number had to be replaced by a measured one.
 *
 * A square wave needs a toggle every half period, so
 *
 *     counts = 3579545 / (2 * f)   and the format's pitch is TENS of Hz,
 *     counts = 178977 / tenths
 *
 * which does not fit sixteen bits, and neither does a half of it. A QUARTER
 * does, and the shift back is exact:
 *
 *     counts = ((44744 + tenths/2) / tenths) * 4
 *
 * CALIBRATED AT THREE POINTS, because this project shipped an octave flat for
 * four months off a single-point check -- and because the octave error above
 * is precisely what one point can miss:
 *
 *     tenths  44 -> 4068 counts -> 440.0 Hz   (wanted 440)
 *     tenths 100 -> 1788 counts -> 1001.0 Hz  (wanted 1000)
 *     tenths  20 -> 8948 counts -> 200.0 Hz   (wanted 200)
 *
 * all inside 0.1%, against a semitone of 5.9%. The timer is twelve bits, so
 * anything below 437 Hz -- which is most of the music -- overflows it, and
 * halving the count while doubling a software divisor in the handler reaches
 * those EXACTLY: a count that is always a multiple of four survives two
 * halvings with nothing lost.
 *
 * THE INTERRUPT IS IRQ AND NOT FIRQ, AND THAT IS A cmoc CONSTRAINT. cmoc's
 * `interrupt` functions end in RTI and SAVE NOTHING. An IRQ stacks the entire
 * register set and RTI restores it, so a clobbered register is harmless; an
 * FIRQ stacks only PC and CC, and the same function would quietly corrupt
 * whatever it interrupted.
 *
 * **THE PIAs SHARE THE IRQ LINE AND THE GIME'S IEN DOES NOT GATE THEM.** Disk
 * BASIC leaves PIA0's 60 Hz field-sync interrupt enabled, so simply clearing
 * the CPU's I flag takes a PIA interrupt that reading $FF92 does nothing
 * about -- immediately, and for ever. Measured as a hard wedge with the
 * handler demonstrably being entered (src/tmr5.c, src/tmr6.c). Disabling the
 * enables is not enough either: a latched flag holds the line down, so the
 * DATA registers have to be read to clear it.
 *
 * TEMPO COMES OFF V-BORD ON THE OTHER REGISTER. A GIME source latches its
 * flag from the ENABLE MASK alone -- IEN and FEN only gate the CPU line -- so
 * V-BORD is armed in $FF93 with FEN clear and polled, where the handler's
 * read of $FF92 cannot steal it.
 *
 * MUSIC.DAT IS LIFTED INTO RAM ONCE. This port's far memory IS THE DISK
 * (gimemem.c), and fetching two bytes per note would seek the drive several
 * times a second underneath a driver that has an interrupt running. The file
 * is MUS_BYTES -- 412 today, and the number comes from the generator so it
 * cannot go stale behind the data.
 */
#include <stdint.h>

#include "../../c128/src/sid.h"
#include "../../c128/src/sidfreq.h"
#include "../../c128/src/music_data.h"
#include "../../core/farmem.h"
#include "egagime.h"

#define INIT0    (*(unsigned char *)0xFF90)
#define INIT1    (*(unsigned char *)0xFF91)
#define IRQENR   (*(unsigned char *)0xFF92)
#define FIRQENR  (*(unsigned char *)0xFF93)
#define TMRHI    (*(unsigned char *)0xFF94)
#define TMRLO    (*(unsigned char *)0xFF95)

#define PIA0_DA  (*(unsigned char *)0xFF00)
#define PIA0_CRA (*(unsigned char *)0xFF01)
#define PIA0_DB  (*(unsigned char *)0xFF02)
#define PIA0_CRB (*(unsigned char *)0xFF03)
#define PIA1_DA  (*(unsigned char *)0xFF20)
#define PIA1_CRA (*(unsigned char *)0xFF21)
#define PIA1_DB  (*(unsigned char *)0xFF22)
#define PIA1_CRB (*(unsigned char *)0xFF23)

#define IRQ_SLOT ((unsigned char *)0xFEF7)

#define TMR_FLAG   0x20
#define VBORD_FLAG 0x08
#define TMR_MAX    4095U
#define TMR_QTR    44744U         /* 3579545 / 80: a quarter count, in 16 bits */

#define DAC_HI   0xFC             /* the DAC is PIA1 side A bits 7..2 */
#define DAC_LO   0x00

#define BEEP_TENTHS 44            /* 440Hz, the A the other ports beep */
#define BEEP_FRAMES 15            /* 250ms at 60Hz */

uint8_t snd_region = REGION_NTSC; /* the GIME comes up 60Hz here */

static uint8_t enabled = 1;
static unsigned char started;

/* Touched by the interrupt. cmoc has no `volatile`, so everything the handler
   and the mainline share is written through a plain static and read back the
   same way -- and every discarded hardware read goes into `sink`, or the
   compiler is free to drop the load that clears the flag. */
static unsigned char sink;
static unsigned char tog_div, tog_cnt, level, sounding;

static unsigned int acc;
static unsigned int mus, mus_head;
static unsigned char mus_on, mus_ok, note_left;
static unsigned int sfx;
static unsigned char sfx_on, sfx_left;

static unsigned char mus_buf[MUS_BYTES];

/* THE HANDLER. Everything it needs is a static; it reads the flag away,
   counts down the software divisor and flips the DAC between two levels. */
interrupt void snd_tick_irq(void)
{
    sink = IRQENR;
    if (!sounding) return;
    if (--tog_cnt) return;
    tog_cnt = tog_div;
    level = (unsigned char)(level ? 0 : 1);
    PIA1_DA = level ? DAC_HI : DAC_LO;
}

static void pia_quiet(void)
{
    PIA0_CRA = (unsigned char)(PIA0_CRA & 0xFE);
    PIA0_CRB = (unsigned char)(PIA0_CRB & 0xFE);
    PIA1_CRA = (unsigned char)(PIA1_CRA & 0xFE);
    PIA1_CRB = (unsigned char)(PIA1_CRB & 0xFE);
    sink = PIA0_DA;                       /* the READ is what clears a flag */
    sink = PIA0_DB;
    sink = PIA1_DA;
    sink = PIA1_DB;
}

/* THE SOURCE IS ARMED ONLY WHILE A NOTE SOUNDS, and that is not tidiness.
   Writing 0 to the timer does NOT stop it -- snd_init used to leave a zero
   count armed and the machine wedged in an interrupt storm before it ever
   returned, with the handler doing nothing but being re-entered. Disarming in
   $FF92 is the thing that actually goes quiet. */
static void voice_off(void)
{
    sounding = 0;
    IRQENR = 0x00;
    sink = IRQENR;
    TMRHI = 0;
    TMRLO = 0;
    PIA1_DA = DAC_LO;
    level = 0;
}

static void voice_note(unsigned char tenths)
{
    unsigned int counts;
    unsigned char div = 1;

    if (tenths == 0) { voice_off(); return; }        /* a rest */

    /* Rounded, not truncated: one add, and free of the systematic flatness
       truncation gives across a whole tune. */
    counts = (unsigned int)(((TMR_QTR + (tenths >> 1)) / tenths) << 2);
    while (counts > TMR_MAX) { counts >>= 1; div = (unsigned char)(div << 1); }
    if (counts == 0) counts = 1;

    sounding = 0;                         /* the handler must not see a
                                             half-written divisor */
    tog_div = div;
    tog_cnt = div;
    TMRHI = (unsigned char)((counts >> 8) & 0x0F);
    TMRLO = (unsigned char)(counts & 0xFF);
    sink = IRQENR;                        /* drop a flag from the old period */
    IRQENR = TMR_FLAG;
    sounding = 1;
}

void snd_init(void)
{
    voice_off();

    /* The analogue mux picks DAC / cassette / cartridge out of PIA0's CA2 and
       CB2; both low is the DAC. Bit 2 keeps the data register selected, bit 5
       puts the control line in output mode and bit 3 is its level. */
    PIA1_CRA = (unsigned char)(PIA1_CRA & 0xFB);     /* DDRA... */
    PIA1_DA  = 0xFC;                                 /* top six bits out */
    PIA1_CRA = 0x34;                                 /* ...data again, no IRQ */
    PIA0_CRA = 0x34;
    PIA0_CRB = 0x34;
    PIA1_CRB = 0x3C;                                 /* CB2 high: sound on */
    PIA1_DA  = DAC_LO;

    pia_quiet();

    INIT1 = 0x20;                         /* TINS: the 1.7897725 MHz clock */
    IRQENR  = 0x00;                       /* nothing armed until a note */
    FIRQENR = VBORD_FLAG;                 /* armed to LATCH, FEN stays clear */
    sink = IRQENR;
    sink = FIRQENR;

    IRQ_SLOT[0] = 0x7E;                   /* JMP over the ROM's LBRA */
    *((void **)0xFEF8) = (void *)snd_tick_irq;

    INIT0 = GIME_INIT0;                   /* IEN, and MC2 for the drive */
    if (!started) { asm { andcc #$EF } started = 1; }
    acc = 0;
}

void snd_off(void)
{
    voice_off();
    mus_on = 0;
    sfx_on = 0;
}

uint8_t snd_enabled(void) { return enabled; }

void snd_toggle(void)
{
    enabled = (uint8_t)!enabled;
    if (!enabled) snd_off();
}

/* Lifted off the disk ONCE, in chunks, because far_read takes a byte length
   and this port's far store is a drive. */
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
        if (!sfx_on) voice_off();
        return;
    }
    mus_head = mus_offset[track];
    mus = mus_head;
    mus_on = 1;
    note_left = 0;                        /* zero forces a note next tick */
    acc = 0;
}

void snd_effect(uint8_t track)
{
    if (!enabled || !mus_ok || track >= MUS_COUNT) return;
    sfx = mus_offset[track];
    sfx_on = 1;
    sfx_left = 0;
}

/* Non-zero once per field. The flag latches from its mask alone, and reading
   $FF93 is what clears it -- an edge, not a level. */
static unsigned char frame_tick(void)
{
    return (unsigned char)((FIRQENR & VBORD_FLAG) ? 1 : 0);
}

/* THE REFUSAL BEEP, blocking as it is in the original, and BOUNDED. An
   unbounded wait on a frame source that has died is a machine that hangs with
   a voice sounding -- the X16 shipped that once. */
void snd_beep(void)
{
    unsigned char frames = 0;
    unsigned int guard = 0;

    if (!enabled) return;
    sfx_on = 0;                           /* a refusal cancels what was playing */
    sink = FIRQENR;                       /* or the first tick is free */
    voice_note(BEEP_TENTHS);
    while (frames < BEEP_FRAMES) {
        if (frame_tick())      { frames++; guard = 0; }
        else if (++guard == 0) { break; }
    }
    voice_off();
    if (mus_on) note_left = 0;            /* the tune picks itself back up */
}

/* ONE DAC, SO ONE NOTE: both streams keep time, the effect gets the speaker.
   When it ends the music takes the next note it was going to take anyway,
   which is how a single speaker has always had to do it. */
static void music_tick(void)
{
    unsigned char re_sound = 0;

    if (mus_on) {
        if (note_left) note_left--;
        if (!note_left) {
            if (mus_buf[mus] == 0) {          /* zero duration ends a track */
                mus = mus_head;               /* and it loops until stopped */
                if (mus_buf[mus] == 0) { mus_on = 0; if (!sfx_on) voice_off(); }
            }
            if (mus_on) {
                note_left = mus_buf[mus];
                if (!sfx_on) voice_note(mus_buf[mus + 1]);
                mus = (unsigned int)(mus + 2);
            }
        }
    }
    if (sfx_on) {
        if (sfx_left) sfx_left--;
        if (!sfx_left) {
            if (mus_buf[sfx] == 0) {
                sfx_on = 0;
                re_sound = 1;
            } else {
                sfx_left = mus_buf[sfx];
                voice_note(mus_buf[sfx + 1]);
                sfx = (unsigned int)(sfx + 2);
            }
        }
    }
    if (re_sound) {
        if (mus_on) note_left = 0;            /* hand the speaker back */
        else voice_off();
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
