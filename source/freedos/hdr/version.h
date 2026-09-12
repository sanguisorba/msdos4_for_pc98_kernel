/****************************************************************/
/*                                                              */
/*                          version.h                           */
/*                                                              */
/*                  Common version information                  */
/*                                                              */
/*                      Copyright (c) 1997                      */
/*                      Pasquale J. Villani                     */
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

#if defined(NEC98)
#define TARGET_PLATFORM "PC-98x1"
#define FREEDOS_NAME "FreeDOS(98)"
#endif


#ifndef TARGET_PLATFORM_FOR
#ifdef TARGET_PLATFORM
#define TARGET_PLATFORM_FOR  " for " TARGET_PLATFORM
#else
#define TARGET_PLATFORM_FOR  ""
#endif
#endif

#ifndef FREEDOS_NAME
#define FREEDOS_NAME  "FreeDOS"
#endif

/* The version the kernel reports as compatible with */
#ifdef WITHFAT32
#define MAJOR_RELEASE   7
#define MINOR_RELEASE   10
#else
#define MAJOR_RELEASE   6
# if defined(NEC98)
   /* DOS 6.20 for NEC PC-98x1, just for a proof... */
#  define MINOR_RELEASE   20
# else
#  define MINOR_RELEASE   22
# endif
#endif

/* The actual kernel revision, 2000+REVISION_SEQ = 2.REVISION_SEQ */
#define REVISION_SEQ    43      /* returned in BL by int 21 function 30 */
#define OEM_ID          0xfd    /* FreeDOS, returned in BH by int 21 30 */

/* Used for version information displayed to user at boot (& stored in os_release string) */
#ifndef KERNEL_VERSION
#define KERNEL_VERSION "- GIT "
#endif

/* actual version string */
#if 1
# if defined(DBCS)
#define KVS(v,s,o) FREEDOS_NAME " kernel " v "(build 20" #s " DBCS OEM:" #o ") [compiled " __DATE__ "]\n"
# else
#define KVS(v,s,o) FREEDOS_NAME " kernel " v "(build 20" #s " OEM:" #o ") [compiled " __DATE__ "]\n"
# endif
#else
#define KVS(v,s,o) "FreeDOS kernel " v "(build 20" #s " OEM:" #o ")" TARGET_PLATFORM_FOR " [compiled " __DATE__ "]\n"
#endif
#define xKVS(v,s,o) KVS(v,s,o)
#define KERNEL_VERSION_STRING xKVS(KERNEL_VERSION, REVISION_SEQ, OEM_ID)

