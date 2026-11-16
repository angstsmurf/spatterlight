/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  us_loader.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Loader for the binary database format of the US (SAGA) games.
 *
 *  Created by Petter Sjölund on 2022-09-29.
 */

#include <string.h>

#include "uk_loader.h"
#include "scott.h"
#include "scott_database.h"

#include "us_loader.h"

/* Parse the US-format dictionary from binary game data.

   Words are stored as fixed-length strings (GameHeader.WordLength chars each).
   The dictionary contains nouns first, then verbs, with (NumWords + 1) entries
   of each. A '*' character in the stream does not count toward word length
   (used as a synonym marker). A zero byte at the start of a word is skipped
   (padding). A character with the high bit set (> 127) signals end of
   dictionary. */
static uint8_t *ReadUSDictionary(uint8_t *ptr)
{
    char dictword[32];
    char c = 0;
    int charindex = 0;

    int nn = GameHeader.NumWords + 1;

    for (int wordnum = 0; wordnum <= GameHeader.NumWords * 2 + 1; wordnum++) {
        for (int i = 0; i < GameHeader.WordLength; i++) {
            c = *(ptr++);
            /* Skip leading zero-padding at the start of a word */
            if (c == 0 && charindex == 0)
                c = *(ptr++);
            dictword[charindex++] = c;
            /* '*' marks a synonym; don't count it toward the word length */
            if (c == '*')
                i--;
        }
        dictword[charindex] = 0;

        /* First (NumWords + 1) entries are nouns, the rest are verbs */
        char *word = MemAlloc(charindex + 1);
        memcpy(word, dictword, charindex + 1);
        if (wordnum < nn) {
            Nouns[wordnum] = word;
            debug_print("Nouns %d: \"%s\"\n", wordnum, word);
        } else {
            Verbs[wordnum - nn] = word;
            debug_print("Verbs %d: \"%s\"\n", wordnum - nn, word);
        }

        /* High bit set on last character signals end of dictionary */
        if (c > 127)
            return ptr;

        charindex = 0;
    }

    return ptr;
}

/* Quick plausibility check on parsed header values to reject files that
   aren't actually Scott Adams games. Verifies that item, action, word,
   room, and message counts fall within reasonable bounds. */
static int SanityCheckScottFreeHeader(void)
{
    int16_t v = header[1]; /* items */
    if (v < 10 || v > 500)
        return 0;
    v = header[2]; /* actions */
    if (v < 100 || v > 500)
        return 0;
    v = header[3]; /* word pairs */
    if (v < 48 || v > 200)
        return 0;
    v = header[4]; /* rooms */
    if (v < 10 || v > 100)
        return 0;
    v = header[5]; /* messages */
    if (v < 10 || v > 255)
        return 0;

    return 1;
}

/* Skip over 'count' bytes of data that we don't need to parse
   (e.g. lookup tables). In debug builds, dumps the skipped values. */
uint8_t *Skip(uint8_t *ptr, int count, uint8_t *eof)
{
#if (DEBUG_PRINT)
    for (int i = 0; i < count && ptr + i + 1 < eof; i += 2) {
        uint16_t val = READ_LE_UINT16(ptr + i);
        debug_print("Unknown value %d: %d (%x)\n", i / 2, val, val);
    }
#else
    (void)eof;
#endif
    return ptr + count;
}

/* Parse the game file header and allocate all game data arrays.

   When dict_start > 0, we're probing for the UK release of Hulk: the
   dictionary position is known, so we calculate a baseline offset to
   compensate for any extra data prepended to the file.

   When dict_start == 0, this is a standard US-format binary database.
   The first 0x38 bytes contain version and adventure number fields, which are
   used to identify the specific game title. */
