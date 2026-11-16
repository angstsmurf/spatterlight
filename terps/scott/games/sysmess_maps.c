/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  sysmess_maps.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Maps each port's table of system messages to the sys[] slots of the
 *  interpreter.
 */

#include <string.h>

#include "apple2draw.h"
#include "scott_parser.h"
#include "scott.h"
#include "detect_game.h"
#include "sysmess_maps.h"

/* Map system_messages[src_offset + i] to sys[keys[i]] for each of the
   n keys. Each port stores its system messages in its own order, so most
   of them need a key table. */
void MapSysMessages(const SysMessageType *keys, size_t n, int src_offset)
{
    for (size_t i = 0; i < n; i++)
        sys[keys[i]] = system_messages[src_offset + i];
}

/* Map the consecutive sys[] slots first to last to the consecutive
   system_messages[] starting at index src. Returns the index after the
   last one used, so that ranges which follow each other in the game's
   table can be chained without typing each start index. */
int MapSysRange(SysMessageType first, SysMessageType last, int src)
{
    for (int i = first; i <= (int)last; i++)
        sys[i] = system_messages[src - first + i];
    return src + (int)last - (int)first + 1;
}

/* The system message order shared by the C64 ports of Adventureland,
   Secret Mission and the Mysterious Adventures. */
static const SysMessageType c64_sysmess_keys[] = {
    NORTH,
    SOUTH,
    EAST,
    WEST,
    UP,
    DOWN,
    EXITS,
    YOU_SEE,
    YOU_ARE,
    TOO_DARK_TO_SEE,
    LIGHT_HAS_RUN_OUT,
    LIGHT_RUNS_OUT_IN,
    TURNS,
    I_DONT_KNOW_HOW_TO,
    SOMETHING,
    I_DONT_KNOW_WHAT_A,
    IS,
    YOU_CANT_GO_THAT_WAY,
    OK,
    WHAT_NOW,
    HUH,
    YOU_HAVENT_GOT_IT,
    INVENTORY,
    YOU_DONT_SEE_IT,
    THATS_BEYOND_MY_POWER,
    DANGEROUS_TO_MOVE_IN_DARK,
    DIRECTION,
    YOU_FELL_AND_BROKE_YOUR_NECK,
    YOURE_CARRYING_TOO_MUCH,
    IM_DEAD,
    PLAY_AGAIN,
    RESUME_A_SAVED_GAME,
    IVE_STORED,
    TREASURES,
    ON_A_SCALE_THAT_RATES,
    YOU_CANT_DO_THAT_YET,
    I_DONT_UNDERSTAND,
    NOTHING,
    YOUVE_SOLVED_IT
};

void Spiderman64Sysmess(void)
{
    SysMessageType messagekey[] = {
        NORTH,
        SOUTH,
        EAST,
        WEST,
        UP,
        DOWN,
        EXITS,
        YOU_SEE,
        YOU_ARE,
        TOO_DARK_TO_SEE,
        LIGHT_HAS_RUN_OUT,
        LIGHT_RUNS_OUT_IN,
        TURNS,
        I_DONT_KNOW_HOW_TO,
        SOMETHING,
        I_DONT_KNOW_WHAT_A,
        IS,
        YOU_CANT_GO_THAT_WAY,
        OK,
        WHAT_NOW,
        HUH,
        YOU_HAVE_IT,
        YOU_HAVENT_GOT_IT,
        INVENTORY,
        YOU_DONT_SEE_IT,
        THATS_BEYOND_MY_POWER,
        DANGEROUS_TO_MOVE_IN_DARK,
        DIRECTION,
        YOU_FELL_AND_BROKE_YOUR_NECK,
        YOURE_CARRYING_TOO_MUCH,
        IM_DEAD,
        DROPPED,
        TAKEN,
        PLAY_AGAIN,
        RESUME_A_SAVED_GAME,
        IVE_STORED,
        TREASURES,
        ON_A_SCALE_THAT_RATES,
        YOU_CANT_DO_THAT_YET,
        I_DONT_UNDERSTAND,
        NOTHING,
        YOUVE_SOLVED_IT
    };

    MAP_SYS_MESSAGES(messagekey, 0);

    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
    sys[SOMETHING] = "\" something. ";
}

/* The text-only Atari 8-bit 16K port. Its interpreter says "OK." where the
   others say "Dropped." and "Taken.", and it has no "<HIT ENTER>". */
