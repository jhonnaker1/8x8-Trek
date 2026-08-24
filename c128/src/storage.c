#include <cbm.h>
#include <string.h>

#include "../../core/storage.h"

/* The C128 half of the disk seam: KERNAL file I/O behind core/storage.h.
 *
 * Written against the RAW KERNAL calls rather than a convenience wrapper,
 * because llvm-mos's <cbm.h> has the entire high-level file API --
 * cbm_open, cbm_read, cbm_write, cbm_close, cbm_load, cbm_save -- inside an
 * `#if 0`. Only the cbm_k_* primitives are exposed. That turns out to suit
 * this port: it wants the status channel under its own control anyway, and
 * the sequence below is the one a wrapper would have run regardless.
 *
 * Device 8, and the caller's opaque token becomes a CBM DOS filename here and
 * nowhere else. `0:` selects drive 0 of the unit -- a 1541 has only one drive
 * but wants the prefix anyway, and a 1571 or an SD2IEC needs it. `,S,R` and
 * `,S,W` name a SEQuential file opened for Read or Write; without the type
 * the DOS guesses, and what it guesses for a write is a PRG.
 *
 * Rewriting an existing file is a SCRATCH followed by a fresh create, not a
 * `@` replace -- see scratch() below, where the measurement that settled that
 * is recorded.
 *
 * WHY THE STATUS CHANNEL. Opening a file that does not exist SUCCEEDS on a
 * CBM drive. The failure only appears when you ask the command channel, and a
 * port that skips that step reads a missing hall of fame as a valid empty
 * one. That is the trap this file exists to absorb.
 */

#define DEV      8
#define LFN_DATA 2
#define LFN_CMD  15

/* Long enough for "0:" + a 16-character CBM name + ",S,W" + NUL. */
static char fname[24];

/* SETBNK, and it is NOT optional on this machine.
 *
 * The C128 KERNAL asks which RAM bank the filename and the data live in, and
 * its default is bank 15 -- where $A500, which is where this port's buffers
 * sit, is BASIC ROM rather than RAM. Skip this and a read appears to work
 * while filling the buffer with fragments of BASIC.
 *
 * A = the bank for data, X = the bank for the filename. Both 0: this program
 * and everything it owns live in RAM bank 0. No C wrapper exists in
 * llvm-mos's cbm.h, so the call is written out. */
/* NOTE cbm_k_ckout, not cbm_k_chkout. llvm-mos's C128 libc has both, and
   cbm_k_chkout is BROKEN: it references __CHKOUT while the platform's
   kernal.S defines __CKOUT, so it fails at link with an undefined symbol.
   Found 2026-08-23 during the migration. cbm_k_ckout is the same KERNAL
   vector and links clean. */

static void set_banks(void) {
    __asm__ volatile("lda #0\n\tldx #0\n\tjsr $ff68" ::: "a", "x");
}

/* 2MHz WAS SUSPECTED AND IS INNOCENT.
 *
 * vdc_init() leaves the machine in 2MHz mode, and when the hall of fame first
 * came back blank the obvious culprit was the serial bus: the KERNAL's IEC
 * routines are timing-critical and the folklore says they want 1MHz. A
 * slow-down was written, the comment explaining it was confident, and it
 * changed nothing -- because the real fault was a test disk that had never
 * had TREK.SCR written to it.
 *
 * MEASURED afterwards rather than argued about: a probe read the same file
 * twice, once with $D030 bit 0 set and once clear. Both returned eight bytes
 * with a clean status and identical contents. So the clock rate is left
 * alone, and the code that used to change it is gone.
 *
 * The limit of that test: it is VICE, whose serial emulation may be more
 * forgiving than a real 1571 on a real bus. If disk I/O ever misbehaves on
 * hardware, this is the first thing to re-test -- but it is not going to be
 * worked around in advance on a hunch that has already been refuted once. */

static const char *cbm_name(const char *name, char mode) {
    char *p = fname;

    /* No '@' -- see scratch() above for why replace is not used. */
    *p++ = '0'; *p++ = ':';
    strcpy(p, name);
    p += strlen(name);
    *p++ = ','; *p++ = 'S';
    *p++ = ','; *p++ = mode;
    *p = '\0';
    return fname;
}

static uint8_t open_file(const char *name, char mode) {
    set_banks();
    cbm_k_setnam(cbm_name(name, mode));
    cbm_k_setlfs(LFN_DATA, DEV, LFN_DATA);
    return cbm_k_open();
}

