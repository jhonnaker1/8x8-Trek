/* Files, through the C65 DOS on device 8 -- reads AND writes.
 *
 * THIS PORT USED TO GO THROUGH THE HYPERVISOR and could not write at all:
 * mega65-libc's fileio is open/read512/close with no write of any kind, and
 * the only writing path the SD card offered was raw sector access, which would
 * have meant implementing a FAT32 writer. That is gone. The internal drive is
 * device 8, driven by the C65 DOS in ROM, and ordinary CBM KERNAL calls reach
 * it -- the same sequence c128/src/storage.c already uses, so this file is
 * mostly that one with a banking wrapper.
 *
 * WHY EVERYTHING MOVED, not just the save. Writes land in a D81 on device 8;
 * hyppo reads the SD card's FAT32. Those are different filesystems, so a save
 * written through the DOS is INVISIBLE to hyppo's findfile -- keeping both
 * would mean saving successfully and never reading it back. Measured
 * 2026-09-07: byte-at-a-time CHRIN pulls all 45,056 bytes of OVERLAYS.BIN in
 * 0.9 seconds, so there was no reason to keep the Hypervisor for speed.
 *
 * WHAT WENT WITH IT: m65hyppo.s and its __rc-saving shim, after_hyppo(), the
 * four-descriptor budget and the closeall-instead-of-close workaround, and a
 * 512-byte sector buffer that this port's README called the single biggest
 * thing it kept in bank 0. Reading a byte at a time needs no buffer at all.
 *
 * THE BANKING, and it is one bit. llvm-mos links unmap-basic.o into every
 * MEGA65 program: its .init pages the C65 ROM out ($01=$3E, $D030=$44) so the
 * program gets the RAM, and OPEN on device 8 then calls DOS code that is not
 * there and never returns. MEASURED which parts have to come back
 * (mega65/README.md, "What the C65 DOS actually needs"):
 *
 *     $01   bit 0 LORAM   BASIC over $A000..$BFFF   NOT needed -- leave it out
 *     $D030 bit 5 ROMC    C65 ROM over $C000..$CFFF REQUIRED -- without it,
 *                                                   OPEN hangs and never returns
 *
 * So the running state ($3E,$44) and what the DOS wants ($3E,$64) differ by
 * one bit, and $A000..$BFFF -- where io_buf, the hall of fame and the soft
 * stack live -- stays our RAM throughout. No trampoline, no relocation, and
 * the write loop can be ordinary C.
 *
 * WHAT IS SHADOWED is $C000..$CFFF, the overlay window. That is safe because
 * no code executes there during a call: every plat_* below maps, calls, and
 * unmaps before returning to whatever overlay called it. And the DOS does not
 * WRITE there either -- $C000..$CBFF was filled with a pattern and re-checked
 * after both a read and a write, zero bytes changed -- which matters because
 * the briefing streams from disk while an overlay is loaded.
 *
 * INTERRUPTS ARE ALREADY OFF and stay off. unmap-basic.o's .init does `sei`
 * and only its .fini does `cli`, so this program runs with I set from start to
 * finish; there is nothing to disable and nothing to restore. A probe with
 * `cli` hung identically to one without, so interrupts were never the reason
 * the DOS needed the ROM.
 */
#include <stdint.h>
#include <string.h>
#include <mega65/memory.h>
#include "../../core/storage.h"
#include "m65vid.h"

#define DEV      8
#define LFN_DATA 2
#define LFN_CMD  15

