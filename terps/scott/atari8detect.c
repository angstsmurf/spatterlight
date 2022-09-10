//
//  atari8detect.c
//  Plus
//
//  Created by Administrator on 2022-01-30.
//
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "scott.h"
#include "graphics.h"
#include "definitions.h"
#include "hulk.h"

#include "gameinfo.h"
#include "atari8c64draw.h"
#include "atari8detect.h"


typedef struct imglist {
    USImageType usage;
    int index;
    size_t offset;
} imglist;


static const struct imglist listHulk[] = {
    { IMG_ROOM, 0, 0x0297 },
    { IMG_ROOM, 1, 0x2479 },
    { IMG_ROOM, 2, 0x071e },
    { IMG_ROOM, 3, 0x2a6b },
    { IMG_ROOM, 4, 0x0d17 },
    { IMG_ROOM, 9, 0x2ef2 },
    { IMG_ROOM, 12, 0x1325 },
    { IMG_ROOM, 15, 0x381e },
    { IMG_ROOM, 16, 0x4064 },
    { IMG_ROOM, 19, 0x4bac },
    { IMG_ROOM, 20, 0x5256 },
    { IMG_ROOM, 81, 0x5ee4 },
    { IMG_ROOM, 82, 0x695d },
    { IMG_ROOM, 83, 0x7207 },
    { IMG_ROOM, 84, 0x8deb },
    { IMG_ROOM, 85, 0x7a8e },
    { IMG_ROOM, 86, 0x9deb },
    { IMG_ROOM, 87, 0x8717 },
    { IMG_ROOM, 88, 0xac90 },
    { IMG_ROOM, 89, 0xba2c },
    { IMG_ROOM, 90, 0xcb00 },
    { IMG_ROOM, 91, 0xd497 },
    { IMG_ROOM, 92, 0xdea5 },
    { IMG_ROOM, 93, 0x10479 },
    { IMG_ROOM, 94, 0xe7c1 },
    { IMG_ROOM, 95, 0x10df2 },
    { IMG_ROOM, 96, 0xf5a5 },
    { IMG_ROOM, 97, 0x1146b },
    { IMG_ROOM, 99, 0x11a33 },
    { IMG_ROOM_OBJ, 13, 0x12700 },
    { IMG_ROOM_OBJ, 20, 0x127ba },
    { IMG_ROOM_OBJ, 24, 0x12e0e },
    { IMG_ROOM_OBJ, 53, 0x12ea5 },
    { IMG_ROOM_OBJ, 26, 0x12f0e },
    { IMG_ROOM_OBJ, 17, 0x13b8e },
    { IMG_ROOM_OBJ, 22, 0x13d17 },
    { IMG_ROOM_OBJ, 25, 0x13fdd },
    { IMG_ROOM_OBJ, 33, 0x14100 },
    { IMG_ROOM_OBJ, 36, 0x141a5 },
    { IMG_ROOM_OBJ, 34, 0x1420e },
    { IMG_ROOM_OBJ, 39, 0x1440e },
    { IMG_ROOM_OBJ, 40, 0x145c1 },
    { IMG_ROOM_OBJ, 45, 0x1469e },
    { IMG_ROOM_OBJ, 70, 0x1508e },
    { IMG_ROOM_OBJ, 72, 0x15307 },
    { IMG_ROOM_OBJ, 47, 0x15fba },
    { IMG_ROOM_OBJ, 250, 0x15f3a },

    { 0, 0, 0, }
};

// "R0198", 0, Disk image A at offset 0x9890

