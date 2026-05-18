//
//  detectgame.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "scott.h"

#include "common_file_utils.h"
#include "decompress_text.h"
#include "detect_game.h"
#include "line_drawing.h"
#include "sagadraw.h"
#include "saga.h"
#include "scott_game_info.h"

#include "game_specific.h"
#include "gremlins.h"
#include "robin_of_sherwood.h"
#include "seas_of_blood.h"

#include "apple2detect.h"
#include "atari8detect.h"
#include "c64decrunch.h"
#include "decompressz80.h"
#include "load_ti99_4a.h"

#include "parser.h"

extern const char *sysdict_zx[MAX_SYSMESS];
extern int header[];

/* Base address of the ZX Spectrum's writable RAM. ROM occupies
   0x0000-0x3FFF; RAM starts at 0x4000, which is where loaded programs
   live. Used to translate Z80 addresses stored in saved .z80 snapshots
   to offsets into our in-memory file buffer. */
#define ZX_RAM_BASE 0x4000

/* Byte signatures used to locate the dictionary in a game file.
   Each entry pairs a dictionary format with a short sequence of bytes
   that uniquely identifies it (typically the first few dictionary words). */
struct dictionaryKey {
    DictionaryType dict;
    const char *signature;
    int len;
};

struct dictionaryKey dictKeys[] = {
    { FOUR_LETTER_UNCOMPRESSED, "AUTO\0GO", 8 },
    { THREE_LETTER_UNCOMPRESSED, "AUT\0GO", 7 },
    { FIVE_LETTER_UNCOMPRESSED, "GO\0\0\0\0*CROSS*RUN", 17 }, // Claymorgue
    { FOUR_LETTER_COMPRESSED, "aUTOgO", 7 },
    { GERMAN_C64, "gEHENSTEIGE", 11 }, // Gremlins German C64
    { GERMAN, "\xc7"
              "EHENSTEIGE", 10 },
    { SPANISH, "\x81\0\0\0\xc9R\0\0ANDAENTR", 16 },
    { SPANISH_C64, "\x81\0\0\0iR\0\0ANDAENTR", 16 },
    { ITALIAN, "AUTO\0VAI\0\0*ENTR", 15 },
    { NOT_A_GAME, NULL, 0 }
};

/* Search `data` (of length `datasize`) for the byte sequence `pattern`
   (of length `patternLen`). Returns the offset of the first match, or
   -1 if not found. */
int FindCode(const uint8_t *data, size_t dataLen, const char *pattern, int patternLen)
{
    if (data == NULL || dataLen < (size_t)patternLen)
        return -1;
    const uint8_t *cursor = data;
    const uint8_t *lastPossibleStart = data + dataLen - patternLen;
    while (cursor < lastPossibleStart) {
        if (memcmp(cursor, pattern, patternLen) == 0) {
            return (int)(cursor - data);
        }
        cursor++;
    }
    return -1;
}

/* Identify the game's dictionary format by scanning `data` for known
   signatures. Returns the dictionary type and sets *offset to the start
   of the dictionary. Some formats require a backward offset adjustment
   because the signature appears partway through the dictionary. */
DictionaryType GetId(const uint8_t *data, size_t dataLen, size_t *offset)
{
    for (int i = 0; dictKeys[i].dict != NOT_A_GAME; i++) {
        *offset = FindCode(data, dataLen, dictKeys[i].signature, dictKeys[i].len);
        if (*offset != -1) {
            switch (dictKeys[i].dict) {
            case GERMAN_C64:
            case GERMAN:
                *offset -= 5;
                break;
            case FIVE_LETTER_UNCOMPRESSED:
                *offset -= 6;
                break;
            default:
                break;
            }
            return dictKeys[i].dict;
        }
    }

    return NOT_A_GAME;
}

/* Read 15 little-endian 16-bit values from ptr into the global header[].
   Returns a pointer to the last byte read (ptr + 29). */
uint8_t *ReadHeader(uint8_t *ptr)
{
    int i, value;
    for (i = 0; i < 15; i++) {
        value = READ_LE_UINT16(ptr);
        header[i] = value;
        ptr += 2;
    }
    return ptr - 1;
}

/* Validate that parsed header values are within reasonable ranges.
   Rejects databases with too few/many items, actions, words, or rooms. */
int SanityCheckHeader(void)
{
    int16_t v = GameHeader.NumItems;
    if (v < 10 || v > 500)
        return 0;
    v = GameHeader.NumActions;
    if (v < 100 || v > 500)
        return 0;
    v = GameHeader.NumWords; // word pairs
    if (v < 50 || v > 190)
        return 0;
    v = GameHeader.NumRooms; // Number of rooms
    if (v < 10 || v > 100)
        return 0;

    return 1;
}

/* Parse the dictionary from a binary game file into the global Verbs[]
   and Nouns[] arrays. Handles three encoding schemes:
   - Compressed/C64: first letter case indicates synonym ('*' prefix)
   - Localized (non-English): high bit on first byte = new word
   - Uncompressed: NUL-separated words, '*' = synonym marker
   A byte > 127 terminates the dictionary. */