/* "p" IN EVERY CLOBBER LIST, AND IT IS THE WHOLE BUG THAT COST A DAY.
 *
 * llvm-mos names the 6502 status register "p", and inline asm that does not
 * declare it lets the compiler believe the FLAGS survive the call. They do
 * not: a `jsr` into the KERNAL returns with the carry set to whatever CLRCHN
 * felt like, and m65k_save's own `dex`/`bpl` loop rewrites N and Z on the way
 * past. What the compiler emitted for `if (a < '0' || a > '9')` was
 *
 *     cpx #$30          ; compare a with '0' -- SETS THE CARRY
 *     jsr m65k_save
 *     jsr $ffcc         ; CLRCHN runs here
 *     jsr m65k_rest
 *     bcc reject        ; branches on the carry from three calls ago
 *
 * which is why cmd_status() stored '0' and '0' into diagnostics and then
 * rejected them as non-digits. THE VALUES WERE NEVER CORRUPTED -- the flag
 * was. Turning the locals into `static volatile` globals appeared to fix it
 * only because it forced a reload after the call and moved the compare with
 * it; that was a symptom disappearing, not a cause being found. mos-platform's
 * own neo6502/kernel.h declares "p" on exactly this shape of call.
 *
 * THERE IS NO REGISTER-SAVING SHIM HERE, and that is a measurement rather than
 * an omission. A `m65k_save`/`m65k_rest` pair was written first, on the theory
 * that the C65 KERNAL clobbers llvm-mos's imaginary registers at $02..$21 the
 * way mega65-libc's own fileio.s clobbers __rc4/__rc5 around the Hypervisor
 * trap (see the September note in mega65-port). It did not fix anything,
 * because that was never the fault.
 *
 * MEASURED: filling $02..$21 with a pattern, making one CHRIN, and reading it
 * back changes ZERO bytes. The hyppo hazard was llvm-mos's OWN ASSEMBLY using
 * those addresses as scratch -- not a property of calling into ROM. A 1986
 * KERNAL knows nothing about imaginary registers and keeps to its own zero
 * page. And plat_open below holds C locals live across OPEN, CHKIN and the
 * status read, so a working save/restore round trip is the test for the calls
 * the probe did not cover.
 * llvm-mos's imaginary registers live at $02..$21, which is the C65 KERNAL's
 * own zero-page workspace, so a C value the compiler parked in one is silently
 * destroyed by any jsr into the ROM. This port already knew: m65hyppo.s did
 * the same thing around the Hypervisor's read512, and dropping the Hypervisor
 * deleted the shim while keeping the hazard.
 *
 * THE KERNAL VECTORS BY HAND, because mega65-libc has no cbm_k_* wrappers.
 * These globals carry arguments and results across the inline asm; they are
 * volatile with EXTERNAL linkage on purpose -- a `static` one written only by
 * asm has no writer the compiler can see, so LTO folds it away and the link
 * then fails on the asm's own reference to a symbol that no longer exists.
 * That cost two builds while the probes were being written. */
volatile uint8_t m65k_nlen, m65k_nlo, m65k_nhi;
volatile uint8_t m65k_lfn, m65k_sa, m65k_byte, m65k_p, m65k_st;

/* $A000..$BFFF stays RAM under this mapping, so these can live anywhere. */
static char cbmname[24];
static uint8_t namelen;

/* MAP AND UNMAP. Deliberately saves and restores $01 rather than assuming it
   is $3E: it is, today, because nothing but unmap-basic.o writes it -- but a
   write path that silently breaks if that ever changes is not worth the two
   bytes saved. `sei` is belt and braces; see the header. */
/* __attribute__((used)): this one is written by rom_in's asm and read by
   rom_out's, and C touches it nowhere -- so LTO internalised it, folded it
   away and the link failed on the asm's own reference. The others survive
   only because C assigns them. */
volatile uint8_t m65k_save01 __attribute__((used));

/* $00 IS SET FIRST, AND IT IS NOT DECORATION. $01's bits only drive the
   banking for the lines $00 marks as OUTPUTS, and mega65_io_enable() -- which
   this file calls on the way out, and which the old Hypervisor path called
   too -- pokes $00 with 65 to force full speed. That leaves the DDR wrong for
   the next map, so `lda $01` reads something that is not what was written and
   rom_out restores it. unmap-basic.o's own .init writes $2F here for exactly
   this reason; matching it costs two instructions. */
static void rom_in(void) {
    __asm__ volatile("sei\n\t"
                     "lda #$2f\n\tsta $00\n\t"
                     "lda $01\n\tsta m65k_save01\n\t"
                     "lda #$3e\n\tsta $01\n\t"
                     "lda #$64\n\tsta $d030" ::: "a", "memory", "p");
}
/* AND THE VIDEO HAS TO BE PUT BACK. The C65 DOS is KERNAL code that programs
   the VIC for its own screen, so it costs more than the Hypervisor ever did --
   after_hyppo() was a single mega65_io_enable() and that is not enough here.
   The first all-D81 build came up with a BLACK SCREEN from the first file load
   onward while the game ran perfectly underneath it. vdc_reclaim() re-applies
   the mode and deliberately does not clear; see m65vid.c. Keeping it here
   keeps the seam self-contained: nothing above this file has to know. */
