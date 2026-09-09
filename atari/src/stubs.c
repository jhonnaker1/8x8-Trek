/* EARLY-LINK STUBS, and their only job is to be UNFOLDABLE.
 *
 * The scope for this target ends with an instruction: "Measure it properly
 * before committing: link the whole game early." The question it answers is
 * whether EGA Trek fits in $4000..$BFFF -- 32,768 bytes, about 4.8K LESS than
 * the C128, which holds the game with 211 bytes spare. That decides how much
 * has to move into overlays before a line of real driver code is worth
 * writing, and it is cheap to find out NOW and expensive to find out later.
 *
 * THE X16 GOT THIS MEASUREMENT WRONG ONCE, and the way it went wrong is why
 * every function here writes through a `volatile`. Its early link measured
 * 1,564 bytes -- the whole game, apparently, in a tenth of the space. The
 * stubs returned constants, LTO proved the results unused, and the optimiser
 * deleted the game rather than the stubs. A stub that cannot be reasoned about
 * is the only kind that measures anything.
 *
 * These are DELIBERATELY not the real drivers. They are the shape of the seams
 * with nothing behind them, so the number they produce is the size of the
 * GAME: main.c, ui.c, layout.c, strpool.c and all of core/, with the platform
 * half stubbed to nearly nothing. The real drivers add to it.
 */
#include <stdint.h>
#include <string.h>

#include "../../c128/src/vdc.h"
#include "../../c128/src/input.h"
#include "../../c128/src/sid.h"
#include "../../core/storage.h"
#include "../../core/farmem.h"
#include "../../core/overlay.h"

/* EVERY stub touches this. The optimiser cannot prove a volatile write is
   unobservable, so it cannot fold a caller away on the grounds that the
   callee does nothing -- which is exactly what happened on the X16. */
volatile unsigned char sink;

/* AS EACH DRIVER LANDS, ITS STUBS DROP OUT and `make early` starts reporting
   the REAL position rather than the game's half of it. The Makefile defines
   these for whichever seams are built for real in that link; the budget is
   only honest if the two halves cannot both be present. */
#ifndef ATARI_HAVE_VIDEO
/* ---- video ---------------------------------------------------------- */
void vdc_init(void)      { sink = 1; }
void vdc_shutdown(void)  { sink = 2; }
void plat_exit(void)     { sink = 3; }
void wait_vsync(void)    { sink = 4; }
void scr_clear(void)     { sink = 5; }

void scr_put(unsigned char x, unsigned char y, unsigned char ch, unsigned char color) {
    sink = (unsigned char)(x ^ y ^ ch ^ color);
}
void scr_puts(unsigned char x, unsigned char y, const char *s, unsigned char color) {
    sink = (unsigned char)(x ^ y ^ color ^ (unsigned char)*s);
}
void scr_fill_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h,
                   unsigned char ch, unsigned char color) {
    sink = (unsigned char)(x ^ y ^ w ^ h ^ ch ^ color);
}
void scr_hline(unsigned char x, unsigned char y, unsigned char w,
               unsigned char ch, unsigned char color) {
    sink = (unsigned char)(x ^ y ^ w ^ ch ^ color);
}
void scr_vline(unsigned char x, unsigned char y, unsigned char h,
               unsigned char ch, unsigned char color) {
    sink = (unsigned char)(x ^ y ^ h ^ ch ^ color);
}

/* The message log's backing store -- spare video RAM on every port that has
   any, and VBXE has 512K of it. Stubbed as a cursor and nothing behind it. */
void vdc_set_address(unsigned int addr) { sink = (unsigned char)addr; }
void vdc_data_write(unsigned char v)    { sink = v; }
unsigned char vdc_data_read(void)       { return sink; }

#endif /* ATARI_HAVE_VIDEO */

#ifndef ATARI_HAVE_INPUT
/* ---- input ---------------------------------------------------------- */
uint16_t kb_entropy;
void kb_init(void)    { sink = 6; }
char kb_waitkey(void) { sink = 7; return (char)sink; }
#endif /* ATARI_HAVE_INPUT */

#ifndef ATARI_HAVE_SOUND
/* ---- sound ---------------------------------------------------------- */
uint8_t snd_region = REGION_PAL;
void snd_init(void)   { sink = 8; }
void snd_off(void)    { sink = 9; }
void snd_music_data(unsigned int base, unsigned char ok) { sink = (unsigned char)(base ^ ok); }
void snd_music(uint8_t t)  { sink = t; }
void snd_effect(uint8_t t) { sink = t; }
void snd_beep(void)   { sink = 10; }
void snd_poll(void)   { sink = 11; }
uint8_t snd_enabled(void) { return sink; }
void snd_toggle(void) { sink = 12; }

#endif /* ATARI_HAVE_SOUND */

/* ---- storage -------------------------------------------------------- */
uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got) {
    sink = (unsigned char)((unsigned char)*name ^ (unsigned char)max);
    *got = 0;
    memset(buf, 0, 1);
    return sink ? STOR_NOTFOUND : STOR_OK;
}
uint8_t plat_write_all(const char *name, const void *buf, uint16_t len) {
    sink = (unsigned char)((unsigned char)*name ^ (unsigned char)len
                           ^ *(const unsigned char *)buf);
    return sink ? STOR_ERROR : STOR_OK;
}
uint8_t plat_open(const char *name) { sink = (unsigned char)*name; return sink ? STOR_NOTFOUND : STOR_OK; }
/* `return 0` HERE DELETED THE GAME on 2026-09-09, the moment far_load became
   real: a literal zero let LTO prove the read loop never ran, so far_load
   always returned FAR_NONE, so ovl_init always reached its noreturn die(),
   so everything main() does after its first ovl_load was unreachable. The
   early link reported 1,507 bytes -- the X16's 1,564-byte reading again,
   arriving through a different door. A stub is unfoldable only while nothing
   downstream of it is real; check it again each time a consumer lands. */
uint16_t plat_read(void *buf, uint16_t len) {
    sink = (unsigned char)len;
    memset(buf, 0, 1);
    return sink ? len : 0;
}
void plat_close(void) { sink = 13; }

#ifndef ATARI_HAVE_FARMEM
/* ---- far memory ----------------------------------------------------- */
uint16_t far_load(const char *name) { sink = (unsigned char)*name; return FAR_NONE; }
uint16_t far_size(void) { return sink; }
void far_read(uint16_t off, void *dst, uint8_t len) {
    sink = (unsigned char)(off ^ len);
    memset(dst, 0, len);
}
#endif /* ATARI_HAVE_FARMEM */

#ifndef ATARI_HAVE_OVERLAY
/* ---- overlays ------------------------------------------------------- */
void ovl_load(uint8_t which) { sink = which; }
#endif /* ATARI_HAVE_OVERLAY */
