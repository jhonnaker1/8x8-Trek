/* Storage for the F256K: FoenixMCP's file API, which is ASYNCHRONOUS.
 *
 * Every other port in this tree opens a file and reads it. Here a call only
 * REQUESTS the work; the answer arrives later as an event, on the same queue
 * that carries the keyboard. So each of the five plat_ functions below is a
 * request followed by a wait, and the wait is f256_wait_file() -- which keeps
 * pumping keystrokes into the ring while it waits, because the alternative is
 * a player who loses everything they typed during a load and no way to know.
 *
 * THREE TRAPS, all of them in the shape of the API rather than in any one
 * call:
 *
 *   1. A READ IS TWO STEPS. File.Read requests; a file.DATA event says how
 *      many bytes are ready; ReadData then copies them into our buffer. Stop
 *      after the event and the buffer holds whatever it held before -- and
 *      the byte count is still correct, so nothing looks wrong.
 *   2. `delivered == 0` MEANS 256, NOT END OF FILE. The count is a byte and a
 *      full read wraps it. End of file is its own event. A reader that treats
 *      0 as EOF truncates every file at the first full block, which for a
 *      save is a file that loads and is wrong rather than one that fails.
 *   3. ONE QUEUE FOR EVERYTHING. See f256evt.c.
 *
 * AND EVERY WAIT HAS A DEADLINE. The reference implementation spins in
 * `for(;;)` on each of these; on a machine where the ordinary failure is an
 * absent SD card, that is a game that hangs with no message. Nothing here
 * waits longer than two seconds.
 */
#include <stdint.h>
#include "storage.h"
#include "f256evt.h"

/* The kernel jump table, four bytes per entry -- assume three and every call
   after the first lands mid-instruction. */
#define K_FILE_OPEN  0xFF5C
#define K_FILE_READ  0xFF60
#define K_FILE_WRITE 0xFF64
#define K_FILE_CLOSE 0xFF68

/* The argument union at $F3, and args.buf/buflen at $FB/$FD which sit
   OUTSIDE it -- see f256kern.h. open.drive, read.stream, write.stream and
   close.stream are all the union's first byte, which is why they are one
   address here and not four. */
#define A_STREAM (*(volatile unsigned char *)0x00F3)
#define A_DRIVE  (*(volatile unsigned char *)0x00F3)
#define A_COOKIE (*(volatile unsigned char *)0x00F4)
#define A_RDLEN  (*(volatile unsigned char *)0x00F4)   /* file.read.buflen */
#define A_MODE   (*(volatile unsigned char *)0x00F5)
#define A_BUF    (*(const void * volatile *)0x00FB)
#define A_BUFLEN (*(volatile unsigned char *)0x00FD)

#define MODE_READ  0
#define MODE_WRITE 1

/* Two seconds at 60 Hz. Generous for an SD card and short enough that a
   machine with no card in it says so instead of appearing to freeze. */
#define FILE_WAIT 120

static volatile unsigned char k_ret, k_err;


/* THE CARRY IS THE ERROR FLAG, so the call and the test are ONE asm block --
   anything the compiler put between them could clear it. STA does not touch
   the flags, which is why the return value can be saved first. */
#define KCALL(addr)                                  \
    __asm__ volatile(                                \
        "        jsr " addr "\n"                     \
        "        sta k_ret\n"                        \
        "        lda #0\n"                           \
        "        rol a\n"                            \
        "        sta k_err\n"                        \
        ::: "a", "x", "y", "memory", "p")

static void kcall_open(void)  { KCALL("$ff5c"); }
static void kcall_read(void)  { KCALL("$ff60"); }
static void kcall_write(void) { KCALL("$ff64"); }
static void kcall_close(void) { KCALL("$ff68"); }

/* Step two of a read: copy the bytes the file.DATA event announced out of the
   kernel's buffer and into ours. Takes its length from args.buflen, where 0
   means 256 -- the kernel's copy loop does `dey` first. */