static void rom_out(void) {
    __asm__ volatile("lda #$44\n\tsta $d030\n\t"
                     "lda m65k_save01\n\tsta $01" ::: "a", "memory", "p");
    vdc_reclaim();
}

static void k_setnam(const char *s, uint8_t len) {
    m65k_nlen = len;
    m65k_nlo  = (uint8_t)(uint16_t)s;
    m65k_nhi  = (uint8_t)(((uint16_t)s) >> 8);
    __asm__ volatile("lda m65k_nlen\n\tldx m65k_nlo\n\tldy m65k_nhi\n\t"
                     "jsr $ffbd" ::: "a", "x", "y", "memory", "p");
}

/* Carry out of OPEN/CHKIN/CHKOUT via php/pla -- no branches and no labels,
   which inline asm cannot safely carry if a function is ever cloned. */
static uint8_t k_open(uint8_t lfn, uint8_t sa) {
    m65k_lfn = lfn; m65k_sa = sa;
    __asm__ volatile("lda m65k_lfn\n\tldx #8\n\tldy m65k_sa\n\tjsr $ffba\n\t"
                     "jsr $ffc0\n\tphp\n\tpla\n\tsta m65k_p"
                     ::: "a", "x", "y", "memory", "p");
    return (uint8_t)(m65k_p & 1);
}
static void k_close(uint8_t lfn) {
    m65k_lfn = lfn;
    __asm__ volatile("lda m65k_lfn\n\tjsr $ffc3" ::: "a", "x", "y", "memory", "p");
}
static uint8_t k_chkin(uint8_t lfn) {
    m65k_lfn = lfn;
    __asm__ volatile("ldx m65k_lfn\n\tjsr $ffc6\n\tphp\n\tpla\n\tsta m65k_p"
                     ::: "a", "x", "memory", "p");
    return (uint8_t)(m65k_p & 1);
}
static uint8_t k_chkout(uint8_t lfn) {
    m65k_lfn = lfn;
    __asm__ volatile("ldx m65k_lfn\n\tjsr $ffc9\n\tphp\n\tpla\n\tsta m65k_p"
                     ::: "a", "x", "memory", "p");
    return (uint8_t)(m65k_p & 1);
}
static void k_bsout(uint8_t c) {
    m65k_byte = c;
    __asm__ volatile("lda m65k_byte\n\tjsr $ffd2" ::: "a", "memory", "p");
}
static uint8_t k_chrin(void) {
    __asm__ volatile("jsr $ffcf\n\tsta m65k_byte\n\t"
                     "jsr $ffb7\n\tsta m65k_st" ::: "a", "x", "y", "memory", "p");
    return m65k_byte;
}
static void k_clrchn(void) {
    __asm__ volatile("jsr $ffcc"
                     ::: "a", "x", "y", "memory", "p");
}

/* "NAME,S,W", and NO "0:" DRIVE PREFIX. The C128 port emits one and its 1541
   wants it; this DOS answered every open with an error until it came off.
   Probes 3 and 4 both opened unprefixed names and worked, which is what made
   the prefix the first suspect -- it was the only thing in this path that no
   probe had exercised.

   No '@' replace either: the at-sign replace is the one documented CBM DOS
   operation drives get wrong, and the scratch below is measured to work.
   Uppercased because the drive's directory is. */
static void cbm_name(const char *name, char mode) {
    uint8_t i = 0, j = 0;
    while (name[i] && j < sizeof cbmname - 5) {
        char c = name[i++];
        cbmname[j++] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }
    cbmname[j++] = ','; cbmname[j++] = 'S';
    cbmname[j++] = ','; cbmname[j++] = mode;
    cbmname[j] = '\0';
    namelen = j;
}

static uint8_t open_file(const char *name, char mode) {
    cbm_name(name, mode);
    k_setnam(cbmname, namelen);
    return k_open(LFN_DATA, LFN_DATA);
}

/* Conventional order: CLRCHN, then CLOSE. */
static void close_file(void) { k_clrchn(); k_close(LFN_DATA); }

