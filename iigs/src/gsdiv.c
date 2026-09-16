/* PROBE 4: is `p / 7` right on this compiler?
 *
 * Probe 3 proved the memory model, the long store and the page targeting are
 * all exact. The only thing probe 1 did that probe 3 did not is compute
 * `band = p / 7` and derive the fill value from it -- and its picture is
 * consistent with that division having produced p/14. So this computes the
 * quotient for every p the probe used and writes it down, with no graphics
 * anywhere near it.
 */
#define ASMVAR __attribute__((used, retain))
ASMVAR unsigned char q[112];
ASMVAR unsigned char done;

__attribute__((naked, used, retain, section(".init.040")))
void gs_emul(void) { __asm__ volatile("sec\n\txce"); }
__attribute__((naked, used, retain, section(".init.050")))
void gs_setdb(void) { __asm__ volatile("phk\n\tplb"); }

int main(void)
{
    unsigned char p;
    for (p = 0; p < 112; p++)
        q[p] = (unsigned char)(p / 7);
    done = 0x5A;
    for (;;) ;
    return 0;
}
