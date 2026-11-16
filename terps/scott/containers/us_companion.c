/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  us_companion.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Finds the other disk or file of a US game that comes in two parts, from
 *  tables of known file name pairs.
 */

#include <stdlib.h>
#include <string.h>

#include "scott.h"
#include "detect_game.h"

#include "us_companion.h"

/* Compare two filenames backward from the end, ignoring file extensions.
   This handles the case where game files may live in different directories
   (different path prefixes) but share the same base name. Compares only
   as many characters as the shorter base name has. Returns 1 if they match. */
static int CompareFilenames(const char *str1, size_t length1, const char *str2, size_t length2)
{
    /* Strip file extensions by scanning backward for '.' */
    while (length1 > 0 && str1[length1] != '.') {
        length1--;
    }
    while (length2 > 0 && str2[length2] != '.') {
        length2--;
    }
    size_t length = MIN(length1, length2);
    if (length <= 0)
        return 0;
    /* Compare characters backward from the end of each base name */
    for (int i = length; i > 0; i--) {
        if (str1[length1--] != str2[length2--]) {
            return 0;
        }
    }
    return 1;
}

/* Give the companion file name the file extension of the game file.
   This makes the companion file logic work even if the player has renamed the
   files to, say, end with ".saga" (well, as long as both files have been
   renamed in the same way.) */
static char *AddGameFileExtension(const char *companion_name, size_t *namelength,
                           const char *game_path, size_t gamepathlen) {
    if (!companion_name || !namelength || !game_path) {
        fprintf(stderr, "AddGameFileExtension: NULL parameter\n");
        return NULL;
    }

    /* Find extension in game_path */
    const char *game_ext = strrchr(game_path, '.');
    /* Find extension in filename */
    const char *companion_ext = strrchr(companion_name, '.');

    /* (It should not really be possible for any of them
       to not have an extension) */
    size_t extension_length = game_ext ? (gamepathlen - (game_ext - game_path)) : 0;
    size_t base_length = companion_ext ? companion_ext - companion_name : *namelength;

    *namelength = base_length + extension_length;

    char *result = MemAlloc(*namelength + 1);

    memcpy(result, companion_name, base_length + 1);
    if (extension_length > 0) {
        /* Copy includes terminating 0 */
        memcpy(result + base_length, game_ext, extension_length + 1);
    }
    return result;
}

static size_t get_filename_length(const char *filename, size_t length_field)
{
    return (length_field != 0) ? length_field : strlen(filename);
}

/* Search a database of filename pairs for a match against the current game
   file.
   Each entry in 'list' is a pair of filenames that belong together (e.g. game
   data file and its companion graphics file). If we match either filename in a
   pair, we return a copy of the other one (the "companion"), with the game
   file's extension applied to it. Returns NULL if no match is found. */
static char *LookForCompanionFilenameInDatabase(const pairrec list[][2], size_t game_filename_len, size_t *out_companion_len)
{
    for (int i = 0; list[i][0].filename != NULL; i++) {
        for (int j = 0; j < 2; j++) {
            const char *candidate = list[i][j].filename;
            size_t candidate_len = get_filename_length(candidate, list[i][j].stringlength);
            if (CompareFilenames(game_file, game_filename_len, candidate, candidate_len)) {
                int companion_idx = 1 - j;
                const char *companion = list[i][companion_idx].filename;
                *out_companion_len = get_filename_length(companion, list[i][companion_idx].stringlength);
                return AddGameFileExtension(companion, out_companion_len, game_file, game_filename_len);
            }
        }
    }
    return NULL;
}

void SwapWithCompanionFile(uint8_t **sf, size_t *extent, uint8_t **companion, size_t *companionsize)
{
    uint8_t *temp = *companion;
    size_t tempsize = *companionsize;
    *companion = *sf;
    *companionsize = *extent;
    *sf = temp;
    *extent = tempsize;
}

/* Look up the companion filename for the current game file in a database
   of known filename pairs, and return its full path (using the same
   directory as the game file). Does not verify whether the file exists.

   For example, if the game file is "/Games/HULK.DAT" and the companion
   database says HULK.DAT pairs with HULK.PIC, this returns
   "/Games/HULK.PIC". */
char *LookInDatabase(const pairrec list[][2], const char *game_path, size_t pathlen)
{
    size_t resultlen;
    char *foundname = LookForCompanionFilenameInDatabase(list, pathlen, &resultlen);
    if (foundname != NULL) {
        /* Extract the directory portion of the game file path */
        const char *file_delimiter_position = strrchr(game_file, '/');
        if (file_delimiter_position == NULL)
            file_delimiter_position = strrchr(game_path, '\\');
        size_t stringlen;
        if (file_delimiter_position == NULL) {
            /* No directory separator found — file is in the working
               directory */
            return foundname;
        } else {
            stringlen = file_delimiter_position - game_path + 1;
        }
        /* Prepend the game file's directory to the companion filename */
        size_t newlen = stringlen + resultlen;
        char *path = MemAlloc(newlen + 1);
        memcpy(path, game_path, stringlen);
        memcpy(path + stringlen, foundname, resultlen + 1);
        free(foundname);
        return path;
    }

    return NULL;
}