static uint8_t *ReadDictionary(const GameInfo *info, uint8_t *ptr)
{
    int wl = info->word_length;
    if (wl < 1 || wl > 30)
        Fatal("Bad word length");
    char dictword[32];
    char c = 0;
    int charindex = 0;
    uint8_t *endptr = entire_file + file_length;

    int nv = info->number_of_verbs;
    int nn = info->number_of_nouns;

    for (int i = 0; i < info->number_of_words + 2; i++) {
        Verbs[i] = ".";
        Nouns[i] = ".";
    }

    for (int wordnum = 0; wordnum <= nv + nn; wordnum++) {
        int restarts = 0;
        for (int i = 0; i < wl; i++) {
            if (ptr >= endptr)
                return ptr;
            c = *(ptr++);

            if (info->dictionary == FOUR_LETTER_COMPRESSED || info->dictionary == GERMAN_C64 || info->dictionary == SPANISH_C64) {
                if (charindex == 0) {
                    if (c >= 'a') {
                        c = toupper(c);
                    } else if (c != '.' && c != 0) {
                        if (charindex < 31)
                            dictword[charindex++] = '*';
                    }
                }
                if (charindex < 31)
                    dictword[charindex++] = c;
            } else if (info->subtype == LOCALIZED) {
                if (charindex == 0) {
                    if (c & 0x80) {
                        c = c & 0x7f;
                    } else if (c != '.') {
                        if (charindex < 31)
                            dictword[charindex++] = '*';
                    }
                }
                if (charindex < 31)
                    dictword[charindex++] = c;
            } else {
                if (c == 0 && charindex == 0) {
                    if (ptr >= endptr)
                        return ptr;
                    c = *(ptr++);
                }
                if (c != ' ' && charindex > 0 && dictword[charindex - 1] == ' ') {
                    i--;
                    charindex--;
                }
                if (c == '*') {
                    charindex = 0;
                    i = -1;
                    if (++restarts > wl)
                        break;
                }
                if (charindex < 31)
                    dictword[charindex++] = c;
            }
        }
        dictword[charindex] = 0;

        char *word = MemAlloc(charindex + 1);
        memcpy(word, dictword, charindex + 1);
        if (wordnum < nv) {
            Verbs[wordnum] = word;
            debug_print("Verb %d: \"%s\"\n", wordnum, word);
        } else {
            Nouns[wordnum - nv] = word;
            debug_print("Noun %d: \"%s\"\n", wordnum - nv, word);
        }

        if (c > 127)
            return ptr;

        charindex = 0;
    }

    return ptr;
}

/* Convert a file offset to a pointer into the loaded file buffer.
   Returns NULL if the offset is out of range. */
uint8_t *SeekToPos(int offset)
{
    if (offset > file_length || entire_file == NULL)
        return NULL;
    return entire_file + offset;
}

/* Seek to a game data section if its position is specified (not FOLLOWS).
   Applies the file_baseline_offset adjustment. Returns 0 on seek failure. */
int SeekIfNeeded(int expected_start, size_t *offset, uint8_t **ptr)
{
    if (expected_start != FOLLOWS) {
        *offset = expected_start + file_baseline_offset;
        *ptr = SeekToPos(*offset);
        if (*ptr == 0)
            return 0;
    }
    return 1;
}

/* Extract game parameters from the raw header[] array based on the header
   layout type. Different platforms and game generations use different field
   orderings — this function abstracts that away. Some formats pack two
   values into one 16-bit word (e.g. max_carry + player_room in high/low bytes). */
int ParseHeader(int *h, HeaderType type, int *num_items, int *num_actions,
    int *num_words, int *num_rooms, int *max_carry, int *player_room,
    int *treasures, int *word_length, int *light_time, int *num_messages,
    int *treasure_room)
{
    switch (type) {
    case NO_HEADER:
        return 0;
    case EARLY:
        *num_items     = h[1];
        *num_actions   = h[2];
        *num_words     = h[3];
        *num_rooms     = h[4];
        *max_carry     = h[5];
        *player_room   = h[6];
        *treasures     = h[7];
        *word_length   = h[8];
        *light_time    = h[9];
        *num_messages  = h[10];
        *treasure_room = h[11];
        break;
    case LATE:
        *num_items     = h[1];
        *num_actions   = h[2];
        *num_words     = h[3];
        *num_rooms     = h[4];
        *max_carry     = h[5];
        *word_length   = h[6];
        *num_messages  = h[7];
        *player_room   = 1;
        *treasures     = 0;
        *light_time    = -1;
        *treasure_room = 0;
        break;
    case US_HEADER:
        *num_items     = h[3];
        *num_actions   = h[2];
        *num_words     = h[1];
        *num_rooms     = h[5];
        *max_carry     = h[6];
        *player_room   = h[7];
        *treasures     = h[8];
        *word_length   = h[0];
        *light_time    = h[9];
        *num_messages  = h[4];
        *treasure_room = h[10] >> 8;
        break;
    case ROBIN_C64_HEADER:
        *num_items     = h[1];
        *num_actions   = h[2];
        *num_words     = h[6];
        *num_rooms     = h[4];
        *max_carry     = h[5];
        *player_room   = 1;
        *treasures     = 0;
        *word_length   = h[7];
        *light_time    = -1;
        *num_messages  = h[3];
        *treasure_room = 0;
        break;
    case GREMLINS_C64_HEADER:
        *num_items     = h[1];
        *num_actions   = h[2];
        *num_words     = h[5];
        *num_rooms     = h[3];
        *max_carry     = h[6];
        *player_room   = h[8];
        *treasures     = 0;
        *word_length   = h[7];
        *light_time    = -1;
        *num_messages  = 98;
        *treasure_room = 0;
        break;
    case SUPERGRAN_C64_HEADER:
        *num_items     = h[3];
        *num_actions   = h[1];
        *num_words     = h[2];
        *num_rooms     = h[4];
        *max_carry     = h[8];
        *player_room   = 1;
        *treasures     = 0;
        *word_length   = h[6];
        *light_time    = -1;
        *num_messages  = h[5];
        *treasure_room = 0;
        break;
    case SEAS_OF_BLOOD_C64_HEADER:
        *num_items     = h[0];
        *num_actions   = h[1];
        *num_words     = 134;
        *num_rooms     = h[3];
        *max_carry     = h[4];
        *player_room   = 1;
        *treasures     = 0;
        *word_length   = h[6];
        *light_time    = -1;
        *num_messages  = h[2];
        *treasure_room = 0;
        break;
    case MYSTERIOUS_C64_HEADER:
        *num_items     = h[1];
        *num_actions   = h[2];
        *num_words     = h[3];
        *num_rooms     = h[4];
        *max_carry     = h[5] & 0xff;
        *player_room   = h[5] >> 8;
        *treasures     = h[6];
        *word_length   = h[7];
        *light_time    = h[8];
        *num_messages  = h[9];
        *treasure_room = 0;
        break;
    case ARROW_OF_DEATH_PT_2_C64_HEADER:
        *num_items     = h[3];
        *num_actions   = h[1];
        *num_words     = h[2];
        *num_rooms     = h[4];
        *max_carry     = h[5] & 0xff;
        *player_room   = h[5] >> 8;
        *treasures     = h[6];
        *word_length   = h[7];
        *light_time    = h[8];
        *num_messages  = h[9];
        *treasure_room = 0;
        break;
    case INDIANS_C64_HEADER:
        *num_items     = h[1];
        *num_actions   = h[2];
        *num_words     = h[3];
        *num_rooms     = h[4];
        *max_carry     = h[5] & 0xff;
        *player_room   = h[5] >> 8;
        *treasures     = h[6] & 0xff;
        *word_length   = h[6] >> 8;
        *light_time    = h[7] >> 8;
        *num_messages  = h[8] >> 8;
        *treasure_room = 0;
        break;
    default:
        debug_print("Unhandled header type!\n");
        return 0;
    }
    return 1;
}

