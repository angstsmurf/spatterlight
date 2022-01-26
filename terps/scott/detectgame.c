//
//  detectgame.c
//  scott
//
//  Created by Administrator on 2022-01-10.
//

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <strings.h>

#include "scott.h"

#include "detectgame.h"
#include "gameinfo.h"
#include "decompresstext.h"
#include "sagadraw.h"

#include "robinofsherwood.h"
#include "seasofblood.h"
#include "gremlins.h"
#include "hulk.h"

#include "parser.h"
#include "decompressz80.h"

extern const char *sysdict_zx[MAX_SYSMESS];
extern int header[];

extern void print_header_info(int header[]);

static int FindCode(char *x, int base)
{
    unsigned const char *p = entire_file + base;
    int len = strlen(x);
    if (len < 7) len = 7;
//    fprintf(stderr, "FindCode: %s len: %d file_length:%lu\n", x, len, file_length);
    while(p < entire_file + file_length - len) {
        if(memcmp(p, x, len) == 0) {
//            fprintf(stderr, "FindCode: found %s at offset %lu\n", x, p - entire_file);
            return p - entire_file;
        }
        p++;
    }
    return -1;
}

dictionary_type getId(size_t *offset) {
    *offset = FindCode("AUTO\0GO\0", 0);
    if (*offset != -1) {
        return FOUR_LETTER_UNCOMPRESSED;
    } else {
        *offset = FindCode("AUT\0GO\0", 0);
        if (*offset != -1) {
            return THREE_LETTER_UNCOMPRESSED;
        } else {
            *offset = FindCode("AUTO\0\0GO", 0);
            if (*offset != -1) {
                return FIVE_LETTER_UNCOMPRESSED;
            } else {
                *offset = FindCode("aUTOgO\0", 0);
                if (*offset != -1) {
                    return FOUR_LETTER_COMPRESSED;
                } else {
                    *offset = FindCode("\xc7" "EHENSTEIGE", 0);
                    if (*offset != -1) {
                        *offset -= 5;
                        return GERMAN;
                    } else {
                        *offset = FindCode("ANDAENTRAVAN", 0);
                        if (*offset !=  -1) {
                            *offset -= 8;
                            return SPANISH;
                        }
                    }
                }
            }
        }
    }
    return NOT_A_GAME;
}

void read_header(uint8_t *ptr) {
    int i,value;
    for (i = 0; i < 24; i++)
    {
        value = *ptr + 256 * *(ptr + 1);
        header[i] = value;
        ptr += 2;
    }
}

int sanity_check_header(void) {
    int16_t v = header[1]; // items
    if (v < 10 || v > 500)
        return 0;
    v = header[2]; // actions
    if (v < 100 || v > 500)
        return 0;
    v = header[3]; // word pairs
    if (v < 50 || v > 190)
        return 0;
    v = header[4]; // Number of rooms
    if (v < 10 || v > 100)
        return 0;

    return 1;
}

uint8_t *read_dictionary(struct GameInfo info, uint8_t **pointer)
{
    uint8_t *ptr = *pointer;
    char dictword[info.word_length + 2];
    char c = 0;
    int wordnum = 0;
    int charindex = 0;

    int nv = info.number_of_verbs;
    int nn = info.number_of_nouns;

    do {
        for (int i = 0; i < info.word_length; i++)
        {
            c = *(ptr++);

            if (info.dictionary == FOUR_LETTER_COMPRESSED) {
                if (charindex == 0) {
                    if (c >= 'a')
                    {
                        c = toupper(c);
                    } else {
                        dictword[charindex++] = '*';
                    }
                }
                dictword[charindex++] = c;
            } else if (info.subtype == LOCALIZED) {
                if (charindex == 0) {
                    if (c & 0x80)
                    {
                        c = c & 0x7f;
                    } else if (c != '.') {
                        dictword[charindex++] = '*';
                    }
                }
                dictword[charindex++] = c;
            } else {
                if (c == 0)
                {
                    if (charindex == 0)
                    {
                        c=*(ptr++);
                    }
                }
                dictword[charindex] = c;
                if (c == '*')
                    i--;
                charindex++;
            }
        }
        dictword[charindex] = 0;
        if (wordnum < nv)
        {
            Verbs[wordnum] = MemAlloc(charindex+1);
            memcpy((char *)Verbs[wordnum], dictword, charindex+1);
//            fprintf(stderr, "Verb %d: \"%s\"\n", wordnum, Verbs[wordnum]);
        } else {
            Nouns[wordnum - nv] = MemAlloc(charindex+1);
            memcpy((char *)Nouns[wordnum - nv], dictword, charindex+1);
//            fprintf(stderr, "Noun %d: \"%s\"\n", wordnum - nv, Nouns[wordnum - nv]);
        }
        wordnum++;

        if (c != 0 && !isascii(c))
            return ptr;
        
        charindex = 0;
    } while (wordnum <= nv + nn);

    for (int i = 0; i <= nn - nv; i++) {
        Verbs[nv + i] = ".\0";
    }

    for (int i = 0; i <= nv - nn; i++) {
        Nouns[nn + i] = ".\0";
    }

    return ptr;
}

