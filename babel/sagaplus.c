/* sagaplus.c  Treaty of Babel module for Saga Plus files
 * GPL license.
 *
 * This file depends on treaty_builder.h
 */

#define FORMAT sagaplus
#define HOME_PAGE "https://github.com/angstsmurf/spatterlight/tree/master/terps/plus"
#define FORMAT_EXT ".dat,.d64,.atr,.st,.msa,.dsk,.woz,.plus,.plu"
#define NO_METADATA
#define NO_COVER

#define MAX_LENGTH 368641
#define MIN_LENGTH 24

#include "treaty_builder.h"
#include "babel_util.h"
#include <ctype.h>
#include <stdbool.h>

static const char *ifids[] = {
    NULL,
    "B5AF6E4DB3C3B2118FAEA3849F807617", // The Sorcerer Of Claymorgue Castle
    "DAEE386546CE71831DC365B0FF10F233", // Questprobe featuring Spider-Man
    "13EA7A22731E90598456D13311923833", // Buckaroo Banzai
    "126E2481-30F5-46D4-ABDD-9339526F516B", // Fantastic Four
    "CAD0523C-5ED9-4337-85DA-A940FC36C118" // X-Men
};

typedef enum {
    NO_IFID,
    SORCERER_OF_CLAYMORGUE_CASTLE_IFID,
    QUESTPROBE_SPIDERMAN_IFID,
    BUCKAROO_BANZAI_IFID,
    QUESTPROBE_FANTASTIC_FOUR_IFID,
    QUESTPROBE_XMEN_IFID
} IfidType;

static const char *idstrings[] = {
    NULL,
    "Sorcerer of Claymorgue Castle. SAGA#13.", // The Sorcerer Of Claymorgue Castle
    "SPIDER-MAN (tm)", // Questprobe featuring Spider-Man
    "BUCKAROO", // Buckaroo Banzai
    "FF #1 ", // Fantastic Four
    "X-MEN "
};

typedef enum {
    NO_ID_STRING,
    SORCERER_OF_CLAYMORGUE_CASTLE_ID_STRING,
    QUESTPROBE_SPIDERMAN_ID_STRING,
    BUCKAROO_BANZAI_ID_STRING,
    QUESTPROBE_FANTASTIC_FOUR_ID_STRING,
    QUESTPROBE_XMEN_ID_STRING
} IdStringType;

struct plusrec {
    int32 length;
    uint16_t chk;
    IdStringType id;
    IfidType ifid;
};

