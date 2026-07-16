/* quest5.c  Treaty of Babel module for Quest 5 files
 * 2026 By Petter Sjölund.
 * GPL license.
 *
 * Quest 5 games are either .quest packages (a zip archive whose directory
 * contains game.aslx plus resources) or raw .aslx XML whose root element is
 * <asl version="NNN">.  A zip's game.aslx entry is normally deflated, so
 * IFID/metadata extraction from packages would need zlib; like quest4 we
 * settle for claiming the file (babel falls back to an MD5 IFID).  Raw
 * .aslx files are plain text, so their <gameid> GUID can be returned as the
 * real IFID.
 *
 * This file depends on treaty_builder.h
 */

#define FORMAT quest5
#define HOME_PAGE "https://textadventures.co.uk/quest"
#define FORMAT_EXT ".quest,.aslx"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

/* If string is found, returns a pointer to the char AFTER it */
static unsigned char *find_string(unsigned char *storyvp, int32 extent, const char *string, int32 stringlength)
{
    for (int i = 0; i < extent - stringlength - 1; i++)
        if (memcmp(string, storyvp + i, (size_t) stringlength) == 0)
            return storyvp + i + stringlength;
    return NULL;
}

/* A .quest package is a zip whose local file headers name game.aslx.  The
 * entry names are stored in plaintext even when the data is deflated. */
static bool quest_package_found(unsigned char *story_file, int32 extent)
{
    if (story_file == NULL || extent < 30)
        return false;
    if (memcmp(story_file, "PK\003\004", 4) != 0)
        return false;
    return find_string(story_file, extent, "game.aslx", 9) != NULL;
}

/* Raw .aslx: after an optional BOM and whitespace the content must be markup,
 * with an <asl ...> root tag near the top (an <?xml?> declaration and
 * comments may precede it). */
static bool aslx_header_found(unsigned char *story_file, int32 extent)
{
    if (story_file == NULL)
        return false;
    int32 i = 0;
    if (extent >= 3 && story_file[0] == 0xef && story_file[1] == 0xbb &&
        story_file[2] == 0xbf)
        i = 3;
    while (i < extent && isspace(story_file[i]))
        i++;
    if (i >= extent || story_file[i] != '<')
        return false;
    int32 limit = extent < 2048 ? extent : 2048;
    for (; i + 4 < limit; i++)
        if (memcmp(story_file + i, "<asl", 4) == 0 &&
            (story_file[i + 4] == ' ' || story_file[i + 4] == '>' ||
             story_file[i + 4] == '\t' || story_file[i + 4] == '\r' ||
             story_file[i + 4] == '\n'))
            return true;
    return false;
}

static int32 claim_story_file(void *storyvp, int32 extent)
{
    if (extent > 30 && quest_package_found(storyvp, extent))
        return VALID_STORY_FILE_RV;
    if (extent > 10 && aslx_header_found(storyvp, extent))
        return VALID_STORY_FILE_RV;
    return INVALID_STORY_FILE_RV;
}

static int32 get_story_file_IFID(void *storyvp, int32 extent, char *output, int32 output_extent)
{
    if (claim_story_file(storyvp, extent) != VALID_STORY_FILE_RV)
        return INVALID_STORY_FILE_RV;

    /* Raw .aslx keeps its <gameid> GUID in plaintext.  (In a .quest package
     * the XML is deflated, so fall through to babel's MD5 fallback.) */
    if (memcmp(storyvp, "PK\003\004", 4) != 0)
    {
        unsigned char *p = find_string(storyvp, extent, "<gameid>", 8);
        if (p != NULL)
        {
            char guid[64];
            int n = 0;
            int32 left = extent - (int32) (p - (unsigned char *) storyvp);
            while (n < 63 && n < left && p[n] != '<' &&
                   (isxdigit(p[n]) || p[n] == '-'))
            {
                guid[n] = (char) toupper(p[n]);
                n++;
            }
            guid[n] = 0;
            if (n == 36 && (n < left && p[n] == '<'))
            {
                ASSERT_OUTPUT_SIZE(n + 1);
                memcpy(output, guid, (size_t) (n + 1));
                return 1;
            }
        }
    }

    ASSERT_OUTPUT_SIZE(1);
    output[0] = '\0';
    return INCOMPLETE_REPLY_RV;
}
