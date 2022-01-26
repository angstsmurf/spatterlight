//
//  hulk.c
//  scott
//
//  Created by Administrator on 2022-01-18.
//
#include <ctype.h>
#include <string.h>

#include "scott.h"
#include "hulk.h"
#include "detectgame.h"

extern void read_header(uint8_t *ptr);
extern void print_header_info(int header[]);
extern int header[];

void hulk_show_image_on_examine(int noun) {
    int image = 0;
    switch (noun) {
        case 55:
            if (Items[11].Location == MyLoc )
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

void hulk_look(void) {
    DrawImage(Rooms[MyLoc].Image);
    for (int ct = 0; ct <= GameHeader.NumItems; ct++) {
        int image = Items[ct].Image;
        if (Items[ct].Location == MyLoc && image != 255) {
            /* Don't draw bio gem in fuzzy area */
            if(ct == 18 && MyLoc != 15)
                continue;
            DrawImage(image);
        }
    }
}
void draw_hulk_image(int p) {
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

uint8_t *read_hulk_dictionary(struct GameInfo info, uint8_t **pointer)
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

            dictword[charindex] = 0;
        }

        if (wordnum < nn)
        {
            Nouns[wordnum] = MemAlloc(charindex+1);
            memcpy((char *)Nouns[wordnum], dictword, charindex+1);
//            fprintf(stderr, "Nouns %d: \"%s\"\n", wordnum, Nouns[wordnum]);
        } else {
            Verbs[wordnum - nn] = MemAlloc(charindex+1);
            memcpy((char *)Verbs[wordnum - nn], dictword, charindex+1);
//            fprintf(stderr, "Verbs %d: \"%s\"\n", wordnum - nn, Verbs[wordnum - nn]);
        }
        wordnum++;

        if (c != 0 && !isascii(c))
            return ptr;

        charindex = 0;
    } while (wordnum <= nv + nn);

    for (int i = 0; i < nn - nv; i++)
        Verbs[nv + i] = ".\0";

    for (int i = 0; i < nv - nn; i++)
        Nouns[nn + i] = ".\0";

    return ptr;
}

int try_loading_hulk(struct GameInfo info, int dict_start) {
    int ni,na,nw,nr,mc,pr,tr,wl,lt,mn,trm;
    int ct;

    Action *ap;
    Room *rp;
    Item *ip;
    /* Load the header */

    uint8_t *ptr = entire_file;

    file_baseline_offset = dict_start - info.start_of_dictionary - 645;
//    fprintf(stderr, "HULK: file baseline offset:%d\n", file_baseline_offset);

    int offset = info.start_of_header + file_baseline_offset;
    ptr = seek_to_pos(entire_file, offset);

    if (ptr == 0)
        return 0;

    read_header(ptr);
//    print_header_info(header);

    if (header[0] != info.word_length || header[1] != info.number_of_words || header[2] != info.number_of_actions || header[3] != info.number_of_items || header[4] != info.number_of_messages || header[5] != info.number_of_rooms || header[6] != info.max_carried) {
        fprintf(stderr, "Non-matching header\n");
        return 0;
    }
    
    ni = 54;
    na = 261;
    nw = 128;
    nr = 20;
    mc = 10;
    pr = 1;
    tr = 17;
    wl = 4;
    lt = 150;
    mn = 99;
    trm = 16;

    GameHeader.NumItems=ni;
    Items=(Item *)MemAlloc(sizeof(Item)*(ni+1));
    GameHeader.NumActions=na;
    Actions=(Action *)MemAlloc(sizeof(Action)*(na+1));
    GameHeader.NumWords=nw;
    GameHeader.WordLength=wl;
    Verbs=MemAlloc(sizeof(char *)*(nw+1));
    Nouns=MemAlloc(sizeof(char *)*(nw+1));
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


#pragma mark Dictionary

    if (seek_if_needed(info.start_of_dictionary, &offset, &ptr) == 0)
        return 0;

    read_hulk_dictionary(info, &ptr);

#pragma mark Rooms

    //    fprintf(stderr, "Position after reading dictionary: %ld\n", ptr - entire_file);

    if (seek_if_needed(info.start_of_room_descriptions, &offset, &ptr) == 0)
        return 0;

    ct=0;
    rp=Rooms;

    uint8_t string_length = 0;
    do {
        string_length  = *(ptr++);
        if (string_length == 0) {
            rp->Text = ".\0";
        } else {
            rp->Text = MemAlloc(string_length + 1);
            for (int i = 0; i < string_length; i++) {
                rp->Text[i] = *(ptr++);
            }
            rp->Text[string_length] = 0;

        }
        rp++;
        ct++;
    } while (ct<nr+1);


#pragma mark Messages

    ct=0;

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
        //        fprintf(stderr, "Message %d: \"%s\"\n", ct, Messages[ct]);
        ct++;
    } while (ct<mn+1);

