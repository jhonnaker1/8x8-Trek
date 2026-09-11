/* The boot record links flat at $0700 and nothing else. No C runtime, no zero
 * page map, no XEX wrapper -- llvm-objcopy -O binary gives the raw bytes that
 * go into sectors 1 onward, which is the only form the Atari OS will read.
 *
 * $0700 is where Atari DOS loads, and that is not a coincidence: it is the
 * lowest address on this machine that is free once the OS is up, so it is
 * both where DOS went and where this port's writable data is headed. The
 * loader is dead by the time the game runs, so the two can share it.
 */
SECTIONS {
    . = 0x0700;
    .text : { *(.text) }
    __boot_end = .;
}

/* TWO BOUNDS, and neither is a style rule.
 *
 * Three sectors is what tools/nodos.py reserves ahead of the directory at
 * sector 4, so a fourth sector of loader would be read as -- and would
 * overwrite -- the directory itself.
 *
 * The OS reads WHOLE SECTORS to $0700, so the last of those three lands at
 * $087F whatever the image's real length is. src/boot.s's sector buffer must
 * start above that, and it does, at $0900. */
ASSERT(__boot_end <= 0x0700 + 3 * 128,
       "boot.s has grown past the three sectors nodos.py reserves for it")
ASSERT(__boot_end <= 0x0900,
       "boot.s reaches its own sector buffer")
