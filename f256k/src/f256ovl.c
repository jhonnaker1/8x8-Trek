/* Code overlays for the F256K, and they are NOT COPIED.
 *
 * Every other port copies an image into a window: off disk on the C128 (a
 * sixth of a second), out of banked RAM on the X16 and MEGA65, out of video
 * RAM on the Atari. Here each overlay image IS an 8K RAM bank and the window
 * is the MMU slot those banks map to -- so ovl_load is A SINGLE STORE and
 * nothing moves at all.
 *
 * WHAT THAT CHANGES, beyond speed. core/overlay.h's rules of thumb all rest
 * on one assumption, stated there: "the test is FREQUENCY, not size",
 * "fire_one_torpedo must NOT move because firing is the most frequent action
 * in the game" -- and both of those are about the cost of a DISK LOAD. The
 * Atari's note already records that a copy out of video RAM changes which
 * splits are affordable rather than which are correct. A slot store is
 * cheaper again. This port pays roughly ten cycles for a swap.
 *
 * The images are read from the card ONCE, at startup, into banks $08 upward.
 * After that the game never touches the disk except to save and restore.
 *
 * RULE 4 FROM overlay.h STILL APPLIES UNCHANGED. Cheap swaps make more
 * splits affordable; they do not make it safe for an overlay to call another
 * overlay, or for resident code to reach into the window unpaired. The window
 * still holds exactly one image at a time.
 */
#include <stdint.h>
#include "../../core/overlay.h"
#include "../../core/storage.h"
#include "../../core/ega.h"
#include "../../c128/src/vdc.h"
#include "f256vid.h"
#include "f256ovl.h"

/* THE WINDOW IS SLOT 5 ($A000-$BFFF) and the images live in banks $08 up.
   $00-$07 are the machine's working set: our address space plus MCP's, and
   bankprobe.c measured which. */
#define OVL_SLOT   F256_WIN_SLOT
#define OVL_BANK0  F256_OVL_BANK0
#define MMU_CTRL (*(volatile unsigned char *)0x0000)
#define MMU_SLOT ((volatile unsigned char *)0x0008)

#define OVL_FILE "OVERLAYS.BIN"
#define OVL_MAGIC0 'T'
#define OVL_MAGIC1 'O'
#define OVL_MAGIC2 'V'
#define OVL_MAGIC3 'L'
/* magic(4) + count(2) + stamp(2) + one size per overlay */
#define OVL_HDR (8 + 2 * OVL_COUNT)

extern char __ovl_start[];
/* A LINKER SYMBOL IS AN ADDRESS, NOT A VALUE. `__ovl_size` is declared as an
   array and its ADDRESS is the number -- writing it as an integer variable
   reads whatever bytes happen to live at $2000. */
extern char __ovl_size[];
#define OVL_WINDOW_BYTES ((uint16_t)(uintptr_t)__ovl_size)

/* THE STAMP'S ANCHOR IS A DATA SYMBOL, not a function, and the X16 learned
   why: LTO inlines ovl_load into main.c's loader stubs and drops the symbol,
   so llvm-nm finds nothing and &ovl_load folds to a constant. A volatile
   global cannot be folded away. */
volatile uint8_t ovl_anchor;

static uint8_t live = OVL_NONE;
static uint8_t ready;          /* 0 = not yet, 1 = images in banks, 2 = failed */

static void die(const char *a, const char *b, const char *c)
{
    scr_clear();
    scr_puts(2, 2, a, EGA_LTRED);
    scr_puts(2, 4, b, EGA_WHITE);
    scr_puts(2, 6, c, EGA_LTCYAN);
    for (;;) { }
}

/* Point the window at a bank. THE EDIT TARGET IS DERIVED FROM THE ACTIVE MAP,
   never assumed: bits 0-1 of $0000 say which MLUT the CPU is running on and
   bits 4-5 say which one $0008-$000F edits. MCP leaves both at 3 with edit
   already enabled ($B3, measured) -- so a bare store would work today and
   would be wrong the moment anything changed the edit target.

   UNDER sei BECAUSE $0008-$000F ARE ZERO PAGE when edit is off. Leaving the
   mode changed across an interrupt would have the kernel's handler read MLUT
   registers as its own variables, or ours read as registers. */
static void map_window(unsigned char bank)
{
    unsigned char save, act;
    __asm__ volatile ("sei");
    save = MMU_CTRL;
    act = (unsigned char)(save & 0x03);
    MMU_CTRL = (unsigned char)(0x80 | (act << 4) | act);
    MMU_SLOT[OVL_SLOT] = bank;
    MMU_CTRL = save;
    __asm__ volatile ("cli");
}

