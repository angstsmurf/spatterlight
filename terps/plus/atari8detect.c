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
#include "gameinfo.h"

#include "atari8detect.h"

#define MAX_LENGTH 300000
#define MIN_LENGTH 24


typedef struct imglist {
    char *filename;
    size_t offset;
    size_t size;
} imglist;

struct imglist listFantastic[] = {

    { "R001", 0x0297, 0x07f7 },

    { "R002", 0x0a8e, 0x031d },

    { "R005", 0x690e, 0x07af },

    { "R006", 0x70ba, 0x05cd },

    { "R007", 0x7687, 0x0716 },

    { "R008", 0x1b8e, 0x07a8 },

    { "R009", 0x9dd6, 0x0c3d },

    { "R010", 0x7d9e, 0x079b },

    { "R011", 0x2333, 0x106c },

    { "R012", 0x853a, 0x033e },

    { "R013", 0x339e, 0x08ae },

    { "R014", 0x8879, 0x0808 },

    { "R015", 0x3c48, 0x08ed },

    { "R016", 0x4533, 0x03ca },

    { "R017", 0x48f9, 0x0a31 },

    { "R018", 0x9080, 0x05ec },

    { "R019", 0x966b, 0x076a },

    { "R020", 0x532c, 0x0c7c },

    { "R021", 0x5fa5, 0x096d },

    { "B020", 0xc10e, 0x0132 },

    { "B002", 0xdd0e, 0x04de },

    { "B003", 0xb800, 0x0397 },

    { "B004", 0xeb87, 0x07fc },

    { "B005", 0xbb97, 0x03e8 },

    { "B006", 0xc01e, 0x00ef },

    { "B007", 0xe1eb, 0x0107 },

    { "B008", 0xd180, 0x0382 },

    { "B009", 0xc3c8, 0x08ab },

    { "B010", 0xdbf2, 0x0121 },

    { "B011", 0xc241, 0x00e1 },

    { "B012", 0xc31e, 0x00ad },

    { "R004", 0xdac, 0x0ddf },

    { "B014", 0xf380, 0x0b00 },

    { "B015", 0xcc72, 0x0336 },

    { "B016", 0xcfa5, 0x01d9 },

    { "B017", 0xd500, 0x00e9 },

    { "B018", 0xd5e4, 0x060e },

    { "B019", 0xe2f2, 0x0338 },

    { "B013", 0xe625, 0x041d },

    { "B021", 0xbfac, 0x0075 },

    { "B022", 0xbf80, 0x002e },

    { "B023", 0xea41, 0x0148 },

    { "S000", 0x10e80, 0x0e8c },

    { "S001", 0xfe80, 0x02ef },

    { "S002", 0x1016b, 0x01c0 },

    { "S003", 0x1032c, 0x01ba },

    { "S004", 0x104e4, 0x0130 },

    { "S005", 0x10610, 0x010c },

    { "S006", 0x10717, 0x0124 },

    { "S007", 0x1083a, 0x0610 },

    { "S008", 0x10e48, 0x0038 },

    { "S009", 0x11d07, 0x0046 },

    { "S010", 0x11d48, 0x0180 },

    { "S011", 0x11ec8, 0x0137 },

    { "S012", 0x12000, 0x00f6 },

    { "S013", 0x120f2, 0x0217 },

    { "S014", 0x12307, 0x0113 },

    { "S015", 0x12417, 0x0116 },

    { "S016", 0x1252c, 0x00a8 },

    { "S017", 0x125cf, 0x0047 },

    { "S018", 0x12617, 0x0027 },

    { "S019", 0x1263a, 0x0087 },

    { "S020", 0x126c1, 0x00c1 },

    { "R022", 0xaa0e, 0x0d72 },

    { "R003", 0x10e48, 0x0038 },
    { NULL, 0, 0 }
};

