/* scott.c  Treaty of Babel module for ScottFree files
 * GPL license.
 *
 * This file depends on treaty_builder.h
 */

#define FORMAT scott
#define HOME_PAGE "https://github.com/cspiegel/scottfree-glk"
#define FORMAT_EXT ".dat,.saga,.sna,.tzx,.tap,.z80,.d64,.t64,.dsk,.fiad"
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

struct scottrec {
    int32 length;
    uint16_t chk;
    char *ifid;
};

static struct scottrec scott_registry[] = {
    { 0x44cd, 0x8b8f, "1A16C10E265A260429FD11B33E975017" }, // Pirate Adventure z80
    { 0x432c, 0xa6df, "2D24B3D60A4605641C204C23511121AA" }, // Voodoo Castle z80
    { 0x431e, 0x7437, "BA821285F91F5F59A6DFDDD7999F7F75" }, // Strange Odyssey z80
    { 0x4024, 0x62c7, "13EA7A22731E90598456D13311923833" }, // Buckaroo Banzai z80
    { 0x3ff8, 0x5fb4, "13EA7A22731E90598456D13311923833" }, // Buckaroo Banzai z80 alt

    { 0x7d3c, 0x142b, "186efec3-b22e-49af-b751-6b84f04b6957" }, // The Golden Baton Z80
   { 0x2ab00, 0x9dca, "186efec3-b22e-49af-b751-6b84f04b6957" }, // The Golden Baton C64, D64
    { 0x5170, 0xb240, "186efec3-b22e-49af-b751-6b84f04b6957" }, // The Golden Baton C64, T64
    { 0x78d0, 0x3db6, "59121f68-6194-4b26-975a-b1b98598930c" }, // The Time Machine Z80
    { 0x5032, 0x5635, "59121f68-6194-4b26-975a-b1b98598930c" }, // The Time Machine C64
    { 0x8539, 0xad02, "2f527feb-1c68-4e83-8ec7-062f5f3de3a3" }, // Arrow of Death part 1 z80
    { 0x5b46, 0x92db, "2f527feb-1c68-4e83-8ec7-062f5f3de3a3" }, // Arrow of Death part 1 C64
    { 0x949b, 0xb7e2, "e1161865-90af-44c0-a706-acf069d5327a" }, // Arrow of Death part 2 z80
    { 0x5fe2, 0xe14f, "e1161865-90af-44c0-a706-acf069d5327a" }, // Arrow of Death part 2 C64
    { 0x7509, 0x497d, "5b92a1fd-03f9-43b3-8d93-f8034ae02534" }, // Escape from Pulsar 7 z80
    { 0x46bf, 0x1679, "5b92a1fd-03f9-43b3-8d93-f8034ae02534" }, // Escape from Pulsar 7 C64
    { 0x6c62, 0xd002, "38a97e57-a465-4fb6-baad-835973b64b3a" }, // Circus z80
    { 0x4269, 0xa449, "38a97e57-a465-4fb6-baad-835973b64b3a" }, // Circus C64
    { 0x9213, 0xf3ca, "18f3167b-93c9-4323-8195-7b9166c9a304" }, // Feasability Experiment z80
    { 0x5a7b, 0x0f48, "18f3167b-93c9-4323-8195-7b9166c9a304" }, // Feasability Experiment C64
    { 0x83d9, 0x1bb9, "258b5da1-180c-48be-928e-add508478301" }, // The Wizard of Akyrz z80
   { 0x2ab00, 0x6cca, "258b5da1-180c-48be-928e-add508478301" }, // The Wizard of Akyrz C64, D64
    { 0x4be1, 0x5a00, "258b5da1-180c-48be-928e-add508478301" }, // The Wizard of Akyrz C64, T64
    { 0x7b10, 0x4f06, "0fa37e50-60ba-48eb-97ac-f4379a5f0096" }, // Perseus and Andromeda.z80
    { 0x502b, 0x913b, "0fa37e50-60ba-48eb-97ac-f4379a5f0096" }, // Perseus and Andromeda C64
    { 0x7bb0, 0x3877, "c25e70bd-120a-4cfd-95b7-60d2faca026b" }, // Ten Little Indians z80
    { 0x4f9f, 0xe6c8, "c25e70bd-120a-4cfd-95b7-60d2faca026b" }, // Ten Little Indians C64
    { 0x7f96, 0x4f96, "c3753c92-a85e-4bf1-8578-8b3623be8fca" }, // Waxworks z80
    { 0x4a11, 0xa37a, "c3753c92-a85e-4bf1-8578-8b3623be8fca" }, // Waxworks C64

