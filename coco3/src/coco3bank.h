/* The GIME's MMU, which is this port's answer to a 64K address space.
 *
 * MEASURED 2026-09-12 on a STOCK 128K machine -- no RAM upgrade needed. The
 * GIME pages 8K blocks: the CPU's 64K is eight slots, and $FFA0..$FFA7 hold
 * the physical block number for each under task 0. A 128K machine has sixteen
 * blocks, $30..$3F; the address space uses the top eight ($38..$3F), which
 * leaves EIGHT SPARE BLOCKS -- 64K of RAM the CPU cannot otherwise reach.
 *
 * Verified by writing distinct signatures into blocks $30, $31 and $37,
 * paging each away and back, and finding them intact -- and by restoring the
 * original block and finding ITS contents intact too. Identical at 512K.
 *
 * WHY THIS BEATS EVERY OTHER PORT'S OVERLAY SCHEME. On the C128, MEGA65 and
 * X16 an overlay swap is a DISK READ. Here it is one write to an MMU
 * register: the image is already in RAM, in a block nothing else is using.
 * The Atari pays a 1541-class seek; this pays a store instruction.
 *
 * STATUS 2026-09-12: WORKING. Verified on the machine -- map a block, write,
 * page it away, page it back, read the value intact, restore the original
 * block and find IT intact. `bank_get` reports the right block throughout.
 *
 * THE RULE THIS COST A DAY TO LEARN, AND IT IS BIGGER THAN THE STACK:
 *
 *     NOTHING THE CODE TOUCHES MAY LIVE IN THE WINDOW -- and in C that
 *     includes EVERY LOCAL VARIABLE, because locals live in the stack frame
 *     and cmoc reloads a pointer local with `LDX -2,U` before every store
 *     through it.
 *
 * The first symptom was a crash: cmoc left S inside $6000-$7FFF, so the first
 * bank_map() paged away its own return address and the 6809 died on the RTS.
 * Moving S with `lds` fixed the crash and hid the rest of the problem -- **U
 * is the frame pointer and it was still in the window.** Every store through
 * a local pointer then went wherever the reloaded garbage pointed, which
 * looked for hours like reads failing while writes worked. They were not:
 * nothing was landing where I thought, including the results.
 *
 * So the port must place S *and* U outside whatever slot it pages, and any
 * variable a paging routine touches has to be static or absolute rather than
 * automatic. core/overlay.h's rule 4 says the same thing about calling into a
 * window; this is the data half of it.
 *
 * Two other real bugs, found by reading the GIME register reference:
 *   - **TR IS IN $FF91, NOT $FF90.** INIT0 bit 6 is MMUEN; its bits 1-0 are
 *     MC1/MC0, the ROM MAP CONTROL. Clearing INIT0 bit 0 to "select task 0"
 *     switched the machine from 32K external ROM to 32K internal and selected
 *     nothing.
 *   - The MMU registers DO read back, but **mask the top two bits** -- they
 *     return bus bleedover, documented, sometimes zero and sometimes one.
 *     (An earlier note here claimed these registers were write-only and that
 *     MAME was showing an internal latch. That was invented and is retracted.)
 *
 * AT BOOT THE MMU IS OFF. INIT0 ($FF90) reads $1B -- bit 6 clear -- and the
 * GIME maps the top 64K flat, which is exactly what task 0 already contains.
 * So the safe way to turn the MMU on is to write $38..$3F into task 0 first
 * and only then set bit 6, which changes nothing at the moment it takes
 * effect. Done the other way round the machine moves under its own feet.
 */
#ifndef COCO3BANK_H
#define COCO3BANK_H

#define BANK_SLOTS      8           /* 8K each, $0000,$2000,...,$E000 */
#define BANK_FIRST_FREE 0x30        /* the eight blocks the address space */
#define BANK_FREE_COUNT 8           /* does not use, on a stock 128K machine */
#define BANK_WINDOW     3           /* slot 3 = $6000..$7FFF is the window */

/* THE STACK MUST NOT LIVE IN THE WINDOW, and this cost a crashed machine to
   learn: cmoc left S inside $6000-$7FFF, so the first bank_map() paged away
   its own return address and the 6809 died on the RTS. The port has to place
   its stack deliberately, outside whatever slot it pages. core/overlay.h
   states the same rule for every other target -- it is just louder here,
   because the swap is instant and the corruption is immediate. */

/* Turns the MMU on without moving anything. Call once, with interrupts
   masked. Safe to call twice. */
void bank_init(void);

/* Maps physical block `blk` into address-space slot `slot` (0..7). The caller
   is responsible for not paging away the ground it is standing on -- the same
   rule core/overlay.h states for every other port, and the reason this takes
   a slot number rather than an address.

   A MACRO, NOT A FUNCTION, and deliberately: as a two-parameter function it
   did not write the register it was asked to, while the one-parameter
   bank_get worked. A macro compiles to a single store and leaves no calling
   convention to be wrong. */
#define bank_map(slot, blk) \
    (*((unsigned char *)(0xFFA0 + ((slot) & 7))) = (unsigned char)(blk))

/* READ AND WRITE THE WINDOW THROUGH THESE, NEVER THROUGH A POINTER IN THE
   CALLER. cmoc has no `volatile`, and it will hoist a load from a literal
   address out of a sequence and cache it in a register: three reads of the
   paged window came back as the same stale $FF while the writes were landing
   correctly in blocks $30 and $31 -- proven by paging those blocks in from
   the host and finding 11 and 22 there. Because these live in their own
   translation unit, a caller cannot cache across them. */
#define bank_peek(off)      (*((unsigned char *)(0x6000 + ((off) & 0x1FFF))))
#define bank_poke(off, v)   (*((unsigned char *)(0x6000 + ((off) & 0x1FFF))) = (unsigned char)(v))

/* What is currently mapped there. The GIME's MMU registers DO read back --
   but MASK THE TOP TWO BITS: they return bus bleedover, sometimes zero and
   sometimes one, which is documented and is why a raw read can look like
   nonsense. */
#define bank_get(slot)      ((unsigned char)(*((unsigned char *)(0xFFA0 + ((slot) & 7))) & 0x3F))

#endif
