/*
 * Copyright 2010 John-Mark Bell <jmb@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Minimal compatibility header for AmigaOS 3
 */

#ifndef AMIGA_OS3SUPPORT_H_
#define AMIGA_OS3SUPPORT_H_

#ifndef __amigaos4__

#include <stdint.h>
#include <proto/exec.h>
#include <proto/dos.h>

/* Include prototypes for amigalib */
#include <clib/alib_protos.h>

#ifndef EXEC_MEMORY_H
#include <exec/memory.h>
#endif

/* Macros */
#define IsMinListEmpty(L) (L)->mlh_Head->mln_Succ == 0

/* Define extra memory type flags */
#define MEMF_PRIVATE	MEMF_ANY
#define MEMF_SHARED	MEMF_ANY

/* Ignore tags that aren't supported */
#define PDTA_PromoteMask	TAG_IGNORE

/* Easy compat macros */
/* application */
#define Notify(...) (void)0;

/* Exec */
/* AllocVecTagList with no tags */
#define AllocVecTagList(SZ,TAG) AllocVec(SZ,MEMF_ANY);
#define GetSucc(N) (N)->ln_Succ;

/* diskfont */
/* Only used in one place we haven't ifdeffed, where it returns the charset name */
#define ObtainCharsetInfo(A,B,C) (const char *)"ISO-8859-1"

/* DOS */
#define FOpen(A,B,C) Open(A,B)
#define FClose(A) Close(A)

/* Intuition */
#define IDoMethod DoMethod
#define IDoMethodA DoMethodA
#define IDoSuperMethodA DoSuperMethodA

/* Integral type definitions */
typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;

/* TimeVal */
struct TimeVal {
	uint32 Seconds;
	uint32 Microseconds;
};

/* Compositing */
#define COMPFLAG_IgnoreDestAlpha 0
#define COMPFLAG_SrcAlphaOverride 0
#define COMPFLAG_SrcFilter 0

#define COMPOSITE_Src 0

#define COMPTAG_ScaleX 0
#define COMPTAG_ScaleY 0
#define COMPTAG_DestX 0
#define COMPTAG_DestY 0
#define COMPTAG_DestWidth 0
#define COMPTAG_DestHeight 0
#define COMPTAG_OffsetX 0
#define COMPTAG_OffsetY 0

#define CompositeTags(a, ...) ((void) (a))
#define COMP_FLOAT_TO_FIX(f) (f)

/* icon.library v51 (ie. AfA_OS version) */
#define ICONCTRLA_SetImageDataFormat        (ICONA_Dummy + 0x67) /*103*/
#define ICONCTRLA_GetImageDataFormat        (ICONA_Dummy + 0x68) /*104*/

#define IDFMT_BITMAPPED     (0)  /* Bitmapped icon (planar, legacy) */
#define IDFMT_PALETTEMAPPED (1)  /* Palette mapped icon (chunky, V44+) */
#define IDFMT_DIRECTMAPPED  (2)  /* Direct mapped icon (truecolor 0xAARRGGBB, V51+) */ 

/* Functions */
/* DOS */
int64 GetFileSize(BPTR fh);

/* Exec */
struct Node *GetHead(struct List *list);

/* Utility */
char *ASPrintf(const char *fmt, ...);
#endif

#endif