uint8_t *seek_to_pos(uint8_t *buf, int offset) {
    if (offset > file_length)
        return 0;
    return buf + offset;
}

int seek_if_needed(int expected_start, int *offset, uint8_t **ptr) {
    if (expected_start != FOLLOWS) {
        *offset = expected_start + file_baseline_offset;
        uint8_t *ptrbefore = *ptr;
        *ptr = seek_to_pos(entire_file, *offset);
        if (*ptr == ptrbefore)
            fprintf(stderr, "Seek unnecessary, could have been set to FOLLOWS.\n");
        if (*ptr == 0)
            return 0;
    }
    return 1;
}

int try_loading_old(struct GameInfo info, int dict_start) {

    int ni,na,nw,nr,mc,pr,tr,wl,lt,mn,trm;
    int ct;

    Action *ap;
    Room *rp;
    Item *ip;
    /* Load the header */

    uint8_t *ptr = entire_file;

//    fprintf(stderr, "dict_start:%x\n", dict_start);
//    fprintf(stderr, " info.start_of_dictionary:%x\n",  info.start_of_dictionary);


    file_baseline_offset = dict_start - info.start_of_dictionary;

//    fprintf(stderr, "file_baseline_offset:%x\n", file_baseline_offset);

    int offset = info.start_of_header + file_baseline_offset;

//   offset = 0;
jumpHere:
    ptr = seek_to_pos(entire_file, offset);
    if (ptr == 0)
        return 0;

    read_header(ptr);

    while (sanity_check_header() == 0) {
        offset++;
        goto jumpHere;
    }


    if (sanity_check_header() == 0) {
        return 0;
    }

//    fprintf(stderr, "Found a valid header at position 0x%x\n", offset);

//    print_header_info(header);

    if (header[1] != info.number_of_items || header[2] != info.number_of_actions || header[3] != info.number_of_words || header[4] != info.number_of_rooms || header[5] != info.max_carried) {
//        fprintf(stderr, "Non-matching header\n");
        return 0;
    }

    ni = header[1];
    na = header[2];
    nw = header[3];
    nr = header[4];
    mc = header[5];
    pr = header[6];
//    fprintf(stderr, "Player start location: %d\n", pr);
    tr = header[7];
//    fprintf(stderr, "Treasure room: %d\n", tr);
    wl = header[8];
//    fprintf(stderr, "Word length: %d\n", wl);
    lt = header[9];
//    fprintf(stderr, "Lightsource time left: %d\n", lt);
    mn = header[10];
//    fprintf(stderr, "Number of messages: %d\n", mn);
    trm = header[11];
//    fprintf(stderr, "Number of treasures: %d\n", trm);


    GameHeader.NumItems=ni;
    Items=(Item *)MemAlloc(sizeof(Item)*(ni+1));
    GameHeader.NumActions=na;
    Actions=(Action *)MemAlloc(sizeof(Action)*(na+1));
    GameHeader.NumWords=nw;
    GameHeader.WordLength=wl;
    Verbs=MemAlloc(sizeof(char *)*(nw+2));
    Nouns=MemAlloc(sizeof(char *)*(nw+2));
    GameHeader.NumRooms=nr;
    Rooms=(Room *)MemAlloc(sizeof(Room)*(nr+1));
    GameHeader.MaxCarry=mc;
    GameHeader.PlayerRoom=pr;
    GameHeader.Treasures=tr;
    GameHeader.LightTime=lt;
    LightRefill=lt;
    GameHeader.NumMessages=mn;
    Messages=MemAlloc(sizeof(char *)*(mn+1));
    GameHeader.TreasureRoom=trm;

#pragma mark actions

    if (seek_if_needed(info.start_of_actions, &offset, &ptr) == 0)
        return 0;

//    offset = 0;
//jumpHereActions:
//    ptr = seek_to_pos(entire_file, offset);

    /* Load the actions */

    ct=0;

    ap=Actions;

    uint16_t value, cond, comm;

    while(ct<na+1)
    {
        value = *(ptr++);
        value += *(ptr++) * 0x100; /* verb/noun */
        ap->Vocab = value;


        cond = 5;
        comm = 2;

        for (int j = 0; j < 5; j++)
        {
            if (j < cond) {
                value = *(ptr++);
                value += *(ptr++) * 0x100;
            } else value = 0;
            ap->Condition[j] = value;
        }
        for (int j = 0; j < 2; j++)
        {
            if (j < comm) {
                value = *(ptr++) ;
                value += *(ptr++) * 0x100;
            } else value = 0;
            ap->Action[j] = value;
        }

//        if (ct == 0 && (ap->Vocab != 100 || ap->Condition[0]!= 29 || ap->Action[0] != 236 || ap->Action[1] != 358 )) {
//            offset++;
//            goto jumpHereActions;
//        }
        ap++;
        ct++;
    }

//    fprintf(stderr, "Found actions at offset %x\n", offset);
//    fprintf(stderr, "Offset after reading actions: %lx\n", ptr - entire_file);

#pragma mark room connections

    if (seek_if_needed(info.start_of_room_connections, &offset, &ptr) == 0)
        return 0;

//    offset = 0;
//jumpRoomConnections:
//    ptr = seek_to_pos(entire_file, offset);

    ct=0;
    rp=Rooms;

    while(ct<nr+1)
    {
        for (int j= 0; j < 6; j++) {
            rp->Exits[j] = *(ptr++);
        }

//        if ((ct == 2 && (rp->Exits[5] != 1 || rp->Exits[0] != 0)) || (ct == 3 && (rp->Exits[2] != 4 || rp->Exits[3] != 2)))
//        {
//            offset++; goto jumpRoomConnections;
//        }
        ct++;
        rp++;
    }

//    fprintf(stderr, "Found room connections at offset %x\n", offset);
//    fprintf(stderr, "Offset after reading room connections: %lx\n", ptr - entire_file);


#pragma mark item locations

    if (seek_if_needed(info.start_of_item_locations, &offset, &ptr) == 0)
        return 0;
    
    ct=0;
    ip=Items;
    while(ct<ni+1)
    {
        ip->Location=*(ptr++);
        ip->InitialLoc=ip->Location;
        ip++;
        ct++;
    }

//    fprintf(stderr, "Offset after reading item locations: %lx\n", ptr - entire_file);

#pragma mark dictionary

    if (seek_if_needed(info.start_of_dictionary, &offset, &ptr) == 0)
        return 0;

    ptr = read_dictionary(info, &ptr);
//    fprintf(stderr, "Offset after reading dictionary: %lx\n", ptr - entire_file);

#pragma mark rooms

    if (seek_if_needed(info.start_of_room_descriptions, &offset, &ptr) == 0)
        return 0;

    if (info.start_of_room_descriptions == FOLLOWS)
        ptr++;

    ct=0;
    rp=Rooms;

    char text[256];
    char c=0;
    int charindex = 0;

    do {
        c = *(ptr++);
        text[charindex] = c;
        if (c == 0) {
            rp->Text = MemAlloc(charindex + 1);
            strcpy(rp->Text, text);
//            fprintf(stderr, "Room %d: \"%s\"\n", ct, rp->Text);
            rp->Image = 255;
            ct++;
            rp++;
            charindex = 0;
        } else {
            charindex++;
        }
        if (c != 0 && !isascii(c))
            return 0;
    } while (ct<nr+1);

#pragma mark messages

    if (seek_if_needed(info.start_of_messages, &offset, &ptr) == 0)
        return 0;

    ct=0;
    charindex = 0;

    while(ct<mn+1)
    {
        c = *(ptr++);
        text[charindex] = c;
        if (c == 0) {
            Messages[ct] = MemAlloc(charindex + 1);
            strcpy((char *)Messages[ct], text);
//            fprintf(stderr, "Messages %d: \"%s\"\n", ct, Messages[ct]);
            ct++;
            charindex = 0;
        } else {
            charindex++;
        }
    }

#pragma mark items

    if (seek_if_needed(info.start_of_item_descriptions, &offset, &ptr) == 0)
        return 0;

    ct=0;
    ip=Items;
    charindex = 0;


    do {
        c = *(ptr++);
        text[charindex] = c;
        if (c == 0) {
            ip->Text = MemAlloc(charindex + 1);
            strcpy(ip->Text, text);
//            fprintf(stderr, "Item %d: \"%s\"\n", ct, ip->Text);
            ip->AutoGet=strchr(ip->Text,'/');
            /* Some games use // to mean no auto get/drop word! */
            if(ip->AutoGet && strcmp(ip->AutoGet,"//") && strcmp(ip->AutoGet,"/*"))
            {
                char *t;
                *ip->AutoGet++=0;
                t=strchr(ip->AutoGet,'/');
                if(t!=NULL)
                    *t=0;
//                fprintf(stderr, "Item %d autoget: \"%s\"\n", ct, ip->AutoGet);
            }
            ct++;
            ip++;
            charindex = 0;
        } else {
            charindex++;
        }
    } while(ct<ni+1);

//    fprintf(stderr, "Offset after reading item descriptions: %lx\n", ptr - entire_file);

#pragma mark System messages

    ct=0;
    if (seek_if_needed(info.start_of_system_messages, &offset, &ptr) == 0)
        return 0;

//    ptr = seek_to_pos(entire_file, 0x2539);

    charindex = 0;

    do {
        c=*(ptr++);
//        fprintf(stderr, "Character at offset %lx: \'%c\' (%d)\n", ptr - entire_file, c, c);
        text[charindex] = c;
        if (c == 0 || c == 0x0d) {
            if (charindex > 0) {
                if (c == 0x0d)
                    charindex++;
                system_messages[ct] = MemAlloc(charindex + 1);
                strncpy(system_messages[ct], text, charindex + 1);
                system_messages[ct][charindex] = '\0';
//                fprintf(stderr, "System message %d: \"%s\"\n", ct, system_messages[ct]);
                ct++;
                charindex = 0;
            }
        } else {
            charindex++;
        }

        if (c != 0 && c != 0x0d && c != '\x83' && !isascii(c))
            break;
    } while (ct<40);

    if (ct == 0)
        fprintf(stderr, "No system messages?\n");

//    fprintf(stderr, "Offset after reading system messages: %lx\n", ptr - entire_file);

    charindex = 0;

    if (seek_if_needed(info.start_of_directions, &offset, &ptr) == 0)
        return 0;

    ct = 0;
    do {
        c=*(ptr++);
        text[charindex] = c;
        if (c == 0 || c == 0x0d) {
            if (charindex > 0) {
                sys[ct] = MemAlloc(charindex + 2);
                strcpy(sys[ct], text);
                if (c == 0x0d)
                    charindex++;
                sys[ct][charindex] = '\0';
                ct++;
                charindex = 0;
            }
        } else {
            charindex++;
        }

        if (c != 0 && c != 0x0d && !isascii(c))
            break;
    } while (ct<6);

//    for (int i = 0; i < 6; i++)
//        fprintf(stderr, "Direction %d: \"%s\"\n", i, sys[i]);

    return 1;
}


