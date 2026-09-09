#ifndef LOWMEM_H
#define LOWMEM_H

/* One writable object, placed somewhere that does not compete with code.
 *
 * THE C128 GETS THIS FOR FREE AND THE ATARI DOES NOT. On the C128 all writable
 * data lives in a `lowram` region at $1300..$1C00 that the code region never
 * sees, and the linker script does the whole job with no source changes. The
 * Atari's .data/.bss/.noinit come out of the same space as its code, and that
 * difference is about 2,300 bytes -- the single largest structural
 * disadvantage that target has. See atari/README.md.
 *
 * IT CANNOT BE FIXED WHOLESALE THERE, because the free low memory is not big
 * enough: $0480..$06FF is 640 bytes against 2,241 of writable data. So it is
 * fixed one object at a time, biggest first, and this is how such an object
 * says so without any other port paying attention.
 *
 * MEASURED, NOT READ OFF A MEMORY MAP. Every Atari memory map calls
 * $0480..$06FF free, but a map is not evidence about a running machine.
 * atari/src/lowprobe.c fills the region and then does the things this program
 * actually does -- a whole-file read, two hundred streamed reads, and a write
 * that makes DOS allocate sectors and rewrite its own VTOC -- and counts what
 * came back changed. Zero, with DOS 2.5 resident.
 *
 * WHAT GOES HERE IS NOT ZEROED. A section outside .bss is outside the range
 * the startup code clears, so an object placed here must be written before it
 * is read. That is a property of the object, checked at the site, not a
 * promise this macro can make.
 *
 * Expands to nothing unless the port asks for it, exactly as OVL_CODE does. */
#ifdef TREK_LOWMEM
#define TREK_LOW __attribute__((section(".lowbss")))
#else
#define TREK_LOW
#endif

#endif