/* Print parsed header values for debugging (only active when DEBUG_PRINT is set) */
void PrintHeaderInfo(int *h, int num_items, int num_actions, int num_words,
    int num_rooms, int max_carry, int player_room, int treasures,
    int word_length, int light_time, int num_messages, int treasure_room)
{
#if (DEBUG_PRINT)
    uint16_t value;
    for (int i = 0; i < 13; i++) {
        value = h[i];
        debug_print("b $%X %d: ", 0x494d + 0x3FE5 + i * 2, i);
        debug_print("\t%d (%04x)\n", value, value);
    }

    debug_print("Number of items =\t%d\n", num_items);
    debug_print("Number of actions =\t%d\n", num_actions);
    debug_print("Number of words =\t%d\n", num_words);
    debug_print("Number of rooms =\t%d\n", num_rooms);
    debug_print("Max carried items =\t%d\n", max_carry);
    debug_print("Word length =\t%d\n", word_length);
    debug_print("Number of messages =\t%d\n", num_messages);
    debug_print("Player start location: %d\n", player_room);
    debug_print("Treasure room: %d\n", treasure_room);
    debug_print("Lightsource time left: %d\n", light_time);
    debug_print("Number of treasures: %d\n", treasures);
#endif
}

typedef struct {
    uint8_t *imagedata;
    uint8_t background_colour;
    size_t size;
} LineImage;

/* Load Mysterious Adventures-format line-drawing vector image data. Each image starts
   with 0xFF followed by a background colour byte, then drawing commands
   until the next 0xFF. Truncated images are patched out with Image=255. */
void LoadVectorData(const GameInfo *info, uint8_t *ptr)
{
    size_t offset;

    if (info->start_of_image_data == FOLLOWS)
        ptr++;
    else if (SeekIfNeeded(info->start_of_image_data, &offset, &ptr) == 0)
        return;

    LineImages = MemAlloc(info->number_of_rooms * sizeof(line_image));
    uint8_t byte = *(ptr++);

    for (int ct = 0; ct < info->number_of_rooms; ct++) {
        Rooms[ct].Image = 0;
        if (byte == 0xff) {
            LineImages[ct].bgcolour = *(ptr++);
            LineImages[ct].data = ptr;
        } else {
            debug_print("Error! Image data does not start with 0xff!\n");
        }
        do {
            byte = *(ptr++);
            if (ptr - entire_file >= file_length) {
                debug_print("Error! Image data for image %d cut off!\n", ct);
                if (GameHeader.NumRooms - ct > 1)
                    Display(Bottom, "[This copy has %d broken or missing pictures. These have been patched out.]\n\n", GameHeader.NumRooms - ct);
                LineImages[ct].size = (LineImages[ct].data < ptr) ? ptr - LineImages[ct].data - 1 : 0;
                for (int i = ct + 2; i < GameHeader.NumRooms; i++)
                    Rooms[i].Image = 255;
                return;
            }
        } while (byte != 0xff);

        LineImages[ct].size = ptr - LineImages[ct].data;
    }
}

struct LineImage *lineImages = NULL;

/* Validate a parsed header against the expected GameInfo values and
   populate the global GameHeader. Returns 0 on any mismatch. */
static int ValidateAndApplyHeader(GameInfo info, int num_items, int num_actions,
                                  int num_words, int num_rooms, int max_carry,
                                  int player_room, int treasures, int word_length,
                                  int light_time, int num_messages,
                                  int treasure_room, size_t offset)
{
    PrintHeaderInfo(header, num_items, num_actions, num_words, num_rooms,
                    max_carry, player_room, treasures, word_length, light_time,
                    num_messages, treasure_room);

    if (num_items != info.number_of_items || num_actions != info.number_of_actions ||
        num_words != info.number_of_words || num_rooms != info.number_of_rooms ||
        max_carry != info.max_carried) {
        debug_print("Non-matching header\n");
        return 0;
    }

    SetGameHeader(num_items, num_actions, num_words, num_rooms, max_carry,
                  player_room, treasures, word_length, light_time, num_messages,
                  treasure_room);

    if (SanityCheckHeader() == 0)
        return 0;

    debug_print("Found a valid header at position 0x%zx\n", offset);
    if (offset != (size_t)(info.start_of_header + file_baseline_offset)) {
        debug_print("Expected: 0x%x\n",
                    info.start_of_header + file_baseline_offset);
    }

    AllocateGameData();
    return 1;
}

/* Set up the file baseline offset, seek to the header, read and parse it,
   then validate against the expected GameInfo values. On success the global
   GameHeader is populated and all game arrays are allocated. */
static int LoadGameHeader(const GameInfo *info, int dict_start,
                          uint8_t **ptr, size_t *offset)
{
    file_baseline_offset = dict_start - info->start_of_dictionary;
    debug_print("file_baseline_offset: %d (%x)\n",
                file_baseline_offset, file_baseline_offset);

    *offset = info->start_of_header + file_baseline_offset;
    *ptr = SeekToPos(*offset);
    if (*ptr == NULL)
        return 0;

    ReadHeader(*ptr);

    int num_items, num_actions, num_words, num_rooms, max_carry, player_room,
        treasures, word_length, light_time, num_messages, treasure_room;
    if (!ParseHeader(header, info->header_style, &num_items, &num_actions,
            &num_words, &num_rooms, &max_carry, &player_room, &treasures,
            &word_length, &light_time, &num_messages, &treasure_room))
        return 0;

    return ValidateAndApplyHeader(*info, num_items, num_actions, num_words,
                                  num_rooms, max_carry, player_room, treasures,
                                  word_length, light_time, num_messages,
                                  treasure_room, *offset);
}

