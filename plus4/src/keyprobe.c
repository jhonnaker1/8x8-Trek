/* Does TED's keyboard port answer at all? Scan all eight rows and report the
   eight column bytes.

   WITH NO KEY HELD EVERY ROW MUST READ $FF, because the columns are active
   low. A row reading $00 means the strobe is inverted or $FD30 is not the
   selector; a row reading garbage means the write to $FF08 is not what makes
   TED sample. This CANNOT tell a correct matrix from a transposed one -- only
   a finger on a key can do that -- but it separates "the registers work" from
   "the table is wrong", which is the question a human test cannot answer for
   me afterwards.

   Byte 15 is a completion marker: a probe that never finishes must not be
   read as data. src/zpprobe.c reported "32 of 32 clobbered" with the KERNAL
   call REMOVED because nobody checked it had run. */
volatile unsigned char report[16];

#define KEY_LATCH (*(volatile unsigned char *)0xFD30)
#define KEY_PORT  (*(volatile unsigned char *)0xFF08)

int main(void)
{
    unsigned char r;

    __asm__ volatile ("sei");
    *(volatile unsigned char *)0xFF0A = 0;

    for (r = 0; r < 8; r++) {
        KEY_LATCH = (unsigned char)~(1 << r);
        KEY_PORT  = 0xFF;
        report[r] = KEY_PORT;
    }
    report[15] = 0x5A;
    for (;;) { }
}
