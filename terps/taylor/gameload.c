//
//  gameload.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//
//  Loading the game file, identifying which game it is, and locating the
//  data tables (dictionary, tokens, messages, rooms, actions, ...) inside it.
//

#include <stdio.h>
#include <string.h>

#include "glk.h"
#include "glkstart.h"
#include "c64decrunch.h"
#include "common_file_utils.h"
#include "extracttape.h"
#include "graphics.h"
#include "player.h"
#include "textoutput.h"
#include "ui.h"
#include "utility.h"

#include "gameload.h"

uint8_t *FileImage = NULL;  /* Raw game file loaded into memory */
uint8_t *EndOfData = NULL;  /* One past the last byte of FileImage */
size_t FileImageLen = 0;
size_t VerbBase;            /* Offset of the verb/noun dictionary in FileImage */
size_t TokenBase;           /* Offset of the compressed token (text fragment) table */
size_t MessageBase;         /* Offset of the primary message text table */
size_t Message2Base;        /* Offset of the secondary message text table */
size_t RoomBase;            /* Offset of room description text */
size_t ObjectBase;          /* Offset of object description text */
size_t ExitBase;            /* Offset of room exit/connection table */
size_t ObjLocBase;          /* Offset of initial object location table */
size_t StatusBase;          /* Offset of automatic (status) action table */
size_t ActionBase;          /* Offset of player command action table */
size_t FlagBase;            /* Offset of initial flag values */
size_t AnimationData = 0;   /* Offset of Kayleth animation data (0 if none) */

/* Offset added to all table addresses to account for the file's load
   position differing from the original Z80 memory layout. */
long FileBaselineOffset = 0;

static char *Filename;
static uint8_t *CompanionFile; /* Second file for Temple of Terror hybrid mode */

/* Search FileImage for a byte pattern starting at base. Returns the
   offset if found, or (size_t)-1 if not found. */
size_t FindCode(const char *code, size_t base, size_t len)
{
    unsigned char *p = FileImage + base;
    while (p < FileImage + FileImageLen - len) {
        if (memcmp(p, code, len) == 0)
            return p - FileImage;
        p++;
    }
    return (size_t)-1;
}

/* Locate the initial flag data in the file image by searching for known
   byte patterns. Falls back to the Game struct's recorded offset. */
static size_t FindFlags(void)
{
    size_t pos;

    /* Questprobe */
    pos = FindCode("\xE7\x97\x51\x95\x5B\x7E\x5D\x7E\x76\x93", 0, 10);
    if (pos == -1) {
        /* Look for the flag initial block copy */
        pos = FindCode("\x01\x06\x00\xED\xB0\xC9\x00\xFD", 0, 8);
        if (pos == -1) {
            if (Game)
                return Game->start_of_flags + FileBaselineOffset;
            else {
                fprintf(stderr, "Cannot find initial flag data.\n");
                glk_exit();
            }
        } else
            return pos + 5;
    }
    return pos + 11;
}

/* Heuristic: check if the data at pos looks like a token table by
   counting lowercase ASCII characters in a fixed-size sample window. */
static int LooksLikeTokens(size_t pos)
{
    enum {
        TOKEN_SAMPLE_SIZE      = 512, /* bytes to inspect */
        TOKEN_SAMPLE_THRESHOLD = 300, /* >this many lowercase letters = likely tokens */
    };

    if (pos > FileImageLen || FileImageLen - pos < TOKEN_SAMPLE_SIZE)
        return 0;

    unsigned char *sample = FileImage + pos;
    int lowercase_count = 0;
    for (int i = 0; i < TOKEN_SAMPLE_SIZE; i++) {
        unsigned char c = sample[i] & TOKEN_BYTE_MASK;
        if (c >= 'a' && c <= 'z')
            lowercase_count++;
    }
    return lowercase_count > TOKEN_SAMPLE_THRESHOLD;
}

/* Detect version 0 format by checking for in-stream end markers
   (MSG_END_SPACE or MSG_END) in the token table. */
static void TokenClassify(size_t pos)
{
    unsigned char *ptr = FileImage + pos;
    int n = 0;
    while (n++ < 256) {
        do {
            if (*ptr == MSG_END_SPACE || *ptr == MSG_END)
                Version = 0;
        } while (!(*ptr++ & TOKEN_LAST_BYTE));
    }
}

/* Locate the token (text fragment) table by searching for known byte
   patterns. Tries game-specific signatures first, then falls back to
   generic Z80 instruction patterns and the "You are in " string. */