static const struct imglist listClaymorgue[] = {
    { IMG_INV_OBJ, 5, 0x0297 },
    { IMG_INV_OBJ, 6, 0x02c1 },
    { IMG_INV_OBJ, 7, 0x02e4 },
    { IMG_INV_OBJ, 8, 0x0307 },
    { IMG_INV_OBJ, 9, 0x032c },
    { IMG_INV_OBJ, 10, 0x03b3 },
    { IMG_INV_OBJ, 11, 0x03d6 },
    { IMG_INV_OBJ, 12, 0x0400 },
    { IMG_INV_OBJ, 13, 0x042c },
    { IMG_INV_OBJ, 14, 0x0456 },
    { IMG_INV_OBJ, 15, 0x0479 },
    { IMG_INV_OBJ, 16, 0x049e },
    { IMG_INV_OBJ, 17, 0x04c1 },
    { IMG_INV_OBJ, 20, 0x04eb },
    { IMG_INV_OBJ, 24, 0x052c },
    { IMG_INV_OBJ, 33, 0x0580 },
    { IMG_INV_OBJ, 34, 0x05cf },
    { IMG_INV_OBJ, 35, 0x061e },
    { IMG_INV_OBJ, 36, 0x066b },
    { IMG_INV_OBJ, 40, 0x06b3 },
    { IMG_INV_OBJ, 41, 0x0707 },
    { IMG_INV_OBJ, 42, 0x074f },
    { IMG_INV_OBJ, 45, 0x07ac },
    { IMG_INV_OBJ, 48, 0x0800 },
    { IMG_INV_OBJ, 50, 0x0864 },
    { IMG_INV_OBJ, 57, 0x08c1 },
    { IMG_INV_OBJ, 59, 0x0910 },
    { IMG_INV_OBJ, 61, 0x093a },
    { IMG_INV_OBJ, 62, 0x098e },
    { IMG_INV_OBJ, 63, 0x09f2 },
    { IMG_INV_OBJ, 64, 0x0a33 },
    { IMG_INV_OBJ, 68, 0x0a87 },
    { IMG_INV_OBJ, 70, 0x0ac8 },
    { IMG_INV_OBJ, 71, 0x0b17 },
    { IMG_INV_OBJ, 73, 0x0b56 },
    { IMG_INV_OBJ, 28, 0x0ba5 },
    { IMG_INV_OBJ, 51, 0x0bf2 },
    { IMG_INV_OBJ, 52, 0x0c97 },
    { IMG_INV_OBJ, 65, 0x0e80 },
    { IMG_INV_OBJ, 66, 0x0ee4 },
    { IMG_INV_OBJ, 69, 0x0f33 },
    { IMG_ROOM_OBJ, 26, 0x0f87 },
    { IMG_ROOM_OBJ, 938, 0x12b3 },
    { IMG_ROOM_OBJ, 30, 0x1e17 },
    { IMG_ROOM_OBJ, 32, 0x1fba },
    { IMG_ROOM_OBJ, 75, 0x211e },
    { IMG_ROOM_OBJ, 39, 0x232c },
    { IMG_ROOM_OBJ, 54, 0x28eb },
    { IMG_ROOM_OBJ, 55, 0x2bc1 },
    { IMG_ROOM_OBJ, 4, 0x2f0e },
    { IMG_ROOM_OBJ, 60, 0x3000 },
    { IMG_ROOM, 29, 0x3641 },
    { IMG_ROOM, 99, 0x4797 },
    { IMG_INV_OBJ, 4, 0x15356 },
    { IMG_INV_OBJ, 12, 0x15397 },
    { IMG_INV_OBJ, 19, 0x153dd },
    { IMG_INV_OBJ, 29, 0x1541e },
    { IMG_INV_OBJ, 30, 0x15464 },
    { IMG_INV_OBJ, 31, 0x154a5 },
    { IMG_INV_OBJ, 32, 0x154eb },
    { IMG_INV_OBJ, 35, 0x1552c },
    { IMG_INV_OBJ, 37, 0x15572 },
    { IMG_INV_OBJ, 42, 0x155b3 },
    { IMG_INV_OBJ, 44, 0x155f9 },
    { IMG_INV_OBJ, 46, 0x1563a },
    { IMG_INV_OBJ, 48, 0x15680 },
    { IMG_INV_OBJ, 50, 0x156c1 },
    { IMG_INV_OBJ, 51, 0x15707 },
    { IMG_INV_OBJ, 2, 0x15748 },
    { IMG_INV_OBJ, 21, 0x157ba },
    { IMG_INV_OBJ, 1, 0x15807 },
    { IMG_INV_OBJ, 18, 0x15cc8 },
    { IMG_INV_OBJ, 3, 0x15da5 },
    { IMG_INV_OBJ, 23, 0x15ea5 },
    { IMG_INV_OBJ, 16, 0x15f79 },
    { IMG_ROOM_OBJ, 49, 0x51c8 },
    { IMG_ROOM_OBJ, 47, 0x549e },
    { IMG_ROOM_OBJ, 250, 0x5580 },
    { IMG_INV_OBJ, 0, 0x55c1 },
    { IMG_ROOM_OBJ, 74, 0x55eb },
    { IMG_ROOM, 0, 0x5617 },
    { IMG_ROOM, 1, 0x57ba },
    { IMG_ROOM, 2, 0x628e },
    { IMG_ROOM, 3, 0x68cf },
    { IMG_ROOM, 4, 0x7864 },
    { IMG_ROOM, 5, 0x8297 },
    { IMG_ROOM, 6, 0x8c33 },
    { IMG_ROOM, 7, 0x95dd },
    { IMG_ROOM, 8, 0x9cb3 },
    { IMG_ROOM, 9, 0xa61e },
    { IMG_ROOM, 10, 0xa95d },
    { IMG_ROOM, 11, 0xad72 },
    { IMG_ROOM, 12, 0xb4ac },
    { IMG_ROOM, 13, 0xbd33 },
    { IMG_ROOM, 14, 0xc39e }, //0xb78e
    { IMG_ROOM, 15, 0xc8d6 },
    { IMG_ROOM, 16, 0xcf4f },
    { IMG_ROOM, 17, 0xdbf2 }, //0xDBEC
    { IMG_ROOM, 18, 0xdc48 },
    { IMG_ROOM, 19, 0xeaba },
    { IMG_ROOM, 20, 0xfd17 },
    { IMG_ROOM, 21, 0x1029e },
    { IMG_ROOM, 22, 0x10aa5 },
    { IMG_ROOM, 23, 0x10f3a },
    { IMG_ROOM, 24, 0x116ac },
    { IMG_ROOM, 25, 0x1228e },
    { IMG_ROOM, 26, 0x1260e },
    { IMG_ROOM, 27, 0x135f2 },
    { IMG_ROOM, 28, 0x14048 },
    { IMG_ROOM, 30, 0x14c97 },
    { IMG_ROOM, 31, 0x158b3 },
    { IMG_ROOM, 32, 0x15ccf },
    {0,0,0}
};

