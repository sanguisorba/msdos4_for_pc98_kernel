/****************************************************************/
/*                                                              */
/*                            initDISK.c                        */
/*                                                              */
/*                      Copyright (c) 2001                      */
/*                      tom ehlert                              */
/*                      All Rights Reserved                     */
/*                                                              */
/* This file is part of DOS-C.                                  */
/*                                                              */
/* DOS-C is free software; you can redistribute it and/or       */
/* modify it under the terms of the GNU General Public License  */
/* as published by the Free Software Foundation; either version */
/* 2, or (at your option) any later version.                    */
/*                                                              */
/* DOS-C is distributed in the hope that it will be useful, but */
/* WITHOUT ANY WARRANTY; without even the implied warranty of   */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See    */
/* the GNU General Public License for more details.             */
/*                                                              */
/* You should have received a copy of the GNU General Public    */
/* License along with DOS-C; see the file COPYING.  If not,     */
/* write to the Free Software Foundation, 675 Mass Ave,         */
/* Cambridge, MA 02139, USA.                                    */
/****************************************************************/

#ifndef INITDISK_HEADERS_INCLUDED
#include "portab.h"
#include "debug.h"
#include "init-mod.h"
#include "dyndata.h"
#endif

#if defined(NEC98)
  /* 0xh, 8xh ... SASI  2xh, Axh ... SCSI */
# define is_daua_hd(d)  (((d) & 0x58) == 0)
# define is_daua_sasi(d)  (((d) & 0x7c) == 0)
# define is_daua_scsi(d)  (((d) & 0x78) == 0x20)
# define is_daua_fd(d)  (((d) & 0x1c) == 0x10)
# define is_daua_2hd(d) (((d) & 0xfc) == 0x90)
# define is_daua_2dd(d) (((d) & 0xfc) == 0x70)
# define is_daua_320k(d)  (((d) & 0xfc) == 0x50)
extern UWORD FAR SasiSectorBytes[4];  /* very important FAR */
extern UWORD FAR ScsiSectorBytes[8];  /* very important FAR */
extern WORD FAR maxsecsize;           /* very important FAR */
extern UBYTE FAR FDtype[26];          /* very important FAR */
#endif

#define PARTITION_TABLES_NEC98  16
#define PARTITION_TABLES_IBMPC  4

#define PARTITION_TABLES_MAX    16

#define FLOPPY_SEC_SIZE 512u  /* common sector size */

#if defined(NEC98) && 0
#undef DebugPrintf
#define DebugPrintf(a) printf a
#endif

#if defined(NEC98) && 1
typedef UWORD  PartitionsField;
#else
typedef int  PartitionsField;
#endif

UBYTE InitDiskTransferBuffer[MAX_SEC_SIZE] BSS_INIT({0});
COUNT nUnits BSS_INIT(0);
#if defined(NEC98)
UWORD BootPartIndex BSS_INIT(0);
UBYTE DauaHDs[12] BSS_INIT({0});
UBYTE DauaSASIs[4] BSS_INIT({0});
UBYTE DauaSCSIs[8] BSS_INIT({0});
UBYTE BootDaua BSS_INIT(0);
COUNT nFDUnits BSS_INIT(0);
# if defined SUPPORT_PC80S31
UBYTE DauaFDs[12]  BSS_INIT({0});
# else
UBYTE DauaFDs[8]  BSS_INIT({0});
# endif
UBYTE Daua2HDs[4] BSS_INIT({0});
UBYTE Daua2DDs[4] BSS_INIT({0});
#endif

/*
 *    Rev 1.0   13 May 2001  tom ehlert
 * Initial revision.
 *
 * this module implements the disk scanning for DOS accesible partitions
 * the drive letter ordering is somewhat chaotic, but like MSDOS does it.
 *
 * this module expects to run with CS = INIT_TEXT, like other init_code,
 * but SS = DS = DATA = DOS_DS, unlike other init_code.
 *
 * history:
 * 1.0 extracted the disk init code from DSK.C
 *     added LBA support
 *     moved code to INIT_TEXT
 *     done the funny code segment stuff to switch between INIT_TEXT and TEXT
 *     added a couple of snity checks for partitions
 *
 ****************************************************************************
 *
 * Implementation note:
 * this module needs some interfacing to INT 13
 * how to implement them
 *    
 * a) using inline assembly 
 *        _ASM mov ax,0x1314
 *
 * b) using assembly routines in some external FLOPPY.ASM
 *
 * c) using the funny TURBO-C style
 *        _AX = 0x1314
 *
 * d) using intr(intno, &regs) method.
 *
 * why not?
 *
 * a) this is my personal favorite, combining the best aof all worlds.
 *    TURBO-C does support inline assembly, but only by using TASM,
 *    which is not free. 
 *    so - unfortunately- its excluded.
 *
 * b) keeping funny memory model in sync with external assembly
 *    routines is everything, but not fun
 *
 * c) you never know EXACT, what the compiler does, if its a bit
 *    more complicated. does
 *      _DL = drive & 0xff    
 *      _BL = driveParam.chs.Sector;
 *    destroy any other register? sure? _really_ sure?
 *    at least, it has it's surprises.
 *    and - I found a couple of optimizer induced bugs (TC 2.01)
 *      believe me.
 *    it was coded - and operational that way.
 *    but - there are many surprises waiting there. so I opted against.
 *
 *
 * d) this method is somewhat clumsy and certainly not the
 *    fastest way to do things.
 *    on the other hand, this is INIT code, executed once.
 *    and since it's the only portable method, I opted for it.
 *
 * e) and all this is my private opinion. tom ehlert.
 *
 *
 * Some thoughts about LBA vs. CHS. by Bart Oldeman 2001/Nov/11
 * Matthias Paul writes in www.freedos.org/freedos/news/technote/113.html:
 * (...) MS-DOS 7.10+, which will always access logical drives in a type
 * 05h extended partition via CHS, even if the individual logical drives
 * in there are of LBA type, or go beyond 8 Gb... (Although this workaround
 * is sometimes used in conjunction with OS/2, using a 05h partition going
 * beyond 8 Gb may cause MS-DOS 7.10 to hang or corrupt your data...) (...)
 *
 * Also at http://www.win.tue.nl/~aeb/partitions/partition_types-1.html:
 * (...) 5 DOS 3.3+ Extended Partition
 *   Supports at most 8.4 GB disks: with type 5 DOS/Windows will not use the
 *   extended BIOS call, even if it is available. (...)
 *
 * So MS-DOS 7.10+ is brain-dead in this respect, but we knew that ;-)
 * However there is one reason to use old-style CHS calls:
 * some programs intercept int 13 and do not support LBA addressing. So
 * it is worth using CHS if possible, unless the user asks us not to,
 * either by specifying a 0x0c/0x0e/0x0f partition type or enabling
 * the ForceLBA setting in the fd kernel (sys) config. This will make
 * multi-sector reads and BIOS computations more efficient, at the cost
 * of some compatibility.
 *
 * However we need to be safe, and with varying CHS at different levels
 * that might be difficult. Hence we _only_ trust the LBA values in the
 * partition tables and the heads and sectors values the BIOS gives us.
 * After all these are the values the BIOS uses to process our CHS values.
 * So unless the BIOS is buggy, using CHS on one partition and LBA on another
 * should be safe. The CHS values in the partition table are NOT trusted.
 * We print a warning if there is a mismatch with the calculated values.
 *
 * The CHS values in the boot sector are used at a higher level. The CHS
 * that DOS uses in various INT21/AH=44 IOCTL calls are converted to LBA
 * using the boot sector values and then converted back to CHS using BIOS
 * values if necessary. Internally we do LBA as much as possible.
 *
 * However if the partition extends beyond cylinder 1023 and is not labelled
 * as one of the LBA types, we can't use CHS and print a warning, using LBA
 * instead if possible, and otherwise refuse to use it.
 *
 * As for EXTENDED_LBA vs. EXTENDED, FreeDOS makes no difference. This is
 * boot time - there is no reason not to use LBA for reading partition tables,
 * and the MSDOS 7.10 behaviour is not desirable.
 *
 * Note: for floppies we need the boot sector values though and the boot sector
 * code does not use LBA addressing yet.
 *
 * Conclusion: with all this implemented, FreeDOS should be able to gracefully
 * handle and read foreign hard disks moved across computers, whether using
 * CHS or LBA, strengthening its role as a rescue environment.
 */

#define LBA_to_CHS   init_LBA_to_CHS

/*
    interesting macros - used internally only
*/

#define SCAN_PRIMARYBOOT 0x00
#define SCAN_PRIMARY     0x01
#define SCAN_EXTENDED    0x02
#define SCAN_PRIMARY2    0x03

#define FAT12_nec       0x81
#define FAT16SMALL_nec  0x91
#define FAT16LARGE_nec  0xa1
#define FAT32_nec       0xe1
#define FAT32_LBA_nec   FAT32_nec
#define FAT16_LBA_nec   FAT16LARGE_nec

#define FAT12_ibm       0x01
#define FAT16SMALL_ibm  0x04
#define EXTENDED_ibm    0x05
#define FAT16LARGE_ibm  0x06
#define FAT32_ibm       0x0b    /* FAT32 partition that ends before the 8.4  */
                                /* GB boundary                               */
#define FAT32_LBA_ibm   0x0c    /* FAT32 partition that ends after the 8.4GB */
                                /* boundary.  LBA is needed to access this.  */
#define FAT16_LBA_ibm   0x0e    /* like 0x06, but it is supposed to end past */
                                /* the 8.4GB boundary                        */