static uint8_t *LoadHeader(uint8_t *ptr, size_t length, GameInfo info, int dict_start) {
    Header hdr;

    size_t offset;

    uint8_t *data = ptr;

    if (dict_start) {
        /* UK Hulk detection: compute file offset adjustment from known
           dictionary position */
        file_baseline_offset = dict_start - info.start_of_dictionary - 645;
        debug_print("LoadBinaryDatabase: file baseline offset:%d\n",
                    file_baseline_offset);
        offset = info.start_of_header + file_baseline_offset;
        if (offset < length)
            ptr = data + offset;
        else
            return NULL;
    } else {
        /* Standard US binary: scan the preamble for version and adventure
           number. These are the first two values under 500 in the 0x38-byte
           preamble. */
        if (length < 0x38)
            return NULL;
        int version = 0;
        int adventure_number = 0;
        for (int i = 0; i < 0x38; i += 2) {
            int value = READ_LE_UINT16(ptr + i);
            if (value < 500) {
                if (version == 0)
                    version = value;
                else {
                    adventure_number = value;
                    break;
                }
            }
        }
        debug_print("Version: %d\n", version);
        debug_print("Adventure number: %d\n", adventure_number);
        if (adventure_number == 0)
            return NULL;
        /* Map known version/adventure pairs to specific game IDs */
        if (version == 127 && adventure_number == 1)
            CurrentGame = HULK_US;
        else if (version == 126) {
            if (adventure_number == 2)
                CurrentGame = HULK_US_PREL;
            else if (adventure_number == 13)
                CurrentGame = CLAYMORGUE_US_126;
        } else
            CurrentGame = adventure_number;
        Game->type = US_VARIANT;
        ptr = Skip(ptr, 0x38, data + length);
    }

    if (ptr == NULL || ptr + 30 > data + length)
        return NULL;

    ptr = ReadHeader(ptr);

    ParseHeader(header, US_HEADER, &hdr);
    PrintHeaderInfo(header, &hdr);

    if (!SanityCheckScottFreeHeader())
        return NULL;

    SetGameHeader(&hdr);
    AllocateGameData();

    /* When probing for UK Hulk, verify the parsed header matches the
       expected values from the game database. A mismatch means this
       isn't the game we're looking for. */
    if (dict_start) {
        if (header[0] != info.word_length
            || header[1] != info.number_of_words
            || header[2] != info.number_of_actions
            || header[3] != info.number_of_items
            || header[4] != info.number_of_messages
            || header[5] != info.number_of_rooms
            || header[6] != info.max_carried) {
            return NULL;
        }
    }
    return ptr;
}

/* Locate and parse the dictionary. The dictionary always starts with
   the word "ANY" (the universal noun), so we scan forward until we
   find that marker, then hand off to ReadUSDictionary. */
static uint8_t *ParseDictionary(uint8_t *ptr, uint8_t *endptr) {
    while (!(*ptr == 'A' && *(ptr + 1) == 'N' && *(ptr + 2) == 'Y') && ptr < endptr)
        ptr++;

    if (ptr >= endptr)
        return NULL;

    return ReadUSDictionary(ptr);
}

/* Read and return a length-prefixed string and update the pointer.
   If string length is 0, return a period string literal (".") */
static char *ReadPascalString(uint8_t **inptr, uint8_t *endptr) {
    char *string = ".";
    uint8_t *ptr = *inptr;
    uint8_t string_length = *ptr++;
    if (string_length != 0 && ptr + string_length < endptr) {
        string = MemAlloc(string_length + 1);
        memcpy(string, ptr, string_length);
        string[string_length] = 0;
        ptr += string_length;
    }
    *inptr = ptr;
    return string;
}

/* Parse room descriptions from the binary data. Each room is stored as
   a length-prefixed string (1 byte length + that many characters). */
uint8_t *ParseRooms(uint8_t *ptr, uint8_t *endptr, int number_of_rooms) {
    Room *rp = Rooms;
    for (int counter = 0; counter <= number_of_rooms; counter++) {
        rp->Text = ReadPascalString(&ptr, endptr);
        debug_print("Room %d: \"%s\"\n", counter, rp->Text);
        rp++;
    }
    return ptr;
}

/* Parse action messages from the binary data. Same length-prefixed
   string format as rooms. */
uint8_t *ParseMessages(uint8_t *ptr, uint8_t *endptr, int number_of_messages) {
    int counter = 0;
    for (counter = 0; counter <= number_of_messages; counter++) {
        Messages[counter] = ReadPascalString(&ptr,endptr);
        debug_print("Message %d: \"%s\"\n", counter, Messages[counter]);
    }
    return ptr;
}

/* Parse item descriptions and initial locations from binary data.

   Item descriptions are length-prefixed strings. An item name containing
   a '/' delimiter has an auto-get/drop word after it (e.g. "Lamp/LAM").
   "//" or "[slash]*" means no auto-get word.

   After all item strings, a separate table of [stride]-byte entries gives each
   item's starting location. */
