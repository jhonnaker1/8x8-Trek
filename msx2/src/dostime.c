/* WHAT DOES ONE DOS2 CALL COST? ui_briefing() streams BRIEF.TXT through
   plat_read(&c, 1) -- ONE BYTE per call, 10,557 of them -- so whether the
   storage driver needs a read buffer is a question about DOS's per-call
   overhead, and this measures it: the same file read at 1, then at 64
   bytes a call, timed in frames. Installed as the shell, like the game. */
#define JIFFY (*(volatile unsigned int *)0xFC9E)

static const char fname[] = "BRIEF.TXT";
static unsigned char buf[64];
unsigned char rA, rB;               /* global: probes read them */
unsigned int rDE, rHL;
unsigned int ncalls;              /* global: a probe reads it from outside */

/* One BDOS call through globals, so no argument convention is in doubt.
   C = function (arrives in A), then A, B, DE, HL from the globals; returns
   DOS's error code in A, and B and HL back into the globals. */
static unsigned char dos(unsigned char fn) __naked
{
    (void)fn;
    __asm
        push ix
        ld   c, a
        ld   a, (_rB)
        ld   b, a
        ld   de, (_rDE)
        ld   hl, (_rHL)
        ld   a, (_rA)
        call 5
        ld   (_rHL), hl
        ld   hl, (_ncalls)
        inc  hl
        ld   (_ncalls), hl
        ld   c, a
        ld   a, b
        ld   (_rB), a
        ld   a, c
        pop  ix
        ret
    __endasm;
}

static void say(const char *s)
{
    rDE = (unsigned int)s;
    dos(9);
}

static void put_dec(unsigned int v)
{
    static char out[8];
    unsigned char i = 6;
    out[6] = ' '; out[7] = '$';
    do { out[--i] = (char)('0' + v % 10); v /= 10; } while (v && i);
    say(&out[i]);
}

static void run(unsigned int chunk)
{
    unsigned int t0, bytes = 0, calls = 0;
    unsigned char err, h;

    rDE = (unsigned int)fname; rA = 1;
    if (dos(0x43)) { say("OPEN FAILED\r\n$"); return; }
    /* KEEP THE HANDLE: _READ does NOT preserve B. The first version wrote B
       back after every call, so the second read used handle 0 -- standard
       input -- and sat in CHGET waiting for a key. */
    h = rB;
    t0 = JIFFY;
    for (;;) {
        rDE = (unsigned int)buf; rHL = chunk; rB = h;
        err = dos(0x48);
        calls++;
        if (err || rHL == 0) break;
        bytes += rHL;
        if ((calls & 1023) == 0) say(".$");     /* progress, so slow != hung */
    }
    t0 = JIFFY - t0;
    rB = h;
    dos(0x45);
    say("CHUNK $"); put_dec(chunk);
    say("BYTES $"); put_dec(bytes);
    say("CALLS $"); put_dec(calls);
    say("FRAMES $"); put_dec(t0);
    say("\r\n$");
}

void main(void)
{
    run(64);
    run(1);
    say("DONE\r\n$");
    for (;;)
        ;
}