   { 0x2ab00, 0xc3fc, "186efec3-b22e-49af-b751-6b84f04b6957" }, // Mysterious Adventures C64 dsk 1
   { 0x2ab00, 0xbfbf, "186efec3-b22e-49af-b751-6b84f04b6957" }, // Mysterious Adventures C64 dsk 1 alt
   { 0x2ab00, 0x9eaa, "18f3167b-93c9-4323-8195-7b9166c9a304" }, // Mysterious Adventures C64 dsk 2
    { 0x2ab00, 0x9c18, "18f3167b-93c9-4323-8195-7b9166c9a304" }, // Mysterious Adventures C64 dsk 2

    { 0xbcd5, 0xae3f, "EEC3C968F850EDF00BC8A80BB3D69FF0" }, // Questprobe 1 - The Hulk.tzx
    { 0xbc1e, 0xb4b6, "EEC3C968F850EDF00BC8A80BB3D69FF0" }, // Questprobe 1 - The Hulk (Americana).tzx
    { 0xafba, 0xb69b, "EEC3C968F850EDF00BC8A80BB3D69FF0" }, // Questprobe 1 - The Hulk Z80
    { 0x2ab00, 0xcdd8, "EEC3C968F850EDF00BC8A80BB3D69FF0" }, // Questprobe 1 - The Hulk C64 (D64)

    { 0xbaef, 0x52cc, "3B1E4CB60F0063B49245B8D7C32DEE1E" }, // Adventureland.tzx
    { 0xbb84, 0x6d93, "3B1E4CB60F0063B49245B8D7C32DEE1E" }, // Adventureland - Alternate.tzx
    { 0xba74, 0x39c2, "3B1E4CB60F0063B49245B8D7C32DEE1E" }, // Adventureland TAP image
    { 0xaaae, 0x7cf6, "3B1E4CB60F0063B49245B8D7C32DEE1E" }, // Adventureland Z80
    { 0x6a10, 0x1910, "3B1E4CB60F0063B49245B8D7C32DEE1E" }, // Adventureland C64 (T64)
    { 0x6a10, 0x1b10, "3B1E4CB60F0063B49245B8D7C32DEE1E" }, // Adventureland C64 (T64) alt
   { 0x2ab00, 0x6638, "3B1E4CB60F0063B49245B8D7C32DEE1E" }, // Adventureland C64 (D64)
   { 0x2adab, 0x751f, "3B1E4CB60F0063B49245B8D7C32DEE1E" }, // Adventureland C64 (D64) alt
   { 0x2adab, 0x64a4, "3B1E4CB60F0063B49245B8D7C32DEE1E" }, // Adventureland C64 (D64) alt 2

    { 0xbae1, 0x0ec0, "2E50256FA2717A4AF3402E6CE18F623F" }, // Secret Mission.tzx
    { 0xbaa7, 0xfc85, "2E50256FA2717A4AF3402E6CE18F623F" }, // Secret Mission TAP image
    { 0x8723, 0xc6da, "2E50256FA2717A4AF3402E6CE18F623F" }, // Secret Mission Z80
    { 0x88be, 0xa122, "2E50256FA2717A4AF3402E6CE18F623F" }, // Secret Mission  C64 (T64)
   { 0x2ab00, 0x04d6, "2E50256FA2717A4AF3402E6CE18F623F" }, // Secret Mission  C64 (D64)

