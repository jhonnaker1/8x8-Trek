/* HOW MUCH OF OUR STACK DOES THE MACHINE TAKE? tools/stackdepth.py bounds
   what SDCC's code uses, and cannot see the rest: the BIOS interrupt handler
   runs on whatever stack is live every frame, CALSLT and the BIOS routines
   push onto ours, and so may BDOS. This fills 1K below its own SP with a
   sentinel, runs one of those at a time, and reports how deep each wrote.

   Each phase is called from main() at the same depth, so a figure is the
   phase function's own few bytes plus what the machine added. Keys are
   typed into the machine by tools/stktest.tcl during phase 1, so the
   handler is measured doing keyboard work and not just counting JIFFY. */
#include "vdc.h"

#define FILL   0xE5
#define SPAN   1024
#define MARGIN 12      /* left unfilled: fill()'s own return address and frame */

static unsigned int base;
static unsigned char key, got;
static unsigned int deep[7];

static unsigned int sp_here(void) __naked
{
    __asm
        ld   hl, #2          ; the caller SP, past our return address
        add  hl, sp
        ex   de, hl          ; sdcccall(1) returns 16 bits in DE
        ret
    __endasm;
}

static void fill(void)
{
    unsigned char *p = (unsigned char *)(base - SPAN);
    unsigned int n;
    for (n = 0; n < SPAN - MARGIN; n++)
        p[n] = FILL;
}

static unsigned int depth(void)
{
    unsigned char *p = (unsigned char *)(base - SPAN);
    unsigned int n = 0;
    while (n < SPAN - MARGIN && p[n] == FILL)
        n++;
    return SPAN - n;                      /* bytes below base that were written */
}

/* CALSLT into the main BIOS, as msxbios.s does: IYH = the slot, IX = the
   address, and CALSLT hands back with interrupts off. */
static unsigned char bios_chsns(void) __naked
{
    __asm
        push ix
        ld   iy, (0xFCC0)
        ld   ix, #0x009C
        call 0x001C
        ei
        pop  ix
        ld   a, #0
        ret  z
        inc  a
        ret
    __endasm;
}

static unsigned char bios_chget(void) __naked
{
    __asm
        push ix
        ld   iy, (0xFCC0)
        ld   ix, #0x009F
        call 0x001C
        ei
        pop  ix
        ret
    __endasm;
}

/* DOS2 handle calls through globals, so no argument convention is in doubt:
   _OPEN $43 (DE = ASCIIZ, A = 1 read-only) -> B handle, _READ $48 (B, DE
   buffer, HL count), _CLOSE $45 (B). */
static const char fname[] = "MSXDOS2.SYS";
static unsigned char iobuf[128];
static unsigned char dos_err, handle;

static void dos_io(void) __naked
{
    __asm
        push ix
        ld   de, #_fname
        ld   a, #1
        ld   c, #0x43
        call 5
        ld   (_dos_err), a
        or   a
        jr   nz, 1$
        ld   a, b
        ld   (_handle), a
        ld   de, #_iobuf
        ld   hl, #128
        ld   c, #0x48
        call 5
        ld   a, (_handle)
        ld   b, a
        ld   c, #0x45
        call 5
    1$:
        pop  ix
        ret
    __endasm;
}

static void bdos_puts(const char *s)
{
    (void)s;
    __asm
        ld   d, h
        ld   e, l
        ld   c, #9
        push ix
        call 5
        pop  ix
    __endasm;
}

static void put_dec(unsigned int v)
{
    static char out[7];
    unsigned char i = 5;
    out[5] = '$';
    do { out[--i] = (char)('0' + v % 10); v /= 10; } while (v && i);
    bdos_puts(&out[i]);
}

extern void bios_chgmod(unsigned char mode);

/* THE CONTROL: a phase that does nothing, so the floor is a measured number
   and not an assumption. Every other figure is read against it. */
static void phase_none(void) { }

/* FORCING THE RACE. CALSLT puts the BIOS ROM in page 0, so an interrupt
   taken inside a BIOS call goes to the ROM's handler, on OUR stack -- while
   in DOS's page 0 it does not (IRQ IDLE reads the floor). A short CHSNS
   catches that only if a frame happens to land in it: 78 bytes on one run,
   28 on the next. CHGET with an empty buffer waits INSIDE the BIOS with
   interrupts on, for every frame until tools/stktest.tcl types a key, so
   the handler runs there dozens of times, keypress path included. */
static void phase_block(void)
{
    key = bios_chget();
}

#define IRQ_OFF_FOR_CONTROL()  __asm__("di")
#define IRQ_ON_AFTER_CONTROL() __asm__("ei")

static void phase_idle(void)
{
    unsigned char n;
    for (n = 0; n < 250; n++) wait_vsync();       /* five seconds of handler */
}

static void phase_keys(void)
{
    got = 0;
    while (bios_chsns()) { key = bios_chget(); got++; }
}

void main(void)
{
    base = sp_here();

    fill(); vdc_init();        deep[0] = depth();   /* CHGMOD 7 via CALSLT */
    fill(); phase_idle();      deep[1] = depth();   /* the interrupt handler */
    fill(); phase_keys();      deep[2] = depth();   /* CHSNS/CHGET via CALSLT */
    fill(); bios_chgmod(0);    deep[3] = depth();   /* CHGMOD 0, back to text */
    fill(); dos_io();          deep[4] = depth();   /* BDOS open/read/close */
    fill(); phase_block();     deep[6] = depth();   /* CHGET waiting in the BIOS */
    IRQ_OFF_FOR_CONTROL();
    fill(); phase_none();      deep[5] = depth();   /* the floor, no interrupts */
    IRQ_ON_AFTER_CONTROL();

    bdos_puts("STACK TEST, BYTES BELOW THE CALLER'S SP\r\n$");
    bdos_puts("CHGMOD 7   $");  put_dec(deep[0]); bdos_puts("\r\n$");
    bdos_puts("IRQ IDLE   $");  put_dec(deep[1]); bdos_puts("\r\n$");
    bdos_puts("CHSNS/GET  $");  put_dec(deep[2]); bdos_puts(" KEYS $"); put_dec(got); bdos_puts("\r\n$");
    bdos_puts("CHGMOD 0   $");  put_dec(deep[3]); bdos_puts("\r\n$");
    bdos_puts("BDOS FILE  $");  put_dec(deep[4]); bdos_puts(" ERR $"); put_dec(dos_err); bdos_puts("\r\n$");
    bdos_puts("CHGET WAIT $");  put_dec(deep[6]); bdos_puts("\r\n$");
    bdos_puts("CONTROL    $");  put_dec(deep[5]); bdos_puts(" (NOTHING, IRQS OFF)\r\n$");
    bdos_puts("SP $");          put_dec(base);    bdos_puts("\r\nDONE\r\n$");
}
