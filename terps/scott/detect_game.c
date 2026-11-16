/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  detect_game.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Created by Petter Sjölund on 2022-01-10.
 */

#include <stdlib.h>

#include "scott.h"
#include "scott_display.h"

#include "common_file_utils.h"
#include "detect_game.h"
#include "game_dispatch.h"
#include "scott_game_info.h"
#include "scottfree_loader.h"

#include "apple2detect.h"
#include "atari8detect.h"
#include "scott_c64detect.h"
#include "load_ti99_4a.h"
#include "zxdetect.h"

#include "parser.h"

/* Size of system_messages[], the raw message strings read from the game
   file. Not to be confused with MAX_SYSMESS (scott_sysmess.h), the size
   of sys[], which is indexed by SysMessageType. */
#define NUM_SYSTEM_MESSAGES 60

const char *game_file = NULL;
char *DirPath = "."; /* Directory containing the game file */

uint8_t *entire_file; /* Raw game file loaded into memory */
size_t file_length;
int file_baseline_offset = 0; /* Byte offset adjustment for files with extra data prepended */

const char *system_messages[NUM_SYSTEM_MESSAGES];

char *title_screen = NULL;
uint8_t *ZXLoadingScreen = NULL; /* ZX Spectrum SCREEN$ shown as a title image, or NULL */

/* Check if the loaded game is a Mysterious Adventures title.
   Checks the current Game pointer first, then falls back to matching header
   values against the game database (for .dat plaintext format). */
static int IsMysterious(void)
{
    if (Game && (Game->subtype & MYSTERIOUS) != 0)
        return 1;
    for (int i = 0; games[i].Title != NULL; i++) {
        if (games[i].subtype & MYSTERIOUS) {
            if (games[i].number_of_items == GameHeader.NumItems &&
                games[i].number_of_actions == GameHeader.NumActions &&
                games[i].number_of_words == GameHeader.NumWords &&
                games[i].number_of_rooms == GameHeader.NumRooms &&
                games[i].max_carried == GameHeader.MaxCarry &&
                games[i].word_length == GameHeader.WordLength &&
                games[i].number_of_messages == GameHeader.NumMessages)
                return 1;
        }
    }

    return 0;
}

/* Top-level game detection: try each supported format in order.

   1. ScottFree plaintext (LoadDatabase)
   2. TI-99/4A cartridge
   3. Commodore 64 (with optional decrunch)
   4. Atari 8-bit
   5. Apple II
   6. ZX Spectrum (.z80 snapshots, Parsec-compressed)

   Once the game is identified and loaded, configures system messages,
   graphics, and game-specific overrides (system message mappings,
   item/room image patches, parser language tables). */
GameIDType DetectGame(const char *file_name)
{
    FILE *f = fopen(file_name, "rb");
    if (f == NULL)
        Fatal("Cannot open game");

    file_length = GetFileLength(f);

    if (file_length > MAX_GAMEFILE_SIZE) {
        debug_print("File too large to be a valid game file (%zu bytes, max is %d)\n",
            file_length, MAX_GAMEFILE_SIZE);
        fclose(f);
        return UNKNOWN_GAME;
    }

    SetParserLanguage(PARSER_ENGLISH);

    Game = MemCalloc(sizeof(GameInfo));

    /* Try the ScottFree plaintext format first */
    GameIDType detectedGame = LoadDatabase(f);

    size_t result = 0;
    if (detectedGame == UNKNOWN_GAME) {
        entire_file = MemAlloc(file_length);
        fseek(f, 0, SEEK_SET);
        result = fread(entire_file, 1, file_length, f);
    }
    fclose(f);

    if (detectedGame == UNKNOWN_GAME) { /* Not a ScottFree game: try each format in turn */
        if (result == 0)
            Fatal("File empty or read error!");

        detectedGame = DetectTI994A();

        if (detectedGame == UNKNOWN_GAME) {
            detectedGame = DetectC64(&entire_file, &file_length, file_name);
        }

        if (detectedGame == UNKNOWN_GAME) {
            detectedGame = DetectAtari8(&entire_file, &file_length);
        }

        if (detectedGame == UNKNOWN_GAME) {
            detectedGame = DetectApple2(&entire_file, &file_length);
        }

        if (detectedGame == UNKNOWN_GAME) {
            detectedGame = DetectZXSpectrum();
        }

        if (detectedGame == UNKNOWN_GAME) {
            free(entire_file);
            free(Game);
            Game = NULL;
            return UNKNOWN_GAME;
        }
    }

    if (detectedGame == HULK_US || detectedGame == RETURN_TO_PIRATES_ISLE) {
        CurrentGame = detectedGame;
        Game->type = US_VARIANT;
    }

    if (detectedGame == SCOTTFREE || detectedGame == TI994A)
        CurrentGame = detectedGame;

    if (IsMysterious()) {
        Options = Options | SCOTTLIGHT | PREHISTORIC_LAMP;
        ImageWidth = 255;
        ImageHeight = 95;
    }

    if (detectedGame == SCOTTFREE || detectedGame == TI994A || Game->type == US_VARIANT)
        return detectedGame;

    GameSetup(detectedGame);

    return detectedGame;
}