static void kcall_readdata(void)
{
    __asm__ volatile("jsr $ff04" ::: "a", "x", "y", "memory", "p");
}

static unsigned char name_len(const char *s)
{
    unsigned char n = 0;
    while (s[n]) n++;
    return n;
}

/* ---------------------------------------------------------- the stream */

/* ONE STREAM AT A TIME, which is the contract storage.h states and also all
   this port needs. `open_stream` is 0 when nothing is open -- and 0 is a safe
   sentinel only because the kernel never hands out stream 0: FoenixMCP
   reserves 0 and 1 for stdin and stdout. */
static unsigned char open_stream;
static unsigned char at_eof;

static uint8_t f_open(const char *name, unsigned char mode)
{
    unsigned char t;

    A_BUF = (const void *)name;
    A_BUFLEN = name_len(name);
    A_DRIVE = 0;            /* names are opaque tokens; this port has one
                               drive and storage.h forbids the core seeing
                               any of this */
    A_COOKIE = 0;
    A_MODE = mode;
    kcall_open();
    if (k_err) return STOR_ERROR;
    open_stream = k_ret;
    at_eof = 0;

    t = f256_wait_file(FILE_WAIT);
    if (t == EV(file.OPENED)) return STOR_OK;
    open_stream = 0;

    /* THE KERNEL NEVER SENDS file.NOT_FOUND. It is in the event enum and
     * nothing in FoenixMCP emits it -- the same as clock.TICK, and found the
     * same way: by opening a file that is not there and reading the event
     * number back. A missing file arrives as file.ERROR ($38), which is also
     * what a broken card would send.
     *
     * So the line this port draws is between AN ANSWER AND NO ANSWER, which
     * is a distinction it can actually make. A file.ERROR on a read-open is
     * reported as STOR_NOTFOUND: the drive answered, and it said no. Silence
     * until the deadline is STOR_ERROR: the drive is not there. storage.h
     * blesses exactly this -- "only some of these platforms can tell them
     * apart", and STOR_NOTFOUND is documented as covering "no drive" on the
     * platforms that cannot.
     *
     * The honest caveat: a genuine I/O error on a file that DOES exist comes
     * out as NOTFOUND here. Directory.Read could tell them apart by scanning
     * for the name, and no caller in this game distinguishes the two codes,
     * so that work would buy nothing today. Written down rather than done. */
    if (t == F256_WAIT_TIMEOUT) return STOR_ERROR;
    if (mode == MODE_READ) return STOR_NOTFOUND;
    return STOR_ERROR;
}

static void f_close(void)
{
    if (!open_stream) return;
    A_STREAM = open_stream;
    kcall_close();
    open_stream = 0;
    /* The CLOSED event is waited for rather than ignored: leaving it in the
       queue means the NEXT file operation's wait returns somebody else's
       answer, which is the class of bug that made the C128's disk seam take
       three attempts. A timeout here is not worth reporting -- the stream is
       gone either way -- but it must not be skipped. */
    (void)f256_wait_file(FILE_WAIT);
}

/* Up to 255 bytes into `buf`. Returns the count, 0 at end of file, or 0xFFFF
   on error. */
static uint16_t f_read(void *buf, unsigned char want)
{
    unsigned char t, delivered;

    if (!open_stream || at_eof) return 0;
    A_STREAM = open_stream;
    A_RDLEN = want;
    kcall_read();
    if (k_err) return 0xFFFF;

    t = f256_wait_file(FILE_WAIT);
    if (t == EV(file.EOF_)) { at_eof = 1; return 0; }
    if (t != EV(file.DATA)) return 0xFFFF;

    /* event file_t is { stream, cookie, requested, read } over the payload. */
    delivered = f256_file_ev.data[3];
    A_BUF = buf;
    A_BUFLEN = delivered;       /* 0 here asks the kernel for 256 -- correct,
                                   because that is exactly what 0 means */
    kcall_readdata();
    return delivered ? (uint16_t)delivered : 256u;
}

