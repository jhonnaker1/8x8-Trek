/* Files for the CoCo 3: the Disk BASIC filesystem, written against STANDALONE
 * DSKCON so that no ROM has to be mapped.
 *
 * WHY THIS IS OURS TO WRITE. cmoc ships a perfectly good read-only filesystem
 * in <disk.h> -- openfile, read, seek, close -- and this port cannot use it,
 * because it reaches the drive through the Disk BASIC ROM's DSKCON vector at
 * $C004. The game is 55,399 bytes spanning $1200..$EA66, which is where that
 * ROM would be, so the port runs in all-RAM mode and there is no ROM to call.
 * <dskcon-standalone.h> drives the WD1773 directly and needs none, but it
 * gives raw sectors only -- so the directory walk and the FAT are ours.
 *
 * THE ATARI PORT ARRIVED HERE THE SAME WAY: it wrote its own boot record,
 * directory and SIO seam after dropping Atari DOS, for the same reason, and
 * got a disk that was ours to give away as the bonus.
 *
 * THE FORMAT, verified against a real image rather than recalled (the chain
 * below was read off a disk `writecocofile` made, and the length it computes
 * matched the host file exactly):
 *
 *   35 tracks of 18 sectors of 256 bytes. Track 17 is the directory track.
 *   Sector 2 is the FAT: 68 granule bytes, $FF free, $C0|n the LAST granule
 *   with n sectors used, anything else the next granule number.
 *   Sectors 3..11 are directory entries, 32 bytes each, eight to a sector:
 *     0..7  name, space padded      11  file type
 *     8..10 extension               12  ASCII flag
 *     13    first granule           14..15 bytes used in the last sector
 *   A first byte of $FF is a free entry -- a freshly formatted disk is $FF
 *   throughout, which is why free and formatted are the same thing here.
 *
 *   A granule is nine sectors, half a track, and TRACK 17 IS SKIPPED in the
 *   numbering: granules 0..33 are tracks 0..16, and 34..67 are tracks 18..34.
 *
 * WRITING IS HERE, as of 2026-09-13. plat_write_all allocates granules and
 * writes the data, the FAT and the directory back, in that order, and
 * tools/writecheck.py has the machine write a file which the HOST then reads
 * off the .dsk with independent arithmetic -- a return code is not a witness.
 * The four paragraphs this replaced said writing was a stub returning
 * STOR_ERROR and stayed that way for a day after it stopped being true.
 *
 * DRIVING THE WD1773 DIRECTLY IS WHAT NORMALLY BREAKS SD-CARD REPLACEMENTS,
 * and it does not break the CoCo SDC -- which matters, because a real floppy
 * drive is now the rarer half of the hardware. The SDC emulates the controller
 * in silicon rather than hooking DSKCON in software:
 *
 *     "The CoCo SDC normally operates in FDC Emulation Mode. This makes it
 *      appear to the CoCo that a standard floppy disk controller is present."
 *         -- CoCo SDC User Guide v4, "Low-Level Hardware Interface"
 *
 * There is exactly one way for a driver to lose that, and the same page names
 * it: storing $43 in the control latch at $FF40 switches the card into
 * Command Mode, chosen because it "would not normally be used with a real
 * floppy controller". That is an assumption about the driver, and this port
 * brought its own. tools/sdccheck.py taps every write to $FF40 across a read
 * and a write and reports the set: $29/$A9 reading, $39/$B9 writing, 5,911
 * writes, no $43. MAME HAS NO SDC DEVICE, so that is the nearest thing to a
 * measurement available here and it is not a claim that the port runs on one.
 */
#include <stdint.h>
#include <dskcon-standalone.h>

#include "../../core/storage.h"

#define DIR_TRACK    17
#define FAT_SECTOR    2
#define DIR_FIRST     3
#define DIR_LAST     11
#define SEC_SIZE    256
#define GRAN_SECS     9
#define GRAN_BYTES  (GRAN_SECS * SEC_SIZE)      /* 2304 */
#define NUM_GRAN     68
#define ENT_SIZE     32
#define ENT_PER_SEC   8

