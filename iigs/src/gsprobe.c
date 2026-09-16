/* PROBE 1 (third form): sixteen colour bands in SHR 320 mode.
 *
 * The nested band/page loop of the first two forms produced bands exactly
 * TWICE the width asked for, and reading the generated code twice did not
 * explain it -- the second reading was worse than the first, because it mixed
 * a disassembly of the PREVIOUS build with symbol addresses from the current
 * one and invented a discrepancy that was not there. bankfill() itself was
 * then tested alone (src/gsfill1.c) and is exact: 256 of 256 bytes in the
 * target page, 0 of 256 in the page above it.
 *
 * So this form does two things at once: ONE FLAT LOOP over 112 pages, and a
 * LOG of the page number every call actually used, in bank 0 where the rig
 * can read it. Whatever the answer is, it is then an observation rather than
 * a reconstruction.
 */
#define ASMVAR __attribute__((used, retain))
ASMVAR __attribute__((section(".zp.bss"))) unsigned char shrp[3];
ASMVAR unsigned char gs_lo, gs_hi, gs_val, gs_pages;

/* THE LOG, and it is a fixed array the linker places rather than an address I
   picked: a probe report written to a hand-chosen address is how the Plus/4
   probe came to scribble on a live system vector. */
ASMVAR unsigned char gs_log[128];
ASMVAR unsigned char gs_vlog[128];
ASMVAR unsigned char gs_logn;
ASMVAR unsigned char gs_done;

#define NEWVIDEO (*(volatile unsigned char *)0xC029)

__attribute__((naked, used, retain, section(".init.040")))
void gs_emul(void) { __asm__ volatile("sec\n\txce"); }
__attribute__((naked, used, retain, section(".init.050")))
void gs_setdb(void) { __asm__ volatile("phk\n\tplb"); }

static void bankpoke(unsigned char lo, unsigned char hi, unsigned char v)
{
    gs_lo = lo; gs_hi = hi; gs_val = v;
    __asm__ volatile(
        "lda gs_lo\n\t"   "sta shrp+0\n\t"
        "lda gs_hi\n\t"   "sta shrp+1\n\t"
        "lda #$e1\n\t"    "sta shrp+2\n\t"
        "ldy #$00\n\t"
        "lda gs_val\n\t"  "sta [shrp],y\n\t"
        : : : "a", "y", "memory");
}

static void bankfill(void)
{
    if (gs_logn < 128) { gs_log[gs_logn] = gs_hi; gs_vlog[gs_logn] = gs_val; gs_logn++; }
    __asm__ volatile(
        "lda #$00\n\t"       "sta shrp+0\n\t"
        "lda gs_hi\n\t"      "sta shrp+1\n\t"
        "lda #$e1\n\t"       "sta shrp+2\n\t"
        "ldx gs_pages\n\t"
        "1:\n\t"
        "ldy #$00\n\t"
        "lda gs_val\n\t"
        "2:\n\t"
        "sta [shrp],y\n\t"
        "iny\n\t"
        "bne 2b\n\t"
        "inc shrp+1\n\t"
        "dex\n\t"
        "bne 1b\n\t"
        : : : "a", "x", "y", "memory");
}

static const unsigned int pal[16] = {
    0x0000, 0x0777, 0x0841, 0x072C, 0x0A00, 0x00C0, 0x000E, 0x0FD0,
    0x0D00, 0x0FA9, 0x0888, 0x00E0, 0x00EE, 0x0FF0, 0x000F, 0x0FFF
};

int main(void)
{
    unsigned int i;
    unsigned char p;

    /* SHR ON FIRST, WHICH IS THE OPPOSITE OF WHAT THIS PROBE DID.
       Drawing into a dark screen and revealing it at the end is the right
       instinct on every other machine here and it is what the first three
       forms did -- and it is the one ordering difference from the 2026-09-05
       Lua measurement, which set $C029 = $C1 BEFORE filling anything and got
       a picture with fifteen distinct colours on this exact rig.
       The readback experiment says why it might matter: with SHR on, a dump
       of bank $E1 $2000+ returns every written page TWICE (124 mismatches of
       125 against 0 with SHR off, same program, one line different). So
       whether SHR is on changes how that region is addressed, and the working
       precedent turns it on first. */
    *(volatile unsigned char *)0xC029 = 0xC1;

    for (i = 0; i < 200; i++)                       /* SCBs: 320 mode, pal 0 */
        bankpoke((unsigned char)i, 0x9D, 0x00);

    for (i = 0; i < 16; i++) {                      /* palette 0 */
        bankpoke((unsigned char)(i * 2),     0x9E, (unsigned char)(pal[i] & 0xFF));
        bankpoke((unsigned char)(i * 2 + 1), 0x9E, (unsigned char)(pal[i] >> 8));
    }

    /* 112 pages, seven per band, one loop. */
    for (p = 0; p < 112; p++) {
        unsigned char band = (unsigned char)(p / 7);
        gs_val   = (unsigned char)(band | (band << 4));
        gs_hi    = (unsigned char)(0x20 + p);
        gs_pages = 1;
        bankfill();
    }

    gs_done = 0x5A;
    for (;;) ;
    return 0;
}
