/* Sound for the Macintosh: the Sound Manager's square-wave synthesizer, on
 * two channels.
 *
 * THE SHAPE IS falcon/src/falconsnd.c's, and so is most of the logic: music
 * on one voice and effects on the other, the track format read two bytes at
 * a time out of far memory, a tick derived from a CLOCK rather than from how
 * often snd_poll() happens to be called. What differs is the chip, which
 * here is not a chip at all but a Toolbox call.
 *
 * WHAT WAS MEASURED BEFORE THIS WAS WRITTEN (mac/src/smoke.c, QEMU's Quadra
 * 800, System 7.6.1, recorded through its wav audiodev):
 *   - TWO noteSynth channels open at once and MIX -- 220/440/880 on one
 *     while 329.4 held on the other, equal amplitude. So the split every port
 *     with two voices makes, a laser not cutting the music off, holds here.
 *   - MIDI note 69 is 440.0 Hz, and 57 and 81 are 220.0 and 880.0: three
 *     points, so neither the scale nor the offset is assumed.
 *   - odd harmonics only: a square wave, which is what the original's PC
 *     speaker made.
 * src/midinote.c turns the track format's Hz/10 into those note numbers.
 *
 * A NOTE IS STARTED, NOT SCHEDULED. noteCmd takes a duration, but the track's
 * durations are ticks of the PC's 18.2 Hz timer and the tick is counted here,
 * so each note is given far longer than it will ever sound and the next one
 * replaces it: flushCmd and quietCmd IMMEDIATELY (so nothing queued waits
 * behind the old note), then the new noteCmd. commodore-uno/mac's start()
 * does the same for its effects.
 *
 * THE CLOCK IS TickCount(), 60.15 per second. The Falcon reads _hz_200; here
 * the arithmetic is the same with a different divisor: 18.2065 track ticks a
 * second against 60.1474 Mac ticks is 18207 / 60147 of a track tick per Mac
 * tick, in integers, catching up after a slow moment but capped so a long
 * stall does not play seconds of music at once.
 */
#include <Sound.h>
#include <Events.h>
#include <OSUtils.h>

#include "../../c128/src/sid.h"
#include "../../core/farmem.h"
#include "music_data.h"
#include "midinote.h"

#define V_MUS 0
#define V_SFX 1
#define AMP   0xC0          /* of 255: uno's level, loud enough, short of harsh */

/* Far longer than any note, in noteCmd's half-milliseconds; the next note or
   a quietCmd ends it. 32767 is the most a short holds: sixteen seconds. */
#define HOLD  32767

uint8_t snd_region = REGION_NTSC;   /* the seam's; nothing here depends on it */

static SndChannelPtr chan[2];
static unsigned char midi[256];
static unsigned char enabled = 1;

static unsigned int  mus_base, mus_head, mus, sfx;
static unsigned char mus_ok, mus_on, sfx_on;
static unsigned char note_left, sfx_left;

static unsigned long last_tick;
static unsigned long acc;

static void immediate(SndChannelPtr c, short cmd)
{
    SndCommand sc;
    sc.cmd = cmd;
    sc.param1 = 0;
    sc.param2 = 0;
    SndDoImmediate(c, &sc);
}

static void voice_off(unsigned char v)
{
    if (!chan[v]) return;
    immediate(chan[v], flushCmd);
    immediate(chan[v], quietCmd);
}

/* `tenths` is the track format's frequency: tens of Hz, 0 for a rest. */
static void voice_note(unsigned char v, unsigned char tenths)
{
    SndCommand sc;

    voice_off(v);
    if (!enabled || tenths == 0 || !chan[v])
        return;
    sc.cmd = noteCmd;
    sc.param1 = HOLD;
    sc.param2 = ((long)AMP << 24) | midi[tenths];
    SndDoCommand(chan[v], &sc, false);
}

void snd_init(void)
{
    int v;

    midi_table(midi);
    for (v = 0; v < 2; v++)
        if (SndNewChannel(&chan[v], noteSynth, 0, NULL) != noErr)
            chan[v] = NULL;
    last_tick = TickCount();
    acc = 0;
}

/* THE WAY OUT: silence, then give the channels back. A Sound Manager
   channel belongs to the application's heap and the Toolbox expects it
   disposed before the application quits -- macvid.c's Quit calls this too. */
void snd_off(void)
{
    int v;

    mus_on = sfx_on = 0;
    for (v = 0; v < 2; v++)
        if (chan[v]) {
            voice_off((unsigned char)v);
            SndDisposeChannel(chan[v], true);
            chan[v] = NULL;
        }
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
    mus_head = (unsigned int)(mus_base + mus_offset[track]);
    mus = mus_head;
    mus_on = 1;
    note_left = 0;              /* zero forces the first note on the next tick */
}

void snd_effect(uint8_t track)
{
    if (!enabled || !mus_ok || track >= MUS_COUNT)
        return;
    sfx = (unsigned int)(mus_base + mus_offset[track]);
    sfx_on = 1;
    sfx_left = 0;
}

/* The original's refusal: 440 Hz for 250 ms, BLOCKING, as on every port.
   15 Mac ticks is 249 ms. Without a channel the Mac's own beep stands in,
   so a refusal is never silent. */
#define BEEP_TENTHS 44
#define BEEP_TICKS  15

void snd_beep(void)
{
    unsigned long t0;

    if (!enabled)
        return;
    if (!chan[V_SFX]) { SysBeep(10); return; }
    sfx_on = 0;                 /* a refusal cancels whatever else was playing */
    voice_note(V_SFX, BEEP_TENTHS);
    t0 = TickCount();
    while (TickCount() - t0 < BEEP_TICKS)
        ;
    voice_off(V_SFX);
}

/* One 18.2065 Hz tick -- the same function as falconsnd.c's. */
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

void snd_poll(void)
{
    unsigned long now, elapsed;

    if (!enabled || !mus_ok)
        return;

    now = TickCount();
    elapsed = now - last_tick;
    if (!elapsed)
        return;
    last_tick = now;

    /* Capped at about 1.7 s, as the Falcon caps at half a second of its
       faster clock: a briefing page that took ten seconds to read should not
       then play ten seconds of music at once. */
    if (elapsed > 100) elapsed = 100;
    acc += elapsed * 18207UL;
    while (acc >= 60147UL) {
        acc -= 60147UL;
        music_tick();
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