struct imglist listSpidey[] = {
    { "R001", 0x0297, 0x0501 },
    { "R002", 0x078e, 0x03d6 },
    { "R003", 0x0b64, 0x0be1 },
    { "R005", 0x1741, 0x06c1 },
    { "R006", 0x1df9, 0x0c21 },
    { "R007", 0x2a17, 0x0ad1 },
    { "R008", 0x34e4, 0x07c1 },
    { "R013", 0x3c97, 0x09a1 },
    { "R015", 0x463a, 0x079a },
    { "R017", 0x4dd6, 0x0871 },
    { "R018", 0x5633, 0x0681 },
    { "R019", 0x5cac, 0x0d61 },
    { "R022", 0x6a00, 0x0a5f },
    { "R024", 0x7464, 0x0d51 },
    { "R025", 0x81ac, 0x06ca },
    { "R034", 0x875d, 0x0b31 },
    { "R037", 0x9280, 0x0eeb },
    { "R040", 0xa16b, 0x0c86 },
    { "B021", 0xadf2, 0x03e1 },
    { "B015", 0xb1c8, 0x0331 },
    { "B016", 0xb56b, 0x0331 },
    { "B023", 0xb88e, 0x0181 },
    { "B025", 0xba07, 0x00d1 },
    { "B027", 0xbac8, 0x01c3 },
    { "B028", 0xbc87, 0x05c3 },
    { "B031", 0xc248, 0x03f0 },
    { "B033", 0xc633, 0x03ac },
    { "B034", 0xc9dd, 0x040e },
    { "B035", 0xcdeb, 0x01b4 },
    { "B038", 0xcf9e, 0x0983 },
    { "B051", 0xd91e, 0x0279 },
    { "B052", 0xdb97, 0x04b2 },
    { "B058", 0xe048, 0x012c },
    { "B059", 0xe172, 0x035b },
    { "B060", 0xe4c8, 0x0128 },
    { "B062", 0xe5eb, 0x0523 },
    { "B071", 0xeb0e, 0x003d },
    { "B049", 0xeb48, 0x02e4 },
    { "B044", 0xee2c, 0x0269 },
    { "B053", 0xf090, 0x040d },
    { "B056", 0xf49e, 0x025b },
    { "B055", 0xf6f9, 0x01e6 },
    { "B050", 0xf8dd, 0x0687 },
    { "S001", 0xff64, 0x0237 },
    { "S002", 0x10197, 0x0215 },
    { "S003", 0x103ac, 0x0203 },
    { "S004", 0x105ac, 0x0289 },
    { "S005", 0x10833, 0x0265 },
    { "S007", 0x11141, 0x0401 },
    { "S008", 0x11541, 0x0a54 },
    { "S009", 0x11f90, 0x0cee },
    { "S010", 0x12c79, 0x091c },
    { "S011", 0x13590, 0x08d9 },
    { "S012", 0x13e64, 0x04e5 },
    { "S013", 0x14348, 0x069c },
    { "S014", 0x149e4, 0x03fb },
    { "S015", 0x14ddd, 0x00ba },
    { "S016", 0x14e97, 0x00a6 },
    { "S017", 0x14f3a, 0x012a },
    { "S018", 0x15064, 0x016f },
    { "S020", 0x151cf, 0x04bd },
    { "S021", 0x15687, 0x04e0 },
    { "S022", 0x15b64, 0x0617 },
    { "S023", 0x16179, 0x0506 },
    { "S024", 0x16682, 0x07aa },
    { "S026", 0x16179, 0x050b },
    { NULL, 0, 0 }
};

//{ "S000", 0, Found in disk image A at offset 6d97
//    0x0e94 },

