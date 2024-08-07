/* hugo.c  Treaty of Babel module for hugo files
 * 2006 By L. Ross Raszewski
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT hugo
#define HOME_PAGE "http://www.generalcoffee.com"
#define FORMAT_EXT ".hex"
#define NO_METADATA
#define NO_COVER


/* The positions of various data in the header */

#define HUGO_CODESTART 0x0B

#define HUGO_OBJTABLE 0x0D
#define HUGO_PROPTABLE 0x0F
#define HUGO_EVENTTABLE 0x11
#define HUGO_ARRAYTABLE 0x13
#define HUGO_DICTTABLE 0x15
#define HUGO_SYNTABLE 0x17

#include "treaty_builder.h"
#include <ctype.h>

static int is_uppercase_hexadecimal(char c)
{
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))
        return 1;
    return 0;
}

static int number_of_hexadecimals_before_hyphen(char *s, size_t len)
{
    size_t offset = 0;

    while (offset < len && is_uppercase_hexadecimal(s[offset]))
        offset++;

    if (offset == len || s[offset] == '-')
        return offset;

    return 0;
}

/* We look for the pattern XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX (8-4-4-4-12)
 * where X is a number or A-F
 */
static int isUUID(char *s)
{
    if (!(number_of_hexadecimals_before_hyphen(s, 9) == 8))
        return 0;
    if (!(number_of_hexadecimals_before_hyphen(s + 9, 5) == 4))
        return 0;
    if (!(number_of_hexadecimals_before_hyphen(s + 14, 5) == 4))
        return 0;
    if (!(number_of_hexadecimals_before_hyphen(s + 19, 5) == 4))
        return 0;
    if (!(number_of_hexadecimals_before_hyphen(s + 24, 12) == 12))
        return 0;
    return 1;
}

/* The Hugo text obfuscation adds 20 to every character */
static char hugo_decode(char c)
{
    int decoded_char = c - 20;
    if (decoded_char < 0)
        decoded_char = decoded_char + 256;
    return (char)decoded_char;
}

static uint32 read_hugo_addx(unsigned char *from)
{
    return ((uint32)from[0]) | ((uint32)from[1] << 8);
}

static int32 get_story_file_IFID(void *s_file, int32 extent, char *output,
                                 int32 output_extent)
{
    size_t i, j;
    char ser[9];
    char buffer[32];
    char UUID_candidate[37];
    char hyphen = '-' + 20;

    char *story_file = (char *)s_file;

    if (extent < HUGO_CODESTART)
        return INVALID_STORY_FILE_RV;

    long dicttable = read_hugo_addx((unsigned char *)story_file + HUGO_DICTTABLE) * 16;

    for (i = dicttable + 8; i < extent - 28; i++)
    {
        /* First we look for an obfuscated hyphen, '-' + 20
         * We need to look 8 characters behind and 28 ahead
         */
        if (story_file[i] == hyphen &&
            i >= 8 && extent - i >= 28 &&
            story_file[i + 5] == hyphen &&
            story_file[i + 10] == hyphen &&
            story_file[i + 15] == hyphen)
        {
            for (j = 0; j < 36; j++)
            {
                /* One Hugo game, PAXLess, uses lowercase letters in its IFID.
                 * We make it uppercase here.
                 */
                UUID_candidate[j] = toupper(hugo_decode(story_file[i + j - 8]));
            }
            if (isUUID(UUID_candidate))
            {
                /* Found valid UUID at file offset i - 8 */
                ASSERT_OUTPUT_SIZE((signed)37);
                memcpy(output, UUID_candidate, 36);
                output[36] = '\0';
                return 1;
            }
        }
    }

    /* Found no UUID in file. Construct legacy IFID */

    memcpy(ser, (char *)story_file + 0x03, 8);
    ser[8] = 0;

    for (j = 0; j < 8; j++)
        if (!isalnum(ser[j]))
            ser[j] = '-';

    snprintf(buffer, sizeof(buffer), "HUGO-%d-%02X-%02X-%s", story_file[0], story_file[1],
             story_file[2], ser);

    ASSERT_OUTPUT_SIZE((signed)strlen(buffer) + 1);
    strcpy((char *)output, buffer);
    return 1;
}

static int32 claim_story_file(void *story_file, int32 exten)
{
    unsigned char *sf = (unsigned char *)story_file;
    int32 i;
    int32 scale;
    uint32 extent = (uint32)exten;

    if (!story_file || extent < 0x28) return  INVALID_STORY_FILE_RV;

    /* 39 is the largest version currently accepted by the Hugo interpreter:
     * https://github.com/garglk/garglk/blob/master/terps/hugo/source/hemisc.c#L1310-L1362
     */
    if (sf[0] > 39) return INVALID_STORY_FILE_RV;

    if (sf[0] < 34) scale = 4;
    else scale = 16;
    for (i = 3; i < HUGO_CODESTART; i++) if (sf[i] < 0x20 || sf[i] > 0x7e)
        return INVALID_STORY_FILE_RV;
    for (i = HUGO_CODESTART; i < 0x18; i += 2)
        if (read_hugo_addx(sf + i) * scale > extent) return INVALID_STORY_FILE_RV;

    long objtable = read_hugo_addx(sf + HUGO_OBJTABLE) * scale;
    long proptable = read_hugo_addx(sf + HUGO_PROPTABLE) * scale;
    if (proptable <= objtable)
    {
        /* proptable is not after objtable, not a valid Hugo file */
        return INVALID_STORY_FILE_RV;
    }
    long eventtable = read_hugo_addx(sf + HUGO_EVENTTABLE) * scale;
    if (eventtable <= proptable)
    {
        /* eventtable is not after proptable, not a valid Hugo file */
        return INVALID_STORY_FILE_RV;
    }
    long arraytable = read_hugo_addx(sf + HUGO_ARRAYTABLE) * scale;
    if (arraytable <= eventtable)
    {
        /* arraytable is not after eventtable, not a valid Hugo file */
        return INVALID_STORY_FILE_RV;
    }
    long syntable = read_hugo_addx(sf + HUGO_SYNTABLE) * scale;
    if (syntable <= arraytable)
    {
        /* syntable is not after arraytable, not a valid Hugo file */
        return INVALID_STORY_FILE_RV;
    }
    long dicttable  = read_hugo_addx(sf + HUGO_DICTTABLE) * scale;
    if (dicttable <= syntable)
    {
        /* dicttable is not after syntable, not a valid Hugo file */
        return INVALID_STORY_FILE_RV;
    }

    return VALID_STORY_FILE_RV;
}