/* Read room exit connections from binary data: one byte per exit,
   6 exits per room. */
static void ReadRoomExits(uint8_t **ptr, int num_rooms)
{
    for (int ct = 0; ct <= num_rooms; ct++) {
        memcpy(Rooms[ct].Exits, *ptr, 6);
        *ptr += 6;
    }
}

/* Read item starting locations from binary data: one byte per item. */
static void ReadItemLocations(uint8_t **ptr, int num_items)
{
    for (int ct = 0; ct <= num_items; ct++) {
        Items[ct].Location = *((*ptr)++);
        Items[ct].InitialLoc = Items[ct].Location;
    }
}

/* Read NUL-terminated room descriptions from binary data, rejecting
   any string containing a byte > 127. Assigns room images based on
   whether the images are in vector format and whether the game has
   pictures. (Irmak tile style room images are already assigned in TryLoading().)
   Returns 0 on invalid data. */
static int ReadBinaryRoomDescs(uint8_t **ptr, int num_rooms, int has_pictures, int vector)
{
    for (int ct = 0; ct <= num_rooms; ct++) {
        uint8_t *p = *ptr;
        while (*p > 0 && *p <= 127)
            p++;
        if (*p > 127)
            return 0;
        size_t len = p - *ptr;
        Rooms[ct].Text = MemAlloc(len + 1);
        memcpy(Rooms[ct].Text, *ptr, len + 1);
        debug_print("Room %d: %s\n", ct, Rooms[ct].Text);
        if (vector || !has_pictures)
            Rooms[ct].Image = has_pictures ? ct - 1 : 255;
        *ptr = p + 1;
    }
    return 1;
}

/* Read NUL-or-CR-terminated strings from binary data into dest[].
   Stops after max_count strings, on out-of-bounds reads, or when a
   byte > 127 is encountered (if allowed_high is non-NULL, only bytes
   not in the string cause a stop; if NULL, all bytes are accepted).
   Returns the number of strings read. */
static int ReadTerminatedStrings(uint8_t **ptr, const char **dest, int max_count,
                                 const char *allowed_high)
{
    int count = 0;

    while (count < max_count) {
        uint8_t *string_start = *ptr;
        if (string_start == NULL)
            return count;
        uint8_t *scan = string_start;

        while (scan >= entire_file && (size_t)(scan - entire_file) < file_length
               && *scan != 0 && *scan != '\r') {
            if (*scan > 127 && allowed_high != NULL && strchr(allowed_high, *scan) == NULL) {
                *ptr = scan;
                return count;
            }
            scan++;
        }

        if (scan < entire_file || (size_t)(scan - entire_file) >= file_length) {
            debug_print("Read out of bounds!\n");
            *ptr = scan;
            return count;
        }

        size_t length = scan - string_start;
        if (length > 0) {
            int is_cr = (*scan == '\r');
            size_t alloc_size = length + (is_cr ? 2 : 1);
            char *string = MemAlloc(alloc_size);
            memcpy(string, string_start, length);
            if (is_cr) {
                string[length] = '\r';
                string[length + 1] = '\0';
            } else {
                string[length] = '\0';
            }
            dest[count] = string;
            debug_print("String %d: \"%s\"\n", count, dest[count]);
            count++;
        }

        *ptr = scan + 1;
    }

    return count;
}

void SetGameHeader(int num_items, int num_actions, int num_words,
                          int num_rooms, int max_carry, int player_room,
                          int treasures, int word_length, int light_time,
                          int num_messages, int treasure_room)
{
    GameHeader.NumItems = num_items;
    GameHeader.NumActions = num_actions;
    GameHeader.NumWords = num_words;
    GameHeader.WordLength = word_length;
    GameHeader.NumRooms = num_rooms;
    GameHeader.MaxCarry = max_carry;
    GameHeader.PlayerRoom = player_room;
    GameHeader.Treasures = treasures;
    GameHeader.LightTime = light_time;
    LightRefill = light_time;
    GameHeader.NumMessages = num_messages;
    GameHeader.TreasureRoom = treasure_room;
}

void AllocateGameData(void)
{
    Items = MemAlloc(sizeof(Item) * (GameHeader.NumItems + 1));
    Actions = MemAlloc(sizeof(Action) * (GameHeader.NumActions + 1));
    Verbs = MemAlloc(sizeof(char *) * (GameHeader.NumWords + 2));
    Nouns = MemAlloc(sizeof(char *) * (GameHeader.NumWords + 2));
    Rooms = MemAlloc(sizeof(Room) * (GameHeader.NumRooms + 1));
    Messages = MemAlloc(sizeof(char *) * (GameHeader.NumMessages + 1));
}

static char *ReadBinaryString(uint8_t **ptr)
{
    size_t len = strlen((char *)*ptr);
    char *result = MemAlloc(len + 1);
    memcpy(result, *ptr, len + 1);
    *ptr += len + 1;
    return result;
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
    Items[index].AutoGet = strchr(Items[index].Text, '/');
    if (Items[index].AutoGet == NULL)
        return;
    if (strcmp(Items[index].AutoGet, "//") == 0 || strcmp(Items[index].AutoGet, "/*") == 0) {
        Items[index].AutoGet = NULL;
        return;
    }
    /* NUL-terminate the description and advance past the '/' */
    *Items[index].AutoGet++ = 0;
    /* Strip the trailing '/' from the word */
    char *trailingslash = strchr(Items[index].AutoGet, '/');
    if (trailingslash != NULL)
        *trailingslash = 0;
}

/* Read action tables from binary data. In compressed format, each action
   has a packed byte encoding the number of conditions (low 5 bits) and
   commands (high 3 bits). In uncompressed format, all 5 condition and
   2 command slots are always present. */