static uint16_t f_write(const void *buf, unsigned char n)
{
    unsigned char t;

    if (!open_stream) return 0xFFFF;
    A_STREAM = open_stream;
    A_BUF = buf;
    A_BUFLEN = n;
    kcall_write();
    if (k_err) return 0xFFFF;

    t = f256_wait_file(FILE_WAIT);
    if (t != EV(file.WROTE)) return 0xFFFF;
    return f256_file_ev.data[3] ? (uint16_t)f256_file_ev.data[3] : 256u;
}

/* -------------------------------------------------------- the contract */

/* CHUNKS OF 255, NOT 256. 256 would have to be expressed as a length of 0,
   and a request of 0 is indistinguishable at the call site from a bug that
   computed nothing left to do. The byte it costs per block is not worth the
   ambiguity. */
#define CHUNK 255

uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got)
{
    unsigned char *p = (unsigned char *)buf;
    uint16_t total = 0;
    uint16_t n;
    uint8_t st;

    *got = 0;
    st = f_open(name, MODE_READ);
    if (st != STOR_OK) return st;

    for (;;) {
        uint16_t room = (uint16_t)(max - total);
        unsigned char want;
        if (room == 0) break;
        want = (unsigned char)(room > CHUNK ? CHUNK : room);
        n = f_read(p + total, want);
        if (n == 0xFFFF) { f_close(); return STOR_ERROR; }
        if (n == 0) { f_close(); *got = total; return STOR_OK; }
        total = (uint16_t)(total + n);
    }

    /* THE BUFFER IS FULL AND THE FILE MAY NOT BE DONE. storage.h is explicit
       that a file longer than `max` is an ERROR, not a truncation -- silently
       reading half a save is worse than refusing -- so ask for one more byte.
       A read that returns anything means the file is too long; reading the
       EOF here is what says it fitted exactly. */
    {
        unsigned char spill;
        n = f_read(&spill, 1);
        f_close();
        if (n == 0xFFFF) return STOR_ERROR;
        if (n != 0) return STOR_ERROR;
    }
    *got = total;
    return STOR_OK;
}

uint8_t plat_write_all(const char *name, const void *buf, uint16_t len)
{
    const unsigned char *p = (const unsigned char *)buf;
    uint16_t done = 0;
    uint8_t st;

    st = f_open(name, MODE_WRITE);
    if (st != STOR_OK) return STOR_ERROR;   /* NOTFOUND is meaningless here */

    while (done < len) {
        uint16_t left = (uint16_t)(len - done);
        unsigned char want = (unsigned char)(left > CHUNK ? CHUNK : left);
        uint16_t n = f_write(p + done, want);
        if (n == 0xFFFF || n == 0) { f_close(); return STOR_ERROR; }
        done = (uint16_t)(done + n);
    }
    f_close();
    return STOR_OK;
}

uint8_t plat_open(const char *name)
{
    /* plat_open() closes whatever was open before it -- storage.h says so,
       and the C128's seam spent three attempts learning that the rule has to
       be one line long: plat_open opens, plat_close closes, nothing else
       touches the stream. */
    plat_close();
    return f_open(name, MODE_READ);
}

uint16_t plat_read(void *buf, uint16_t len)
{
    unsigned char *p = (unsigned char *)buf;
    uint16_t total = 0;

    /* FILL THE REQUEST, don't return the first short block. The briefing
       streamer asks for a page and a block boundary is not a page boundary;
       a caller that got 255 bytes and assumed end-of-file would drop the rest
       of every briefing page after the first. */
    while (total < len) {
        uint16_t room = (uint16_t)(len - total);
        unsigned char want = (unsigned char)(room > CHUNK ? CHUNK : room);
        uint16_t n = f_read(p + total, want);
        if (n == 0xFFFF) return total;
        if (n == 0) break;                  /* genuine end of file */
        total = (uint16_t)(total + n);
    }
    return total;
}

void plat_close(void)
{
    f_close();
}