    { 0xbae1, 0x83e9, "B5AF6E4DB3C3B2118FAEA3849F807617" }, // Sorcerer Of Claymorgue Castle.tzx
    { 0xbc2e, 0x4d84, "B5AF6E4DB3C3B2118FAEA3849F807617" }, // Sorcerer Of Claymorgue Castle - Alternate.tzx
    { 0xbaa7, 0x7371, "B5AF6E4DB3C3B2118FAEA3849F807617" }, // Sorcerer Of Claymorgue Castle TAP image
    { 0x726c, 0x0d57, "B5AF6E4DB3C3B2118FAEA3849F807617" }, // Sorcerer Of Claymorgue Castle Z80
    { 0x7298, 0x77fb, "B5AF6E4DB3C3B2118FAEA3849F807617" }, // Sorcerer Of Claymorgue Castle Z80 alt
    { 0x6ff7, 0xe4ed, "B5AF6E4DB3C3B2118FAEA3849F807617" }, // Sorcerer Of Claymorgue Castle C64 (T64)
    { 0x912f, 0xa69f, "B5AF6E4DB3C3B2118FAEA3849F807617" }, // Sorcerer Of Claymorgue Castle C64 (T64) alt
    { 0xc0dd, 0x3701, "B5AF6E4DB3C3B2118FAEA3849F807617" }, // Sorcerer Of Claymorgue Castle C64 (T64) alt 2
    { 0xbc5f, 0x492c, "B5AF6E4DB3C3B2118FAEA3849F807617" }, // Sorcerer Of Claymorgue Castle C64 (T64) alt 3
   { 0x2ab00, 0xfd67, "B5AF6E4DB3C3B2118FAEA3849F807617" }, // Sorcerer Of Claymorgue Castle C64 (D64)

    { 0xb36e, 0xbe5d, "DAEE386546CE71831DC365B0FF10F233" }, // Questprobe 2 - Spiderman.tzx
    { 0xb280, 0x196d, "DAEE386546CE71831DC365B0FF10F233" }, // Questprobe 2 - Spiderman - Alternate.tzx
    { 0xb36e, 0xbf88, "DAEE386546CE71831DC365B0FF10F233" }, // Questprobe 2 - Spiderman (Americana).tzx
    { 0xa4c4, 0x1ae8, "DAEE386546CE71831DC365B0FF10F233" }, // Spiderman Z80
    { 0xb316, 0x3c3c, "DAEE386546CE71831DC365B0FF10F233" }, // Spiderman TAP image
    { 0x08e72, 0xb2f4, "DAEE386546CE71831DC365B0FF10F233" }, // Spiderman C64 (T64)
   { 0x2ab00, 0xde56, "DAEE386546CE71831DC365B0FF10F233" }, // Spiderman C64 (D64)

    { 0x9e51, 0x8be7, "E247F4152EC664464BD9A6C0A092E05B" }, // Savage Island Part 1.tzx
    { 0x9e46, 0x7792, "E247F4152EC664464BD9A6C0A092E05B" }, // Savage Island Part 1 - Alternate.tzx
   { 0x11886, 0xd588, "E247F4152EC664464BD9A6C0A092E05B" }, // Savage Island.tzx (Contains both parts, but ScottFree will only play the first)
    { 0x9d6a, 0x41d6, "E247F4152EC664464BD9A6C0A092E05B" }, // Savage Island TAP image
    { 0x9ab2, 0xfaaa, "E247F4152EC664464BD9A6C0A092E05B" }, // Savage Island Z80 image
   { 0x2ab00, 0xc361, "E247F4152EC664464BD9A6C0A092E05B" }, // Savage Island C64 (D64)
   { 0x2ab00, 0x8801, "E247F4152EC664464BD9A6C0A092E05B" }, // Savage Island C64 (D64) alt