int try_loading(struct GameInfo info, int dict_start) {

    /* The Hulk does everything differently */
    /* so it gets its own function */
    if (info.gameID == HULK)
        return try_loading_hulk(info, dict_start);

    if (info.type == TEXT_ONLY)
        return try_loading_old(info, dict_start);
    
    int ni,na,nw,nr,mc,pr,tr,wl,lt,mn,trm;
    int ct;

    Action *ap;
    Room *rp;
    Item *ip;
    /* Load the header */

    uint8_t *ptr = entire_file;

//    fprintf(stderr, "dict_start:%x\n", dict_start);
//    fprintf(stderr, " info.start_of_dictionary:%x\n",  info.start_of_dictionary);

    file_baseline_offset = dict_start - info.start_of_dictionary;

//    fprintf(stderr, "file_baseline_offset:%d\n", file_baseline_offset);

    int offset = info.start_of_header + file_baseline_offset;
    int flag = 0;
jumpHere:
    ptr = seek_to_pos(entire_file, offset);
    if (ptr == 0)
        return 0;

    read_header(ptr);

        while (sanity_check_header() == 0) {
            if (flag == 0) {
                offset = 0;
                flag = 1;
            }
            offset++;
            goto jumpHere;
        }

//    fprintf(stderr, "Found a header at %s position 0x%x\n", flag ? "unexpected" : "expected", offset);

    if (sanity_check_header() == 0) {
        return 0;
    }

//    print_header_info(header);
    
    if (header[1] != info.number_of_items || header[2] != info.number_of_actions || header[3] != info.number_of_words || header[4] != info.number_of_rooms || header[5] != info.max_carried) {
//        fprintf(stderr, "Non-matching header\n");
        return 0;
    }

//    fprintf(stderr, "Found a valid header at offset 0s%x\n", offset);


    ni = header[1];
    na = header[2];
    nw = header[3];
    nr = header[4];
    mc = header[5];
    if (info.header_style == LATE) {
    wl = header[6];
    mn = header[7];
    pr = 1;
    tr = 0;
    lt = -1;
    trm = 0;
    } else {
        pr = header[6];
//        fprintf(stderr, "Player start location: %d\n", pr);
        tr = header[7];
//        fprintf(stderr, "Treasure room: %d\n", tr);
        wl = header[8];
//        fprintf(stderr, "Word length: %d\n", wl);
        lt = header[9];
//        fprintf(stderr, "Lightsource time left: %d\n", lt);
        mn = header[10];
//        fprintf(stderr, "Number of messages: %d\n", mn);
        trm = header[11];
//        fprintf(stderr, "Number of treasures: %d\n", trm);
    }

    if (info.gameID == SAVAGE_ISLAND2)
        pr = 30;

    GameHeader.NumItems=ni;
    Items=(Item *)MemAlloc(sizeof(Item)*(ni+1));
    GameHeader.NumActions=na;
    Actions=(Action *)MemAlloc(sizeof(Action)*(na+1));
    GameHeader.NumWords=nw;
    GameHeader.WordLength=wl;
    Verbs=MemAlloc(sizeof(char *)*(nw+2));
    Nouns=MemAlloc(sizeof(char *)*(nw+2));
    GameHeader.NumRooms=nr;
    Rooms=(Room *)MemAlloc(sizeof(Room)*(nr+1));
    GameHeader.MaxCarry=mc;
    GameHeader.PlayerRoom=pr;
    GameHeader.Treasures=tr;
    GameHeader.LightTime=lt;
    LightRefill=lt;
    GameHeader.NumMessages=mn;
    Messages=MemAlloc(sizeof(char *)*(mn+1));
    GameHeader.TreasureRoom=trm;

    int compressed = (info.dictionary == FOUR_LETTER_COMPRESSED);

#pragma mark room images

    if (info.start_of_room_image_list != 0) {
        if (seek_if_needed(info.start_of_room_image_list, &offset, &ptr) == 0)
            return 0;

        /* Load the room images */
        rp=Rooms;

        for (ct = 0; ct <= GameHeader.NumRooms; ct++) {
            rp->Image = *(ptr++);
            rp++;
        }
    }

//    fprintf(stderr, "Offset after reading room images: %lx\n", ptr - entire_file);

#pragma mark Item flags

    if (info.start_of_item_flags != 0) {

        if (seek_if_needed(info.start_of_item_flags, &offset, &ptr) == 0)
            return 0;

        /* Load the item flags */

        ip=Items;

        for (ct = 0; ct <= GameHeader.NumItems; ct++) {
            ip->Flag = *(ptr++);
            ip++;
        }

    }

//    fprintf(stderr, "Offset after reading item flags: %lx\n", ptr - entire_file);

#pragma mark item images

    if (info.start_of_item_image_list != 0) {

        if (seek_if_needed(info.start_of_item_image_list, &offset, &ptr) == 0)
            return 0;

        //    offset = 0;
        //jumpItemImages:
        //
        //    ptr = seek_to_pos(entire_file, offset);

        /* Load the item images */

        ip=Items;

        for (ct = 0; ct <= GameHeader.NumItems; ct++) {
            ip->Image = *(ptr++);
            //        if (ip->Image != 255 && (ip->Image - GameHeader.NumRooms) > GameInfo->number_of_pictures) {
            //        if ((ct == 23 && ip->Image != 29) || (ct == 27 && ip->Image != 40)) {
            //
            //            offset++;
            //            goto jumpItemImages;
            //        }
            ip++;
        }

//        fprintf(stderr, "Found item images at %x\n", offset - file_baseline_offset);
    }

//    fprintf(stderr, "Offset after reading item images: %lx\n", ptr - entire_file);

#pragma mark actions

    if (seek_if_needed(info.start_of_actions, &offset, &ptr) == 0)
        return 0;

    /* Load the actions */

    ct=0;

    ap=Actions;

    uint16_t value, cond, comm;

    while(ct<na+1)
    {
        value = *(ptr++);
        value += *(ptr++) * 0x100; /* verb/noun */
        ap->Vocab = value;

        if (info.actions_style == COMPRESSED) {
            value = *(ptr++); /* count of actions/conditions */
            cond = value & 0x1f;
            if (cond > 5) {
                fprintf(stderr, "Condition error at action %d!\n", ct);
                cond = 5;
            }
            comm = (value & 0xe0) >> 5;
            if (comm > 2) {
                fprintf(stderr, "Command error at action %d!\n", ct);
                comm = 2;
            }
        } else {
            cond = 5;
            comm = 2;
        }
        for (int j = 0; j < 5; j++)
        {
            if (j < cond) {
                value = *(ptr++);
                value += *(ptr++) * 0x100;
            } else value = 0;
            ap->Condition[j] = value;
        }
        for (int j = 0; j < 2; j++)
        {
            if (j < comm) {
                value = *(ptr++) ;
                value += *(ptr++) * 0x100;
            } else value = 0;
            ap->Action[j] = value;
        }

        ap++;
        ct++;
    }

//    fprintf(stderr, "Offset after reading actions: %lx\n", ptr - entire_file);

#pragma mark dictionary

    if (seek_if_needed(info.start_of_dictionary, &offset, &ptr) == 0)
        return 0;

    ptr = read_dictionary(info, &ptr);

//    fprintf(stderr, "Offset after reading dictionary: %lx\n", ptr - entire_file);

#pragma mark rooms

    if (info.start_of_room_descriptions != 0) {
        if (seek_if_needed(info.start_of_room_descriptions, &offset, &ptr) == 0)
            return 0;

        ct=0;
        rp=Rooms;

        char text[256];
        char c=0;
        int charindex = 0;

        if (!compressed) {
            do {
                c = *(ptr++);
                text[charindex] = c;
                if (c == 0) {
                    rp->Text = MemAlloc(charindex + 1);
                    strcpy(rp->Text, text);
                    ct++;
                    rp++;
                    charindex = 0;
                } else {
                    charindex++;
                }
                if (c != 0 && !isascii(c))
                    return 0;
            } while (ct<nr+1);
        } else {
            do {
                rp->Text = decompress_text(ptr, ct);
                if (rp->Text == NULL)
                    return 0;
                *(rp->Text) = tolower(*(rp->Text));
                ct++;
                rp++;
            } while (ct<nr);
        }
    }

//    fprintf(stderr, "Offset after reading rooms: %lx\n", ptr - entire_file);

#pragma mark room connections

    if (seek_if_needed(info.start_of_room_connections, &offset, &ptr) == 0)
        return 0;

    ct=0;
    rp=Rooms;

    while(ct<nr+1)
    {
        for (int j= 0; j < 6; j++) {
            rp->Exits[j] = *(ptr++);
        }

        ct++;
        rp++;
    }

//    fprintf(stderr, "Offset after reading room connections: %lx\n", ptr - entire_file);

#pragma mark messages

    if (seek_if_needed(info.start_of_messages, &offset, &ptr) == 0)
        return 0;

    ct=0;
    char text[256];
    char c=0;
    int charindex = 0;

    if (compressed) {

        while(ct<mn+1)
        {
            Messages[ct] = decompress_text(ptr, ct);
            if (Messages[ct] == NULL)
                return 0;
            ct++;
        }

    } else {

        while(ct<mn+1)
        {
            c = *(ptr++);
            text[charindex] = c;
            if (c == 0) {
                Messages[ct] = MemAlloc(charindex + 1);
                strcpy((char *)Messages[ct], text);
//                fprintf(stderr, "Messages %d: \"%s\"\n", ct, Messages[ct]);
                ct++;
                charindex = 0;
            } else {
                charindex++;
            }
        }
    }

//    fprintf(stderr, "Offset after reading messages: %lx\n", ptr - entire_file);

#pragma mark items

    if (seek_if_needed(info.start_of_item_descriptions, &offset, &ptr) == 0)
        return 0;

    ct=0;
    ip=Items;
    charindex = 0;

    if (compressed) {
        do {
            ip->Text = decompress_text(ptr, ct);
            ip->AutoGet = NULL;
            if (ip->Text != NULL && ip->Text[0] != '.') {
                ip->AutoGet=strchr(ip->Text,'.');
                if (ip->AutoGet) {
                    char *t;
                    *ip->AutoGet++=0;
                    ip->AutoGet++;
                    t=strchr(ip->AutoGet,'.');
                    if(t!=NULL)
                        *t=0;
                    for (int i = 1; i < GameHeader.WordLength; i++)
                        *(ip->AutoGet+i) = toupper(*(ip->AutoGet+i));
//                    fprintf(stderr, "Item %d autoget: \"%s\"\n", ct, ip->AutoGet);
                }
            }
            ct++;
            ip++;
        } while(ct<ni+1);
    } else {
        do {

            c = *(ptr++);
            text[charindex] = c;
            if (c == 0) {
                ip->Text = MemAlloc(charindex + 1);
                strcpy(ip->Text, text);
                ip->AutoGet=strchr(ip->Text,'/');
                /* Some games use // to mean no auto get/drop word! */
                if(ip->AutoGet && strcmp(ip->AutoGet,"//") && strcmp(ip->AutoGet,"/*"))
                {
                    char *t;
                    *ip->AutoGet++=0;
                    t=strchr(ip->AutoGet,'/');
                    if(t!=NULL)
                        *t=0;
                }
                ct++;
                ip++;
                charindex = 0;
            } else {
                charindex++;
            }
        } while(ct<ni+1);
    }

//    fprintf(stderr, "Offset after reading item descriptions: %lx\n", ptr - entire_file);

#pragma mark item locations

    if (seek_if_needed(info.start_of_item_locations, &offset, &ptr) == 0)
        return 0;

    ct=0;
    ip=Items;
    while(ct<ni+1)
    {
        ip->Location=*(ptr++);
        ip->InitialLoc=ip->Location;
        ip++;
        ct++;
    }

//    fprintf(stderr, "Offset after reading item locations: %lx\n", ptr - entire_file);

#pragma mark System messages

    ct=0;
    if (seek_if_needed(info.start_of_system_messages, &offset, &ptr) == 0)
        return 0;

    charindex = 0;

    do {
        c=*(ptr++);
        text[charindex] = c;
        if (c == 0 || c == 0x0d) {
            if (charindex > 0) {
                if (c == 0x0d)
                    charindex++;
                system_messages[ct] = MemAlloc(charindex + 1);
                strncpy(system_messages[ct], text, charindex + 1);
                system_messages[ct][charindex] = '\0';
                ct++;
                charindex = 0;
            }
        } else {
            charindex++;
        }

        if (c != 0 && c != 0x0d && c != '\x83' && !isascii(c))
            break;
    } while (ct<40);

//    fprintf(stderr, "Offset after reading system messages: %lx\n", ptr - entire_file);

    charindex = 0;

    if (seek_if_needed(info.start_of_directions, &offset, &ptr) == 0)
        return 0;

    ct = 0;
    do {
        c=*(ptr++);
        text[charindex] = c;
        if (c == 0 || c == 0x0d) {
            if (charindex > 0) {
                sys[ct] = MemAlloc(charindex + 2);
                strcpy(sys[ct], text);
                if (c == 0x0d)
                    charindex++;
                sys[ct][charindex] = '\0';
                ct++;
                charindex = 0;
            }
        } else {
            charindex++;
        }

        if (c != 0 && c != 0x0d && !isascii(c))
            break;
    } while (ct<6);

    return 1;
}


