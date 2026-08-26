/* Which way of reading a file off the disk is faster: LOAD, or a CHRIN loop?
 *
 * NOT part of the game. Built and run on its own -- `make -C c128 loadbench`
 * writes a disk with two copies of the same 4,096-byte payload, one SEQ and
 * one PRG, reads each, and leaves the two jiffy counts in `result` for
 * tools/vice_mon.py to read out.
 *
 * WHY IT EXISTS. c128/src/storage.c reads a byte at a time through
 * cbm_k_chrin() with a cbm_k_readst() after every one -- two KERNAL calls per
 * byte, the slowest path the KERNAL offers -- and measurement on 2026-08-26
 * put a 1,092-byte file at about 550 bytes a second on a JiffyDOS machine
 * that should do far better. The KERNAL's LOAD reads a whole file in one call
 * and is what JiffyDOS accelerates hardest. If it is much faster then an
 * overlay swap is one LOAD straight into the window, and the far store and
 * the startup preload both drop out of the overlay design.
 *
 * BOTH PATHS MOVE THE SAME 4,096 BYTES into the same buffer, and the buffer
 * is cleared between them so a stale success cannot look like a fast one.
 */
#include <cbm.h>
#include <stdint.h>
#include <string.h>

#define DEV      8
#define LFN_DATA 2
#define LFN_CMD  15
#define PAYLOAD  4096u

/* Where the answers go. Volatile and file-scope so the linker gives them a
   fixed address the monitor can read, and nothing optimises them away. */
volatile uint16_t result[6];      /* chrin j, load j, n1, n2, bank1 j, n3 */
static unsigned char buf[PAYLOAD];

/* SETBNK. Not optional on this machine -- the C128 KERNAL defaults to bank
   15, where our buffer's address is ROM. Same call storage.c makes. */
static void set_banks(void) {
    __asm__ volatile("lda #0\n\tldx #0\n\tjsr $ff68" ::: "a", "x");
}

/* THE INTERESTING ONE. SETBNK's A is the bank the DATA goes to, so asking for
   bank 1 should make the KERNAL's LOAD write straight into the far store with
   no bank-0 buffer and no STASH at all. X stays 0 -- the filename is here. */
static void set_banks_far(void) {
    __asm__ volatile("lda #1\n\tldx #0\n\tjsr $ff68" ::: "a", "x");
}

/* The jiffy clock, $A0..$A2, high byte first. Read twice and retry if the
   low byte went backwards, so a tick between the two reads cannot tear the
   value -- the same argument as sid.c's raster_line(). */
static uint16_t jiffies(void) {
    volatile const unsigned char *t = (volatile const unsigned char *)0xA1;
    unsigned char hi, lo, hi2;
    for (;;) {
        hi  = t[0];
        lo  = t[1];
        hi2 = t[0];
        if (hi == hi2) return (uint16_t)((uint16_t)hi << 8 | lo);
    }
}

/* ---- path 1: the port's own read, a byte at a time through CHRIN ------- */
static uint16_t read_chrin(void) {
    uint16_t n = 0;

    set_banks();
    cbm_k_setlfs(LFN_CMD, DEV, LFN_CMD);
    cbm_k_setnam("");
    if (cbm_k_open()) return 0;

    cbm_k_setlfs(LFN_DATA, DEV, LFN_DATA);
    cbm_k_setnam("0:BENCH,S,R");
    if (cbm_k_open()) { cbm_k_close(LFN_CMD); return 0; }

    if (cbm_k_chkin(LFN_DATA) == 0) {
        while (n < PAYLOAD) {
            unsigned char c = cbm_k_chrin();
            if (cbm_k_readst()) { buf[n++] = c; break; }
            buf[n++] = c;
        }
    }
    cbm_k_clrch();
    cbm_k_close(LFN_DATA);
    cbm_k_close(LFN_CMD);
    return n;
}

/* ---- path 2: one KERNAL LOAD ------------------------------------------ */
static uint16_t read_load(void) {
    void *end;

    set_banks();
    /* Secondary address 0 means "put it where I say", so the file's own two
       header bytes are read and discarded. That is what an overlay wants:
       the destination is the window, whatever the file was built at. */
    cbm_k_setlfs(LFN_DATA, DEV, 0);
    cbm_k_setnam("0:BENCHP");
    end = cbm_k_load(0, buf);
    return (uint16_t)((unsigned char *)end - buf);
}

/* ---- path 3: one LOAD, straight into RAM bank 1 ----------------------- */
#define FAR_BASE 0x4000
static uint16_t read_load_bank1(void) {
    void *end;
    set_banks_far();
    cbm_k_setlfs(LFN_DATA, DEV, 0);
    cbm_k_setnam("0:BENCHP");
    end = cbm_k_load(0, (void *)FAR_BASE);
    set_banks();                       /* put it back before anything else */
    return (uint16_t)((uint16_t)(uintptr_t)end - FAR_BASE);
}

int main(void) {
    uint16_t t0, t1, t2, t3, n1, n2, n3;

    memset(buf, 0, sizeof buf);
    t0 = jiffies();
    n1 = read_chrin();
    t1 = jiffies();

    memset(buf, 0, sizeof buf);
    n2 = read_load();
    t2 = jiffies();

    n3 = read_load_bank1();
    t3 = jiffies();

    result[0] = (uint16_t)(t1 - t0);
    result[1] = (uint16_t)(t2 - t1);
    result[2] = n1;
    result[3] = n2;
    result[4] = (uint16_t)(t3 - t2);
    result[5] = n3;

    for (;;) { }
}
