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

#include "common.h"
#include "graphics.h"
#include "definitions.h"

#include "gameinfo.h"

#include "atari8detect.h"


typedef struct imglist {
    char *filename;
    size_t offset;
} imglist;


static const struct imglist listFantastic[] = {
    { "R001", 0x0297 },
    { "R002", 0x0a8e },
    { "R003", 0x10e48 },
    { "R004", 0xdac },
    { "R005", 0x690e },
    { "R006", 0x70ba },
    { "R007", 0x7687 },
    { "R008", 0x1b8e },
    { "R009", 0x9dd6 },
    { "R010", 0x7d9e },
    { "R011", 0x2333 },
    { "R012", 0x853a },
    { "R013", 0x339e },
    { "R014", 0x8879 },
    { "R015", 0x3c48 },
    { "R016", 0x4533 },
    { "R017", 0x48f9 },
    { "R018", 0x9080 },
    { "R019", 0x966b },
    { "R020", 0x532c },
    { "R021", 0x5fa5 },
    { "R022", 0xaa0e },
    { "B020", 0xc10e },
    { "B002", 0xdd0e },
    { "B003", 0xb800 },
    { "B004", 0xeb87 },
    { "B005", 0xbb97 },
    { "B006", 0xc01e },
    { "B007", 0xe1eb },
    { "B008", 0xd180 },
    { "B009", 0xc3c8 },
    { "B010", 0xdbf2 },
    { "B011", 0xc241 },
    { "B012", 0xc31e },
    { "B014", 0xf380 },
    { "B015", 0xcc72 },
    { "B016", 0xcfa5 },
    { "B017", 0xd500 },
    { "B018", 0xd5e4 },
    { "B019", 0xe2f2 },
    { "B013", 0xe625 },
    { "B021", 0xbfac },
    { "B022", 0xbf80 },
    { "B023", 0xea41 },
    { "S000", 0x10e80 },
    { "S001", 0xfe80 },
    { "S002", 0x1016b },
    { "S003", 0x1032c },
    { "S004", 0x104e4 },
    { "S005", 0x10610 },
    { "S006", 0x10717 },
    { "S007", 0x1083a },
    { "S008", 0x10e48 },
    { "S009", 0x11d07 },
    { "S010", 0x11d48 },
    { "S011", 0x11ec8 },
    { "S012", 0x12000 },
    { "S013", 0x120f2 },
    { "S014", 0x12307 },
    { "S015", 0x12417 },
    { "S016", 0x1252c },
    { "S017", 0x125cf },
    { "S018", 0x12617 },
    { "S019", 0x1263a },
    { "S020", 0x126c1 },
    { NULL, 0, }
};

static const struct imglist listSpidey[] = {
    { "R001", 0x0297 },
    { "R002", 0x078e },
    { "R003", 0x0b64 },
    { "R005", 0x1741 },
    { "R006", 0x1df9 },
    { "R007", 0x2a17 },
    { "R008", 0x34e4 },
    { "R013", 0x3c97 },
    { "R015", 0x463a },
    { "R017", 0x4dd6 },
    { "R018", 0x5633 },
    { "R019", 0x5cac },
    { "R022", 0x6a00 },
    { "R024", 0x7464 },
    { "R025", 0x81ac },
    { "R034", 0x875d },
    { "R037", 0x9280 },
    { "R040", 0xa16b },
    { "B021", 0xadf2 },
    { "B015", 0xb1c8 },
    { "B016", 0xb56b },
    { "B023", 0xb88e },
    { "B025", 0xba07 },
    { "B027", 0xbac8 },
    { "B028", 0xbc87 },
    { "B031", 0xc248 },
    { "B033", 0xc633 },
    { "B034", 0xc9dd },
    { "B035", 0xcdeb },
    { "B038", 0xcf9e },
    { "B051", 0xd91e },
    { "B052", 0xdb97 },
    { "B058", 0xe048 },
    { "B059", 0xe172 },
    { "B060", 0xe4c8 },
    { "B062", 0xe5eb },
    { "B071", 0xeb0e },
    { "B049", 0xeb48 },
    { "B044", 0xee2c },
    { "B053", 0xf090 },
    { "B056", 0xf49e },
    { "B055", 0xf6f9 },
    { "B050", 0xf8dd },
    { "S001", 0xff64 },
    { "S002", 0x10197 },
    { "S003", 0x103ac },
    { "S004", 0x105ac },
    { "S005", 0x10833 },
    { "S007", 0x11141 },
    { "S008", 0x11541 },
    { "S009", 0x11f90 },
    { "S010", 0x12c79 },
    { "S011", 0x13590 },
    { "S012", 0x13e64 },
    { "S013", 0x14348 },
    { "S014", 0x149e4 },
    { "S015", 0x14ddd },
    { "S016", 0x14e97 },
    { "S017", 0x14f3a },
    { "S018", 0x15064 },
    { "S020", 0x151cf },
    { "S021", 0x15687 },
    { "S022", 0x15b64 },
    { "S023", 0x16179 },
    { "S024", 0x16682 },
    { "S026", 0x16179 },
    { NULL, 0 }
};

