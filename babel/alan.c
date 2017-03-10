/* alan.c  Treaty of Babel module for ALAN files
 * 2006 By L. Ross Raszewski
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT alan
#define HOME_PAGE "http://www.alanif.se/"
#define FORMAT_EXT ".acd"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdio.h>

static int32 read_alan_int(unsigned char *from)
{
 return ((unsigned long int) from[3])| ((unsigned long int)from[2] << 8) |
       ((unsigned long int) from[1]<<16)| ((unsigned long int)from[0] << 24);
}
static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{

 if (story_file || extent) { }
 ASSERT_OUTPUT_SIZE(6);
 strcpy(output,"ALAN-");
 return INCOMPLETE_REPLY_RV;
}
/*
  The claim algorithm for Alan files is:
   * For Alan 3, check for the magic word
   * load the file length in blocks
   * check that the file length is correct
   * For alan 2, each word between byte address 24 and 81 is a
      word address within the file, so check that they're all within
      the file
   * Locate the checksum and verify that it is correct
*/
static int32 claim_story_file(void *story_file, int32 extent)
{
 unsigned char *sf = (unsigned char *) story_file;
 int32 bf, i, crc=0;
 if (extent < 160) return INVALID_STORY_FILE_RV;
 if (memcmp(sf,"ALAN",4))
 { /* Identify Alan 2.x */
 bf=read_alan_int(sf+4);
 if (bf < 0 || bf > extent/4) return INVALID_STORY_FILE_RV;
 for (i=24;i<81;i+=4)
 if (read_alan_int(sf+i) > extent/4) return INVALID_STORY_FILE_RV;
 for (i=160;i<(bf*4);i++)
 crc+=sf[i];
 if (crc!=read_alan_int(sf+152)) return INVALID_STORY_FILE_RV;
 return VALID_STORY_FILE_RV;
 }
 else
 { /* Identify Alan 3 */
//   bf=read_alan_int(sf+12);
//   if (bf < 0 || bf > (extent/4)) return INVALID_STORY_FILE_RV;
//   for (i=184;i<(bf*4);i++)
//    crc+=sf[i];
// if (crc!=read_alan_int(sf+176)) return INVALID_STORY_FILE_RV;
return VALID_STORY_FILE_RV;
 }
 return INVALID_STORY_FILE_RV;
}