    { 0x9d9e, 0x4d76, "87EAA235B329356EF902933F6B8A1141" }, // Savage Island Part 2.tzx
    { 0x9e59, 0x79f4, "87EAA235B329356EF902933F6B8A1141" }, // Savage Island Part 2 - Alternate.tzx
    { 0x9d6a, 0x3de4, "87EAA235B329356EF902933F6B8A1141" }, // Savage Island 2 TAP image

    { 0xbae8, 0x6d7a, "D0723E8E-9C03-4DEF-BE4F-A4B594AD930A" }, // Gremlins - The Adventure.tzx
    { 0xbc36, 0x47cd, "D0723E8E-9C03-4DEF-BE4F-A4B594AD930A" }, // Gremlins The Adventure - Alternate.tzx
    { 0xbacc, 0x69fa, "D0723E8E-9C03-4DEF-BE4F-A4B594AD930A" }, // Gremlins TAP image
    { 0xb114, 0x8a72, "D0723E8E-9C03-4DEF-BE4F-A4B594AD930A" }, // Gremlins z80 image
    { 0xbbb6, 0x54cb, "AA9FF03F-FBE3-4767-8ACD-29FBFDCD3A91" }, // Gremlins - The Adventure (German).tzx
    { 0xc003, 0x558c, "AA9FF03F-FBE3-4767-8ACD-29FBFDCD3A91" }, // German Gremlins C64 (T64) version
    { 0xdd94, 0x25a8, "AA9FF03F-FBE3-4767-8ACD-29FBFDCD3A91" }, // German Gremlins C64 (T64) version alt
   { 0x2ab00, 0x6729, "AA9FF03F-FBE3-4767-8ACD-29FBFDCD3A91" }, // German Gremlins C64 (D64) version
    { 0xbb63, 0x3cc4, "C9C5B722-D87E-46B4-9092-4DCA978A4668" }, // Gremlins - The Adventure (Spanish).tzx
    { 0xbb63, 0x3ec7, "C9C5B722-D87E-46B4-9092-4DCA978A4668" }, // Gremlins (Spanish) alt.tzx
    { 0xbab4, 0x05a6, "C9C5B722-D87E-46B4-9092-4DCA978A4668" }, // Gremlins Spanish TAP image
   { 0x2ab00, 0xc402, "D0723E8E-9C03-4DEF-BE4F-A4B594AD930A" }, // Gremlins C64 (D64) version
   { 0x2ab00, 0xabf8, "D0723E8E-9C03-4DEF-BE4F-A4B594AD930A" }, // Gremlins C64 (D64) version alt
   { 0x2ab00, 0xa265, "D0723E8E-9C03-4DEF-BE4F-A4B594AD930A" }, // Gremlins C64 (D64) version alt 2
   { 0x2ab00, 0x3ccf, "D0723E8E-9C03-4DEF-BE4F-A4B594AD930A" }, // Gremlins C64 (D64) version alt 3

    { 0xba60, 0x3734, "75EE0452-0A6A-4100-9185-A79316812E0B" }, // Super Gran - The Adventure.tzx
    { 0xba71, 0x2d56, "75EE0452-0A6A-4100-9185-A79316812E0B" }, // Super Gran - Alternate.tzx
    { 0xba71, 0x2dc3, "75EE0452-0A6A-4100-9185-A79316812E0B" }, // Super Gran - Alternate 2.tzx
    { 0x9df0, 0x2238, "75EE0452-0A6A-4100-9185-A79316812E0B" }, // Super Gran z80 snap
    { 0x726f, 0x0901, "75EE0452-0A6A-4100-9185-A79316812E0B" }, // Super Gran C64 (T64)

