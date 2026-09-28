/* The probe's link: a PRG at $0900, the header first, everything in the
 * file. C64 OS allocates the pages an application's FILE covers and no more,
 * so nothing may be NOLOAD. PAD_END, if defined on the command line
 * (-Wl,--defsym=PAD_END=0x6F00), grows the file with zeros up to that
 * address -- the test of how large a file C64 OS will load at $0900.
 */
MEMORY {
    ram : ORIGIN = 0x0900, LENGTH = 0xA500 - 0x0900
}

SECTIONS {
    .app_header : { KEEP(*(.app_header)) } >ram
    .text       : { *(.text .text.*) } >ram
    .data       : { *(.data .data.* .rodata .rodata.*) } >ram
    .pad        : {
        BYTE(0)
        . = DEFINED(PAD_END) ? MAX(., PAD_END) : .;
    } >ram
}

OUTPUT_FORMAT {
    SHORT(ORIGIN(ram))
    TRIM(ram)
}
