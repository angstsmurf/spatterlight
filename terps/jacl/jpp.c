/* jpp.c --- The JACL Preprocessor
   (C) 2001 Andreas Matthias

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "jacl.h"
#include "language.h"
#include "types.h"
#include "prototypes.h"
#include "encapsulate.h"
#include <string.h>

static int			lines_written;

static FILE 	 	*outputFile = NULL;
static FILE   	    *inputFile = NULL;

static char			*stripped_line;

/* #include depth ceiling. Real games chain ~8 deep at most
 * (game.jacl -> verbs.library -> none); 32 is comfortably above
 * legitimate use. Without this, a circular include
 * (`#include "a"` in a, or a -> b -> a) recurses until process
 * stack exhaustion / SIGSEGV. */
#define JPP_MAX_INCLUDE_DEPTH 32
static int			include_depth = 0;

/* INDICATES THAT THE CURRENT '.j2' FILE BEING WORKED 
 * WITH BEING PREPARED FOR RELEASE (DON'T INCLUDE DEBUG LIBARIES) */
short int			release = FALSE;

/* INDICATES THAT THE CURRENT '.j2' FILE BEING WORKED 
 * SHOULD BE ENCRYPTED */
static short int	do_encrypt = TRUE;

/* INDICATES THAT THE CURRENT '.processed' FILE BRING WRITTEN SHOULD NOW
 * HAVE EACH LINE ENCRYPTED AS THE FIRST NONE COMMENT LINE HAS BEEN HIT */
static short int	encrypting = FALSE;

static int process_file(const char *sourceFile1, const char *sourceFile2);

int
jpp()
{
	int				game_version;

	lines_written = 0;

	/* CHECK IF GAME FILE IS ALREADY A PROCESSED FILE BY LOOKING FOR THE
	 * STRING "#encrypted" OR "#processed" WITHIN THE FIRST FIVE LINES OF 
	 * THE GAME FILE IF SO, RETURN THE GAME FILE AS THE PROCESSED FILE */
	if ((inputFile = fopen(game_file, "r")) != NULL) {
		int index = 0;
		char *result = NULL;

		result = fgets(text_buffer, 1024, inputFile);
		if (!result) {
			sprintf(error_buffer, CANT_OPEN_SOURCE, game_file);
			return (FALSE);
		}

		while (!feof(inputFile) && index < 10) {
			if (strstr(text_buffer, "#processed")) {
				/* THE GAME FILE IS ALREADY A PROCESSED FILE, JUST USE IT
				 * DIRECTLY */
				if (sscanf(text_buffer, "#processed:%d", &game_version) == 1) {
					if (INTERPRETER_VERSION < game_version) {
						sprintf (error_buffer, OLD_INTERPRETER, game_version);
						return (FALSE);
					}
				}
				strcpy(processed_file, game_file);
				
				return (TRUE);	
			}					
			result = fgets(text_buffer, 1024, inputFile);
			if (!result) {
				//return (FALSE);
				break;
			}
			index++;
		}

		fclose(inputFile);
	} else {
		sprintf (error_buffer, NOT_FOUND);
		return (FALSE);
	}

	/* SAVE A TEMPORARY FILENAME INTO PROCESSED_FILE. processed_file
	 * is 256 bytes (jacl.c globals); temp_directory + prefix could
	 * approach that, so snprintf instead of sprintf. */
	snprintf(processed_file, 256, "%s%s.j2", temp_directory, prefix);

	/* ATTEMPT TO OPEN THE PROCESSED FILE IN THE TEMP DIRECTORY */
	if ((outputFile = fopen(processed_file, "w")) == NULL) {
		/* NO LUCK, TRY OPEN THE PROCESSED FILE IN THE CURRENT DIRECTORY */
		snprintf(processed_file, 256, "%s.j2", prefix);
		if ((outputFile = fopen(processed_file, "w")) == NULL) {
			/* NO LUCK, CAN'T CONTINUE */
			sprintf(error_buffer, CANT_OPEN_PROCESSED, processed_file);
			return (FALSE);
		}
	}

	if (process_file(game_file, (char *) NULL) == FALSE) {
		return (FALSE);
	}

	fclose(outputFile);

	/* ALL OKAY, RETURN TRUE */
	return (TRUE);
}

