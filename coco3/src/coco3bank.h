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
 * STATUS 2026-09-12: THE MECHANISM IS PROVEN, THIS WRAPPER IS NOT.
 * `tools/coco3/mmu3.c` -- the same operations written inline -- gives
 * 11 / 22 / 11 / 33 and restores the original block, at 128K and at 512K.
 * Going through these functions, `bank_map` and `bank_get` round-trip
 * correctly (the register reads back what was written) but reads through the
 * window come back $FF. **I have not explained that discrepancy**, so do not
 * trust this file until it reproduces the raw probe's result. The difference
 * is function calls versus inline stores and nothing else I can see.
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

/* What is currently mapped there. The GIME's MMU registers read back, which
   not every machine's do. */
unsigned char bank_get(unsigned char slot);

#endif
