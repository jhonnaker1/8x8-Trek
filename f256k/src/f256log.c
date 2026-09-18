/* The message log's backing store: a RAM bank standing in for VDC RAM.
 *
 * WHAT THIS IS. The shared UI keeps its 2K message log outside the address
 * space, reached through three calls that were written for the C128's VDC --
 * set an address, then read or write bytes that auto-increment. It is not
 * video memory in any sense the UI cares about; it is a byte-addressed store
 * that costs no resident RAM, and the C128 happened to have one lying in its
 * display chip.
 *
 * This machine has forty-five spare banks, so it has one too. The log lives at
 * $1000-$17FF within a bank of its own -- 2K of the 8K, and the other 6K goes
 * unused rather than shared, because a second tenant in here would have to
 * agree with `log_addr()` about a layout that belongs to ui.c.
 *
 * WHY NOT A PLAIN ARRAY. 2,048 bytes against 2,682 spare in the resident
 * region would leave 634, and the low region has 2,153 left which is the soft
 * stack's slack. Spending a bank costs nothing -- that is what having 448K is
 * for.
 *
 * ONE BORROW PER BYTE, WHICH LOOKS WASTEFUL AND IS THE ONLY SAFE SHAPE. The
 * obvious optimisation is for vdc_set_address() to borrow the window and hold
 * it across the run of data calls that follows -- but ui_messages_view is
 * OVL_MSGS, so that loop can be executing FROM THE WINDOW, and holding the
 * borrow would unmap the code doing the borrowing. The cost is four MMU
 * stores a byte on a path that runs once per message and once per screenful,
 * which farmem.h's "read in chunks" rule is not about: nothing here is in a
 * drawing loop.
 */
#include <stdint.h>
#include "f256ovl.h"

/* After the overlays and the far store. far memory is capped at 64K by its
   uint16_t offsets -- eight banks -- so this sits above the highest it can
   reach rather than immediately after the images. */
#define F256_LOG_BANK (F256_FAR_BANK0 + 8)

static uint16_t at;

void vdc_set_address(unsigned int a) { at = (uint16_t)a; }

void vdc_data_write(unsigned char v)
{
    f256_win_borrow(F256_LOG_BANK);
    F256_WIN[at & 0x1FFFU] = v;
    f256_win_return();
    at++;
}

unsigned char vdc_data_read(void)
{
    unsigned char v;
    f256_win_borrow(F256_LOG_BANK);
    v = F256_WIN[at & 0x1FFFU];
    f256_win_return();
    at++;
    return v;
}