GameIDType detect_game(FILE *f) {

    for (int i = 0; i < NUMBER_OF_DIRECTIONS; i++)
        Directions[i] = EnglishDirections[i];
    for (int i = 0; i < NUMBER_OF_SKIPPABLE_WORDS; i++)
        SkipList[i] = EnglishSkipList[i];
    for (int i = 0; i < NUMBER_OF_DELIMITERS; i++)
        DelimiterList[i] = EnglishDelimiterList[i];
    for (int i = 0; i < NUMBER_OF_EXTRA_NOUNS; i++)
        ExtraNouns[i] = EnglishExtraNouns[i];

    fprintf(stderr, "Attempting to read ScottFree text format.\n");

    // For now, we just: check if the standard ScottFree LoadDatabase() function can read it.
    // if not, we assume it is a ZX Spectrum format uncompressed memory or cassette dump of some kind.
    if (LoadDatabase(f, 0)) {
        fclose(f);
        GameInfo = MemAlloc(sizeof(GameInfo));
        GameInfo->gameID = SCOTTFREE;
        return SCOTTFREE;
    }

    file_length = GetFileLength(f);
    fprintf(stderr, "Length of file: %zu\n", file_length);

    if (file_length > MAX_GAMEFILE_SIZE)  {
        fprintf(stderr, "File too large to be a vaild game file (%zu, max is %d)\n", file_length, MAX_GAMEFILE_SIZE);
        return 0;
    }

    entire_file = MemAlloc(file_length);
    fseek(f, 0, SEEK_SET);
    file_length = fread(entire_file, 1, file_length, f);

    fprintf(stderr, "Read %zu bytes from file\n", file_length);

    uint8_t *uncompressed = decompress_z80(entire_file, file_length);
    if (uncompressed != NULL) {
        free(entire_file);
        entire_file = uncompressed;
        file_length = 0xc000;
    }

    size_t offset;

    dictionary_type dict_type = getId(&offset);

    if (dict_type == NOT_A_GAME)
        return 0;

    for (int i = 0; i < NUMGAMES; i++) {
        if (games[i].dictionary == dict_type) {
//            fprintf(stderr, "The game might be %s\n", games[i].Title);
            if (try_loading(games[i], offset)) {
                GameInfo = &games[i];
                break;
            }
//            else
//                fprintf(stderr, "It was not.\n");
        }
    }

    if (GameInfo == NULL)
        return 0;

    for (int i = 6; i < MAX_SYSMESS && sysdict_zx[i] != NULL; i++) {
        sys[i] = (char *)sysdict_zx[i];
    }

    switch (CurrentGame) {
        case ROBIN_OF_SHERWOOD:
            LoadExtraSherwoodData();
            break;
        case SEAS_OF_BLOOD:
            LoadExtraSeasOfBloodData();
            break;
        case CLAYMORGUE:
            for (int i = OK; i <= RESUME_A_SAVED_GAME; i++)
                sys[i] = system_messages[6 - OK + i];
            for (int i = PLAY_AGAIN; i <= ON_A_SCALE_THAT_RATES; i++)
                sys[i] = system_messages[2 - PLAY_AGAIN + i];
            break;
        case SECRET_MISSION:
            Items[3].Image = 23;
            Items[3].Flag = 128 | 2;
            Rooms[2].Image = 0;
        case ADVENTURELAND:
            for (int i = PLAY_AGAIN; i <= ON_A_SCALE_THAT_RATES; i++)
                sys[i] = system_messages[2 - PLAY_AGAIN + i];
            for (int i = OK; i <= YOU_HAVENT_GOT_IT; i++)
                sys[i] = system_messages[6 - OK + i];
            for (int i = YOU_DONT_SEE_IT; i <= RESUME_A_SAVED_GAME; i++)
                sys[i] = system_messages[13 - YOU_DONT_SEE_IT + i];
            break;
        case GREMLINS_GERMAN:
            LoadExtraGermanGremlinsData();
        case GREMLINS:
        case SAVAGE_ISLAND:
            Items[20].Image = 13;
        case SAVAGE_ISLAND2:
        case SUPERGRAN:
            for (int i = DROPPED; i <= OK; i++)
                sys[i] = system_messages[2 - DROPPED + i];
            for (int i = I_DONT_UNDERSTAND; i <= THATS_BEYOND_MY_POWER; i++)
                sys[i] = system_messages[6 - I_DONT_UNDERSTAND + i];
            for (int i = YOU_ARE; i <= HIT_ENTER; i++)
                sys[i] = system_messages[17 - YOU_ARE + i];
            if (CurrentGame == GREMLINS_GERMAN)
                sys[YOU_ARE] = "Ich bin ";
            sys[PLAY_AGAIN] = system_messages[5];
            sys[YOURE_CARRYING_TOO_MUCH] = system_messages[24];
            sys[IM_DEAD] = system_messages[25];
            sys[YOU_CANT_GO_THAT_WAY] = system_messages[14];
            break;
        case GREMLINS_SPANISH:
            LoadExtraSpanishGremlinsData();
            break;
        case HULK:
            for (int i = 0; i < 6; i++) {
                sys[i] = (char *)sysdict_zx[i];
            }
            break;
        default:
            if (GameInfo->subtype == MYSTERIOUS) {
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
            break;
    }

//    for (int i = PLAY_AGAIN; i <= RESUME_A_SAVED_GAME; i++)
//        fprintf(stderr, "\"%s\":\"%s\"\n", sysdict_zx[i], sys[i]);

    if (GameInfo->number_of_pictures > 0)
        saga_setup();

    return CurrentGame;
}
