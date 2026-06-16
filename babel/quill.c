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
#define FORMAT_EXT ".quill,.sna,.s64,.z80,.t64,.tzx"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "md5.h"

enum { ARCH_SPECTRUM = 'S', ARCH_CPC = 'C', ARCH_C64 = '6' };

/* Decode one Z80-snapshot memory block (ED ED run-length encoding) from
 * in[0..inlen) into out[0..outsize), zero-padding any shortfall. Same decoder
 * as babel's level9 module, faithful to Paul David Doherty's l9cut. */
static void z80_decode_block(const unsigned char *in, int32 inlen,
                             unsigned char *out, int32 outsize)
{
    int32 x = 0, y = 0;
    while (x < inlen && y < outsize) {
        if (in[x] != 237) { out[y++] = in[x]; x++; }
        else if (x+1 >= inlen) { out[y++] = in[x]; x++; }
        else if (in[x+1] != 237) {
            out[y++] = 237;
            if (y < outsize) out[y++] = in[x+1];
            x += 2;
        } else {
            if (x+3 >= inlen) break;
            int count = in[x+2];
            unsigned char val = in[x+3];
            while (count-- > 0 && y < outsize) out[y++] = val;
            x += 4;
        }
    }
    while (y < outsize) out[y++] = 0;
}

/* For a v2/v3 Z80 snapshot, walk the page list starting at `pos` and decode the
 * 16K page whose page-type byte equals `wanted` into out[0..16384). */
static int z80_find_page(const unsigned char *sf, int32 extent, int32 pos,
                         int wanted, unsigned char *out)
{
    while (pos + 3 <= extent) {
        int32 len = sf[pos] | (sf[pos+1] << 8);
        int type = sf[pos+2];
        int32 data = pos + 3;
        if (type == wanted) {
            if (len == 0xffff) {
                int32 n = extent - data; if (n > 16384) n = 16384;
                if (n > 0) memcpy(out, sf+data, n);
            } else {
                int32 n = len; if (n > extent - data) n = extent - data;
                z80_decode_block(sf+data, n, out, 16384);
            }
            return 1;
        }
        if (len == 0xffff) len = 16384;
        pos = data + len;
    }
    return 0;
}

/* If 'sf' is a Z80 Spectrum snapshot, rebuild the visible 48K memory image
 * (0x4000-0xffff) into a freshly malloc'd 49152-byte buffer so the Quill
 * colour-table scan can see through Z80 compression. Returns NULL when the
 * buffer is not a recognisable Z80 snapshot. Mirrors babel's level9 module. */
static unsigned char *z80_to_image(const unsigned char *sf, int32 extent, int32 *out_len)
{
    unsigned char *img;
    if (extent < 34) return NULL;

    if ((sf[6] | (sf[7] << 8)) != 0) {
        /* v1.45: a single 48K block follows the 30-byte header */
        int flags = sf[12];
        if (flags == 255) flags = 1;
        if ((img = malloc(49152)) == NULL) return NULL;
        if (flags & 0x20) {
            /* compressed up to the trailing 00 ED ED 00 end marker */
            int32 inlen = extent - 34;
            if (inlen < 0) { free(img); return NULL; }
            z80_decode_block(sf+30, inlen, img, 49152);
        } else {
            int32 inlen = extent - 30;
            if (inlen > 49152) inlen = 49152;
            memcpy(img, sf+30, inlen);
            if (inlen < 49152) memset(img+inlen, 0, 49152-inlen);
        }
        *out_len = 49152;
        return img;
    }

    /* v2/v3: extra header length distinguishes the sub-version */
    {
        int xhlen = sf[30] | (sf[31] << 8);
        int32 seekseed;
        int firsttype, pages[3], k;
        if (xhlen == 23) seekseed = 55;                 /* v2.01 */
        else if (xhlen == 54 || xhlen == 55) seekseed = 86; /* v3.0x */
        else return NULL;
        if (seekseed + 3 > extent) return NULL;
        firsttype = sf[seekseed+2];
        if (firsttype == 4) {       /* 48K snapshot: pages 5-1-2 */
            pages[0]=8; pages[1]=4; pages[2]=5;
        } else if (firsttype == 3) {/* 128K snapshot: banks 5-2-0 */
            pages[0]=8; pages[1]=5; pages[2]=3;
        } else return NULL;
        if ((img = malloc(49152)) == NULL) return NULL;
        memset(img, 0, 49152);
        for (k = 0; k < 3; k++)
            z80_find_page(sf, extent, seekseed, pages[k], img + k*16384);
        *out_len = 49152;
        return img;
    }
}

/* Gilsoft's Professional Adventure Writer (PAW), the Quill's successor, shares
 * the Quill-style colour-control byte run that the scans here key on, so a PAW
 * image can be misclaimed as Quill (and then crashes UnQuill, which only reads
 * Quill databases). A PAW *development* snapshot carries the PAW editor's menu
 * text verbatim, and these phrases are unique to PAW: the Quill has no adverbs
 * and no high/low-priority condition tables. (Shipped PAW games tokenise their
 * text and carry no such plain marker, so this only rejects editor snapshots
 * like "A Shadow on Glass".) Scanning for any one of these is enough; they
 * never occur in a genuine Quill game. */
