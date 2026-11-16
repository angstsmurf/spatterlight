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

#include "common_file_utils.h"
#include "detect_game.h"
#include "sagadraw.h"
#include "scott_game_info.h"
#include "scottfree_loader.h"

#include "sysmess_maps.h"
#include "gremlins.h"
#include "robin_of_sherwood.h"
#include "seas_of_blood.h"

#include "apple2detect.h"
#include "atari8detect.h"
#include "scott_c64detect.h"
#include "load_ti99_4a.h"
#include "zxdetect.h"

#include "parser.h"

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

/* Set up the system messages, extra data, image patches and graphics of a
   detected Adventure International UK / Brian Howarth or C64 game. */
static void SetUpGameSpecifics(GameIDType detectedGame)
{
    /* Copy ZX Spectrum style system messages as a base, then apply
       game-specific overrides below. The direction words before
       PLAY_AGAIN are copied for the Hulk only, below. */
    for (int i = PLAY_AGAIN; i < MAX_SYSMESS && sysdict_zx[i] != NULL; i++) {
        sys[i] = sysdict_zx[i];
    }

    /* Game-specific post-load setup: load extra data (Robin of Sherwood
       forest images, Seas of Blood combat tables), fix image assignments, and
       map system_messages[] indices to the sys[] slots the engine uses. */
    switch (detectedGame) {
    case ROBIN_OF_SHERWOOD:
        LoadExtraSherwoodData(0);
        break;
    case ROBIN_OF_SHERWOOD_C64:
        LoadExtraSherwoodData(1);
        break;
    case SEAS_OF_BLOOD:
        LoadExtraSeasOfBloodData(0);
        break;
    case SEAS_OF_BLOOD_C64:
        LoadExtraSeasOfBloodData(1);
        break;
    case CLAYMORGUE: {
        int src = MapSysRange(PLAY_AGAIN, ON_A_SCALE_THAT_RATES, 2);
        MapSysRange(OK, RESUME_A_SAVED_GAME, src);
        break;
    }
    case SECRET_MISSION:
    case SECRET_MISSION_C64:
        Items[3].Image = 23;
        Items[3].Flag = 128 | 2;
        Rooms[2].Image = 0;
        if (detectedGame == SECRET_MISSION_C64) {
            SecretMission64Sysmess();
            break;
        }
        /* fallthrough */
    case ADVENTURELAND: {
        int src = MapSysRange(PLAY_AGAIN, ON_A_SCALE_THAT_RATES, 2);
        src = MapSysRange(OK, YOU_HAVENT_GOT_IT, src);
        MapSysRange(YOU_DONT_SEE_IT, RESUME_A_SAVED_GAME, src);
        break;
    }
    case ADVENTURELAND_C64:
        Adventureland64Sysmess();
        break;
    case CLAYMORGUE_C64:
        Claymorgue64Sysmess();
        break;
    case GREMLINS_GERMAN_C64:
        LoadExtraGermanGremlinsC64Data();
        break;
    case SPIDERMAN_C64:
        Spiderman64Sysmess();
        break;
    case SPIDERMAN_ATARI8:
        SpidermanAtari8Sysmess();
        break;
    case SUPERGRAN_C64:
        Supergran64Sysmess();
        break;
    case SAVAGE_ISLAND_C64:
        Items[20].Image = 13;
        /* fallthrough */
    case SAVAGE_ISLAND2_C64:
        Supergran64Sysmess();
        sys[IM_DEAD] = "I'm DEAD!! ";
        if (detectedGame == SAVAGE_ISLAND2_C64)
            Rooms[30].Image = 20;
        break;
    case SAVAGE_ISLAND:
        Items[20].Image = 13;
        /* fallthrough */
    case SAVAGE_ISLAND2:
        MyLoc = 30; /* Both parts of Savage Island begin in room 30 */
        /* fallthrough */
    case GREMLINS_GERMAN:
    case GREMLINS:
    case SUPERGRAN:
        MapGremlinsSysMessages(2);
        break;
    case GREMLINS_SPANISH:
        LoadExtraSpanishGremlinsData();
        break;
    case GREMLINS_SPANISH_C64:
        LoadExtraSpanishGremlinsC64Data();
        break;
    case HULK_C64:
    case HULK:
        for (int i = NORTH; i < PLAY_AGAIN; i++) {
            sys[i] = sysdict_zx[i];
        }
        break;
    default:
        if (!(Game->subtype & C64)) {
            if (Game->subtype & MYSTERIOUS) {
                int src = MapSysRange(PLAY_AGAIN, YOU_HAVENT_GOT_IT, 2);
                src = MapSysRange(YOU_DONT_SEE_IT, WHAT_NOW, src);
                MapSysRange(LIGHT_HAS_RUN_OUT, RESUME_A_SAVED_GAME, src);
                sys[ITEM_DELIMITER] = " - ";
                sys[MESSAGE_DELIMITER] = "\n";
                sys[YOU_SEE] = "\nThings I can see:\n";
                break;
            } else {
                MapSysRange(PLAY_AGAIN, RESUME_A_SAVED_GAME, 2);
            }
        }
        break;
    }

    switch (detectedGame) {
    case GREMLINS_GERMAN:
        LoadExtraGermanGremlinsData();
        break;
    case PERSEUS_ITALIAN:
        PerseusItalianSysmess();
        break;
    default:
        break;
    }

    if ((Game->subtype & (MYSTERIOUS | C64)) == (MYSTERIOUS | C64))
        Mysterious64Sysmess();

    /* If it is a C64 or a Mysterious Adventures game, we have set up the
       graphics already */
    if (!(Game->subtype & (C64 | MYSTERIOUS)) && Game->number_of_pictures > 0) {
        SagaGraphicsSetup(0);
    }
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

    SetParserWordLists(EnglishDirections, EnglishSkipList, EnglishDelimiterList,
        NULL, EnglishExtraNouns);

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

    SetUpGameSpecifics(detectedGame);

    return detectedGame;
}
