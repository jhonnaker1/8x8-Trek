/* IS $0400-$1FFF OURS? Seven kilobytes of slot 0, and the linker script has
 * been calling it "MCP leaves this" on no evidence at all.
 *
 * IT MATTERS BECAUSE IT IS RESIDENT ADDRESS SPACE, not banked. The shared UI
 * links to 58,661 bytes of .text against 40,960 of address space, so 17,701
 * has to be banked; 7K here takes a bite out of that number and costs nothing
 * at runtime, because unlike a bank it is always mapped.
 *
 * THE PLUS/4 IS WHY THIS PROBE HAS TWO HALVES. Its $0400-$04FF looked free to
 * an instrument that counted WRITES -- and it was free of writes, because the
 * ROM only ever READ AND EXECUTED the routines living there. The soft stack
 * went in, and startup became sensitive to a 34-byte shift. Instrument #38 in
 * the list is that probe: sound, controlled, complete, and answering a
 * question I had not asked.
 *
 * So:
 *
 *   1. DOES THE KERNEL WRITE HERE? Fill the region with a position-dependent
 *      pattern, exercise the machine, and see what changed. A pattern rather
 *      than a constant so that a byte MOVED from elsewhere in the region is
 *      still a mismatch.
 *   2. DOES THE KERNEL READ OR EXECUTE HERE? A sentinel cannot see that -- so
 *      DESTROY the region and find out whether anything notices. If FoenixMCP
 *      keeps code or a table in these 7K, filling them with garbage and then
 *      running files, keys and timers through the kernel will break something
 *      visible. Every operation below reports whether it still worked.
 *
 * The stage marker exists because the honest failure mode here is a machine
 * that dies mid-fill, and a probe that dies reports nothing. $11 started,
 * $20 filled, $30 exercised, $5A scanned.
 */
#include <stdint.h>
#include "../../c128/src/vdc.h"
#include "../../c128/src/input.h"
#include "../../core/ega.h"
#include "../../core/storage.h"
#include "f256kern.h"
#include "f256evt.h"

TREK_SIGNATURE;
__attribute__((used, retain)) volatile unsigned char ran;

#define LO_START 0x0400u
#define LO_END   0x2000u
#define LO_SIZE  (LO_END - LO_START)

__attribute__((used, retain)) volatile unsigned int changed;      /* bytes that moved */
#define NSHOW 12
__attribute__((used, retain)) volatile unsigned char ch_lo[NSHOW], ch_hi[NSHOW];
__attribute__((used, retain)) volatile unsigned char ch_was[NSHOW], ch_now[NSHOW];
__attribute__((used, retain)) volatile unsigned char ch_n;
/* Did the kernel still work while its memory was full of garbage? */
__attribute__((used, retain)) volatile unsigned char ok_write, ok_read, ok_bytes;
__attribute__((used, retain)) volatile unsigned char ok_frames, ok_stream;
__attribute__((used, retain)) volatile unsigned int  frames_seen;

static unsigned char buf[300];
static const char NAME[] = "LOWTEST.DAT";

/* POSITION-DEPENDENT, so a byte copied from one part of the region to another
   still reads as wrong. A constant fill would call that a pass. */
static unsigned char pat(unsigned int a)
{
    return (unsigned char)(((a >> 8) * 5u) ^ (a & 0xFFu) ^ 0x5Au);
}

int main(void)
{
    unsigned int a;
    unsigned char st;
    uint16_t got;
    unsigned char f0, f1;

    ran = 0x11;
    vdc_init();
    kb_init();
    scr_puts(2, 1, "F256K LOW MEMORY PROBE -- IS $0400-$1FFF OURS", EGA_YELLOW);
    scr_puts(2, 2, "FILLING IT WITH GARBAGE, THEN ASKING THE KERNEL TO WORK", EGA_DKGRAY);

    /* 1. DESTROY IT. */
    for (a = LO_START; a < LO_END; a++)
        *(volatile unsigned char *)a = pat(a);
    ran = 0x20;

    /* 2. EXERCISE THE KERNEL HARD. Files, the event queue and the frame timer
          are three different parts of FoenixMCP; a fourth, the display, has
          been running since vdc_init. */
    f0 = f256_frames();
    for (a = 0; a < 300; a++) buf[a] = (unsigned char)(a ^ 0x3C);

    st = plat_write_all(NAME, buf, 300);
    ok_write = (unsigned char)(st == STOR_OK);

    for (a = 0; a < 300; a++) buf[a] = 0;
    st = plat_read_all(NAME, buf, 300, &got);
    ok_read = (unsigned char)(st == STOR_OK && got == 300);
    ok_bytes = 1;
    for (a = 0; a < 300; a++)
        if (buf[a] != (unsigned char)(a ^ 0x3C)) { ok_bytes = 0; break; }

    /* The streaming path too -- it is a different sequence of kernel calls. */
    ok_stream = 0;
    if (plat_open(NAME) == STOR_OK) {
        uint16_t n = plat_read(buf, 300);
        plat_close();
        ok_stream = (unsigned char)(n == 300);
    }

    /* And the timer, over enough frames that a broken one is obvious. */
    for (a = 0; a < 20000u; a++) { f256_pump(); }
    f1 = f256_frames();
    frames_seen = (unsigned char)(f1 - f0);
    ok_frames = (unsigned char)(frames_seen > 0);
    ran = 0x30;

    /* 3. WHAT MOVED. */
    changed = 0;
    for (a = LO_START; a < LO_END; a++) {
        unsigned char now = *(volatile unsigned char *)a;
        if (now != pat(a)) {
            changed++;
            if (ch_n < NSHOW) {
                ch_lo[ch_n] = (unsigned char)a;
                ch_hi[ch_n] = (unsigned char)(a >> 8);
                ch_was[ch_n] = pat(a);
                ch_now[ch_n] = now;
                ch_n++;
            }
        }
    }

    scr_puts(2, 4, "DONE -- SEE THE HOST", EGA_LTGREEN);
    ran = 0x5A;
    for (;;) { f256_pump(); }
}
