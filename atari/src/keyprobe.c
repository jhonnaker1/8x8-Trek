/* WHAT DOES A Key ACTUALLY PUT IN CH? -- and it is asked rather than looked
 * up, because the X16 shipped a header comment that had REASONED about its
 * keyboard encoding and was wrong: GETIN returns lowercase ASCII, not
 * PETSCII, and only a probe found it.
 *
 * The Atari OS's keyboard IRQ writes the raw key code into CH ($02FC) --
 * $FF when nothing is waiting -- with bit 6 for shift and bit 7 for ctrl.
 * That code is a KEYBOARD MATRIX POSITION, not ATASCII and not ASCII, so the
 * input driver needs a table, and the table should be MEASURED.
 *
 * This program takes no screen at all. It records every code it sees into a
 * buffer at $0600 -- page 6, the Atari's traditional free page, and nowhere
 * near the $3000.. this links to -- and tools/keytable.py injects a named key,
 * reads the buffer back through the bridge, and prints the pairing.
 *
 *      $0600   count of codes recorded
 *      $0601+  the codes, in the order they arrived
 */
#define CH   (*(volatile unsigned char *)0x02FC)
#define BUF  ((volatile unsigned char *)0x0600)

int main(void) {
    BUF[0] = 0;
    for (;;) {
        unsigned char c = CH;
        if (c != 0xFF) {
            CH = 0xFF;                     /* consume it, as the OS expects */
            if (BUF[0] < 200) {
                BUF[0]++;
                BUF[BUF[0]] = c;
            }
        }
    }
    return 0;
}