static const struct imglist listCount[] = {
    { IMG_INV_OBJ, 0, 0x0297 },
    { IMG_INV_OBJ, 16, 0x0310 }, // 1
    { IMG_INV_OBJ, 19, 0x036b }, // 2
    { IMG_INV_OBJ, 19, 0x03e4 }, // 3
    { IMG_INV_OBJ, 45, 0x0425 }, // 4
    { IMG_INV_OBJ, 26, 0x049e }, // 5
    { IMG_INV_OBJ, 47, 0x04c8 }, // 6
    { IMG_INV_OBJ, 3, 0x04f9 }, // 7
    { IMG_INV_OBJ, 17, 0x058e }, // 8
    { IMG_INV_OBJ, 27, 0x05dd }, // 9
    { IMG_INV_OBJ, 31, 0x0656 }, // 10
    { IMG_INV_OBJ, 5, 0x0697 }, // 11
    { IMG_INV_OBJ, 12, 0x06f2 },
    { IMG_INV_OBJ, 13, 0x073a },
    { IMG_INV_OBJ, 14, 0x07a5 },
    { IMG_INV_OBJ, 15, 0x07eb },
    { IMG_INV_OBJ, 21, 0x0848 },
    { IMG_INV_OBJ, 20, 0x08a5 }, // 17
    { IMG_INV_OBJ, 18, 0x0900 },
    { IMG_INV_OBJ, 19, 0x0997 },
    { IMG_INV_OBJ, 20, 0x0a4f },
    { IMG_INV_OBJ, 21, 0x0add },
    { IMG_INV_OBJ, 37, 0x0b56 }, // 22
    { IMG_INV_OBJ, 23, 0x0bc8 },
    { IMG_INV_OBJ, 24, 0x0c2c },
    { IMG_INV_OBJ, 25, 0x0ceb },
    { IMG_INV_OBJ, 26, 0x0d72 },
    { IMG_INV_OBJ, 27, 0x0df9 },
    { IMG_INV_OBJ, 28, 0x0e90 },
    { IMG_INV_OBJ, 29, 0x0f2c }, // 29
    { IMG_INV_OBJ, 35, 0x0fac }, // 30
    { IMG_ROOM_OBJ, 6, 0x10c8 }, // 31
    { IMG_ROOM_OBJ, 31, 0x116b }, // 32
    { IMG_ROOM_OBJ, 33, 0x12ba },
    { IMG_ROOM_OBJ, 5, 0x13f9 }, // 34
    { IMG_ROOM_OBJ, 37, 0x168e }, // 35
    { IMG_ROOM_OBJ, 52, 0x16b3 }, // 36
    { IMG_ROOM_OBJ, 55, 0x1b48 },
    { IMG_ROOM, 38, 0x1ccf },
    { IMG_ROOM_OBJ, 63, 0x261e }, // 39
    { IMG_ROOM_OBJ, 67, 0x2733 }, // 40
    { IMG_ROOM_OBJ, 17, 0x2cdd }, // 41
    { IMG_ROOM_OBJ, 11, 0x2ecf },
    { IMG_ROOM, 43, 0x3000 },
    { IMG_ROOM_OBJ, 38, 0x302c },
    { IMG_ROOM_OBJ, 45, 0x334f },
    { IMG_ROOM_OBJ, 46, 0x3580 },
    { IMG_ROOM_OBJ, 1, 0x4072 }, // 47
    { IMG_ROOM_OBJ, 36, 0x408e }, // 48
    { IMG_ROOM, 2, 0x413a },
    { IMG_ROOM, 0, 0x4a25 },
    { IMG_ROOM, 1, 0x5580 },
    { IMG_ROOM, 3, 0x5e48 },
    { IMG_ROOM, 4, 0x6a17 },
    { IMG_ROOM, 5, 0x74c1 },
    { IMG_ROOM, 55, 0x7cba },
    { IMG_ROOM, 56, 0x8f41 },
    { IMG_ROOM, 8, 0x984f },
    { IMG_ROOM, 9, 0xa10e },
    { IMG_ROOM, 59, 0xad79 },
    { IMG_ROOM, 11, 0xb687 },
    { IMG_ROOM, 12, 0xc0d6 },
    { IMG_ROOM, 13, 0xcc56 },
    { IMG_ROOM, 14, 0xdf5d },
    { IMG_ROOM, 15, 0xe872 },
    { IMG_ROOM, 16, 0xf72c },
    { IMG_ROOM, 17, 0x10441 },
    { IMG_ROOM, 18, 0x1159e },
    { IMG_ROOM, 19, 0x12872 },
    { IMG_ROOM, 20, 0x12f5d },
    { IMG_ROOM, 21, 0x136b3 },
    { IMG_ROOM, 22, 0x1400e },
    { IMG_ROOM, 72, 0x14f97 },
    { IMG_ROOM, 73, 0x16156 },
    { IMG_ROOM, 74, 0x1641a },
    { 0, 0, 0 }
};
//
////{ "S000", 0, Disk image A at offset 6d97
////    0x0e94 },
//
static const struct imglist listVoodoo[] = {
    { IMG_ROOM, 0, 0x0297 },
{ IMG_ROOM, 1, 0x0ac1 },
{ IMG_ROOM, 2, 0x1a79 },
{ IMG_ROOM, 3, 0x1e2c },
{ IMG_ROOM, 4, 0x28a5 },
{ IMG_ROOM, 5, 0x3817 },
{ IMG_ROOM, 6, 0x3f87 },
{ IMG_ROOM, 7, 0x4664 },
{ IMG_ROOM, 8, 0x4f0e },
{ IMG_ROOM, 9, 0x52ac },
{ IMG_ROOM, 10, 0x5b48 },
{ IMG_ROOM, 11, 0x6264 },
{ IMG_ROOM, 12, 0x67dd },
{ IMG_ROOM, 13, 0x740e },
{ IMG_ROOM, 14, 0x7d4f },
{ IMG_ROOM, 15, 0x8af2 },
{ IMG_ROOM, 16, 0x954f },
{ IMG_ROOM, 17, 0xa19e },
{ IMG_ROOM, 18, 0xab17 },
{ IMG_ROOM, 19, 0xbe1e },
{ IMG_ROOM, 20, 0xc579 },
{ IMG_ROOM, 21, 0xcf9e },
{ IMG_ROOM, 22, 0xd533 },
{ IMG_ROOM, 23, 0xe5e4 },
{ IMG_ROOM, 24, 0xeb3a },
{ IMG_ROOM, 25, 0xf856 },
{ IMG_ROOM, 26, 0xfd87 },
{ IMG_ROOM, 27, 0x1068e },
{ IMG_ROOM, 28, 0x114e4 },
{ IMG_ROOM, 29, 0x11956 },
{ IMG_ROOM, 30, 0x12879 },
{ IMG_ROOM, 31, 0x131cf },
{ IMG_ROOM, 32, 0x13325 },
{ IMG_ROOM, 33, 0x13407 },
{ IMG_ROOM, 34, 0x134ba },
{ IMG_ROOM, 35, 0x1355d },
{ IMG_ROOM, 36, 0x1359e },
{ IMG_ROOM, 37, 0x1360e },
{ IMG_ROOM, 38, 0x136ba },
{ IMG_ROOM, 39, 0x13733 },
{ IMG_ROOM, 40, 0x1378e },
{ IMG_ROOM, 41, 0x13817 },
{ IMG_ROOM, 42, 0x13879 },
{ IMG_ROOM, 43, 0x138dd },
{ IMG_ROOM, 44, 0x1392c },
{ IMG_ROOM, 45, 0x13972 },
{ IMG_ROOM, 46, 0x139c1 },
{ IMG_ROOM, 47, 0x13a2c },
{ IMG_ROOM, 48, 0x13aa5 },
{ IMG_ROOM, 49, 0x13b0e },
{ IMG_ROOM, 50, 0x13b64 },
{ IMG_ROOM, 51, 0x13c56 },
{ IMG_ROOM, 52, 0x13cac },
{ IMG_ROOM, 53, 0x13d6b },
{ IMG_ROOM, 54, 0x13db3 },
{ IMG_ROOM, 55, 0x13e97 },
{ IMG_ROOM, 56, 0x13f72 },
{ IMG_ROOM, 57, 0x13ff9 },
{ IMG_ROOM, 58, 0x1405d },
{ IMG_ROOM, 59, 0x140d6 },
{ IMG_ROOM, 60, 0x1415d },
{ IMG_ROOM, 61, 0x14217 },
{ IMG_ROOM, 62, 0x14279 },
{ IMG_ROOM, 63, 0x14333 },
{ IMG_ROOM, 64, 0x144c1 },
{ IMG_ROOM, 65, 0x147f2 },
{ IMG_ROOM, 66, 0x14e1e },
{ IMG_ROOM, 67, 0x15710 },
{ IMG_ROOM, 68, 0x158c8 },
{ IMG_ROOM, 69, 0x15972 },
{ IMG_ROOM, 70, 0x15cf9 },
{ IMG_ROOM, 71, 0x15eb3 },
{ IMG_ROOM, 72, 0x16025 },
{ IMG_ROOM, 73, 0x1625d },
{ IMG_ROOM, 74, 0x16348 },
    { 0, 0, 0, }
};

