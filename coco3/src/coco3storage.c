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
 * WRITING IS NOT HERE YET. plat_write_all returns STOR_ERROR, so SAVE reports
 * "COULD NOT SAVE." rather than pretending -- the same honesty the Falcon's
 * sound stub kept. DSKCON writes sectors happily (DCOPC = 3); what is missing
 * is granule allocation and writing the FAT and directory back.
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

static unsigned char read_sec(unsigned char trk, unsigned char sec)
{
#ifdef STOR_TRACE
    TR[0] = trk;
    TR[1] = sec;
    TR[2] = (unsigned char)(TR[2] + 1);      /* sectors attempted */
    TR[3] = 0xB1;                            /* about to call DSKCON */
#endif
    DCOPC = 2;
    DCDRV = 0;
    DCTRK = trk;
    DCSEC = sec;
    DCBPT = secbuf;
    dskcon_processSector();
#ifdef STOR_TRACE
    TR[3] = 0xB2;                            /* DSKCON returned */
    TR[4] = DCSTA;
    TR[5] = (unsigned char)(TR[5] + 1);      /* sectors completed */
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

/* WRITING IS NOT IMPLEMENTED. DSKCON writes sectors (DCOPC = 3); what is
   missing is allocating granules and writing the FAT and directory back.
   Returning STOR_ERROR makes SAVE say "COULD NOT SAVE." instead of pretending
   -- the Falcon's sound stub kept the same honesty. */
uint8_t plat_write_all(const char *name, const void *buf, uint16_t len)
{
    (void)name; (void)buf; (void)len;
    return STOR_ERROR;
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