    { 0xc581, 0x940f, "E8021308-8719-4A34-BDF9-C6F388129E53" }, // Robin Of Sherwood.tzx
    { 0xc581, 0x940f, "E8021308-8719-4A34-BDF9-C6F388129E53" }, // Robin Of Sherwood - Alternate.tzx
    { 0xae0c, 0x4661, "E8021308-8719-4A34-BDF9-C6F388129E53" }, // Robin Of Sherwood TAP image
    { 0xc495, 0x4d3c, "E8021308-8719-4A34-BDF9-C6F388129E53" }, // Robin Of Sherwood TAP image alt
    { 0xace8, 0x991c, "E8021308-8719-4A34-BDF9-C6F388129E53" }, // Robin Of Sherwood z80 snap
   { 0x2ab00, 0xcf9e, "E8021308-8719-4A34-BDF9-C6F388129E53" }, // Robin Of Sherwood C64 (D64)
    { 0xb2ef, 0x7c44, "E8021308-8719-4A34-BDF9-C6F388129E53" }, // Robin Of Sherwood C64 (T64)
    { 0x8db6, 0x7853, "E8021308-8719-4A34-BDF9-C6F388129E53" }, // Robin Of Sherwood C64 (T64) alt
    { 0xb690, 0x7b61, "E8021308-8719-4A34-BDF9-C6F388129E53" }, // Robin Of Sherwood C64 (T64) alt 2

    { 0xbc60, 0xce3d, "8A23C0CB-2DB3-4A19-A87E-E511477D2CDB" }, // Seas Of Blood.tzx
    { 0xbd61, 0x0277, "8A23C0CB-2DB3-4A19-A87E-E511477D2CDB" }, // Seas Of Blood - Alternate.tzx
    { 0xbc26, 0xbe47, "8A23C0CB-2DB3-4A19-A87E-E511477D2CDB" }, // Seas of Blood TAP image
    { 0xb4b1, 0x7ec8, "8A23C0CB-2DB3-4A19-A87E-E511477D2CDB" }, // Seas of Blood z80 snap
    { 0xa209, 0xf115, "8A23C0CB-2DB3-4A19-A87E-E511477D2CDB" }, // Seas of Blood C64 (T64)
   { 0x2ab00, 0x5c1d, "8A23C0CB-2DB3-4A19-A87E-E511477D2CDB" }, // Seas of Blood C64 (D64)
   { 0x2ab00, 0xe308, "8A23C0CB-2DB3-4A19-A87E-E511477D2CDB" }, // Seas of Blood C64 (D64) alt
    { 0, 0, NULL }
};

static struct scottrec TI994A_registry[] = {
    { 0xff, 0x8618, "3B1E4CB60F0063B49245B8D7C32DEE1E" }, // Adventureland, TI-99/4A version
    { 0xff, 0x9333, "1A16C10E265A260429FD11B33E975017" }, // Pirate Adventure, TI-99/4A version
    { 0xff, 0x9f67, "2E50256FA2717A4AF3402E6CE18F623F" }, // Secret Mission, TI-99/4A version
    { 0xff, 0x96b7, "2D24B3D60A4605641C204C23511121AA" }, // Voodoo Castle, TI-99/4A version
    { 0xff, 0x9095, "34894B343CAACE66763AE40114A1C12F" }, // The Count, TI-99/4A version
    { 0xff, 0x9ae3, "BA821285F91F5F59A6DFDDD7999F7F75" }, // Strange Odyssey, TI-99/4A version
    { 0xff, 0x90ce, "CE3AEE5F23E49107BEB2E333C29F2AFC" }, // Mystery Fun House, TI-99/4A version
    { 0xff, 0x7d03, "BB7E4E7CB57249E7671E7DE081D715B7" }, // Pyramid of Doom, TI-99/4A version
    { 0xff, 0x8dde, "F0D164D13861E3FB074B9B0BAC810777" }, // Ghost Town, TI-99/4A version
    { 0xff, 0x9bc6, "E247F4152EC664464BD9A6C0A092E05B" }, // Savage Island part I, TI-99/4A version
    { 0xff, 0xa6d2, "87EAA235B329356EF902933F6B8A1141" }, // Savage Island part II, TI-99/4A version
    { 0xff, 0x7f47, "707015A14ADCE841F1E1DCC90C847BBA" }, // The Golden Voyage, TI-99/4A version
    { 0xff, 0x882d, "B5AF6E4DB3C3B2118FAEA3849F807617" }, // The Sorcerer Of Claymorgue Castle, TI-99/4A version

