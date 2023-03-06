/* taylor.c  Treaty of Babel module for Adventure Soft files
 * GPL license.
 *
 * This file depends on treaty_builder.h
 */

#define FORMAT taylor
#define HOME_PAGE "https://github.com/EtchedPixels/TaylorMade"
#define FORMAT_EXT ".sna,.tzx,.tap,.z80,.d64,.t64,.tay,.taylor"
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

static const char *ifids[] = {
    NULL,
    "126e2481-30f5-46d4-abdd-9339526f516b", // Questprobe 3
    "ec96dff8-3ef9-4dbc-a165-9c509ff35f44", // Rebel Planet
    "485BA862E1E192026D2158DCA87A482A", // Blizzard Pass
    "9dc7259c-de3d-47fa-b524-1c8d30262716", // Temple of Terror
    "c9f6bfe4-3b84-41d1-99d4-b510b994e537", // Masters of The Universe
    "94a47aef-a838-4c9e-bd43-5f0adcfefc52" // Kayleth
};

typedef enum {
    NO_IFID,
    FANTASTIC4_IFID,
    REBEL_PLANET_IFID,
    BLIZZARD_PASS_IFID,
    TEMPLE_OF_TERROR_IFID,
    TERRAQUAKE_IFID,
    KAYLETH_IFID
} IfidType;

struct rec {
    int32 length;
    uint16_t chk;
    IfidType ifid;
};

