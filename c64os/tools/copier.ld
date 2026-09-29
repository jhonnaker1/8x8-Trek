/* tools/copier.s: a PRG at $C000, code then data, below the result bytes
   at $C3F0 it reports through. */
MEMORY { ram : ORIGIN = 0xC000, LENGTH = 0x3F0 }
SECTIONS {
    .text : { KEEP(*(.text)) } >ram
    .data : { KEEP(*(.data)) } >ram
}
OUTPUT_FORMAT { SHORT(ORIGIN(ram)) TRIM(ram) }
