//
//  saga.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-29.
//

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "apple2draw.h"
#include "apple2_vector_draw.h"
#include "atari_8bit_vector_draw.h"
#include "c64a8draw.h"
#include "c64a8scott.h"
#include "c64_small.h"
#include "detectgame.h"
#include "pcdraw.h"
#include "rtpi_graphics.h"
#include "sagagraphics.h"
#include "scott.h"

#include "saga.h"

// clang-format off

static const char *DOSFilenames[] =
  { "B01024R", "B01044I", "R0109",   "R0190",
    "B01001I", "B01025R", "B01045R", "R0112",  "R0191",
    "B01002I", "B01026R", "B01046I", "R0115",  "R0192",
    "B01003I", "B01029I", "B01048I", "R0116",  "R0193",
    "B01004I", "B01030I", "B01050I", "R0119",  "R0194",
    "B01012I", "B01031I", "B01051I", "R0120",  "R0195",
    "B01013R", "B01032I", "B01053R", "R0181",  "R0196",
    "B01016I", "B01033R", "B01070R", "R0182",  "R0197",
    "B01017R", "B01034R", "B01072R", "R0183",  "R0198",
    "B01018I", "B01035I", "R0184",   "R0199",  "B01019I",
    "B01036R", "R0100",   "R0185",   "B01020R","B01037I",
    "R0101",   "R0186",   "B01021I", "B01039R","R0102",
    "R0187",   "B01022R", "B01040R", "R0103",  "R0188",
    "B01023I", "B01042I", "R0104",   "R0189",  NULL };

// clang-format on

int LoadDOSImages(void)
{
    USImages = NewImage();

    USImage *image = USImages;
    size_t datasize;
    int found = 0;

    for (int i = 0; DOSFilenames[i] != NULL; i++) {
        const char *shortname = DOSFilenames[i];
        uint8_t *data = FindImageFile(shortname, &datasize);
        if (data) {
            found++;
            image->datasize = datasize;
            image->imagedata = MemAlloc(datasize);
            memcpy(image->imagedata, data, datasize);
            free(data);
            image->systype = SYS_MSDOS;
            if (shortname[0] == 'R') {
                image->usage = IMG_ROOM;
                image->index = shortname[4] - '0' + (shortname[3] - '0') * 10;
            } else {
                image->index = shortname[5] - '0' + (shortname[4] - '0') * 10;
                if (shortname[6] == 'R') {
                    image->usage = IMG_ROOM_OBJ;
                } else {
                    image->usage = IMG_INV_OBJ;
                }
            }
            image->next = NewImage();
            image = image->next;
        }
    }
    if (!found) {
        free(USImages);
        USImages = NULL;
        return 0;
    }
    return 1;
}

uint8_t *ReadUSDictionary(uint8_t *ptr)
{
    char dictword[GameHeader.WordLength + 2];
    char c = 0;
    int wordnum = 0;
    int charindex = 0;

    int nn = GameHeader.NumWords + 1;

    do {
        for (int i = 0; i < GameHeader.WordLength; i++) {
            c = *(ptr++);
            if (c == 0) {
                if (charindex == 0) {
                    c = *(ptr++);
                }
            }
            dictword[charindex] = c;
            if (c == '*')
                i--;
            charindex++;

            dictword[charindex] = 0;
        }

        if (wordnum < nn) {
            Nouns[wordnum] = MemAlloc(charindex + 1);
            memcpy((char *)Nouns[wordnum], dictword, charindex + 1);
            debug_print("Nouns %d: \"%s\"\n", wordnum,
                Nouns[wordnum]);
        } else {
            Verbs[wordnum - nn] = MemAlloc(charindex + 1);
            memcpy((char *)Verbs[wordnum - nn], dictword, charindex + 1);
            debug_print("Verbs %d: \"%s\"\n", wordnum - nn,
                Verbs[wordnum - nn]);
        }
        wordnum++;

        if (c > 127)
            return ptr;

        charindex = 0;
    } while (wordnum <= GameHeader.NumWords * 2 + 1);

    return ptr;
}

int DrawApple2Image(USImage *image)
{
    if (image->usage == IMG_ROOM)
        ClearApple2ScreenMem();
    DrawApple2ImageFromData(image->imagedata, image->datasize, CurrentGame == COUNT_US, NULL);
    debug_print("Drawing image with index %d, usage %d\n", image->index, image->usage);
    return 1;
}

