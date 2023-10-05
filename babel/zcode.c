/* zcode.c  Treaty of Babel module for Z-code files
 * 2006 By L. Ross Raszewski
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT zcode
#define HOME_PAGE "http://www.inform-fiction.org"
#define FORMAT_EXT ".z3,.z4,.z5,.z6,.z7,.z8,.dat,.data"
#define NO_METADATA
#define NO_COVER
#define CUSTOM_EXTENSION

#define MAX_LENGTH 368641
#define MIN_LENGTH 24

#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>

static const char *ifids[] = {
    NULL,
    "ZCODE-383-890602", // Zork 0
    "ZCODE-77-890616", // Journey
    "ZCODE-311-890510", // Shogun
    "ZCODE-63-890622" // Arthur
};

typedef enum {
    NO_IFID,
    ZORK0_IFID,
    JOURNEY_IFID,
    SHOGUN_IFID,
    ARTHUR_IFID
} ZIfidType;

struct zrec {
    int32 length;
    uint16_t chk;
    ZIfidType ifid;
};

static const struct zrec z6_registry[] = {
    { 0x39564, 0x8305, ZORK0_IFID }, // Zork Zero disk 1
    { 0x39564, 0xf794, ZORK0_IFID }, // Zork Zero disk 2
    { 0x39564, 0xcbb0, ZORK0_IFID }, // Zork Zero disk 3
    { 0x39564, 0xb5ad, ZORK0_IFID }, // Zork Zero disk 4

    { 0x3956f, 0xf833, JOURNEY_IFID }, // Journey disk 1
    { 0x3956f, 0x5435, JOURNEY_IFID }, // Journey disk 2
    { 0x3956f, 0xf529, JOURNEY_IFID }, // Journey disk 3
    { 0x3956f, 0xc184, JOURNEY_IFID }, // Journey disk 4
    { 0x3956f, 0xece8, JOURNEY_IFID }, // Journey disk 5

    { 0x39538, 0x4e89, SHOGUN_IFID }, // Shogun disk 1
    { 0x39538, 0x7e5f, SHOGUN_IFID }, // Shogun disk 2
    { 0x39538, 0x9457, SHOGUN_IFID }, // Shogun disk 3
    { 0x39538, 0x91ed, SHOGUN_IFID }, // Shogun disk 4
    { 0x39538, 0xdf5a, SHOGUN_IFID }, // Shogun disk 5

    { 0x39557, 0x5f18, ARTHUR_IFID }, // Arthur disk 1
    { 0x39557, 0x45be, ARTHUR_IFID }, // Arthur disk 2
    { 0x39557, 0x0dfe, ARTHUR_IFID }, // Arthur disk 3
    { 0x39557, 0xb3ec, ARTHUR_IFID }, // Arthur disk 4
    { 0x39557, 0x3bb1, ARTHUR_IFID }, // Arthur disk 5

    { 0, 0, NO_IFID }
};

static int iswoz(void *story_file) {
    uint8_t *chars = (uint8_t *)story_file;
    return (chars[0] == 'W' && chars[1] == 'O' && chars[2] == 'Z');
}

static uint16_t checksum(unsigned char *sf, int32 extent)
{
    uint16_t c=0;
    for(int i = 0; i < extent; i++)
        c+=sf[i];
    return c;
}

static int32 find_in_database(unsigned char *sf, int32 extent, char **ifid) {

    fprintf(stderr, "The length of this file is %x, and its checksum %x\n", extent, checksum(sf, extent));

    if (extent > MAX_LENGTH || extent < MIN_LENGTH)
        return INVALID_STORY_FILE_RV;

    int calculated_checksum = 0;
    uint16_t chksum;

    for (int i = 0; z6_registry[i].ifid != NO_IFID; i++) {
        if (extent == z6_registry[i].length) {
            if (calculated_checksum == 0) {
                chksum = checksum(sf, extent);
                calculated_checksum = 1;
            }
            if (chksum == z6_registry[i].chk) {
                size_t length = strlen(ifids[z6_registry[i].ifid]);
                if (ifid != NULL) {
                    strncpy(*ifid, ifids[z6_registry[i].ifid], length);
                    (*ifid)[length] = 0;
                }
                return VALID_STORY_FILE_RV;
            }
        }
    }
    return INVALID_STORY_FILE_RV;
}

static int32 get_woz_IFID(void *story_file, int32 extent, char *output, int32 output_extent) {
    return find_in_database(story_file, extent, &output);
}


static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{
   uint32 i,j;
   char ser[7];
   char buffer[32];

   if (extent<0x1D) return INVALID_STORY_FILE_RV;

   if (iswoz(story_file))
        return get_woz_IFID(story_file, extent, output, output_extent);

   memcpy(ser, (char *) story_file+0x12, 6);
   ser[6]=0;
   /* Detect vintage story files */
   if (!(ser[0]=='8' || ser[0]=='9' ||
         (ser[0]=='0' && ser[1]>='0' && ser[1]<='5')))
   {
      for(i=0;i<(uint32) extent-7;i++) if (memcmp((char *)story_file+i,"UUID://",7)==0) break;
      if (i<(uint32) extent) /* Found explicit IFID */
      {
         for(j=i+7;j<(uint32)extent && ((char *)story_file)[j]!='/';j++);
         if (j<(uint32) extent)
         {
            i+=7;
            ASSERT_OUTPUT_SIZE((int32) (j-i));
            memcpy(output,(char *)story_file+i,j-i);
            output[j-i]=0;
            return 1;
         }
      }
   }
   /* Did not find intact IFID.  Build one */
   i=((unsigned char *)story_file)[2] << 8 |((unsigned char *)story_file)[3];
   for(j=0;j<6;j++)
      if (!isalnum(ser[j])) ser[j]='-';

   j=((unsigned char *)story_file)[0x1C] << 8 |((unsigned char *)story_file)[0x1D];

   if (strcmp(ser,"000000") && isdigit(ser[0]) && ser[0]!='8')
      snprintf(buffer, 32, "ZCODE-%d-%s-%04X",i,ser,j);
   else
      snprintf(buffer, 32, "ZCODE-%d-%s",i,ser);

   ASSERT_OUTPUT_SIZE((signed) strlen(buffer)+1);
   strcpy((char *)output,buffer);
   return 1;

}

static uint32 read_zint(unsigned char *sf)
{
   return ((uint32)sf[0] << 8) | ((uint32) sf[1]);
}

static int32 claim_woz_story_file(void *story_file, int32 extent) {
    return find_in_database(story_file, extent, NULL);
}


static int32 claim_story_file(void *story_file, int32 extent)
{
    if (iswoz(story_file)) {
        return claim_woz_story_file(story_file, extent);
    }


   unsigned char *sf=(unsigned char *)story_file;
   uint32 i,j;
   if (extent<0x3c ||
       sf[0] < 1 ||
       sf[0] > 8
       ) return INVALID_STORY_FILE_RV;
   for(i=4;i<=14;i+=2)
   {
      j=read_zint(sf+i);
      if (j>(uint32) extent || j < 0x40) return INVALID_STORY_FILE_RV;
   }

   return VALID_STORY_FILE_RV;
}

static int32 get_story_file_extension(void *sf, int32 extent, char *out, int32 output_extent)
{
   int v;
   if (!extent) return INVALID_STORY_FILE_RV;
   v= ((char *) sf)[0];
   if (v>9) ASSERT_OUTPUT_SIZE(5);
   else ASSERT_OUTPUT_SIZE(4);
   sprintf(out,".z%d",v);
   return 3+(v>9);
}
