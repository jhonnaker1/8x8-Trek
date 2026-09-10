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
/* HOW LONG THE BEEP HOLDS, in ticks of the original's 18.2065Hz clock -- the
   unit this driver already counts in, rather than frames.

   The measured original is 250.6ms. Five ticks is 5/18.2065 = 274.6ms and four
   is 219.7ms, so five is the nearer whole tick, the same way the C128 picks 13
   PAL frames over 12. Not a frame count here on purpose: this machine's raster
   counter wraps TWICE per frame and counting it is what ran the music at
   double speed. */
#define BEEP_TICKS 5

#ifdef TREK_DEBUG_INPUT
/* THE BEEP PROBE, added 2026-09-10 to settle a SOURCE READ against two
 * evenings of play. `snd_beep()` gates V2 on and never gates it off, which
 * reads as a tone that never stops -- but this port has been played by hand
 * twice with no report of one, so the reading is in conflict with the only
 * instrument that has ever been pointed at it.
 *
 * Three things are worth measuring and one is not. NOT worth measuring:
 * whether the code clears the gate, which is settled by reading it. Worth
 * measuring: whether `snd_beep` is REACHED in play, whether anything ELSE on
 * the machine clears V2, and whether the emulated SID reads back at all --
 * the first version of this experiment said "ask Xemu for the gate bit" and
 * that is not obviously available: SID $D400..$D418 are WRITE-ONLY on real
 * hardware, so a readback measures the emulator, not the chip.
 *
 * DEBUG BUILD ONLY. Six bytes of bss the release build never sees. Read with
 *     make drive DRIVE_PEEK="--peek snd_dbg:6" DRIVE_KEYS="..."
 *
 * VOLATILE, AND THE FIRST VERSION WAS NOT. Nothing in the PROGRAM reads these
 * bytes -- only a debugger does -- so LTO split the array into six independent
 * symbols, dropped the three nobody read, and parked two of the survivors in
 * ZERO PAGE. `--peek snd_dbg` then found no such symbol at all. A probe the
 * program never reads is dead code to the optimiser; `volatile` is what tells
 * it the reader is outside. */
volatile uint8_t snd_dbg[6];
#define DBG_BEEPS 0     /* snd_beep() calls */
#define DBG_V2OFF 1     /* voice_off(V2) calls -- the missing statement */
#define DBG_WROTE 2     /* last value THIS code wrote to SID[V2+4] */
#define DBG_ECHO  3     /* SID[V2+4] read straight back after that write */
#define DBG_LATE  4     /* CIA1 tenths from snd_beep to the gate-off: the
                           BEEP'S REAL DURATION, which the tick count cannot
                           give -- this driver's tick calibration is exactly
                           what once ran the music at double speed */
#define DBG_POLLS 5     /* snd_polls since the last beep, saturating */
#define DBG_HIT(i) (snd_dbg[i] = (uint8_t)(snd_dbg[i] < 255 ? snd_dbg[i] + 1 : 255))
#define DBG_V2(val) do { snd_dbg[DBG_WROTE] = (val); \
                         snd_dbg[DBG_ECHO]  = SID[V2 + 4]; } while (0)
#endif

static uint16_t mus, sfx, mus_base;
static uint8_t  mus_on, sfx_on, mus_track, mus_ok;
static uint8_t  note_left, sfx_left;
/* Ticks of refusal beep left to play, 0 = not beeping. See snd_beep(). */
static uint8_t  beep_left;
#ifdef TREK_DEBUG_INPUT
static uint8_t  beep_tod;      /* CIA1 tenths when the beep started */
#endif
static uint8_t  enabled = 1;
static uint16_t acc;
uint8_t snd_region = REGION_NTSC;   /* pitch only; sid.h declares it */
static unsigned int last_raster;

