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
volatile uint16_t result[4];      /* chrin jiffies, load jiffies, bytes, ok */
static unsigned char buf[PAYLOAD];

/* SETBNK. Not optional on this machine -- the C128 KERNAL defaults to bank
   15, where our buffer's address is ROM. Same call storage.c makes. */
static void set_banks(void) {
    __asm__ volatile("lda #0\n\tldx #0\n\tjsr $ff68" ::: "a", "x");
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

int main(void) {
    uint16_t t0, t1, t2, n1, n2;

    memset(buf, 0, sizeof buf);
    t0 = jiffies();
    n1 = read_chrin();
    t1 = jiffies();

    memset(buf, 0, sizeof buf);
    n2 = read_load();
    t2 = jiffies();

    result[0] = (uint16_t)(t1 - t0);
    result[1] = (uint16_t)(t2 - t1);
    result[2] = n1;
    result[3] = n2;

    for (;;) { }
}
