/* A minimal overlay image, linked at the WINDOW address and loaded into a
 * spare RAM block at startup. Proves the scheme end to end: page a block in,
 * call code living in it, get the right answer back, page it out again.
 *
 * FIRST FUNCTION IN THE FILE IS THE ENTRY POINT, and the resident half calls
 * it at a fixed address. A real overlay set wants a jump table at the top of
 * the image so the entry does not move when the code inside changes; this is
 * the mechanism test, so one entry is enough.
 *
 * NOTHING HERE MAY USE A LOCAL VARIABLE THAT OUTLIVES A PAGE-OUT, and nothing
 * here may page the window itself -- it would swap itself away mid-call, the
 * rule core/overlay.h states as rule 4 for every other port.
 */

unsigned char ovl_entry(unsigned char x)
{
    /* Deliberately not a constant: a constant could be folded into the
       caller if the two halves ever got linked together by accident, and
       then this would pass without the overlay being paged in at all. */
    return (unsigned char)(x ^ 0xA5);
}
