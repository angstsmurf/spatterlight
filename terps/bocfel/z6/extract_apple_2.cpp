//
//  extract_apple_2.c
//  bocfel6
//
//  Created by Administrator on 2023-07-16.
//
//  Based on ZCut by Stefan Jokisch

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "debugprint.h"
extern "C" {
#include "ciderpress.h"
#include "woz2nib.h"
}
#include "v6_image.h"
#include "extract_image_data.hpp"
#include "extract_apple_2.h"

#define GET_BYTE(p,i)    (p[i])
#define GET_WORD(p,i)    (((uint8_t) p[i] << 8) | p[i+1])

static uint8_t table[1024];

static size_t filesize;
static size_t totalfilesize;

static uint8_t *final_game;
static size_t writepos;

static bool table_is_set = false;


#define    EXIT_FAILURE    1
#define    EXIT_SUCCESS    0

static size_t get_file_length(FILE *in)
{
    if (fseek(in, 0, SEEK_END) == -1) {
        return 0;
    }
    size_t length = ftell(in);
    if (length == -1) {
        return 0;
    }
    fseek(in, SEEK_SET, 0);
    return length;
}

static uint8_t *data_from_woz(const char *filename, size_t filenamelen, int *game, int *index, size_t *file_length){
    FILE *fp;

    if ((fp = fopen (filename, "rb")) == NULL) {
        fprintf(stderr, "Failed to open file \"%s\".\n", filename);
        perror ("fopen");
        return NULL;
    }

    char extension[5];
    extension[4] = 0;
    if (filenamelen > 4) {
        extension[3] = tolower(filename[filenamelen-1]);
        extension[2] = tolower(filename[filenamelen-2]);
        extension[1] = tolower(filename[filenamelen-3]);
        extension[0] = tolower(filename[filenamelen-4]);
    }

    *file_length = get_file_length(fp);
    if (*file_length == 0) {
        fprintf(stderr, "data_from_woz: file length 0!\n");
        return nullptr;
    }
    uint8_t *entire_file = (uint8_t *)malloc(*file_length);
    if (entire_file == NULL) {
        perror ("malloc");
        exit (EXIT_FAILURE);
    }

    size_t actual_size = fread(entire_file, 1, *file_length, fp) ;
    if (actual_size != *file_length) {
        fprintf(stderr, "Expected %ld, got %ld\n", *file_length, actual_size);
    }

    fclose(fp);

    if (entire_file[0] == 'W' && entire_file[1] == 'O' && entire_file[2] == 'Z') {
        uint8_t *result = woz2nib(entire_file, file_length);
        if (result) {
            free(entire_file);
            entire_file = result;
            InitNibImage(entire_file, *file_length);
        } else {
            perror ("woz2nib could not convert file to nib format!\n");
            return NULL;
        }
    }

    uint8_t *finalresult = ReadInfocomV6File(entire_file, file_length, game, index);
    free(entire_file);
    return finalresult;
}

static uint8_t *get_large_apple_game (size_t *file_size);

