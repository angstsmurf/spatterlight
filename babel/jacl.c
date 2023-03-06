/* jacl.c  Treaty of Babel module for JACL files *
 * This file depends on treaty_builder.h
 */

#define FORMAT jacl
#define HOME_PAGE "https://code.google.com/archive/p/jacl/downloads"
#define FORMAT_EXT ".jacl,.j2"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdbool.h>


static int extract_ifid(unsigned char *text, int32 offset, int32 extent, char *output, int32 output_extent, bool encrypted)
{
    int c;
    int ct=0;
    char temp[100];
    do {
        c = text[offset++];
        if (encrypted && c != '\n')
            c = c ^ 255;
    } while(offset < extent && (c == '"' || c == ':' || isspace(c)));

    temp[ct++] = c;

    do
    {
        c = text[offset++];
        if (encrypted && c != '\n')
            c = c ^ 255;
        if(offset >= extent)
        {
            return INVALID_STORY_FILE_RV;
        }
        if (c == '"' || isspace(c))
        {
            break;
        }

        temp[ct++] = c;

    } while (offset < extent);

    temp[ct++] = '\0';

    ASSERT_OUTPUT_SIZE(ct);
    for(int i = 0; i < ct; i++)
        output[i] = temp[i];
    return VALID_STORY_FILE_RV;
}

static int32 claim_story_file(void *storyvp, int32 extent)
{
    if (extent > 13 && memcmp(storyvp, "#!../bin/jacl", 13) == 0)
        return VALID_STORY_FILE_RV;
    if (extent > 26 && memcmp(storyvp, "#!/usr/local/jacl/bin/jacl", 26) == 0)
        return VALID_STORY_FILE_RV;
    if (extent > 15 && memcmp(storyvp, "#!/usr/bin/jacl", 15) == 0)
        return VALID_STORY_FILE_RV;
    if (extent > 29 && memcmp(storyvp, "#!/usr/local/jacl/bin/cgijacl", 29) == 0)
        return VALID_STORY_FILE_RV;
    if (extent > 16 && memcmp(storyvp, "#!../bin/cgijacl", 16) == 0)
        return VALID_STORY_FILE_RV;
    return INVALID_STORY_FILE_RV;
}

static int32 get_story_file_IFID(void *storyvp, int32 extent, char *output, int32 output_extent)
{
    if (claim_story_file(storyvp, extent) == VALID_STORY_FILE_RV)
    {
        unsigned char *story_file = (unsigned char *)storyvp;
        int32 j;


        unsigned char i = 'i' ^ 255;
        unsigned char f = 'f' ^ 255;
        unsigned char d = 'd' ^ 255;
        bool encrypted = false;

        for(j = 0; j < extent - 7; j++) {
            if (memcmp((char *)story_file + j,"#ifid:",6) == 0) {
                j += 6;
                break;
            };
            if (j + 14 < extent && (memcmp((char *)story_file + j,"constant ifid",13) == 0 ||
                                    memcmp((char *)story_file + j,"constant\tifid",13) == 0)) {
                j += 13;
                break;
            }
            if (story_file[j] == i &&
                story_file[j + 1] == f &&
                story_file[j + 2] == i &&
                story_file[j + 3] == d) {
                encrypted = true;
                j += 4;
                break;
            }

        }
        if (j < extent) /* Found explicit IFID */ {
            return extract_ifid(story_file, j, extent, output, output_extent, encrypted);
        } else {
            ASSERT_OUTPUT_SIZE(1);
            strcpy(output, "\0");
            return INCOMPLETE_REPLY_RV;
        }
    }
    return INVALID_STORY_FILE_RV;
}