struct imglist listBanzai[] = {
    { "R001", 0x0297, 0x05aa },
    { "R002", 0x083a, 0x092a },
    { "R003", 0xb072, 0x06fd },
    { "R004", 0x115d, 0x0ad1 },
    { "R005", 0x1c2c, 0x0a27 },
    { "R006", 0x264f, 0x0778 },
    { "R008", 0xb7f2, 0x0ac4 },
    { "R009", 0x2dc1, 0x0761 },
    { "R011", 0xc2b3, 0x056a },
    { "R012", 0x351e, 0x0651 },
    { "R013", 0x9d56, 0x08f1 },
    { "R014", 0x3b6b, 0x066a },
    { "R015", 0x1196b, 0x068a },
    { "R016", 0xc817, 0x022a },
    { "R017", 0x41cf, 0x0b1a },
    { "R018", 0x4ce4, 0x0b61 },
    { "R019", 0x11bc8, 0x0b61 },
    { "R020", 0x583a, 0x0c01 },
    { "R021", 0x5eb3, 0x114a },
    { "R022", 0x6556, 0x10ba },
    { "R023", 0x6b07, 0x11b1 },
    { "R024", 0x7080, 0x0f2a },
    { "R025", 0xa641, 0x0a3a },
    { "R026", 0x7941, 0x0711 },
    { "R028", 0x8048, 0x128a },
    { "R029", 0x856b, 0x0801 },
    { "R035", 0x8b33, 0x0d3b },
    { "S000", 0x92ac, 0x0ab1 },
    { "B027", 0xca33, 0x05a1 },
    { "B004", 0xcf9e, 0x00da },
    { "B005", 0xd017, 0x046a },
    { "B006", 0xd479, 0x04ca },
    { "B019", 0xd81e, 0x01b1 },
    { "B007", 0xd9cf, 0x0092 },
    { "B030", 0xda5d, 0x0170 },
    { "B025", 0xdbc1, 0x006a },
    { "B026", 0xdc25, 0x00aa },
    { "B051", 0xdcc8, 0x010a },
    { "B023", 0xddc8, 0x0100 },
    { "B055", 0xdeb3, 0x088a },
    { "B056", 0xe2b3, 0x088a },
    { "B070", 0xe7d6, 0x0181 },
    { "B071", 0xe956, 0x0100 },
    { "B073", 0xe9d6, 0x0046 },
    { "B074", 0xea90, 0x009a },
    { "B037", 0xd479, 0x020a },
    { "B034", 0xeaf9, 0x026a },
    { "B035", 0xed5d, 0x02aa },
    { "S001", 0xf000, 0x080a },
    { "S002", 0xf807, 0x005a },
    { "S003", 0xf85d, 0x027a },
    { "S004", 0xfacf, 0x020a },
    { "S005", 0xfcd6, 0x0281 },
    { "S006", 0xff56, 0x022a },
    { "S007", 0x10180, 0x0251 },
    { "S008", 0x103cf, 0x0284 },
    { "S009", 0x1064f, 0x0246 },
    { "S010", 0x1088e, 0x068a },
    { "S011", 0x10f17, 0x022b },
    { "S012", 0x11141, 0x02f4 },
    { "S013", 0x11433, 0x01e7 },
    { "S014", 0x11617, 0x015a },
    { "S015", 0x1176b, 0x00db },
    { "S016", 0x11841, 0x00aa },
    { "S017", 0x118e4, 0x008a },
    { NULL, 0, 0 }
};

typedef enum {
    TYPE_B,
    TYPE_TWO,
} CompanionNameType;

FILE *LookForCompanionFilename(int index, CompanionNameType type, size_t length) {
    char *sideB = MemAlloc(length + 1);
    memcpy(sideB, game_file, length + 1);
    if (type == TYPE_B) {
        sideB[index] = 'B';
    } else {
        sideB[index] = 't';
        sideB[index + 1] = 'w';
        sideB[index + 2] = 'o';
    }

    fprintf(stderr, "Looking for companion file with name \"%s\"\n", sideB);
    return fopen(sideB, "r");
}