//{ "S000", 0, Disk image A at offset 6d97
//    0x0e94 },

static const struct imglist listBanzai[] = {
    { "R001", 0x0297 },
    { "R002", 0x083a },
    { "R003", 0xb072 },
    { "R004", 0x115d },
    { "R005", 0x1c2c },
    { "R006", 0x264f },
    { "R008", 0xb7f2 },
    { "R009", 0x2dc1 },
    { "R011", 0xc2b3 },
    { "R012", 0x351e },
    { "R013", 0x9d56 },
    { "R014", 0x3b6b },
    { "R015", 0x1196b },
    { "R016", 0xc817 },
    { "R017", 0x41cf },
    { "R018", 0x4ce4 },
    { "R019", 0x11bc8 },
    { "R020", 0x583a },
    { "R021", 0x5eb3 },
    { "R022", 0x6556 },
    { "R023", 0x6b07 },
    { "R024", 0x7080 },
    { "R025", 0xa641 },
    { "R026", 0x7941 },
    { "R028", 0x8048 },
    { "R029", 0x856b },
    { "R035", 0x8b33 },
    { "S000", 0x92ac },
    { "B027", 0xca33 },
    { "B004", 0xcf9e },
    { "B005", 0xd017 },
    { "B006", 0xd479 },
    { "B019", 0xd81e },
    { "B007", 0xd9cf },
    { "B030", 0xda5d },
    { "B025", 0xdbc1 },
    { "B026", 0xdc25 },
    { "B051", 0xdcc8 },
    { "B023", 0xddc8 },
    { "B055", 0xdeb3 },
    { "B056", 0xe2b3 },
    { "B070", 0xe7d6 },
    { "B071", 0xe956 },
    { "B073", 0xe9d6 },
    { "B074", 0xea90 },
    { "B037", 0xd479 },
    { "B034", 0xeaf9 },
    { "B035", 0xed5d },
    { "S001", 0xf000 },
    { "S002", 0xf807 },
    { "S003", 0xf85d },
    { "S004", 0xfacf },
    { "S005", 0xfcd6 },
    { "S006", 0xff56 },
    { "S007", 0x10180 },
    { "S008", 0x103cf },
    { "S009", 0x1064f },
    { "S010", 0x1088e },
    { "S011", 0x10f17 },
    { "S012", 0x11141 },
    { "S013", 0x11433 },
    { "S014", 0x11617 },
    { "S015", 0x1176b },
    { "S016", 0x11841 },
    { "S017", 0x118e4 },
    { NULL, 0, }
};

typedef enum {
    TYPE_B,
    TYPE_TWO,
} CompanionNameType;

static FILE *LookForAtari8CompanionFilename(int index, CompanionNameType type, size_t length) {
    char sideB[length + 1];
    memcpy(sideB, game_file, length + 1);
    if (type == TYPE_B) {
        sideB[index] = 'B';
    } else {
        sideB[index] = 't';
        sideB[index + 1] = 'w';
        sideB[index + 2] = 'o';
    }
    return fopen(sideB, "r");
}


