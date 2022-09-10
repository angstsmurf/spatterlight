//
//  hulk.c
//  scott
//
//  Created by Administrator on 2022-01-18.
//
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "detectgame.h"
#include "scott.h"
#include "pcdraw.h"
#include "hulk.h"
#include "atari8c64draw.h"

static const char *DOSFilenames[] =
{ "B01024R", "B01044I", "R0109", "R0190",
    "B01001I", "B01025R", "B01045R", "R0112", "R0191", "B01002I", "B01026R", "B01046I", "R0115", "R0192", "B01003I", "B01029I", "B01048I", "R0116", "R0193", "B01004I", "B01030I", "B01050I", "R0119", "R0194", "B01012I", "B01031I", "B01051I", "R0120", "R0195", "B01013R", "B01032I", "B01053R", "R0181", "R0196", "B01016I", "B01033R", "B01070R", "R0182", "R0197", "B01017R", "B01034R", "B01072R", "R0183", "R0198", "B01018I", "B01035I", "R0184", "R0199", "B01019I", "B01036R", "R0100", "R0185", "B01020R", "B01037I", "R0101", "R0186", "B01021I", "B01039R", "R0102", "R0187", "B01022R", "B01040R", "R0103", "R0188", "B01023I", "B01042I", "R0104", "R0189", NULL };