void SpidermanAtari8Sysmess(void)
{
    SysMessageType messagekey[] = {
        NORTH,
        SOUTH,
        EAST,
        WEST,
        UP,
        DOWN,
        NONE, /* "TAPE ERROR" */
        EXITS,
        YOU_SEE,
        YOU_ARE,
        TOO_DARK_TO_SEE,
        LIGHT_HAS_RUN_OUT,
        LIGHT_RUNS_OUT_IN,
        TURNS,
        I_DONT_KNOW_HOW_TO,
        SOMETHING,
        I_DONT_KNOW_WHAT_A,
        IS,
        YOU_CANT_GO_THAT_WAY,
        OK,
        WHAT_NOW,
        HUH,
        YOU_HAVE_IT,
        YOU_HAVENT_GOT_IT,
        YOU_DONT_SEE_IT,
        THATS_BEYOND_MY_POWER,
        DANGEROUS_TO_MOVE_IN_DARK,
        DIRECTION,
        YOU_FELL_AND_BROKE_YOUR_NECK,
        YOURE_CARRYING_TOO_MUCH,
        IM_DEAD,
        RESUME_A_SAVED_GAME,
        IVE_STORED,
        TREASURES,
        ON_A_SCALE_THAT_RATES,
        YOU_CANT_DO_THAT_YET,
        INVENTORY,
        I_DONT_UNDERSTAND,
        NOTHING,
        PLAY_AGAIN,
        YOUVE_SOLVED_IT
    };

    const char *none = sys[NONE];

    MAP_SYS_MESSAGES(messagekey, 0);

    sys[NONE] = none;
    /* This string follows the unterminated direction letters "NSEWUD" */
    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
    /* A carriage return ends the room description in the original */
    sys[YOU_SEE] = "\nI can see:";
    sys[DROPPED] = sys[OK];
    sys[TAKEN] = sys[OK];
}

void Adventureland64Sysmess(void)
{
    MAP_SYS_MESSAGES(c64_sysmess_keys, 0);

    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
    sys[SOMETHING] = "\" something. ";
}

void Claymorgue64Sysmess(void)
{
    SysMessageType messagekey[] = {
        NORTH,
        SOUTH,
        EAST,
        WEST,
        UP,
        DOWN,
        EXITS,
        YOU_SEE,
        YOU_ARE,
        TOO_DARK_TO_SEE,
        LIGHT_HAS_RUN_OUT,
        LIGHT_RUNS_OUT_IN,
        TURNS,
        I_DONT_KNOW_HOW_TO,
        SOMETHING,
        I_DONT_KNOW_WHAT_A,
        IS,
        YOU_CANT_GO_THAT_WAY,
        OK,
        WHAT_NOW,
        HUH,
        YOU_HAVE_IT,
        YOU_HAVENT_GOT_IT,
        INVENTORY,
        YOU_DONT_SEE_IT,
        THATS_BEYOND_MY_POWER,
        DANGEROUS_TO_MOVE_IN_DARK,
        DIRECTION,
        YOU_FELL_AND_BROKE_YOUR_NECK,
        YOURE_CARRYING_TOO_MUCH,
        IM_DEAD,
        PLAY_AGAIN,
        RESUME_A_SAVED_GAME,
        IVE_STORED,
        TREASURES,
        ON_A_SCALE_THAT_RATES,
        YOU_CANT_DO_THAT_YET,
        I_DONT_UNDERSTAND,
        NOTHING,
        YOUVE_SOLVED_IT
    };

    MAP_SYS_MESSAGES(messagekey, 0);

    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
    sys[SOMETHING] = "\" something. ";
}

void Mysterious64Sysmess(void)
{
    MAP_SYS_MESSAGES(c64_sysmess_keys, 0);
    /* Here YOUVE_SOLVED_IT is the 40th message, not the 39th */
    sys[YOUVE_SOLVED_IT] = system_messages[39];

    sys[ITEM_DELIMITER] = " - ";
    sys[MESSAGE_DELIMITER] = "\n";

    sys[YOU_SEE] = "\nThings I can see:\n";

    sys[I_DONT_KNOW_HOW_TO] = "\"";
    sys[PLAY_AGAIN] = "The game is over, thanks for playing\nWant to play again ? ";

    char *dictword = NULL;
    size_t len = GameHeader.WordLength;
    for (int i = 1; i <= 6; i++) {
        dictword = MemAlloc(len + 1);
        strncpy(dictword, sys[i - 1], GameHeader.WordLength);
        dictword[len] = 0;
        Nouns[i] = dictword;
    }

    Nouns[0] = "ANY";

    switch (CurrentGame) {
    case BATON_C64:
        Nouns[79] = "CAST";
        Verbs[79] = ".";
        GameHeader.NumWords = 79;
        break;
    case TIME_MACHINE_C64:
        Verbs[86] = ".";
        break;
    case ARROW1_C64:
    case PERSEUS_C64:
        Nouns[82] = ".";
        break;
    case ARROW2_C64:
        Verbs[80] = ".";
        break;
    case PULSAR7_C64:
        Nouns[102] = ".";
        break;
    case CIRCUS_C64:
        Nouns[96] = ".";
        break;
    case FEASIBILITY_C64:
        Nouns[80] = ".";
        break;
    default:
        break;
    }
}