static void ReadActions(uint8_t **ptr, int num_actions, int compressed)
{
    for (int ct = 0; ct <= num_actions; ct++) {
        Actions[ct].Vocab = READ_LE_UINT16_AND_ADVANCE(ptr);

        uint16_t cond, comm;
        if (compressed) {
            uint16_t value = *((*ptr)++);
            cond = value & 0x1f;
            if (cond > 5) {
                debug_print("Condition error at action %d!\n", ct);
                cond = 5;
            }
            comm = (value & 0xe0) >> 5;
            if (comm > 2) {
                debug_print("Command error at action %d!\n", ct);
                comm = 2;
            }
        } else {
            cond = 5;
            comm = 2;
        }
        for (int j = 0; j < 5; j++)
            Actions[ct].Condition[j] = (j < cond) ? READ_LE_UINT16_AND_ADVANCE(ptr) : 0;
        for (int j = 0; j < 2; j++)
            Actions[ct].Opcode[j] = (j < comm) ? READ_LE_UINT16_AND_ADVANCE(ptr) : 0;
    }
}

/* Read item descriptions terminated by NUL or high byte (>126).
   Each item's text is parsed for the /AutoGet/ word. */
static void ReadBinaryItemDescs(uint8_t **ptr, int num_items)
{
    for (int ct = 0; ct <= num_items; ct++) {
        uint8_t *p = *ptr;
        while (*p > 0 && *p <= 126)
            p++;
        size_t len = p - *ptr;
        Items[ct].Text = MemAlloc(len + 1);
        memcpy(Items[ct].Text, *ptr, len);
        Items[ct].Text[len] = 0;
        *ptr = p + 1;
        ParseItemSlashAutoGet(ct);
    }
}

/* Load a game using the "old style" Brian Howarth binary format
   (Mysterious Adventures era). Reads the header, actions, room connections,
   item locations, dictionary, room descriptions, messages, item descriptions,
   vector images, and system messages sequentially from known offsets.
   Returns the game ID on success. */