int LoadDOSImages(void) {
    USImages = new_image();

    struct USImage *image = USImages;
    size_t datasize;
    int found = 0;

    for (int i = 0; DOSFilenames[i] != NULL; i++) {
        const char *shortname = DOSFilenames[i];
        uint8_t *data = FindImageFile(shortname, &datasize);

        if (data) {
            fprintf(stderr, "Found image file %s\n", shortname);
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
            fprintf(stderr, "Usage %d Index:%d\n", image->usage, image->index);
            image->next = new_image();
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

uint8_t *ReadHeader(uint8_t *ptr);
// extern void print_header_info(int header[]);
extern int header[];

void DrawUSImage(USImage *image) {
    if (image->systype == SYS_MSDOS)
        DrawDOSImage(image);
    else if (image->systype == SYS_C64 || image->systype == SYS_ATARI8)
        DrawAtariC64Image(image);
}

void DrawInventoryImages(void) {
    struct USImage *image = USImages;
    if (image != NULL) {
        do {
            if (image->usage == IMG_INV_OBJ && Items[image->index].Location == CARRIED) {
                DrawUSImage(image);
            }
            image = image->next;
        } while (image != NULL);
    }
}

void DrawRoomObjectImages(void) {
    struct USImage *image = USImages;
    if (image != NULL) {
        do {
            if (image->usage == IMG_ROOM_OBJ && image->index <= GameHeader.NumItems && Items[image->index].Location == MyLoc) {
                DrawUSImage(image);
            }
            image = image->next;
        } while (image != NULL);
    }
}

void DrawUSRoom(int room) {
    struct USImage *image = USImages;
    if (image != NULL) {
        do {
            if (image->usage == IMG_ROOM && image->index == room) {
                DrawUSImage(image);
                return;
            }
            image = image->next;
        } while (image != NULL);
    }
}

void DrawUSRoomObject(int item) {
    struct USImage *image = USImages;
    if (image != NULL) {
        do {
            if (image->usage == IMG_ROOM_OBJ && image->index == item) {
                DrawUSImage(image);
                return;
            }
            image = image->next;
        } while (image != NULL);
    }
}

void HulkInventoryUS(void)
{
    DrawUSRoom(98);
    DrawInventoryImages();
    Output(sys[HIT_ENTER]);
    HitEnter();
}

void HulkShowImageOnExamineUS(int noun) {
    int image = -1;
    switch (noun) {
        case 55:
            if (Items[11].Location == MyLoc)
                // Dome
                image = 0;
            break;
        case 124: // Bio-Gem
        case 41:
            if (Items[18].Location == MyLoc || Items[18].Location == CARRIED)
                image = 1;
            break;
        case 108:
            if (Items[17].Location == MyLoc || Items[17].Location == CARRIED)
                // Natter energy egg
                image = 2;
            break;
        case 72:
            if (Items[20].Location == MyLoc || Items[20].Location == CARRIED)
                // Alien army ants
                image = 3;
            break;
        case 21: // Killer Bees
            if (Items[24].Location == MyLoc)
                image = 4;
            break;
        case 83: // Iron ring
            if (Items[33].Location == MyLoc)
                image = 5;
            break;
        case 121: // Cage
            if (Items[47].Location == MyLoc)
                image = 6;
            break;
        case 26: // BASE
        case 66: // HOLE
        case 67: // OUTL
        case 68: // GAS
            if (MyLoc == 14)
                image = 7;
            break;
        default:
            break;
    }

    if (image >= 0) {
        if (Graphics)
            glk_window_clear(Graphics);
        DrawUSRoom(90 + image);
        Output(sys[HIT_ENTER]);
        HitEnter();
    }
}

void HulkShowImageOnExamine(int noun)
{
    if (CurrentGame == HULK_US) {
        HulkShowImageOnExamineUS(noun);
        return;
    }
    int image = 0;
    switch (noun) {
    case 55:
        if (Items[11].Location == MyLoc)
            // Dome
            image = 28;
        break;
    case 108:
        if (Items[17].Location == MyLoc || Items[17].Location == CARRIED)
            // Natter energy egg
            image = 30;
        break;
    case 124: // Bio-Gem
    case 41:
        if (Items[18].Location == MyLoc || Items[18].Location == CARRIED)
            image = 29;
        break;
    case 21: // Killer Bees
        if (Items[24].Location == MyLoc)
            image = 31;
        break;
    case 83: // Iron ring
        if (Items[33].Location == MyLoc)
            image = 32;
        break;
    case 121: // Cage
        if (Items[47].Location == MyLoc)
            image = 33;
        break;
    default:
        break;
    }
    if (image) {
        DrawImage(image);
        Output(sys[HIT_ENTER]);
        HitEnter();
    }
}

void HulkLook(void)
{
    DrawImage(Rooms[MyLoc].Image);
    for (int ct = 0; ct <= GameHeader.NumItems; ct++) {
        int image = Items[ct].Image;
        if (Items[ct].Location == MyLoc && image != 255) {
            /* Don't draw bio gem in fuzzy area */
            if ((ct == 18 && MyLoc != 15) ||
                /* Don't draw Dr. Strange until outlet is plugged */
                (ct == 26 && Items[28].Location != MyLoc))
                continue;
            DrawImage(image);
        }
    }
}

void HulkLookUS(void)
{
    if (CurrentGame == HULK_US)
        fprintf(stderr, "CurrentGame == HULK_US\n");
    else
        fprintf(stderr, "CurrentGame == %d\n", CurrentGame);

    glk_window_clear(Top);

    int room = MyLoc;

    if (CurrentGame == HULK_US)
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

    if (Graphics)
        glk_window_clear(Graphics);

    DrawUSRoom(room);
    DrawRoomObjectImages();
    if (CurrentGame == HULK_US) {
        if (Items[18].Location == MyLoc && MyLoc == Items[18].InitialLoc)
            DrawUSRoomObject(70);
        if (Items[21].Location == MyLoc && MyLoc == Items[21].InitialLoc)
            DrawUSRoomObject(72);
        if (Items[14].Location == MyLoc || Items[15].Location == MyLoc)
            DrawUSRoomObject(13);
    }
}

void DrawHulkImage(int p)
{
    if (CurrentGame == HULK_US) {
        DrawUSRoom(p);
        Output(sys[HIT_ENTER]);
        HitEnter();
        return;
    }
    int image = 0;
    switch (p) {
    case 85:
        image = 34;
        break;
    case 86:
        image = 35;
        break;
    case 83:
        image = 36;
        break;
    case 84:
        image = 37;
        break;
    case 87:
        image = 38;
        break;
    case 88:
        image = 39;
        break;
    case 89:
        image = 40;
        break;
    case 82:
        image = 41;
        break;
    case 81:
        image = 42;
        break;
    default:
        fprintf(stderr, "Unhandled image number %d!\n", p);
        break;
    }

    if (image != 0) {
        DrawImage(image);
        Output(sys[HIT_ENTER]);
        HitEnter();
    }
}

uint8_t *read_US_dictionary(uint8_t *ptr)
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
                        fprintf(stderr, "Nouns %d: \"%s\"\n", wordnum,
                        Nouns[wordnum]);
        } else {
            Verbs[wordnum - nn] = MemAlloc(charindex + 1);
            memcpy((char *)Verbs[wordnum - nn], dictword, charindex + 1);
                        fprintf(stderr, "Verbs %d: \"%s\"\n", wordnum - nn,
                        Verbs[wordnum - nn]);
        }
        wordnum++;

        if (c != 0 && !isascii(c))
            return ptr;

        charindex = 0;
    } while (wordnum <= GameHeader.NumWords * 2 + 1);

    return ptr;
}

