/* A BASIC stub so RUN starts the program.
 *
 * llvm-mos has no `plus4` platform, so nothing supplies `basic-header.o`, and
 * the one in `c64/lib` encodes the C64's load address. Twelve bytes:
 *
 * BYTE-IDENTICAL TO WHAT BASIC 3.5 PRODUCES FOR THE SAME LINE, and that is
 * the whole design. A hand-built twelve-byte version -- the same bytes without
 * the space after the token -- loaded correctly, sat at $1001 with TXTTAB
 * pointing at it and a valid JMP at the SYS address, and answered ?SYNTAX
 * ERROR IN 10 when RUN. Rather than reason about why, the machine was asked:
 * `10 sys 4110` typed into BASIC 3.5 and $1001 read back. It keeps the space.
 *
 *   $1001  0C 10     link to the next line, $100C
 *   $1003  0A 00     line number 10
 *   $1005  9E        the SYS token
 *   $1006  20        A SPACE -- BASIC 3.5 stores one and the hand-built
 *                    header did not
 *   $1007  "4110"    the address in DECIMAL TEXT -- $100E, this header's own
 *                    thirteen bytes past the $1001 load address
 *   $100B  00        end of line
 *   $100C  00 00     end of program
 *
 * IN C RATHER THAN ASSEMBLY, and that is not a preference: a `.s` file with
 * `.section .basic_header,"a",@progbits` assembled to a section of size ZERO
 * without a diagnostic, and the program silently started at crt0 with the
 * header missing. `__attribute__((section))` puts the bytes where they are
 * asked for and the build fails loudly if it cannot.
 *
 * CHANGE THE HEADER AND 4109 CHANGES WITH IT. tools/verify_p4.py reads both
 * out of the built PRG and fails if they disagree.
 */
__attribute__((used, section(".basic_header")))
const unsigned char basic_header[13] = {
    0x0C, 0x10,             /* link to $100C */
    0x0A, 0x00,             /* line 10 */
    0x9E,                   /* SYS */
    0x20,                   /* A SPACE, and it is here because the machine puts
                               one here. See the note above. */
    '4', '1', '1', '0',
    0x00,                   /* end of line */
    0x00, 0x00              /* end of program */
};