static GameIDType TryLoadingOld(const GameInfo *info, int dict_start)
{
    uint8_t *ptr;
    size_t offset;

    if (!LoadGameHeader(info, dict_start, &ptr, &offset))
        return UNKNOWN_GAME;

#pragma mark actions

    if (SeekIfNeeded(info->start_of_actions, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ReadActions(&ptr, GameHeader.NumActions, 0);

#pragma mark room connections

    if (SeekIfNeeded(info->start_of_room_connections, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ReadRoomExits(&ptr, GameHeader.NumRooms);

#pragma mark item locations

    if (SeekIfNeeded(info->start_of_item_locations, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ReadItemLocations(&ptr, GameHeader.NumItems);

#pragma mark dictionary

    if (SeekIfNeeded(info->start_of_dictionary, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ptr = ReadDictionary(info, ptr);

#pragma mark rooms

    if (info->start_of_room_descriptions != 0) {
        if (SeekIfNeeded(info->start_of_room_descriptions, &offset, &ptr) == 0)
            return UNKNOWN_GAME;

        if (info->start_of_room_descriptions == FOLLOWS)
            ptr++;

        if (!ReadBinaryRoomDescs(&ptr, GameHeader.NumRooms, info->number_of_pictures > 0, info->picture_format_version == 99))
            return UNKNOWN_GAME;
    }

#pragma mark messages

    if (SeekIfNeeded(info->start_of_messages, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    for (int ct = 0; ct <= GameHeader.NumMessages; ct++) {
        Messages[ct] = ReadBinaryString(&ptr);
        debug_print("Message %d: \"%s\"\n", ct, Messages[ct]);
    }

#pragma mark items

    if (SeekIfNeeded(info->start_of_item_descriptions, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ReadBinaryItemDescs(&ptr, GameHeader.NumItems);

#pragma mark line images

    if (info->number_of_pictures > 0 && info->picture_format_version == 99) {
        LoadVectorData(info, ptr);
    }

#pragma mark System messages

    if (SeekIfNeeded(info->start_of_system_messages, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ReadTerminatedStrings(&ptr, system_messages, 40, "\x83\xc9");

    if (SeekIfNeeded(info->start_of_directions, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ReadTerminatedStrings(&ptr, sys, 6, "");

    return info->gameID;
}

/* Load a Adventure International UK / Brian Howarth game database given its
   GameInfo descriptor and dictionary offset.
   Dispatches to the SAGA binary loader for UK Hulk, to TryLoadingOld for
   old-style formats, or handles the later binary format directly.

   Validates the header against the expected GameInfo values. Supports both
   compressed (packed condition/command counts) and uncompressed action
   formats, and compressed vs plain NUL-terminated text strings.

   On success, all global game arrays are populated and the game ID is returned. */
GameIDType TryLoading(uint8_t *data, size_t datasize, const GameInfo *info, int dict_start)
{
    /* TryLoading's downstream helpers (LoadGameHeader, SeekIfNeeded,
       SeekToPos, ReadDictionary, etc.) currently read the game buffer
       via the global entire_file / file_length pair. Adopt the caller's
       buffer into those globals here. If the caller passed a buffer
       different from the current entire_file (e.g. a PRG extracted from
       a D64 disk image), the old buffer is freed — we're committed to
       this game, so the rest of the disk image is no longer needed. */
    if (data != entire_file) {
        free(entire_file);
        entire_file = data;
    }
    file_length = datasize;

    /* The UK versions of Hulk uses the Mak Jukic binary database format */
    if (info->gameID == HULK || info->gameID == HULK_C64)
        return LoadBinaryDatabase(entire_file, file_length, *info, dict_start);

    if (info->type == OLD_STYLE)
        return TryLoadingOld(info, dict_start);

    uint8_t *ptr;
    size_t offset;

    if (!LoadGameHeader(info, dict_start, &ptr, &offset))
        return UNKNOWN_GAME;

    int compressed = (info->dictionary == FOUR_LETTER_COMPRESSED);

#pragma mark room images

    if (info->start_of_room_image_list != 0) {
        if (SeekIfNeeded(info->start_of_room_image_list, &offset, &ptr) == 0)
            return UNKNOWN_GAME;

        for (int ct = 0; ct <= GameHeader.NumRooms; ct++) {
            Rooms[ct].Image = *ptr++;
            debug_print("Room %d image: %d\n", ct, Rooms[ct].Image);
        }
    }
#pragma mark Item flags

    if (info->start_of_item_flags != 0) {
        if (SeekIfNeeded(info->start_of_item_flags, &offset, &ptr) == 0)
            return UNKNOWN_GAME;

        for (int ct = 0; ct <= GameHeader.NumItems; ct++)
            Items[ct].Flag = *(ptr++);
    }

#pragma mark item images

    if (info->start_of_item_image_list != 0) {
        if (SeekIfNeeded(info->start_of_item_image_list, &offset, &ptr) == 0)
            return UNKNOWN_GAME;

        for (int ct = 0; ct <= GameHeader.NumItems; ct++)
            Items[ct].Image = *(ptr++);
        debug_print("Offset after reading item images: %lx\n",
                ptr - entire_file - file_baseline_offset);
    }

#pragma mark actions

    if (SeekIfNeeded(info->start_of_actions, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ReadActions(&ptr, GameHeader.NumActions, info->actions_style == COMPRESSED);
    debug_print("Offset after reading actions: %lx\n",
            ptr - entire_file - file_baseline_offset);

#pragma mark dictionary

    if (SeekIfNeeded(info->start_of_dictionary, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ptr = ReadDictionary(info, ptr);

#pragma mark rooms

    if (info->start_of_room_descriptions != 0) {
        if (SeekIfNeeded(info->start_of_room_descriptions, &offset, &ptr) == 0)
            return UNKNOWN_GAME;

        if (!compressed) {
            if (!ReadBinaryRoomDescs(&ptr, GameHeader.NumRooms, info->number_of_pictures > 0, 0))
                return UNKNOWN_GAME;
        } else {
            for (int ct = 0; ct < GameHeader.NumRooms; ct++) {
                Rooms[ct].Text = DecompressText(ptr, ct);
                if (Rooms[ct].Text == NULL)
                    return UNKNOWN_GAME;
                Rooms[ct].Text[0] = tolower(Rooms[ct].Text[0]);
            }
        }
    }

#pragma mark room connections

    if (SeekIfNeeded(info->start_of_room_connections, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ReadRoomExits(&ptr, GameHeader.NumRooms);

#pragma mark messages

    if (SeekIfNeeded(info->start_of_messages, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    if (compressed) {
        for (int ct = 0; ct <= GameHeader.NumMessages; ct++) {
            Messages[ct] = DecompressText(ptr, ct);
            debug_print("Message %d: \"%s\"\n", ct, Messages[ct]);
            if (Messages[ct] == NULL)
                return UNKNOWN_GAME;
        }
    } else {
        for (int ct = 0; ct <= GameHeader.NumMessages; ct++) {
            Messages[ct] = ReadBinaryString(&ptr);
            debug_print("Message %d: \"%s\"\n", ct, Messages[ct]);
        }
    }

#pragma mark items

    if (SeekIfNeeded(info->start_of_item_descriptions, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    if (compressed) {
        for (int ct = 0; ct <= GameHeader.NumItems; ct++) {
            Items[ct].Text = DecompressText(ptr, ct);
            Items[ct].AutoGet = NULL;
            if (Items[ct].Text != NULL && Items[ct].Text[0] != '.') {
                debug_print("Item %d: %s\n", ct, Items[ct].Text);
                Items[ct].AutoGet = strchr(Items[ct].Text, '.');
                if (Items[ct].AutoGet) {
                    *Items[ct].AutoGet++ = 0;
                    Items[ct].AutoGet++;
                    char *t = strchr(Items[ct].AutoGet, '.');
                    if (t != NULL)
                        *t = 0;
                    for (int i = 1; i < GameHeader.WordLength; i++)
                        Items[ct].AutoGet[i] = toupper(Items[ct].AutoGet[i]);
                }
            }
        }
    } else {
        for (int ct = 0; ct <= GameHeader.NumItems; ct++) {
            Items[ct].Text = ReadBinaryString(&ptr);
            debug_print("Item %d: %s\n", ct, Items[ct].Text);
            ParseItemSlashAutoGet(ct);
        }
    }

#pragma mark item locations

    if (SeekIfNeeded(info->start_of_item_locations, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    ReadItemLocations(&ptr, GameHeader.NumItems);

#pragma mark vector images

    if (info->number_of_pictures > 0 && info->picture_format_version == 99) {
        LoadVectorData(info, ptr);
    }

#pragma mark System messages

    if (SeekIfNeeded(info->start_of_system_messages, &offset, &ptr) == 0)
        return info->gameID;

    int is_c64_english = (info->subtype & (C64 | ENGLISH)) == (C64 | ENGLISH);

    if (is_c64_english) {
        while (offset > 0) {
            ptr = SeekToPos(offset);
            while (ptr - entire_file < file_length && (*ptr == 0 || *ptr == '\r'))
                ptr++;
            if (ptr - entire_file + 5 <= file_length && memcmp(ptr, "NORTH", 5) == 0)
                break;
            offset--;
        }
    }

    ptr = SeekToPos(offset);
    if (ptr == NULL)
        return UNKNOWN_GAME;
    ReadTerminatedStrings(&ptr, system_messages, 45, NULL);

    debug_print("Offset after reading system messages: %lx\n",
            ptr - entire_file);

    if (is_c64_english)
        return info->gameID;

    if (SeekIfNeeded(info->start_of_directions, &offset, &ptr) == 0)
        return UNKNOWN_GAME;

    while(!isalpha(*ptr)) {
        ptr++;
        if (ptr - entire_file > file_length) {
            debug_print("Read out of bounds!\n");
            return info->gameID;
        }
    }

    ReadTerminatedStrings(&ptr, sys, 6, "");

    return info->gameID;
}

/* Check if the loaded game is a Mysterious Adventures title.
   Checks the current Game pointer first, then falls back to matching header
   values against the game database (for .dat plaintext format). */
int IsMysterious(void)
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

/* Decompress a Parsec-packed ZX Spectrum game file. Four "scene releases"
   by Parsec (Denmark) use this RLE scheme: Pirate Adventure, Voodoo Castle,
   Strange Odyssey, and Buckaroo Banzai. Operates backwards through memory,
   expanding either literal runs or repeated byte-pairs. */
uint8_t *DecompressParsec(uint8_t *start, uint8_t *end, uint8_t *dataptr)
{
    if (dataptr - 3 < entire_file) {
        return NULL;
    }
    uint8_t *pos = start;

    while (pos != end && dataptr - 3 > entire_file) {
        if (pos <= entire_file) {
            return NULL;
        }
        uint8_t *nextptr = dataptr - 2;
        if ((*dataptr & 0x80) == 0) {
            int repeats = READ_LE_UINT16(dataptr - 1);
            for (int i = 0; i < repeats && nextptr > entire_file && pos > entire_file; i++) {
                *pos = *nextptr;
                pos--;
                nextptr--;
            }
        } else {
            uint8_t val1 = *(dataptr - 1);
            uint8_t val2 = *(dataptr - 2);
            nextptr = dataptr - 3;
            int repeats = *dataptr & 0x7f;
            for (int i = 0; i < repeats && pos > entire_file; i++) {
                *pos = val1;
                pos--;
                *pos = val2;
                pos--;
            }
        }
        dataptr = nextptr;
    }
    return dataptr;
}

/* Detect and load a ZX Spectrum game file. Handles .z80 snapshot
   decompression, dictionary signature scanning, and Parsec RLE
   decompression (a two-pass scheme using Z80 register values
   stored at fixed offsets in the snapshot). */
GameIDType DetectZXSpectrum(void)
{
    GameIDType detectedGame = UNKNOWN_GAME;
    int was_z80_snapshot = 0;
    uint8_t *uncompressed = DecompressZ80(entire_file, &file_length);
    if (uncompressed != NULL) {
        was_z80_snapshot = 1;
        free(entire_file);
        entire_file = uncompressed;
    }

    size_t dict_offset;

    DictionaryType dict_type = GetId(entire_file, file_length, &dict_offset);

    if (dict_type == NOT_A_GAME)
        return UNKNOWN_GAME;

    for (int i = 0; games[i].Title != NULL; i++) {
        if (games[i].dictionary == dict_type) {
            detectedGame = TryLoading(entire_file, file_length, &games[i], dict_offset);
            if (detectedGame != UNKNOWN_GAME) {
                free(Game);
                Game = &games[i];
                break;
            }
        }
    }

    /* If no dictionary was found and this was a .z80 snapshot, try
       Parsec RLE decompression. The decompression parameters (start/end
       addresses) are stored in saved Z80 registers at fixed offsets.
       Addresses are adjusted by -ZX_RAM_BASE (0x4000) to convert from
       the Spectrum's memory map to the file buffer. */
    if (!detectedGame && was_z80_snapshot) {
        uint8_t *file_data = entire_file;
        /* HL: source pointer for the first Parsec pass (the compressed
           data inside the snapshot). */
        uint16_t HL = READ_LE_UINT16(file_data + 0x1b42) - ZX_RAM_BASE;
        uint8_t *first_pass_result = 0;
        if (HL < file_length)
            first_pass_result = DecompressParsec(&file_data[0x0fff], &file_data[0x07ff], &file_data[HL]);
        if (first_pass_result) {
            /* BC/DE: output start/end for the second Parsec pass, which
               unpacks the first-pass result into its final position. */
            uint16_t BC = READ_LE_UINT16(file_data + 0x1b48) - ZX_RAM_BASE;
            uint16_t DE = READ_LE_UINT16(file_data + 0x1b4b) - ZX_RAM_BASE;
            DecompressParsec(&file_data[BC], &file_data[DE], first_pass_result);
            dict_type = GetId(entire_file, file_length, &dict_offset);
            if (dict_type == NOT_A_GAME)
                Fatal("Unsupported game!");
            for (int i = 0; games[i].Title != NULL; i++) {
                if (games[i].dictionary == dict_type) {
                    if (TryLoading(entire_file, file_length, &games[i], dict_offset)) {
                        free(Game);
                        Game = &games[i];
                        detectedGame = Game->gameID;
                        break;
                    }
                }
            }
        }
    }

    if (!detectedGame)
        Fatal("Unsupported game!");

    return detectedGame;
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

    for (int i = 0; i < NUMBER_OF_DIRECTIONS; i++)
        Directions[i] = EnglishDirections[i];
    for (int i = 0; i < NUMBER_OF_SKIPPABLE_WORDS; i++)
        SkipList[i] = EnglishSkipList[i];
    for (int i = 0; i < NUMBER_OF_DELIMITERS; i++)
        DelimiterList[i] = EnglishDelimiterList[i];
    for (int i = 0; i < NUMBER_OF_EXTRA_NOUNS; i++)
        ExtraNouns[i] = EnglishExtraNouns[i];

    Game = (GameInfo *)MemAlloc(sizeof(GameInfo));
    memset(Game, 0, sizeof(GameInfo));

    /* Try the ScottFree plaintext format first (closes f on success) */
    GameIDType detectedGame = LoadDatabase(f, Options & DEBUGGING);

    if (detectedGame == UNKNOWN_GAME) { /* Not a ScottFree game, check if TI99/4A */
        entire_file = MemAlloc(file_length);
        fseek(f, 0, SEEK_SET);
        size_t result = fread(entire_file, 1, file_length, f);
        fclose(f);
        if (result == 0)
            Fatal("File empty or read error!");

        detectedGame = DetectTI994A();

        if (detectedGame == UNKNOWN_GAME) { /* Not a TI99/4A game, check if C64 */
            detectedGame = DetectC64(&entire_file, &file_length, file_name);
        }

        if (detectedGame == UNKNOWN_GAME) { /* Not a C64 game, check if Atari */
            detectedGame = DetectAtari8(&entire_file, &file_length);
        }

        if (detectedGame == UNKNOWN_GAME) { /* Not an Atari game, check if Apple 2 */
            detectedGame = DetectApple2(&entire_file, &file_length);
        }

        if (detectedGame == UNKNOWN_GAME) { /* Not an Apple 2 game, check if ZX Spectrum */
            detectedGame = DetectZXSpectrum();
        }

        if (detectedGame == UNKNOWN_GAME) {
            free(entire_file);
            free(Game);
            Game = NULL;
            return UNKNOWN_GAME;
        }
    }

    if (detectedGame == HULK_US) {
        CurrentGame = HULK_US;
        Game->type = US_VARIANT;
    }

    if (detectedGame == RETURN_TO_PIRATES_ISLE) {
        CurrentGame = RETURN_TO_PIRATES_ISLE;
        Game->type = US_VARIANT;
    }

    if (detectedGame == SCOTTFREE || detectedGame == TI994A)
        CurrentGame = detectedGame;

    if (IsMysterious()) {
        Options = Options | SCOTTLIGHT | PREHISTORIC_LAMP;
        ImageHeight = 95;
    }

    if (detectedGame == SCOTTFREE || detectedGame == TI994A || Game->type == US_VARIANT)
        return detectedGame;

    /* Copy ZX Spectrum style system messages as a base, then apply
       game-specific overrides below */
    for (int i = 6; i < MAX_SYSMESS && sysdict_zx[i] != NULL; i++) {
        sys[i] = (char *)sysdict_zx[i];
    }

    /* Game-specific post-load setup: load extra data (Robin of Sherwood
       forest images, Seas of Blood combat tables), fix image assignments, and map
       system_messages[] indices to the sys[] slots the engine uses. */
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
    case CLAYMORGUE:
        for (int i = OK; i <= RESUME_A_SAVED_GAME; i++)
            sys[i] = system_messages[6 - OK + i];
        for (int i = PLAY_AGAIN; i <= ON_A_SCALE_THAT_RATES; i++)
            sys[i] = system_messages[2 - PLAY_AGAIN + i];
        break;
    case SECRET_MISSION:
    case SECRET_MISSION_C64:
        Items[3].Image = 23;
        Items[3].Flag = 128 | 2;
        Rooms[2].Image = 0;
        if (detectedGame == SECRET_MISSION_C64) {
            SecretMission64Sysmess();
            break;
        }
    case ADVENTURELAND:
        for (int i = PLAY_AGAIN; i <= ON_A_SCALE_THAT_RATES; i++)
            sys[i] = system_messages[2 - PLAY_AGAIN + i];
        for (int i = OK; i <= YOU_HAVENT_GOT_IT; i++)
            sys[i] = system_messages[6 - OK + i];
        for (int i = YOU_DONT_SEE_IT; i <= RESUME_A_SAVED_GAME; i++)
            sys[i] = system_messages[13 - YOU_DONT_SEE_IT + i];
        break;
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
    case SUPERGRAN_C64:
        Supergran64Sysmess();
        break;
    case SAVAGE_ISLAND_C64:
        Items[20].Image = 13;
        // fallthrough
    case SAVAGE_ISLAND2_C64:
        Supergran64Sysmess();
        sys[IM_DEAD] = "I'm DEAD!! ";
        if (detectedGame == SAVAGE_ISLAND2_C64)
            Rooms[30].Image = 20;
        break;
    case SAVAGE_ISLAND:
        Items[20].Image = 13;
        // fallthrough
    case SAVAGE_ISLAND2:
        MyLoc = 30; /* Both parts of Savage Island begin in room 30 */
        // fallthrough
    case GREMLINS_GERMAN:
    case GREMLINS:
    case SUPERGRAN:
        for (int i = DROPPED; i <= OK; i++)
            sys[i] = system_messages[2 - DROPPED + i];
        for (int i = I_DONT_UNDERSTAND; i <= THATS_BEYOND_MY_POWER; i++)
            sys[i] = system_messages[6 - I_DONT_UNDERSTAND + i];
        for (int i = YOU_ARE; i <= HIT_ENTER; i++)
            sys[i] = system_messages[17 - YOU_ARE + i];
        sys[PLAY_AGAIN] = system_messages[5];
        sys[YOURE_CARRYING_TOO_MUCH] = system_messages[24];
        sys[IM_DEAD] = system_messages[25];
        sys[YOU_CANT_GO_THAT_WAY] = system_messages[14];
        break;
    case GREMLINS_SPANISH:
        LoadExtraSpanishGremlinsData();
        break;
    case GREMLINS_SPANISH_C64:
        LoadExtraSpanishGremlinsC64Data();
        break;
    case HULK_C64:
    case HULK:
        for (int i = 0; i < 6; i++) {
            sys[i] = (char *)sysdict_zx[i];
        }
        break;
    default:
        if (!(Game->subtype & C64)) {
            if (Game->subtype & MYSTERIOUS) {
                for (int i = PLAY_AGAIN; i <= YOU_HAVENT_GOT_IT; i++)
                    sys[i] = system_messages[2 - PLAY_AGAIN + i];
                for (int i = YOU_DONT_SEE_IT; i <= WHAT_NOW; i++)
                    sys[i] = system_messages[15 - YOU_DONT_SEE_IT + i];
                for (int i = LIGHT_HAS_RUN_OUT; i <= RESUME_A_SAVED_GAME; i++)
                    sys[i] = system_messages[31 - LIGHT_HAS_RUN_OUT + i];
                sys[ITEM_DELIMITER] = " - ";
                sys[MESSAGE_DELIMITER] = "\n";
                sys[YOU_SEE] = "\nThings I can see:\n";
                break;
            } else {
                for (int i = PLAY_AGAIN; i <= RESUME_A_SAVED_GAME; i++)
                    sys[i] = system_messages[2 - PLAY_AGAIN + i];
            }
        }
        break;
    }

    switch (detectedGame) {
    case GREMLINS_GERMAN:
        LoadExtraGermanGremlinsData();
        break;
    case GREMLINS_GERMAN_C64:
        LoadExtraGermanGremlinsC64Data();
        break;
    case PERSEUS_ITALIAN:
        PerseusItalianSysmess();
        break;
    default:
        break;
    }

    if ((Game->subtype & (MYSTERIOUS | C64)) == (MYSTERIOUS | C64))
        Mysterious64Sysmess();

    /* If it is a C64 or a Mysterious Adventures game, we have set up the graphics already */
    if (!(Game->subtype & (C64 | MYSTERIOUS)) && Game->number_of_pictures > 0) {
        SagaGraphicsSetup(0);
    }

    return detectedGame;
}