/* THE MULTIPAK SLOT REGISTER IS NOT TOUCHED, and that is a decision.
   $FF7F selects which slot sees the $FF40-$FF5F I/O range in bits 1-0 AND
   which sees the ROM at $C000-$FEFF in bits 5-4. Writing it was tried while
   chasing the boot and never proved anything; worse, the ROM half of it moved
   the map mid-transfer.
   THE EVIDENCE THAT IT IS ALREADY RIGHT: `LOADM"TREKLDR"` answers OK. Disk
   BASIC reads three kilobytes off this drive with whatever the machine
   already had in $FF7F, so the slot is correct before this port runs, and
   setting it can only be a way to get it wrong. */


static unsigned char fat[NUM_GRAN];
static unsigned char secbuf[SEC_SIZE];
static unsigned char ready;                     /* dskcon_init has run */
static unsigned long dsk_handle;

/* The open stream, for plat_open/plat_read. ONE at a time, which is what
   core/storage.h promises. */
static struct {
    unsigned char open;
    unsigned char gran;         /* current granule */
    unsigned char sec;          /* 0..8 within the granule */
    unsigned int  pos;          /* read offset within secbuf */
    unsigned int  avail;        /* valid bytes in secbuf */
    unsigned int  lastbytes;    /* bytes used in the file's last sector */
} st;

static void gran_loc(unsigned char g, unsigned char *trk, unsigned char *sec)
{
    unsigned char t = (unsigned char)(g >> 1);
    if (t >= DIR_TRACK) t++;                    /* the directory track is skipped */
    *trk = t;
    *sec = (unsigned char)((g & 1) ? 10 : 1);
}

/* One sector into secbuf. Non-zero on success. */
/* STOR_TRACE: leave a breadcrumb at $2010 before every sector, so a hang says
   WHICH sector rather than just "somewhere in the read". Absolute address,
   not a local pointer -- cmoc drops stores through one of those. */
#ifdef STOR_TRACE
#define TR ((unsigned char *)0x2010)
#endif

/* IRQ OFF ACROSS EVERY DSKCON CALL, AND IT IS NOT DEFENSIVE TIDINESS.
   The CoCo's WD1773 transfers a sector in HALT MODE: the controller holds the
   CPU between bytes and releases it exactly when the next one is ready. An
   interrupt taken in that window costs the byte, the sector never completes,
   and DSKCON then waits for ever for an NMI that is not coming. It presents
   as a hang inside dskcon_processSector with the drive idle.
   MEASURED: the card-less port's sound driver is this project's first
   interrupt source, and the game hung on the 46th sector of its startup --
   forty-five read perfectly, and the forty-sixth was the first one after
   snd_music() armed the timer. Disk BASIC's own DSKCON masks for the same
   reason; nothing here had to until now.
   THE CALLER'S FLAGS ARE PRESERVED rather than blindly re-enabled, because
   the first-stage loader runs with interrupts masked and no handler
   installed -- turning them on underneath it would be a different hang. And
   the save goes in a STATIC, not on the stack: cmoc addresses locals
   relative to S with -fomit-frame-pointer, so a `pshs cc` here would move
   every one of them out from under the function. */
static unsigned char dsk_cc;