typedef enum {
    TYPE_B,
    TYPE_TWO,
    TYPE_2
} CompanionNameType;

static int StripBrackets(char **string, size_t length) {
    char *sideB = *string;
    int left_bracket = 0;
    int right_bracket = 0;
    if (length > 4) {
        for (int i = length - 4; i > 0; i--) {
            char c = sideB[i];
            if (c == ']') {
                if (right_bracket == 0) {
                    right_bracket = i;
                } else {
                    return 0;
                }
            } else if (c == '[') {
                if (right_bracket > 0) {
                    left_bracket = i;
                    break;
                } else {
                    return 0;
                }
            }

        }
        if (right_bracket && left_bracket) {
            sideB[left_bracket++] = '.';
            sideB[left_bracket++] = 'a';
            sideB[left_bracket++] = 't';
            sideB[left_bracket++] = 'r';
            sideB[left_bracket] = '\0';
            return 1;
        }
    }
    return 0;
}


static FILE *LookForCompanionFilename(int index, CompanionNameType type, size_t length) {
    FILE *result = NULL;
    char *sideB = MemAlloc(length + 1);
    memcpy(sideB, game_file, length + 1);
    if (type == TYPE_B) {
        sideB[index] = 'B';
    } else if (type == TYPE_2) {
        sideB[index] = '2';
    } else {
        sideB[index] = 't';
        sideB[index + 1] = 'w';
        sideB[index + 2] = 'o';
    }
    fprintf(stderr, "looking for companion file \"%s\"\n", sideB);
    result = fopen(sideB, "r");
    if (!result) {
        if (StripBrackets(&sideB, length)) {
            fprintf(stderr, "looking for companion file \"%s\"\n", sideB);
            result = fopen(sideB, "r");
        }
    }
    return result;
}