/* THE COMMAND CHANNEL STAYS OPEN ACROSS A READ, and this is the C128 port's
 * hardest-won trap arriving here for free.
 *
 * On CBM DOS, closing secondary address 15 closes EVERY other file open on
 * that drive. So the obvious shape -- open the file, open 15, read the status,
 * close 15, then read the file -- destroys the very channel it was checking,
 * and the symptom is a read of zero bytes with READST 0x42 while the status it
 * just read said 00, OK. It looks exactly like a missing file.
 *
 * So 15 is opened FIRST, kept open as long as the data file, and closed LAST.
 */
static uint8_t cmd_open(void) {
    k_setnam(cbmname, 0);
    return k_open(LFN_CMD, LFN_CMD);
}
static void cmd_close(void) { k_clrchn(); k_close(LFN_CMD); }

/* The DOS error code, or 255 if the channel could not be read at all. The
   drive answers "NN, MESSAGE,TT,SS" and the WHOLE LINE has to be consumed or
   the next status read returns the tail of this one. MEASURED on this machine
   2026-09-07: 00 OK, 01 FILES SCRATCHED, 62 FILE NOT FOUND, 63 FILE EXISTS. */
static uint8_t cmd_status(void) {
    uint8_t a, b, i;

    if (k_chkin(LFN_CMD)) { k_clrchn(); return 255; }
    a = k_chrin();
    b = k_chrin();
    for (i = 0; i < 60; i++) {
        if (m65k_st) break;
        if (k_chrin() == 0x0D) break;
    }
    k_clrchn();
    if (a < '0' || a > '9' || b < '0' || b > '9') return 255;
    return (uint8_t)((a - '0') * 10 + (b - '0'));
}

static void cmd_send(const char *s) {
    uint8_t i = 0;
    if (k_chkout(LFN_CMD)) return;
    while (s[i]) k_bsout((uint8_t)s[i++]);
    k_clrchn();
}

/* Scratch before writing. MEASURED that this is load-bearing rather than
   assumed: writing the same name twice with no scratch between is refused with
   63 FILE EXISTS, and the file on the disk stays the old one. A probe where
   every step succeeds cannot tell you which step mattered. */
static void scratch(const char *name) {
    char cmd[24];
    uint8_t i = 0, j = 0;

    cmd[j++] = 'S'; cmd[j++] = '0'; cmd[j++] = ':';
    while (name[i] && j < sizeof cmd - 1) {
        char c = name[i++];
        cmd[j++] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }
    cmd[j] = '\0';
    cmd_send(cmd);
    (void)cmd_status();          /* consume it; the value is not interesting */
}

/* 62 is FILE NOT FOUND. Everything else non-zero is a real fault. */
static uint8_t classify(uint8_t code) {
    if (code == 0)  return STOR_OK;
    if (code == 62) return STOR_NOTFOUND;
    return STOR_ERROR;
}

uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got) {
    uint8_t *p = (uint8_t *)buf;
    uint16_t n = 0;
    uint8_t st;

    if (got) *got = 0;
    rom_in();

    if (cmd_open()) { cmd_close(); rom_out(); return STOR_ERROR; }
    if (open_file(name, 'R')) { close_file(); cmd_close(); rom_out();
                                return STOR_ERROR; }

    /* Ask BEFORE reading, which is only possible because 15 is still open. A
       missing file opens cleanly and then returns zero bytes, indistinguishable
       from an empty file until you look. */
    st = cmd_status();
    if (st) { close_file(); cmd_close(); rom_out(); return classify(st); }

    if (k_chkin(LFN_DATA)) { close_file(); cmd_close(); rom_out();
                             return STOR_ERROR; }
    while (n < max) {
        uint8_t c = k_chrin();
        /* READST after the read, not before: the end-of-file bit is set by the
           read that hit it, and that read still returned a real byte. The probe
           that measured this came back one byte short until it was fixed. */
        if (m65k_st) { p[n++] = c; break; }
        p[n++] = c;
    }
    close_file();
    cmd_close();
    rom_out();

    if (got) *got = n;
    /* A file that filled the buffer exactly might have had more to give and we
       cannot tell -- an error beats a silent truncation. Callers size with the
       format's own length plus one. */
    if (n == max) return STOR_ERROR;
    return STOR_OK;
}

