//
//  companionfile.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-10-10.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "companionfile.h"

// Constants for readability
#define MAX_FILENAME_BUFFER_EXTRA 10
#define CRACK_BRACKET_LENGTH 8  // Length of "[cr CSS]"
#define BUCKAROO_BANZAI_OFFSET 27
#define MIN_FILENAME_LENGTH 4
#define MIN_COMPANION_KEYWORD_LENGTH 3

typedef enum {
    TYPE_NONE,
    TYPE_A,
    TYPE_B,
    TYPE_ONE,
    TYPE_TWO,
    TYPE_1,
    TYPE_2,
} CompanionNameType;

// Helper function to check if character is path separator
static inline int is_path_separator(char c)
{
    return (c == '/' || c == '\\');
}

/**
 * Strips bracket notation from filename (e.g., "file[tag].dsk" -> "file.dsk")
 * Returns: 1 on success, 0 on failure
 */
static int StripBrackets(char sideB[], size_t length)
{
    if (!sideB || length < MIN_FILENAME_LENGTH) {
        return 0;
    }

    // Find file extension (everything after the last period)
    char *extension = strrchr(sideB, '.');
    if (!extension) {
        return 0;
    }

    size_t extlen = length - (extension - sideB) + 1;

    // Find last closing bracket before extension
    char *close_bracket = strrchr(sideB, ']');
    if (!close_bracket || close_bracket >= extension) {
        return 0;  // No valid bracket notation
    }

    // Find matching opening bracket
    char *open_bracket = strrchr(sideB, '[');
    if (!open_bracket || open_bracket >= close_bracket) {
        return 0;  // No matching opening bracket
    }

    // Remove bracketed section by copying extension over it
    memcpy(open_bracket, close_bracket + 1, extlen);
    return 1;
}

/**
 * Tries to add "[cr CSS]" to companion file name
 * Example: "file.dsk" -> "file[cr CSS].dsk"
 */
static int try_add_css_marker(char *buffer, size_t buffer_size, size_t original_len)
{
    // Find the period before file extension
    size_t ppos = original_len - 1;
    while (buffer[ppos] != '.' && ppos > 0) {
        ppos--;
    }

    if (ppos < 1) {
        return 0;
    }

    // Check if we have enough space
    if (original_len + CRACK_BRACKET_LENGTH >= buffer_size) {
        return 0;
    }

    // Shift extension to make room for marker
    for (size_t i = original_len; i >= ppos; i--) {
        buffer[i + CRACK_BRACKET_LENGTH] = buffer[i];
    }

    // Insert "[cr CSS]" marker
    buffer[ppos++] = '[';
    buffer[ppos++] = 'c';
    buffer[ppos++] = 'r';
    buffer[ppos++] = ' ';
    buffer[ppos++] = 'C';
    buffer[ppos++] = 'S';
    buffer[ppos++] = 'S';
    buffer[ppos++] = ']';

    return 1;
}

/**
 * Checks for the special case of Buckaroo Banzai 177:
 * Changes 'a' to 'b' at a specific offset
 * Example: "177a - Buckaroo Banzai - Side 1.dsk" ->
 *        "177b - Buckaroo Banzai - Side 2.dsk"
 */
static uint8_t *try_buckaroo_banzai_hack(char *buffer, int index, size_t *filesize)
{
    if (index > BUCKAROO_BANZAI_OFFSET && buffer[index - BUCKAROO_BANZAI_OFFSET] == 'a') {
        buffer[index - BUCKAROO_BANZAI_OFFSET] = 'b';
        debug_print("looking for companion file (Buckaroo hack): \"%s\"\n", buffer);
        return ReadFileIfExists(buffer, filesize);
    }
    return NULL;
}

/**
 * Searches for companion file using various naming strategies
 */
static uint8_t *LookForCompanionFilename(const char *game_file, int index,
                                         CompanionNameType type, size_t stringlen,
                                         size_t *filesize)
{
    char buffer[stringlen + MAX_FILENAME_BUFFER_EXTRA];
    uint8_t *result = NULL;

    // Copy original filename to buffer
    memcpy(buffer, game_file, stringlen + 1);

    // Modify filename based on companion type
    switch (type) {
        case TYPE_A:
            buffer[index] = 'A';
            break;
        case TYPE_B:
            buffer[index] = 'B';
            break;
        case TYPE_1:
            buffer[index] = '1';
            break;
        case TYPE_2:
            buffer[index] = '2';
            break;
        case TYPE_ONE:
            buffer[index] = 'o';
            buffer[index + 1] = 'n';
            buffer[index + 2] = 'e';
            break;
        case TYPE_TWO:
            buffer[index] = 't';
            buffer[index + 1] = 'w';
            buffer[index + 2] = 'o';
            break;
        case TYPE_NONE:
            return NULL;
    }

    // Try the primary filename variant
    debug_print("looking for companion file \"%s\"\n", buffer);
    result = ReadFileIfExists(buffer, filesize);

    // If not found, try fallback strategies
    if (!result) {
        switch (type) {
            case TYPE_B:
                // Try stripping text in brackets: "fileB[tag].dsk" -> "fileB.dsk"
                if (StripBrackets(buffer, stringlen)) {
                    debug_print("looking for companion file (with removed text in brackets): \"%s\"\n", buffer);
                    result = ReadFileIfExists(buffer, filesize);
                }
                break;

            case TYPE_A:
                // Try adding CSS marker: "fileA.dsk" -> "fileA[cr CSS].dsk"
                if (try_add_css_marker(buffer, sizeof(buffer), stringlen)) {
                    debug_print("looking for companion file (with added [cr CSS]): \"%s\"\n", buffer);
                    result = ReadFileIfExists(buffer, filesize);
                }
                break;

            case TYPE_2:
                // Check for "177a - Buckaroo Banzai" pattern
                result = try_buckaroo_banzai_hack(buffer, index, filesize);
                break;

            default:
                break;
        }
    }

    return result;
}

