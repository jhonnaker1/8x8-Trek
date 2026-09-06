/* Far memory across the X16's 8K pages. The case that matters is a read that
   STRADDLES a bank boundary -- impossible on the C128, where the far store is
   one flat 64K bank, and therefore untested by anything inherited from it.
   Loads a 6000-byte file TWICE (12,000 bytes, spanning banks 1 and 2), then
   reads across offset 8192 and checks every byte. */
#include <stdint.h>
#include <string.h>
#include "../../core/farmem.h"

void chrout(char c);
__asm__(".global chrout\nchrout:\n jsr $FFD2\n rts\n");
static void say(const char *s) { while (*s) chrout(*s++); }
static void line(const char *s) { say(s); chrout(13); }
static void hex4(uint16_t v) { const char *h="0123456789ABCDEF";
    chrout(h[(v>>12)&15]); chrout(h[(v>>8)&15]); chrout(h[(v>>4)&15]); chrout(h[v&15]); }

#define FSIZE 6000
static unsigned char got[64];

/* what byte `j` of the store must be, given the file was loaded twice */
static unsigned char want(uint16_t j) {
    uint16_t k = (j < FSIZE) ? j : (uint16_t)(j - FSIZE);
    return (unsigned char)((k * 7 + 11) & 0xFF);
}

static void check(const char *tag, uint16_t off, uint8_t len) {
    uint8_t i; uint16_t bad = 0;
    memset(got, 0, sizeof got);
    far_read(off, got, len);
    for (i = 0; i < len; i++) if (got[i] != want((uint16_t)(off + i))) bad++;
    say(tag); say(" off="); hex4(off); say(" bad="); hex4(bad);
    line(bad ? "  FAIL" : "  PASS");
}

int main(void) {
    uint16_t a, b;

    line("FAR: BEGIN");
    a = far_load("FARDATA");
    b = far_load("FARDATA");
    say("FAR: base1="); hex4(a); say(" base2="); hex4(b);
    say(" size="); hex4(far_size()); chrout(13);

    if (a != 0 || b != FSIZE || far_size() != 2 * FSIZE) line("FAR: APPEND FAIL");
    else                                                 line("FAR: APPEND PASS");

    check("FAR: within-bank ", 100,   64);   /* control: no boundary involved */
    check("FAR: at-boundary ", 8160,  64);   /* 8160..8223 -- CROSSES 8192    */
    check("FAR: one-byte-over", 8191,  2);   /* the tightest straddle there is */
    check("FAR: second-bank ", 9000,  64);   /* wholly inside bank 2          */

    for (;;) { }
    return 0;
}