uint8_t plat_write_all(const char *name, const void *buf, uint16_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    uint16_t i;
    uint8_t st;

    rom_in();

    /* Scratch on 15, then CLOSE 15 before the data file is opened. Not held
       open across a write: on a read it has to be, but here the status is only
       wanted afterwards, and leaving 15 open across the data file's close left
       the new directory entry unclosed on the C128. */
    if (cmd_open()) { cmd_close(); rom_out(); return STOR_ERROR; }
    scratch(name);
    cmd_close();

    if (open_file(name, 'W')) { close_file(); rom_out(); return STOR_ERROR; }
    if (k_chkout(LFN_DATA))   { close_file(); rom_out(); return STOR_ERROR; }

    for (i = 0; i < len; i++) k_bsout(p[i]);
    close_file();

    /* AFTER the close, not before: on a CBM drive a write is not committed
       until the file is closed, and a disk-full shows up at that point. */
    if (cmd_open()) { rom_out(); return STOR_ERROR; }
    st = cmd_status();
    cmd_close();
    rom_out();
    return classify(st);
}

/* THE STREAMING READ maps and unmaps per call rather than holding the ROM in
 * across the three of them. It has to: the briefing draws to the screen
 * between reads, and the overlay window is shadowed while the ROM is mapped.
 * The KERNAL's open-file table lives in low RAM, which never moves, so a file
 * opened by plat_open() is still open when plat_read() maps again.
 */
static uint8_t open_live;

/* WHERE plat_open GAVE UP, and the DOS code if it got that far. Two bytes, and
   they stay because "plat_open returned STOR_ERROR" names four different
   faults -- which cost three whole-game runs of guessing before they existed.
   src/probe_stream.c reads them; so can the monitor, via llvm-nm. */
uint8_t plat_dbg_stage __attribute__((used)), plat_dbg_status __attribute__((used));

uint8_t plat_open(const char *name) {
    uint8_t st;

    plat_close();
    plat_dbg_stage = 0; plat_dbg_status = 0;
    rom_in();
    if (cmd_open()) { plat_dbg_stage = 1;
                      cmd_close(); rom_out(); return STOR_ERROR; }
    if (open_file(name, 'R')) { plat_dbg_stage = 2;
                                close_file(); cmd_close(); rom_out();
                                return STOR_ERROR; }
    st = cmd_status();
    plat_dbg_status = st;
    if (st) { plat_dbg_stage = 3;
              close_file(); cmd_close(); rom_out(); return classify(st); }

    /* CHKIN ONCE, HERE -- not per plat_read(). CLRCHN sends UNTALK on the
       serial bus and ends the transfer, so doing chkin/read/clrchn per 64-byte
       chunk means 114 of those boundaries across STRINGS.DAT and 704 across
       OVERLAYS.BIN, and the pool came back corrupt. The channel stays selected
       for the life of the stream; unmapping the ROM between calls does not
       disturb it, because what CHKIN sets is KERNAL state in low RAM. */
    if (k_chkin(LFN_DATA)) { plat_dbg_stage = 4;
                             close_file(); cmd_close(); rom_out();
                             return STOR_ERROR; }
    plat_dbg_stage = 5;
    /* 15 stays open for the life of the stream -- closing it would take the
       data channel with it. plat_close() closes both, in order. */
    rom_out();
    open_live = 1;
    return STOR_OK;
}

uint16_t plat_read(void *dst, uint16_t len) {
    uint8_t *out = (uint8_t *)dst;
    uint16_t n = 0;

    /* 1 = open with more to give, 2 = end of file already seen. Reading on
       past EOF does not fail loudly: CHRIN keeps handing back a byte with the
       status bit set, so a caller looping `while (plat_read(...))` never
       terminates and fills its destination with repeats. */
    if (open_live != 1) return 0;
    rom_in();
    while (n < len) {
        uint8_t c = k_chrin();
        if (m65k_st & 0x80) break;            /* error: the byte is junk */
        out[n++] = c;
        if (m65k_st & 0x40) { open_live = 2; break; }   /* EOF, byte was good */
    }
    rom_out();
    return n;
}

void plat_close(void) {
    if (!open_live) return;
    rom_in();
    close_file();
    cmd_close();
    rom_out();
    open_live = 0;
}