#define EXTENDED_LBA_ibm   0x0f    /* like 0x05, but it is supposed to end past */

/* Let's play it safe and do not allow partitions with clusters above  *
 * or equal to 0xff0/0xfff0/0xffffff0 to be created                    *
 * the problem with fff0-fff6 is that they might be interpreted as BAD *
 * even though the standard BAD value is ...ff7                        */

#define FAT12MAX        (FAT_MAGIC-6)
#define FAT16MAX        (FAT_MAGIC16-6)
#define FAT32MAX        (FAT_MAGIC32-6)

#define IsExtPartition_nec(parttyp) (0)
#define IsLBAPartition_nec(parttyp) (!0)

#define IsFATPart32_nec(parttyp) (((parttyp) & 0x7f) == FAT12_nec      || \
                                  ((parttyp) & 0x7f) == FAT16SMALL_nec || \
                                  ((parttyp) & 0x7f) == FAT16LARGE_nec || \
                                  ((parttyp) & 0x7f) == FAT32_nec )
#define IsFATPart16_nec(parttyp) (((parttyp) & 0x7f) == FAT12_nec      || \
                                  ((parttyp) & 0x7f) == FAT16SMALL_nec || \
                                  ((parttyp) & 0x7f) == FAT16LARGE_nec )

#define IsExtPartition_ibm(parttyp) ((parttyp) == EXTENDED_ibm  || \
                                     (parttyp) == EXTENDED_LBA_ibm )
#define IsLBAPartition_ibm(parttyp) ((parttyp) == FAT12_LBA_ibm || \
                                     (parttyp) == FAT16_LBA_ibm || \
                                     (parttyp) == FAT32_LBA_ibm)
#define IsFATPart32_ibm(parttyp)    ((parttyp) == FAT12_ibm      || \
                                     (parttyp) == FAT16SMALL_ibm || \
                                     (parttyp) == FAT16LARGE_ibm || \
                                     (parttyp) == FAT16_LBA_ibm  || \
                                     (parttyp) == FAT32_ibm      || \
                                     (parttyp) == FAT32_LBA_ibm)
#define IsFATPart16_ibm(parttyp)    ((parttyp) == FAT12_ibm      || \
                                     (parttyp) == FAT16SMALL_ibm || \
                                     (parttyp) == FAT16LARGE_ibm || \
                                     (parttyp) == FAT16_LBA_ibm)

#ifdef WITHFAT32
# define IsFATPartition_ibm(parttyp)    IsFATPart32_ibm(parttyp)
#else
# define IsFATPartition_ibm(parttyp)    IsFATPart16_ibm(parttyp)
#endif


#define MSDOS_EXT_SIGN 0x29     /* extended boot sector signature */
#define MSDOS_FAT12_SIGN "FAT12   "     /* FAT12 filesystem signature */
#define MSDOS_FAT16_SIGN "FAT16   "     /* FAT16 filesystem signature */
#define MSDOS_FAT32_SIGN "FAT32   "     /* FAT32 filesystem signature */

/* local - returned and used for BIOS interface INT 13, AH=48*/
struct _bios_LBA_disk_parameterS {
  UWORD size;
  UWORD information;
  ULONG cylinders;
  ULONG heads;
  ULONG sectors;

  ULONG totalSect;
  ULONG totalSectHigh;
  UWORD BytesPerSector;

  ULONG eddparameters;
};

/* physical characteristics of a drive */

struct DriveParamS {
  UBYTE driveno;                /* = 0x8x                           */
  UWORD descflags;
  ULONG total_sectors;
#if defined(NEC98)
  UWORD sector_size;        /* bytes per a sector (for BIOS) */
  BYTE partition_type;
#endif

  struct CHS chs;               /* for normal   INT 13 */
};
#define PARTITION_TYPE_UNKNOWN  0
#define PARTITION_TYPE_IBMPC    0x10    /* common PC MBR (fdisk) */
#define PARTITION_TYPE_NEC98    0x20    /* nec98 extended parttition */

struct PartTableEntry           /* INTERNAL representation of partition table entry */
{
  UBYTE Bootable;
  UBYTE FileSystem;
  struct CHS Begin;
  struct CHS End;
  ULONG RelSect;
  ULONG NumSect;
#if defined(NEC98)
  UWORD part_index;
  struct CHS ipl;
  UBYTE oem_name[17];
#endif
};

/*
    internal global data
*/

BOOL ExtLBAForce = FALSE;
UBYTE GlobalLBA;
UBYTE DLASort;

#if defined(NEC98)
#define init_readdasd_nec98  init_readdasd
COUNT init_readdasd_nec98(UBYTE drive)
{
  return is_daua_hd(drive) ? DF_FIXED : 0;
}
#elif defined(IBMPC)
COUNT init_readdasd(UBYTE drive)
{
  static iregs regs;

  regs.a.b.h = 0x15;
  regs.d.b.l = drive;
  init_call_intr(0x13, &regs);
  if ((regs.flags & 1) == 0)
    switch (regs.a.b.h)
    {
      case 2:
        return DF_CHANGELINE;
      case 3:
        return DF_FIXED;
    }
  return 0;
}
#else
#error need platform specific init_readdasd()
#endif

static void init_kernel_config(void)
{
  GlobalLBA = InitKernelConfig.GlobalEnableLBAsupport;
  DLASort = InitKernelConfig.DLASortByDriveNo;
}

typedef struct {
  UWORD bpb_nbyte;              /* Bytes per Sector             */
  UBYTE bpb_nsector;            /* Sectors per Allocation Unit  */
  UWORD bpb_nreserved;          /* # Reserved Sectors           */
  UBYTE bpb_nfat;               /* # FATs                       */
  UWORD bpb_ndirent;            /* # Root Directory entries     */
  UWORD bpb_nsize;              /* Size in sectors              */
  UBYTE bpb_mdesc;              /* MEDIA Descriptor Byte        */
  UWORD bpb_nfsect;             /* FAT size in sectors          */
  UWORD bpb_nsecs;              /* Sectors per track            */
  UWORD bpb_nheads;             /* Number of heads              */
} floppy_bpb;

static int Read1LBASector(struct DriveParamS *driveParam, unsigned drive, ULONG LBA_address, void * buffer);
static int Read1CHSSector(struct DriveParamS *driveParam, unsigned drive, UWORD cylinder, UBYTE head, UBYTE sector, void * buffer);

#if defined(NEC98)
#define init_getdriveparm_nec98  init_getdriveparm
STATIC floppy_bpb floppy_bpb_2hd = { 1024, 1, 1, 2, 192, 1232, 0xfe, 2, 8, 2 };

COUNT init_getdriveparm_nec98(UBYTE drive, bpb * pbpbarray)
{
  /* todo: support 2DD, 1.44M */
  UBYTE drvtype;
  floppy_bpb *fb;
  
  if (is_daua_hd(drive))
    return 5;
  /* 4 as 2HD (compatible with 8inches 2D), 2 as 2DD (640/720) and 7 as 1.44M */
  /* 2HD only, for now... */
  drvtype = 4;
  fb = &floppy_bpb_2hd;
  if (pbpbarray)
  {
    memcpy(pbpbarray, fb, sizeof(floppy_bpb));
    ((bpb *)pbpbarray)->bpb_hidden = 0;
    ((bpb *)pbpbarray)->bpb_huge = 0;
  }
  if (maxsecsize < fb->bpb_nbyte)
    maxsecsize = fb->bpb_nbyte;
  return drvtype;
}
#endif

/*
    translate LBA sectors into CHS addressing
    initially copied and pasted from dsk.c!

    LBA to/from CHS conversion - see http://www.ata-atapi.com/ How It Works section on CHSxlat - CHS Translation
    LBA (logical block address) simple 0 to N-1 used internally and with extended int 13h (BIOS)
    L-CHS (logical CHS) is the CHS view when using int 13h (BIOS)
    P-CHS (physical CHS) is the CHS view when directly accessing disk, should not, but could be used in BS or MBR

    LBA = ( (cylinder * heads_per_cylinder + heads ) * sectors_per_track ) + sector - 1

    cylinder = LBA / (heads_per_cylinder * sectors_per_track)
        temp = LBA % (heads_per_cylinder * sectors_per_track)
        head = temp / sectors_per_track
      sector = temp % sectors_per_track + 1

    where heads_per_cylinder and sectors_per_track are the current translation mode values.
    cyclinder and heads are 0 to N-1 based, sector is 1 to N based
*/

#if defined(NEC98)
#define init_LBA_to_CHS_nec98  init_LBA_to_CHS
#define printCHS_nec98  printCHS
#define CHS_to_LBA_nec98  CHS_to_LBA
void init_LBA_to_CHS_nec98(struct CHS *chs, ULONG LBA_address,
                           struct DriveParamS *driveparam)
{
  unsigned hs = driveparam->chs.Sector * driveparam->chs.Head;
  unsigned hsrem = (unsigned)(LBA_address % hs);
  
  LBA_address /= hs;

  chs->Cylinder = LBA_address >= 0x10000ul ? 0xffffu : (unsigned)LBA_address;
  chs->Head = hsrem / driveparam->chs.Sector;
  chs->Sector = hsrem % driveparam->chs.Sector;
}

/*
    translate CHS addressing into LBA sectors
*/

static ULONG CHS_to_LBA_nec98(struct CHS *chs, struct DriveParamS *driveparam)
{
  return (ULONG)chs->Cylinder * driveparam->chs.Head * driveparam->chs.Sector
         + chs->Head * driveparam->chs.Sector
         + chs->Sector;
}

