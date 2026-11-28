/* Sound for the NeXT: two square-wave voices, mixed here into one stream of
 * 16-bit samples and fed to NeXTSTEP's C sound library a chunk at a time.
 *
 * THE FIRST VERSION STARTED ONE SOUND PER NOTE, and Jamie heard it: "awful
 * and choppy". Between two SNDStartPlaying() calls the NeXT's sound output
 * STOPS and starts again, and the gap is audible on every note. My own check
 * missed it because BMNeXT's /audio hands out samples only while the output
 * runs -- the gaps were cut out of the recording (8.3 s of sound from about
 * 9 s of music), so the notes looked back to back. The rule for measuring
 * this: count samples against the wall clock, not notes against each other.
 *
 * SO THE OUTPUT NEVER STOPS WHILE ANYTHING PLAYS. snd_poll() keeps a few
 * short chunks queued ahead of the hardware (SNDStartPlaying with preempt
 * off QUEUES a sound behind the one playing), each rendered here from both
 * voices. A chunk's end is reported by the library's end function, which is
 * what paces the queue -- the sound hardware's clock, not ours.
 *
 * THE MUSIC IS TIMED IN SAMPLES, inside the renderer: a tick of the PC's
 * 18.2065 Hz timer is 2422.2 frames at 44100 Hz. The track format is read two
 * bytes at a time out of far memory exactly as on the Mac and the Falcon, but
 * a slow frame on screen can no longer stretch a note -- only an empty queue
 * can make a gap.
 *
 * TWO VOICES, MIXED: music on one and effects on the other, so a laser does
 * not cut the music off -- the split every port with two voices makes.
 *
 * THE PITCH IS THE NOTE THAT WAS MEANT, not the byte's own frequency:
 * mac/src/midinote.c, linked from the Mac, turns each Hz/10 byte into the
 * nearest equal-tempered semitone (played raw, D4 came out at 290 Hz, 26
 * cents flat), and that note's true frequency is what is rendered.
 */
#include <sound/sound.h>
#include <stdio.h>
#include <math.h>

#include "../../c128/src/sid.h"
#include "../../core/farmem.h"
#include "music_data.h"
#include "nextvid.h"
#include "../../mac/src/midinote.h"

#define RATE   44100
#define AMP    5000         /* per voice, of 32767: two together stay clear */
#define RAMP   88           /* 2 ms to reach or leave full level: no clicks */
#define CHUNK  2048         /* frames, 46 ms */
#define SLOTS  6            /* chunk sounds allocated once, reused */
#define DEPTH  3            /* chunks queued ahead: about 140 ms */
#define TICK_FRAMES_X10 24222UL   /* frames per PC tick, times ten */

uint8_t snd_region = REGION_NTSC;   /* the seam's; nothing here depends on it */

static unsigned char midi[256];      /* track byte -> MIDI note */
static unsigned long step_of[128];   /* MIDI note -> phase step, 16.16 of a cycle */
static unsigned char enabled = 1;
static unsigned char works;          /* the chunks were allocated */
static unsigned char mus_ok;
static unsigned int  mus_base;

/* ---- the voices ---- */

struct voice {
    unsigned char on;           /* reading a track (or holding the beep) */
    unsigned char loop;         /* music loops; effects end */
    unsigned int  head, pos;    /* far offsets: the track's start, next note */
    unsigned long left;         /* frames left in the current note */
    unsigned long step;         /* 0 = a rest */
    unsigned long phase;
    long          amp;          /* current level, ramps toward AMP or 0 */
};
static struct voice v_mus, v_sfx;

static void next_note(struct voice *v)
{
    unsigned char note[2];

    far_read(v->pos, note, 2);
    if (note[0] == 0) {                 /* zero duration ends the track */
        if (!v->loop) { v->on = 0; v->step = 0; return; }
        v->pos = v->head;               /* and the music loops until stopped */
        far_read(v->pos, note, 2);
        if (note[0] == 0) { v->on = 0; v->step = 0; return; }
    }
    v->left = ((unsigned long)note[0] * TICK_FRAMES_X10) / 10UL;
    v->step = note[1] ? step_of[midi[note[1]]] : 0;
    v->pos += 2;
}

/* One sample of one voice, advancing it. */
static long voice_sample(struct voice *v)
{
    long target;

    if (v->on && v->left == 0 && v->head != 0xFFFFu)
        next_note(v);
    target = (v->on && v->step) ? AMP : 0;
    if (v->amp < target) { v->amp += AMP / RAMP; if (v->amp > target) v->amp = target; }
    else if (v->amp > target) { v->amp -= AMP / RAMP; if (v->amp < target) v->amp = target; }
    if (v->on && v->left) {
        v->left--;
        if (v->left == 0 && v->head == 0xFFFFu) { v->on = 0; v->step = 0; }  /* the beep */
    }
    if (v->amp == 0) return 0;
    v->phase += v->step;
    return (v->phase & 0x8000UL) ? -v->amp : v->amp;
}