#pragma mark Items

    if (seek_if_needed(info.start_of_item_descriptions, &offset, &ptr) == 0)
        return 0;

    ct=0;
    ip=Items;

    do {
        string_length  = *(ptr++);
        if (string_length == 0) {
            ip->Text = ".\0";
        } else {
            ip->Text = MemAlloc(string_length + 1);

            for (int i = 0; i < string_length; i++) {
                ip->Text[i] = *(ptr++);
            }
            ip->Text[string_length] = 0;

            ip->AutoGet=strchr(ip->Text,'/');
            /* Some games use // to mean no auto get/drop word! */
            if(ip->AutoGet && strcmp(ip->AutoGet,"//") && strcmp(ip->AutoGet,"/*"))
            {
                char *t;
                *ip->AutoGet++=0;
                t=strchr(ip->AutoGet,'/');
                if(t!=NULL)
                    *t=0;
                ptr++;
            }
        }

        ct++;
        ip++;
    } while(ct<ni+1);

#pragma mark Room connections

    if (seek_if_needed(info.start_of_room_connections, &offset, &ptr) == 0)
        return 0;

    /* The room connections are ordered by direction, not room, so all the North connections for all the rooms come first, then the South connections, and so on. */
    for (int j = 0; j < 6; j++) {
        ct=0;
        rp=Rooms;

        while(ct<nr+1) {
            rp->Exits[j] = *(ptr++);
            ptr++;
            ct++;
            rp++;
        }
    }

#pragma mark item locations

    if (seek_if_needed(info.start_of_item_locations, &offset, &ptr) == 0)
        return 0;

    ct=0;
    ip=Items;
    while(ct<ni+1) {
        ip->Location=*(ptr++);
        ip->Location += *(ptr++) * 0x100;
        ip->InitialLoc=ip->Location;
        ip++;
        ct++;
    }

#pragma mark room images

    if (seek_if_needed(info.start_of_room_image_list, &offset, &ptr) == 0)
        return 0;

    rp=Rooms;

    for (ct = 0; ct <= GameHeader.NumRooms; ct++) {
        rp->Image = *(ptr++);
        rp++;
    }

#pragma mark item images

    if (seek_if_needed(info.start_of_item_image_list, &offset, &ptr) == 0)
        return 0;

    ip=Items;

    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        ip->Image = 255;
        ip++;
    }

    int index, image = 10;

    do {
        index = (*ptr++);
        Items[index].Image = image++;
    } while (index != 255);

#pragma mark item flags

    /* Hulk does not seem to use item flags */

#pragma mark Actions

    if (seek_if_needed(info.start_of_actions, &offset, &ptr) == 0)
        return 0;

    ct=0;
    ap=Actions;

    int verb, noun, value, value2, plus;
    while(ct<na+1)
    {
        plus = na + 1;
        verb = entire_file[offset + ct];
        noun = entire_file[offset + ct + plus];

        ap->Vocab = verb * 150 + noun;

        for (int j = 0; j < 2; j++)
        {
            plus += na + 1;
            value = entire_file[offset + ct + plus];
            plus += na + 1;
            value2 = entire_file[offset + ct + plus];
            ap->Action[j] = 150 * value + value2;
        }

        int offset2 = offset + 0x624;
        plus = 0;

        for (int j = 0; j < 5; j++)
        {
            value = entire_file[offset2 + ct * 2 + plus];
            value2 = entire_file[offset2 + ct * 2 + plus + 1];
            ap->Condition[j] = value + value2 * 0x100;
            plus += (na + 1) * 2;
        }

        ap++;
        ct++;
    }

    return 1;
}