static FILE *GetCompanionFile(void) {
    FILE *result = NULL;
    size_t gamefilelen = strlen(game_file);
    char c;
    for (int i = (int)gamefilelen - 1; i >= 0 && game_file[i] != '/' && game_file[i] != '\\'; i--) {
        c = tolower(game_file[i]);
        if (i > 3 && ((c == 'e' && game_file[i - 1] == 'd' && game_file[i - 2] == 'i' && tolower(game_file[i - 3]) == 's') ||
            (c == 'k' && game_file[i - 1] == 's' && game_file[i - 2] == 'i' && tolower(game_file[i - 3]) == 'd'))) {
            if (gamefilelen > i + 2) {
                c = game_file[i + 1];
                if (c == ' ' || c == '_') {
                    c = tolower(game_file[i + 2]);
                    if (c == 'a') {
                        result = LookForCompanionFilename(i + 2, TYPE_B, gamefilelen);
                        if (result)
                            return result;
                    } else if (c == 'o' && gamefilelen > i + 4) {
                        if (game_file[i + 3] == 'n' && game_file[i + 4] == 'e') {
                            result = LookForCompanionFilename(i + 2, TYPE_TWO, gamefilelen);
                            if (result)
                                return result;
                        }
                    } else if (c == '1') {
                        result = LookForCompanionFilename(i + 2, TYPE_2, gamefilelen);
                        if (result)
                            return result;
                    }
                }
            }
        }
    }
    return NULL;
}