int ParseHeader(int *h, HeaderType type, int *ni, int *na, int *nw, int *nr,
    int *mc, int *pr, int *tr, int *wl, int *lt, int *mn,
    int *trm);

void PrintHeaderInfo(int *h, int ni, int na, int nw, int nr, int mc, int pr,
    int tr, int wl, int lt, int mn, int trm);

extern size_t hulk_coordinates;
extern size_t hulk_item_image_offsets;
extern size_t hulk_look_image_offsets;
extern size_t hulk_special_image_offsets;
extern size_t hulk_image_offset;

uint8_t *Skip(uint8_t *ptr, int count, uint8_t *eof) {
    for (int i = 0; i < count && ptr + i + 1 < eof; i += 2) {
        uint16_t val =  ptr[i] + ptr[i+1] * 0x100;
        fprintf(stderr, "Unknown value %d: %d (%x)\n", i/2, val, val);
    }
    return  ptr + count;
}

int LoadBinaryDatabase(uint8_t *data, size_t length, struct GameInfo info, int dict_start)
{
    int ni, na, nw, nr, mc, pr, tr, wl, lt, mn, trm;
    int ct;

    Action *ap;
    Room *rp;
    Item *ip;

    /* Load the header */
    uint8_t *ptr = data;

    size_t offset;

    if (dict_start) {
        file_baseline_offset = dict_start - info.start_of_dictionary - 645;
        fprintf(stderr, "HULK: file baseline offset:%d\n",
                file_baseline_offset);
        offset = info.start_of_header + file_baseline_offset;
        ptr = SeekToPos(data, offset);
    } else {
        int version = 0;
        int adventure_number = 0;
        for (int i = 0; i < 0x38; i += 2) {
            int value = ptr[i] + ptr[i + 1] * 0x100;
            if (value < 255) {
                if (version == 0)
                    version = value;
                else {
                    adventure_number = value;
                    break;
                }
            }
        }
        fprintf(stderr, "Version: %d\n", version);
        fprintf(stderr, "Adventure number: %d\n", adventure_number);
        if (version == 127 && adventure_number == 1)
            CurrentGame = HULK_US;
        else
            CurrentGame = adventure_number;
        Game->type = US_VARIANT;
        ptr = Skip(ptr, 0x38, data + length);
    }

    if (ptr == NULL)
        return 0;

    ptr = ReadHeader(ptr);

    ParseHeader(header, US_HEADER, &ni, &na, &nw, &nr, &mc, &pr, &tr,
                &wl, &lt, &mn, &trm);

    PrintHeaderInfo(header, ni, na, nw, nr, mc, pr, tr, wl, lt, mn, trm);

    GameHeader.NumItems = ni;
    Items = (Item *)MemAlloc(sizeof(Item) * (ni + 1));
    GameHeader.NumActions = na;
    Actions = (Action *)MemAlloc(sizeof(Action) * (na + 1));
    GameHeader.NumWords = nw;
    GameHeader.WordLength = wl;
    Verbs = MemAlloc(sizeof(char *) * (nw + 1));
    Nouns = MemAlloc(sizeof(char *) * (nw + 1));
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
            //    fprintf(stderr, "Non-matching header\n");
            return 0;
        }
    }

#pragma mark Dictionary

    while (!(*ptr == 'A' && *(ptr + 1) == 'N' && *(ptr + 2) == 'Y') && ptr - data < length - 2)
        ptr++;

    if (ptr - data >= length - 2)
        return 0;

    ptr = read_US_dictionary(ptr);

#pragma mark Rooms

    fprintf(stderr, "Offset %lx\n", ptr-data);

    ct = 0;
    rp = Rooms;

    uint8_t string_length = 0;
    do {
        string_length = *(ptr++);
        if (string_length == 0) {
            rp->Text = ".\0";
        } else {
            rp->Text = MemAlloc(string_length + 1);
            for (int i = 0; i < string_length; i++) {
                rp->Text[i] = *(ptr++);
            }
            rp->Text[string_length] = 0;
        }
        fprintf(stderr, "Room %d: \"%s\"\n", ct, rp->Text);
        rp++;
        ct++;
    } while (ct < nr + 1);

#pragma mark Messages

    ct = 0;
    char *string;

    do {
        string_length = *(ptr++);
        if (string_length == 0) {
            string = ".\0";
        } else {
            string = MemAlloc(string_length + 1);
            for (int i = 0; i < string_length; i++) {
                string[i] = *(ptr++);
            }
            string[string_length] = 0;
        }
        Messages[ct] = string;
        fprintf(stderr, "Message %d: \"%s\"\n", ct, Messages[ct]);
        ct++;
    } while (ct < mn + 1);

