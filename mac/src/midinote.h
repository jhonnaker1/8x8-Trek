#ifndef MIDINOTE_H
#define MIDINOTE_H

/* Fills out[t] with the MIDI note nearest t * 10 Hz, for every byte the
   track format can hold; out[0] is 0, a rest. See midinote.c. */
void midi_table(unsigned char out[256]);

#endif