void PrintFirstTenBytes(uint8_t *ptr, size_t offset) {
    fprintf(stderr, "First 10 bytes at 0x%04zx: ", offset);
    for (int i = 0; i < 10; i++)
        fprintf(stderr, "\\x%02x", ptr[offset + i]);
    fprintf(stderr, "\n");
}

extern int x, y, xoff, yoff, xlen, ylen;

void SetColor(int32_t index, const RGB *color);
void DrawA8C64Pixels(int pattern, int pattern2);
extern RGB colors[];

void drawall(FILE *infile) {
    OpenGraphicsWindow();
    fseek(infile,0,SEEK_SET);
    int offset=0x0297;
    int count=75;
    int outpic = 0;
    int work, work2, size;
    int curpos, i, j, c;
    int countflag = (CurrentGame == COUNT_US || CurrentGame == VOODOO_CASTLE_US);
//    fprintf(stderr, "countflag == %d\n", countflag);
//    if (strcmp(argv[4],"y")==0) countflag=1;
//    printf("%d\n",countflag);
//
    // Get the size of the graphics chuck
    fseek(infile, offset, SEEK_SET);

    // Now loop round for each image
    while (outpic<count)
    {
        x=0;y=0;
        work2=fgetc(infile);
        do
        {
            work=work2;
            work2=fgetc(infile);
            size=(work2*256)+work;
//            fprintf(stderr, "%x\n",size);
        } while ( size == 0 || work==0);
        curpos=ftell(infile)-2;
//        fprintf(stderr, "%d: %x\n",outpic,curpos);
        fprintf(stderr, "    { IMG_ROOM, %d, 0x%04x },\n",outpic,curpos);

        //size=work+(fgetc(infile)*256)+curpos;
        size+=curpos;
        if (curpos < 0xb390 && size > 0xb390)
            size+=0x80;
//        fprintf(stderr, "Size: %x\n",size);
        // Get the offset
        xoff=fgetc(infile)-3;
        yoff=fgetc(infile);
        x=xoff*8;
        y=yoff;
//        fprintf(stderr, "xoff: %d yoff: %d\n",xoff,yoff);

        // Get the x length
        xlen=fgetc(infile);
        ylen=fgetc(infile);
        if (countflag) ylen-=2;
//        fprintf(stderr, "xlen: %x ylen: %x\n",xlen, ylen);

        // Get the palette
//        fprintf(stderr, "Colours: ");
        for (i=1;i<4;i++)
        {
            work=fgetc(infile);
            SetColor(i,&colors[work]);
            SetColor(i,&colors[work]);
//            fprintf(stderr, "%d ",work);
        }
        work=fgetc(infile);
        SetColor(0,&colors[work]);
//        fprintf(stderr, "%d\n",work);

        //fseek(infile,offset+10,SEEK_SET);
        j=0;
        while (!feof(infile) && ftell(infile)<size)
        {
            // First get count
            c=fgetc(infile);

            if (((c & 0x80) == 0x80) || countflag)
            { // is a counter
                if (!countflag) c &= 0x7f;
                if (countflag) c-=1;
                work=fgetc(infile);
                work2=fgetc(infile);
                for (i=0;i<c+1;i++)
                {
                    DrawA8C64Pixels(work,work2);
                }
            }
            else
            {
                // Don't count on the next j characters

                for (i=0;i<c+1;i++)
                {
                    work=fgetc(infile);
                    work2=fgetc(infile);
                    DrawA8C64Pixels(work,work2);
                }
            }
        }
        outpic++;
        HitEnter();
        // Now go to the next image
        fseek(infile,size,SEEK_SET);
        glk_window_clear(Graphics);
    }
    fclose(infile);
}