/* Conventional order: CLRCHN, then CLOSE. The reverse was tried while
   chasing the write bug below and made no difference, so the documented
   order stands. */
static void close_file(void) {
    cbm_k_clrch();
    cbm_k_close(LFN_DATA);
}

/* THE COMMAND CHANNEL STAYS OPEN, and this is the trap that cost the most
 * time on 2026-08-23.
 *
 * On CBM DOS, **closing secondary address 15 closes every other file open on
 * that drive.** So the obvious shape -- open the file, open channel 15, read
 * the status, close 15, then read the file -- destroys the very channel it
 * was checking. The symptom is a read that returns zero bytes with READST
 * 0x42 (end of file plus timeout) while the status it just read said 00, OK.
 *
 * It looks exactly like a missing file, which is what sent the first
 * diagnosis after the serial clock rate instead. Measured with a probe that
 * ran plat_read_all's sequence with and without the status check: without it,
 * eight bytes and a clean status; with it, nothing.
 *
 * So the command channel is opened FIRST, kept open for as long as the data
 * file is, and closed LAST.
 */

static uint8_t cmd_open(void) {
    set_banks();
    cbm_k_setnam("");
    cbm_k_setlfs(LFN_CMD, DEV, LFN_CMD);
    return cbm_k_open();
}

static void cmd_close(void) {
    cbm_k_clrch();
    cbm_k_close(LFN_CMD);
}

/* The DOS error code, or 255 if the channel could not be read at all -- which
   on this machine means no drive is attached. The drive answers
   "NN, MESSAGE,TT,SS" and the whole line has to be consumed, or the next
   status read returns the tail of this one. */
static unsigned char cmd_status(void) {
    unsigned char a, b, c;
    unsigned char guard = 40;

    if (cbm_k_chkin(LFN_CMD)) { cbm_k_clrch(); return 255; }
    a = cbm_k_chrin();
    b = cbm_k_chrin();
    do { c = cbm_k_chrin(); } while (c != 13 && --guard && !cbm_k_readst());
    cbm_k_clrch();

    if (a < '0' || a > '9' || b < '0' || b > '9') return 255;
    return (unsigned char)((a - '0') * 10 + (b - '0'));
}

/* Sends a DOS command on the already-open channel 15. */
static void cmd_send(const char *cmd) {
    if (cbm_k_ckout(LFN_CMD)) return;
    while (*cmd) cbm_k_bsout((unsigned char)*cmd++);
    cbm_k_clrch();
}

/* SCRATCH FIRST, DO NOT USE `@` REPLACE.
 *
 * The obvious way to rewrite a file is `@0:NAME,S,W`, and it does not work
 * here. MEASURED 2026-08-23: after a write that reported success at every
 * step -- open 0, ckout 0, READST 0 on every byte, status 00 -- the new data
 * was on the disk but the directory entry read back as type 0xA1: a SEQ file
 * with the REPLACE-PENDING bit still set and a nonsense block count. The
 * replace had been started and never committed, so the next read still got
 * the old file.
 *
 * This is CBM DOS's save-with-replace, which has a bad reputation on 1541s
 * for exactly this. The reliable pattern is older and simpler: scratch the
 * file, then create it fresh.
 *
 * A scratch of a file that does not exist reports 01, FILES SCRATCHED with a
 * count of zero, or 62 -- neither is a failure here, so the status after this
 * is deliberately not checked. */
static void scratch(const char *name) {
    char cmd[20];
    char *p = cmd;

    *p++ = 'S'; *p++ = '0'; *p++ = ':';
    strcpy(p, name);
    cmd_send(cmd);
}

/* 62 is FILE NOT FOUND. Everything else non-zero is a real fault. */
static uint8_t classify(unsigned char code) {
    if (code == 0)  return STOR_OK;
    if (code == 62) return STOR_NOTFOUND;
    return STOR_ERROR;
}

uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got) {
    unsigned char *p = (unsigned char *)buf;
    uint16_t n = 0;
    unsigned char st;

    *got = 0;
    if (cmd_open()) { cmd_close(); return STOR_ERROR; }
    if (open_file(name, 'R')) { close_file(); cmd_close(); return STOR_ERROR; }

    /* Ask BEFORE reading, which is only possible because channel 15 is still
       open. A missing file opens cleanly and then returns zero bytes,
       indistinguishable from an empty file until you look. */
    st = cmd_status();
    if (st) { close_file(); cmd_close(); return classify(st); }

    if (cbm_k_chkin(LFN_DATA)) { close_file(); cmd_close(); return STOR_ERROR; }
    while (n < max) {
        unsigned char c = cbm_k_chrin();
        /* READST after the read, not before: the end-of-file bit is set by
           the read that hit it, and that read still returned a real byte. */
        if (cbm_k_readst()) { p[n++] = c; break; }
        p[n++] = c;
    }
    close_file();
    cmd_close();

    *got = n;

    /* A file that filled the buffer exactly might have had more to give, and
       we cannot tell -- so treat a full buffer as an error rather than hand
       back a silent truncation. Callers size with the format's own length. */
    if (n == max) return STOR_ERROR;
    return STOR_OK;
}

