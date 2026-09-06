/* Stubs so the WHOLE GAME can be linked before any of these seams is written.
 *
 * The point is one number: does EGA Trek fit the X16's code space? The scope
 * for this port quoted 38,655 bytes from cx16/lib/link.ld, and the Atari has
 * since taught this project that a linker region is NOMINAL, not available --
 * VBXE's window silently owned 8K of the address space there. So link it now
 * and read the real figure, rather than discovering it after the overlays are
 * cut. Nothing here does anything; they exist to satisfy the linker. */
#include <stdint.h>

/* OPAQUE ON PURPOSE. The first version of this file returned constants, and
   -Oz with LTO folded the entire game down to 1,564 bytes: kb_waitkey()
   returning 0 kills the command loop, plat_read_all() failing kills every
   load path, far_read() doing nothing empties the string pool. That is an
   INSTRUMENT ARTEFACT, not a measurement. Routing every return through a
   volatile makes the optimiser keep the code that consumes it. */
volatile unsigned char opaque = 0;
volatile unsigned int  opaque16 = 0;
#include "../../core/farmem.h"

char kb_waitkey(void) { return (char)opaque; }

void snd_init(void) {}
void snd_off(void) {}
void snd_music_data(unsigned int base, unsigned char ok) { (void)base; (void)ok; }
void snd_music(uint8_t t) { (void)t; }
void snd_effect(uint8_t t) { (void)t; }
void snd_beep(void) {}
void snd_poll(void) {}
uint8_t snd_enabled(void) { return opaque; }
void snd_toggle(void) {}


uint16_t far_load(const char *n) { (void)n; return opaque16; }
uint16_t far_size(void) { return opaque16; }
void far_read(uint16_t off, void *dst, uint8_t len) {
    uint8_t i; unsigned char *d = (unsigned char *)dst;
    (void)off;
    for (i = 0; i < len; i++) d[i] = opaque;   /* really writes: pool stays live */
}

/* Without TREK_OVERLAYS the ten OVL_LOADER stubs in main.c still call this;
   linking everything resident is what makes the total meaningful. */
void ovl_load(unsigned char id) { (void)id; }

/* A VARIABLE, not a function -- which is why the scope's "platform surface,
   derived not assumed" missed it: that grep looked for call syntax only.
   main.c seeds the RNG from it, and it counts across games. */
uint16_t kb_entropy;