void printCHS_nec98(char *title, struct CHS *chs)
{
  /* has no fixed size for head/sect: is often 1/1 in our context */
  printf("%s%4u-%u-%u", title, chs->Cylinder, chs->Head, chs->Sector);
}
#endif

/*
    reason for this modules existence:
    
    we have found a partition, and add them to the global 
    partition structure.

*/

/* Compute ceil(a/b) */
#define cdiv(a, b) (((a) + (b) - 1) / (b))

/* calculates FAT data:
   code adapted by Bart Oldeman from mkdosfs from the Linux dosfstools:
      Author:       Dave Hudson
      Updated by:   Roman Hodek
      Portions copyright 1992, 1993 Remy Card
      and 1991 Linus Torvalds
*/
/* defaults: */
#define MAXCLUSTSIZE 128
#define NSECTORFAT12 8
#define NFAT 2

VOID CalculateFATData(ddt * pddt, ULONG NumSectors, UBYTE FATType)
{
  ULONG fatdata;
#if defined(NEC98) || 1
  UWORD sec_size;
#endif

  bpb *defbpb = &pddt->ddt_defbpb;

#if defined(NEC98) || 1
  sec_size = defbpb->bpb_nbyte;
  if (sec_size != 128 && sec_size != 256 && sec_size != 512 && sec_size != 1024 && sec_size != 2048 && sec_size != 4096) {
    sec_size = FLOPPY_SEC_SIZE;
  }
  if (maxsecsize < sec_size)
    maxsecsize = sec_size;
#endif
  /* FAT related items */
  defbpb->bpb_nfat = NFAT;
  /* normal value of number of entries in root dir */
  defbpb->bpb_ndirent = 512;
  defbpb->bpb_nreserved = 1;
  /* SEC_SIZE * DIRENT_SIZE / defbpb->bpb_ndirent + defbpb->bpb_nreserved */
  fatdata = NumSectors - (DIRENT_SIZE + 1);
  if (FATType == 12)
  {
    unsigned fatdat;
    /* in DOS, FAT12 defaults to 4096kb (8 sector) - clusters. */
    defbpb->bpb_nsector = NSECTORFAT12;
    /* Force maximal fatdata=32696 sectors since with our only possible sector
       size (512 bytes) this is the maximum for 4k clusters.
       #clus*secperclus+#fats*fatlength= 4077 * 8 + 2 * 12 = 32640.
       max FAT12 size for FreeDOS = 16,728,064 bytes */
    fatdat = (unsigned)fatdata;
    if (fatdata > 32640)
      fatdat = 32640;
    /* The "+2*NSECTORFAT12" is for the reserved first two FAT entries */
    defbpb->bpb_nfsect = (UWORD)cdiv((fatdat + 2 * NSECTORFAT12) * 3UL,
                                     sec_size * 2 * NSECTORFAT12 + NFAT*3);
#if defined(DEBUG)
    /* Need to calculate number of clusters, since the unused parts of the
     * FATS and data area together could make up space for an additional,
     * not really present cluster.
     * (This is really done in fatfs.c, bpbtodpb) */
    {
      unsigned clust = (fatdat - 2 * defbpb->bpb_nfsect) / NSECTORFAT12;
      unsigned maxclust = (defbpb->bpb_nfsect * 2 * sec_size) / 3;
      if (maxclust > FAT12MAX)
        maxclust = FAT12MAX;
      printf("FAT12: #clu=%u, fatlength=%u, maxclu=%u, limit=%u\n",
             clust, defbpb->bpb_nfsect, maxclust, FAT12MAX);
      if (clust > maxclust - 2)
      {
        clust = maxclust - 2;
        printf("FAT12: too many clusters: setting to maxclu-2\n");
      }
    }
#endif
    memcpy(pddt->ddt_fstype, MSDOS_FAT12_SIGN, 8);
  }
  else
  { /* FAT16/FAT32 */
    CLUSTER fatlength, maxcl;
    unsigned long clust, maxclust;
    unsigned fatentpersec;
    unsigned divisor;

#ifdef WITHFAT32
    if (FATType == 32)
    {
      /* For FAT32, use the cluster size table described in the FAT spec:
       * http://www.microsoft.com/hwdev/download/hardware/fatgen103.pdf
       */
      unsigned sz_gb = (unsigned)(NumSectors / 2097152UL);
      unsigned char nsector = 64; /* disks greater than 32 GB, 32K cluster */
      if (sz_gb <= 32)            /* disks up to 32 GB, 16K cluster */
        nsector = 32;
      if (sz_gb <= 16)            /* disks up to 16 GB, 8K cluster */
        nsector = 16;
      if (sz_gb <= 8)             /* disks up to 8 GB, 4K cluster */
        nsector = 8;
      if (NumSectors <= 532480UL)   /* disks up to 260 MB, 0.5K cluster */
        nsector = 1;
      defbpb->bpb_nsector = nsector;
      defbpb->bpb_ndirent = 0;
      defbpb->bpb_nreserved = 0x20;
      fatdata = NumSectors - 0x20;
      fatentpersec = sec_size / 4;
      maxcl = FAT32MAX;
    }
    else
#endif
    {
      /* FAT16: start at 4 sectors per cluster */
      defbpb->bpb_nsector = 4;
      /* Force maximal fatdata=8387584 sectors (NumSectors=8387617)
         since with our only possible sectorsize (512 bytes) this is the
         maximum we can address with 64k clusters
         #clus*secperclus+#fats*fatlength=65517 * 128 + 2 * 256=8386688.
         max FAT16 size for FreeDOS = 4,293,984,256 bytes = 4GiB-983,040 */
      if (fatdata > 8386688ul)
        fatdata = 8386688ul;
      fatentpersec = sec_size / 2;
      maxcl = FAT16MAX;
    }

#if defined(NEC98)
    DebugPrintf(("%ld sectors for FAT+data, starting with %d sectors/cluster (%ubytes/sector)\n", fatdata, defbpb->bpb_nsector, sec_size));
#else
    DebugPrintf(("%ld sectors for FAT+data, starting with %d sectors/cluster\n", fatdata, defbpb->bpb_nsector));
#endif
    do
    {
      DebugPrintf(("Trying with %d sectors/cluster:\n", defbpb->bpb_nsector));
      divisor = fatentpersec * defbpb->bpb_nsector + NFAT;
      fatlength = (CLUSTER)((fatdata + (2 * defbpb->bpb_nsector + divisor - 1))/
                            divisor);
      /* Need to calculate number of clusters, since the unused parts of the
       * FATS and data area together could make up space for an additional,
       * not really present cluster. */
      clust = (fatdata - NFAT * fatlength) / defbpb->bpb_nsector;
      maxclust = fatlength * fatentpersec;
      if (maxclust > maxcl)
        maxclust = maxcl;
      DebugPrintf(("FAT: #clu=%lu, fatlen=%lu, maxclu=%lu, limit=%lu\n",
                   clust, fatlength, maxclust, maxcl));
      if (clust > maxclust - 2)
      {
        clust = 0;
        DebugPrintf(("FAT: too many clusters\n"));
      }
      else if (clust <= FAT_MAGIC)
      {
        /* The <= 4086 avoids that the filesystem will be misdetected as having a
         * 12 bit FAT. */
        DebugPrintf(("FAT: would be misdetected as FAT12\n"));
        clust = 0;
      }
      if (clust)
        break;
      defbpb->bpb_nsector <<= 1;
    }
    while (defbpb->bpb_nsector && defbpb->bpb_nsector <= MAXCLUSTSIZE);
#ifdef WITHFAT32
    if (FATType == 32)
    {
      defbpb->bpb_nfsect = 0;
      defbpb->bpb_xnfsect = fatlength;
      /* set up additional FAT32 fields */
      defbpb->bpb_xflags = 0;
      defbpb->bpb_xfsversion = 0;
      defbpb->bpb_xrootclst = 2;
      defbpb->bpb_xfsinfosec = 1;
      defbpb->bpb_xbackupsec = 6;
      memcpy(pddt->ddt_fstype, MSDOS_FAT32_SIGN, 8);
    }
    else
#endif /* WITHFAT32 */
    {
      defbpb->bpb_nfsect = (UWORD)fatlength;
      memcpy(pddt->ddt_fstype, MSDOS_FAT16_SIGN, 8);
    }
  }
  pddt->ddt_fstype[8] = '\0';
}

STATIC void push_ddt(ddt *pddt)
{
  ddt FAR *fddt = DynAlloc("ddt", 1, sizeof(ddt));
  fmemcpy(fddt, pddt, sizeof(ddt));
  if (pddt->ddt_logdriveno != 0) {
    (fddt - 1)->ddt_next = fddt;
    if (pddt->ddt_driveno == 0 && pddt->ddt_logdriveno == 1)
      (fddt - 1)->ddt_descflags |= DF_CURLOG | DF_MULTLOG;
  }
}


STATIC UBYTE PreferredFATType(CONST struct DriveParamS *driveParam, CONST struct PartTableEntry *pEntry)
{
  UBYTE fattype = 0;
  UBYTE fsys = pEntry->FileSystem;

  if (fsys == 0xff /* FAT12_LBA */) return 12;
  if (driveParam->partition_type == PARTITION_TYPE_IBMPC)
  {
    switch(fsys) {
      case FAT12_ibm:
        fattype = 12;
        break;
      case FAT16SMALL_ibm:
      case FAT16LARGE_ibm:
      case FAT16_LBA_ibm:
        fattype = 16;
        break;
#ifdef WITHFAT32
      case FAT32_ibm:
      case FAT32_LBA_ibm:
        fattype = 32;
        break;
#endif
    }
  }
  else if (driveParam->partition_type == PARTITION_TYPE_NEC98)
  {
    switch(fsys) {
      case FAT12_nec:
        fattype = 12;
        break;
      case FAT16SMALL_nec:
      case FAT16LARGE_nec:
        fattype = 16;
        break;
#ifdef WITHFAT32
      case FAT32_nec:
        fattype = 32;
        break;
#endif
    }
  }

  return fattype;
}


