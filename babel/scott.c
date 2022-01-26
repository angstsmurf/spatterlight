/* scott.c  Treaty of Babel module for ScottFree files
 * GPL license.
 *
 * This file depends on treaty_builder.h
 */

#define FORMAT scott
#define HOME_PAGE "https://github.com/cspiegel/scottfree-glk"
#define FORMAT_EXT ".dat,.saga,.sna,.tzx,.tap,.z80"
#define NO_METADATA
#define NO_COVER

#define MAX_LENGTH 300000
#define MIN_LENGTH 24


#include "treaty_builder.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

struct scottrec {
    int32 length;
    uint16_t chk;
    char *ifid;
};


static struct scottrec scott_registry[] = {
    { 0xbcd5, 0xae3f, "9108AF63-67F2-4023-B730-E1BDC0606C3F" }, // Questprobe 1 - The Hulk.tzx
    { 0xbc1e, 0xb4b6, "9108AF63-67F2-4023-B730-E1BDC0606C3F" }, // Questprobe 1 - The Hulk (Americana).tzx
    { 0xbaef, 0x52cc, "D4021CA3-A208-4944-8283-434EB9C9661E" }, // Adventureland.tzx
    { 0xbb84, 0x6d93, "D4021CA3-A208-4944-8283-434EB9C9661E" }, // Adventureland - Alternate.tzx
    { 0xba74, 0x39c2, "D4021CA3-A208-4944-8283-434EB9C9661E" }, // Adventureland TAP image
    { 0xbae1, 0x0ec0, "FE8B6275-1F78-425A-BE64-242CC07CF448" }, // Secret Mission.tzx
    { 0xbaa7, 0xfc85, "FE8B6275-1F78-425A-BE64-242CC07CF448" }, // Secret Mission TAP image
    { 0xbae1, 0x83e9, "86438A1E-86F7-4F9E-8515-1FF673636B1B" }, // Sorcerer Of Claymorgue Castle.tzx
    { 0xbc2e, 0x4d84, "86438A1E-86F7-4F9E-8515-1FF673636B1B" }, // Sorcerer Of Claymorgue Castle - Alternate.tzx
    { 0xbaa7, 0x7371, "86438A1E-86F7-4F9E-8515-1FF673636B1B" }, // Sorcerer Of Claymorgue Castle TAP image
    { 0xb36e, 0xbe5d, "F2EE4920-846B-4F4D-8335-973E49D14402" }, // Questprobe 2 - Spiderman.tzx
    { 0xb280, 0x196d, "F2EE4920-846B-4F4D-8335-973E49D14402" }, // Questprobe 2 - Spiderman - Alternate.tzx
    { 0xb36e, 0xbf88, "F2EE4920-846B-4F4D-8335-973E49D14402" }, // Questprobe 2 - Spiderman (Americana).tzx
    { 0xb316, 0x3c3c, "F2EE4920-846B-4F4D-8335-973E49D14402" }, // Spiderman TAP image
    { 0x9e51, 0x8be7, "85387E91-2F9B-4E0D-8EBE-3292B3C62333" }, // Savage Island Part 1.tzx
    { 0x9e46, 0x7792, "85387E91-2F9B-4E0D-8EBE-3292B3C62333" }, // Savage Island Part 1 - Alternate.tzx
   { 0x11886, 0xd588, "85387E91-2F9B-4E0D-8EBE-3292B3C62333" }, // Savage Island.tzx (Contains both part, but ScottFree will only play the first)
    { 0x9d6a, 0x41d6, "85387E91-2F9B-4E0D-8EBE-3292B3C62333" }, // Savage Island TAP image
    { 0x9d9e, 0x4d76, "151CC0E0-F873-437C-9A04-2A121AC9499C" }, // Savage Island Part 2.tzx
    { 0x9e59, 0x79f4, "151CC0E0-F873-437C-9A04-2A121AC9499C" }, // Savage Island Part 2 - Alternate.tzx
    { 0x9d6a, 0x3de4, "151CC0E0-F873-437C-9A04-2A121AC9499C" }, // Savage Island 2 TAP image
    { 0xbae8, 0x6d7a, "D0723E8E-9C03-4DEF-BE4F-A4B594AD930A" }, // Gremlins - The Adventure.tzx
    { 0xbc36, 0x47cd, "D0723E8E-9C03-4DEF-BE4F-A4B594AD930A" }, // Gremlins The Adventure - Alternate.tzx
    { 0xbacc, 0x69fa, "D0723E8E-9C03-4DEF-BE4F-A4B594AD930A" }, // Gremlins TAP image
    { 0xb114, 0x8a72, "D0723E8E-9C03-4DEF-BE4F-A4B594AD930A" }, // Gremlins z80 image
    { 0xbbb6, 0x54cb, "AA9FF03F-FBE3-4767-8ACD-29FBFDCD3A91" }, // Gremlins - The Adventure (German).tzx
    { 0xbb63, 0x3cc4, "C9C5B722-D87E-46B4-9092-4DCA978A4668" }, // Gremlins - The Adventure (Spanish).tzx
    { 0xbb63, 0x3ec7, "C9C5B722-D87E-46B4-9092-4DCA978A4668" }, // Gremlins - The Adventure (Spanish) alt.tzx
    { 0xbab4, 0x05a6, "C9C5B722-D87E-46B4-9092-4DCA978A4668" }, // Gremlins Spanish TAP image
    { 0xba60, 0x3734, "75EE0452-0A6A-4100-9185-A79316812E0B" }, // Super Gran - The Adventure.tzx
    { 0xba71, 0x2d56, "75EE0452-0A6A-4100-9185-A79316812E0B" }, // Super Gran - Alternate.tzx
    { 0xba71, 0x2dc3, "75EE0452-0A6A-4100-9185-A79316812E0B" }, // Super Gran - The Adventure - Alternate.tzx
    { 0x9df0, 0x2238, "75EE0452-0A6A-4100-9185-A79316812E0B" }, // Super Gran z80 snap
    { 0xc581, 0x940f, "E8021308-8719-4A34-BDF9-C6F388129E53" }, // Robin Of Sherwood.tzx
    { 0xc581, 0x940f, "E8021308-8719-4A34-BDF9-C6F388129E53" }, // Robin Of Sherwood - Alternate.tzx
    { 0xae0c, 0x4661, "E8021308-8719-4A34-BDF9-C6F388129E53" }, // Robin Of Sherwood TAP image
    { 0xace8, 0x991c, "E8021308-8719-4A34-BDF9-C6F388129E53" }, // Robin Of Sherwood z80 snap
    { 0xbc60, 0xce3d, "8A23C0CB-2DB3-4A19-A87E-E511477D2CDB" }, // Seas Of Blood.tzx
    { 0xbd61, 0x0277, "8A23C0CB-2DB3-4A19-A87E-E511477D2CDB" }, // Seas Of Blood - Alternate.tzx
    { 0xbc26, 0xbe47, "8A23C0CB-2DB3-4A19-A87E-E511477D2CDB" }, // Seas of Blood TAP image
    { 0xb4b1, 0x7ec8, "8A23C0CB-2DB3-4A19-A87E-E511477D2CDB" }, // Seas of Blood z80 snap

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

