/* Far memory end to end: two tenants, bank straddles, and a read from inside
 * an overlay.
 *
 * THE STRADDLES ARE THE POINT. A bank is 8K and a record does not care where
 * one ends, so the reads here are deliberately placed across $2000 and across
 * $4000. A far_read that mapped one bank and copied `len` bytes would pass
 * every check except those -- one read in thirty-two, arriving late, looking
 * like corrupt data rather than like a boundary.
 *
 * AND THE OVERLAY READ IS THE HAZARD UNIQUE TO THIS MACHINE. Far memory has
 * no slot of its own, so it borrows the overlay window. An overlay calling
 * far_read is therefore asking for the memory it is executing from to be
 * unmapped and put back underneath it. Check 7 does exactly that and then
 * check 8 asks the same overlay whether it survived.
 *
 * The data files are made on the host and copied onto the card, because a
 * 10,000-byte pattern cannot be built in a program whose entire .bss lives in
 * 7K of low memory -- and generating it outside also means the expected bytes
 * are computed twice, independently, rather than compared against themselves.
 */
#include <stdint.h>
#include "../../c128/src/vdc.h"
#include "../../c128/src/input.h"
#include "../../core/ega.h"
#include "../../core/farmem.h"
#include "../../core/overlay.h"
#include "f256kern.h"
#include "f256evt.h"

/* the signature is f256vid.c's now -- one definition for every build */
__attribute__((used, retain)) volatile unsigned char ran;
#define NCHECK 9
__attribute__((used, retain)) volatile unsigned char res[NCHECK];
__attribute__((used, retain)) volatile unsigned int  detail[NCHECK];
__attribute__((used, retain)) volatile unsigned char done_n;

#define BIG  10000U          /* FARTEST.DAT  -- spans two bank boundaries */
#define SMALL  300U          /* FARTEST2.DAT -- the second tenant */

static unsigned char buf[300];
static uint16_t big_base, small_base;

/* The same two patterns tools/mkfar.py writes. Computed here independently so
   a mismatch means the DATA is wrong, not that the test agreed with itself. */
static unsigned char pat_big(uint16_t i)
{ return (unsigned char)((i * 7u) ^ (i >> 3) ^ 0x5Au); }
static unsigned char pat_small(uint16_t i)
{ return (unsigned char)((i * 11u) ^ 0xC3u); }

__attribute__((used, retain)) volatile unsigned char ovl_seed;

#define OVLFN(sec, name, val) \
    __attribute__((used, retain)) OVL_CODE(sec) \
    unsigned char name(void) { return (unsigned char)((val) ^ ovl_seed); }

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
OVLFN("enemy",  t_enemy,  0xEB)
OVLFN("move",   t_move,   0xEC)

/* THE ONE THAT READS FAR MEMORY WHILE IT IS ITSELF IN THE WINDOW. It takes a
   straddling read on purpose -- if borrowing the window corrupted anything,
   the likeliest victim is the code running from it. */
__attribute__((used, retain)) OVL_CODE("eval")
unsigned char t_eval_far(unsigned char *out, uint16_t base)
{
    far_read((uint16_t)(base + 8100u), out, 200);
    return (unsigned char)(0xE0 ^ ovl_seed);
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

static void check(const char *label, unsigned char ok, unsigned int d)
{
    unsigned char y = (unsigned char)(4 + done_n);
    res[done_n] = ok;
    detail[done_n] = d;
    scr_puts(2, y, label, EGA_LTGRAY);
    scr_puts(36, y, ok ? "PASS" : "FAIL", ok ? EGA_LTGREEN : EGA_LTRED);
    scr_put(43, y, hexdig((unsigned char)((d >> 12) & 15)), EGA_DKGRAY);
    scr_put(44, y, hexdig((unsigned char)((d >> 8) & 15)), EGA_DKGRAY);
    scr_put(45, y, hexdig((unsigned char)((d >> 4) & 15)), EGA_DKGRAY);
    scr_put(46, y, hexdig((unsigned char)(d & 15)), EGA_DKGRAY);
    done_n++;
}

/* Compare `n` bytes read from far offset base+at against pat_big. */
static uint16_t cmp_big(uint16_t at, uint8_t n)
{
    uint8_t i;
    far_read((uint16_t)(big_base + at), buf, n);
    for (i = 0; i < n; i++)
        if (buf[i] != pat_big((uint16_t)(at + i))) return (uint16_t)(at + i);
    return 0xFFFFU;
}

int main(void)
{
    uint16_t bad;
    unsigned char v;

    ran = 0x11;
    vdc_init();
    kb_init();
    scr_puts(2, 1, "F256K FAR MEMORY TEST", EGA_YELLOW);
    ran = 0x5A;

    big_base = far_load("FARTEST.DAT");
    check("LOAD 10000 BYTES", big_base == 0, big_base);
    check("FAR_SIZE IS 10000", far_size() == BIG, far_size());

    bad = cmp_big(0, 200);
    check("READ AT THE START", bad == 0xFFFFU, bad);

    /* $2000 is bank 0 -> bank 1. */
    bad = cmp_big(8100, 200);
    check("READ ACROSS BANK 1", bad == 0xFFFFU, bad);

    bad = cmp_big((uint16_t)(BIG - 50), 50);
    check("READ THE LAST 50", bad == 0xFFFFU, bad);

    /* THE SECOND TENANT MUST APPEND. far_load returning 0 twice is the bug
       farmem.h records: the music silently overwrote the prose. */
    small_base = far_load("FARTEST2.DAT");
    check("SECOND TENANT APPENDS", small_base == BIG, small_base);
    {
        uint8_t i; bad = 0xFFFFU;
        far_read((uint16_t)(small_base + 40), buf, 200);
        for (i = 0; i < 200; i++)
            if (buf[i] != pat_small((uint16_t)(40 + i))) { bad = (uint16_t)(40 + i); break; }
        check("SECOND TENANT READS BACK", bad == 0xFFFFU, bad);
    }

    /* THE HAZARD: a far_read issued from code executing in the window. */
    {
        uint8_t i; bad = 0xFFFFU;
        ovl_load(OVL_EVAL);
        v = t_eval_far(buf, big_base);
        for (i = 0; i < 200; i++)
            if (buf[i] != pat_big((uint16_t)(8100 + i))) { bad = (uint16_t)(8100 + i); break; }
        check("far_read FROM AN OVERLAY", bad == 0xFFFFU && v == 0xE0, bad);
    }
    /* And is that overlay still the one that is mapped? */
    ovl_load(OVL_TITLE);
    v = t_title();
    ovl_load(OVL_EVAL);
    check("THE OVERLAY SURVIVED IT",
          v == 0xE8 && t_eval_far(buf, big_base) == 0xE0, v);

    scr_puts(2, (unsigned char)(6 + done_n), "DONE.", EGA_WHITE);
    for (;;) { f256_pump(); }
}