static size_t FindTokens(void)
{
    size_t addr;
    size_t pos = 0;

    if (Game)
        switch (CurrentGame) {
        case TOT_TEXT_ONLY_64:
        case TOT_HYBRID_64:
            if ((pos = FindCode("\x80\x59\x6f\x75\x20\x61\x72\x65\x20\x69", 0, 10)) != -1)
                return pos;
            break;
        case TEMPLE_OF_TERROR_64:
            if ((pos = FindCode("\x80\x20\x54\x68\x65\x72\x65\x20\x69\x73", 0, 10)) != -1)
                return pos;
            break;
        case REBEL_PLANET_64:
            if ((pos = FindCode("\xa7\x2e\xfe\x20\xfe\x2c\xfe\x21\xfe\x3f", 0, 10)) != -1)
                return pos;
            break;
        case QUESTPROBE3_64:
            if ((pos = FindCode("\x61\xa0\x64\xa0\x65\xa0\x67\xa0\x69\xa0", 0, 10)) != -1)
                return pos;
            break;
        case HEMAN_64:
        case KAYLETH_64:
            if ((pos = FindCode("\x80\x59\x6f\x75\x20\x61\x72\x65\x20\x69", 0, 10)) != -1)
                return pos;
            break;
        default:
            break;
        }

    do {
        pos = FindCode("\x47\xB7\x28\x0B\x2B\x23\xCB\x7E", pos + 1, 8);
        if (pos == -1) {
            /* Questprobe */
            pos = FindCode("\x58\x58\x58\x58\xFF", 0, 5);
            if (pos == -1) {
                /* Last resort */
                addr = FindCode("You are in ", 0, 11) - 1;
                if (addr == -1) {
                    if (Game)
                        return Game->start_of_tokens + FileBaselineOffset;
                    fprintf(stderr, "Unable to find token table.\n");
                    return 0;
                }
                return addr;
            } else
                return pos + 6;
        }
        addr = READ_LE_UINT16(FileImage + pos - 2) - ZX_RAM_BASE + FileBaselineOffset;
    } while (LooksLikeTokens(addr) == 0);
    TokenClassify(addr);
    return addr;
}

/* Resolve all data table offsets from the Game struct, applying
   FileBaselineOffset to convert from original Z80 addresses. */
void FindTables(void)
{
    TokenBase = FindTokens();
    RoomBase = Game->start_of_room_descriptions + FileBaselineOffset;
    ObjectBase = Game->start_of_item_descriptions + FileBaselineOffset;
    StatusBase = Game->start_of_automatics + FileBaselineOffset;
    ActionBase = Game->start_of_actions + FileBaselineOffset;
    ExitBase = Game->start_of_room_connections + FileBaselineOffset;
    FlagBase = FindFlags();
    ObjLocBase = Game->start_of_item_locations + FileBaselineOffset;
    MessageBase = Game->start_of_messages + FileBaselineOffset;
    Message2Base = Game->start_of_messages_2 + FileBaselineOffset;

    if (BaseGame == KAYLETH) {
        AnimationData = FindCode("\xff\x00\x00\x00\x0f\x00\x5d\x0f\x00\x61", 0, 10);
        if (AnimationData == -1)
            AnimationData = FindCode("\xff\x00\x00\x15\x0f\x00\x5d\x0f\x00\x61", 0, 10);
    }
}

/* Guess where "low" (room-description) objects end for Rebel Planet.
   Scans object descriptions looking for a comma at the end of an entry,
   which signals the start of "high" (standalone) objects. */
static int GuessLowObjectEndRebelPlanet(void)
{
    unsigned char *p = FileImage + ObjectBase;
    unsigned char *t = NULL;
    unsigned char c = 0, lc;
    int n = 0;

    while (p < EndOfData) {
        if (t == NULL)
            t = TokenText(*p++);
        lc = c;
        c = *t & TOKEN_BYTE_MASK;
        if (c == MSG_END_SPACE || c == MSG_END) {
            if (lc == ',' && n > 20)
                return n;
            n++;
        }
        if (*t++ & TOKEN_LAST_BYTE)
            t = NULL;
    }
    return -1;
}

/* Guess the boundary between low objects (embedded in room descriptions)
   and high objects (listed separately after "You see:"). Low object
   descriptions end with a comma in the last token. */
int GuessLowObjectEnd(void)
{
    /* Can't automatically guess in this case */
    if (CurrentGame == BLIZZARD_PASS)
        return 70;
    else if (Version == QUESTPROBE3_TYPE)
        return 49;

    if (Version == REBEL_PLANET_TYPE)
        return GuessLowObjectEndRebelPlanet();

    unsigned char *obj_text = FileImage + ObjectBase;
    int obj_index = 0;

    while (obj_index < NumObjects()) {
        while (*obj_text != MSG_END && *obj_text != MSG_END_SPACE) {
            obj_text++;
        }
        /* obj_text[-1] is the last token index of the description; find
           that token's last byte to check whether it ends in a comma. */
        unsigned char *last_byte = TokenText(obj_text[-1]);
        while (!(*last_byte & TOKEN_LAST_BYTE)) {
            last_byte++;
        }
        if ((*last_byte & TOKEN_BYTE_MASK) == ',')
            return obj_index;
        obj_index++;
        obj_text++;
    }
    fprintf(stderr, "Unable to guess the last description object.\n");
    return 0;
}