/* KNOWN BROKEN 2026-08-23 -- writing does not commit. READ WORKS; this does
 * not, and the failure is at the very last step.
 *
 * What is established, all by dumping the d64's directory sector after a run
 * rather than by reasoning:
 *
 *   - the DATA reaches the disk. A run that recorded JAMIE at -730 put those
 *     exact bytes in the image, in the right place-major record.
 *   - the OLD entry is removed correctly by scratch().
 *   - the NEW entry is created and never closed. It reads back as a splat
 *     file -- `*seq`, zero blocks -- so the next read finds nothing.
 *   - `@` replace is worse, not better: it left the entry as type 0xA1, with
 *     the replace-pending bit set and a nonsense block count.
 *   - swapping CLRCHN and CLOSE changes nothing.
 *   - none of it is the 2MHz clock, which was tested and cleared.
 *
 * So the fault is in the final CLOSE, and the most likely suspect is
 * llvm-mos's own KERNAL wrapper: `cbm_k_chkout` in the SAME library is
 * provably broken (it references __CHKOUT where the platform defines
 * __CKOUT, and fails at link), so a second bad wrapper is not a stretch.
 *
 * Next thing to try: write the OPEN/CHKOUT/BSOUT/CLOSE sequence in assembly
 * against the KERNAL vectors directly, bypassing the wrappers entirely.
 *
 * Until then a finished game is displayed correctly and simply is not saved.
 * The hall of fame still READS, which is most of what it is for. */
uint8_t plat_write_all(const char *name, const void *buf, uint16_t len) {
    const unsigned char *p = (const unsigned char *)buf;
    uint16_t i;
    unsigned char st;
    /* Scratch with channel 15, then CLOSE 15 before the data file is opened.
       The command channel is deliberately NOT held open across a write: on a
       read it has to be (closing it would take the data channel with it), but
       on a write the status is only wanted afterwards, and leaving 15 open
       across the data file's close left the new directory entry unclosed. */
    if (cmd_open()) { cmd_close(); return STOR_ERROR; }
    scratch(name);
    cmd_close();

    if (open_file(name, 'W')) { close_file(); return STOR_ERROR; }
    if (cbm_k_ckout(LFN_DATA)) { close_file(); return STOR_ERROR; }

    for (i = 0; i < len; i++) {
        cbm_k_bsout(p[i]);
        if (cbm_k_readst()) { close_file(); return STOR_ERROR; }
    }
    close_file();

    /* AFTER the close, not before it: on a CBM drive a write is not committed
       until the file is closed, and a disk-full shows up at that point. */
    if (cmd_open()) return STOR_ERROR;
    st = cmd_status();
    cmd_close();
    return classify(st);
}

/* Streaming, for the briefing. One file at a time by contract, so the open
   closes whatever was left behind rather than trusting the caller. */
static uint8_t streaming = 0;

uint8_t plat_open(const char *name) {
    unsigned char st;

    plat_close();
    if (open_file(name, 'R')) { close_file(); return STOR_ERROR; }

    st = cmd_status();
    if (st) { close_file(); return classify(st); }
    streaming = 1;
    return STOR_OK;
}

uint16_t plat_read(void *buf, uint16_t len) {
    unsigned char *p = (unsigned char *)buf;
    uint16_t n = 0;

    if (!streaming) return 0;
    if (cbm_k_chkin(LFN_DATA)) { return 0; }
    while (n < len) {
        unsigned char c = cbm_k_chrin();
        if (cbm_k_readst()) { p[n++] = c; streaming = 2; break; }
        p[n++] = c;
    }
    cbm_k_clrch();
    if (streaming == 2) { streaming = 0; cbm_k_close(LFN_DATA); }
    return n;
}

void plat_close(void) {
    if (streaming) { cbm_k_clrch(); cbm_k_close(LFN_DATA);
                     streaming = 0; }
}