void PerseusItalianSysmess(void)
{
    sys[YOU_ARE] = "Sono in ";
    sys[YOU_SEE] = "\nQui posso vedere:\n";
    sys[INVENTORY] = "Ho raccolto: ";

    /* The messages that the game does not have, with ' for an accent as in
       those that it has. It asks for Y or N itself, and the tape has
       "FILE ERRATO!!  Premi un tasto" and "<CARICAMENTO> ..Premi <ENTER>"
       for its own saved games. */
    sys[HIT_ENTER] = "<Premi ENTER> ";
    sys[YOU_HAVE_IT] = "Ce l'ho gia' ! ";
    sys[NONE] = "Nessuna";
    sys[NOTHING_HERE_TO_TAKE] = "Qui non c'e' niente da prendere. ";
    sys[YOU_HAVE_NOTHING] = "Non ho niente da lasciare. ";
    sys[LIGHT_GROWING_DIM] = "La mia luce si sta affievolendo. ";
    sys[WHAT] = "Cosa ? ";
    sys[YES] = "y";
    sys[NO] = "n";
    sys[ANSWER_YES_OR_NO] = "Rispondi Y o N.\n";
    sys[ARE_YOU_SURE] = "Sei sicuro?(Y/N) ";
    sys[MOVE_UNDONE] = "Mossa annullata. ";
    sys[CANT_UNDO_ON_FIRST_TURN] = "Non si puo' annullare la prima mossa. ";
    sys[NO_UNDO_STATES] = "Non ci sono altre mosse da annullare. ";
    sys[SAVED] = "Gioco salvato. ";
    sys[CANT_USE_ALL] = "Non puoi usare TUTTO con questo verbo. ";
    sys[TRANSCRIPT_ON] = "La trascrizione e' ora attiva.\n";
    sys[TRANSCRIPT_OFF] = "La trascrizione e' ora disattivata.\n";
    sys[NO_TRANSCRIPT] = "Nessuna trascrizione in corso.\n";
    sys[TRANSCRIPT_ALREADY] = "Una trascrizione e' gia' in corso.\n";
    sys[FAILED_TRANSCRIPT] = "Impossibile creare il file di trascrizione. ";
    sys[TRANSCRIPT_START] = "Inizio della trascrizione\n\n";
    sys[TRANSCRIPT_END] = "\n\nFine della trascrizione\n";
    sys[BAD_DATA] = "FILE ERRATO!! File di salvataggio non valido.\n";
    sys[STATE_SAVED] = "Stato salvato.\n";
    sys[STATE_RESTORED] = "Stato ripristinato.\n";
    sys[NO_SAVED_STATE] = "Nessuno stato salvato.\n";

    SetParserLanguage(PARSER_ITALIAN);
}

void Supergran64Sysmess(void)
{
    SysMessageType messagekey[] = {
        NORTH,
        SOUTH,
        EAST,
        WEST,
        UP,
        DOWN,
        EXITS,
        YOU_SEE,
        YOU_ARE,
        I_DONT_KNOW_WHAT_A,
        IS,
        YOU_CANT_GO_THAT_WAY,
        OK,
        WHAT_NOW,
        HUH,
        YOU_HAVE_IT,
        TAKEN,
        DROPPED,
        YOU_HAVENT_GOT_IT,
        INVENTORY,
        YOU_DONT_SEE_IT,
        THATS_BEYOND_MY_POWER,
        DIRECTION,
        YOURE_CARRYING_TOO_MUCH,
        IM_DEAD,
        PLAY_AGAIN,
        RESUME_A_SAVED_GAME,
        YOU_CANT_DO_THAT_YET,
        I_DONT_UNDERSTAND,
        NOTHING
    };

    MAP_SYS_MESSAGES(messagekey, 0);

    sys[I_DONT_KNOW_WHAT_A] = "\"";
    sys[IS] = "\" is a word I don't know. ";
}

void SecretMission64Sysmess(void)
{
    MAP_SYS_MESSAGES(c64_sysmess_keys, 0);

    sys[I_DONT_KNOW_HOW_TO] = "I don't know how to \"";
}