    { 0xff, 0x80e9, "13EA7A22731E90598456D13311923833" }, // The Adventures of Buckaroo Banzai, TI-99/4A version

    { 0xff, 0x80c0, "EEC3C968F850EDF00BC8A80BB3D69FF0" }, // Questprobe 1 - The Hulk, TI-99/4A version
    { 0xff, 0x884b, "DAEE386546CE71831DC365B0FF10F233" }, // Questprobe 2 - Spider-Man, TI-99/4A version

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

    for (int i = 0; scott_registry[i].length != 0; i++) {
        if (extent == scott_registry[i].length &&
            chksum == scott_registry[i].chk) {
            if (ifid != NULL) {
                size_t length = strlen(scott_registry[i].ifid);
                strncpy(*ifid, scott_registry[i].ifid, length);
                (*ifid)[length] = 0;
            }
            return VALID_STORY_FILE_RV;
        }
    }
    return INVALID_STORY_FILE_RV;
}

static int32 find_in_TI994Adatabase(uint16_t chksum, char **ifid) {
    for (int i = 0; TI994A_registry[i].length != 0; i++) {
        if (chksum == TI994A_registry[i].chk) {
            if (ifid != NULL) {
                strncpy(*ifid, TI994A_registry[i].ifid, 32);
                (*ifid)[32] = '\0';
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

static int detect_ti994a(unsigned char *sf, int32 extent) {
    return find_code("\x30\x30\x30\x30\x00\x30\x30\x00\x28\x28", 10, sf, extent);
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

    fprintf(stderr, "The length of this file is %x, and its checksum %x\n", extent, checksum(storystring, extent));

    if (extent < 24 || extent > 300000)
        return INVALID_STORY_FILE_RV;

    if (detect_scottfree(storystring, extent) == VALID_STORY_FILE_RV)
        return VALID_STORY_FILE_RV;

    if (detect_ti994a(storystring, extent) != -1)
        return VALID_STORY_FILE_RV;

    if (find_in_database(storystring, extent, NULL) == VALID_STORY_FILE_RV)
        return VALID_STORY_FILE_RV;

    return INVALID_STORY_FILE_RV;
}

static int32 get_story_file_IFID(void *storyvp, int32 extent, char *output, int32 output_extent)
{
    ASSERT_OUTPUT_SIZE(37);

    unsigned char *storystring = (unsigned char *)storyvp;

    int ti99offset = detect_ti994a(storystring, extent);

    if (ti99offset != -1) {
        int title_screen_offset = ti99offset - 0x509;
        title_screen_offset = MAX(title_screen_offset, 0);
        /* Calculate checksum using 3 Kb after the title screen */
        int checksum_length = MIN(3072, extent - title_screen_offset);

        int sum = checksum(storyvp + title_screen_offset, checksum_length);

        if (find_in_TI994Adatabase(sum, &output) == VALID_STORY_FILE_RV) {
            return VALID_STORY_FILE_RV;
        }
    }

    if (find_in_database(storystring, extent, &output) == VALID_STORY_FILE_RV) {
        size_t length = strlen(output);
        for (int i = 0; i< length; i++)
            output[i] = toupper(output[i]);
        output[length] = 0;
        return VALID_STORY_FILE_RV;
    }

    if (detect_scottfree(storystring, extent) == VALID_STORY_FILE_RV
     || ti99offset != -1)
    {
        strcpy(output, "\0");
        return INCOMPLETE_REPLY_RV;
    }
    return INVALID_STORY_FILE_RV;
}
