/* WHO OWNS THE TOP 1,280 BYTES OF THE TPA? Measured 2026-09-25 with a
   breakpoint at $0100 on every program start:

       COMMAND2.COM entered with ($0006) = $DB06
       any program it runs, from the prompt OR a batch file, ($0006) = $D606

   COMMAND2 stays resident above what it runs -- and $D606 is a JP: ($0006)
   points into it, so it INTERCEPTS EVERY BDOS CALL. A program that
   overwrites it cannot return (the prompt never came back) and cannot even
   call BDOS (the first _OPEN hung). Those bytes are not spare memory; they
   are the path to DOS.

   Installed AS the boot disk's COMMAND2.COM, a program is entered like the
   shell: ($0006) = $DB06, no COMMAND2 in memory, BDOS direct. This is that
   test: it fills $D606..$DB05 with HALT -- the worst garbage there is --
   then reads a file and prints, and prints "OK" only if both worked. Then
   it spins, because a shell has nothing to return to. (The game's own quit
   _TERM0s, and MSX-DOS 2 reloads the shell -- the game -- measured.)

   `make shell` builds the disk and reads the screen. */
static const char fname[] = "MSXDOS2.SYS";
static unsigned char iobuf[128];
static unsigned char err;
static unsigned int got;

static void dos_file(void) __naked
{
    __asm
        push ix
        ld   de, #_fname
        ld   a, #1
        ld   c, #0x43            ; _OPEN, read only
        call 5
        ld   (_err), a
        or   a
        jr   nz, 1$
        push bc
        ld   de, #_iobuf
        ld   hl, #128
        ld   c, #0x48            ; _READ
        call 5
        ld   (_got), hl
        pop  bc
        ld   c, #0x45            ; _CLOSE
        call 5
    1$:
        pop  ix
        ret
    __endasm;
}

static void say(const char *s)
{
    (void)s;
    __asm
        ex   de, hl
        ld   c, #9
        push ix
        call 5
        pop  ix
    __endasm;
}

void main(void)
{
    unsigned char *p = (unsigned char *)0xD606;
    unsigned int top = *(unsigned int *)6;

    if (top != 0xDB06) {
        say("NOT THE SHELL: ($0006) IS NOT $DB06\r\n$");
        return;                                /* and leave the bytes alone */
    }
    while (p < (unsigned char *)0xDB06)
        *p++ = 0x76;
    dos_file();
    say(err == 0 && got == 128 ? "SHELL OK: TOP 1280 TRASHED, FILE READ\r\n$"
                               : "SHELL FAILED: FILE READ\r\n$");
    for (;;)
        ;
}