int
process_file(const char *sourceFile1, const char *sourceFile2)
{
	char            temp_buffer1[1025];
	char            temp_buffer2[1025];
	FILE           *inputFile = NULL;
	char           *includeFile = NULL;

	/* Reject circular / runaway includes. The caller manages
	 * include_depth around the recursive call; here we just
	 * refuse to descend further. */
	if (include_depth >= JPP_MAX_INCLUDE_DEPTH) {
		snprintf(error_buffer, 1024,
			"Include depth exceeded %d while processing '%s'; possible cycle.",
			JPP_MAX_INCLUDE_DEPTH, sourceFile1);
		return (FALSE);
	}

	/* THIS FUNCTION WILL CREATE A PROCESSED FILE THAT HAS HAD ALL
	 * LEADING AND TRAILING WHITE SPACE REMOVED AND ALL INCLUDED
	 * FILES INSERTED */
	if ((inputFile = fopen(sourceFile1, "r")) == NULL) {
		if (sourceFile2 != NULL) {
			if ((inputFile = fopen(sourceFile2, "r")) == NULL) {
				sprintf(error_buffer, CANT_OPEN_OR, sourceFile1, sourceFile2);
				return (FALSE);
			}

		} else {
			sprintf(error_buffer, CANT_OPEN_SOURCE, sourceFile1);
			return (FALSE);
		}
	}

	*text_buffer = 0;

	if (fgets(text_buffer, 1024, inputFile) == NULL) {
		sprintf (error_buffer, READ_ERROR);
		fclose(inputFile);
		return (FALSE);
	}

	while (!feof(inputFile) || *text_buffer != 0) {
		if (!strncmp(text_buffer, "#include", 8) ||
		   (!strncmp(text_buffer, "#debug", 6) && !release)) {
			includeFile = strrchr(text_buffer, '"');

			if (includeFile != NULL)
				*includeFile = 0;

			includeFile = strchr(text_buffer, '"');

			if (includeFile != NULL) {
				/* Bound path assembly: game_path (up to 255 chars per
				 * cgijacl globals) plus an include name read from the
				 * source line (up to ~1023 chars after the leading
				 * quote) easily overflows temp_buffer1[1025] with
				 * strcpy+strcat. snprintf truncates cleanly instead. */
				snprintf(temp_buffer1, sizeof(temp_buffer1),
					 "%s%s", game_path, includeFile + 1);
				snprintf(temp_buffer2, sizeof(temp_buffer2),
					 "%s%s", include_directory, includeFile + 1);
				include_depth++;
				if (process_file(temp_buffer1, temp_buffer2) == FALSE) {
					include_depth--;
					fclose(inputFile);
					return (FALSE);
				}
				include_depth--;
			} else {
				sprintf (error_buffer, BAD_INCLUDE);
				fclose(inputFile);
				return (FALSE);
			}
		} else {
			/* STRIP WHITESPACE FROM LINE BEFORE WRITING TO OUTPUTFILE. */
			stripped_line = stripwhite(text_buffer);

			if (!encrypting && *stripped_line != '#' && *stripped_line != '\0' && do_encrypt && release) {
				/* START ENCRYPTING FROM THE FIRST NON-COMMENT LINE IN
				 * THE SOURCE FILE */
				fputs("#encrypted\n", outputFile);
				encrypting = TRUE;
			} 

			/* ENCRYPT PROCESSED FILE IF REQUIRED */
			if (encrypting) {
				jacl_obfuscate(stripped_line);
			}

			fputs(stripped_line, outputFile);

			lines_written++;
			if (lines_written == 1) {
				sprintf(temp_buffer, "#processed:%d\n", INTERPRETER_VERSION);
				fputs(temp_buffer, outputFile);
			}
		}

		*text_buffer = 0;

		if (fgets(text_buffer, 1024, inputFile) == NULL) {
			// EOF HAS BEEN REACHED
			break;
		}
	}

	fclose(inputFile);

	/* ALL OKAY, RETURN TRUE */
	return (TRUE);
}