static const struct plusrec plus_registry[] = {
    { 0, 0, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // Spider-Man MS-DOS
    { 0x2ab00, 0x833c, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // questprobe_spider-man[gvp_1985](!).d64
    { 0x2ab00, 0x83dc, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // questprobe_spider-man[gvp_1985](!).d64
    { 0x2ab00, 0xe1cd, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // questprobe_spider-man[sharedata_1987].d64
    { 0x16810, 0xc003, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // atr
    { 0x16810, 0xbfc1, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // atr alt
    { 0x16810, 0xfea3, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // atr alt 2
    { 0x16810, 0x6358, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // atr alt 2 side two
    { 0x16810, 0xbfc7, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // atr Rev 258 side A
    { 0x16810, 0x8b46, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // atr Rev 258 side B
    { 0x16810, 0xfea9, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // atr Rev 258 side A CSS
    { 0x16810, 0x6258, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // atr Rev 258 side B
    { 0x5a000, 0x973b, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // st
    { 0x23000, 0xc267, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // apple 2
    { 0x23000, 0x80b2, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // apple 2 side A
    { 0x39547, 0x874f, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // apple 2 (WOZ) side A
    { 0x39547, 0xd729, QUESTPROBE_SPIDERMAN_ID_STRING, QUESTPROBE_SPIDERMAN_IFID }, // apple 2 (WOZ) side B
    { 0, 0, SORCERER_OF_CLAYMORGUE_CASTLE_ID_STRING, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // Claymorgue MS-DOS
    { 0x5a000, 0xcc3, SORCERER_OF_CLAYMORGUE_CASTLE_ID_STRING, SORCERER_OF_CLAYMORGUE_CASTLE_IFID }, // st
    { 0, 0, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID },
    { 0x2ab00, 0x3464, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // BuckarooBanzai.d64
    { 0x2ab00, 0x3720, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // BuckarooBanzai.d64 alt
    { 0x2ab00, 0x3720, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // BuckarooBanzai.d64 alt
    { 0x2adab, 0x786e, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // BuckarooBanzai.d64 alt 2 s1
    { 0x2adab, 0xfee0, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // BuckarooBanzai.d64 alt 2 s2
    { 0x2ab00, 0x52df, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // Side B of the e=mc2 release of Sorcerer of Claymorgue Castle. Damaged: 19 pictures are missing

    { 0x16810, 0x5434, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai Atari 8-bit
    { 0x16810, 0x19ef, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai Atari 8-bit side B
    { 0x16810, 0x4735, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai Atari 8-bit Rev 387
    { 0x16810, 0x473b, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai Atari 8-bit Rev 387 side A
    { 0x16810, 0x19ee, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai Atari 8-bit Rev 387 side B
    { 0x16810, 0xe399, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai Atari 8-bit Rev 397
    { 0x16810, 0xe39f, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai Atari 8-bit Rev 397 alt
    { 0x16810, 0x9234, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai Atari 8-bit Rev 397 side B
    { 0x23000, 0x3d97, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai Apple 2
    { 0x23000, 0xa9e3, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai Apple 2 side A
    { 0x23000, 0xca17, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai Apple 2 alt (177b)
    { 0x23000, 0x268c, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai Apple 2 alt sida A (177a)
    { 0x23000, 0xa543, BUCKAROO_BANZAI_ID_STRING, BUCKAROO_BANZAI_IFID }, // Buckaroo Banzai Apple 2 alt 2
    { 0, 0, QUESTPROBE_FANTASTIC_FOUR_ID_STRING, QUESTPROBE_FANTASTIC_FOUR_IFID }, // Fantastic Four MS-DOS
    { 0x2ab00, 0xec72, QUESTPROBE_FANTASTIC_FOUR_ID_STRING, QUESTPROBE_FANTASTIC_FOUR_IFID }, // Fantastic Four C64
    { 0x2ab00, 0xd2c0, QUESTPROBE_FANTASTIC_FOUR_ID_STRING, QUESTPROBE_FANTASTIC_FOUR_IFID }, // Fantastic Four C64 alt
    { 0x2ab00, 0xd938, QUESTPROBE_FANTASTIC_FOUR_ID_STRING, QUESTPROBE_FANTASTIC_FOUR_IFID }, // Fantastic Four C64 alt 2
    { 0x16810, 0xe3ad, QUESTPROBE_FANTASTIC_FOUR_ID_STRING, QUESTPROBE_FANTASTIC_FOUR_IFID }, // Fantastic Four Atari 8-bit
    { 0x16810, 0xeafb, QUESTPROBE_FANTASTIC_FOUR_ID_STRING, QUESTPROBE_FANTASTIC_FOUR_IFID }, // Fantastic Four Atari 8-bit side B
    { 0x16810, 0xb09d, QUESTPROBE_FANTASTIC_FOUR_ID_STRING, QUESTPROBE_FANTASTIC_FOUR_IFID }, // Fantastic Four Atari 8-bit alt
    { 0x16810, 0x3c1e, QUESTPROBE_FANTASTIC_FOUR_ID_STRING, QUESTPROBE_FANTASTIC_FOUR_IFID }, // Fantastic Four Atari 8-bit side B alt
    { 0x16810, 0x35d1, QUESTPROBE_FANTASTIC_FOUR_ID_STRING, QUESTPROBE_FANTASTIC_FOUR_IFID }, // Fantastic Four Atari 8-bit vF-172 side A
    { 0x16810, 0xb0a6, QUESTPROBE_FANTASTIC_FOUR_ID_STRING, QUESTPROBE_FANTASTIC_FOUR_IFID }, // Fantastic Four Atari 8-bit vF-172 side B

    { 0x23000, 0xa450, QUESTPROBE_FANTASTIC_FOUR_ID_STRING, QUESTPROBE_FANTASTIC_FOUR_IFID }, // Fantastic Four Apple 2
    { 0x23000, 0x79e0, QUESTPROBE_FANTASTIC_FOUR_ID_STRING, QUESTPROBE_FANTASTIC_FOUR_IFID }, // Fantastic Four Apple 2 side A
    { 0x23000, 0xd841, QUESTPROBE_FANTASTIC_FOUR_ID_STRING, QUESTPROBE_FANTASTIC_FOUR_IFID }, // Fantastic Four Apple 2 alt
    { 0x23000, 0xf46f, QUESTPROBE_FANTASTIC_FOUR_ID_STRING, QUESTPROBE_FANTASTIC_FOUR_IFID }, // Fantastic Four Apple 2 alt side A
    { 0x3426a, 0x8ffa, QUESTPROBE_FANTASTIC_FOUR_ID_STRING, QUESTPROBE_FANTASTIC_FOUR_IFID }, // Fantastic Four Atari ST
    { 0xc480, 0xd462, QUESTPROBE_XMEN_ID_STRING, QUESTPROBE_XMEN_IFID }, // X-Men prototype

    { 0, 0, NO_ID_STRING, NO_IFID }
};

static int read_string(unsigned char *text, int32_t extent, int32_t *offset, bool *failure)
{
    int c,nc;
    int ct=0;
    do {
        if(*offset >= extent) {
            *failure = true;
            return 0;
        }
        c=text[(*offset)++];
    } while(isspace(c));
    
    if(c!='"') {
        *failure = true;
        return 0;
    }
    do {
        if(*offset >= extent) {
            *failure = true;
            return 0;
        }
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

/* Compares an id string to the quoted string that read_string() has just
   read from the very start of the file, leaving offset after the closing quote. */
static bool title_matches(unsigned char *sf, int32 offset, const char *id)
{
    size_t length = strlen(id);
    return (offset >= 2 && length == (size_t)(offset - 2) && memcmp(sf + 1, id, length) == 0);
}

static int32 find_dskimg_in_database(unsigned char *sf, int32 extent, char *ifid) {
    if (extent > MAX_LENGTH || extent < MIN_LENGTH)
        return INVALID_STORY_FILE_RV;

    int calculated_checksum = 0;
    uint16_t chksum;

    for (int i = 0; plus_registry[i].ifid != NO_IFID; i++) {
        if (extent == plus_registry[i].length) {
            if (calculated_checksum == 0) {
                chksum = babel_sum16(sf, extent);
                calculated_checksum = 1;
            }
            if (chksum == plus_registry[i].chk) {
                if (ifid != NULL)
                    babel_copy_ifid(ifid, ifids[plus_registry[i].ifid]);
                return VALID_STORY_FILE_RV;
            }
        }
    }
    return INVALID_STORY_FILE_RV;
}

static int32 find_in_database(unsigned char *sf, int32 extent, char *ifid) {
    if (extent > MAX_LENGTH || extent < MIN_LENGTH)
        return INVALID_STORY_FILE_RV;

    int32 offset = 0;
    bool failure = false;

    read_string(sf, extent, &offset, &failure);
    if (failure == true)
        return INVALID_STORY_FILE_RV;

    if (offset < 2)
        return INVALID_STORY_FILE_RV;

    for (int i = 0; plus_registry[i].id != NO_ID_STRING; i++) {
        if (title_matches(sf, offset, idstrings[plus_registry[i].id])) {
            if (ifid != NULL)
                babel_copy_ifid(ifid, ifids[plus_registry[i].ifid]);
            return VALID_STORY_FILE_RV;
        }
    }
    return INVALID_STORY_FILE_RV;
}


static int32 claim_story_file(void *storyvp, int32 extent)
{
    unsigned char *storystring = (unsigned char *)storyvp;

#ifdef DEBUG
    fprintf(stderr, "The length of this file is %x, and its checksum %x\n", extent, babel_sum16(storystring, extent));
#endif

    if (extent < MIN_LENGTH || extent > MAX_LENGTH)
        return INVALID_STORY_FILE_RV;

    /* A text database is known by its title string, a disk image by its
       length and checksum */
    if (find_in_database(storystring, extent, NULL) == VALID_STORY_FILE_RV)
        return VALID_STORY_FILE_RV;

    return find_dskimg_in_database(storystring, extent, NULL);
}

static int32 get_story_file_IFID(void *storyvp, int32 extent, char *output, int32 output_extent)
{
    ASSERT_OUTPUT_SIZE(37);

    unsigned char *storystring = (unsigned char *)storyvp;

    if (find_in_database(storystring, extent, output) == VALID_STORY_FILE_RV)
        return VALID_STORY_FILE_RV;

    return find_dskimg_in_database(storystring, extent, output);
}
