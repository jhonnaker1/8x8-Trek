/* DOES IT PLAY THE RIGHT NOTES, AND AT THE RIGHT SPEED?
 *
 * TWO QUESTIONS, AND ONE TOOL CANNOT ANSWER BOTH. #42 on this project's list
 * is a session where every burst frequency measured correct while the tune
 * ran at EXACTLY DOUBLE SPEED -- a pitch tool is silent about tempo, and "it
 * makes the right notes" is not "it makes them at the right time". So this
 * does both: four tone bursts for hearit.py to measure, then a synthetic
 * track whose frames and ticks the host counts against its own clock.
 *
 * THREE PITCHES, NOT ONE. A single check cannot see an octave error and this
 * project has shipped one in each direction -- the X16 an octave FLAT for
 * four months, the CoCo 3's first build an octave SHARP.
 *
 * AND THE SN76489 IS THE OTHER WAY ROUND FROM THE PLUS/4: N is a DIVIDER, so
 * it falls as pitch rises, where TED's register rises. An inverted formula
 * plays a tune's intervals inside out -- wrong without sounding broken, and
 * only a measurement separates that from a tuning error.
 *
 * Includes the driver rather than linking it, so it can reach voice_note and
 * hold a known pitch. That is the Plus/4's sndtest pattern.
 */
#include "f256snd.c"

TREK_SIGNATURE;
__attribute__((used, retain)) volatile unsigned char ran;
/* The host reads these against its own clock: 60 frames and 18.2065 ticks a
   second is the answer. They are the driver's own counters, not a copy. */
__attribute__((used, retain)) volatile unsigned char tempo_running;

/* Wait n frames off the kernel counter, BOUNDED -- an unbounded wait on a
   frame source that has died is a probe that hangs silently. */
static void frames_wait(unsigned int n)
{
    unsigned char last = f256_frames();
    unsigned int guard = 0;
    while (n) {
        unsigned char now = f256_frames();
        unsigned char d = (unsigned char)(now - last);
        if (d) { last = now; n = (n > d) ? (unsigned int)(n - d) : 0; guard = 0; }
        else if (++guard == 0) return;
    }
}

int main(void)
{
    ran = 0x11;
    snd_init();

    /* A second each with a clear half-second gap, so the analysis can segment
       the bursts without guessing where one ended. Voice 1 -- the music
       voice -- at the three calibration points. */
    voice_note(0, 44);  frames_wait(60); voice_off(0); frames_wait(30);
    voice_note(0, 100); frames_wait(60); voice_off(0); frames_wait(30);
    voice_note(0, 20);  frames_wait(60); voice_off(0); frames_wait(30);

    /* And voice 2, to prove the second generator is separately addressable --
       which is the whole basis of "a hit during the title track does not chop
       the tune". A driver that wrote both notes to one channel would produce
       three correct bursts above and a fourth correct one here. */
    voice_note(1, 44);  frames_wait(60); voice_off(1); frames_wait(30);

    /* THE TEMPO HALF. A synthetic two-note loop rather than MUSIC.DAT: the
       question is whether frames become ticks at the right rate, and a real
       track would make that depend on far memory and the string pool being
       right as well. Reaching into the driver's own state is what including
       it buys. */
    mus_buf[0] = 4; mus_buf[1] = 44;     /* four ticks of A440 */
    mus_buf[2] = 4; mus_buf[3] = 0;      /* four ticks of rest */
    mus_buf[4] = 0;                      /* zero duration: loop from the head */
    mus_ok = 1;
    mus_head = 0;
    mus = 0;
    mus_on = 1;
    note_left = 0;
    acc = 0;
    snd_frames = 0;
    snd_ticks = 0;
    ran = 0x5A;
    tempo_running = 1;

    for (;;) snd_poll();
}
