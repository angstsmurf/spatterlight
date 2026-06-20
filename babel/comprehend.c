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
 * MS-DOS Comprehend games ship as a directory of files, with the game
 * strings embedded in a standard MZ executable (NOVEL.EXE). We identify
 * those executables by the MD5 of their first 1024 bytes, which is unique
 * per known release. The caller passes NOVEL.EXE as the story file; the
 * interpreter chdirs to the same directory and loads the data files by name.
 *
 * Comprehend defines no embedded IFID and has no scheme of its own in the
 * Treaty of Babel, so the IFID is just the MD5 of the story file (the
 * default for "other legacy file formats", Treaty section 2.2.5.12).
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT comprehend
#define HOME_PAGE "https://ifarchive.org/indexes/if-archive/games/comprehend/"
#define FORMAT_EXT ".dsk,.woz,.exe"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include "md5.h"
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

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

    /* The Coveted Mirror 2.0 (Polarware, Comprehend re-release). The original
     * 1.0 release uses Penguin's earlier engine and is not a Comprehend game. */
    { 0x23000, 0xb0b2, "The Coveted Mirror disk 1 (DSK)" },
    { 0x23000, 0xd9dd, "The Coveted Mirror disk 2 (DSK)" },
    { 0x3957e, 0x695f, "The Coveted Mirror 2.0 side A (WOZ / woz-a-day)" },
    { 0x3957e, 0x41c6, "The Coveted Mirror 2.0 side B - boot (WOZ / woz-a-day)" },
    /* NOTE: the "The Coveted Mirror (woz-a-day collection)" images without
     * the 2.0 tag (0x39539 bytes, chk 0xb77b / 0x530d) are the 1983 original:
     * a DOS 3.3 disk running Penguin's earlier animated engine (CM.ACTIONS0-3,
     * CM.MESSAGES, JOUST/FISHGAME/SIGNGAME minigames, .ANM animations), so
     * they must not be registered here. */

    /* Talisman: Challenging the Sands of Time (Polarware, 1987) */
    { 0x23000, 0x7467, "Talisman disk 1 - Boot (DSK, 4am crack)" },
    { 0x23000, 0x5398, "Talisman disk 2 - Empire (DSK, 4am crack)" },
    { 0x23000, 0xe62a, "Talisman disk 3 - Lands Beyond (DSK, 4am crack)" },
    { 0x3953f, 0xc5b9, "Talisman side 1 - Boot (WOZ)" },
    { 0x39541, 0xebab, "Talisman side 2 - Empire (WOZ)" },
    { 0x39547, 0x6076, "Talisman side 3 - Lands Beyond (WOZ)" },

    { 0, 0, NULL }
};

/* Known MS-DOS Comprehend executables, identified by MD5 of first 1024 bytes.
 * This matches the same check the interpreter performs in game_tm.cpp. */
static const char *comprehend_dos_exe_md5s[] = {
    "881f6c504456a41a70f8fb515edd04cd",  /* Transylvania NOVEL.EXE */
    "e708b5f744e1f9b59354722ab55added",  /* Transylvania v2 NOVEL.EXE (g0 release) */
    "3fc2072f6996b17d2f21f0a92e53cdcc",  /* OO-Topos NOVEL.EXE */
    "3d4f5d26c64aa5b403914be83f4e8d73",  /* Crimson Crown NOVEL.EXE */
    "0e7f002971acdb055f439020363512ce",  /* Talisman NOVEL.EXE (original release) */
    "2e18c88ce352ebea3e14177703a0485f",  /* Talisman NOVEL.EXE (later release) */
    "15cd75f98788071aee2af1f63893f613",  /* Talisman NOVEL1.EXE (later release) */
    NULL
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
    int i;

    /* MS-DOS MZ executable: identify by MD5 of the first 1024 bytes. */
    if (extent >= 1024 && sf[0] == 0x4d && sf[1] == 0x5a) {
        md5_state_t state;
        md5_byte_t digest[16];
        char hexdigest[33];
        md5_init(&state);
        md5_append(&state, sf, 1024);
        md5_finish(&state, digest);
        for (i = 0; i < 16; i++)
            sprintf(hexdigest + i * 2, "%02x", (unsigned int)digest[i]);
        hexdigest[32] = '\0';
        for (i = 0; comprehend_dos_exe_md5s[i]; i++) {
            if (strcmp(hexdigest, comprehend_dos_exe_md5s[i]) == 0)
                return VALID_STORY_FILE_RV;
        }
        fprintf(stderr, "hexdigest for this file: %s\n", hexdigest);
        return INVALID_STORY_FILE_RV;
    }

    /* Apple II disk images: identify by file length and additive checksum. */
    {
        uint16_t chk = 0;
        int calculated = 0;
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
    }
    return INVALID_STORY_FILE_RV;
}

/* The IFID for Comprehend is simply the MD5 of the story file. Leaving the
 * output empty makes the handler fill in the bare MD5 checksum.
 */
static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{
    if (claim_story_file(story_file, extent) != VALID_STORY_FILE_RV)
        return INVALID_STORY_FILE_RV;
    ASSERT_OUTPUT_SIZE(33);
    output[0] = '\0';
    return INCOMPLETE_REPLY_RV;
}