int DrawDOSImage(USImage *image) {
    if (image->imagedata == NULL) {
        fprintf(stderr, "DrawDOSImage: ptr == NULL\n");
        return 0;
    }

    debug_print("DrawDOSImage: usage:%d index:%d\n", image->usage, image->index);

    return DrawDOSImageFromData(image->imagedata);
}

int DrawUSImage(USImage *image)
{
    last_image_index = image->index;
    switch (image->systype) {
        case SYS_MSDOS:
            return DrawDOSImage(image);
        case SYS_C64_TINY:
            return DrawMiniC64(image);
        case SYS_APPLE2:
            return DrawApple2Image(image);
        case SYS_APPLE2_LINES:
            return DrawApple2VectorImage(image);
        case SYS_C64:
        case SYS_ATARI8:
            return DrawC64A8Image(image);
        case SYS_ATARI8_LINES:
            DrawAtari8BitVectorImage(image);
            return 1;
        case SYS_TI994A:
            DrawRTPIImage(image);
            return 1;
        default:
            return 0;
    }
}

void DrawInventoryImages(void)
{
    USImage *image = USImages;
    if (image != NULL) {
        do {
            if ((image->usage == IMG_INV_OBJ || image->usage == IMG_INV_AND_ROOM_OBJ) &&
                image->index <= GameHeader.NumItems &&
                Items[image->index].Location == CARRIED) {
                DrawUSImage(image);
            }
            image = image->next;
        } while (image != NULL);
    }
}

void DrawRoomObjectImages(void)
{
    USImage *image = USImages;
    if (image != NULL) {
        do {
            if ((image->usage == IMG_ROOM_OBJ ||
                 image->usage == IMG_INV_AND_ROOM_OBJ) &&
                image->index <= GameHeader.NumItems && Items[image->index].Location == MyLoc) {
                DrawUSImage(image);
            }
            image = image->next;
        } while (image != NULL);
    }
}

int DrawUSRoom(int room)
{
    USImage *image = USImages;
    if (image != NULL) {
        do {
            if (image->usage == IMG_ROOM && image->index == room) {
                return DrawUSImage(image);
            }
            image = image->next;
        } while (image != NULL);
    }
    return 0;
}

int DrawFuzzyRoom(int room)
{
    // If at sea at night, and floodlights are off or battery is out, don't draw fuzzy image
    // (because it really is dark)
    if (Items[32].Location == DESTROYED && ((BitFlags & (1 << 29)) == 0 || CurrentCounter <= 0))
        return 0;
    // Also don't draw fuzzy image if bitflag 2 is set, which means we are
    // wearing glasses or mask with lenses
    if ((BitFlags & (1 << 2)) == 0)
        return 0;

    USImage *image = USImages;
    if (image != NULL) {
        do {
            if (image->usage == IMG_ROOM && image->index == room) {
                return DrawFuzzyRTPIImage(image);
            }
            image = image->next;
        } while (image != NULL);
    }
    return 0;
}

void DrawUSRoomObject(int item)
{
    USImage *image = USImages;
    if (image != NULL) {
        do {
            if ((image->usage == IMG_ROOM_OBJ ||
                image->usage == IMG_INV_AND_ROOM_OBJ) &&
                image->index == item) {
                DrawUSImage(image);
                return;
            }
            image = image->next;
        } while (image != NULL);
    }
}