/* ---- the stream ---- */

static SNDSoundStruct *slot[SLOTS];
static volatile int busy[SLOTS];     /* cleared by the library's end function */
static int next_slot, tag;

/* Called on the sound library's own thread when a chunk has played. */
static int chunk_done(SNDSoundStruct *s, int t, int err)
{
    int i;
    for (i = 0; i < SLOTS; i++)
        if (slot[i] == s) busy[i] = 0;
    return 0;
}

static int in_flight(void)
{
    int i, n = 0;
    for (i = 0; i < SLOTS; i++) n += busy[i] != 0;
    return n;
}

static int sounding(void)
{
    return v_mus.on || v_sfx.on || v_mus.amp || v_sfx.amp;
}

static void queue_chunk(void)
{
    int i, s = next_slot;
    short *p;

    if (busy[s]) return;                /* cannot happen with DEPTH < SLOTS */
    p = (short *)((char *)slot[s] + slot[s]->dataLocation);
    for (i = 0; i < CHUNK; i++) {
        long v = voice_sample(&v_mus) + voice_sample(&v_sfx);
        p[2 * i] = p[2 * i + 1] = (short)v;
    }
    busy[s] = 1;
    if (SNDStartPlaying(slot[s], ++tag, 1, 0, SND_NULL_FUN, chunk_done)
        != SND_ERR_NONE)
        busy[s] = 0;
    next_slot = (s + 1) % SLOTS;
}

/* ---- the seam ---- */

void snd_init(void)
{
    int i, err;

    midi_table(midi);
    for (i = 0; i < 128; i++)
        step_of[i] = (unsigned long)(440.0 * pow(2.0, (i - 69) / 12.0) *
                                     65536.0 / RATE + 0.5);
    for (i = 0; i < SLOTS; i++) {
        err = SNDAlloc(&slot[i], CHUNK * 4, SND_FORMAT_LINEAR_16, RATE, 2, 4);
        if (err != SND_ERR_NONE) {
            fprintf(stderr, "EGA Trek: no sound (%s)\n", SNDSoundError(err));
            return;
        }
    }
    works = 1;
}

/* Keeps DEPTH chunks queued while anything is playing. Called from every
   wait -- kb_waitkey() and wait_vsync() both -- and cheap when idle. */
void snd_poll(void)
{
    if (!works || !enabled) return;
    while (sounding() && in_flight() < DEPTH)
        queue_chunk();
}

void snd_off(void)
{
    int t;
    v_mus.on = v_sfx.on = 0;
    v_mus.amp = v_sfx.amp = 0;
    for (t = tag - SLOTS; t <= tag; t++)
        if (t > 0) SNDStop(t);
}

void snd_music_data(unsigned int base, unsigned char ok)
{
    mus_base = base;
    mus_ok = ok;
}

static void start(struct voice *v, uint8_t track, unsigned char loop)
{
    v->head = (unsigned int)(mus_base + mus_offset[track]);
    v->pos = v->head;
    v->loop = loop;
    v->left = 0;                /* the first note is read on the next sample */
    v->on = 1;
}

void snd_music(uint8_t track)
{
    if (!mus_ok || track == MUS_NONE || track >= MUS_COUNT) {
        v_mus.on = 0;
        return;
    }
    start(&v_mus, track, 1);
    snd_poll();
}

void snd_effect(uint8_t track)
{
    if (!enabled || !mus_ok || track >= MUS_COUNT)
        return;
    start(&v_sfx, track, 0);
    snd_poll();
}

/* The original's refusal: 440 Hz for 250 ms, BLOCKING, as on every port.
   It is the effects voice holding one note -- head 0xFFFF marks a note with
   no track behind it -- and the wait keeps the stream fed. */
void snd_beep(void)
{
    unsigned long t0;

    if (!enabled)
        return;
    v_sfx.head = 0xFFFFu;
    v_sfx.loop = 0;
    v_sfx.left = RATE / 4;
    v_sfx.step = step_of[69];
    v_sfx.on = 1;
    t0 = next_ticks();
    while (next_ticks() - t0 < 15)          /* 250 ms */
        snd_poll();
}

uint8_t snd_enabled(void) { return enabled; }

/* SND off stops feeding the stream, which drains in a seventh of a second;
   the voices keep their place, so SND on picks the music up where it was,
   as on the Mac. */
void snd_toggle(void)
{
    enabled = (unsigned char)!enabled;
}
