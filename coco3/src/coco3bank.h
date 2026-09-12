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
 * STATUS 2026-09-12: THE HARDWARE IS PROVEN. WRITES THROUGH THIS FILE WORK.
 * READS THROUGH IT DO NOT, AND I HAVE NOT EXPLAINED WHY.
 *
 * What is established, by paging blocks in FROM THE HOST and looking at the
 * physical RAM rather than trusting the 6809's own readback:
 *   - `bank_init` + `bank_map` + `bank_poke` put 0x11 into block $30 and
 *     0x22 into block $31, exactly as asked. The mapping and the writes are
 *     correct.
 *   - `bank_peek` returns $FF for every offset, even after filling the whole
 *     first page of the block with 0x11. Reads and writes to the same address
 *     in the same translation unit are reaching different places.
 *
 * Things ruled out, each by testing rather than reasoning: dead-store
 * elimination (the writes land); a cached load in the caller (moving the
 * accessors into this file changed nothing); a bad offset (filling the page
 * changed nothing); a read-modify-write on $FF90 (fixed, and it was not the
 * cause); and the stack in the window (that was a real crash, fixed with an
 * explicit LDS, and it is a separate bug).
 *
 * NEXT ATTEMPT STARTS AT THE GENERATED ASSEMBLY -- `cmoc -i` keeps it. Do not
 * build the overlay scheme on this until a read round-trips, because far
 * memory is nothing but reads.
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

/* What is currently mapped there. The GIME's MMU registers read back, which
   not every machine's do. */
unsigned char bank_get(unsigned char slot);

#endif
