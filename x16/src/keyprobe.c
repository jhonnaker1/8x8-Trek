/* WHAT DOES GETIN ACTUALLY RETURN? -- a probe, not part of any build.
 *
 * The dispatcher in main.c compares cmd[0] against uppercase ASCII, and an
 * autoplay build that feeds it 'W','5' straight past the keyboard sets warp
 * 5 correctly. So the encoding coming out of $FFE4 is the only thing left,
 * and it has to be READ rather than reasoned about: the KERNAL's own mode
 * byte at $0372 says charset 2 and ISO off, which should mean PETSCII, and
 * PETSCII unshifted letters are $41..$5A -- the same values as the ASCII the
 * KB_* constants use. That prediction is what this contradicts or confirms.
 *
 * Sets the charset exactly as x16vera.c does first, in case that matters. */

static void chrout(unsigned char c) {
    __asm__ volatile("lda %0\n jsr $FFD2\n" :: "r"(c) : "a", "x", "y");
}

static unsigned char getin(void) {
    unsigned char c;
    __asm__ volatile("jsr $FFE4\n sta %0\n" : "=r"(c) :: "a", "x", "y");
    return c;
}

static void hex(unsigned char v) {
    unsigned char h = (unsigned char)(v >> 4), l = (unsigned char)(v & 15);
    chrout((unsigned char)(h < 10 ? '0' + h : 'A' + h - 10));
    chrout((unsigned char)(l < 10 ? '0' + l : 'A' + l - 10));
    chrout(' ');
}

int main(void) {
    unsigned char c;
    __asm__ volatile("lda #2\n jsr $FF62\n" ::: "a", "x", "y");
    for (;;) {
        c = getin();
        if (c) hex(c);
    }
}
