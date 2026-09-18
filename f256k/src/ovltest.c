/* Eleven overlays, eleven banks, and a swap that is one store.
 *
 * WHAT MAKES THIS TEST WORTH ANYTHING is that every overlay function returns
 * ITS OWN NUMBER. All eleven are linked at the same address, $A000, so if the
 * window is pointed at the wrong bank the call still succeeds, still returns,
 * and returns the WRONG VALUE -- which is precisely the failure mode
 * overlay.h describes as "the call went to the address trek_score_sheet has
 * in the EVAL layout, the shorter hof image does not reach that far, and the
 * CPU ran into unwritten bytes". A test that only checked it did not crash
 * would pass on a completely wrong mapping.
 *
 * It also swaps BACKWARDS through all eleven and then interleaves, because a
 * loader that caches `live` wrongly works perfectly on a single pass.
 */
#include <stdint.h>
#include "../../c128/src/vdc.h"
#include "../../core/overlay.h"
#include "../../c128/src/input.h"
#include "../../core/ega.h"
#include "f256kern.h"
#include "f256evt.h"

TREK_SIGNATURE;
__attribute__((used, retain)) volatile unsigned char ran;
__attribute__((used, retain)) volatile unsigned char got[OVL_COUNT];
__attribute__((used, retain)) volatile unsigned char got_back[OVL_COUNT];
__attribute__((used, retain)) volatile unsigned char got_mixed[8];
__attribute__((used, retain)) volatile unsigned char pass;

/* `used, retain` on top of OVL_CODE: the attribute gives noinline and the
   section, but nothing references these through a pointer, and LTO will drop
   a static function it can prove unreachable -- leaving an EMPTY overlay,
   which is rule 1 in overlay.h wearing a different hat.
 *
 * AND THE BODY MUST TOUCH SOMETHING VOLATILE, which cost a control run to
 * find. These were `return 0xE0 + n;` -- constant functions -- and the whole
 * test PASSED with ovl_load deliberately mapping the wrong bank. noinline
 * stops the BODY being inlined; it does not stop interprocedural constant
 * propagation from replacing the CALL with the value LTO proved it returns.
 * Eleven overlays verified without one byte being executed from the window.
 *
 * A volatile read cannot be proved away, so the call has to happen -- and it
 * has to happen at $A000, through whatever bank is mapped there. */
__attribute__((used, retain)) volatile unsigned char ovl_seed;

#define OVLFN(sec, name, val) \
    __attribute__((used, retain)) OVL_CODE(sec) \
    unsigned char name(void) { return (unsigned char)((val) ^ ovl_seed); }

OVLFN("eval",   t_eval,   0xE0)
OVLFN("hof",    t_hof,    0xE1)
OVLFN("front",  t_front,  0xE2)
OVLFN("info",   t_info,   0xE3)
OVLFN("repair", t_repair, 0xE4)
OVLFN("msgs",   t_msgs,   0xE5)
OVLFN("planet", t_planet, 0xE6)
OVLFN("cmds",   t_cmds,   0xE7)
OVLFN("title",  t_title,  0xE8)
OVLFN("events", t_events, 0xE9)
OVLFN("xtra",   t_xtra,   0xEA)
/* The opt-in pair. They are OVL_BASE_COUNT and OVL_BASE_COUNT+1, which is
   only true while the Makefile defines both -- overlay.h keeps the ids
   contiguous by making each one's presence shift the next. */
OVLFN("enemy",  t_enemy,  0xEB)
OVLFN("move",   t_move,   0xEC)

/* Resident, and the ONLY place that pairs a load with a call -- overlay.h's
   rule 3. Written as a switch rather than a table of pointers because a
   pointer table would hold eleven addresses that are all $A000-something and
   all equally valid-looking. */
static unsigned char call_ovl(unsigned char n)
{
    switch (n) {
        case OVL_EVAL:   return t_eval();
        case OVL_HOF:    return t_hof();
        case OVL_FRONT:  return t_front();
        case OVL_INFO:   return t_info();
        case OVL_REPAIR: return t_repair();
        case OVL_MSGS:   return t_msgs();
        case OVL_PLANET: return t_planet();
        case OVL_CMDS:   return t_cmds();
        case OVL_TITLE:  return t_title();
        case OVL_EVENTS: return t_events();
        case OVL_XTRA:   return t_xtra();
        case OVL_ENEMY:  return t_enemy();
        default:         return t_move();
    }
}

static unsigned char fetch(unsigned char n)
{
    ovl_load(n);
    return call_ovl(n);
}

void ovl_fatal(uint8_t which)
{
    scr_clear();
    scr_puts(2, 2, "OVERLAY LOAD FAILED", EGA_LTRED);
    scr_put(2, 4, (unsigned char)(48 + (which % 10)), EGA_WHITE);
    for (;;) { }
}

static unsigned char hexdig(unsigned char v)
{ return (unsigned char)(v < 10 ? 48 + v : 1 + v - 10); }

int main(void)
{
    unsigned char n, ok = 1;

    ran = 0x11;
    vdc_init();
    kb_init();
    scr_puts(2, 1, "F256K OVERLAY TEST -- THIRTEEN BANKS, ONE SLOT", EGA_YELLOW);
    ran = 0x5A;

    /* Forwards. */
    for (n = 0; n < OVL_COUNT; n++) {
        got[n] = fetch(n);
        if (got[n] != (unsigned char)(0xE0 + n)) ok = 0;
        scr_put((unsigned char)(2 + n * 3), 3, hexdig((unsigned char)(got[n] >> 4)), EGA_LTGRAY);
        scr_put((unsigned char)(3 + n * 3), 3, hexdig((unsigned char)(got[n] & 15)), EGA_WHITE);
    }

    /* Backwards, because a loader that caches `live` wrongly passes one pass. */
    for (n = OVL_COUNT; n-- > 0; ) {
        got_back[n] = fetch(n);
        if (got_back[n] != (unsigned char)(0xE0 + n)) ok = 0;
    }

    /* Interleaved, including LOADING THE SAME ONE TWICE -- ovl_load is
       documented idempotent, so a stub may call it on every entry, and an
       idempotency bug looks like nothing at all until a swap is skipped. */
    got_mixed[0] = fetch(OVL_TITLE);
    got_mixed[1] = fetch(OVL_TITLE);
    got_mixed[2] = fetch(OVL_EVAL);
    got_mixed[3] = fetch(OVL_XTRA);
    got_mixed[4] = fetch(OVL_EVAL);
    got_mixed[5] = fetch(OVL_HOF);
    got_mixed[6] = fetch(OVL_XTRA);
    got_mixed[7] = fetch(OVL_TITLE);
    if (got_mixed[0] != 0xE8 || got_mixed[1] != 0xE8 || got_mixed[2] != 0xE0 ||
        got_mixed[3] != 0xEA || got_mixed[4] != 0xE0 || got_mixed[5] != 0xE1 ||
        got_mixed[6] != 0xEA || got_mixed[7] != 0xE8) ok = 0;

    pass = ok;
    scr_puts(2, 6, ok ? "ALL THIRTEEN SWAPPED CORRECTLY" : "MISMATCH -- SEE THE HOST",
             ok ? EGA_LTGREEN : EGA_LTRED);
    for (;;) { f256_pump(); }
}