static void dsk_mask(void)
{
    asm { tfr cc,a
          sta _dsk_cc
          orcc #$10 };
}

static void dsk_unmask(void)
{
    asm { lda _dsk_cc
          tfr a,cc };
}

static unsigned char read_sec(unsigned char trk, unsigned char sec)
{
#ifdef STOR_TRACE
    TR[0] = trk;
    TR[1] = sec;
    /* SIXTEEN BITS, because eight was ambiguous and I misread it. A byte
       counter said "46 attempted" and I took that as the 46th sector; the
       string pool alone does hundreds, so 46 could have been 302 or 558 and
       the read I was chasing might not have been the one I named. */
    if (++TR[2] == 0) TR[6]++;               /* sectors attempted, lo/hi */
    TR[3] = 0xB1;                            /* about to call DSKCON */
#endif
    DCOPC = 2;
    DCDRV = 0;
    DCTRK = trk;
    DCSEC = sec;
    DCBPT = secbuf;
    dsk_mask();
    dskcon_processSector();
    dsk_unmask();
#ifdef STOR_TRACE
    TR[3] = 0xB2;                            /* DSKCON returned */
    TR[4] = DCSTA;
    if (++TR[5] == 0) TR[7]++;               /* sectors completed, lo/hi */
#endif
    return (unsigned char)(DCSTA == 0);
}

/* THE VECTOR TABLE, MEASURED ON THE MACHINE RATHER THAN RECALLED -- and it is
   not the order this port assumed. $FFF2..$FFFF hold FE EE / FE F1 / FE F4 /
   FE F7 / FE FA / FE FD, and each of those is an LBRA whose 16-bit wrap lands
   in the low table:

       $FEEE SWI3 -> $0100   $FEF1 SWI2 -> $0103   $FEF4 FIRQ -> $010F
       $FEF7 IRQ  -> $010C   $FEFA SWI  -> $0106   $FEFD NMI  -> $0109

   As BASIC leaves them: $0100/$0103 are RTI, $0106 is three zero bytes,
   $0109 is JMP $D8A1, $010C is JMP $D8AF, $010F is JMP $A0F6. THE LAST THREE
   ALL POINT INTO ROM THIS PORT PAGES AWAY, so once $FFDF is written every one
   of them jumps into whatever the image happens to hold there.

   ORCC #$50 DOES NOT COVER THIS, and that was the load-bearing mistake. The
   DSKCON library unmasks the interrupts itself -- ANDCC #$AF sits inside its
   own wait loop and there is a CWAI #$3A further on -- so masking them before
   calling it buys nothing past the first sector. <dskcon-standalone.h> says so
   in as many words: "The IRQ service routine must be coded so that it invokes
   dskcon_irqService()." Nothing in this port did. */
#define VECSLOT(a, fn)                                                       \
    do {                                                                     \
        *((unsigned char *)(a)) = 0x7E;              /* JMP */               \
        *((void **)((a) + 1)) = (void *)(fn);                                \
    } while (0)

/* The service the library asks for. The PIA read IS the acknowledgement, and
   it is inline asm because CMOC HAS NO volatile: a C read whose value is
   thrown away is free to vanish, and this one must not -- the same trap that
   deleted the loader's first report. */
#ifdef STOR_TRACE
/* COUNT THEM. A ~1000x slowdown is an interrupt RATE claim, and a rate claim
   should be measured rather than reasoned about from which bit is set. */
#define IRQCNT ((unsigned int *)0x2020)
#endif

interrupt void trek_irq(void)
{
    asm { lda $FF02 }                   /* ack PIA0 port B, the 60Hz tick */
#ifdef STOR_TRACE
    IRQCNT[0] = (unsigned int)(IRQCNT[0] + 1);
#endif
    dskcon_irqService();                /* the motor timer is its business */
}

/* FIRQ HAS TO BE ACKNOWLEDGED, NOT JUST RETURNED FROM. PIA1's $FF23 reads $37
   on this machine -- bit 0 set, so CB1 (the cartridge line) can raise a FIRQ --
   and a PIA interrupt is a LEVEL, not an edge: returning without reading the
   data register leaves the line asserted and the 6809 comes straight back in.
   "A bare RTI makes a stray interrupt harmless" is FALSE HERE; it makes it
   permanent. That belief is what the game's bootstrap was written on. */
interrupt void trek_firq(void)
{
    asm { lda $FF22 }                   /* ack PIA1 port B, the CART line */
#ifdef STOR_TRACE
    IRQCNT[1] = (unsigned int)(IRQCNT[1] + 1);
#endif
}

/* SWI is an INSTRUCTION, not a line -- there is nothing to acknowledge, so
   here a bare RTI really is harmless. */
interrupt void trek_stray(void)
{
}

/* One sector OUT of secbuf. DSKCON opcode 3 is write; 2 is read. */
static unsigned char write_sec(unsigned char trk, unsigned char sec)
{
    DCOPC = 3;
    DCDRV = 0;
    DCTRK = trk;
    DCSEC = sec;
    DCBPT = secbuf;
    dsk_mask();                              /* same reason as read_sec */
    dskcon_processSector();
    dsk_unmask();
    return (unsigned char)(DCSTA == 0);
}

static unsigned char disk_ready(void)
{
    if (ready) return 1;
    asm { orcc #$50 }                           /* init wants interrupts masked */
    dsk_handle = dskcon_init(dskcon_nmiService);

    /* TAKE THE NMI JUMP SLOT. THIS IS THE ONE THAT COST THE MOST TO FIND.
       The 6809's NMI vector at $FFFC points to $FEFD -- a JMP in the CoCo's
       RAM vector table -- and that JMP goes to Disk BASIC's NMI handler in
       $8000-$BFFF. dskcon_init() only sets DNMIVC and NMIFLG, which are
       variables THAT ROM HANDLER reads. THIS PORT PAGES THE ROM AWAY, so the
       handler address holds the port's own image, and the floppy controller
       raises an NMI on every completed operation.
       Measured: the first-stage loader read 99 sectors -- every one
       successful, status 00 -- and died once the copy passed $8000 and
       replaced the handler with game data. It also explains why the failure
       moved around between runs: it depends on when an NMI lands.
       So the slot is pointed straight at dskcon's own service, which is what
       the ROM handler would have called anyway. */
    *((unsigned char *)0xFEFD) = 0x7E;                  /* JMP */
    *((void **)0xFEFE) = (void *)dskcon_nmiService;

    /* AND THE OTHER THREE, before the first sector rather than never.
       Measured: the loader read 97 data sectors, then took an interrupt that
       vectored to $D8AF -- Disk BASIC's IRQ handler, in ROM that is no longer
       there -- and the CPU was caught executing $D8B7, eight bytes past it.
       It ended up at $A7D5, BASIC's keyboard poll, which is not a hang and
       not a slow floppy: both of those were read off the same run and both
       were wrong. */
#ifdef STOR_TRACE
    IRQCNT[0] = 0;                      /* RAM reads $FF; start from zero */
    IRQCNT[1] = 0;
#endif
    VECSLOT(0x010C, trek_irq);                          /* IRQ  */
    VECSLOT(0x010F, trek_firq);                         /* FIRQ */
    VECSLOT(0x0106, trek_stray);                        /* SWI  */
    if (!read_sec(DIR_TRACK, FAT_SECTOR)) return 0;
    {   unsigned char i;
        for (i = 0; i < NUM_GRAN; i++) fat[i] = secbuf[i];
    }
    ready = 1;
    return 1;
}

/* "STRINGS.DAT" -> eleven bytes of name and extension, space padded, which is
   how a directory entry stores it. Upper-cased: every name this game uses is
   already upper case, and a lower-case one would silently never be found. */
static void normalise(const char *src, char *out)
{
    unsigned char i = 0, j;
    for (j = 0; j < 11; j++) out[j] = ' ';
    for (j = 0; j < 8 && src[i] && src[i] != '.'; j++, i++)
        out[j] = (char)((src[i] >= 'a' && src[i] <= 'z') ? src[i] - 32 : src[i]);
    while (src[i] && src[i] != '.') i++;
    if (src[i] == '.') {
        i++;
        for (j = 8; j < 11 && src[i]; j++, i++)
            out[j] = (char)((src[i] >= 'a' && src[i] <= 'z') ? src[i] - 32 : src[i]);
    }
}

/* Finds a file and returns its first granule, or 0xFF. `lastbytes` receives
   the byte count of its final sector. */
static unsigned char find_file(const char *name, unsigned int *lastbytes)
{
    char want[11];
    unsigned char s, e, k;

    normalise(name, want);
    for (s = DIR_FIRST; s <= DIR_LAST; s++) {
        if (!read_sec(DIR_TRACK, s)) return 0xFF;
        for (e = 0; e < ENT_PER_SEC; e++) {
            unsigned char *p = secbuf + (unsigned int)e * ENT_SIZE;
            if (p[0] == 0xFF || p[0] == 0x00) continue;   /* free entry */
            for (k = 0; k < 11 && p[k] == (unsigned char)want[k]; k++) { }
            if (k == 11) {
                *lastbytes = (unsigned int)p[14] * 256u + p[15];
                return p[13];
            }
        }
    }
    return 0xFF;
}

/* Length from the FAT chain: every full granule is 2304 bytes, and the last
   contributes (sectors-1)*256 plus the directory's byte count. */
static unsigned long file_len(unsigned char gran, unsigned int lastbytes)
{
    unsigned long n = 0;
    unsigned char g = gran, guard = 0;

    while (guard++ < NUM_GRAN + 1) {
        unsigned char v = fat[g];
        if ((v & 0xC0) == 0xC0)
            return n + (unsigned long)((v & 0x3F) - 1) * SEC_SIZE + lastbytes;
        n += GRAN_BYTES;
        g = v;
        if (g >= NUM_GRAN) break;               /* a corrupt chain, not a loop */
    }
    return 0;
}

/* ---------------------------------------------------------- random access
 *
 * RANDOM ACCESS, FOR A FAR STORE THAT LIVES ON THE DISK. core/storage.h is
 * deliberately sequential -- plat_open/plat_read stream, and nothing in the
 * shared code needs to seek -- but the card-less CoCo 3 has no VRAM to keep a
 * string pool in and no MMU to bank one with, so its far_read has to reach
 * into STRINGS.DAT at an arbitrary offset. See coco3gime/src/gimemem.c.
 *
 * NOT ADDED TO core/storage.h. That header is the portable contract and it
 * must not learn about seeking for one machine's benefit; this is a CoCo 3
 * filesystem operation that happens to be useful to a CoCo 3 far store, and
 * it is declared in coco3storage.h beside the machine it belongs to.
 *
 * THE FAT WALK COSTS NO DISK ACCESS. The chain is already in RAM -- disk_ready
 * reads it once -- so finding the granule for a byte offset is arithmetic,
 * and only the sector itself is a read. A 7,483-byte pool is four granules
 * and twenty-nine sectors.
 */
unsigned char plat_raw_open(const char *name, unsigned long *len)
{
    unsigned int lastbytes = 0;
    unsigned char gran;

    if (!disk_ready()) return 0xFF;
    gran = find_file(name, &lastbytes);
    if (gran == 0xFF) return 0xFF;
    if (len) *len = file_len(gran, lastbytes);
    return gran;
}

/* One 256-byte sector, by its index within the file, into `dst`. Returns
   non-zero on success. The caller caches; this does not, because secbuf is
   shared with every other operation here and a cache tag would have to be
   invalidated by all of them. */
unsigned char plat_raw_sector(unsigned char first, unsigned int index,
                              unsigned char *dst)
{
    unsigned char g = first, trk, sec, guard = 0;
    unsigned int within = index;
    unsigned int i;

    if (first == 0xFF || !disk_ready()) return 0;

    /* Walk the chain nine sectors at a time. GRAN_SECS is the granule size in
       sectors; the last granule is short and its count is in the FAT byte. */
    while (within >= 9) {
        unsigned char v = fat[g];
        if ((v & 0xC0) == 0xC0) return 0;       /* past the end of the file */
        within = (unsigned int)(within - 9);
        g = v;
        if (g >= NUM_GRAN || guard++ > NUM_GRAN) return 0;
    }

    gran_loc(g, &trk, &sec);
    if (!read_sec(trk, (unsigned char)(sec + within))) return 0;
    for (i = 0; i < SEC_SIZE; i++) dst[i] = secbuf[i];
    return 1;
}

/* Forces the next call to re-run dskcon_init and re-read the FAT. Enabling
   the GIME's MMU appears to disturb something the disk driver set up, and
   because disk_ready() latches, nothing would ever re-establish it. */
void plat_disk_reset(void)
{
    ready = 0;
}

uint8_t plat_read_all(const char *name, void *buf, uint16_t max, uint16_t *got)
{
    unsigned char *dst = (unsigned char *)buf;
    unsigned char gran, trk, sec, i;
    unsigned int lastbytes;
    unsigned long len;
    unsigned int copied = 0;

    if (got) *got = 0;
    if (!disk_ready()) return STOR_ERROR;

    gran = find_file(name, &lastbytes);
    if (gran == 0xFF) return STOR_NOTFOUND;

    len = file_len(gran, lastbytes);
#ifdef STOR_TRACE
    /* WHAT DID THE CHAIN ACTUALLY SAY? 24,832 bytes copied is 97 sectors
       against 99 read, and that two-sector gap is what `n` going to zero
       looks like -- which happens when copied reaches len. So len is the
       suspect, and len comes from the FAT chain and the directory. */
    TR[11] = (unsigned char)(len >> 8);
    TR[12] = (unsigned char)(len & 0xFF);
    TR[13] = (unsigned char)(lastbytes >> 8);
    TR[14] = (unsigned char)(lastbytes & 0xFF);
    TR[15] = gran;                 /* first granule */
    TR[16] = fat[gran];            /* and the first few links */
    TR[17] = fat[12];
    TR[18] = fat[13];
    TR[19] = fat[20];
#endif
    if (len == 0) return STOR_ERROR;
    /* storage.h: a file longer than max is an ERROR, not a truncation. */
    if (len > (unsigned long)max) return STOR_ERROR;

    while (gran < NUM_GRAN) {
        unsigned char v = fat[gran];
        unsigned char nsec = (unsigned char)(((v & 0xC0) == 0xC0) ? (v & 0x3F) : GRAN_SECS);
        gran_loc(gran, &trk, &sec);
        for (i = 0; i < nsec; i++) {
            unsigned int n;
            if (!read_sec(trk, (unsigned char)(sec + i))) return STOR_ERROR;
            n = (copied + SEC_SIZE <= (unsigned int)len)
                    ? (unsigned int)SEC_SIZE
                    : (unsigned int)len - copied;
            {   unsigned int k;
                for (k = 0; k < n; k++) dst[copied + k] = secbuf[k];
            }
            copied = (unsigned int)(copied + n);
#ifdef STOR_TRACE
            TR[6] = (unsigned char)(copied >> 8);   /* how far the copy got */
            TR[7] = (unsigned char)(copied & 0xFF);
            TR[8] = gran;                           /* and in which granule */
            TR[9] = (unsigned char)(n >> 8);
            TR[10] = (unsigned char)(n & 0xFF);
#endif
        }
        if ((v & 0xC0) == 0xC0) break;
        gran = v;
    }

    if (got) *got = copied;
    return (copied == (unsigned int)len) ? STOR_OK : STOR_ERROR;
}

/* WRITING: allocate granules, write the data, then the FAT, then the
 * directory -- IN THAT ORDER, so a file only becomes findable once its bytes
 * are already on the disk. An interrupted write leaves granules marked free
 * and nothing pointing at them, which is a leak; the other order leaves a
 * directory entry pointing at garbage, which is a corrupt file.
 *
 * THE FORMAT IS NOT INVENTED HERE. tools/mkdisk.py already writes this
 * filesystem and tools/checkdisk.py already round-trips it, so the
 * conventions are copied from the host writer rather than re-derived:
 * ngran = ceil(len/2304), the last granule's FAT byte is $C0|nsec with nsec =
 * ceil(bytes-in-that-granule/256), lastbytes is `len % 256 or 256` stored
 * BIG ENDIAN at [14..15], file type 2 (machine language), ASCII flag 0, and
 * short sectors are padded with $00.
 *
 * CAPACITY IS CHECKED BEFORE ANYTHING IS FREED. Replacing an existing file
 * frees its chain first, and if the check came after, a write that then
 * turned out not to fit would leave the in-memory FAT disagreeing with the
 * disk -- with the old file's granules marked free and its directory entry
 * still pointing at them.
 */
uint8_t plat_write_all(const char *name, const void *buf, uint16_t len)
{
    const unsigned char *src = (const unsigned char *)buf;
    char want[11];
    unsigned char dsec = 0, dent = 0, haveslot = 0;
    unsigned char oldgran = 0xFF;
    unsigned char need, freeg, oldn, g, prev, first;
    unsigned char s, e, k;
    unsigned int  pos;

    if (len == 0) return STOR_ERROR;
    if (!disk_ready()) return STOR_ERROR;

    normalise(name, want);

    /* 1. The directory: an entry with this name, or failing that the first
          free slot. Both are wanted in ONE pass -- the entry may come after
          the free slot, and replacing beats appending. */
    for (s = DIR_FIRST; s <= DIR_LAST; s++) {
        if (!read_sec(DIR_TRACK, s)) return STOR_ERROR;
        for (e = 0; e < ENT_PER_SEC; e++) {
            unsigned char *p = secbuf + (unsigned int)e * ENT_SIZE;
            if (p[0] == 0xFF || p[0] == 0x00) {
                if (!haveslot) { dsec = s; dent = e; haveslot = 1; }
                continue;
            }
            for (k = 0; k < 11 && p[k] == (unsigned char)want[k]; k++) { }
            if (k == 11) {
                dsec = s; dent = e; haveslot = 2;    /* replacing */
                oldgran = p[13];
                break;
            }
        }
        if (haveslot == 2) break;
    }
    if (!haveslot) return STOR_ERROR;                /* directory full */

    /* 2. How many granules, and are there that many? COUNT BEFORE FREEING. */
    need = (unsigned char)((len - 1) / GRAN_BYTES + 1);
    freeg = 0;
    for (g = 0; g < NUM_GRAN; g++) if (fat[g] == 0xFF) freeg++;
    oldn = 0;
    if (oldgran < NUM_GRAN) {
        unsigned char t = oldgran, guard = 0;
        while (guard++ < NUM_GRAN + 1) {
            unsigned char v = fat[t];
            oldn++;
            if ((v & 0xC0) == 0xC0) break;
            if (v >= NUM_GRAN) break;                /* a corrupt chain */
            t = v;
        }
    }
    if ((unsigned int)freeg + oldn < (unsigned int)need) return STOR_ERROR;

    /* 3. Now it is safe to give the old chain back. */
    if (oldgran < NUM_GRAN) {
        unsigned char t = oldgran, guard = 0;
        while (guard++ < NUM_GRAN + 1) {
            unsigned char v = fat[t];
            fat[t] = 0xFF;
            if ((v & 0xC0) == 0xC0 || v >= NUM_GRAN) break;
            t = v;
        }
    }

    /* 4. Allocate and chain. The last granule carries its sector count. */
    first = 0xFF;
    prev  = 0xFF;
    pos   = 0;
    g     = 0;
    for (k = 0; k < need; k++) {
        unsigned int chunk;
        while (g < NUM_GRAN && fat[g] != 0xFF) g++;
        if (g >= NUM_GRAN) return STOR_ERROR;        /* counted, cannot happen */
        if (first == 0xFF) first = g;
        if (prev != 0xFF) fat[prev] = g;
        chunk = (unsigned int)(len - pos);
        if (chunk > GRAN_BYTES) chunk = GRAN_BYTES;
        /* Claim it now so the scan above cannot hand out the same one twice. */
        fat[g] = (unsigned char)(0xC0 | (unsigned char)((chunk - 1) / SEC_SIZE + 1));
        prev = g;
        pos = (unsigned int)(pos + chunk);
        g++;
    }

    /* 5. The data. Short sectors are padded, as the host writer pads them. */
    pos = 0;
    g = first;
    while (g < NUM_GRAN) {
        unsigned char v = fat[g], nsec, trk, sec0;
        nsec = (unsigned char)(((v & 0xC0) == 0xC0) ? (v & 0x3F) : GRAN_SECS);
        gran_loc(g, &trk, &sec0);
        for (s = 0; s < nsec; s++) {
            unsigned int i;
            for (i = 0; i < SEC_SIZE; i++)
                secbuf[i] = (pos + i < len) ? src[pos + i] : 0x00;
            if (!write_sec(trk, (unsigned char)(sec0 + s))) return STOR_ERROR;
            pos = (unsigned int)(pos + SEC_SIZE);
            if (pos > len) pos = len;
        }
        if ((v & 0xC0) == 0xC0) break;
        g = v;
    }

    /* 6. The FAT, read-modify-write so whatever else the sector holds
          survives -- only the first NUM_GRAN bytes are ours. */
    if (!read_sec(DIR_TRACK, FAT_SECTOR)) return STOR_ERROR;
    for (k = 0; k < NUM_GRAN; k++) secbuf[k] = fat[k];
    if (!write_sec(DIR_TRACK, FAT_SECTOR)) return STOR_ERROR;

    /* 7. The directory entry, LAST. */
    if (!read_sec(DIR_TRACK, dsec)) return STOR_ERROR;
    {
        unsigned char *p = secbuf + (unsigned int)dent * ENT_SIZE;
        unsigned int lastb = (unsigned int)(len % SEC_SIZE);
        if (lastb == 0) lastb = SEC_SIZE;
        for (k = 0; k < ENT_SIZE; k++) p[k] = 0x00;
        for (k = 0; k < 11; k++) p[k] = (unsigned char)want[k];
        p[11] = 2;                       /* machine language */
        p[12] = 0;                       /* binary, not ASCII */
        p[13] = first;
        p[14] = (unsigned char)(lastb >> 8);
        p[15] = (unsigned char)(lastb & 0xFF);
    }
    if (!write_sec(DIR_TRACK, dsec)) return STOR_ERROR;

    return STOR_OK;
}

uint8_t plat_open(const char *name)
{
    unsigned char gran;
    unsigned int lastbytes;

    st.open = 0;
    if (!disk_ready()) return STOR_ERROR;
    gran = find_file(name, &lastbytes);
    if (gran == 0xFF) return STOR_NOTFOUND;

    st.gran = gran;
    st.sec = 0;
    st.pos = SEC_SIZE;          /* forces the first sector to be fetched */
    st.avail = 0;
    st.lastbytes = lastbytes;
    st.open = 1;
    return STOR_OK;
}

/* Pulls the next sector of the open file into secbuf. Zero at end of file. */
static unsigned char next_sector(void)
{
    unsigned char v, nsec, trk, sec;

    if (!st.open || st.gran >= NUM_GRAN) return 0;
    v = fat[st.gran];
    nsec = (unsigned char)(((v & 0xC0) == 0xC0) ? (v & 0x3F) : GRAN_SECS);

    if (st.sec >= nsec) {                       /* move to the next granule */
        if ((v & 0xC0) == 0xC0) return 0;       /* that was the last one */
        st.gran = v;
        st.sec = 0;
        if (st.gran >= NUM_GRAN) return 0;
        v = fat[st.gran];
        nsec = (unsigned char)(((v & 0xC0) == 0xC0) ? (v & 0x3F) : GRAN_SECS);
    }

    gran_loc(st.gran, &trk, &sec);
    if (!read_sec(trk, (unsigned char)(sec + st.sec))) return 0;

    /* The final sector of the final granule is short. */
    st.avail = ((v & 0xC0) == 0xC0 && st.sec == nsec - 1)
                   ? st.lastbytes : (unsigned int)SEC_SIZE;
    st.sec++;
    st.pos = 0;
    return 1;
}

uint16_t plat_read(void *buf, uint16_t len)
{
    unsigned char *dst = (unsigned char *)buf;
    unsigned int done = 0;

    if (!st.open) return 0;
    while (done < len) {
        unsigned int n;
        if (st.pos >= st.avail && !next_sector()) break;
        n = st.avail - st.pos;
        if (n > (unsigned int)(len - done)) n = (unsigned int)(len - done);
        {   unsigned int k;
            for (k = 0; k < n; k++) dst[done + k] = secbuf[st.pos + k];
        }
        st.pos = (unsigned int)(st.pos + n);
        done = (unsigned int)(done + n);
    }
    return done;
}

void plat_close(void) { st.open = 0; }
