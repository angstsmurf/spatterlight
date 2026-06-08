/* archetype.c  Treaty of Babel module for Archetype files
 * 2026 By Petter Sjölund
 *
 * Archetype is an object-oriented adventure-writing language by
 * Derek T. Jones, distributed with its Turbo Pascal source in 1995.
 * Its translator (CREATE.EXE) emits a binary .ACX file which is run by
 * the interpreter (PERFORM.EXE).
 *
 * Archetype defines no embedded IFID, so the IFID is the MD5 of the
 * story file, prefixed with "ARCHETYPE-".
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT archetype
#define HOME_PAGE "https://ifarchive.org/indexes/if-archive/programming/archetype/"
#define FORMAT_EXT ".acx"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>

/* Every .ACX file begins with the human-readable version banner that
 * CREATE.EXE writes (SYNTAX.PAS, dump_game) and PERFORM.EXE checks
 * (INTRPTR.PAS, load_game):
 *
 *   "Archetype version " <version> 0x0A 0x0D 0x1A
 *
 * followed by the binary header (a 6-byte Turbo Pascal real holding the
 * version number, an encryption byte, and a 4-byte timestamp). PERFORM
 * itself only matches the leading "Archetype version " stub to decide
 * that a file is an Archetype file, so we do the same.
 */
#define ARCHETYPE_MAGIC "Archetype version "

static int32 claim_story_file(void *story_file, int32 extent)
{
    if (extent < (int32) sizeof(ARCHETYPE_MAGIC) - 1 ||
        memcmp(story_file, ARCHETYPE_MAGIC, sizeof(ARCHETYPE_MAGIC) - 1))
        return INVALID_STORY_FILE_RV;
    return VALID_STORY_FILE_RV;
}

/* IFIDs for Archetype are formed by prepending ARCHETYPE- to the default
 * MD5 ifid.
 */
static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{
    if (claim_story_file(story_file, extent) != VALID_STORY_FILE_RV)
        return INVALID_STORY_FILE_RV;
    ASSERT_OUTPUT_SIZE(11);
    strcpy(output, "ARCHETYPE-");
    return INCOMPLETE_REPLY_RV;
}
