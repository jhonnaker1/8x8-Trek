/* WHAT IS TED'S RASTER COUNTER, ACTUALLY?
 *
 * snd_poll detects a frame as "the raster went backwards" and was counting
 * 99.74 frames a second against PAL's 50.125 -- exactly twice. Reading the
 * counter as 9 bits ($FF1C bit 0 << 8 | $FF1D) did not change that, so the
 * assumption that $FF1C bit 0 is the ninth bit is in doubt.
 *
 * Sampled FROM THE MACHINE, in a tight loop at full speed, because the
 * monitor stops the emulator at a deterministic point in the frame: 120 reads
 * through it returned the same value 120 times and I read that as proof.
 *
 * report[0..1] max $FF1D          report[6..7] backward steps, 9-bit
 * report[2..3] max 9-bit value    report[8]    OR of all $FF1C bit 0
 * report[4..5] backward steps, 8-bit           report[15] completion marker
 */
volatile unsigned char report[16];

#define RHI (*(volatile unsigned char *)0xFF1C)
#define RLO (*(volatile unsigned char *)0xFF1D)

int main(void)
{
    unsigned int i, maxlo = 0, maxw = 0, backlo = 0, backw = 0;
    unsigned int plo = 0, pw = 0, lo, w;
    unsigned char hi, orhi = 0;

    __asm__ volatile ("sei");
    *(volatile unsigned char *)0xFF0A = 0;

    for (i = 0; i < 60000u; i++) {
        hi = (unsigned char)(RHI & 1);
        lo = RLO;
        orhi |= hi;
        w = (unsigned int)(((unsigned int)hi << 8) | lo);
        if (lo > maxlo) maxlo = lo;
        if (w  > maxw)  maxw  = w;
        if (lo < plo) backlo++;
        if (w  < pw)  backw++;
        plo = lo; pw = w;
    }
    report[0] = (unsigned char)maxlo;  report[1] = (unsigned char)(maxlo >> 8);
    report[2] = (unsigned char)maxw;   report[3] = (unsigned char)(maxw  >> 8);
    report[4] = (unsigned char)backlo; report[5] = (unsigned char)(backlo >> 8);
    report[6] = (unsigned char)backw;  report[7] = (unsigned char)(backw  >> 8);
    report[8] = orhi;
    report[15] = 0x5A;
    for (;;) { }
}