void DosDefinePartition(struct DriveParamS *driveParam,
                        ULONG StartSector, struct PartTableEntry *pEntry,
                        int extendedPartNo, int PrimaryNum)
{
  ddt nddt;
  ddt *pddt = &nddt;
  struct CHS chs;

#if defined(NEC98)
  UWORD phys_bytes_sector;
  /* todo */
  UNREFERENCED_PARAMETER(extendedPartNo);
  UNREFERENCED_PARAMETER(PrimaryNum);
#endif

  if (nUnits >= NDEV)
  {
    printf("more Partitions detected then possible, max = %d\n", NDEV);
    return;                     /* we are done */
  }

  pddt->ddt_next = MK_FP(0, 0xffff);
  pddt->ddt_driveno = driveParam->driveno;
  pddt->ddt_logdriveno = nUnits;
  pddt->ddt_descflags = driveParam->descflags;
  /* Turn of LBA if not forced and the partition is within 1023 cyls and of the right type */
  /* the FileSystem type was internally converted to LBA_xxxx if a non-LBA partition
     above cylinder 1023 was found */
#if defined(NEC98)
  if (!GlobalLBA)
#else
  if (!InitKernelConfig.ForceLBA && !ExtLBAForce && !IsLBAPartition(pEntry->FileSystem))
#endif
    pddt->ddt_descflags &= ~DF_LBA;
  pddt->ddt_ncyl = driveParam->chs.Cylinder;

  DebugPrintf(("LBA %senabled for drive %c:\n", (pddt->ddt_descflags & DF_LBA)?"":"not ", 'A' + nUnits));

  pddt->ddt_offset = StartSector;

#if defined(NEC98)
  phys_bytes_sector = 0;
  if (is_daua_sasi(driveParam->driveno)) /* SASI(IDE) #1...#4 */
    phys_bytes_sector = SasiSectorBytes[driveParam->driveno & 0x3];
  if (is_daua_scsi(driveParam->driveno)) /* SCSI #0...#6 */
    phys_bytes_sector = ScsiSectorBytes[driveParam->driveno & 0x7];
  {
  /* get actual sector size (for block device, not always same for BIOS) from the disk */
    UWORD rc = GlobalLBA ? Read1LBASector(driveParam, driveParam->driveno, pEntry->RelSect, InitDiskTransferBuffer)
                         : Read1CHSSector(driveParam, driveParam->driveno, pEntry->Begin.Cylinder, pEntry->Begin.Head, pEntry->Begin.Sector, InitDiskTransferBuffer);
    if (rc == 0) rc = *((UWORD FAR *)&InitDiskTransferBuffer[BT_BPB]);
    pddt->ddt_defbpb.bpb_nbyte = (rc == 256 || rc == 512 || rc == 1024 || rc == 2048 || rc == 4096) ? rc : 1024; /* todo: set proper value for BIOS in default */ 
  }

#else
  /* IBMPC */
  pddt->ddt_defbpb.bpb_nbyte = FLOPPY_SEC_SIZE;
#endif
  pddt->ddt_defbpb.bpb_mdesc = 0xf8;
  pddt->ddt_defbpb.bpb_nheads = driveParam->chs.Head;
  pddt->ddt_defbpb.bpb_nsecs = driveParam->chs.Sector;
# if 0 && defined(NEC98)
  if (InitDiskTransferBuffer[0x26] == 0x29)
    pddt->ddt_defbpb.bpb_hidden = *((ULONG FAR *)&InitDiskTransferBuffer[0x1c]);
  else
# endif
  pddt->ddt_defbpb.bpb_hidden = pEntry->RelSect;

  pddt->ddt_defbpb.bpb_nsize = 0;
  pddt->ddt_defbpb.bpb_huge = pEntry->NumSect;
  if (pEntry->NumSect <= 0xffff)
  {
    pddt->ddt_defbpb.bpb_nsize = (UWORD) (pEntry->NumSect);
    pddt->ddt_defbpb.bpb_huge = 0;  /* may still be set on Win95 */
  }

  /* sectors per cluster, sectors per FAT etc. */
#if 1
  CalculateFATData(pddt, pEntry->NumSect, PreferredFATType(driveParam, pEntry));
#else
  CalculateFATData(pddt, pEntry->NumSect, pEntry->FileSystem);
#endif

  pddt->ddt_serialno = 0x12345678l;
  /* drive inaccessible until bldbpb successful */
  pddt->ddt_descflags |= init_readdasd(pddt->ddt_driveno) | DF_NOACCESS;
  pddt->ddt_type = 5;
  memcpy(&pddt->ddt_bpb, &pddt->ddt_defbpb, sizeof(bpb));

  push_ddt(pddt);

  /* Alain whishes to keep this in later versions, too 
     Tom likes this too, so he made it configurable by SYS CONFIG ...
   */

  if (InitKernelConfig.InitDiskShowDriveAssignment)
  {
#if defined(NEC98)
/* NEC PC-98x1 (pc98) */
    LBA_to_CHS(&chs, StartSector, driveParam);
    printf("\r%c: ", 'A' + nUnits);
    if ((driveParam->driveno & 0x70) == 0)
      printf("SASI%u:%3u", (driveParam->driveno & 0xf) + 1, phys_bytes_sector);
    else if ((driveParam->driveno & 0x70) == 0x20)
      printf("SCSI%u:%3u", (driveParam->driveno & 0xf), phys_bytes_sector);
    else
      printf("DA:%02X    ", driveParam->driveno);

    switch(driveParam->partition_type) {
      case PARTITION_TYPE_NEC98:
        printf(" [%-16s]", pEntry->oem_name);
        break;
      case PARTITION_TYPE_IBMPC: {
        char *ExtPri;
        int num;

        LBA_to_CHS(&chs, StartSector, driveParam);

        ExtPri = "*Pri";
        num = PrimaryNum + 1;
        if (extendedPartNo)
        {
          ExtPri = "*Ext";
          num = extendedPartNo;
        }
        printf(" %s[%2d]:%02x", ExtPri, num, pEntry->FileSystem);
        break;
      }
    }
# if 0
    printf("\r%c: HD%d[%-16s]", 'A' + nUnits,
            (driveParam->driveno & 0x7f) + 1, pEntry->oem_name);
# endif

#else
/* IBMPC fdisk (msdos) */
    char *ExtPri;
    int num;

    LBA_to_CHS(&chs, StartSector, driveParam);

    ExtPri = "Pri";
    num = PrimaryNum + 1;
    if (extendedPartNo)
    {
      ExtPri = "Ext";
      num = extendedPartNo;
    }
    printf("\r%c: HD%d, %s[%2d]", 'A' + nUnits,
           (driveParam->driveno & 0x7f) + 1, ExtPri, num);
#endif

    printCHS(", CHS= ", &chs);

#if defined(NEC98)
    if (phys_bytes_sector >= 1024)
    {
      ULONG scale = phys_bytes_sector / 1024;
      printf(", start=%6lu MB, size=%6lu MB",
             (StartSector / 1024U) * scale, (pEntry->NumSect / 1024U) * scale);
    }
    else if (phys_bytes_sector > 0)
    {
      ULONG scale = 1024UL * 1024U / phys_bytes_sector;
      printf(", start=%6lu MB, size=%6lu MB",
             StartSector / scale, pEntry->NumSect / scale);
    }
    printf("\n");
#else
  /* IBMPC */
    printf(", start=%6lu MB, size=%6lu MB\n",
           StartSector / 2048, pEntry->NumSect / 2048);
#endif
  }
#if defined(NEC98)
  /* store DA/UA list in internal work area */
  if (nUnits < 16)
    pokeb(0x60, 0x006c + nUnits, driveParam->driveno);
  if (nUnits < 26)
  {
    UBYTE part_flag = 0;
    if ((pEntry->FileSystem & 0x7f) == 0x21)
      part_flag |= 2; /* DOS 5+ (32bit sector_count) */
    pokeb(0x60, 0x2c86 + nUnits*2, part_flag);
    pokeb(0x60, 0x2c87 + nUnits*2, driveParam->driveno);
  }
  if (is_daua_hd(BootDaua) && driveParam->driveno == BootDaua && pEntry->part_index == BootPartIndex)
  {
# if 0
    printf("\nBootUnit = %02x, part_index=%d, BootPartIndex=0x%x, logdrive=%d, nUnits=%d\n", pddt->ddt_driveno, pEntry->part_index, BootPartIndex, pddt->ddt_logdriveno, nUnits);
# endif
    LoL->BootDrive = pddt->ddt_logdriveno + 1;
  }
#endif


  nUnits++;
}