    for (int i = 0; scott_registry[i].length != 0; i++) {
        if (extent == scott_registry[i].length &&
            checksum(sf, extent) == scott_registry[i].chk) {
            if (ifid != NULL) {
                strncpy(*ifid, scott_registry[i].ifid, 36);
            }
            return VALID_STORY_FILE_RV;
        }
    }
    return INVALID_STORY_FILE_RV;
}



/* All numbers in ScottFree text format files are stored as text delimited by whitespace */
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

static int32 detect_scottfree(unsigned char *storystring, int32 extent) {
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
            /* The word length is given in header[8], but many games have words exceeding this */
            /* in the dictionary. We use 10 characters as a reasonable upper limit */
            if (read_string(storystring, extent, &offset, &failure) > 10 || failure == true)
                return INVALID_STORY_FILE_RV;
        }
    }

    return VALID_STORY_FILE_RV;
}

static int32 claim_story_file(void *storyvp, int32 extent)
{
    unsigned char *storystring = (unsigned char *)storyvp;

    if (extent < 24)
        return INVALID_STORY_FILE_RV;

    if (detect_scottfree(storystring, extent) == VALID_STORY_FILE_RV)
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
        for (int i = 0; i<36; i++)
            output[i] = toupper(output[i]);
        output[36] = 0;
        return VALID_STORY_FILE_RV;
    }

    if (detect_scottfree(storystring, extent) == VALID_STORY_FILE_RV)
    {
        strcpy(output, "\0");
        return INCOMPLETE_REPLY_RV;
    }
    return INVALID_STORY_FILE_RV;
}
