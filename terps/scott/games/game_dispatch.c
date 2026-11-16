/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  game_dispatch.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  The places where the interpreter core hands over to the code of one
 *  Adventure International UK game: setup after loading, the game-specific
 *  opcode, the room picture and the timer animations.
 */

#include "scott.h"

#include "sagadraw.h"
#include "scott_display.h"
#include "scott_game_info.h"

#include "adventureland.h"
#include "game_dispatch.h"
#include "gremlins.h"
#include "hulk.h"
#include "robin_of_sherwood.h"
#include "seas_of_blood.h"
#include "secret_mission.h"
#include "sysmess_maps.h"

/* The message layout of the ZX Spectrum Adventureland, which Secret Mission
   shares */
static void MapAdventurelandSysMessages(void)
{
    int src = MapSysRange(PLAY_AGAIN, ON_A_SCALE_THAT_RATES, 2);
    src = MapSysRange(OK, YOU_HAVENT_GOT_IT, src);
    MapSysRange(YOU_DONT_SEE_IT, RESUME_A_SAVED_GAME, src);
}

/* The message layout of the games that have no case of their own in
   GameSetup(). The C64 ones are mapped by their loader. */
static void MapStandardSysMessages(void)
{
    if (Game->subtype & C64)
        return;
    if (Game->subtype & MYSTERIOUS) {
        int src = MapSysRange(PLAY_AGAIN, YOU_HAVENT_GOT_IT, 2);
        src = MapSysRange(YOU_DONT_SEE_IT, WHAT_NOW, src);
        MapSysRange(LIGHT_HAS_RUN_OUT, RESUME_A_SAVED_GAME, src);
        sys[ITEM_DELIMITER] = " - ";
        sys[MESSAGE_DELIMITER] = "\n";
        sys[YOU_SEE] = "\nThings I can see:\n";
    } else {
        MapSysRange(PLAY_AGAIN, RESUME_A_SAVED_GAME, 2);
    }
}

/* Picture assignments that the Secret Mission database gets wrong */
static void PatchSecretMissionImages(void)
{
    Items[3].Image = 23;
    Items[3].Flag = 128 | 2;
    Rooms[2].Image = 0;
}

/* Set up the system messages, extra data, image patches and graphics of a
   detected Adventure International UK / Brian Howarth or C64 game. */
void GameSetup(GameIDType detectedGame)
{
    /* Copy ZX Spectrum style system messages as a base, then apply
       game-specific overrides below. The direction words before
       PLAY_AGAIN are copied for the Hulk only, below. */
    for (int i = PLAY_AGAIN; i < MAX_SYSMESS && sysdict_zx[i] != NULL; i++) {
        sys[i] = sysdict_zx[i];
    }

    /* Game-specific post-load setup: load extra data (Robin of Sherwood
       forest images, Seas of Blood combat tables), fix image assignments, and
       map system_messages[] indices to the sys[] slots the engine uses.
       Every case is complete in itself: none falls through to the next. */
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
    case CLAYMORGUE_C64:
        Claymorgue64Sysmess();
        break;
    case SECRET_MISSION:
        PatchSecretMissionImages();
        MapAdventurelandSysMessages();
        break;
    case SECRET_MISSION_C64:
        PatchSecretMissionImages();
        SecretMission64Sysmess();
        break;
    case ADVENTURELAND:
        MapAdventurelandSysMessages();
        break;
    case ADVENTURELAND_C64:
        Adventureland64Sysmess();
        break;
    case SPIDERMAN_C64:
        Spiderman64Sysmess();
        break;
    case SPIDERMAN_ATARI8:
        SpidermanAtari8Sysmess();
        break;
    case SUPERGRAN:
        MapGremlinsSysMessages(2);
        break;
    case SUPERGRAN_C64:
        Supergran64Sysmess();
        break;
    case SAVAGE_ISLAND:
        Items[20].Image = 13;
        MyLoc = 30; /* Both parts of Savage Island begin in room 30 */
        MapGremlinsSysMessages(2);
        break;
    case SAVAGE_ISLAND2:
        MyLoc = 30;
        MapGremlinsSysMessages(2);
        break;
    case SAVAGE_ISLAND_C64:
        Items[20].Image = 13;
        Supergran64Sysmess();
        sys[IM_DEAD] = "I'm DEAD!! ";
        break;
    case SAVAGE_ISLAND2_C64:
        Rooms[30].Image = 20;
        Supergran64Sysmess();
        sys[IM_DEAD] = "I'm DEAD!! ";
        break;
    case GREMLINS:
        MapGremlinsSysMessages(2);
        break;
    case GREMLINS_GERMAN:
        MapGremlinsSysMessages(2);
        LoadExtraGermanGremlinsData();
        break;
    case GREMLINS_GERMAN_C64:
        LoadExtraGermanGremlinsC64Data();
        break;
    case GREMLINS_SPANISH:
        LoadExtraSpanishGremlinsData();
        break;
    case GREMLINS_SPANISH_C64:
        LoadExtraSpanishGremlinsC64Data();
        break;
    case HULK:
    case HULK_C64:
        for (int i = NORTH; i < PLAY_AGAIN; i++) {
            sys[i] = sysdict_zx[i];
        }
        break;
    case PERSEUS_ITALIAN:
        MapStandardSysMessages();
        PerseusItalianSysmess();
        break;
    default:
        MapStandardSysMessages();
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

/* Run the game-specific opcode of the current game with the parameter p. */
void GameOpcode(int p)
{
    switch (CurrentGame) {
    case SPIDERMAN:
    case SPIDERMAN_C64:
    case SPIDERMAN_ATARI8:
        DrawBlack();
        break;
    case SECRET_MISSION:
    case SECRET_MISSION_C64:
        SecretAction(p);
        break;
    case ADVENTURELAND:
    case ADVENTURELAND_C64:
        AdventurelandAction(p);
        break;
    case SEAS_OF_BLOOD:
    case SEAS_OF_BLOOD_C64:
        BloodAction(p);
        break;
    case ROBIN_OF_SHERWOOD:
    case ROBIN_OF_SHERWOOD_C64:
        SherwoodAction(p);
        break;
    case GREMLINS:
    case GREMLINS_SPANISH:
    case GREMLINS_SPANISH_C64:
    case GREMLINS_GERMAN:
    case GREMLINS_GERMAN_C64:
        GremlinsAction();
        break;
    default:
        break;
    }
}

/* Draw the room picture if the current game has its own way of doing it.
   Returns 1 if it did. */
int GameRoomImage(void)
{
    switch (CurrentGame) {
    case SEAS_OF_BLOOD:
    case SEAS_OF_BLOOD_C64:
        SeasOfBloodRoomImage();
        return 1;
    case ROBIN_OF_SHERWOOD:
    case ROBIN_OF_SHERWOOD_C64:
        RobinOfSherwoodLook();
        return 1;
    case HULK:
    case HULK_C64:
        HulkLook();
        return 1;
    default:
        break;
    }
    return 0;
}

/* Advance the timer animation of the current game, if it has one. Returns 1
   if it does. */
int GameTimer(void)
{
    switch (Game->type) {
    case SHERWOOD_VARIANT:
        UpdateRobinOfSherwoodAnimations();
        return 1;
    case GREMLINS_VARIANT:
        UpdateGremlinsAnimations();
        return 1;
    case SECRET_MISSION_VARIANT:
        UpdateSecretAnimations();
        return 1;
    default:
        return 0;
    }
}