/* THE TICK RATE IS CALIBRATED AT STARTUP, NOT ASSUMED.
 *
 * The C128 driver counts raster wraps and treats each as one frame, because on
 * that machine it is one. HERE IT IS TWO: the MEGA65's native display is 625
 * physical lines, so the VIC-II compatible counter at $D011/$D012 runs 0..311
 * TWICE per frame. Measured 99.8 wraps a second against PAL's 50 -- and the
 * title track, which the data says should run 106 notes over 51.7 seconds,
 * played at nearly double speed. Jamie heard it before any of this was checked.
 *
 * Halving it would have worked here and been a guess about every other machine
 * and video mode. Instead CIA1's time-of-day clock -- ten ticks a second, and
 * measured to run -- is used once at startup to count how many wraps a second
 * actually holds, and the accumulator step follows from that. As a check on
 * the arithmetic: 50 wraps a second yields 364, and sidfreq.h's hand-computed
 * PAL constant is 363. */
#define CIA1_TOD10 (*(volatile unsigned char *)0xDC08)
static uint16_t tick_num;         /* thousandths of an original tick per wrap */
static uint16_t cal_wraps, cal_tenths, cal_window;
static unsigned char cal_tod;

/* Bit 8 of the raster lives in $D011, so the two reads have to agree about
   which half of the frame they are in -- the same guard c128/src/sid.c uses,
   for the same reason. */
static unsigned int raster_line(void) {
    unsigned char c1, c2 = 0, r = 0, tries;

    /* BOUNDED, AND THAT IS THE POINT. This was `for (;;)`, spinning until two
       reads of $D011 agreed about bit 7 -- a hang with no escape, in a routine
       the key loop calls thousands of times a second. If the register ever
       stops behaving, the game stops dead with the screen frozen and the
       current SID note still gated on, which is exactly what Jamie saw at the
       hall of fame on 2026-09-03. NOT REPRODUCED, so this is a hypothesis and
       not a diagnosis -- but an unbounded wait on a hardware read has no place
       here either way.

       Agreement happens on the first pass in every run measured. After eight
       tries, take the reading: the cost is a possible one-line error, and
       frame detection does not care about one line. */
    for (tries = 0; tries < 8; tries++) {
        c1 = VIC_CTRL1;
        r  = VIC_RASTER;
        c2 = VIC_CTRL1;
        if ((c1 & 0x80) == (c2 & 0x80))
            return (unsigned int)r + ((c1 & 0x80) ? 256u : 0u);
    }
    return (unsigned int)r + ((c2 & 0x80) ? 256u : 0u);
}

/* CALIBRATION IS CONTINUOUS, NOT ONCE AT STARTUP, and that is measured rather
   than tidy-minded. A one-shot calibration in snd_init() counted 49 wraps in
   its second while the same code in the running game counted 99.6 -- both
   numbers out of ONE run, so the wrap rate genuinely changes after startup and
   calibrating once, early, gets the wrong half of it.

   So snd_poll() keeps counting against CIA1's time-of-day tenths and re-derives
   the step from the last window. The first window is short, so the title track
   is only briefly wrong; later ones are a second, because a short window's
   count jitters by a wrap or two and that lands straight on the tempo. */
static void recalibrate(uint16_t wraps, uint16_t tenths) {
    unsigned long wps = ((unsigned long)wraps * 10UL) / tenths;
    if (wps >= 20UL && wps <= 2000UL)
        tick_num = (uint16_t)(18206UL / wps);
}

static void voice_off(uint8_t v) {
    SID[v + 4] = GATE_OFF;
#ifdef TREK_DEBUG_INPUT
    if (v == V2) { DBG_HIT(DBG_V2OFF); DBG_V2(GATE_OFF); }
#endif
}

static void voice_note(uint8_t v, uint8_t tenths) {
    unsigned int f;
    if (tenths == 0) {
        SID[v + 4] = GATE_OFF;
#ifdef TREK_DEBUG_INPUT
        if (v == V2) DBG_V2(GATE_OFF);
#endif
        return;
    }
    f = sid_freq(tenths, REGION_NTSC);
    SID[v + 0] = (unsigned char)(f & 0xFF);
    SID[v + 1] = (unsigned char)(f >> 8);
    SID[v + 4] = GATE_ON;
#ifdef TREK_DEBUG_INPUT
    if (v == V2) DBG_V2(GATE_ON);
#endif
}