static int ExtractImagesFromCompanionFile(FILE *infile)
{
    int work,work2;
    size_t size;

    int outpic;

    const struct imglist *list = listHulk;
    if (CurrentGame == CLAYMORGUE_US)
        list = listClaymorgue;
    else if (CurrentGame == COUNT_US)
        list = listCount;
    else if (CurrentGame == VOODOO_CASTLE_US)
        list = listVoodoo;
//
//    OpenGraphicsWindow();

    USImages = new_image();
    struct USImage *image = USImages;

//    // Now loop round for each image
    for (outpic = 0; list[outpic].offset != 0; outpic++)
    {
        fseek(infile, list[outpic].offset, SEEK_SET);

        size = fgetc(infile);
        size += fgetc(infile) * 256 + 4;

        fseek(infile, list[outpic].offset - 2, SEEK_SET);

        image->usage = list[outpic].usage;
        image->index = list[outpic].index;
        image->imagedata = MemAlloc(size);
        image->datasize = size;
        image->systype = SYS_ATARI8;
        size_t readsize = fread(image->imagedata, 1, size, infile);
        if (list[outpic].offset < 0xb390 && list[outpic].offset + image->datasize > 0xb390) {
            fseek(infile, 0xb410, SEEK_SET);
            fread(image->imagedata + 0xb390 - list[outpic].offset + 2, 1, size - 0xb390 + list[outpic].offset - 2, infile);
        }
        if (readsize != size)
            fprintf(stderr, "Weird size for image %d. Expected %zu, got %zu\n", outpic, size, readsize);

        size = image->imagedata[2] + image->imagedata[3] * 256;

        fseek(infile, list[outpic].offset + size, SEEK_SET);

        int found = 1;
        work2 = fgetc(infile);
        do
        {
            work = work2;
            work2 = fgetc(infile);
            size = (work2 * 256) + work;
            if (feof(infile)) {
                found = 0;
                break;
            }
        } while ( size == 0 || size > 0x1600 || work == 0);
        if (found) {
            size_t possoff = ftell(infile) - 2;
            int found2 = 0;
            for (int i = 0; list[i].offset != 0; i++) {
                if (list[i].offset == possoff) {
                    found2 = 1;
                    break;
                }
            }
            if (!found2)
                fprintf(stderr, "Could not find offset %lx in database\n", possoff);
        }
//        DrawAtariC64Image(image);
//        HitEnter();

        image->next = new_image();
        image->next->previous = image;
        image = image->next;
    }

    if (CurrentGame != HULK_US && CurrentGame != CLAYMORGUE_US) {
        fprintf(stderr, "CurrentGame: %d\n", CurrentGame);
        if (image->previous) {
            image = image->previous;
            free(image->next);
            image->next = NULL;
        }
    }
    fclose(infile);

    infile = fopen(game_file, "r");
    if (infile)
        fseek(infile, 0x988e, SEEK_SET);
    image->usage = IMG_ROOM;
    image->index = 98;
    image->datasize = 1529 + 4;
    image->imagedata = MemAlloc(image->datasize);
    image->systype = SYS_ATARI8;
    fread(image->imagedata, image->datasize, 1, infile);
    fclose(infile);

    return 1;
}


