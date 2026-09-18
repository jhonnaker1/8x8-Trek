/* 80x60 IS TOO BIG FOR THIS GAME, AND THE CHIP HAS A BETTER MODE.
 *
 * The console is 80x25. At 640x480 with an 8x8 cell that is 25 rows of
 * content stranded in 60, and 280 of 480 pixels black -- the game would sit
 * in a letterbox looking like it had failed to fill the screen.
 *
 * $D001 bit 2 is Tiny Vicky's DOUBLE_Y: the character cell becomes 8x16 and
 * the grid becomes 80x30. Two things follow, and the second is the real prize:
 *
 *   * 25 rows of 30 is a 2-row margin, not a 35-row hole.
 *   * AN 8x16 CELL IS CLOSER TO THE ORIGINAL THAN ANY OTHER PORT GETS. EGA
 *     Trek runs at 640x350 in an 8x14 cell; every 8-bit port here draws it at
 *     8x8 and loses the vertical detail. This one loses none of it.
 *
 * So: set the bit, then put markers where 80x30 says the corners are. If the
 * mode is what I think, they land on the corners; if it is not, the snapshot
 * says which way. Bit 1 (DOUBLE_X, 40 columns) is deliberately left alone --
 * the whole reason this machine is worth porting to is the eighty.
 */
#define IOCTRL (*(volatile unsigned char *)0x0001)
#define MATRIX ((volatile unsigned char *)0xC000)
#define VKY_MCR_L (*(volatile unsigned char *)0xD000)
#define VKY_MCR_H (*(volatile unsigned char *)0xD001)
#define COLS 80
#define ROWS 30          /* under test */

__attribute__((used, retain)) volatile unsigned char ran;

static void cell(unsigned char x, unsigned char y, unsigned char ch, unsigned char col)
{
    unsigned int off = (unsigned int)y * COLS + x;
    __asm__ volatile ("sei");
    IOCTRL = 2; MATRIX[off] = ch;
    IOCTRL = 3; MATRIX[off] = col;
    IOCTRL = 0;
    __asm__ volatile ("cli");
}
static void text(unsigned char x, unsigned char y, const char *s, unsigned char col)
{ while (*s) cell(x++, y, (unsigned char)*s++, col); }

int main(void)
{
    unsigned char x, y;
    unsigned int i;

    ran = 0x11;

    /* Clear the WHOLE 80x60 matrix, not the 80x30 under test: leftover cells
       from the old geometry would be indistinguishable from the new mode
       putting text where I did not expect. Clear more than you measure. */
    __asm__ volatile ("sei");
    IOCTRL = 2; for (i = 0; i < 80U * 60U; i++) MATRIX[i] = ' ';
    IOCTRL = 3; for (i = 0; i < 80U * 60U; i++) MATRIX[i] = 0x10;
    IOCTRL = 0;
    __asm__ volatile ("cli");

    __asm__ volatile ("sei");
    IOCTRL = 0;
    VKY_MCR_L = 0x01;         /* text on, no graphics layers */
    VKY_MCR_H = 0x04;         /* DOUBLE_Y: 8x16 cell, 80x30 */
    __asm__ volatile ("cli");

    /* The full 80x30 frame, so the edges are visible rather than inferred. */
    for (x = 0; x < COLS; x++) { cell(x, 0, '-', 0x90); cell(x, ROWS - 1, '-', 0x90); }
    for (y = 0; y < ROWS; y++) { cell(0, y, '|', 0x90); cell(COLS - 1, y, '|', 0x90); }
    cell(0, 0, '1', 0xC0);
    cell(COLS - 1, 0, '2', 0xC0);
    cell(0, ROWS - 1, '3', 0xC0);
    cell(COLS - 1, ROWS - 1, '4', 0xC0);

    /* And the console's own 80x25, centred: two rows above, three below. This
       is the thing that actually has to fit. */
    for (x = 1; x < COLS - 1; x++) { cell(x, 2, '=', 0xE0); cell(x, 26, '=', 0xE0); }
    text(3, 2,  " EGA TREK CONSOLE -- 80 COLUMNS BY 25 ROWS, 8x16 CELL ", 0xE0);
    text(3, 26, " TWO ROWS ABOVE, THREE BELOW, AND THE ORIGINAL IS 8x14 ", 0xE0);
    text(3, 14, "IF THE FRAME TOUCHES ALL FOUR EDGES, $D001 BIT 2 IS THE MODE.", 0xF0);

    ran = 0x5A;
    for (;;) { }
}
