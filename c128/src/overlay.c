#include <cbm.h>
#include <stdint.h>

#include "../../core/overlay.h"
#include "vdc.h"
#include "egavdc.h"
#include "../../core/ega.h"

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
/* NO EXPLICIT SIZE, AND THE ASSERT BELOW IS WHY. Written as
   ovl_name[OVL_COUNT] this array was left with TEN entries when OVL_XTRA made
   eleven, and C quietly zero-filled the eleventh: ovl_load(OVL_XTRA) called
   cbm_k_setnam(NULL), the load failed, and the window kept OVL_FRONT. main
   then called trek_new_game at the address it has in the XTRA layout, landed
   inside the FRONT image, and the game opened on an UNINITIALISED galaxy --
   stardate 0.0, no energy, scanners inoperative -- having first shown the SAVE
   GAME dialog, because that is what lives at that address in FRONT.

   Nothing caught it. The compiler is entitled to zero-fill, `make verify` was
   looking at sections rather than at this table, and every test passes because
   none of them loads an overlay. Sizing the array from its contents turns the
   next omission into a compile error. */
static const char *const ovl_name[] = {
    "0:OVLEVAL",
    "0:OVLHOF",
    "0:OVLFRONT",
    "0:OVLINFO",
    "0:OVLREPAIR",
    "0:OVLMSGS",
    "0:OVLPLANET",
    "0:OVLCMDS",
    "0:OVLTITLE",
    "0:OVLEVENTS",
    "0:OVLXTRA"
};

/* Same shape as the save-record assert in core/serial.c: a negative bitfield
   width is a hard error on every compiler this port has met. */
struct ovl_name_count_check {
    int ovl_name_has_one_entry_per_overlay :
        1 - 2 * !(sizeof ovl_name / sizeof ovl_name[0] == OVL_COUNT);
};

static uint8_t live = OVL_NONE;

/* SETBNK: A = the bank the data lands in, X = the bank holding the filename.
   Both 0 -- the window and this file are in bank 0. farmem.c leaves the
   KERNAL set to bank 1 only for the duration of its own load, but setting it
   here costs three bytes and removes the ordering assumption entirely. */
static void bank_for_data(void) {
    __asm__ volatile("lda #0\n\tldx #0\n\tjsr $ff68" ::: "a", "x", "memory", "p");
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
    {
        uint16_t end = (uint16_t)(uintptr_t)cbm_k_load(0, __ovl_start);
        uint16_t stamp;
        const uint8_t *tail;

        /* A FAILED LOAD IS FATAL, NOT SOMETHING TO RETURN FROM. This used to
           `return` quietly, which is the second half of the OVLXTRA bug: the
           caller's very next instruction jumps into the window, so a silent
           failure means running whatever the LAST overlay left there. There is
           no recovering from that and no way to describe it afterwards, so say
           which file could not be read and stop -- the same treatment the
           stamp mismatch below already got, and for the same reason. */
        if (end <= (uint16_t)(uintptr_t)__ovl_start) {
            scr_clear();
            scr_puts(2, 2, "CANNOT LOAD AN OVERLAY FROM THE DISK",
                     EGA_TO_VDC(EGA_LTRED));
            scr_puts(2, 4, ovl_name[which] + 2, EGA_TO_VDC(EGA_WHITE));
            scr_puts(2, 6, "IS MISSING FROM THE DISK IN DRIVE 8.",
                     EGA_TO_VDC(EGA_WHITE));
            scr_puts(2, 8, "THE GAME CANNOT RUN WITHOUT IT.",
                     EGA_TO_VDC(EGA_LTCYAN));
            for (;;) { }
        }

        /* THE STAMP: the low sixteen bits of ovl_load's address in the link
           this image was cut from, appended by the Makefile as its last two
           bytes. An overlay is linked WITH the resident half, so every call it
           makes into resident code is a fixed address from that same link --
           put yesterday's ovl*.prg on the disk beside today's trek128 and the
           game jumps into the middle of some other function, with no error and
           nothing naming either file.

           It cost an afternoon on the MEGA65 and produced two wrong diagnoses
           before the disk was even suspected, and it caught a real mismatch
           here the day this went in. Say which file is wrong, and stop. */
        tail  = (const uint8_t *)(uintptr_t)(end - 2);
        stamp = (uint16_t)(tail[0] | ((uint16_t)tail[1] << 8));
        if (stamp != (uint16_t)(uintptr_t)&ovl_load) {
            scr_clear();
            scr_puts(2, 2, "OVERLAY FILES ARE FROM A DIFFERENT BUILD",
                     EGA_TO_VDC(EGA_LTRED));
            scr_puts(2, 4, "THE OVL FILES ON THE DISK AND THE PROGRAM IN",
                     EGA_TO_VDC(EGA_WHITE));
            scr_puts(2, 5, "MEMORY MUST COME FROM THE SAME LINK.",
                     EGA_TO_VDC(EGA_WHITE));
            scr_puts(2, 7, "REBUILD THE D64 AND START AGAIN.",
                     EGA_TO_VDC(EGA_LTCYAN));
            for (;;) { }
        }
    }

    live = which;
}
