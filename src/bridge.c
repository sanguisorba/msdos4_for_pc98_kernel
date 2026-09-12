/* PC-98 MS-DOS BIOS adapter, new implementation. GPL-2.0-or-later.
   Hardware block/clock drivers are from the separately preserved FreeDOS tree. */
#include "portab.h"
#include "globals.h"
#define LOC_CONV BIOS_LOC_CONV
#define LOC_HMA BIOS_LOC_HMA
#include "lol.h"
#undef LOC_CONV
#undef LOC_HMA
#include "kconfig.h"

extern COUNT dsk_init(void);
extern ddt *getddt(int);
extern UWORD ASMPASCAL init_call_intr(int, iregs *);
extern COUNT ASMCFUNC FAR blk_driver(rqptr);
extern WORD ASMCFUNC FAR clk_driver(rqptr);
extern UBYTE FAR FDtype[26];
extern UBYTE BootDaua;
extern UWORD BootPartIndex;
extern VOID ASMPASCAL putch98(UWORD);
extern UWORD ASMPASCAL key98(void);
extern VOID ASMPASCAL halt98(void);
extern UWORD ASM final_dos_seg;
extern UWORD ASM bios_end_seg;
extern UWORD ASM boot_unit;
extern UWORD ASM bpb_table[26];
extern UBYTE FAR ASM disk_last_access_unit;
extern unsigned init_oem(void);
UWORD ASM memory_paras;
static struct lol private_lol;
/* Minimal FreeDOS state used by the imported disk initialization code.
 * This is private bridge storage, not the running MS-DOS list of lists. */
struct lol FAR *ASM LoL = &private_lol;
struct _KernelConfig InitKernelConfig = {
    {'C', 'O', 'N', 'F', 'I', 'G'}, 14, 0x80, 1, 0, 0, 0, 0, 0xfd, 2, 42, 0};
struct
{
    unsigned Allocated;
    char Buffer[26 * sizeof(ddt)];
} ASM Dyn;
extern UBYTE FAR ASM daua_list[26];
WORD ASM maxsecsize = 1024;
UBYTE ASM nblkdev;
BYTE ASM bootdrive;

/* Emit CR before each LF because the raw console path does not provide
 * C text-mode newline translation. Callers use LF-only message strings. */
static void say(const char *s)
{
    while (*s)
    {
        if (*s == '\n')
            putch98(13);
        putch98(*s++);
    }
}

/* Allocate DDT storage from a fixed arena; no DOS allocator is available
 * before SYSINIT. Exhaustion stops boot rather than overwriting adjacent data. */
void far *DynAlloc(char *what, unsigned num, unsigned size)
{
    unsigned bytes = num * size;
    void far *p;
    if (bytes > sizeof(Dyn.Buffer) - Dyn.Allocated)
    {
        say("DDT overflow\n");
        halt98();
    }
    p = Dyn.Buffer + Dyn.Allocated;
    Dyn.Allocated += bytes;
    return p;
}

/* Satisfy the imported IO layer callback without adding interrupt handling. */
void FAR ASMCFUNC unhandled_int_handler_iosys(void)
{
}