static const uint8_t atrheader[6] = { 0x96, 0x02, 0x80, 0x16, 0x80, 0x00 };

int DetectAtari8(uint8_t **sf, size_t *extent)
{
    size_t data_start = 0x04c1;
    // Header actually starts at offset 0x04f9 (0x04c1 + 0x38).
    // We add 50 bytes at the head to match the C64 files.

    if (*extent > MAX_LENGTH || *extent < data_start)
        return 0;

    for (int i = 0; i < 6; i++)
        if ((*sf)[i] != atrheader[i])
            return 0;

    size_t newlength = *extent - data_start;
    uint8_t *datafile = MemAlloc(newlength);
    memcpy(datafile, *sf + data_start, newlength);

    if (datafile) {
        free(*sf);
        *sf = datafile;
        *extent = newlength;
//        CurrentSys = SYS_ATARI8;
        ImageWidth = 280;
        ImageHeight = 158;
        int result = LoadBinaryDatabase(*sf, *extent, *Game, 0);
        if (result)
            LookForAtari8Images(sf, extent);
        else
            fprintf(stderr, "Failed loading database\n");
        return result;

    }
    return 0;
}

int LookForAtari8Images(uint8_t **sf, size_t *extent) {
    FILE *CompanionFile = GetCompanionFile();
    if (!CompanionFile) {
        fprintf(stderr, "Did not find companion file.\n");
        return 0;
    }
//    drawall(CompanionFile);
//    fseek(CompanionFile,0,SEEK_SET);
    ExtractImagesFromCompanionFile(CompanionFile);
    return 1;
}