#if defined(NEC98)
#define LBA_Get_Drive_Parameters_nec98  LBA_Get_Drive_Parameters
/* Get the parameters of the hard disk */
STATIC int LBA_Get_Drive_Parameters_nec98(int drive, struct DriveParamS *driveParam)
{
  iregs regs;

  ExtLBAForce = TRUE;
  memset(driveParam, 0, sizeof *driveParam);
  if (drive < 0 || drive >= sizeof DauaHDs / sizeof DauaHDs[0])
    goto ErrorReturn;
  drive = DauaHDs[drive];
  if (!drive)
    goto ErrorReturn;
  
  /* wake up SASI(IDE) drives... */
  if (is_daua_sasi(drive))
  {
    regs.a.b.h = 0x8e;
    regs.a.b.l = drive;
    init_call_intr(0x1b, &regs);
  }

  regs.a.b.h = 0x84;
  regs.a.b.l = drive;
  regs.b.x = 256;
  regs.d.x = 0;
  init_call_intr(0x1b, &regs);

  if ((regs.flags & 0x01) || regs.d.x == 0)
    goto ErrorReturn;

  driveParam->chs.Head = regs.d.b.h;
  driveParam->chs.Sector = regs.d.b.l;
  driveParam->chs.Cylinder = regs.c.x;
  driveParam->sector_size = regs.b.x;
  if (is_daua_sasi(drive))
  {
    if (InitKernelConfig.ForceLBA)
      driveParam->descflags = DF_LBA;
    SasiSectorBytes[drive & 3] = driveParam->sector_size;
  }
  else if (is_daua_scsi(drive))
  {
    if (InitKernelConfig.ForceLBA)
      driveParam->descflags = DF_LBA;
    ScsiSectorBytes[drive & 7] = driveParam->sector_size;
  }

  if (maxsecsize < driveParam->sector_size)
    maxsecsize = driveParam->sector_size;

#if !defined(NEC98)
  if (!(driveParam->descflags & DF_LBA))
#endif
  {
    driveParam->total_sectors =
        (ULONG)driveParam->chs.Cylinder
        * driveParam->chs.Head * driveParam->chs.Sector;
  }

  driveParam->driveno = drive;

  DebugPrintf(("drive %02Xh total: C = %u, H = %u, S = %u,",
               drive,
               driveParam->chs.Cylinder,
               driveParam->chs.Head, driveParam->chs.Sector));
#if defined(NEC98)
  DebugPrintf((" sct=%ubytes(bios)", driveParam->sector_size));
  DebugPrintf((" total size %luMB\n\n", driveParam->total_sectors / 1024L * driveParam->sector_size / 1024L));
#else
  DebugPrintf((" total size %luMB\n\n", driveParam->total_sectors / (1014L * 1024L / FLOPPY_SEC_SIZE) * (regs.b.x / (unsigned)(FLOPPY_SEC_SIZE)));
#endif

ErrorReturn:

  return driveParam->driveno;
}
#endif

/*
    converts physical into logical representation of partition entry
*/

STATIC void ConvCHSToIntern_nec98(struct CHS *chs, UBYTE * pDisk)
{
  chs->Sector = pDisk[0];
  chs->Head = pDisk[1];
  chs->Cylinder = *(UWORD *) (pDisk + 2);
}

STATIC void ConvCHSToIntern_ibm(struct CHS *chs, UBYTE * pDisk)
{
  chs->Head = pDisk[0];
  chs->Sector = pDisk[1] & 0x3f;
  chs->Cylinder = pDisk[2] + ((pDisk[1] & 0xc0) << 2);
}


BOOL ConvPartTableEntryToIntern_nec98(struct PartTableEntry * pEntry,
                                      UBYTE * pDisk,
                                      struct DriveParamS * driveParam)
{
  int i;

  for (i = 0; i < PARTITION_TABLES_NEC98; i++, pDisk += 32, pEntry++)
  {
    ULONG BeginSect;
    ULONG EndSect;

    pEntry->part_index = i;
    pEntry->Bootable = pDisk[0];
    pEntry->FileSystem = pDisk[1];

    ConvCHSToIntern_nec98(&pEntry->ipl, pDisk + 4);
    ConvCHSToIntern_nec98(&pEntry->Begin, pDisk + 8);
    ConvCHSToIntern_nec98(&pEntry->End, pDisk + 12);

    memcpy(pEntry->oem_name, pDisk + 16, 16);
    pEntry->oem_name[16] = '\0';

    BeginSect = CHS_to_LBA(&pEntry->Begin, driveParam);
    EndSect = CHS_to_LBA(&pEntry->End, driveParam);
    pEntry->RelSect = BeginSect;
    pEntry->NumSect = EndSect - BeginSect + 1;
  }
  return TRUE;
}

BOOL ConvPartTableEntryToIntern_ibm(struct PartTableEntry * pEntry,
                                    UBYTE * pDisk,
                                    struct DriveParamS * driveParam)
{
  int i;

  UNREFERENCED_PARAMETER(driveParam);
  if (pDisk[0x1fe] != 0x55 || pDisk[0x1ff] != 0xaa)
  {
/*
    memset(pEntry, 0, 4 * sizeof(struct PartTableEntry));
*/
    return FALSE;
  }

  pDisk += 0x1be;

  for (i = 0; i < PARTITION_TABLES_IBMPC; i++, pDisk += 16, pEntry++)
  {

    pEntry->Bootable = pDisk[0];
    pEntry->FileSystem = pDisk[4];

    ConvCHSToIntern_ibm(&pEntry->Begin, pDisk+1);
    ConvCHSToIntern_ibm(&pEntry->End, pDisk+5);

    pEntry->RelSect = *(ULONG *) (pDisk + 8);
    pEntry->NumSect = *(ULONG *) (pDisk + 12);
  }
  return TRUE;
}

BOOL ConvPartTableEntryToIntern(struct PartTableEntry * pEntry,
                                UBYTE * pDisk,
                                struct DriveParamS * driveParam)
{
  switch (driveParam->partition_type) {
    case PARTITION_TYPE_NEC98:
      return ConvPartTableEntryToIntern_nec98(pEntry, pDisk, driveParam);
    case PARTITION_TYPE_IBMPC:
      return ConvPartTableEntryToIntern_ibm(pEntry, pDisk, driveParam);
  }
  return FALSE;
}

#if defined(NEC98)
#define is_suspect_nec98  is_suspect
BOOL is_suspect_nec98(struct CHS *chs, struct CHS *pEntry_chs)
{
  return !((pEntry_chs->Cylinder == chs->Cylinder &&
            pEntry_chs->Head     == chs->Head     &&
            pEntry_chs->Sector   == chs->Sector)        ||
            (ULONG)chs->Cylinder > LBA_CYLINDER_MAX &&
           (pEntry_chs->Cylinder == (UWORD)LBA_CYLINDER_MAX ||
            pEntry_chs->Cylinder == (/* (UWORD)LBA_CYLINDER_MAX & */ chs->Cylinder)));
}
#endif

void print_warning_suspect(char *partitionName, UBYTE fs, struct CHS *chs,
                           struct CHS *pEntry_chs)
{
  if (!InitKernelConfig.ForceLBA)
  {
    printf("WARNING: using suspect partition %s FS %02x:", partitionName, fs);
    printCHS(" with calculated values ", chs);
    printCHS(" instead of ", pEntry_chs);
    printf("\n");
  }
  memcpy(pEntry_chs, chs, sizeof(struct CHS));
}

PartitionsField ScanForPrimaryPartitions(struct DriveParamS * driveParam,
                         int scan_type,
                         struct PartTableEntry * pEntry, ULONG startSector,
                         PartitionsField partitionsToIgnore, int extendedPartNo)
{
/* todo: partitionsToIgnore - need ULONG for portablity, probably */
  int partition_max = PARTITION_TABLES_NEC98;
  int i;
  UBYTE ptype;
  struct CHS chs, end;
  ULONG partitionStart;
#if defined(NEC98)
  char partitionName[24];
#else
  char partitionName[12];
#endif

  ptype = driveParam->partition_type;
  if (ptype == PARTITION_TYPE_IBMPC) partition_max = PARTITION_TABLES_IBMPC;
  for (i = 0; i < partition_max; i++, pEntry++)
  {
    if (ptype == PARTITION_TYPE_NEC98)
    {
#if defined(WITHFAT32)
      if (pEntry->FileSystem != 0x81 && pEntry->FileSystem != 0x91 && pEntry->FileSystem != 0xa1 && pEntry->FileSystem != 0xe1)	/* 81h or 91h; MS-DOS Active Partition (pc98) */
        continue;
#else
      if (pEntry->FileSystem != 0x81 && pEntry->FileSystem != 0x91 && pEntry->FileSystem != 0xa1)	/* 81h or 91h; MS-DOS Active Partition (pc98) */
        continue;
#endif
    }
    else
    {
      if (pEntry->FileSystem == 0)
        continue;
    }

    if (partitionsToIgnore & (1 << i))
      continue;

    if (ptype == PARTITION_TYPE_NEC98)
    {
      partitionStart = pEntry->RelSect;
    }
    else
    {
/* IBMPC fdisk (msdos) */
#if 1
      if (pEntry->FileSystem == EXTENDED_ibm || pEntry->FileSystem == EXTENDED_LBA_ibm)
#else
      if (IsExtPartition_ibm(pEntry->FileSystem))
#endif
        continue;

      if (scan_type == SCAN_PRIMARYBOOT && !pEntry->Bootable)
        continue;

      partitionStart = startSector + pEntry->RelSect;

      if (!IsFATPartition_ibm(pEntry->FileSystem))
      {
        continue;
      }

      if (extendedPartNo)
        sprintf(partitionName, "Ext:%d", extendedPartNo);
      else
        sprintf(partitionName, "Pri:%d", i + 1);
    }

    /*
       some sanity checks, that partition
       structure is OK
     */
    LBA_to_CHS(&chs, partitionStart, driveParam);
    LBA_to_CHS(&end, partitionStart + pEntry->NumSect - 1, driveParam);

    /* some FDISKs enter for partitions 
       > 8 GB cyl = 1023, other (cyl&1023)
     */

#if 0
    if (is_suspect(&chs, &pEntry->Begin))
    {
      print_warning_suspect(partitionName, pEntry->FileSystem, &chs,
                            &pEntry->Begin);
    }

    if (is_suspect(&end, &pEntry->End))
    {
      if (pEntry->NumSect == 0)
      {
        printf("Not using partition %s with 0 sectors\n", partitionName);
        continue;
      }
      print_warning_suspect(partitionName, pEntry->FileSystem, &end,
                            &pEntry->End);
    }
#endif

#if (LBA_CYLINDER_MAX >= 0xfffful) /* defined(NEC98) */
    if ((ULONG)chs.Cylinder > LBA_CYLINDER_MAX || (ULONG)end.Cylinder > LBA_CYLINDER_MAX)
#else
/* IBMPC fdisk (msdos) */
    if (chs.Cylinder > (UWORD)LBA_CYLINDER_MAX || end.Cylinder > (UWORD)LBA_CYLINDER_MAX)
#endif
    {

      if (!(driveParam->descflags & DF_LBA))
      {
        printf
            ("can't use LBA partition without LBA support - part %s FS %02x",
             partitionName, pEntry->FileSystem);

        printCHS(" start ", &chs);
        printCHS(", end ", &end);
        printf("\n");

        continue;
      }

      if (!InitKernelConfig.ForceLBA && !ExtLBAForce 
# if !defined(NEC98)
          && !IsLBAPartition(pEntry->FileSystem))
# else
          )
# endif
      {
        printf
            ("WARNING: Partition ID does not suggest LBA - part %s FS %02x.\n"
             "Please run FDISK to correct this - using LBA to access partition.\n",
             partitionName, pEntry->FileSystem);

        printCHS(" start ", &chs);
        printCHS(", end ", &end);
        printf("\n");
#if defined(NEC98)
#else
/* IBMPC fdisk (msdos) */
        pEntry->FileSystem = (pEntry->FileSystem == FAT12 ? FAT12_LBA :
                              pEntry->FileSystem == FAT32 ? FAT32_LBA :
                              /*  pEntry->FileSystem == FAT16 ? */
                              FAT16_LBA);
#endif
      }

      /* else its a diagnostic message only */
#ifdef DEBUG
      printf("found and using LBA partition %s FS %02x",
             partitionName, pEntry->FileSystem);
      printCHS(" start ", &chs);
      printCHS(", end ", &end);
      printf("\n");
#endif
    }

    /*
       here we have a partition table in our hand !!
     */

    partitionsToIgnore |= 1 << i;

    DosDefinePartition(driveParam, partitionStart, pEntry,
                       extendedPartNo, i);

    if (scan_type == SCAN_PRIMARYBOOT || scan_type == SCAN_PRIMARY)
    {
      return partitionsToIgnore;
    }
  }

  return partitionsToIgnore;
}

