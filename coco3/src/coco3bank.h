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
 * STATUS 2026-09-12: THE HARDWARE IS PROVEN. TWO REAL BUGS FIXED. ONE LEFT,
 * AND IT IS NARROWED TO TWO-PARAMETER FUNCTIONS.
 *
 * FIXED, both found by reading the GIME register reference rather than by
 * more guessing:
 *   - **TR IS IN $FF91, NOT $FF90.** INIT0 bit 6 is MMUEN, but its bits 1-0
 *     are MC1/MC0, the ROM MAP CONTROL. Clearing INIT0 bit 0 to "select task
 *     0" actually switched the machine from 32K external ROM to 32K internal
 *     and never selected a task at all. TR is bit 0 of INIT1 ($FF91).
 *   - An earlier note here claimed INIT0/INIT1 are WRITE-ONLY and that MAME's
 *     debugger was showing an internal latch. **That was invented and it is
 *     wrong: both registers are readable.** Retracted.
 *
 * STILL BROKEN, and here is the shape of it. `bank_get(3)` returns $3B
 * correctly after `bank_init`. Immediately after `bank_map(3, 0x30)`, the
 * SAME call returns $FF -- so **bank_map is not writing the register it is
 * asked to.** The one-parameter function works; the two-parameter ones
 * (bank_map, bank_peek, bank_poke) all misbehave, and bank_peek0 with no
 * parameters compiles to a correct `LDB $6000` yet reads the wrong block
 * because the mapping never happened.
 *
 * NEXT ATTEMPT: cmoc's generated code for bank_map (kept by `cmoc -i`). It
 * loads one parameter, PSHS B, then loads the other at an offset adjusted for
 * that push -- suspect either that adjustment or my reading of it. Write the
 * accessors in inline assembly if the calling convention cannot be trusted.
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
   a slot number rather than an address. */
void bank_map(unsigned char slot, unsigned char blk);

/* READ AND WRITE THE WINDOW THROUGH THESE, NEVER THROUGH A POINTER IN THE
   CALLER. cmoc has no `volatile`, and it will hoist a load from a literal
   address out of a sequence and cache it in a register: three reads of the
   paged window came back as the same stale $FF while the writes were landing
   correctly in blocks $30 and $31 -- proven by paging those blocks in from
   the host and finding 11 and 22 there. Because these live in their own
   translation unit, a caller cannot cache across them. */
unsigned char bank_peek(unsigned int off);
void          bank_poke(unsigned int off, unsigned char v);
unsigned char bank_peek0(void);
void          bank_poke0(unsigned char v);

/* What is currently mapped there. The GIME's MMU registers read back, which
   not every machine's do. */
unsigned char bank_get(unsigned char slot);

#endif
