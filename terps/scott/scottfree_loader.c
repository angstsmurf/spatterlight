/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  scottfree_loader.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Loader for the plain text ScottFree database format (.dat files).
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "dos_images.h"
#include "scott.h"
#include "scott_database.h"
#include "uk_loader.h"

#include "scottfree_loader.h"

#define READSTRING_BUFFER_SIZE 1024

#define HULK_DOS_ID 703
#define HULK_DOS_WIDTH 280
#define HULK_DOS_HEIGHT 158

/* Read a double-quoted string from a ScottFree plaintext database file.
   Handles escaped quotes (""), backtick-to-quote conversion, and
   strips non-ASCII characters for Glk compatibility. */
static char *ReadString(FILE *f)
{
    char tmp[READSTRING_BUFFER_SIZE];
    int c;
    int ct = 0;
    do {
        c = fgetc(f);
    } while (c != EOF && isspace((unsigned char)c));
    if (c != '"')
        Fatal("Initial quote expected");

    for (;;) {
        c = fgetc(f);
        if (c == EOF)
            Fatal("EOF in string");
        if (c == '"') {
            int nc = fgetc(f);
            if (nc != '"') {
                ungetc(nc, f);
                break;
            }
        }
        if (c == '`')
            c = '"';
        /* Ignore CR (assume CRLF in DOS-formatted files) */
        if (c == '\r')
            continue;
        /* Keep consuming to the closing quote so the file stays in sync, but
           never write past the buffer: a string longer than the buffer (a
           corrupt or hostile database) would otherwise smash the stack, since
           the terminator is fully controlled by the file. Reserve one byte for
           the trailing NUL. */
        if (ct >= READSTRING_BUFFER_SIZE - 1)
            continue;
        /* Pass ASCII and newline to Glk; replace anything else */
        if (c == '\n' || (c >= ' ' && c <= '~'))
            tmp[ct++] = c;
        else
            tmp[ct++] = '?';
    }
    return MemStrndup(tmp, ct);
}

/* Read and range-check the header. Returns 0 if it is not a usable one. */
static int LoadHeader(FILE *f)
{
    if (fscanf(f, "%*d %hd %hd %hd %hd %hd %hd %hd %hd %hd %hd %hd",
            &GameHeader.NumItems, &GameHeader.NumActions,
            &GameHeader.NumWords, &GameHeader.NumRooms,
            &GameHeader.MaxCarry, &GameHeader.PlayerRoom,
            &GameHeader.Treasures, &GameHeader.WordLength,
            &GameHeader.LightTime, &GameHeader.NumMessages,
            &GameHeader.TreasureRoom)
        < 10) {
        LoadTrace("Invalid database(bad header)\n");
        return 0;
    }
    /* The counts size the game arrays, and the player is placed in
       PlayerRoom without further checks */
    if (GameHeader.NumItems < 0 || GameHeader.NumActions < 0
        || GameHeader.NumWords < 0 || GameHeader.NumRooms < 0
        || GameHeader.NumMessages < 0 || GameHeader.WordLength < 1
        || GameHeader.PlayerRoom < 0
        || GameHeader.PlayerRoom > GameHeader.NumRooms) {
        LoadTrace("Invalid database(header values out of range)\n");
        return 0;
    }
    return 1;
}

static int LoadActions(FILE *f)
{
    LoadTrace("Reading %d actions.\n", GameHeader.NumActions);
    for (int ct = 0; ct <= GameHeader.NumActions; ct++) {
        if (fscanf(f, "%hu %hu %hu %hu %hu %hu %hu %hu",
                &Actions[ct].Vocab,
                &Actions[ct].Condition[0],
                &Actions[ct].Condition[1],
                &Actions[ct].Condition[2],
                &Actions[ct].Condition[3],
                &Actions[ct].Condition[4],
                &Actions[ct].Opcode[0],
                &Actions[ct].Opcode[1])
            != 8) {
            debug_print("Bad action line (%d)\n", ct);
            return 0;
        }

        if (Options & DEBUGGING) {
            LoadTrace("Action %d Vocab: %d (Verb:%d/NounOrChance:%d)\n", ct, Actions[ct].Vocab,
               Actions[ct].Vocab / VOCAB_MULTIPLIER, Actions[ct].Vocab % VOCAB_MULTIPLIER);
            for (int i = 0; i < NUM_CONDITIONS; i++)
                LoadTrace("Action %d Condition[%d]: %d (%d/%d)\n", ct, i,
                    Actions[ct].Condition[i], Actions[ct].Condition[i] % CONDITION_MULTIPLIER, Actions[ct].Condition[i] / CONDITION_MULTIPLIER);
            LoadTrace("Action %d Opcode [0]]: %d (%d/%d)\n", ct, Actions[ct].Opcode[0], Actions[ct].Opcode[0] % VOCAB_MULTIPLIER, Actions[ct].Opcode[0] / VOCAB_MULTIPLIER);
            LoadTrace("Action %d Opcode [1]]: %d (%d/%d)\n", ct, Actions[ct].Opcode[1], Actions[ct].Opcode[1] % VOCAB_MULTIPLIER, Actions[ct].Opcode[1] / VOCAB_MULTIPLIER);
        }
    }
    return 1;
}