uint8_t *ParseItems(uint8_t *ptr, uint8_t *endptr, int number_of_items, int stride) {
    for (int counter = 0; counter <= number_of_items && ptr < endptr; counter++) {
        Items[counter].Text = ReadPascalString(&ptr, endptr);

        ParseItemSlashAutoGet(counter);
        if (Items[counter].AutoGet != NULL && ptr < endptr)
            ptr++;

        debug_print("Item %d: %s\n", counter, Items[counter].Text);
        if (Items[counter].AutoGet)
            debug_print("Autoget:%s\n", Items[counter].AutoGet);
    }

    ptr++;
    for (int ct = 0; ct <= number_of_items && ptr < endptr; ct++) {
        Items[ct].Location = *ptr;
        Items[ct].InitialLoc = Items[ct].Location;
        debug_print("Item %d (%s) start location: %d\n", ct, Items[ct].Text, Items[ct].Location);
        ptr += stride;
    }

    return ptr;
}

/* Parse the action table from binary data.

   The US binary format stores action data in a columnar layout rather
   than row-by-row: all verb bytes come first (one per action), then all
   noun bytes, then opcode bytes (2 pairs of 2 columns), then
   condition words (5 columns of 16-bit values). Each column has
   (number_of_actions + 1) entries.

   The verb and noun are packed into Vocab as (verb * 150 + noun).
   Action opcodes are similarly packed as (value * 150 + value2).
   Conditions are 16-bit words encoding condition type and argument. */
uint8_t *ParseActions(uint8_t *ptr, uint8_t *data, size_t datalength, int number_of_actions, int big_endian) {
    size_t base = ptr - data;
    size_t stride = number_of_actions + 1;

    /* The byte columns region spans 6 columns of 'stride' bytes each.
       The condition region follows: 5 columns of 'stride' 16-bit words. */
    size_t cond_base = base + 6 * stride;
    size_t cond_stride = stride * 2;

    /* Verify the entire action table fits within the data */
    size_t end_of_table = cond_base + 5 * cond_stride;
    if (end_of_table > datalength)
        return NULL;

    for (int ct = 0; ct <= number_of_actions; ct++) {
        /* Column 0: verbs, Column 1: nouns */
        int verb = data[base + ct];
        int noun = data[base + ct + stride];
        debug_print("Action %d: verb:%d noun:%d\n", ct, verb, noun);

        Actions[ct].Vocab = verb * 150 + noun;

        /* Columns 2-5: two opcode pairs (each pair is two byte columns) */
        for (int j = 0; j < 2; j++) {
            int value  = data[base + ct + (2 + j * 2) * stride];
            int value2 = data[base + ct + (3 + j * 2) * stride];
            Actions[ct].Opcode[j] = 150 * value + value2;
            debug_print("Action %d: Opcode[%d]: %d %d\n", ct, j, value, value2);
        }

        /* 5 columns of 16-bit condition words */
        for (int j = 0; j < 5; j++) {
            size_t off = cond_base + ct * 2 + j * cond_stride;
            Actions[ct].Condition[j] = big_endian
                ? READ_BE_UINT16(data + off)
                : READ_LE_UINT16(data + off);
            debug_print("Action %d: Condition %d: %d\n", ct, j, Actions[ct].Condition[j]);
        }
    }

    return data + end_of_table;
}

/* Parse room exit connections from binary data.
   Like the action table, exits are stored in columnar layout: all North
   exits for every room, then all South exits, etc. (6 directions total).
   Each exit is a [stride]-byte value giving the destination room number. */
uint8_t *ParseRoomConnections(uint8_t *ptr, uint8_t *endptr, int number_of_rooms, int stride) {
    for (int j = 0; j < 6; j++) {
        int counter = 0;
        Room *rp = Rooms;

        while (counter <= number_of_rooms && ptr < endptr) {
            rp->Image = 255;
            rp->Exits[j] = *ptr;
            debug_print("Room %d (%s) exit %d (%s): %d\n", counter, Rooms[counter].Text, j, Nouns[j + 1], rp->Exits[j]);
            ptr += stride;
            counter++;
            rp++;
        }
    }
    return ptr;
}