void BIOS_drive_reset(unsigned drive);

#if defined(NEC98)
static int Read1Sector(UBYTE driveno, UWORD param0, UWORD param1, void *buffer, UWORD read_bytes)
{
  iregs regs;
  int num_retries;

  for (num_retries = 0; num_retries < N_RETRY; num_retries++)
  {
    regs.a.b.h = 0x06;
    regs.a.b.l = driveno;
    regs.d.x = param1;
    regs.c.x = param0;
    regs.b.x = read_bytes;
    regs.bp = FP_OFF(buffer);
    regs.es = FP_SEG(buffer);

    init_call_intr(0x1b, &regs);

    if ((regs.flags & FLG_CARRY) == 0)
      break;
    BIOS_drive_reset(driveno);
  }
  return regs.flags & FLG_CARRY ? 1 : 0;
}

static int Read1LBASector(struct DriveParamS *driveParam, unsigned drive,
                         ULONG LBA_address, void * buffer)
{
  /* todo: support FD */
  UBYTE driveno = driveParam->driveno;
  if (is_daua_hd(drive)) driveno &= 0x7f;
  return Read1Sector(driveno, (UWORD)LBA_address, (UWORD)(LBA_address >> 16), buffer, 1024);
}

static int Read1CHSSector(struct DriveParamS *driveParam, unsigned drive,
                          UWORD cylinder, UBYTE head, UBYTE sector, void * buffer)
{
  /* todo: support FD */
  UNREFERENCED_PARAMETER(drive);
  return Read1Sector(driveParam->driveno, cylinder, ((UWORD)head << 8) | sector, buffer, 1024);
}
#else
#error need platform specific Read1LBASector()
#endif

/* Load the Partition Tables and get information on all drives */
PartitionsField ProcessDisk(int scanType, unsigned drive, PartitionsField PartitionsToIgnore)
{

  struct PartTableEntry PTable[PARTITION_TABLES_MAX];
  ULONG RelSectorOffset;
  ULONG PartitionTableOffset;
  UWORD ptoffset;
  ULONG ExtendedPartitionOffset;
  int iPart;
  int strangeHardwareLoop;

  int num_extended_found = 0;

  struct DriveParamS driveParam;

  /* Get the hard drive parameters and ensure that the drive exists. */
  /* If there was an error accessing the drive, skip that drive. */

  if (!LBA_Get_Drive_Parameters(drive, &driveParam))
  {
    printf("can't get drive parameters for drive %02x\n", drive);
    return PartitionsToIgnore;
  }

  driveParam.partition_type = PARTITION_TYPE_UNKNOWN;
  RelSectorOffset = 0;          /* boot sector */
  ptoffset = 0;      /* partition table (+1=NEC98) */
  ExtendedPartitionOffset = 0;  /* not found yet */
  iPart = 0;
#if defined(NEC98)
  if (Read1LBASector(&driveParam, drive, 0, InitDiskTransferBuffer) != 0)
  {
    printf("Error reading MBR on drive %02Xh", drive);
    return PartitionsToIgnore;
  }

  if (*(UWORD FAR *)&(InitDiskTransferBuffer[0xfe]) == 0xaa55U
      || fmemcmp(InitDiskTransferBuffer, "\xeb\x09\x00\x00\x49\x50\x4c\x31\x00\x00\x00", 11) == 0 /* FreeBSD(98) boot selector */
# if 0
      /* to some paranoids' relief... */
      && (driveParam.sector_size == 256 || (driveParam.sector_size > 256 && *(UWORD FAR *)&(InitDiskTransferBuffer[0x1fe]) == 0xaa55U))
# endif
     )
  {
    /* Extended Partition (DOS 3+) */
    driveParam.partition_type = PARTITION_TYPE_NEC98;
    ptoffset = 1;
  }
  else
  {
    /* todo: support NEC PC-98 'standard' style (DOS 2.x, 256bytes per physical sector) */
    if (InitKernelConfig.GlobalEnableLBAsupport && driveParam.sector_size == 512 && *(UWORD FAR *)&(InitDiskTransferBuffer[0x1fe]) == 0xaa55U)
      driveParam.partition_type = PARTITION_TYPE_IBMPC;
    else
      return PartitionsToIgnore;
  }
#endif

  /* Read the Primary Partition Table. */

ReadNextPartitionTable:

  strangeHardwareLoop = 0;
strange_restart:

  PartitionTableOffset = RelSectorOffset + ptoffset;
  if (Read1LBASector
      (&driveParam, drive, PartitionTableOffset, InitDiskTransferBuffer))
  {
    printf("Error reading partition table drive %02Xh sector %lu", drive,
           PartitionTableOffset);
    return PartitionsToIgnore;
  }

#if defined(NEC98)
  if (!ConvPartTableEntryToIntern(PTable, InitDiskTransferBuffer, &driveParam))
#else
  if (!ConvPartTableEntryToIntern(PTable, InitDiskTransferBuffer))
#endif
  {
    /* there is some strange hardware out in the world,
       which returns OK on first read, but the data are
       rubbish. simply retrying works fine.
       there is no logic behind this, but it works TE */

    if (++strangeHardwareLoop < 3)
      goto strange_restart;

    printf("illegal partition table - drive %02x sector %lu\n", drive,
           PartitionTableOffset);
    return PartitionsToIgnore;
  }

  if (scanType == SCAN_PRIMARYBOOT ||
      scanType == SCAN_PRIMARY ||
      scanType == SCAN_PRIMARY2 || num_extended_found != 0)
  {

    PartitionsToIgnore = ScanForPrimaryPartitions(&driveParam, scanType,
                                                  PTable, RelSectorOffset,
                                                  PartitionsToIgnore,
                                                  num_extended_found);
  }

  if (scanType != SCAN_EXTENDED)
  {
    return PartitionsToIgnore;
  }

  /* scan for extended partitions now */
  PartitionsToIgnore = 0;

/* IBMPC fdisk (msdos) */
  if (driveParam.partition_type == PARTITION_TYPE_IBMPC) for (iPart = 0; iPart < PARTITION_TABLES_IBMPC; iPart++)
  {
    if (IsExtPartition_ibm(PTable[iPart].FileSystem))
    {
      RelSectorOffset = ExtendedPartitionOffset + PTable[iPart].RelSect;

      if (ExtendedPartitionOffset == 0)
      {
        ExtendedPartitionOffset = PTable[iPart].RelSect;
        /* grand parent LBA -> all children and grandchildren LBA */
        ExtLBAForce = TRUE; /* (PTable[iPart].FileSystem == EXTENDED_LBA_ibm); */
      }

      num_extended_found++;

      if (num_extended_found > 30)
      {
        printf("found more then 30 extended partitions, terminated\n");
        return 0;
      }

      goto ReadNextPartitionTable;
    }
  }

  return PartitionsToIgnore;
}