static const struct rec taylor_registry[] = {


    { 0xb56a,  0xf1fd, REBEL_PLANET_IFID }, // Rebel Planet Z80
    { 0xa52b,  0x994f, REBEL_PLANET_IFID }, // Rebel_Planet_1986_U.S._Gold_a.z80
    { 0xa50d,  0x973c, REBEL_PLANET_IFID }, // Rebel_Planet_1986_U.S._Gold_a2.z80
    { 0xa50d,  0x9450, REBEL_PLANET_IFID }, // Rebel_Planet_1986_U.S._Gold_a3.z80
    { 0xbd3b,  0xec4f, REBEL_PLANET_IFID }, // Rebel Planet TAP
    { 0xbd5b,  0xf5ea, REBEL_PLANET_IFID }, // Rebel Planet tzx
    { 0xd541,  0x593a, REBEL_PLANET_IFID }, // Rebel Planet C64
    { 0x2ab00, 0xa46f, REBEL_PLANET_IFID }, // Rebel_Planet_1986_Adventure_International.d64
    { 0x2ab00, 0x932e, REBEL_PLANET_IFID }, // Rebel_Planet_1986_Adventure_International_cr_TFF.d64

    { 0xb345,  0xc48b, FANTASTIC4_IFID }, // Questprobe 3 Z80
    { 0xaa2a,  0x7bee, FANTASTIC4_IFID }, // Questprobe 3 Z80 alt
    { 0xb49d,  0x00c6, FANTASTIC4_IFID }, // Questprobe 3 Z80 alt 2
    { 0xc01e,  0x8865, FANTASTIC4_IFID }, // Questprobe 3 Z80 alt 3
    { 0xbcb6,  0x6902, FANTASTIC4_IFID }, // Questprobe 3 tzx
    { 0xbb08,  0xab12, FANTASTIC4_IFID }, // Questprobe 3 tzx alt
    { 0xbc20,  0x3726, FANTASTIC4_IFID }, // Questprobe 3 tzx alt 2
    { 0xbbe6,  0x2683, FANTASTIC4_IFID }, // Questprobe 3 TAP
    { 0x69e3,  0x3b96, FANTASTIC4_IFID }, // Questprobe 3 C64
    { 0x8298,  0xb93e, FANTASTIC4_IFID }, // Questprobe 3 C64 alt
    { 0x2ab00, 0x863d, FANTASTIC4_IFID }, // Questprobe 3 C64 D64

    { 0x104c4, 0x40dd, BLIZZARD_PASS_IFID }, // Blizzard Pass TAP
    { 0x10428, 0xe67d, BLIZZARD_PASS_IFID }, // Blizzard Pass tzx
    { 0x2001f, 0x275a, BLIZZARD_PASS_IFID }, // Blizzard Pass sna
    { 0x12b4d, 0xee17, BLIZZARD_PASS_IFID }, // Blizzard Pass z80
    { 0xb77b,  0x77b3, TEMPLE_OF_TERROR_IFID }, // Temple of Terror z80
    { 0xa496,  0xfa4c, TEMPLE_OF_TERROR_IFID }, // Temple of Terror z80 alt
    { 0xbe30,  0x73a5, TEMPLE_OF_TERROR_IFID }, // Temple of Terror TAP
    { 0xccca,  0xe88f, TEMPLE_OF_TERROR_IFID }, // Temple of Terror tzx side A
    { 0xa000,  0x63ca, TEMPLE_OF_TERROR_IFID }, // Temple of Terror tzx side B
    { 0xf716,  0x2b54, TEMPLE_OF_TERROR_IFID }, // Temple of Terror C64
    { 0x10baa, 0x3b37, TEMPLE_OF_TERROR_IFID }, // Temple of Terror C64 alt
    { 0x2ab00, 0x5720, TEMPLE_OF_TERROR_IFID }, // Temple of Terror C64 D64

    { 0x2ab00, 0xf3b4, TEMPLE_OF_TERROR_IFID }, // Temple_of_Terror_1987_U.S._Gold_cr_FBR.d64
    { 0x2ab00, 0x577e, TEMPLE_OF_TERROR_IFID }, // Temple_of_Terror_1987_U.S._Gold_cr_Triad.d64
    { 0x2ab00, 0x4661, TEMPLE_OF_TERROR_IFID }, // Temple_of_Terror_1987_U.S._Gold_cr_Trianon.d64
    { 0x2ab00, 0x7b2d, TEMPLE_OF_TERROR_IFID }, // Temple of Terror - Trianon.d64
    { 0x2ab00, 0x3ee3, TEMPLE_OF_TERROR_IFID }, // Temple_of_Terror_Graphic_Version_1987_U.S._Gold_cr_FLT.d64
    { 0x2ab00, 0x5b97, TEMPLE_OF_TERROR_IFID }, // Temple_of_Terror_1987_U.S._Gold_h_FLT.d64
    { 0x2ab00, 0x55a1, TEMPLE_OF_TERROR_IFID }, // Temple_of_Terror_1987_US_Gold_cr_FLT.d64

    { 0xa5dc, 0x42c9, TERRAQUAKE_IFID }, // Heman Z80
    { 0xa3fd, 0x9b82, TERRAQUAKE_IFID }, // Heman Z80 alt
    { 0xa5d7, 0x44a2, TERRAQUAKE_IFID }, // Masters_of_the_Universe_The_Super_Adventure_1987_U.S._Gold_a2.z80
    { 0xa5d7, 0x4765, TERRAQUAKE_IFID }, // Masters_of_the_Universe_The_Super_Adventure_1987_U.S._Gold.z80
    { 0xcd17, 0x6f68, TERRAQUAKE_IFID }, // Heman tzx
    { 0xcd15, 0x6bc4, TERRAQUAKE_IFID }, // Heman tzx alt
    { 0xfa17, 0xfbd2, TERRAQUAKE_IFID }, // Heman C64
    { 0x2ab00, 0x4625, TERRAQUAKE_IFID }, // Masters_of_the_Universe_Terraquake_1987_Gremlin_Graphics_cr_TIA.d64
    { 0x2ab00, 0x78ba, TERRAQUAKE_IFID }, // Masters_of_the_Universe_Terraquake_1987_Gremlin_Graphics_cr_Popeye.d64

    { 0xb4bb, 0x2f47, KAYLETH_IFID }, // Kayleth Z80 (Needs de-shuffling)
    { 0xa4f1, 0x8c14, KAYLETH_IFID }, // Kayleth Z80 alt
    { 0xa354, 0xdee4, KAYLETH_IFID }, // Kayleth_1986_Adventuresoft_UK_a3.z80
    { 0xa355, 0xe2a5, KAYLETH_IFID }, // Kayleth_1986_Adventuresoft_UK_a2.z80

    { 0xbd33, 0x0005, KAYLETH_IFID }, // Kayleth TAP
    { 0xcadc, 0x733d, KAYLETH_IFID }, // Kayleth tzx
    { 0xb1f2, 0x7757, KAYLETH_IFID }, // Kayleth C64 T64
    { 0x2ab00,0xc75f, KAYLETH_IFID }, // Kayleth C64 D64

    { 0x2ab00, 0x2359, KAYLETH_IFID }, // Compilation_Karate_Chop_Kayleth_Kettle_Knotie_in_Cave_19xx_-.d64
    { 0x2ab00, 0x3a10, KAYLETH_IFID }, // Kayleth_1987_U.S._Gold_cr_Newstars.d64
    { 0x2ab00, 0xc184, KAYLETH_IFID }, // Kayleth_1987_U.S._Gold_cr_Triad.d64
    { 0x2ab00, 0x7a59, KAYLETH_IFID }, // Kayleth_1987_U.S._Gold_cr_OkKoral.d64
    { 0x2ab00, 0xbf75, KAYLETH_IFID }, // Kayleth_1987_U.S._Gold.d64

    { 0, 0, NO_IFID }
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

    int calculated_checksum = 0;
    uint16_t chksum;

    for (int i = 0; taylor_registry[i].length != 0; i++) {
        if (extent == taylor_registry[i].length) {
            if (calculated_checksum == 0) {
                chksum = checksum(sf, extent);
                calculated_checksum = 1;
            }
            if (chksum == taylor_registry[i].chk) {
                if (ifid != NULL) {
                    size_t length = strlen(ifids[taylor_registry[i].ifid]);
                    strncpy(*ifid, ifids[taylor_registry[i].ifid], length);
                    (*ifid)[length] = 0;
                }
                return VALID_STORY_FILE_RV;
            }
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