static int buffer_is_paw(const unsigned char *buf, int32 extent)
{
    static const char *const markers[] = {
        "priority condition",   /* "High/Low priority conditions" */
        "EDIT ADVERBS",
    };
    size_t m;
    for (m = 0; m < sizeof markers / sizeof markers[0]; m++) {
        int32 ml = (int32)strlen(markers[m]);
        int32 i;
        for (i = 0; i + ml <= extent; i++)
            if (buf[i] == (unsigned char)markers[m][0] &&
                !memcmp(buf + i, markers[m], ml))
                return 1;
    }
    return 0;
}

/* A genuine Quill database, located 13 bytes after the colour-definition table,
 * holds six core table pointers — responses, process, objects, locations,
 * messages and connections, at zxptr+4,+6,+8,+10,+12,+14 — and none of them can
 * be null (every Quill game has all six tables). Some non-Quill snapshots match
 * the loose colour-table pattern by coincidence but yield a "header" with null
 * core pointers, and UnQuill then crashes following them ("Invalid address").
 * Reject those. `buf` maps Spectrum address `a` to buf[a - base]; the six words
 * are non-null pointers in both the early and later Quill database formats, so
 * this check is independent of which the game uses. */
static int quill_header_plausible(const unsigned char *buf, int32 buflen,
                                  int32 base, int32 zxptr)
{
    int off;
    for (off = 4; off <= 14; off += 2) {
        int32 i = zxptr + off - base;
        if (i < 0 || i + 1 >= buflen)
            return 0;
        if ((buf[i] | (buf[i+1] << 8)) == 0)
            return 0;
    }
    return 1;
}

/* Scan a decoded 48K Spectrum image (base address 0x4000) for the Quill
 * colour-definition table, then confirm a plausible database header follows
 * (skipping coincidental colour-table matches). */
static int z80_image_is_quill(const unsigned char *img, int32 len)
{
    int n;
    for (n = 0x5C00; n < 0xFFF5; n++) {
        int o = n - 0x4000;
        if (o + 10 >= len) break;
        if (img[o] == 0x10 && img[o+2] == 0x11 && img[o+4] == 0x12 &&
            img[o+6] == 0x13 && img[o+8] == 0x14 && img[o+10] == 0x15 &&
            quill_header_plausible(img, len, 0x4000, n + 13))
            return 1;
    }
    return 0;
}

/* Walk a TZX tape image and scan every standard/turbo speed data block for the
 * Quill colour-definition table. Commercial loaders often use non-standard flag
 * bytes (e.g. 0xC7) rather than the ROM standard 0xFF, so we scan all block
 * content. Metadata blocks are skipped by their known sizes. */
static int tzx_is_quill(const unsigned char *sf, int32 extent)
{
    int32 pos = 10; /* skip 10-byte TZX header */

    while (pos < extent) {
        int id = sf[pos++];
        int32 dlen = 0;
        const unsigned char *data = NULL;

        switch (id) {
        case 0x10: /* Standard Speed Data Block */
            if (pos + 4 > extent) return 0;
            dlen = sf[pos+2] | (sf[pos+3] << 8);
            data = sf + pos + 4;
            pos += 4 + dlen;
            break;
        case 0x11: /* Turbo Speed Data Block */
            if (pos + 18 > extent) return 0;
            dlen = sf[pos+15] | (sf[pos+16] << 8) | (sf[pos+17] << 16);
            data = sf + pos + 18;
            pos += 18 + dlen;
            break;
        case 0x20: pos += 2;                       continue;
        case 0x21: if (pos < extent) pos += 1 + sf[pos]; continue;
        case 0x22:                                 continue;
        case 0x24: pos += 2;                       continue;
        case 0x25:                                 continue;
        case 0x2A: pos += 4;                       continue;
        case 0x2B: pos += 5;                       continue;
        case 0x30: if (pos < extent) pos += 1 + sf[pos]; continue;
        case 0x31: if (pos + 1 < extent) pos += 2 + sf[pos+1]; continue;
        case 0x32: if (pos + 1 < extent) { int32 l = sf[pos] | (sf[pos+1]<<8); pos += 2 + l; } continue;
        case 0x33: if (pos < extent) pos += 1 + sf[pos] * 3; continue;
        case 0x35: if (pos + 20 <= extent) { int32 l = sf[pos+16]|(sf[pos+17]<<8)|(sf[pos+18]<<16)|(sf[pos+19]<<24); pos += 20 + l; } continue;
        default: return 0;
        }

        if (pos > extent || dlen < 2 || !data) { continue; }

        if (dlen >= 12) { /* scan entire block for Quill colour table */
            int32 n;
            for (n = 0; n + 10 <= dlen; n++) {
                if (data[n]   == 0x10 && data[n+2]  == 0x11 &&
                    data[n+4] == 0x12 && data[n+6]  == 0x13 &&
                    data[n+8] == 0x14 && data[n+10] == 0x15)
                    return 1;
            }
        }
    }
    return 0;
}

