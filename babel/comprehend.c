/* comprehend.c  Treaty of Babel module for Comprehend files
 * 2026 By Petter Sjölund
 *
 * Comprehend is the adventure engine Polarware/Penguin Software used for
 * Transylvania, The Crimson Crown, Oo-Topos and Talisman. The games ship
 * as Apple II disk images (.dsk sector dumps and .woz flux images), which
 * are generic container formats shared with countless other Apple II
 * programs. We therefore can't claim a file from its extension or a disk
 * header alone; instead we recognise the specific Comprehend disk images
 * by their length and a 16-bit additive checksum, the same way the scott
 * module recognises Apple II Scott Adams disks.
 *
 * Comprehend defines no embedded IFID, so the IFID is the MD5 of the
 * disk image, prefixed with "COMPREHEND-".
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT comprehend
#define HOME_PAGE "https://ifarchive.org/indexes/if-archive/games/comprehend/"
#define FORMAT_EXT ".dsk,.woz"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>

struct comprehend_entry {
    int32 length;       /* exact size of the disk image */
    uint16_t chk;         /* 16-bit additive checksum of the whole image */
    const char *name;   /* which game/side this is (for reference only) */
};

/* Known Comprehend disk images. The .dsk dumps are all the standard DOS 3.3
 * size (0x23000), so the checksum is what tells them apart; the .woz flux
 * images vary slightly in size between dumps. */
static struct comprehend_entry comprehend_registry[] = {
    /* Transylvania (Polarware, 1985 Comprehend re-release) */
    { 0x23000, 0x9b50, "Transylvania side A (DSK, 4am crack)" },
    { 0x23000, 0xbf9f, "Transylvania side B (DSK, 4am crack)" },
    { 0x3af40, 0xadc3, "Transylvania side A (WOZ)" },
    { 0x3af40, 0x207e, "Transylvania side B (WOZ)" },

    /* The Crimson Crown (Transylvania II) (Polarware, 1985) */
    { 0x23000, 0xb74e, "The Crimson Crown side A (DSK, 4am crack)" },
    { 0x23000, 0x3f11, "The Crimson Crown side B (DSK, 4am crack)" },
    { 0x3950e, 0x5c39, "The Crimson Crown side A (WOZ)" },
    { 0x3950e, 0xfb9c, "The Crimson Crown side B (WOZ)" },

    /* Oo-Topos (Polarware, 1986 Comprehend re-release) */
    { 0x23000, 0xb9cb, "Oo-Topos side A (DSK, 4am crack)" },
    { 0x23000, 0xf95c, "Oo-Topos side B - boot (DSK, 4am crack)" },
    { 0x3951a, 0x994b, "Oo-Topos side A (WOZ)" },
    { 0x3951a, 0x9a1e, "Oo-Topos side B (WOZ)" },

    /* Talisman: Challenging the Sands of Time (Polarware, 1987) */
    { 0x23000, 0x7467, "Talisman disk 1 - Boot (DSK, 4am crack)" },
    { 0x23000, 0x5398, "Talisman disk 2 - Empire (DSK, 4am crack)" },
    { 0x23000, 0xe62a, "Talisman disk 3 - Lands Beyond (DSK, 4am crack)" },
    { 0x3953f, 0xc5b9, "Talisman side 1 - Boot (WOZ)" },
    { 0x39541, 0xebab, "Talisman side 2 - Empire (WOZ)" },
    { 0x39547, 0x6076, "Talisman side 3 - Lands Beyond (WOZ)" },

    { 0, 0, NULL }
};

static uint16_t comprehend_checksum(unsigned char *sf, int32 extent)
{
    uint16_t c = 0;
    int32 i;
    for (i = 0; i < extent; i++)
        c += sf[i];
    return c;
}

static int32 claim_story_file(void *story_file, int32 extent)
{
    unsigned char *sf = (unsigned char *)story_file;
    uint16_t chk = 0;
    int calculated = 0;
    int i;

    for (i = 0; comprehend_registry[i].length; i++) {
        if (extent == comprehend_registry[i].length) {
            if (!calculated) {
                chk = comprehend_checksum(sf, extent);
                calculated = 1;
            }
            if (chk == comprehend_registry[i].chk)
                return VALID_STORY_FILE_RV;
        }
    }
    return INVALID_STORY_FILE_RV;
}

/* IFIDs for Comprehend are formed by prepending COMPREHEND- to the default
 * MD5 ifid.
 */
static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{
    if (claim_story_file(story_file, extent) != VALID_STORY_FILE_RV)
        return INVALID_STORY_FILE_RV;
    ASSERT_OUTPUT_SIZE(12);
    strcpy(output, "COMPREHEND-");
    return INCOMPLETE_REPLY_RV;
}