#if defined(NEC98)
#define BIOS_nrdrives_nec98  BIOS_nrdrives
#define BIOS_nfdrives_nec98 BIOS_nfdrives

STATIC int BIOS_fdtype_nec98(UBYTE daua)
{
  iregs r;
  UBYTE da, ua;
  if (!is_daua_fd(daua))
    return 0;
  
  da = daua & 0xf0;
  ua = daua & 0x0f;
  
  if (da == 0x50) /* 320K(PC-80S31K) */
    return 1;
  
  /* check 1.44M 3mode at first */
  if (da == 0x30 || da == 0x90)
  {
    r.AH = 0xc4;
    r.AL = 0x30 | ua;
    init_call_intr(0x1b, &r);
    if ((r.AH & 0xf0) != 0x40 /* (r.flags & FLG_CARRY) == 0 */)
    {
      if (r.AH & 4)
        return 3;       /* 1.44M supported (3mode, probably) */
    }
  }
  /* check 2HD/2DD 2mode */
  r.AH = 0x84;
  r.AL = daua;
  if (da == 0x90 || da == 0x30)
    r.AL = 0x10 | ua;
  if (da == 0x70)
    r.AL = 0xf0 | ua;
  init_call_intr(0x1b, &r);
  if ((r.AH & 0xf0) != 0x40 /* (r.flags & FLG_CARRY) == 0 */)
  {
    if (r.AH & 8)
      return 2;       /* dual-mode */
  }
  
  return 1;
}

#ifdef SUPPORT_PC80S31
static int BIOS_n320kdrives_nec98(int units, UWORD equip)
{
  int i;
  
  for(i=0; i<4; ++i)
  {
    if (equip & (0x10U << i))
      DauaFDs[units++] = 0x50 + i;
  }
  
  return units;
}
#endif

int BIOS_nfdrives_nec98(void)
{
  UWORD equip;
  int units;
  int i, n;
  UBYTE *p1, *p2;
  
  equip = peekw(0, 0x55c); /* DISK_EQUIP */
  units = 0;
#ifdef SUPPORT_PC80S31
  if (is_daua_320k(BootDaua))
    units = BIOS_n320kdrives_nec98(units, equip);
#endif
  for(i=0, n=0; i<4; ++i)
  {
    if (equip & (1U << i))
      Daua2HDs[n++] = 0x90 + i;
  }
  for(i=0, n=0; i<4; ++i)
  {
    if (equip & (0x1000U << i))
      Daua2DDs[n++] = 0x70 + i;
  }
  p1 = Daua2HDs;
  p2 = Daua2DDs;
  if (is_daua_2dd(BootDaua))
  {
    p1 = Daua2DDs;
    p2 = Daua2HDs;
  }
  for(i=0; i<4; ++i)
    if (p1[i] != 0)
      DauaFDs[units++] = p1[i];
  for(i=0; i<4; ++i)
    if (p2[i] != 0)
      DauaFDs[units++] = p2[i];
#ifdef SUPPORT_PC80S31
  if (!is_daua_320k(BootDaua))
    units = BIOS_n320kdrives_nec98(units, equip);
#endif
  
  return units;
}

int BIOS_nrdrives_nec98(void)
{
  UWORD equip;
  UBYTE equips;
  int units, units_all;
  int i;
  
  equip = peekw(0, 0x55c); /* DISK_EQUIP */
  units_all = 0;
  for(i=0, units = 0; i<4; ++i)
  {
    if (equip & (0x0100U << i))
      DauaHDs[units_all++] = DauaSASIs[units++] = 0x80 + i;
  }
  equips = peekb(0, 0x482); /* DISK_EQUIPS */
  for(i=0, units = 0; i<8; ++i)
  {
    if (equips & (1U << i))
      DauaHDs[units_all++] = DauaSCSIs[units++] = 0xA0 + i;
  }
  
  return units_all;
}
#elif defined(IBMPC)
int BIOS_nrdrives(void)
{
  iregs regs;

  regs.a.b.h = 0x08;
  regs.d.b.l = 0x80;
  init_call_intr(0x13, &regs);

  if (regs.flags & 1)
  {
    printf("no hard disks detected\n");
    return 0;
  }

  return regs.d.b.l;
}
#else
#error need platform specific BIOS_nrdrives()
#endif

#if defined(NEC98)
#define BIOS_drive_reset_nec98  BIOS_drive_reset
void BIOS_drive_reset_nec98(unsigned drive)
{
  UNREFERENCED_PARAMETER(drive);
}
#elif defined(IBMPC)
void BIOS_drive_reset(unsigned drive)
{
  iregs regs;

  regs.d.b.l = drive | 0x80;
  regs.a.b.h = 0;

  init_call_intr(0x13, &regs);
}
#else
#error need platform specific BIOS_drive_reset()
#endif

/* 
    thats what MSDN says:

    How Windows 2000 Assigns, Reserves, and Stores Drive Letters
    ID: q234048 
 
  BASIC Disk - Drive Letter Assignment Rules
The following are the basic disk drive letter assignment rules for Windows 2000: 
Scan all fixed hard disks as they are enumerated, assign drive letters 
starting with any active primary partitions (if there is one), otherwise,
scan the first primary partition on each drive. Assign next available 
letter starting with C:

Repeat scan for all fixed hard disks and removable (JAZ, MO) disks 
and assign drive letters to all logical drives in an extended partition, 
or the removable disk(s) as enumerated. Assign next available letter 
starting with C: 

Finally, repeat scan for all fixed hard disk drives, and assign drive 
letters to all remaining primary partitions. Assign next available letter 
starting with C:

Floppy drives. Assign letter starting with A:

CD-ROM drives. Assign next available letter starting with D:

*************************************************************************
Order in Which MS-DOS and Windows Assign Drive Letters
ID: q51978 
 
MORE INFORMATION
The following occurs at startup: 

MS-DOS checks all installed disk devices, assigning the drive letter A 
to the first physical floppy disk drive that is found.

If a second physical floppy disk drive is present, it is assigned drive letter B. If it is not present, a logical drive B is created that uses the first physical floppy disk drive.

Regardless of whether a second floppy disk drive is present, 
MS-DOS then assigns the drive letter C to the primary MS-DOS 
partition on the first physical hard disk, and then goes on 
to check for a second hard disk. 

If a second physical hard disk is found, and a primary partition exists 
on the second physical drive, the primary MS-DOS partition on the second
physical hard drive is assigned the letter D. MS-DOS version 5.0, which 
supports up to eight physical drives, will continue to search for more 
physical hard disk drives at this point. For example, if a third physical 
hard disk is found, and a primary partition exists on the third physical 
drive, the primary MS-DOS partition on the third physical hard drive is 
assigned the letter E.

MS-DOS returns to the first physical hard disk drive and assigns drive 
letters to any additional logical drives (in extended MS-DOS partitions) 
on that drive in sequence.

MS-DOS repeats this process for the second physical hard disk drive, 
if present. MS-DOS 5.0 will repeat this process for up to eight physical 
hard drives, if present. After all logical drives (in extended MS-DOS 
partitions) have been assigned drive letters, MS-DOS 5.0 returns to 
the first physical drive and assigns drive letters to any other primary 
MS-DOS partitions that exist, then searches other physical drives for 
additional primary MS-DOS partitions. This support for multiple primary 
MS-DOS partitions was added to version 5.0 for backward compatibility 
with the previous OEM MS-DOS versions that support multiple primary partitions.

After all logical drives on the hard disk(s) have been assigned drive 
letters, drive letters are assigned to drives installed using DRIVER.SYS 
or created using RAMDRIVE.SYS in the order in which the drivers are loaded 
in the CONFIG.SYS file. Which drive letters are assigned to which devices 
can be influenced by changing the order of the device drivers or, if necessary, 
by creating "dummy" drive letters with DRIVER.SYS.

********************************************************

or

  as rather well documented, DOS searches 1st) 1 primary patitions on
     all drives, 2nd) all extended partitions. that
     makes many people (including me) unhappy, as all DRIVES D:,E:...
     on 1st disk will move up/down, if other disk with
     primary partitions are added/removed, but
     thats the way it is (hope I got it right)
     TE (with a little help from my friends)
     see also above for WIN2000,DOS,MSDN

I don't know, if I did it right, but I tried to do it that way. TE

***********************************************************************/

#if defined(NEC98)
STATIC void make_ddt (ddt *pddt, int Unit, int driveno, int flags)
{
  bpb *defbpb = &pddt->ddt_defbpb;
  
  pddt->ddt_next = MK_FP(0, 0xffff);
  pddt->ddt_logdriveno = Unit;
  pddt->ddt_driveno = driveno;
  pddt->ddt_type = init_getdriveparm(driveno, defbpb);
  pddt->ddt_ncyl = defbpb->bpb_nsize / defbpb->bpb_nheads / defbpb->bpb_nsecs;
  pddt->ddt_descflags = init_readdasd(driveno) | flags;

  pddt->ddt_offset = 0;
  pddt->ddt_serialno = 0x12345678l;
  memcpy(&pddt->ddt_bpb, defbpb, sizeof(bpb));
  push_ddt(pddt);
}