static int32 claim_story_file(void *storyvp, int32 extent)
{
    char *zxmemory = storyvp;
    int mem_base;          /* Minimum address loaded from snapshot     */
    int mem_size;          /* Size of memory loaded from snapshot      */
//    int mem_offset;        /* Load address of snapshot in physical RAM */
//    int zxptr;
    int n, seekpos;
    int found;
//    int dbver = 0;
    int arch;

#define zmem(addr) ( \
     (addr < mem_base || \
      (arch != ARCH_SPECTRUM && \
       (addr >= (mem_base + mem_size)))) ? -1 : zxmemory[addr - mem_base] )

     arch       = ARCH_SPECTRUM;
     seekpos    = 0x1C1B;	/* Number of bytes to skip in the file */
     mem_base   = 0x5C00;	/* First address loaded */
     mem_size   = 0xA400;	/* Number of bytes to load from it */
//     mem_offset = 0x3FE5;	/* Load address of snapshot in host system memory */

     if (extent < 20)
	 return INVALID_STORY_FILE_RV;

     /* TZX tape image: walk the block list and scan CODE data block payloads
      * for the Quill colour-definition table. */
     if (extent >= 10 && !memcmp(zxmemory, "ZXTape!\x1a", 8))
         return tzx_is_quill((const unsigned char *)zxmemory, extent)
                ? VALID_STORY_FILE_RV : INVALID_STORY_FILE_RV;

     /* Crunched C64 .t64 tape images can't be recognised structurally (the
      * Quill database only appears after the game decompresses itself), so
      * known images are identified by size + a 16-bit additive checksum, as
      * the scott/taylor modules do for their C64 games. */
     if (extent >= 96 && !memcmp(zxmemory, "C64", 3))
     {
	 unsigned int sum = 0;
	 int i;
	 for (i = 0; i < extent; i++)
	     sum += (unsigned char)zxmemory[i];
	 sum &= 0xFFFF;
	 if (extent == 0xbee9 && sum == 0xc404)  /* Blizzard Pass (C64) */
	     return VALID_STORY_FILE_RV;
     }

     /* A compressed .z80 snapshot won't expose the Quill colour table in its
      * raw bytes (and is usually shorter than the .SNA length check below), so
      * decode the 48K image and scan that. */
     {
	 int32 dlen = 0;
	 unsigned char *img = z80_to_image((const unsigned char *)zxmemory, extent, &dlen);
	 if (img)
	 {
	     /* A PAW development snapshot can carry the Quill colour-table
	      * pattern by coincidence; reject it before claiming it as Quill. */
	     int paw = buffer_is_paw(img, dlen);
	     int ok = !paw && z80_image_is_quill(img, dlen);
	     free(img);
	     if (paw)
		 return INVALID_STORY_FILE_RV;
	     if (ok)
		 return VALID_STORY_FILE_RV;
	 }
     }

     if (!memcmp(zxmemory + 0, "MV - SNA", 9))
     {
	 arch = ARCH_CPC;
	 seekpos    = 0x1C00;
	 mem_base   = 0x1B00;
	 mem_size   = 0xA500;
//	 mem_offset = -0x100;
//	 dbver = 10;	/* CPC engine is equivalent to the "later" Spectrum one. */
     }
     
     if (!memcmp(zxmemory, "VICE Snapshot File\032", 19))
     {
	 arch = ARCH_C64;
	 seekpos    =  0x874;
	 mem_base   =  0x800;
	 mem_size   = 0xA500;
//	 mem_offset =  -0x74;
//	 dbver = 5;	/* C64 engine is between the two Spectrum ones. */
     }
     
     if (extent < seekpos + mem_size)
	 return INVALID_STORY_FILE_RV;

     /* An uncompressed PAW snapshot exposes the editor text directly; reject
      * it before the (loose) colour-table scan can misclaim it as Quill. */
     if (arch == ARCH_SPECTRUM && buffer_is_paw((const unsigned char *)zxmemory, extent))
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
		     (zmem(n+8) == 0x14) && (zmem(n+10) == 0x15) &&
		     quill_header_plausible((const unsigned char *)zxmemory,
					    extent, mem_base, n + 13))
		 {
		     found = 1;
//		     zxptr = n + 13;
		     break;
		 }
	     }
	     break;
	     
	 case ARCH_CPC: 
	     found = 1;
//	     zxptr = 0x1BD1;	/* From guesswork: CPC Quill files always seem to start at 0x1BD1 */
	     break;
	     
	 case ARCH_C64: 
	     found = 1;
//	     zxptr = 0x804;	/* From guesswork: C64 Quill files always seem to start at 0x804 */
	     break;
     }
     
     if (found)
	 return VALID_STORY_FILE_RV;
     return INVALID_STORY_FILE_RV;
}

static int32 get_story_file_IFID(void *storyvp, int32 extent, char *output, int32 output_extent)
{
//    char *story = storyvp;
    ASSERT_OUTPUT_SIZE(7);
    if (claim_story_file(storyvp, extent) == VALID_STORY_FILE_RV)
    {
	strcpy(output, "QUILL-");
	return INCOMPLETE_REPLY_RV;
    }
    return INVALID_STORY_FILE_RV;
}