void LookUS(void)
{
    // We should draw an image:
    // • after closeups and inventory•
    // • after restore and undo
    // • after successful taking and dropping
    // • after resizing or changing preferences
    // • when light is turned on or off•
    if (!Graphics || !(should_look_in_transcript || should_draw_image))
        return;

    glk_window_clear(Top);

    int room = MyLoc;

    // The US disk releases (except Apple II) of The Incredible Hulk
    // reuse images for several locations.
    if (CurrentGame == HULK_US && USImages && USImages->systype != SYS_APPLE2)
        switch (MyLoc) {
        case 5: // Tunnel going outside
        case 6:
            room = 3;
            break;
        case 7: // Field
        case 8:
            room = 4;
            break;
        case 10: // Hole
        case 11:
            room = 9;
            break;
        case 13: // Dome
        case 14:
            room = 2;
            break;
        case 17: // warp
        case 18:
            room = 16;
            break;
        default:
            break;
        }

    if (should_draw_image) {
        glk_window_clear(Graphics);
        if (!DrawUSRoom(room)) {
            return;
        }
        DrawRoomObjectImages();

        switch (CurrentGame) {
            case HULK_US:
                if (Items[18].Location == MyLoc && MyLoc == Items[18].InitialLoc) // Bio Gem
                    DrawUSRoomObject(70);
                if (Items[21].Location == MyLoc && MyLoc == Items[21].InitialLoc) // Wax
                    DrawUSRoomObject(72);
                if (Items[14].Location == MyLoc || Items[15].Location == MyLoc) // Large pit
                    DrawUSRoomObject(13);
                break;
            case COUNT_US:
                if (Items[17].Location == MyLoc && MyLoc == 8) // Only draw mirror in bathroom
                    DrawUSRoomObject(80);
                if (Items[35].Location == MyLoc && MyLoc == 18) // Only draw other end of sheet in pit
                    DrawUSRoomObject(81);
                if (Items[5].Location == MyLoc && MyLoc == 9) // Only draw coat-of-arms at gate
                    DrawUSRoomObject(82);
                break;
            case VOODOO_CASTLE_US:
                if (Items[45].Location == MyLoc && MyLoc == 14) // Only draw boards in chimney
                    DrawUSRoomObject(80);
                break;
            case RETURN_TO_PIRATES_ISLE:
                if (MyLoc == 22) {
                    DrawUSRoomObject(27); // Draw underside of dock when under the dock
                    DrawUSRoom(31); // Draw low beam (on top of underside of dock)
                } else if (Items[27].Location == MyLoc) {
                    DrawUSRoom(30); // Draw high beam (on top of underside of dock)
                } else if (MyLoc == 11 && Items[32].Location == DESTROYED) {
                    DrawUSRoom(29); // Draw sea at night
                    if (Items[28].Location == MyLoc) {
                        DrawUSRoom(32); // Boat to the west at night
                    }
                }
            default:
                break;
        }
        DrawImageOrVector();
    }
    should_draw_image = 0;
}

void InventoryUS(void)
{
    if (!Graphics || !HasGraphics())
        return;
    glk_window_clear(Graphics);
    if (!DrawUSRoom(98)) {
        should_draw_image = 1;
        LookUS();
        return;
    }
    DrawInventoryImages();
    DrawImageOrVector();
    if (!showing_inventory) {
        showing_inventory = 1;
        Output(sys[HIT_ENTER]);
        HitEnter();
    }
}

static int SanityCheckScottFreeHeader(int ni, int na, int nw, int nr, int mc)
{
    int16_t v = header[1]; // items
    if (v < 10 || v > 500)
        return 0;
    v = header[2]; // actions
    if (v < 100 || v > 500)
        return 0;
    v = header[3]; // word pairs
    if (v < 48 || v > 200)
        return 0;
    v = header[4]; // Number of rooms
    if (v < 10 || v > 100)
        return 0;
    v = header[5]; // Number of Messages
    if (v < 10 || v > 255)
        return 0;

    return 1;
}

uint8_t *Skip(uint8_t *ptr, int count, uint8_t *eof)
{
#if (DEBUG_PRINT)
    for (int i = 0; i < count && ptr + i + 1 < eof; i += 2) {
        uint16_t val = READ_LE_UINT16(ptr + i);
        debug_print("Unknown value %d: %d (%x)\n", i / 2, val, val);
    }
#endif
    return ptr + count;
}

