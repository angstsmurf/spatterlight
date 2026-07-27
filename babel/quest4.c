/* quest.c  Treaty of Babel module for Quest 4 files
 * Parts based on the Geas source
 * 2021 By Petter Sjölund.
 * GPL license.
 *
 * This file depends on treaty_builder.h
 */

#define FORMAT quest4
#define HOME_PAGE "https://alexwarren.uk/projects/quest-versions-1-to-4"
#define FORMAT_EXT ".cas,.asl"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include "babel_handler.h"
#include <stdbool.h>


/* If string is found, returns a pointer to the char AFTER it */
static unsigned char *find_string(unsigned char *storyvp,  int32 extent, char *string, int32 stringlength)
{
    for(int i=0;i<extent-stringlength-1;i++)
        if (memcmp(string,storyvp+i,stringlength)==0)
            return storyvp + i + stringlength;
    return NULL;
}

/* A compiled Quest file begins with its container header, and three versions
 * exist: QCGF001 is the Quest 2.x era format, 002 adds three more text-mode
 * block types, and 003 appends a resource catalogue. Quest reads all three with
 * one decompiler (LoadCASFile, V4Game.cs:1921) and so does geas (readfile.cc,
 * "Three compiled-game container versions exist"), so claim all three. Only 002
 * used to be claimed here, which left QCGF001 games (Bargain, MagicSwordP1) and
 * QCGF003 ones (Beyond Exile 2.2, Forward and Back) unrecognised even though
 * they play. */
static bool quest4_header_found(unsigned char *story_file) {
    if(story_file == NULL)
        return false;
    if (memcmp(story_file, "QCGF00", 6) != 0)
        return false;
    return story_file[6] >= '1' && story_file[6] <= '3';
}

/* We simply look for the string "asl-version" inside an ASL "define block" */
static bool asl_header_found(unsigned char *story_file, int32 extent)
{
    if(story_file == NULL)
        return false;
    unsigned char *start = find_string(story_file, extent, "define game ", 12);
    if (start == NULL)
        return false;
    size_t offset = start - story_file;
    unsigned char *end = find_string(start, extent - offset, "end define", 10);
    if (end == NULL)
        return false;
    unsigned char *version = find_string(start, end - start - 10, "asl-version", 11);
    if (version == NULL)
        return false;
    return true;
}

static int32 claim_story_file(void *storyvp, int32 extent)
{
    bool found = false;

    if (extent > 8 && quest4_header_found(storyvp))
    {
        found = true;
    }
    else if (extent > 25 && asl_header_found(storyvp, extent))
    {
        found = true;
    }

    if (found)
        return VALID_STORY_FILE_RV;
    return INVALID_STORY_FILE_RV;
}

static int32 get_story_file_IFID(void *storyvp, int32 extent, char *output, int32 output_extent)
{
    if (claim_story_file(storyvp, extent) == VALID_STORY_FILE_RV)
    {
        /* Construct IFID */
        ASSERT_OUTPUT_SIZE(1);
        output[0] = '\0';
        return INCOMPLETE_REPLY_RV;
    }
    return INVALID_STORY_FILE_RV;
}
