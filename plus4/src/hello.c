/* The smallest possible question: does main() run at all? */
__attribute__((used, section(".lowbss"))) unsigned char ran;
int main(void) { ran = 0x5A; *(volatile unsigned char *)0xFF19 = 0x72; for(;;){} return 0; }