uint8_t *LoadHeader(uint8_t *ptr, size_t length, GameInfo info, int dict_start) {
    int ni, na, nw, nr, mc, pr, tr, wl, lt, mn, trm;

    size_t offset;

    uint8_t *data = ptr;

    if (dict_start) {
        file_baseline_offset = dict_start - info.start_of_dictionary - 645;
        debug_print("LoadBinaryDatabase: file baseline offset:%d\n",
                    file_baseline_offset);
        offset = info.start_of_header + file_baseline_offset;
        if (offset < length)
            ptr = data + offset;
        else
            return NULL;
    } else {
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
        fprintf(stderr, "Version: %d\n", version);
        debug_print("Version: %d\n", version);
        debug_print("Adventure number: %d\n", adventure_number);
        if (adventure_number == 0)
            return 0;
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

    if (ptr == NULL)
        return NULL;

    ptr = ReadHeader(ptr);

    ParseHeader(header, US_HEADER, &ni, &na, &nw, &nr, &mc, &pr, &tr,
                &wl, &lt, &mn, &trm);

    PrintHeaderInfo(header, ni, na, nw, nr, mc, pr, tr, wl, lt, mn, trm);

    if (!SanityCheckScottFreeHeader(ni, na, nw, nr, mc))
        return NULL;

    GameHeader.NumItems = ni;
    Items = (Item *)MemAlloc(sizeof(Item) * (ni + 1));
    GameHeader.NumActions = na;
    Actions = (Action *)MemAlloc(sizeof(Action) * (na + 1));
    GameHeader.NumWords = nw;
    GameHeader.WordLength = wl;
    Verbs = MemAlloc(sizeof(char *) * (nw + 2));
    Nouns = MemAlloc(sizeof(char *) * (nw + 2));
    GameHeader.NumRooms = nr;
    Rooms = (Room *)MemAlloc(sizeof(Room) * (nr + 1));
    GameHeader.MaxCarry = mc;
    GameHeader.PlayerRoom = pr;
    GameHeader.Treasures = tr;
    GameHeader.LightTime = lt;
    LightRefill = lt;
    GameHeader.NumMessages = mn;
    Messages = MemAlloc(sizeof(char *) * (mn + 1));
    GameHeader.TreasureRoom = trm;

    /* if dict_start > 0, we are checking for the UK version of Questprobe featuring The Hulk */
    if (dict_start) {
        if (header[0] != info.word_length || header[1] != info.number_of_words || header[2] != info.number_of_actions || header[3] != info.number_of_items || header[4] != info.number_of_messages || header[5] != info.number_of_rooms || header[6] != info.max_carried) {
            //    debug_print("Non-matching header\n");
            return NULL;
        }
    }
    return ptr;
}

uint8_t *ParseDictionary(uint8_t *ptr, uint8_t *endptr) {
    while (!(*ptr == 'A' && *(ptr + 1) == 'N' && *(ptr + 2) == 'Y') && ptr < endptr)
        ptr++;

    if (ptr >= endptr)
        return NULL;

    return ReadUSDictionary(ptr);
}

uint8_t *ParseRooms(uint8_t *ptr, uint8_t *endptr, int number_of_rooms) {
    int counter = 0;
    Room *rp = Rooms;

    uint8_t string_length = 0;
    do {
        string_length = *ptr++;
        if (string_length == 0) {
            rp->Text = ".";
        } else {
            rp->Text = MemAlloc(string_length + 1);
            for (int i = 0; i < string_length && ptr < endptr; i++) {
                rp->Text[i] = *ptr++;
            }
            rp->Text[string_length] = 0;
        }
        debug_print("Room %d: \"%s\"\n", counter, rp->Text);
        rp++;
        counter++;
    } while (counter <= number_of_rooms);
    return ptr;
}

uint8_t *ParseMessages(uint8_t *ptr, uint8_t *endptr, int number_of_messages) {
    int counter = 0;
    char *string;

    do {
        uint8_t string_length = *ptr++;
        if (string_length == 0) {
            string = ".";
        } else {
            string = MemAlloc(string_length + 1);
            for (int i = 0; i < string_length && ptr < endptr; i++) {
                string[i] = *ptr++;
            }
            string[string_length] = 0;
        }
        Messages[counter] = string;
        debug_print("Message %d: \"%s\"\n", counter, Messages[counter]);
        counter++;
    } while (counter < number_of_messages + 1);
    return ptr;
}

uint8_t *ParseItems(uint8_t *ptr, uint8_t *endptr, int number_of_items) {
    int counter = 0;
    Item *ip = Items;

    do {
        int string_length = *ptr++;
        if (string_length == 0) {
            ip->Text = ".";
            ip->AutoGet = NULL;
        } else {
            ip->Text = MemAlloc(string_length + 1);

            for (int i = 0; i < string_length && ptr < endptr; i++) {
                ip->Text[i] = *ptr++;
            }
            ip->Text[string_length] = 0;
            ip->AutoGet = strchr(ip->Text, '/');
            /* Some games use // to mean no auto get/drop word! */
            if (ip->AutoGet && strcmp(ip->AutoGet, "//") && strcmp(ip->AutoGet, "/*") && ptr < endptr) {
                char *t;
                *ip->AutoGet++ = 0;
                t = strchr(ip->AutoGet, '/');
                if (t != NULL)
                    *t = 0;
                ptr++;
            }
        }

        debug_print("Item %d: %s\n", counter, ip->Text);
        if (ip->AutoGet)
            debug_print("Autoget:%s\n", ip->AutoGet);

        counter++;
        ip++;
    } while (counter <= number_of_items && ptr < endptr);

    ptr++;
    counter = 0;
    ip = Items;
    while (counter <= number_of_items && ptr < endptr) {
        ip->Location = *ptr;
        ip->InitialLoc = ip->Location;
        debug_print("Item %d (%s) start location: %d\n", counter, Items[counter].Text, ip->Location);
        ptr += 2;
        ip++;
        counter++;
    }

    return ptr;
}

uint8_t *ParseActions(uint8_t *ptr, uint8_t *data, size_t datalength, int number_of_actions) {
    int counter = 0;
    Action *ap = Actions;

    size_t offset = ptr - data;
    size_t offset2;

    int verb, noun, value, value2, plus = 0;
    while (counter <= number_of_actions && offset + counter + plus < datalength) {
        plus = number_of_actions + 1;
        verb = data[offset + counter];
        noun = data[offset + counter + plus];
        debug_print("Action %d: verb:%d noun:%d\n", counter, verb, noun);

        ap->Vocab = verb * 150 + noun;

        for (int j = 0; j < 2; j++) {
            plus += number_of_actions + 1;
            value = data[offset + counter + plus];
            plus += number_of_actions + 1;
            value2 = data[offset + counter + plus];
            ap->Subcommand[j] = 150 * value + value2;
            debug_print("Action %d: Subcommand[%d]: %d %d\n", counter, j, value, value2);
        }

        offset2 = offset + 6 * (number_of_actions + 1);
        plus = 0;

        for (int j = 0; j < 5 && offset2 + counter * 2 + plus < datalength; j++) {
            ap->Condition[j] = READ_LE_UINT16(data + offset2 + counter * 2 + plus);
            ptr = data + offset2 + counter * 2 + plus + 2;
            plus += (number_of_actions + 1) * 2;
            debug_print("Action %d: Condition %d: %d\n", counter, j, ap->Condition[j]);
        }

        ap++;
        counter++;
    }
    return ptr;
}

uint8_t *ParseRoomConnections(uint8_t *ptr, uint8_t *endptr, int number_of_rooms) {
    /* The room connections are ordered by direction, not room, so all the North
     * connections for all the rooms come first, then the South connections, and
     * so on. */
    for (int j = 0; j < 6; j++) {
        int counter = 0;
        Room *rp = Rooms;

        while (counter <= number_of_rooms && ptr < endptr) {
            rp->Image = 255;
            rp->Exits[j] = *ptr;
            debug_print("Room %d (%s) exit %d (%s): %d\n", counter, Rooms[counter].Text, j, Nouns[j + 1], rp->Exits[j]);
            ptr += 2;
            counter++;
            rp++;
        }
    }
    return ptr;
}

void FreeDatabase(void);

GameIDType HulkUKSpecificParsing(uint8_t *ptr, uint8_t *endptr, GameInfo info) {
#pragma mark room images
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

#pragma mark item images

    if (SeekIfNeeded(info.start_of_item_image_list, &offset, &ptr) == 0) {
        FreeDatabase();
        return UNKNOWN_GAME;
    }

    Item *ip = Items;

    for (int i = 0; i <= GameHeader.NumItems; i++) {
        ip->Image = 0xff;
        ip++;
    }

    int index, image = 10;

    do {
        index = *ptr++;
        if (index != 0xff)
            Items[index].Image = image++;
    } while (index != 0xff && ptr < endptr);

    return info.gameID;
}

GameIDType FreeGameResources(void) {
    FreeDatabase();
    return UNKNOWN_GAME;
}

GameIDType LoadBinaryDatabase(uint8_t *data, size_t length, GameInfo info, int dict_start) {
    FreeDatabase();
    uint8_t *ptr = data;

    uint8_t *endptr =  data + length - 2;

    // Parse the header
    ptr = LoadHeader(ptr, length, info, dict_start);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    // Parse the dictionary
    ptr = ParseDictionary(ptr, endptr);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    // Parse rooms
    ptr = ParseRooms(ptr, endptr, GameHeader.NumRooms);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    // Parse messages
    ptr = ParseMessages(ptr, endptr, GameHeader.NumMessages);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    // Parse items
    ptr = ParseItems(ptr, endptr, GameHeader.NumItems);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    // Skip item strings lookup table
    ptr = Skip(ptr, (GameHeader.NumItems + 1) * 4, data + length);

    // Parse actions
    ptr = ParseActions(ptr, data, length, GameHeader.NumActions);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    // Skip room descriptions lookup table
    ptr = Skip(ptr, (GameHeader.NumRooms + 1) * 2, data + length);

    endptr += 2;

    ptr = ParseRoomConnections(ptr, endptr, GameHeader.NumRooms);
    if (ptr == NULL || ptr > endptr) {
        return FreeGameResources();
    }

    // The dict_start parameter is only set if we are
    // trying to detect the Adventure International UK
    // release of Questprobe featuring The Incredible Hulk.
    if (dict_start) {
        return HulkUKSpecificParsing(ptr, endptr, info);
    }

    return CurrentGame;
}


// Compare two files from the end to the start of the shortest one, skipping the file extension
int CompareFilenames(const char *str1, size_t length1, const char *str2, size_t length2)
{
    while (length1 > 0 && str1[length1] != '.') {
        length1--;
    }
    while (length2 > 0 && str2[length2] != '.') {
        length2--;
    }
    size_t length = MIN(length1, length2);
    if (length <= 0)
        return 0;
    for (int i = length; i > 0; i--) {
        if (str1[length1--] != str2[length2--]) {
            return 0;
        }
    }
    return 1;
}

// Give the companion file name the file extension of the game file.
// This makes the companion file logic work even if the player has renamed the files to, say, end with ".saga"
// (well, as long as both files have been renamed in the same way.)
char *AddGameFileExtension(const char *companion_name, size_t *namelength,
                           const char *game_path, size_t gamepathlen) {
    if (!companion_name || !namelength || !game_path) {
        fprintf(stderr, "AddGameFileExtension: NULL parameter\n");
        return NULL;
    }

    // Find extension in game_path
    const char *game_ext = strrchr(game_path, '.');
    // Find extension in filename
    const char *companion_ext = strrchr(companion_name, '.');

    //(It should not really be possible for any of them
    // to not have an extension)
    size_t extension_length = game_ext ? (gamepathlen - (game_ext - game_path)) : 0;
    size_t base_length = companion_ext ? companion_ext - companion_name : *namelength;

    *namelength = base_length + extension_length;

    // Allocate result
    char *result = MemAlloc(*namelength + 1);

    // Build result string
    memcpy(result, companion_name, base_length + 1);
    if (extension_length > 0) {
        // Copy includes terminating 0
        memcpy(result + base_length, game_ext, extension_length + 1);
    }
    return result;
}

// Helper to get filename length
static size_t get_filename_length(const char *filename, size_t length_field)
{
    return (length_field != 0) ? length_field : strlen(filename);
}

char *LookForCompanionFilenameInDatabase(const pairrec list[][2], size_t game_filename_len, size_t *out_companion_len)
{
    for (int i = 0; list[i][0].filename != NULL; i++) {
        for (int j = 0; j < 2; j++) {
            const char *candidate = list[i][j].filename;
            size_t candidate_len = get_filename_length(candidate, list[i][j].stringlength);
            if (CompareFilenames(game_file, game_filename_len, candidate, candidate_len)) {
                // Found match; use the companion (the other entry in the pair)
                int companion_idx = 1 - j;
                const char *companion = list[i][companion_idx].filename;
                *out_companion_len = get_filename_length(companion, list[i][companion_idx].stringlength);
                // Copy the game file extension to the companion file
                return AddGameFileExtension(companion, out_companion_len, game_file, game_filename_len);
            }
        }
    }
    return NULL;
}

// Look for a match for game file name in the list, and return a
// search path to the corresponding compantion file name.
// We do not check whether that file exists here.
char *LookInDatabase(const pairrec list[][2], const char *game_path, size_t pathlen)
{
    size_t resultlen;
    char *foundname = LookForCompanionFilenameInDatabase(list, pathlen, &resultlen);
    if (foundname != NULL) {
        const char *file_delimiter_position = strrchr(game_file, '/');
        if (file_delimiter_position == NULL)
            file_delimiter_position = strrchr(game_path, '\\');
        size_t stringlen;
        if (file_delimiter_position == NULL) {
            // Found no search path before the game file name.
            // We assume that means it is in the current working directory
            // and just return the plain companion file name.
            return foundname;
        } else {
            stringlen = file_delimiter_position - game_path + 1;
        }
        // Add the search path to the found companion file name
        // (i.e. the path to the location of the game file)
        size_t newlen = stringlen + resultlen;
        char *path = MemAlloc(newlen + 1);
        memcpy(path, game_path, stringlen);
        memcpy(path + stringlen, foundname, resultlen + 1);
        free(foundname);
        return path;
    }

    return NULL;
}
