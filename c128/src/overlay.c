#include <cbm.h>
#include <stdint.h>

#include "../../core/overlay.h"

/* The C128's overlays: one KERNAL LOAD into a fixed window.
 *
 * WHY LOAD RATHER THAN BANK 1. The far store (core/farmem.h) reaches bank 1
 * through FETCH, a KERNAL call per byte, and nothing can execute from there
 * anyway -- so an overlay held in bank 1 would have to be copied into bank 0
 * before it could run. LOAD does the whole job in one call and lands the
 * bytes directly where they are to run. MEASURED 2026-08-26: 4,096 bytes in 6
 * to 14 jiffies, against 107 for the byte-at-a-time path. A swap is well
 * under a quarter of a second, which is nothing at a screen change.
 *
 * SECONDARY ADDRESS 0 is what makes it work: "put it where I say", so the
 * KERNAL reads and discards the file's two-byte load address and the overlay
 * lands at the window whatever address the linker built it for. The build
 * writes each overlay as a PRG with a two-byte header for exactly this
 * reason -- see the ovl rule in c128/Makefile.
 *
 * WHERE THE WINDOW IS is the linker's business, not a number repeated here:
 * trek128.ld sets __ovl_start at the top of the program region and shrinks
 * `ram` by the window size, so an overlay that outgrows the window is a LINK
 * ERROR rather than something that corrupts the resident code at run time.
 */

#define DEV      8
#define LFN_OVL  1        /* clear of storage.c's 2 and 15, and farmem's 1 --
                             both are done with theirs before this ever runs */

/* Defined by the linker script. Its ADDRESS is the window; the object itself
   is never read. */
extern char __ovl_start[];

/* One name per id in core/overlay.h. Still literals in the binary rather than
   pooled strings: the loader must work whether or not the pool loaded, and
   one short name is cheaper than the id and the fetch it would take. */
static const char *const ovl_name[OVL_COUNT] = {
    "0:OVLEVAL",
    "0:OVLHOF",
    "0:OVLFRONT",
    "0:OVLINFO",
    "0:OVLREPAIR",
    "0:OVLMSGS",
    "0:OVLPLANET"
};

static uint8_t live = OVL_NONE;

/* SETBNK: A = the bank the data lands in, X = the bank holding the filename.
   Both 0 -- the window and this file are in bank 0. farmem.c leaves the
   KERNAL set to bank 1 only for the duration of its own load, but setting it
   here costs three bytes and removes the ordering assumption entirely. */
static void bank_for_data(void) {
    __asm__ volatile("lda #0\n\tldx #0\n\tjsr $ff68" ::: "a", "x", "memory");
}

void ovl_load(uint8_t which) {
    if (which >= OVL_COUNT || which == live) return;

    /* Marked absent BEFORE the load, not after. If the load fails half way
       the window holds a mixture, and claiming it holds `which` would be a
       lie that survives into the next call. */
    live = OVL_NONE;

    bank_for_data();
    cbm_k_setlfs(LFN_OVL, DEV, 0);
    cbm_k_setnam(ovl_name[which]);
    if ((uint16_t)(uintptr_t)cbm_k_load(0, __ovl_start)
            <= (uint16_t)(uintptr_t)__ovl_start)
        return;                     /* KERNAL error code, not an end address */

    live = which;
}