uint8_t *fpin[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
size_t fpinlengths[5] = { 0, 0, 0, 0, 0 };


static off_t find_graphics_offset (int disk);

static void set_table(void) {
    writepos = 0;
    uint8_t *data = fpin[0];
    if (data == nullptr) {
        fprintf(stderr, "Apple 2 disk data not initialized!\n");
        return;
    }
    memset(table, 0, 1024);
    memcpy(table, fpin[0], 512);

    if (GET_WORD(table,0) > 256) {
        memcpy(table + 512, data + 512, 512);
    }
    table_is_set = true;
}

static void free_fpin(void) {
    for (int i = 0; i < 5; i++) {
        if (fpin[i] != nullptr) {
            free(fpin[i]);
            fpin[i] = nullptr;
        }
        fpinlengths[i] = 0;
    }
    table_is_set = false;
}

static int global_numdisks = 0;

static int populate_fpin(const char *filename, size_t *file_length) {
    if (fpin[0] != nullptr)
        return global_numdisks;
    size_t filenamelen = strlen(filename);
    int game = -1, index;

    uint8_t *rawdata = data_from_woz(filename, filenamelen, &game, &index, file_length);

    global_numdisks = 5 - (game == 0);

    if (rawdata) {
        fpin[index - 1] = rawdata;
        fpinlengths[index - 1] = *file_length;

        int delimidx = 0;
        for (int i = (int)filenamelen - 1; i >= 0; i--) {
            char c = filename[i];
            if (c == '/' || c == '\\' || c == ':') {
                delimidx = i;
                break;
            }
        }

        int numidx = -1;
        for (int i = delimidx; i < filenamelen; i++) {
            if (filename[i] - '0' == index) {
                numidx = i;
                break;
            }
        }

        char newfile[1024];

        memcpy(newfile, filename, filenamelen + 1);

        if (numidx >= 0) {
            for (int i = 0; i < global_numdisks; i++) {
                if (i != index - 1) {
                    newfile[numidx] = '1' + i;
                    int index2;
                    if (fpin[i] == nullptr) {
                        fpin[i] = data_from_woz(newfile, filenamelen, NULL, &index2, file_length);
                        fpinlengths[i] = *file_length;
                    }
                    if (fpin[i] == nullptr) {
                        fprintf(stderr, "Could not extract file from disk %d!\n", i + 1);
                        free_fpin();
                        return 0;
                    }
                }
            }
        }
        return global_numdisks;
    }
    return 0;
}

static int extract_apple2_images(ImageStruct **raw, int *version, int numdisks) {

    ImageStruct *tempimages[5];

    int tempimagenum[5];
    int totalimages = 0;

    for (int i = 0; i < numdisks; i++) {
        off_t offset = find_graphics_offset(i+1);
        if (offset != 0) {
            // Trying to extract images from disk i + 1
            GraphicsType type = kGraphicsTypeApple2;
            tempimagenum[i] = extract_images(fpin[i], fpinlengths[i], i + 1, offset, &tempimages[i], version, &type);
            totalimages += tempimagenum[i];
        } else {
            tempimagenum[i] = 0;
            tempimages[i] = nullptr;
        }
    }

    int idx = 0;
    *raw = (ImageStruct *)calloc(totalimages, sizeof(ImageStruct));
    for (int i = 0; i < numdisks; i++) {
        if (tempimagenum[i] > 0) {
            for (int j = 0; j < tempimagenum[i]; j++) {
                (*raw)[idx].index = tempimages[i][j].index;
                (*raw)[idx].width = tempimages[i][j].width;
                (*raw)[idx].height = tempimages[i][j].height;
                (*raw)[idx].type = kGraphicsTypeApple2;
                (*raw)[idx].data = tempimages[i][j].data;
                (*raw)[idx].datasize = tempimages[i][j].datasize;
                (*raw)[idx].transparency = tempimages[i][j].transparency;
                (*raw)[idx].transparent_color = tempimages[i][j].transparent_color;
                idx++;
            }
        }
        if (tempimages[i] != nullptr)
            free(tempimages[i]);
    }
    return idx;
}


uint8_t *extract_apple_2(const char *filename, size_t *file_length, ImageStruct **rawimg, int *num_images, int *version) {

    int numdisks = populate_fpin(filename, file_length);
    if (numdisks == 0) {
        free_fpin();
        return nullptr;
    }
    uint8_t *finalresult = get_large_apple_game(file_length);

    if (rawimg != nullptr) {
        *num_images = extract_apple2_images(rawimg, version, numdisks);
    }

    return finalresult;
}

int extract_apple_2_images(const char *filename, ImageStruct **rawimg, int *version) {

    size_t dummy_file_length;

    int numdisks = populate_fpin(filename, &dummy_file_length);
    if (numdisks == 0) {
        free_fpin();
        return 0;
    }

    set_table();

    int numimages = extract_apple2_images(rawimg, version, numdisks);

    return numimages;
}

static uint16_t read_table_word (int offs) {
    return GET_WORD(table,offs);
}/* read_table_word */


static uint8_t header[64];

static void find_story_block (uint16_t log_block, int *disk, uint16_t *phys_block) {

    int offs = 20;
    uint16_t b1 = 0;
    uint16_t b2 = 0;

    for (*disk = 0; *disk < read_table_word (2); (*disk)++) {

        uint16_t entries = read_table_word (offs + 4);
        offs += 8;

        while (entries--) {

            b1 = read_table_word (offs + 0);
            b2 = read_table_word (offs + 2);
            uint16_t at = read_table_word (offs + 4);
            offs += 6;

            if (b1 <= log_block && log_block <= b2) {
                *phys_block = log_block + at - b1;
                return;

            }
        }
    }
}/* find_story_block */

static off_t find_graphics_offset(int disk) {

    if (disk == 1) {
        return read_table_word(22) * 0x200;
    }

    int offs = 20;

    for (int thisdisk = 1; thisdisk < read_table_word(2); thisdisk++) {

        uint16_t entries = read_table_word(offs + 4);

        offs += 8 + 6 * entries;

        if (thisdisk + 1 == disk)
            return read_table_word(offs + 2) * 0x200;
    }
    return 0;
}/* find_graphics_offset */



static void trans(int disk, int phys_block) {
    uint8_t buf[256];

    int size = sizeof (buf);

    if (filesize < size)
        size = (int) filesize;

    if (size != 0) {
        size_t readoffset = 512 * (phys_block);
        if (readoffset + 512 >= fpinlengths[disk] || writepos + 512 >= totalfilesize) {
            if (readoffset < fpinlengths[disk]) {
                size_t toread = fpinlengths[disk] - readoffset;
                if (writepos + toread >= totalfilesize) {
                    if (writepos >= totalfilesize)
                        return;
                    toread = totalfilesize - writepos;
                }
                memcpy(final_game + writepos, fpin[disk] + readoffset, toread);
                writepos += toread;
            }
            return;
        }
        memcpy(final_game + writepos, fpin[disk] + readoffset, 512);
    }
    writepos += 512;
    filesize -= size;
}/* trans */

static uint8_t *get_large_apple_game(size_t *outgoing_filesize) {

    if (!table_is_set)
        set_table();

    uint8_t *data1 = fpin[0];

    int disk;
    uint16_t phys_block = 0;

    find_story_block (0, &disk, &phys_block);

    memcpy (header, data1 + 512 * phys_block, 64);
    filesize = GET_WORD(header,26) * 8;

    if (filesize > 0x80000L)
        fprintf(stderr, "Disk image has bad format. filesize %zx maxsize 0x80000\n", filesize);

    final_game = (uint8_t *)malloc(filesize);
    totalfilesize = filesize;
    *outgoing_filesize = filesize;

    int numdisks = read_table_word (2);

    uint16_t log_block = 0;

    while (filesize > 0L) {

        find_story_block (log_block++, &disk, &phys_block);

        if (disk > numdisks - 1)
            break;

        trans (disk, phys_block);

    }

    return final_game;

}/* get_large_apple_game */
