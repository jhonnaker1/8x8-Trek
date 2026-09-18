#ifndef F256OVL_H
#define F256OVL_H

/* THE WINDOW IS SHARED, AND IT HAS TO BE. All eight MMU slots are spoken for:
 *
 *     0   $0000-$1FFF  zero page, the 6502 stack, MCP, and this port's
 *                      .rodata / .bss / soft stack
 *     1-4 $2000-$9FFF  resident code
 *     5   $A000-$BFFF  THE WINDOW
 *     6   $C000-$DFFF  the I/O window -- the display driver needs it
 *     7   $E000-$FFFF  the FoenixMCP kernel
 *
 * There is no eighth slot to give far memory, so far memory borrows slot 5
 * and puts the live overlay back. The Atari's farmem.h note already describes
 * this shape from the other side: "far memory, the message log and the SCREEN
 * are one mechanism seen three ways".
 *
 * BORROWING IT FROM INSIDE AN OVERLAY IS SAFE, and the reason is worth
 * stating because it looks unsafe. far_read is RESIDENT code, so the call
 * leaves the window before the window changes; the return address is on the
 * 6502 stack in slot 0, which never moves; and nothing executes from $A000
 * between the borrow and the return. What would NOT be safe is an overlay
 * reading far memory through a pointer it still holds across the call -- so
 * far_read copies into a caller-supplied buffer, which is what farmem.h's
 * "read in chunks, not bytes" rule already demands.
 */
#include "../../core/overlay.h"

#define F256_WIN       ((volatile unsigned char *)0xA000)
#define F256_WIN_SIZE  0x2000U
#define F256_WIN_SLOT  5

/* Banks $00-$07 are the machine's working set -- our address space plus MCP's
   -- measured by src/bankprobe.c. Overlays take one bank each from $08, and
   far memory starts after them. */
#define F256_OVL_BANK0 8
#define F256_FAR_BANK0 (F256_OVL_BANK0 + OVL_COUNT)

/* Point the window at a bank, and put the live overlay back afterwards.
   ALWAYS PAIRED: a borrow that does not return leaves the next call into an
   overlay running whatever the borrower mapped, at exactly the right address
   and with a plausible stack -- overlay.h's description of the worst failure
   this seam has. */
void f256_win_borrow(unsigned char bank);
void f256_win_return(void);

#endif