static FILE *GetCompanionFile(void) {
    FILE *result = NULL;
    size_t gamefilelen = strlen(game_file);
    char c;
    for (int i = (int)gamefilelen - 1; i >= 0 && game_file[i] != '/' && game_file[i] != '\\'; i--) {
        c = tolower(game_file[i]);
        if (i > 3 && c == 'e' && game_file[i - 1] == 'd' && game_file[i - 2] == 'i' && tolower(game_file[i - 3]) == 's') {
            if (gamefilelen > i + 2) {
                c = game_file[i + 1];
                if (c == ' ' || c == '_') {
                    c = tolower(game_file[i + 2]);
                    if (c == 'a') {
                        result = LookForAtari8CompanionFilename(i + 2, TYPE_B, gamefilelen);
                        if (result)
                            return result;
                    } else if (c == 'o' && gamefilelen > i + 4) {
                        if (game_file[i + 3] == 'n' && game_file[i + 4] == 'e') {
                            result = LookForAtari8CompanionFilename(i + 2, TYPE_TWO, gamefilelen);
                            if (result)
                                return result;
                        }
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

static int ExtractImagesFromAtariCompanionFile(FILE *infile)
{
    int work,work2;
    int count;
    size_t size;

    count = Game->no_of_room_images + Game->no_of_item_images + Game->no_of_special_images;
    Images = MemAlloc((count + 1) * sizeof(struct imgrec));

    if (CurrentGame == SPIDERMAN)
        count--;

    int outpic;

    const struct imglist *list = listSpidey;
    if (CurrentGame == BANZAI)
        list = listBanzai;
    else if (CurrentGame == FANTASTIC4)
        list = listFantastic;

    // Now loop round for each image
    for (outpic = 0; outpic < count && list[outpic].filename != NULL; outpic++)
    {
        fseek(infile, list[outpic].offset, SEEK_SET);

        size = fgetc(infile);
        size += fgetc(infile) * 256 + 4;

        fseek(infile, list[outpic].offset - 2, SEEK_SET);

        Images[outpic].Filename = list[outpic].filename;
        Images[outpic].Data = MemAlloc(size);
        Images[outpic].Size = size;
        size_t readsize = fread(Images[outpic].Data, 1, size, infile);
        if (list[outpic].offset < 0xb390 && list[outpic].offset + Images[outpic].Size > 0xb390) {
            fseek(infile, 0xb410, SEEK_SET);
            size_t expectedsize = size - 0xb390 + list[outpic].offset - 2;
            size_t readsize2 = fread(Images[outpic].Data + 0xb390 - list[outpic].offset + 2, 1, expectedsize, infile);
            if (readsize2 != expectedsize)
                fprintf(stderr, "Weird read for image %d. Expected %zu, got %zu\n", outpic, expectedsize, readsize2);

        }
        if (readsize != size)
            fprintf(stderr, "Weird size for image %d. Expected %zu, got %zu\n", outpic, size, readsize);

        size = Images[outpic].Data[2] + Images[outpic].Data[3] * 256;

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
            for (int i = 0; list[i].filename != NULL; i++) {
                if (list[i].offset == possoff) {
                    found2 = 1;
                    break;
                }
            }
            if (!found2)
                fprintf(stderr, "Could not find offset %lx in database\n", possoff);
        }
    }

    //{ "S000", 0, Found in disk image A at offset 6d97

    Images[outpic].Filename = NULL;
    fclose(infile);

    if (CurrentGame == SPIDERMAN) {
        infile = fopen(game_file, "r");
        if (infile)
            fseek(infile, 0x6d95, SEEK_SET);
        Images[outpic].Filename = "S000";
        Images[outpic].Size = 0x0e96;
        Images[outpic].Data = MemAlloc(Images[outpic].Size);
        size_t result = fread(Images[outpic].Data, Images[outpic].Size, 1, infile);
        if (result != Images[outpic].Size)
        fprintf(stderr, "Weird read for image %d. Expected %zu, got %zu\n", outpic, Images[outpic].Size, result);
        Images[outpic + 1].Filename = NULL;
        fclose(infile);
    }
    return 1;
}


static const uint8_t atrheader[6] = { 0x96 , 0x02, 0x80, 0x16, 0x80, 0x00 };

int DetectAtari8(uint8_t **sf, size_t *extent)
{
    size_t data_start = 0xff8e;
    // Header actually starts at offset 0xffc0 (0xff8e + 0x32).
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
        CurrentSys = SYS_ATARI8;
        ImageWidth = 280;
        ImageHeight = 158;
        return 1;
    }
    return 0;
}

int LookForAtari8Images(uint8_t **sf, size_t *extent) {
    FILE *CompanionFile = GetCompanionFile();
    if (!CompanionFile) {
        Images = MemAlloc(sizeof(imgrec));
        Images[0].Filename = NULL;
        return 0;
    }
    ExtractImagesFromAtariCompanionFile(CompanionFile);
    return 1;
}