/* Cumulative days before each month, including a final whole-year total. */
const UWORD *is_leap_year_monthdays(UWORD y)
{
    static const UWORD t[2][13] = {
        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
        {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}};
    return t[(y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 1 : 0];
}

/* Convert a calendar date to the DOS clock epoch: 1980-01-01 is day zero. */
UWORD DaysFromYearMonthDay(UWORD y, UWORD m, UWORD d)
{
    UWORD n = 0, a;
    for (a = 1980; a < y; a++)
        n += is_leap_year_monthdays(a)[12];
    return n + is_leap_year_monthdays(y)[m - 1] + d - 1;
}

/* Boot-only scratch state. These buffers and the request packet are reused
 * serially while the loader reads the boot volume. */
static request req;
static unsigned char sec[2048];
static unsigned char fatsec[2048];
static unsigned boot_index;
static unsigned bps, spc, res, nf, spf, roots;
static unsigned long root_lba, data_lba, clusters;

/*
 * PC-98 BPB/DA-UA identifiers use the high nibble for the device/media
 * family.  Floppy families have an odd high nibble (3, 5, 7, 9, F), while
 * hard-disk families have an even high nibble (0, 2, 4, 6, 8, A, C, E).
 * Do not infer this from the DOS drive letter: PC-98 can enumerate HDD and
 * FDD volumes in either order.
 */
static int bpbid_is_floppy(UBYTE bpbid)
{
    return ((bpbid >> 4) & 1) != 0;
}

static int bpbid_is_hard_disk(UBYTE bpbid)
{
    return !bpbid_is_floppy(bpbid);
}

static void pc98_initialize_hdd_families(void)
{
    iregs regs;

    /*
     * A floppy boot does not guarantee that the PC-98 BIOS has populated the
     * HDD equipment bytes.  Initialize the two HDD BIOS families before the
     * unmodified FreeDOS drive enumeration reads 0000:055Ch and 0000:0482h.
     */
    memset(&regs, 0, sizeof(regs));
    regs.a.x = 0x0380;
    init_call_intr(0x1b, &regs); /* SASI/IDE */
    memset(&regs, 0, sizeof(regs));
    regs.a.x = 0x03a0;
    init_call_intr(0x1b, &regs); /* SCSI */
}

static void fail(const char *s)
{
    say(s);
    say("\nBoot stopped.\n");
    halt98();
}

/* Read one volume-relative sector through the enumerated FreeDOS unit.
 * Start values at or above FFFFh use the extended request-sector field. */
static WORD readsector(unsigned long lba, void far *buf)
{
    fmemset(&req, 0, sizeof(req));
    req.r_length = sizeof(req);
    req.r_unit = boot_index;
    req.r_command = C_INPUT;
    req.r_trans = buf;
    req.r_count = 1;
    if (lba < 0xffffUL)
        req.r_start = (UWORD)lba;
    else
    {
        req.r_start = 0xffff;
        req.r_huge = lba;
    }
    return blk_driver(&req);
}

/* Decode on-disk little-endian fields without requiring aligned pointers. */
static unsigned wordat(unsigned char *p)
{
    return p[0] | (unsigned)p[1] << 8;
}
static unsigned long longat(unsigned char *p)
{
    return wordat(p) | ((unsigned long)wordat(p + 2) << 16);
}

/* Fetch one FAT12/FAT16 entry, including entries split across sectors.
 * FAT12 packs two 12-bit cluster numbers into each three-byte group. */
static unsigned fatnext(unsigned cl)
{
    unsigned long pos = (clusters < 4085) ? cl + (cl >> 1) : (unsigned long)cl * 2;
    unsigned off = (unsigned)(pos % bps), v;
    if (readsector(res + pos / bps, fatsec) & S_ERROR)
        fail("FAT read error");
    v = fatsec[off];
    if (off + 1 == bps)
    {
        if (readsector(res + pos / bps + 1, fatsec) & S_ERROR)
            fail("FAT boundary error");
        v |= (unsigned)fatsec[0] << 8;
    }
    else
        v |= (unsigned)fatsec[off + 1] << 8;
    if (clusters < 4085)
    {
        if (cl & 1)
            v >>= 4;
        v &= 0xfff;
    }
    return v;
}

/* Locate MSDOS.SYS in the FAT12/FAT16 root directory and follow its chain.
 * SYSINIT expects a raw DOS image in the reserved A000h-byte load area. */
static void load_dos(void)
{
    unsigned i, j, cl = 0, count = 0, dseg = final_dos_seg;
    unsigned long size = 0, lba, remaining, total;
    ddt *d = getddt(boot_index);
    bps = d->ddt_bpb.bpb_nbyte;
    spc = d->ddt_bpb.bpb_nsector;
    res = d->ddt_bpb.bpb_nreserved;
    nf = d->ddt_bpb.bpb_nfat;
    spf = d->ddt_bpb.bpb_nfsect;
    roots = d->ddt_bpb.bpb_ndirent;
    if ((bps != 512 && bps != 1024 && bps != 2048) || !spc || !spf)
        fail("Unsupported boot_index BPB");
    root_lba = res + (unsigned long)nf * spf;
    data_lba = root_lba + ((unsigned long)roots * 32 + bps - 1) / bps;
    total = d->ddt_bpb.bpb_nsize ? d->ddt_bpb.bpb_nsize : d->ddt_bpb.bpb_huge;
    if (total <= data_lba)
        fail("Invalid data area");
    clusters = (total - data_lba) / spc;
    if (clusters >= 65525UL)
        fail("FAT32 is not supported by MS-DOS 4.0");
    for (lba = root_lba; lba < data_lba && !cl; lba++)
    {
        if (readsector(lba, sec) & S_ERROR)
            fail("Root directory read error");
        for (i = 0; i < bps; i += 32)
        {
            if (!sec[i])
                break;
            if ((sec[i + 11] & 0x18) || sec[i] == 0xe5)
                continue;
            if (!memcmp(sec + i, "MSDOS   SYS", 11))
            {
                cl = wordat(sec + i + 26);
                size = longat(sec + i + 28);
                break;
            }
        }
    }
    if (cl < 2 || size < 16 || size > 0xa000UL)
        fail("MSDOS.SYS missing or size incompatible with SYSINIT");
    /* Clear the reserved image area, then advance the destination by paragraphs. */
    fmemset(MK_FP(dseg, 0), 0, 0xa000);
    remaining = size;
    while (remaining)
    {
        if (cl < 2 || cl > clusters + 1 || ++count > clusters)
            fail("Invalid MSDOS.SYS FAT chain");
        lba = data_lba + (unsigned long)(cl - 2) * spc;
        for (j = 0; j < spc && remaining; j++)
        {
            unsigned n = remaining > bps ? bps : (unsigned)remaining;
            if (readsector(lba + j, sec) & S_ERROR)
                fail("MSDOS.SYS read error");
            fmemcpy(MK_FP(dseg, 0), sec, n);
            dseg += bps / 16;
            remaining -= n;
        }
        if (remaining)
            cl = fatnext(cl);
    }
    say("Loading MSDOS.SYS ...\n");
}

/* Called from entry.asm after the imported runtime initialization.
 * Enumerate volumes, publish their BPBs, then load DOS before SYSINIT runs. */
void bios_main(void)
{
    unsigned i;
    ddt *d;
    WORD bpb_status;
    UBYTE bpbid;
    *(UBYTE FAR *)MK_FP(0x60, 0x31) = *(UBYTE FAR *)MK_FP(0, 0x401);
    i = init_oem();
    if (i > 640)
        i = 640;
    memory_paras = i * 64;
    pc98_initialize_hdd_families();
    Dyn.Allocated = 0;
    nblkdev = dsk_init();
    blk_dev.dh_name[0] = nblkdev;
    if (!nblkdev || nblkdev > 26)
        fail("No DOS volumes");
    boot_index = 0xffff;
    for (i = 0; i < nblkdev; i++)
    {
        d = getddt(i);
        bpbid = d->ddt_driveno;
        daua_list[i] = bpbid;

        /* Keep the DDT's fixed/removable flag consistent with the PC-98 BPBID. */
        if (bpbid_is_hard_disk(bpbid))
            d->ddt_descflags |= DF_FIXED;
        else
            d->ddt_descflags &= ~DF_FIXED;

        fmemset(&req, 0, sizeof(req));
        req.r_length = sizeof(req);
        req.r_unit = i;
        req.r_command = C_BLDBPB;
        req.r_trans = sec;
        bpb_status = blk_driver(&req);
        if (bpb_status & S_ERROR)
        {
            /*
             * An installed FDD may be empty at boot.  Its default BPB remains valid
             * until MEDIA CHECK/BUILD BPB identifies the inserted medium on access.
             * An HDD BPB failure is fatal because its partition BPB is needed now.
             */
            if (bpbid_is_hard_disk(bpbid))
                fail("HDD BPB initialization failed");
        }
        /* SYSINIT receives BPB offsets in the driver data segment. */
        bpb_table[i] = FP_OFF(&d->ddt_bpb);
        if (i + 1 == LoL->BootDrive)
            boot_index = i;
    }
    if (boot_index == 0xffff)
        fail("Boot volume not enumerated");
    boot_unit = boot_index;
    bootdrive = boot_index + 1;
    load_dos();
}

/* Translate only device initialization into the MS-DOS BIOS contract.
 * Runtime block requests remain the responsibility of the FreeDOS driver. */
WORD ASMCFUNC FAR bios_disk(rqptr p)
{
    if (p->r_command == C_INIT)
    {
        p->r_nunits = nblkdev;
        p->r_endaddr = MK_FP(bios_end_seg, 0);
        p->r_bpbptr = (bpb * FAR *)bpb_table;
        return S_DONE;
    }
    return blk_driver(p);
}

void put_string(const char *s)
{
    say(s);
}

/* Normalize a real-mode far pointer while preserving its physical address. */
void FAR *adjust_far(const void FAR *p)
{
    return MK_FP(FP_SEG(p) + (FP_OFF(p) >> 4), FP_OFF(p) & 15);
}

/* CLOCK$ initialization returns the bridge residency boundary; subsequent
 * requests use the imported PC-98 clock driver unchanged. */
WORD ASMCFUNC FAR bios_clock(rqptr p)
{
    if (p->r_command == C_INIT)
    {
        p->r_nunits = 0;
        p->r_endaddr = MK_FP(bios_end_seg, 0);
        return S_DONE;
    }
    return clk_driver(p);
}

/* Diagnostic helper required by the unchanged INT DC implementation. */
void put_unsigned(unsigned value, int radix, int width)
{
    char text[17];
    unsigned n = 0;
    if (radix < 2 || radix > 16)
        radix = 10;
    do
    {
        text[n++] = "0123456789ABCDEF"[value % radix];
        value /= radix;
    } while (value);
    while (width-- > (int)n)
        putch98('0');
    while (n)
        putch98(text[--n]);
}