/* FAR MEMORY BORROWS THIS WINDOW, because there is no slot left to give it --
   see f256ovl.h. The live overlay is restored on return, and `live` is the
   only record of which one that is, which is why these two live here beside
   it rather than in f256far.c. */
void f256_win_borrow(unsigned char bank)
{
    map_window(bank);
}

void f256_win_return(void)
{
    /* OVL_NONE means no overlay has been loaded yet, so there is nothing to
       put back -- and mapping bank OVL_BANK0 + 0xFF would be a bank that does
       not exist. Leaving the borrowed bank mapped is correct in that case:
       the next ovl_load maps what it wants. */
    if (live != OVL_NONE) map_window((unsigned char)(OVL_BANK0 + live));
}

/* Read the images off the card into their banks. Called lazily from
   ovl_load(), which is how the X16 and the MEGA65 do it: a public ovl_init()
   that main() does not know about is a function LTO drops, taking the stamp's
   anchor with it. */
static void ovl_init(void)
{
    unsigned char hdr[OVL_HDR];
    uint16_t stamp, want, n;

    ovl_anchor = 0;     /* A REAL ACCESS, or the symbol does not survive:
                           volatile constrains how an access is emitted, not
                           whether an untouched object exists. */
    ready = 2;

    if (plat_open(OVL_FILE) != STOR_OK)
        die("OVERLAYS.BIN IS MISSING",
            "THE GAME CANNOT RUN WITHOUT IT.",
            "PUT IT BESIDE THE PROGRAM AND START AGAIN.");

    if (plat_read(hdr, OVL_HDR) != OVL_HDR ||
        hdr[0] != OVL_MAGIC0 || hdr[1] != OVL_MAGIC1 ||
        hdr[2] != OVL_MAGIC2 || hdr[3] != OVL_MAGIC3)
        die("OVERLAYS.BIN IS NOT AN OVERLAY FILE",
            "ITS HEADER DOES NOT SAY SO.",
            "REBUILD AND START AGAIN.");

    n = (uint16_t)(hdr[4] | ((uint16_t)hdr[5] << 8));
    if (n != OVL_COUNT)
        die("OVERLAYS.BIN HAS THE WRONG NUMBER OF IMAGES",
            "IT DOES NOT MATCH THIS PROGRAM.",
            "REBUILD AND START AGAIN.");

    /* THE STAMP, from day one rather than after it bites. An overlay is linked
       WITH the resident half, so every call it makes into resident code is a
       fixed address from that same link. Put yesterday's images beside today's
       program and the game jumps into the middle of some other function -- no
       error, and nothing naming either file. That cost an afternoon on the
       MEGA65 and produced two wrong diagnoses before the disk was suspected. */
    stamp = (uint16_t)(hdr[6] | ((uint16_t)hdr[7] << 8));
    if (stamp != (uint16_t)(uintptr_t)&ovl_anchor)
        die("OVERLAYS.BIN IS FROM A DIFFERENT BUILD",
            "THE IMAGES AND THE PROGRAM MUST COME FROM ONE LINK.",
            "REBUILD AND START AGAIN.");

    for (n = 0; n < OVL_COUNT; n++) {
        want = (uint16_t)(hdr[8 + n * 2] | ((uint16_t)hdr[9 + n * 2] << 8));
        if (want > OVL_WINDOW_BYTES)
            die("AN OVERLAY IS BIGGER THAN THE WINDOW",
                "THE IMAGES DO NOT FIT THIS PROGRAM.",
                "REBUILD AND START AGAIN.");
        /* Straight into the bank: the window IS the destination, so the
           storage layer writes where the image will run from. */
        map_window((unsigned char)(OVL_BANK0 + n));
        if (plat_read(__ovl_start, want) != want)
            die("OVERLAYS.BIN IS SHORT",
                "IT ENDED IN THE MIDDLE OF AN IMAGE.",
                "REBUILD AND START AGAIN.");
    }
    plat_close();
    ready = 1;
    live = OVL_NONE;
}

void ovl_load(uint8_t which)
{
    if (which >= OVL_COUNT || which == live) return;
    if (!ready) ovl_init();
    /* ovl_init() does not return on failure -- but if it ever learns to, the
       caller's NEXT INSTRUCTION jumps into the window, so a quiet return runs
       whatever the last overlay left there: code for a different screen, at
       the right address, with a plausible stack. overlay.h is explicit. */
    if (ready != 1) ovl_fatal(which);

    /* Marked absent BEFORE the store, as on every other port: if this were
       ever to fail half way, claiming the window holds `which` would be a lie
       that survives into the next call. */
    live = OVL_NONE;
    map_window((unsigned char)(OVL_BANK0 + which));
    live = which;
}