STATIC COUNT make_floppy_ddts(ddt *pddt, COUNT unit_index, COUNT units)
{
  COUNT n;
  UBYTE boot_daua;
  
  boot_daua = BootDaua;
  if ((BootDaua & 0x70) == 0x30)
    boot_daua = 0x90 | (BootDaua & 0x0f);   /* assume 1.44M as 2HD drive */
  n = sizeof(DauaFDs)/sizeof(DauaFDs[0]);
  units += unit_index;
  if (units > n) units = n;
  for(n = unit_index; n < units; ++n)
  {
    UBYTE daua = DauaFDs[n];
    if (daua)
    {
      UBYTE da, ua;
      int fd_type;
      
      da = daua & 0xf0;
      ua = daua & 0x0f;
      fd_type = BIOS_fdtype_nec98(daua);
      if (InitKernelConfig.InitDiskShowDriveAssignment)
      {
        printf("\r%c: FD%u ", 'A' + nUnits, nFDUnits);
        switch(da)
        {
          case 0x30: case 0x90: printf("2HD #%u", ua); break;
          case 0x70: printf("2DD #%u", ua); break;
          case 0x50: printf("320K-2D #%u", ua); break;
          default:
            printf("(unknown DA/UA=%02x)", daua);
        }
        switch(fd_type)
        {
          case 2: printf(" (2HD/2DD)"); break;
          case 3: printf(" (1.44M/2HD/2DD)"); break;
        }
        printf("\n");
      }
      make_ddt(pddt, nUnits, daua, 0);
      /* store DA/UA list in internal work area */
      if (nUnits < 16)
        pokeb(0x60, 0x006c + nUnits, daua);
      if (nUnits < 26)
      {
        pokeb(0x60, 0x2c86 + nUnits*2, 0);
        pokeb(0x60, 0x2c87 + nUnits*2, daua);
        FDtype[nUnits] = fd_type;
      }
      if (boot_daua == daua)
        LoL->BootDrive = pddt->ddt_logdriveno + 1;
      ++nUnits;
      ++nFDUnits;
    }
  }
  return n;
}

#elif defined(IBMPC)
STATIC void make_ddt (ddt *pddt, int Unit, int driveno, int flags)
{
  pddt->ddt_next = MK_FP(0, 0xffff);
  pddt->ddt_logdriveno = Unit;
  pddt->ddt_driveno = driveno;
  pddt->ddt_type = init_getdriveparm(driveno, &pddt->ddt_defbpb);
  pddt->ddt_ncyl = (pddt->ddt_type & 7) ? 80 : 40;
  pddt->ddt_descflags = init_readdasd(driveno) | flags;

  pddt->ddt_offset = 0;
  pddt->ddt_serialno = 0x12345678l;
  memcpy(&pddt->ddt_bpb, &pddt->ddt_defbpb, sizeof(bpb));
  push_ddt(pddt);
}
#endif /* IBMPC */

void ReadAllPartitionTables(void)
{
#if 1
  PartitionsField foundPartitions[MAX_HARD_DRIVE];    /* need ULONG for portablity, maybe... */
#else
  UBYTE foundPartitions[MAX_HARD_DRIVE];
#endif

  int HardDrive;
  int nHardDisk;
  ddt nddt;
#if defined(IBMPC)
  static iregs regs;
#endif
  COUNT nFloppyIndex;
  COUNT nFloppyRest;
  UBYTE dla;
  UBYTE use_hd;

  BootDaua = peekb(0, 0x584); /* DISK_BOOT */
  BootPartIndex = peekw(0x0, 0x3fe); /* fetch boot partition from BOOTPART_SCRATCHPAD (see boot.asm) */
  pokew(0, 0x3fc, FP_OFF(unhandled_int_handler_iosys));
  pokew(0, 0x3fe, FP_SEG(unhandled_int_handler_iosys));
  if ((BootPartIndex & 0xff00) == 0x100)  /* boot from HD with 256bytes/sector */
    BootPartIndex = (BootPartIndex & 0xff) / 32;
  else                                /* or (probably) 512bytes/sector */
    BootPartIndex = (BootPartIndex & 0x1ff) / 32;
  
  nUnits = 0;
  nFloppyRest = BIOS_nfdrives();
  nFloppyIndex = 0;
  /*
    DLASortByDriveNo:
    
    0x80    Boot media type at first, other type succeed (NEC98 common way)
    0x81    Always A: and B: are reserved for FD, C: is for HD (like IBMPC)
    0x82    Always DOS partitions in HDs first, then FDs
    0x83    Always FDs first, then DOS partitions in HDs
    
    0xF0~F3 when boot from FD, all partitions in HDD are ignored
            otherwise same as 0x80~83
    
    others  same as 0x80
  */
  use_hd = 1;
  dla = DLASort;
  if ((dla & 0xf0) == 0xf0) {
    if (is_daua_fd(BootDaua)) {
      use_hd = 0;
      dla = 0x83;
    }
    else {
      dla &= 0x8f;
    }
  }
  
  if (dla == 0x81) {
    /* A: and B: as FD, C: as HD (like IBMPC) */
    if (nFloppyRest >= 2)
      nFloppyIndex = make_floppy_ddts(&nddt, nFloppyIndex, 1);
    else
      make_ddt(&nddt, 0, 0, 0);
    if (nFloppyRest >= 1)
      nFloppyIndex = make_floppy_ddts(&nddt, nFloppyIndex, 1);
    else
      make_ddt(&nddt, 0, 0, 0);
    nUnits = 2;
  }
  else if (dla != 0x82 && nFloppyRest > 0 && (is_daua_fd(BootDaua) || dla == 0x83))
  {
  
    nFloppyIndex = make_floppy_ddts(&nddt, nFloppyIndex, nFloppyRest);
    nFloppyRest = 0;
  }

  nHardDisk = BIOS_nrdrives();
  if (!use_hd)
    nHardDisk = 0;
  if (nHardDisk > LENGTH(foundPartitions))
    nHardDisk = LENGTH(foundPartitions);

  DebugPrintf(("DSK init: found %d disk drives\n", nHardDisk));

  /* Reset the drives                                             */
  for (HardDrive = 0; HardDrive < nHardDisk; HardDrive++)
  {
#if defined(NEC98)
    BIOS_drive_reset(DauaHDs[HardDrive]);
#else
    BIOS_drive_reset(HardDrive);
#endif
    foundPartitions[HardDrive] = 0;
  }

#if defined(IBMPC)
  if (InitKernelConfig.DLASortByDriveNo == 0)
  {
    /* printf("Drive Letter Assignment - DOS order\n"); */

    /* Process primary partition table   1 partition only      */
    for (HardDrive = 0; HardDrive < nHardDisk; HardDrive++)
    {
      foundPartitions[HardDrive] =
          ProcessDisk(SCAN_PRIMARYBOOT, HardDrive, 0);

      if (foundPartitions[HardDrive] == 0)
        foundPartitions[HardDrive] =
            ProcessDisk(SCAN_PRIMARY, HardDrive, 0);
    }

    /* Process extended partition table                      */
    for (HardDrive = 0; HardDrive < nHardDisk; HardDrive++)
    {
      ProcessDisk(SCAN_EXTENDED, HardDrive, 0);
    }

    /* Process primary a 2nd time */
    for (HardDrive = 0; HardDrive < nHardDisk; HardDrive++)
    {
      ProcessDisk(SCAN_PRIMARY2, HardDrive, foundPartitions[HardDrive]);
    }
  }
  else
#endif
  {
#if defined(NEC98)
    /* nec98: no drive letter re-order (for HDDs) */
#elif defined(IBMPC)
    UBYTE bootdrv = peekb(0,0x5e0);

    /* printf("Drive Letter Assignment - sorted by drive\n"); */

    /* Process primary partition table   1 partition only      */
    for (HardDrive = 0; HardDrive < nHardDisk; HardDrive++)
    {
      struct DriveParamS driveParam;
      if (LBA_Get_Drive_Parameters(HardDrive, &driveParam) &&
          driveParam.driveno == bootdrv)
      {
        foundPartitions[HardDrive] =
          ProcessDisk(SCAN_PRIMARYBOOT, HardDrive, 0);
        break;
      }
    }
#endif

    for (HardDrive = 0; HardDrive < nHardDisk; HardDrive++)
    {
      if (foundPartitions[HardDrive] == 0)
      {
        foundPartitions[HardDrive] =
          ProcessDisk(SCAN_PRIMARYBOOT, HardDrive, 0);

        if (foundPartitions[HardDrive] == 0)
          foundPartitions[HardDrive] =
            ProcessDisk(SCAN_PRIMARY, HardDrive, 0);
      }

      /* Process extended partition table                      */
      ProcessDisk(SCAN_EXTENDED, HardDrive, 0);

      /* Process primary a 2nd time */
      ProcessDisk(SCAN_PRIMARY2, HardDrive, foundPartitions[HardDrive]);
    }
  }
#if defined(NEC98) /* test */
  if (nFloppyRest > 0)
    make_floppy_ddts(&nddt, nFloppyIndex, nFloppyRest);
#endif
}

/* disk initialization: returns number of units */
COUNT dsk_init()
{
  printf(" - InitDisk");

#ifdef DEBUG
# if defined(NEC98)
# elif defined(IBMPC)
  {
    iregs regs;
    regs.a.x = 0x1112;          /* select 43 line mode - more space for partinfo */
    regs.b.x = 0;
    init_call_intr(0x10, &regs);
  }
# endif
#endif /* DEBUG */

  init_kernel_config();
  /* Reset the drives                                             */
  BIOS_drive_reset(0);

  ReadAllPartitionTables();

  return nUnits;
}