/* Additional parsing for the UK release of Questprobe featuring The Hulk.
   The UK version stores room-to-image and item-to-image mapping tables
   at known offsets that the standard US parser doesn't handle. */
static GameIDType HulkUKSpecificParsing(uint8_t *ptr, uint8_t *endptr, GameInfo info) {
/* MARK: room images */
    size_t offset;

    if (SeekIfNeeded(info.start_of_room_image_list, &offset, &ptr) == 0) {
        FreeDatabase();
        return UNKNOWN_GAME;
    }

    Room *rp = Rooms;

    for (int i = 0; i <= GameHeader.NumRooms && ptr < endptr; i++) {
        rp->Image = *ptr++;
        rp++;
    }

/* MARK: item images */

    if (SeekIfNeeded(info.start_of_item_image_list, &offset, &ptr) == 0) {
        FreeDatabase();
        return UNKNOWN_GAME;
    }

    Item *ip = Items;

    for (int i = 0; i <= GameHeader.NumItems; i++) {
        ip->Image = 0xff;
        ip++;
    }

    /* Item image indices are stored as a list of item numbers, terminated
       by 0xFF. Image numbers start at 10 and increment sequentially. */
    int index, image = 10;

    do {
        index = *ptr++;
        if (index != 0xff)
            Items[index].Image = image++;
    } while (index != 0xff && ptr < endptr);

    /* The C64 release has 15 bytes of stray text (a disk command, "N0:B"
       + eight characters + "SU") lying over the third condition word of
       action lines 184 to 191, which costs line 185 its second parameter.
       Put back the values that the Spectrum release has there. */
    if (info.gameID == HULK_C64 && GameHeader.NumActions >= 191) {
        for (int i = 184; i <= 191; i++)
            Actions[i].Condition[2] = 0;
        Actions[185].Condition[2] = 21 * 20; /* parameter 21 */
    }

    return info.gameID;
}

GameIDType FreeGameResources(void) {
    FreeDatabase();
    return UNKNOWN_GAME;
}

/* Top-level parser for US-format SAGA binary game files.

   Parses the file sequentially: header, dictionary, rooms, messages,
   items, actions, and room connections. Returns the detected game ID
   on success, or UNKNOWN_GAME on failure (freeing any partial parse).

   When dict_start > 0, this is being called as a probe for the UK
   Hulk variant — additional image mapping tables are parsed at the end. */
GameIDType LoadBinaryDatabase(uint8_t *data, size_t length, GameInfo info, int dict_start) {
    FreeDatabase();
    uint8_t *ptr = data;

    uint8_t *endptr =  data + length - 2;

    ptr = LoadHeader(ptr, length, info, dict_start);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    ptr = ParseDictionary(ptr, endptr);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    ptr = ParseRooms(ptr, endptr, GameHeader.NumRooms);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    ptr = ParseMessages(ptr, endptr, GameHeader.NumMessages);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    /* Parse items */
    ptr = ParseItems(ptr, endptr, GameHeader.NumItems, 2);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    /* Skip item strings lookup table */
    ptr = Skip(ptr, (GameHeader.NumItems + 1) * 4, data + length);

    /* Parse actions */
    ptr = ParseActions(ptr, data, length, GameHeader.NumActions, 0);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    /* Skip room descriptions lookup table */
    ptr = Skip(ptr, (GameHeader.NumRooms + 1) * 2, data + length);

    endptr += 2;

    ptr = ParseRoomConnections(ptr, endptr, GameHeader.NumRooms, 2);
    if (ptr == NULL || ptr > endptr) {
        return FreeGameResources();
    }

    /* The dict_start parameter is only set if we are
       trying to detect the Adventure International UK
       release of Questprobe featuring The Incredible Hulk. */
    if (dict_start) {
        return HulkUKSpecificParsing(ptr, endptr, info);
    }

    /* Two C64 images of Voodoo Castle (the "Cracked by Toko" disk and the
       T64) have one damaged byte, 0x55 for 0x01, in the third condition
       word of action line 27. It turns parameter 21 into a test for room
       1096, so the line never fires. Put back the value that the other
       releases have there. */
    if (CurrentGame == VOODOO_CASTLE_US && GameHeader.NumActions >= 27
        && Actions[27].Condition[2] == 0x55a4)
        Actions[27].Condition[2] = 21 * 20; /* parameter 21 */

    return CurrentGame;
}
