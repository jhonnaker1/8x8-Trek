#define INIT0 (*(unsigned char *)0xFF90)
#define MMU0  ((unsigned char *)0xFFA0)
#define W     ((unsigned char *)0x6000)
int main(void)
{
    unsigned char *r = (unsigned char *) 0x2F00;
    unsigned char i;
    r[0] = 0xA5;
    asm { orcc #$50 }
    for (i = 0; i < 8; i++) MMU0[i] = (unsigned char)(0x38 + i);
    INIT0 = (unsigned char)((0x1B | 0x40) & 0xFE);

    W[0] = 0x77; r[3] = W[0];          /* current block $3B -- does the WRITE path work? */

    MMU0[3] = 0x30; W[0] = 0x11; r[4] = W[0];
    MMU0[3] = 0x31; W[0] = 0x22; r[5] = W[0];
    MMU0[3] = 0x30;              r[6] = W[0];   /* 0x11 if $30 is real and distinct */
    MMU0[3] = 0x37; W[0] = 0x33; r[7] = W[0];
    MMU0[3] = 0x3B;              r[8] = W[0];   /* should be 0x77 again */
    r[2] = 0x5A;
    for (;;) ;
}
