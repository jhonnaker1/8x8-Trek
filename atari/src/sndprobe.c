/* WHAT FREQUENCY DOES POKEY ACTUALLY PRODUCE, and does the OS frame clock run?
 *
 * Two questions the sound seam turns on, and both are the kind this project
 * has got wrong by reasoning. The MEGA65 lost a day to a raster that wraps
 * twice per frame; the X16 paced its music off a jiffy clock that returns zero
 * for ever once a program has taken the machine over, and only measuring said
 * so. POKEY's divisor formula differs per mode -- 8-bit, 16-bit, which base
 * clock -- and a manual is not evidence about the core actually running.
 *
 * SO THIS PROBE IS DRIVEN FROM OUTSIDE. The harness pokes a POKEY register set
 * into $0602.. and raises $0600; the 6502 copies it to the real registers, so
 * the write is a hardware write and not a debugger one. Then the harness asks
 * Altirra what frequency came out. Any configuration can be tried without
 * rebuilding.
 *
 *      $0600  command: nonzero = apply the register block below
 *      $0601  ack: incremented each time a block is applied
 *      $0602  AUDCTL   $0603..$0606 AUDF0..3   $0607..$060A AUDC0..3
 *      $0610  RTCLOK low byte, refreshed every pass -- does the OS VBI run?
 *      $0611  the last DIFFERENT RTCLOK value seen, and
 *      $0612  how many times it changed: zero means the clock is dead
 *      $0613  GTIA $D014, the PAL/NTSC register, read once
 */
#define CMD  ((volatile unsigned char *)0x0600)
#define AUDF ((volatile unsigned char *)0xD200)   /* AUDF0,AUDC0,AUDF1,... */
#define AUDCTL (*(volatile unsigned char *)0xD208)
#define SKCTL  (*(volatile unsigned char *)0xD20F)
#define RTCLOK (*(volatile unsigned char *)0x0014)
#define GTIA_PAL (*(volatile unsigned char *)0xD014)

int main(void) {
    unsigned char last;
    unsigned char i;

    SKCTL = 0x03;                 /* POKEY out of reset, no keyboard scan change */
    CMD[0] = 0;
    CMD[1] = 0;
    CMD[0x13] = GTIA_PAL;
    last = RTCLOK;
    CMD[0x11] = last;
    CMD[0x12] = 0;

    for (;;) {
        unsigned char now = RTCLOK;
        CMD[0x10] = now;
        if (now != last) {
            last = now;
            CMD[0x11] = now;
            if (CMD[0x12] < 255) CMD[0x12]++;
        }

        if (CMD[0]) {
            AUDCTL = CMD[2];
            for (i = 0; i < 4; i++) {
                AUDF[i * 2]     = CMD[3 + i];      /* AUDFn */
                AUDF[i * 2 + 1] = CMD[7 + i];      /* AUDCn */
            }
            CMD[0] = 0;
            CMD[1]++;
        }
    }
    return 0;
}
