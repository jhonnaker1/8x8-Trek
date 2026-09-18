/* WHAT IS THE IIgs's VIDEO LINE COUNTER, AND CAN IT TELL 60Hz FROM 50Hz?
 *
 * gssnd.c hardcodes REGION_NTSC with a comment admitting it does not detect.
 * That is not a rounding error: snd_tick_num converts FRAMES to the original's
 * 18.2065Hz ticks, and frames are 60 a second at 60Hz against 50 at 50Hz, so
 * a 50Hz IIgs plays the music about 17% slow. A IIgs sold outside NTSC
 * countries can be switched to 50Hz from the Control Panel.
 *
 * THE COUNTER, from the IIgs Hardware Reference:
 *
 *     $C02E  vertical video address DIVIDED BY 2  (line bits 8..1)
 *     $C02F  bit 7 = the LSB of the vertical address; bits 6..0 horizontal
 *
 * so the line is ($C02E << 1) | ($C02F >> 7), nine bits. In NTSC mode it runs
 * $0FA..$1FF -- 262 lines. At 50Hz there are 312, so it would run $0C8..$1FF.
 *
 * THE MAXIMUM IS $1FF IN BOTH. It is the MINIMUM that discriminates: 250
 * against 200. I assumed the opposite before reading the manual, because on
 * the Plus/4 it was the maximum.
 *
 * MAME HAS NO 50Hz IIgs -- every apple2gs clone is a ROM revision -- so this
 * probe can only establish the NTSC reading and that the register is live at
 * all. That is exactly why the driver must fall back to NTSC on anything it
 * does not clearly recognise.
 *
 * vmin/vmax are the extremes seen; vframes counts VBL falling edges so the
 * frame rate can be sanity-checked against the same run.
 */
#define ASMVAR __attribute__((used, retain))

#define VERTCNT  (*(volatile unsigned char *)0xC02E)
#define HORIZCNT (*(volatile unsigned char *)0xC02F)
#define VBL      (*(volatile unsigned char *)0xC019)

ASMVAR unsigned char vmin_lo, vmin_hi, vmax_lo, vmax_hi;
ASMVAR unsigned char vframes_lo, vframes_hi;
ASMVAR unsigned char gs_done;

/* phk/plb, AND WITHOUT IT THIS PROBE READ AND WROTE BANK $E1. llvm-mos leaves
   the data bank wherever it was -- MAME hands control over with DB=$E1 -- so
   every absolute access here went to $E1/xxxx: the reads fetched $E1/C02E
   instead of the VGC, and the results were stored into $E1/092A instead of
   this program's own bss. The dump came back all zeros, completion byte
   included, twice. Every other probe in this port opens with this line. */
__attribute__((used, retain))
void gs_setdb(void) { __asm__ volatile("phk\n\tplb"); }

static unsigned int line(void)
{
    unsigned char v = VERTCNT, h = HORIZCNT;
    return (unsigned int)(((unsigned int)v << 1) | (h >> 7));
}

int main(void)
{
    unsigned int i, l, mn = 0xFFFF, mx = 0, frames = 0;
    unsigned char last, now;

    gs_setdb();

    last = (unsigned char)(VBL & 0x80);
    /* STORED EVERY PASS, not once at the end. The first version wrote the
       results after 40000 iterations and the dump came back all zeros --
       including the completion byte, so it was correctly unreadable rather
       than misleading. A probe whose findings only exist if it finishes tells
       you nothing about a run that did not. */
    for (i = 0; i < 6000u; i++) {
        l = line();
        if (l < mn) mn = l;
        if (l > mx) mx = l;
        now = (unsigned char)(VBL & 0x80);
        if (last && !now) frames++;
        last = now;
        vmin_lo = (unsigned char)mn;      vmin_hi = (unsigned char)(mn >> 8);
        vmax_lo = (unsigned char)mx;      vmax_hi = (unsigned char)(mx >> 8);
        vframes_lo = (unsigned char)frames; vframes_hi = (unsigned char)(frames >> 8);
    }
    gs_done = 0x5A;
    for (;;) { }
}