void snd_init(void) {
    uint8_t i;
    for (i = 0; i < 25; i++) SID[i] = 0;
    SID[24] = 0x0F;
    SID[V1 + 2] = PW_LO; SID[V1 + 3] = PW_HI;
    SID[V1 + 5] = AD_FLAT; SID[V1 + 6] = SR_FLAT;
    SID[V2 + 2] = PW_LO; SID[V2 + 3] = PW_HI;
    SID[V2 + 5] = AD_FLAT; SID[V2 + 6] = SR_FLAT;
    mus_on = sfx_on = 0; beep_left = 0; acc = 0;
    last_raster = 0;
    /* The C128's hand-computed PAL step until the first window closes: one
       frame per wrap, which is what every other machine does. */
    tick_num = snd_tick_num(REGION_PAL);
    cal_wraps = cal_tenths = 0;
    cal_window = 3;                /* 0.3s for the first, a second after */
    cal_tod = CIA1_TOD10;
}

void snd_off(void) { voice_off(V1); voice_off(V2); mus_on = sfx_on = 0; beep_left = 0; }
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
    beep_left = 0;                 /* an effect takes V2 over; see snd_beep */
}
/* THE REFUSAL BEEP, and it used to never stop.
 *
 * This was `voice_note(V2, BEEP_TENTHS); sfx_on = 0; sfx_left = 0;` -- it
 * gated voice 2 on and nothing ever gated it off. `sfx_on = 0` guaranteed it,
 * because tick()'s effects branch is the only other code that touches V2, and
 * `SR_FLAT` holds sustain at 15 with release 0 so there is no envelope to
 * decay through. MEASURED on the machine 2026-09-10, not merely read: an
 * ordinary keypress reached here, the last write to $D40B was $41 GATE_ON, and
 * voice_off(V2) was called zero times afterwards -- against one, and $40, in a
 * run that typed SND. See README, "What is verified, and what is not".
 *
 * The other three ports wait their frames and call voice_off. This one counts
 * the driver's own ticks instead and lets tick() do it, so the beep does not
 * block the caller -- and `enabled` is now tested, which matters more than
 * tidiness: with sound off, snd_poll returns before tick(), so a beep started
 * here would have had nothing left to turn it off. */
void snd_beep(void) {
#ifdef TREK_DEBUG_INPUT
    DBG_HIT(DBG_BEEPS); snd_dbg[DBG_POLLS] = 0;
#endif
    if (!enabled) return;
    sfx_on = 0; sfx_left = 0;      /* a refusal cancels whatever was playing */
    voice_note(V2, BEEP_TENTHS);
    beep_left = BEEP_TICKS;
#ifdef TREK_DEBUG_INPUT
    beep_tod = CIA1_TOD10;
#endif
}

/* One original tick. Called from the frame loop through snd_poll(). */
static void tick(void) {
    /* FIRST, and before the effects branch: a beep and an effect never both
       own V2, because snd_beep clears sfx_on and snd_effect clears this. */
    if (beep_left && --beep_left == 0) {
        voice_off(V2);
#ifdef TREK_DEBUG_INPUT
        /* Tenths actually elapsed, 0..9. The tick count is 5 by construction;
           this is the only number here that could come out wrong. */
        /* ACCUMULATES over every beep in the run, because ONE sample of a
           tenth-resolution clock cannot separate 220ms from 275ms -- the
           phase decides whether either reads as 2 or 3. Divide by DBG_BEEPS. */
        snd_dbg[DBG_LATE] = (uint8_t)(snd_dbg[DBG_LATE]
                                      + (CIA1_TOD10 + 10 - beep_tod) % 10);
#endif
    }
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

#ifdef TREK_DEBUG_INPUT
    DBG_HIT(DBG_POLLS);
#endif

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

    /* CIA1's time-of-day tenths are the only honest clock on this machine --
       measured at 9.8 a second against wall time, while no ROM interrupt runs
       at all. Everything here is counted against it. */
    {
        unsigned char t = CIA1_TOD10;
        if (t != cal_tod) {
            cal_tod = t;
            if (++cal_tenths >= cal_window) {
                recalibrate(cal_wraps, cal_tenths);
                cal_wraps = cal_tenths = 0;
                cal_window = 10;
            }
        }
    }

    if (r >= last_raster) { last_raster = r; return; }
    last_raster = r;
    cal_wraps++;

    acc = (uint16_t)(acc + tick_num);
    while (acc >= SND_TICK_DEN) { acc = (uint16_t)(acc - SND_TICK_DEN); tick(); }
}
