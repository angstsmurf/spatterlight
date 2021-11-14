/* scott.c  Treaty of Babel module for ScottFree files
 * GPL license.
 *
 * This file depends on treaty_builder.h
 */

#define FORMAT scott
#define HOME_PAGE "https://github.com/cspiegel/scottfree-glk"
#define FORMAT_EXT ".dat,.saga"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

/* All numbers in ScottFree files are stored as text delimited by whitespace */
static int read_next_number(unsigned char *text, int32_t extent, int32_t *offset, bool *failure) {
    char numstring[100];
    int i;
    bool number_found = false;
    for (i = 0; i < extent - *offset && i < 99; i++) {
        char c = text[*offset + i];
        numstring[i] = c;
        if (isspace(c)) {
            if (number_found == true)
                break;
        } else if (isdigit(c) || c == '-') {
            number_found = true;
        } else {
            *failure = true;
            return 0;
        }
    }

    if (number_found == false) {
        *failure = true;
        return 0;
    }
    numstring[i+1] = '\0';
    *offset += i;
    int result = atoi(numstring);
    if (result > INT16_MAX || result < INT16_MIN)
        *failure = true;

    return result;
}

static int read_string(unsigned char *text, int32_t extent, int32_t *offset, bool *failure)
{
    int c,nc;
    int ct=0;
    do {
        c=text[(*offset)++];
    } while(*offset < extent && isspace(c));
    
    if(c!='"') {
        *failure = true;
        return 0;
    }
    do {
        c=text[(*offset)++];
        if(*offset >= extent) {
            *failure = true;
            return 0;
        }
        if(c=='"') {
            nc=text[(*offset)++];
            if(nc!='"') {
                (*offset)--;
                break;
            }
        }
        if (!isprint(c)) {
            *failure = true;
            return 0;
        }

       ct++;

    } while(*offset < extent);
    return ct;
}

static int32 claim_story_file(void *storyvp, int32 extent)
{
    unsigned char *storystring = (unsigned char *)storyvp;

    if (extent < 24)
        return INVALID_STORY_FILE_RV;

    /* Load the header */

    int32 offset = 0;
    int header[12];
    bool failure = false;

    /* We simply run a sped-up version of the first three parts of reading the database
     * from the file, and bail at the first sign of failure.
     */

    /* First the 12 integers of the header */
    for (int i = 0; i < 12; i++) {
        header[i] = read_next_number(storystring, extent, &offset, &failure);
        if (failure == true)
            return INVALID_STORY_FILE_RV;
    }

    /* Then the number of actions given in header[2] + 1 */
    for (int i = 0; i <= header[2]; i++) {
        /* 8 integers per action */
        for (int j = 0; j < 8; j++) {
            read_next_number(storystring, extent, &offset, &failure);
            if (failure == true)
                return INVALID_STORY_FILE_RV;
        }
    }

    /* Finally the number of dictionary words given in header[3] + 1 */
    for (int i = 0; i <= header[3]; i++) {
        for (int j = 0; j < 2; j++) {
            /* The word length is given in header[8], but many games have word exceeding this */
            /* in the dictionary. We use 10 characters as a reasonable upper limit */
            if (read_string(storystring, extent, &offset, &failure) > 10 || failure == true)
                return INVALID_STORY_FILE_RV;
        }
    }

    return VALID_STORY_FILE_RV;
}

static int32 get_story_file_IFID(void *storyvp, int32 extent, char *output, int32 output_extent)
{
    ASSERT_OUTPUT_SIZE(1);
    if (claim_story_file(storyvp, extent) == VALID_STORY_FILE_RV)
    {
        strcpy(output, "\0");
        return INCOMPLETE_REPLY_RV;
    }
    return INVALID_STORY_FILE_RV;
}
