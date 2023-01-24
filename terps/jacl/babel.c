/* jacl.c  Treaty of Babel module for hugo files
 * 2008 By Stuart Allen, based on code by L. Ross Raszewski (again...)
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT jacl2
#define HOME_PAGE "http://code.google.com/p/jacl/"
#define FORMAT_EXT ".j2"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"

#include <ctype.h>
#include <stdio.h>

static int32 get_story_file_IFID(void *s_file, int32 extent, char *output, int32 output_extent)
{

	char current_line[2048];

	char *ifid;

	/* TAKE A COPY OF THE FIRST 2000 BYTES THEN NULL-TERMINATE IT */
	strncpy (current_line, (char*) s_file, 2000);
	current_line[2000] = 0;

	ifid = strstr(current_line, "ifid:JACL-");
	
	if (ifid != NULL) {
		ifid += 5;
		*(ifid + 8) = 0;
 		ASSERT_OUTPUT_SIZE((signed) 9);
 		strcpy((char *)output, ifid);
 		return 1;
	}

	return NO_REPLY_RV; 
}

static int32 claim_story_file(void *story_file, int32 extent)
{

	char current_line[2048];

	/* TAKE A COPY OF THE FIRST 2000 BYTES THEN NULL-TERMINATE IT */
	strncpy (current_line, (char *) story_file, 2000);
	current_line[2000] = 0;

	if (strstr(current_line, "processed:2")) {
  		return VALID_STORY_FILE_RV;
	}
			
	return INVALID_STORY_FILE_RV;
}
