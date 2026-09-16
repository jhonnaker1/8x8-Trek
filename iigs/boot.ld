/* Block 0 runs at $0800 and is entered at $0801, so the first byte is a
   branch over itself -- the boot firmware jumps PAST the first byte, which is
   a detail that silently costs one instruction if you forget it. */
/* ENTRY AND KEEP, AND THE LINK IS SILENT WITHOUT THEM. The first build of
   this produced a 512-byte block containing the header byte and 509 zeros:
   --gc-sections is on by default, nothing here is referenced from an entry
   symbol, and the linker said so in a warning that reads like boilerplate --
   "cannot find entry symbol _start; not setting start address". It emitted a
   file, exit status 0, correct size. The same trap the Plus/4's BASIC header
   fell into. */
ENTRY(_boot)

/* $0300, NOT $0800, and the block still LOADS at $0800. The firmware puts
   block 0 at $0800 and jumps there; the first thing the code does is copy its
   own page down to $0300 and continue, because the game image wants $0800.
   Assembling for $0300 is what makes the `jmp` after the copy land in the
   copy. See the head of src/boot.S. */
MEMORY { blk (rwx) : ORIGIN = 0x0300, LENGTH = 512 }
SECTIONS {
    .boot 0x0300 : {
        BYTE(0x01);          /* block count, and it is also the byte the boot
                                firmware SKIPS -- entry is $0801 */
        KEEP(*(.boot))
        . = 0x0300 + 512;
    } >blk
}