/**
 * Checks if the position in filename matches a companion file pattern
 * Patterns: "side", "disk", or single digit/letter before extension
 */
static int is_companion_name(const char *filename, int i, char c)
{
    if (i <= MIN_COMPANION_KEYWORD_LENGTH) {
        return 0;
    }

    char c_minus1 = tolower(filename[i - 1]);
    char c_minus2 = tolower(filename[i - 2]);
    char c_minus3 = tolower(filename[i - 3]);

    // Check for "side" pattern
    if (c == 'e' && c_minus1 == 'd' && c_minus2 == 'i' && c_minus3 == 's') {
        return 1;
    }

    // Check for "disk" pattern
    if (c == 'k' && c_minus1 == 's' && c_minus2 == 'i' && c_minus3 == 'd') {
        return 1;
    }

    // Check for single digit/letter before extension
    if (c == '.' && (c_minus1 == '1' || c_minus1 == '2' ||
                     c_minus1 == 'a' || c_minus1 == 'b')) {
        return 1;
    }

    return 0;
}

/**
 * Determines the companion file type based on the current character
 * This maps the current disk identifier to what we should look for
 */
static CompanionNameType determine_type(const char *filename, size_t length,
                                        int i, char character)
{
    switch (character) {
        case 'a':
            return TYPE_B;  // If we have 'a', look for 'B'

        case 'b':
            return TYPE_A;  // If we have 'b', look for 'A'

        case 't':
            // Check for "two" -> look for "one"
            if (length > (size_t)(i + 4) &&
                filename[i + 3] == 'w' &&
                filename[i + 4] == 'o') {
                return TYPE_ONE;
            }
            break;

        case 'o':
            // Check for "one" -> look for "two"
            if (length > (size_t)(i + 4) &&
                filename[i + 3] == 'n' &&
                filename[i + 4] == 'e') {
                return TYPE_TWO;
            }
            break;

        case '2':
            return TYPE_1;  // If we have '2', look for '1'

        case '1':
            return TYPE_2;  // If we have '1', look for '2'
    }

    return TYPE_NONE;
}

/**
 * Main function to find and load companion files for multi-disk games
 *
 * Searches for companion files based on common naming patterns:
 * - "side 1" <-> "side 2"
 * - "disk 1" <-> "disk 2"
 * - "file1.dsk" <-> "file2.dsk"
 * - "fileA.dsk" <-> "fileB.dsk"
 *
 * @param gamefile Global variable containing the original filename
 * @param size Output parameter for the companion file size
 * @return: Pointer to loaded file data, or NULL if not found
 */
uint8_t *GetCompanionFile(const char *gamefile, size_t *size)
{
    if (!size) {
        return NULL;
    }

    size_t gamefilelen = strlen(gamefile);
    if (gamefilelen == 0) {
        return NULL;
    }

    uint8_t *result = NULL;

    // Scan backwards through filename from the end
    // Stop at path separator (we only care about the filename, not the path)
    for (int i = (int)gamefilelen - 1; i >= 0 && !is_path_separator(gamefile[i]); i--) {
        char c = tolower(gamefile[i]);

        // Check if this position matches a companion file pattern
        if (is_companion_name(gamefile, i, c) && gamefilelen > (size_t)(i + 2)) {
            // Extract the separator character (space, underscore, or period)
            if (c != '.') {
                c = gamefile[i + 1];
            }

            // Only proceed if separator is valid
            if (c == ' ' || c == '_' || c == '.') {
                // Extract the disk identifier character
                char disk_char;
                int adjusted_index;

                if (c == '.') {
                    // Pattern: "file1.dsk" or "filea.dsk"
                    disk_char = tolower(gamefile[i - 1]);
                    adjusted_index = i - 3;
                } else {
                    // Pattern: "side 1.dsk" or "disk_2.dsk"
                    disk_char = tolower(gamefile[i + 2]);
                    adjusted_index = i;
                }

                // Determine what type of companion to look for
                CompanionNameType type = determine_type(gamefile, gamefilelen,
                                                        adjusted_index, disk_char);

                if (type != TYPE_NONE) {
                    result = LookForCompanionFilename(gamefile, adjusted_index + 2,
                                                      type, gamefilelen, size);
                    if (result) {
                        return result;
                    }
                }
            }
        }
    }

    return NULL;
}