int glkunix_startup_code(glkunix_startup_t *data)
{
    int argc = data->argc;
    char **argv = data->argv;

    if (argc < 1)
        return 0;

    if (argc > 1)
        while (argv[1]) {
            if (*argv[1] != '-')
                break;
            switch (argv[1][1]) {
            case 'n':
                Options |= NO_DELAYS;
                break;
            }
            argv++;
            argc--;
        }

    if (argv[1] == NULL) {
        fprintf(stderr, "%s: <file>.\n", argv[0]);
        glk_exit();
    }

    size_t namelen = strlen(argv[1]);
    Filename = MemAlloc(namelen + 1);
    strncpy(Filename, argv[1], namelen);
    Filename[namelen] = '\0';

    FileImage = ReadFileIfExists(Filename, &FileImageLen);
    if (FileImage == NULL) {
        perror(Filename);
        CleanupAndExit();
    }

    FileImage = ProcessFile(FileImage, &FileImageLen);
    EndOfData = FileImage + FileImageLen;

#ifdef GARGLK
    garglk_set_program_name("TaylorMade 0.4");
    garglk_set_program_info("TaylorMade 0.4 by Alan Cox\n"
                            "Glk port, graphics and Questprobe 3 support by Petter Sjölund\n");
    const char *s;
    if ((s = strrchr(Filename, '/')) != NULL || (s = strrchr(Filename, '\\')) != NULL) {
        garglk_set_story_name(s + 1);
    } else {
        garglk_set_story_name(Filename);
    }
#endif

    return 1;
}

/* Identify the game by trying each entry in the games[] table. For each
   candidate, compute the baseline offset from the verb table and check
   whether the token table falls at the expected relative position. */
GameInfo *DetectGame(size_t LocalVerbBase)
{
    GameInfo *LocalGame;

    for (int i = 0; i < NUMGAMES; i++) {
        LocalGame = &games[i];
        FileBaselineOffset = (long)LocalVerbBase - (long)LocalGame->start_of_dictionary;
        long diff = FindTokens() - LocalVerbBase;
        if ((LocalGame->start_of_tokens - LocalGame->start_of_dictionary) == diff) {
#ifdef DEBUG
            fprintf(stderr, "This is %s\n", LocalGame->Title);
#endif
            return LocalGame;
        } else {
#ifdef DEBUG
            fprintf(stderr, "Diff for game %s: %d. Looking for %ld\n", LocalGame->Title, LocalGame->start_of_tokens - LocalGame->start_of_dictionary, diff);
#endif
        }
    }
    return NULL;
}

/* Restore the FileImage globals to a previously saved state. Used when
   switching back from a companion file. */
static void UnparkFileImage(uint8_t *ParkedFile, size_t ParkedLength, long ParkedOffset, int FreeCompanion)
{
    FileImage = ParkedFile;
    FileImageLen = ParkedLength;
    FileBaselineOffset = ParkedOffset;
    if (FreeCompanion)
        free(CompanionFile);
}

/* Temple of Terror has separate graphics and text-only versions. If we
   loaded one, look for the other by swapping the last character before
   the extension ('a' <-> 'b'). If found, offer to create a hybrid that
   uses the text-only version's longer prose with the graphics version's
   images. */
void LookForSecondTOTGame(void)
{
    size_t namelen = strlen(Filename);
    char *secondfile = MemAlloc(namelen + 1);
    strncpy(secondfile, Filename, namelen);
    secondfile[namelen] = '\0';

    char *period = strrchr(secondfile, '.');
    if (period == NULL)
        period = &secondfile[namelen - 1];
    else
        period--;

    if (CurrentGame == TEMPLE_OF_TERROR)
        *period = 'b';
    else
        *period = 'a';

    size_t filelength;

    CompanionFile = ReadFileIfExists(secondfile, &filelength);

    if (CompanionFile == NULL) {
        return;
    }

    uint8_t *ParkedFile = FileImage;
    size_t ParkedLength = FileImageLen;
    size_t ParkedOffset = FileBaselineOffset;

    CompanionFile = ProcessFile(CompanionFile, &filelength);

    FileImage = CompanionFile;
    FileImageLen = filelength;

    size_t AltVerbBase = FindCode("NORT\001N", 0, 6);

    if (AltVerbBase == -1) {
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 1);
        return;
    }

    GameInfo *AltGame = DetectGame(AltVerbBase);

    if ((CurrentGame == TOT_TEXT_ONLY && AltGame->gameID != TEMPLE_OF_TERROR) || (CurrentGame == TEMPLE_OF_TERROR && AltGame->gameID != TOT_TEXT_ONLY)) {
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 1);
        return;
    }

    Display(Bottom, "Found files for both the text-only version and the graphics version of Temple of Terror.\n"
                    "Would you like to use the longer texts from the text-only version along with the graphics from the other file? (Y/N) ");
    if (!YesOrNo()) {
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 1);
        return;
    }

    int index = 0;

    if (CurrentGame == TOT_TEXT_ONLY) {
        while (Game->gameID != TOT_HYBRID) {
            Game = &games[index++];
        }
        InitGraphics();
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 0);
    } else {
        UnparkFileImage(ParkedFile, ParkedLength, ParkedOffset, 0);
        InitGraphics();
        while (Game->gameID != TOT_HYBRID) {
            Game = &games[index++];
        }
        FileImage = CompanionFile;
        FileImageLen = filelength;
        VerbBase = AltVerbBase;
    }

    EndOfData = FileImage + FileImageLen;
}
