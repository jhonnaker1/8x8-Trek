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

MEMORY { blk (rwx) : ORIGIN = 0x0800, LENGTH = 512 }
SECTIONS {
    .boot 0x0800 : {
        BYTE(0x01);          /* block count, and it is also the byte the boot
                                firmware SKIPS -- entry is $0801 */
        KEEP(*(.boot))
        . = 0x0800 + 512;
    } >blk
}