/* Load the verb/noun dictionary (word pairs) */
static void LoadWords(FILE *f)
{
    LoadTrace("Reading %d word pairs.\n", GameHeader.NumWords);
    for (int ct = 0; ct <= GameHeader.NumWords; ct++) {
        Verbs[ct] = ReadString(f);
        LoadTrace("Verbs %d:%s.\n", ct, Verbs[ct]);
        Nouns[ct] = ReadString(f);
        LoadTrace("Nouns %d:%s.\n", ct, Nouns[ct]);
    }
}

/* Load rooms: 6 exit directions followed by a description string */
static int LoadRooms(FILE *f)
{
    LoadTrace("Reading %d rooms.\n", GameHeader.NumRooms);
    for (int ct = 0; ct <= GameHeader.NumRooms; ct++) {
        int exits[NUM_EXITS];
        if (fscanf(f, "%d %d %d %d %d %d",
                &exits[0], &exits[1], &exits[2],
                &exits[3], &exits[4], &exits[5])
            != NUM_EXITS) {
            debug_print("Bad room line (%d)\n", ct);
            return 0;
        }
        for (int i = 0; i < NUM_EXITS; i++)
            Rooms[ct].Exits[i] = exits[i];

        Rooms[ct].Text = ReadString(f);
        if (Options & DEBUGGING) {
            LoadTrace("Room %d: \"%s\"\n", ct, Rooms[ct].Text);
            LoadTrace("Room connections for room %d:\n", ct);
            for (int i = 0; i < NUM_EXITS; i++)
                LoadTrace("Exit %d: %d\n", i, Rooms[ct].Exits[i]);
        }
        Rooms[ct].Image = NO_IMAGE;
    }
    return 1;
}

/* Load messages (printed by action ops 1-51 and 102+) */
static void LoadMessages(FILE *f)
{
    LoadTrace("Reading %d messages.\n", GameHeader.NumMessages);
    for (int ct = 0; ct <= GameHeader.NumMessages; ct++) {
        Messages[ct] = ReadString(f);
        LoadTrace("Message %d: \"%s\"\n", ct, Messages[ct]);
    }
}

/* Load items: description string (with optional /AutoGet/ word) and
   location */
static int LoadItems(FILE *f)
{
    LoadTrace("Reading %d items.\n", GameHeader.NumItems);
    for (int ct = 0; ct <= GameHeader.NumItems; ct++) {
        Items[ct].Text = ReadString(f);
        LoadTrace("Item %d: \"%s\"\n", ct, Items[ct].Text);
        ParseItemSlashAutoGet(ct);
        int location;
        if (fscanf(f, "%d", &location) != 1) {
            debug_print("Bad item line (%d)\n", ct);
            return 0;
        }
        Items[ct].Location = (unsigned char)location;
        LoadTrace("Location of item %d: %d\n", ct, Items[ct].Location);
        Items[ct].InitialLoc = Items[ct].Location;
    }
    return 1;
}

/* Read the version and adventure number trailer */
static int LoadTrailer(FILE *f)
{
    int val;
    if (fscanf(f, "%d", &val) != 1) {
        debug_print("Cannot read version\n");
        return 0;
    }
    LoadTrace("Version %d.%02d of Adventure ", val / 100, val % 100);
    if (fscanf(f, "%d", &val) != 1) {
        debug_print("Cannot read adventure number\n");
        return 0;
    }
    LoadTrace("%d.\nLoad Complete.\n\n", val);
    return 1;
}

/* Load a ScottFree plaintext database from a FILE stream.

   Reads the header, actions, word pairs (verb/noun dictionary), rooms
   with exits, messages, items with locations, then the version/adventure
   number trailer. Returns SCOTTFREE on success, HULK_US if the DOS Hulk
   images are found, or UNKNOWN_GAME on parse failure. The caller
   closes f.

   The file format is the standard Scott Adams plain-text format as
   used by the original IBM PC releases, the ScottKit toolkit,
   and compatible tools. */
GameIDType LoadDatabase(FILE *f)
{
    if (!LoadHeader(f))
        return UNKNOWN_GAME;
    LightRefill = GameHeader.LightTime;
    AllocateGameData();

    if (Options & DEBUGGING)
        PrintHeaderInfo(NULL, &GameHeader);

    int ok = LoadActions(f);
    if (ok) {
        LoadWords(f);
        ok = LoadRooms(f);
    }
    if (ok) {
        LoadMessages(f);
        ok = LoadItems(f);
    }
    if (ok) {
        /* Discard action comment strings (one per action, used only by
           the original authoring tools for documentation) */
        for (int ct = 0; ct <= GameHeader.NumActions; ct++)
            free(ReadString(f));
        ok = LoadTrailer(f);
    }
    if (!ok) {
        FreeDatabase();
        return UNKNOWN_GAME;
    }

    /* Extra value in at least Hulk */
    int extra = 0;
    if (fscanf(f, "%d", &extra) != 1) {
        debug_print("No extra value in file. This is not Hulk.\n");
        extra = 0;
    }

    if (extra == HULK_DOS_ID && LoadDOSImages()) {
        CurrentSys = SYS_MSDOS;
        ImageWidth = HULK_DOS_WIDTH;
        ImageHeight = HULK_DOS_HEIGHT;
        return HULK_US;
    }
    return SCOTTFREE;
}

