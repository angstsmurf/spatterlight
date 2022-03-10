/* taylor.c  Treaty of Babel module for Adventure Soft files
 * GPL license.
 *
 * This file depends on treaty_builder.h
 */

#define FORMAT taylor
#define HOME_PAGE "https://github.com/EtchedPixels/TaylorMade"
#define FORMAT_EXT ".sna,.tzx,.tap,.z80"
#define NO_METADATA
#define NO_COVER

#define MAX_LENGTH 300000
#define MIN_LENGTH 24

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#include "treaty_builder.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

struct rec {
    int32 length;
    uint16_t chk;
    char *ifid;
};

static struct rec taylor_registry[] = {
    { 0x104c4, 0x40dd, "485BA862E1E192026D2158DCA87A482A" }, // Blizzard pass TAP
    { 0x10428, 0xe67d, "485BA862E1E192026D2158DCA87A482A" }, // Blizzard pass tzx
    { 0x2001f, 0x275a, "485BA862E1E192026D2158DCA87A482A" }, // Blizzard pass sna

    { 0, 0, NULL }
};


static uint16_t checksum(unsigned char *sf, int32 extent)
{
    uint16_t c=0;
    for(int i = 0; i < extent; i++)
        c+=sf[i];
    return c;
}

static int32 find_in_database(unsigned char *sf, int32 extent, char **ifid) {
    if (extent > MAX_LENGTH || extent < MIN_LENGTH)
        return INVALID_STORY_FILE_RV;

    uint16_t chksum = checksum(sf, extent);

    for (int i = 0; taylor_registry[i].length != 0; i++) {
        if (extent == taylor_registry[i].length &&
            chksum == taylor_registry[i].chk) {
            if (ifid != NULL) {
                size_t length = strlen(taylor_registry[i].ifid);
                strncpy(*ifid, taylor_registry[i].ifid, length);
                (*ifid)[length] = 0;
            }
            return VALID_STORY_FILE_RV;
        }
    }
    return INVALID_STORY_FILE_RV;
}

static int find_code(char *x, int codelen, unsigned char *sf, int32 extent) {
    if (codelen >= extent)
        return -1;
    unsigned const char *p = sf;
    while (p < sf + extent - codelen) {
        if (memcmp(p, x, codelen) == 0) {
            return p - sf;
        }
        p++;
    }
    return -1;
}

static int detect_verbs(unsigned char *sf, int32 extent) {
    return find_code("NORT\01N   \01S   \02SOUT", 10, sf, extent);
}



static int32 claim_story_file(void *storyvp, int32 extent)
{
    unsigned char *storystring = (unsigned char *)storyvp;

    fprintf(stderr, "The length of this file is %x, and its checksum %x\n", extent, checksum(storystring, extent));

    if (extent < 24 || extent > 300000)
        return INVALID_STORY_FILE_RV;

    if (detect_verbs(storystring, extent))
        return VALID_STORY_FILE_RV;

    if (find_in_database(storystring, extent, NULL) == VALID_STORY_FILE_RV)
        return VALID_STORY_FILE_RV;

    return INVALID_STORY_FILE_RV;
}

static int32 get_story_file_IFID(void *storyvp, int32 extent, char *output, int32 output_extent)
{
    ASSERT_OUTPUT_SIZE(37);

    unsigned char *storystring = (unsigned char *)storyvp;

    if (find_in_database(storystring, extent, &output) == VALID_STORY_FILE_RV) {
        size_t length = strlen(output);
        for (int i = 0; i< length; i++)
            output[i] = toupper(output[i]);
        output[length] = 0;
        return VALID_STORY_FILE_RV;
    }

    if (detect_verbs(storystring, extent) == VALID_STORY_FILE_RV)
    {
        strcpy(output, "\0");
        return INCOMPLETE_REPLY_RV;
    }
    return INVALID_STORY_FILE_RV;
}
