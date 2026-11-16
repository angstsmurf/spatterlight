/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  scott_database.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  The game database in memory: the header, rooms, items, actions,
 *  messages and dictionary that the four loaders fill in, how they are
 *  allocated and freed, and the look-ups by dictionary word.
 */

#include <stdlib.h>
#include <string.h>

#include "bsd.h"
#include "scott.h"

#include "scott_database.h"

/* Core game data — populated by LoadDatabase or the SAGA loaders */
Header GameHeader;
Item *Items = NULL;
Room *Rooms = NULL;
const char **Verbs = NULL;
const char **Nouns = NULL;
const char **Messages = NULL;
Action *Actions = NULL;

/* Copy parsed header values into the global GameHeader (leaving its Unknown
   field alone) and refill the light source. */
void SetGameHeader(const Header *h)
{
    GameHeader.NumItems = h->NumItems;
    GameHeader.NumActions = h->NumActions;
    GameHeader.NumWords = h->NumWords;
    GameHeader.WordLength = h->WordLength;
    GameHeader.NumRooms = h->NumRooms;
    GameHeader.MaxCarry = h->MaxCarry;
    GameHeader.PlayerRoom = h->PlayerRoom;
    GameHeader.Treasures = h->Treasures;
    GameHeader.LightTime = h->LightTime;
    LightRefill = h->LightTime;
    GameHeader.NumMessages = h->NumMessages;
    GameHeader.TreasureRoom = h->TreasureRoom;
}

void AllocateGameData(void)
{
    /* The lamp is item LIGHT_SOURCE (9) by convention, and the engine reads
       Items[LIGHT_SOURCE] every turn for the lamp timer without consulting
       NumItems. Every real Scott database has at least ten items, but a
       malformed one need not, and the read would then be out of bounds. Always
       allocate through LIGHT_SOURCE so it reads back as a destroyed item. */
    int num_items = GameHeader.NumItems + 1;
    if (num_items < LIGHT_SOURCE + 1)
        num_items = LIGHT_SOURCE + 1;

    /* Zero-initialise: the engine treats an unset Room.Text / Item.Image /
       Item.Flag as "absent" (e.g. Look()'s `if (!r->Text) return;`), so any
       field a particular loader doesn't fill must read back as NULL/0 rather
       than as heap garbage. */
    Items = MemCalloc(sizeof(Item) * num_items);
    Actions = MemCalloc(sizeof(Action) * (GameHeader.NumActions + 1));
    Verbs = MemCalloc(sizeof(char *) * (GameHeader.NumWords + 2));
    Nouns = MemCalloc(sizeof(char *) * (GameHeader.NumWords + 2));
    Rooms = MemCalloc(sizeof(Room) * (GameHeader.NumRooms + 1));
    Messages = MemCalloc(sizeof(char *) * (GameHeader.NumMessages + 1));
}

/* Free a database string, unless it is unset (a database that failed to
   load halfway will have NULL strings in its arrays) or the shared "."
   placeholder. */
static void FreeDatabaseString(char *string)
{
    if (string != NULL && string[0] != '.')
        free(string);
}

/* Free entries 0..last of a string array, then the array itself. The
   arrays are const for the parser, which only reads them; the loaders
   allocated the strings, so the cast is the one place that owns them. */
static void FreeStringArray(const char **array, int last)
{
    if (array == NULL)
        return;
    for (int i = 0; i <= last; i++)
        FreeDatabaseString((char *)array[i]);
    free(array);
}

/* Free all dynamically allocated game data arrays (actions, items, rooms,
   dictionary, messages). Frees individual string elements that aren't
   the shared "." placeholder, then frees the arrays and NULLs them. */
void FreeDatabase(void)
{
    if (Actions != NULL)
        free(Actions);
    if (Items != NULL) {
        for (int i = 0; i <= GameHeader.NumItems; i++)
            FreeDatabaseString(Items[i].Text);
        free(Items);
    }
    if (Rooms != NULL) {
        for (int i = 0; i <= GameHeader.NumRooms; i++)
            FreeDatabaseString(Rooms[i].Text);
        free(Rooms);
    }
    FreeStringArray(Nouns, GameHeader.NumWords);
    FreeStringArray(Verbs, GameHeader.NumWords);
    FreeStringArray(Messages, GameHeader.NumMessages);

    Items = NULL;
    Actions = NULL;
    Verbs = NULL;
    Nouns = NULL;
    Rooms = NULL;
    Messages = NULL;
}

/* Map a noun index to its canonical (non-synonym) word.
   Synonyms in the dictionary are prefixed with '*' and share
   the word number of the preceding canonical entry. */
const char *MapSynonym(int noun)
{
    int n = 1;
    const char *lastword = ""; /* Last non synonym */
    while (n <= GameHeader.NumWords) {
        if (*Nouns[n] != '*')
            lastword = Nouns[n];
        if (n == noun)
            return (lastword);
        n++;
    }
    return (NULL);
}

/* Find the item whose AutoGet word matches the given noun at a given location.
   Returns the item index, or -1 if no match. loc=0 matches any location. */
int MatchUpItem(int noun, int loc)
{
    const char *word = MapSynonym(noun);
    int ct = 0;

    if (word == NULL)
        word = Nouns[noun];

    while (ct <= GameHeader.NumItems) {
        if (Items[ct].AutoGet && (loc == 0 || Items[ct].Location == loc) && xstrncasecmp(Items[ct].AutoGet, word, GameHeader.WordLength) == 0)
            return (ct);
        ct++;
    }
    return (-1);
}

/* Extract the auto-get/drop word from an item description.
   Item text uses the format "description/WORD/" where WORD is the
   vocabulary word the engine uses for automatic GET/DROP commands.
   This splits the text at the first '/' (NUL-terminating the
   description) and points AutoGet at the word between the slashes.
   Sets AutoGet to NULL if no word is present or if the suffix is
   "//" or "[slash]*" (conventions meaning "no auto-get word"). */
void ParseItemSlashAutoGet(int index)
{
    char *word = strchr(Items[index].Text, '/');
    Items[index].AutoGet = NULL;
    if (word == NULL || strcmp(word, "//") == 0 || strcmp(word, "/*") == 0)
        return;
    /* NUL-terminate the description and advance past the '/' */
    *word++ = 0;
    /* Strip the trailing '/' from the word */
    char *trailingslash = strchr(word, '/');
    if (trailingslash != NULL)
        *trailingslash = 0;
    Items[index].AutoGet = word;
}

