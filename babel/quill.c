/* quill.c  Treaty of Babel module for Quill files
 * 2006 By Tor Andersson from UnQuill,
 * UnQuill is copyright 1996-2002 by John Elliott.
 * GPL license.
 *
 *      CPC .SNA (used by CPCEMU)  and  Commodore  .S64	(used  by
 *      VICE).	spconv	(1)  can be used to convert many Spectrum
 *      snapshot formats to .SNA; I'm not aware of equivalent  CPC
 *      or C64 tools. Note that spconv v1.08 or later is needed to
 *      convert the most recent format of .Z80 snapshots.
 *
 * This file depends on treaty_builder.h
 */

#define FORMAT quill
#define HOME_PAGE "http://ifarchive.org/if-archive/programming/quill/"
#define FORMAT_EXT ".quill,.sna,.s64"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "md5.h"

enum { ARCH_SPECTRUM = 'S', ARCH_CPC = 'C', ARCH_C64 = '6' };

static int32 claim_story_file(void *storyvp, int32 extent)
{
    char *zxmemory = storyvp;
    int mem_base;          /* Minimum address loaded from snapshot     */
    int mem_size;          /* Size of memory loaded from snapshot      */
    int mem_offset;        /* Load address of snapshot in physical RAM */
    int zxptr;
    int n, seekpos;
    int found;
    int dbver = 0;
    int arch;
    
#define zmem(addr) ( \
     (addr < mem_base || \
      (arch != ARCH_SPECTRUM && \
       (addr >= (mem_base + mem_size)))) ? -1 : zxmemory[addr - mem_base] )
			 
     arch       = ARCH_SPECTRUM;
     seekpos    = 0x1C1B;	/* Number of bytes to skip in the file */
     mem_base   = 0x5C00;	/* First address loaded */
     mem_size   = 0xA400;	/* Number of bytes to load from it */
     mem_offset = 0x3FE5;	/* Load address of snapshot in host system memory */
     
     if (extent < 20)
	 return INVALID_STORY_FILE_RV;
     
     if (!memcmp(zxmemory + 0, "MV - SNA", 9))
     {
	 arch = ARCH_CPC;
	 seekpos    = 0x1C00;
	 mem_base   = 0x1B00;
	 mem_size   = 0xA500;
	 mem_offset = -0x100;
	 dbver = 10;	/* CPC engine is equivalent to the "later" Spectrum one. */
     }
     
     if (!memcmp(zxmemory, "VICE Snapshot File\032", 19))
     {
	 arch = ARCH_C64;
	 seekpos    =  0x874;
	 mem_base   =  0x800;
	 mem_size   = 0xA500;
	 mem_offset =  -0x74;
	 dbver = 5;	/* C64 engine is between the two Spectrum ones. */
     }
     
     if (extent < seekpos + mem_size)
	 return INVALID_STORY_FILE_RV;
     
     found = 0;
     
     switch (arch)
     {
	 case ARCH_SPECTRUM:
	     
	     /* I could _probably_ assume that the Spectrum database is 
	     * always at one location for "early" and another for "late"
	     * games. But this method is safer. */
	     
	     for (n = 0x5C00; n < 0xFFF5; n++)
	     {
		 if ((zmem(n  ) == 0x10) && (zmem(n+2) == 0x11) && 
		     (zmem(n+4) == 0x12) && (zmem(n+6) == 0x13) && 
		     (zmem(n+8) == 0x14) && (zmem(n+10) == 0x15))
		 {
		     found = 1;
		     zxptr = n + 13;
		     break;
		 }
	     }
	     break;
	     
	 case ARCH_CPC: 
	     found = 1;
	     zxptr = 0x1BD1;	/* From guesswork: CPC Quill files always seem to start at 0x1BD1 */
	     break;
	     
	 case ARCH_C64: 
	     found = 1;
	     zxptr = 0x804;	/* From guesswork: C64 Quill files always seem to start at 0x804 */
	     break;
     }
     
     if (found)
	 return VALID_STORY_FILE_RV;
     return INVALID_STORY_FILE_RV;
}

static int32 get_story_file_IFID(void *storyvp, int32 extent, char *output, int32 output_extent)
{
    char *story = storyvp;
    ASSERT_OUTPUT_SIZE(7);
    if (claim_story_file(storyvp, extent) == VALID_STORY_FILE_RV)
    {
	strcpy(output, "QUILL-");
	return INCOMPLETE_REPLY_RV;
    }
    return INVALID_STORY_FILE_RV;
}