FILE *GetCompanionFile(void) {
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
                        result = LookForCompanionFilename(i + 2, TYPE_B, gamefilelen);
                        if (result)
                            return result;
                    } else if (c == 'o' && gamefilelen > i + 4) {
                        if (game_file[i + 3] == 'n' && game_file[i + 4] == 'e') {
                            result = LookForCompanionFilename(i + 2, TYPE_TWO, gamefilelen);
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

static int ExtractImagesFromCompanionFileNew(FILE *infile)
{
    int work,work2;
    int count;
    size_t size;

    count = Game->no_of_room_images + Game->no_of_item_images + Game->no_of_special_images;
    Images = MemAlloc((count + 1) * sizeof(struct imgrec));

    if (CurrentGame == SPIDERMAN)
        count--;

    int outpic;

    struct imglist *list = listSpidey;
    if (CurrentGame == BANZAI)
        list = listBanzai;
    else if (CurrentGame == FANTASTIC4)
        list = listFantastic;

    // Now loop round for each image
    for (outpic = 0; outpic < count && list[outpic].filename != NULL; outpic++)
    {
        fseek(infile, list[outpic].offset - 2, SEEK_SET);

        Images[outpic].filename = list[outpic].filename;
        Images[outpic].data = MemAlloc(list[outpic].size);
        Images[outpic].size = list[outpic].size;
        size_t readsize = fread(Images[outpic].data, 1, list[outpic].size, infile);
        if (list[outpic].offset < 0xb390 && list[outpic].offset + Images[outpic].size > 0xb390) {
            fseek(infile, 0xb410, SEEK_SET);
            fread(Images[outpic].data + 0xb390 - list[outpic].offset + 2, 1, list[outpic].size - 0xb390 + list[outpic].offset - 2, infile);
            PrintFirstTenBytes(Images[outpic].data, 0x31c);

        }
        if (readsize != list[outpic].size)
            fprintf(stderr, "Weird size for image %d. Expected %zu, got %zu\n", outpic, list[outpic].size, readsize);

        fprintf(stderr, "Read image %d, name %s, offset 0x%04lx, size %ld (0x%04lx)\n", outpic, Images[outpic].filename, list[outpic].offset, Images[outpic].size, Images[outpic].size);
        PrintFirstTenBytes(Images[outpic].data, 2);

        size = Images[outpic].data[2] + Images[outpic].data[3] * 256;

        fseek(infile, list[outpic].offset + size, SEEK_SET);

        int found = 1;
        work2 = fgetc(infile);
        do
        {
            fprintf(stderr, "%zx: ",ftell(infile));
            work = work2;
            work2 = fgetc(infile);
            size = (work2 * 256) + work;
            fprintf(stderr, "%zx\n",size);
            if (feof(infile)) {
//                fprintf(stderr, "Could not find size of image %d. Total file size: %ld\n", outpic, ftell(infile));
//                return 0;
                found = 0;
                break;
            }
        } while ( size == 0 || size > 0x1600 || work == 0);
        if (found) {
            size_t possoff = ftell(infile) - 2;
            fprintf(stderr, "Possible following image size %ld (0x%04lx) at offset %lx\n", size, size, possoff);
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


    Images[outpic].filename = NULL;
    fclose(infile);

    if (CurrentGame == SPIDERMAN) {
        infile = fopen(game_file, "r");
        if (infile)
            fseek(infile, 0x6d95, SEEK_SET);
        Images[outpic].filename = "S000";
        Images[outpic].size = 0x0e96;
        Images[outpic].data = MemAlloc(Images[outpic].size);
        fread(Images[outpic].data, Images[outpic].size, 1, infile);
        Images[outpic + 1].filename = NULL;
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
        return 1;
    }
    return 0;
}

int LookForAtari8Images(uint8_t **sf, size_t *extent) {
    FILE *CompanionFile = GetCompanionFile();
    if (!CompanionFile) {
        Images = MemAlloc(sizeof(imgrec));
        Images[0].filename = NULL;
        return 0;
    }
    fprintf(stderr, "Successfully opened companion file\n");
    ExtractImagesFromCompanionFileNew(CompanionFile);
    return 1;
}