#pragma mark Items

    ct = 0;
    ip = Items;

    do {
        string_length = *(ptr++);
        if (string_length == 0) {
            ip->Text = ".\0";
            ip->AutoGet = NULL;
        } else {
            ip->Text = MemAlloc(string_length + 1);

            for (int i = 0; i < string_length; i++) {
                ip->Text[i] = *(ptr++);
            }
            ip->Text[string_length] = 0;
            ip->AutoGet = strchr(ip->Text, '/');
            /* Some games use // to mean no auto get/drop word! */
            if (ip->AutoGet && strcmp(ip->AutoGet, "//") && strcmp(ip->AutoGet, "/*")) {
                char *t;
                *ip->AutoGet++ = 0;
                t = strchr(ip->AutoGet, '/');
                if (t != NULL)
                    *t = 0;
                ptr++;
            }
        }

        fprintf(stderr, "Item %d: %s\n", ct, ip->Text);
        if (ip->AutoGet)
            fprintf(stderr, "Autoget:%s\n", ip->AutoGet);

        ct++;
        ip++;
    } while (ct <= ni);

#pragma mark item locations

    ptr++;
    ct = 0;
    ip = Items;
    while (ct < ni + 1) {
        ip->Location = *ptr;
        ip->InitialLoc = ip->Location;
        fprintf(stderr, "Item %d (%s) start location: %d\n", ct, Items[ct].Text, ip->Location);
        ptr += 2;
        ip++;
        ct++;
    }

    // Image strings lookup table
    ptr = Skip(ptr, (ni + 1) * 4,  data + length);

#pragma mark Actions

    ct = 0;
    ap = Actions;

    offset = ptr - data;
    size_t offset2 = offset;

    int verb, noun, value, value2, plus = na + 1;
    while (ct <= na) {
        plus = na + 1;
        verb = data[offset + ct];
        noun = data[offset + ct + plus];

        ap->Vocab = verb * 150 + noun;

        for (int j = 0; j < 2; j++) {
            plus += na + 1;
            value = data[offset + ct + plus];
            plus += na + 1;
            value2 = data[offset + ct + plus];
            ap->Subcommand[j] = 150 * value + value2;
        }

        offset2 = offset + 6 * (na + 1);
        plus = 0;

        for (int j = 0; j < 5; j++) {
            value = data[offset2 + ct * 2 + plus];
            value2 = data[offset2 + ct * 2 + plus + 1];
            ptr = &data[offset2 + ct * 2 + plus + 2];
            ap->Condition[j] = value + value2 * 0x100;
            plus += (na + 1) * 2;
        }
        ap++;
        ct++;
    }

    // Room descriptions lookup table
    ptr = Skip(ptr, (nr + 1) * 2, data + length);

#pragma mark Room connections
    /* The room connections are ordered by direction, not room, so all the North
     * connections for all the rooms come first, then the South connections, and
     * so on. */
    for (int j = 0; j < 6; j++) {
        ct = 0;
        rp = Rooms;

        while (ct < nr + 1) {
            rp->Exits[j] = *ptr;
            fprintf(stderr, "Room %d (%s) exit %d (%s): %d\n", ct, Rooms[ct].Text, j, Nouns[j + 1], rp->Exits[j]);
            ptr += 2;
            ct++;
            rp++;
        }
    }

    // Return if not reading UK Hulk
    if (!dict_start) {
        ptr = Skip(ptr, 0xe4, data + length);
        return 1;
    }

#pragma mark room images

    if (SeekIfNeeded(info.start_of_room_image_list, &offset, &ptr) == 0)
        return 0;

    rp = Rooms;

    for (ct = 0; ct <= GameHeader.NumRooms; ct++) {
        rp->Image = *(ptr++);
        //        fprintf(stderr, "Room %d (%s) has image %d\n", ct, rp->Text,
        //        rp->Image );
        rp++;
    }

#pragma mark item images

    if (SeekIfNeeded(info.start_of_item_image_list, &offset, &ptr) == 0)
        return 0;

    ip = Items;

    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        ip->Image = 255;
        ip++;
    }

    int index, image = 10;

    do {
        index = *ptr++;
        if (index != 255)
            Items[index].Image = image++;
    } while (index != 255);

    if (CurrentGame == HULK_C64) {
        hulk_coordinates = 0x22cd;
        hulk_item_image_offsets = 0x2731;
        hulk_look_image_offsets = 0x2761;
        hulk_special_image_offsets = 0x2781;
        hulk_image_offset = -0x7ff;
    }

    return 1;
}
