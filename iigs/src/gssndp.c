/* PROBE 8: what frequency does the Ensoniq actually play?
 *
 * The DOC's output frequency is a register value times a constant that
 * depends on the sample rate, the wavetable size and the resolution field --
 * and the sample rate itself depends on how many oscillators are enabled.
 * Every one of those is a chance to be an octave out, and this project has
 * already shipped a port AN OCTAVE FLAT FOR FOUR MONTHS because one check
 * passed by ear. So the constant is measured, from THREE points, before a
 * note of music is written.
 *
 * Three tones an octave apart by register value ($0400, $0800, $1000). If the
 * constant is right the measured pitches are in the same 1:2:4 ratio; if the
 * ratio holds and the absolute values are wrong, the constant is wrong and
 * the shape is right; if the ratio does not hold, the model is wrong. One
 * point cannot tell those apart, which is exactly how the X16 got away with
 * it.
 *
 * A SQUARE WAVETABLE, deliberately: tools/hearit.py counts zero crossings,
 * which is exact for a square and only approximate for anything else.
 *
 * AND NO ZERO BYTES IN THE TABLE. The DOC halts an oscillator when it reads a
 * sample of zero -- that is how one-shot sounds end -- so a waveform that
 * swings through zero stops itself. $40 and $C0.
 */
#define ASMVAR __attribute__((used, retain))

#define SOUNDCTL (*(volatile unsigned char *)0xC03C)
#define SNDDATA  (*(volatile unsigned char *)0xC03D)
#define ADDRLO   (*(volatile unsigned char *)0xC03E)
#define ADDRHI   (*(volatile unsigned char *)0xC03F)
#define VBL      (*(volatile unsigned char *)0xC019)

/* DOC register files, each 32 entries, one per oscillator. */
#define DOC_FREQLO 0x00
#define DOC_FREQHI 0x20
#define DOC_VOL    0x40
#define DOC_PTR    0x80
#define DOC_CTL    0xA0
#define DOC_SIZE   0xC0
#define DOC_OSCEN  0xE1

ASMVAR unsigned char gs_done;

__attribute__((naked, used, retain, section(".init.040")))
void gs_emul(void) { __asm__ volatile("sec\n\txce"); }
__attribute__((naked, used, retain, section(".init.050")))
void gs_setdb(void) { __asm__ volatile("phk\n\tplb"); }

/* Bit 5 of SOUNDCTL picks the DOC's REGISTERS or its 64K of wavetable RAM;
   bit 6 auto-increments the address after each access; the low nibble is the
   master volume. */
static void doc_reg(unsigned char reg, unsigned char val)
{
    SOUNDCTL = 0x0F;              /* registers, no auto-inc, full volume */
    ADDRLO = reg;
    ADDRHI = 0;
    SNDDATA = val;
}

static void doc_ram_fill(unsigned int addr, unsigned char val, unsigned int n)
{
    unsigned int i;
    SOUNDCTL = 0x6F;              /* RAM, auto-increment, full volume */
    ADDRLO = (unsigned char)(addr & 0xFF);
    ADDRHI = (unsigned char)(addr >> 8);
    for (i = 0; i < n; i++) SNDDATA = val;
}

static void wait_frames(unsigned int n)
{
    while (n--) {
        while (VBL & 0x80) ;
        while (!(VBL & 0x80)) ;
    }
}

static void tone(unsigned int freq, unsigned int frames)
{
    doc_reg(DOC_FREQLO + 0, (unsigned char)(freq & 0xFF));
    doc_reg(DOC_FREQHI + 0, (unsigned char)(freq >> 8));
    doc_reg(DOC_VOL + 0, 0xFF);
    doc_reg(DOC_CTL + 0, 0x00);   /* free-run, channel 0, not halted */
    wait_frames(frames);
    doc_reg(DOC_CTL + 0, 0x01);   /* halt */
    wait_frames(20);
}

int main(void)
{
    __asm__ volatile("sei");

    /* A 256-byte square at DOC RAM $0000. */
    doc_ram_fill(0x0000, 0xC0, 128);
    doc_ram_fill(0x0080, 0x40, 128);

    doc_reg(DOC_PTR + 0, 0x00);   /* wavetable at page 0 */
    doc_reg(DOC_SIZE + 0, 0x00);  /* 256 entries, resolution 0 */
    doc_reg(DOC_OSCEN, 0x02);     /* ONE oscillator enabled: (n+1)*2 = 2.
                                     This also sets the sample rate, which is
                                     why it is part of the frequency answer
                                     and not a detail. */

    wait_frames(30);
    tone(0x0400, 90);             /* three octaves by register value */
    tone(0x0800, 90);
    tone(0x1000, 90);

    gs_done = 0x5A;
    for (;;) ;
    return 0;
}
