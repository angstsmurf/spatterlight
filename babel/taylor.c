/* taylor.c  Treaty of Babel module for Adventure Soft files
 * GPL license.
 *
 * This file depends on treaty_builder.h
 */

#define FORMAT taylor
#define HOME_PAGE "https://github.com/EtchedPixels/TaylorMade"
#define FORMAT_EXT ".sna,.tzx,.tap,.z80,.d64,.t64"
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

static const struct rec taylor_registry[] = {
    { 0x104c4, 0x40dd, "485BA862E1E192026D2158DCA87A482A" }, // Blizzard Pass TAP
    { 0x10428, 0xe67d, "485BA862E1E192026D2158DCA87A482A" }, // Blizzard Pass tzx
    { 0x2001f, 0x275a, "485BA862E1E192026D2158DCA87A482A" }, // Blizzard Pass sna
    { 0x12b4d, 0xee17, "485BA862E1E192026D2158DCA87A482A" }, // Blizzard Pass z80
    { 0xb77b, 0x77b3, "9dc7259c-de3d-47fa-b524-1c8d30262716" }, // Temple of Terror z80
    { 0xa496, 0xfa4c, "9dc7259c-de3d-47fa-b524-1c8d30262716" }, // Temple of Terror z80 alt
    { 0xbe30, 0x73a5, "9dc7259c-de3d-47fa-b524-1c8d30262716" }, // Temple of Terror TAP
    { 0xccca, 0xe88f, "9dc7259c-de3d-47fa-b524-1c8d30262716" }, // Temple of Terror tzx side A
    { 0xa000, 0x63ca, "9dc7259c-de3d-47fa-b524-1c8d30262716" }, // Temple of Terror tzx side B
    { 0xf716, 0x2b54, "9dc7259c-de3d-47fa-b524-1c8d30262716" }, // Temple of Terror C64
    { 0x10baa, 0x3b37, "9dc7259c-de3d-47fa-b524-1c8d30262716" }, // Temple of Terror C64 alt
    { 0x2ab00, 0x5720, "9dc7259c-de3d-47fa-b524-1c8d30262716" }, // Temple of Terror C64 D64
    { 0xb4bb, 0x2f47, "94a47aef-a838-4c9e-bd43-5f0adcfefc52" }, // Kayleth Z80 (Needs de-shuffling)
    { 0xa4f1, 0x8c14, "94a47aef-a838-4c9e-bd43-5f0adcfefc52" }, // Kayleth Z80 alt
    { 0xbd33, 0x0005, "94a47aef-a838-4c9e-bd43-5f0adcfefc52" }, // Kayleth TAP
    { 0xcadc, 0x733d, "94a47aef-a838-4c9e-bd43-5f0adcfefc52" }, // Kayleth tzx
    { 0xb1f2, 0x7757, "94a47aef-a838-4c9e-bd43-5f0adcfefc52" }, // Kayleth C64 T64
    { 0x2ab00, 0xc75f, "94a47aef-a838-4c9e-bd43-5f0adcfefc52" }, // Kayleth C64 D64
    { 0xa5dc, 0x42c9, "c9f6bfe4-3b84-41d1-99d4-b510b994e537" }, // Heman Z80
    { 0xa3fd, 0x9b82, "c9f6bfe4-3b84-41d1-99d4-b510b994e537" }, // Heman Z80 alt
    { 0xcd17, 0x6f68, "c9f6bfe4-3b84-41d1-99d4-b510b994e537" }, // Heman tzx
    { 0xcd15, 0x6bc4, "c9f6bfe4-3b84-41d1-99d4-b510b994e537" }, // Heman tzx alt
    { 0xfa17, 0xfbd2, "c9f6bfe4-3b84-41d1-99d4-b510b994e537" }, // Heman C64
    { 0xb56a, 0xf1fd, "ec96dff8-3ef9-4dbc-a165-9c509ff35f44" }, // Rebel Planet Z80
    { 0xbd3b, 0xec4f, "ec96dff8-3ef9-4dbc-a165-9c509ff35f44" }, // Rebel Planet TAP
    { 0xbd5b, 0xf5ea, "ec96dff8-3ef9-4dbc-a165-9c509ff35f44" }, // Rebel Planet tzx
    { 0xd541, 0x593a, "ec96dff8-3ef9-4dbc-a165-9c509ff35f44" }, // Rebel Planet C64
    { 0xb345, 0xc48b, "126e2481-30f5-46d4-abdd-9339526f516b" }, // Questprobe 3 Z80
    { 0xbcb6, 0x6902, "126e2481-30f5-46d4-abdd-9339526f516b" }, // Questprobe 3 tzx
    { 0xbb08, 0xab12, "126e2481-30f5-46d4-abdd-9339526f516b" }, // Questprobe 3 tzx alt
    { 0xbc20, 0x3726, "126e2481-30f5-46d4-abdd-9339526f516b" }, // Questprobe 3 tzx alt 2
    { 0xbbe6, 0x2683, "126e2481-30f5-46d4-abdd-9339526f516b" }, // Questprobe 3 TAP
    { 0x69e3, 0x3b96, "126e2481-30f5-46d4-abdd-9339526f516b" }, // Questprobe 3 C64
    { 0x8298, 0xb93e, "126e2481-30f5-46d4-abdd-9339526f516b" }, // Questprobe 3 C64 alt
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
    return find_code("NORT\01N   \01S   \02SOUT", 19, sf, extent);
}



static int32 claim_story_file(void *storyvp, int32 extent)
{
    unsigned char *storystring = (unsigned char *)storyvp;

//    fprintf(stderr, "The length of this file is %x, and its checksum %x\n", extent, checksum(storystring, extent));

    if (extent < 24 || extent > 300000)
        return INVALID_STORY_FILE_RV;

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
