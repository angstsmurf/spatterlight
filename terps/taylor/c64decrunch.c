//
//  c64decrunch.c
//  scott
//
//  Created by Administrator on 2022-01-30.
//
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "utility.h"
#include "taylor.h"

#include "c64decrunch.h"
#include "diskimage.h"

#include "unp64_interface.h"

#define MAX_LENGTH 300000
#define MIN_LENGTH 24

typedef enum {
    UNKNOWN_FILE_TYPE,
    TYPE_D64,
    TYPE_T64 } file_type;

struct c64rec {
    GameIDType id;
    size_t length;
    uint16_t chk;
    file_type type;
    int decompress_iterations;
    const char *switches;
    const char *appendfile;
    int parameter;
    size_t copysource;
    size_t copydest;
    size_t copysize;
    size_t imgoffset;
};

static const struct c64rec c64_registry[] = {
    { QUESTPROBE3_64,  0x69e3, 0x3b96, TYPE_T64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // Questprobe 3, PUCrunch
    { QUESTPROBE3_64,  0x8298, 0xb93e, TYPE_T64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // Questprobe 3, PUCrunch
    { REBEL_PLANET_64, 0xd541, 0x593a, TYPE_T64, 1, NULL, NULL, 0, 0, 0, 0, 0 }, // Rebel Planet C64 (T64) 1001 CardCruncher Old Packer
    { HEMAN_64, 0xfa17, 0xfbd2, TYPE_T64, 2, NULL, NULL, 0, 0, 0, 0, 0 }, // Terraquake C64 (T64) Super Compressor / Flexible -> ECA Compacker
    { TEMPLE_OF_TERROR_64, 0xf716, 0x2b54, TYPE_T64, 4, NULL, NULL, 0, 0, 0, 0, 0 }, // Temple of Terror C64 (T64) 1001 CardCruncher New Packer -> 1001 CardCruncher ACM -> Triad-01 -> Mr.Z Packer
    { TEMPLE_OF_TERROR_64, 0x10baa, 0x3b37, TYPE_T64, 4, NULL, NULL, 0, 0, 0, 0, 0 }, // Temple of Terror C64 alt (T64) 1001 CardCruncher New Packer -> 1001 CardCruncher ACM -> Triad-01 -> Mr.Z Packer
    { TEMPLE_OF_TERROR_64, 0x2ab00, 0x5720, TYPE_D64, 4, "-e0xc1ef", NULL, 2, 0, 0, 0, 0 }, // Temple of Terror C64 (D64) ECA Compacker -> Super Compressor / Flexible -> Super Compressor / Equal sequences -> Super Compressor / Equal chars
    { KAYLETH_64, 0x2ab00, 0xc75f, TYPE_D64, 3, NULL, NULL, 0, 0, 0, 0, 0 }, // Kayleth D64 Super Compressor / Flexible Hack -> Super Compressor / Equal sequences -> Super Compressor / Equal chars
    { KAYLETH_64, 0xb1f2, 0x7757, TYPE_T64, 3, NULL, NULL, 0, 0, 0, 0, 0 }, // Kayleth T64 Super Compressor / Flexible Hack -> Super Compressor / Equal sequences -> Super Compressor / Equal chars

    { UNKNOWN_GAME, 0, 0, UNKNOWN_FILE_TYPE, 0, NULL, NULL, 0, 0, 0, 0, 0 }
};

static uint16_t checksum(uint8_t *sf, size_t extent)
{
    uint16_t c = 0;
    for (int i = 0; i < extent; i++)
        c += sf[i];
    return c;
}

static int DecrunchC64(uint8_t **sf, size_t *extent, struct c64rec entry);

static uint8_t *get_largest_file(uint8_t *data, size_t length, size_t *newlength)
{
    uint8_t *file = NULL;
    *newlength = 0;
    DiskImage *d64 = di_create_from_data(data, (int)length);
    if (d64) {
        RawDirEntry *largest = find_largest_file_entry(d64);
        if (largest) {
            ImageFile *c64file = di_open(d64, largest->rawname, largest->type, "rb");
            if (c64file) {
                uint8_t buf[0xffff];
                *newlength = di_read(c64file, buf, 0xffff);
                file = MemAlloc(*newlength);
                memcpy(file, buf, *newlength);
            }
        }
    }
    return file;
}

static uint8_t *get_file_named(uint8_t *data, size_t length, size_t *newlength,
    const char *name)
{
    uint8_t *file = NULL;
    *newlength = 0;
    DiskImage *d64 = di_create_from_data(data, (int)length);
    unsigned char rawname[100];
    di_rawname_from_name(rawname, name);
    if (d64) {
        ImageFile *c64file = di_open(d64, rawname, 0xc2, "rb");
        if (c64file) {
            uint8_t buf[0xffff];
            *newlength = di_read(c64file, buf, 0xffff);
            file = MemAlloc(*newlength);
            memcpy(file, buf, *newlength);
        }
    }
    return file;
}

static size_t CopyData(size_t dest, size_t source, uint8_t **data, size_t datasize,
    size_t bytestomove)
{
    if (source > datasize || *data == NULL)
        return 0;

    size_t newsize = MAX(dest + bytestomove, datasize);
    uint8_t *megabuf = MemAlloc(newsize);
    memcpy(megabuf, *data, datasize);
    memcpy(megabuf + dest, *data + source, bytestomove);
    free(*data);
    *data = megabuf;
    return newsize;
}

int DetectC64(uint8_t **sf, size_t *extent)
{
    if (*extent > MAX_LENGTH || *extent < MIN_LENGTH)
        return 0;

    uint16_t chksum = checksum(*sf, *extent);

    for (int i = 0; c64_registry[i].length != 0; i++) {
        if (*extent == c64_registry[i].length && chksum == c64_registry[i].chk) {
            if (c64_registry[i].type == TYPE_D64) {
                size_t newlength;
                uint8_t *largest_file = get_largest_file(*sf, *extent, &newlength);
                uint8_t *appendix = NULL;
                size_t appendixlen = 0;

                if (c64_registry[i].appendfile != NULL) {
                    appendix = get_file_named(*sf, *extent, &appendixlen,
                        c64_registry[i].appendfile);
                    if (appendix == NULL)
                        fprintf(stderr, "TAYLOR: DetectC64() Appending file failed!\n");
                    appendixlen -= 2;
                }

                size_t buflen = newlength + appendixlen;
                if (buflen <= 0 || buflen > MAX_LENGTH)
                    return 0;

                uint8_t *megabuf[buflen];
                memcpy(megabuf, largest_file, newlength);
                if (appendix != NULL) {
                    memcpy(megabuf + newlength + c64_registry[i].parameter, appendix + 2,
                        appendixlen);
                    newlength = buflen;
                }

                if (largest_file) {
                    free(*sf);
                    *sf = MemAlloc(newlength);
                    memcpy(*sf, megabuf, newlength);
                    *extent = newlength;
                }

            } else if (c64_registry[i].type == TYPE_T64) {
                uint8_t *file_records = *sf + 64;
                int number_of_records = (*sf)[36] + (*sf)[37] * 0x100;
                int offset = file_records[8] + file_records[9] * 0x100;
                int start_addr = file_records[2] + file_records[3] * 0x100;
                int end_addr = file_records[4] + file_records[5] * 0x100;
                size_t size;
                if (number_of_records == 1)
                    size = *extent - offset;
                else
                    size = end_addr - start_addr;
                uint8_t *first_file = MemAlloc(size + 2);
                memcpy(first_file + 2, *sf + offset, size);
                memcpy(first_file, file_records + 2, 2);
                free(*sf);
                *sf = first_file;
                *extent = size + 2;
            } else {
                Fatal("Unknown type");
            }
            return DecrunchC64(sf, extent, c64_registry[i]);
        }
    }
    return 0;
}

extern struct GameInfo games[NUMGAMES];


static int DecrunchC64(uint8_t **sf, size_t *extent, struct c64rec record)
{
    FileImageLen = *extent;
    size_t decompressed_length = *extent;

    uint8_t *uncompressed = MemAlloc(0xffff);

    char *switches[4];
    char string[100];
    int numswitches = 0;

    if (record.switches != NULL) {
        strcpy(string, record.switches);
        switches[numswitches] = strtok(string, " ");

        while (switches[numswitches] != NULL)
            switches[++numswitches] = strtok(NULL, " ");
    }

    size_t result = 0;

    writeToFile("/Users/administrator/Desktop/TaylorC64Raw", *sf, *extent);

    for (int i = 1; i <= record.decompress_iterations; i++) {
        /* We only send switches on the iteration specified by parameter */
        if (i == record.parameter && record.switches != NULL) {
            result = unp64(FileImage, FileImageLen, uncompressed,
                &decompressed_length, switches, numswitches);
        } else
            result = unp64(FileImage, FileImageLen, uncompressed,
                &decompressed_length, NULL, 0);
        if (result) {
            if (FileImage != NULL)
                free(FileImage);
            FileImage = MemAlloc(decompressed_length);
            memcpy(FileImage, uncompressed, decompressed_length);
            FileImageLen = decompressed_length;
        } else {
            free(uncompressed);
            uncompressed = NULL;
            break;
        }
    }

    if (uncompressed != NULL)
        free(uncompressed);

    for (int i = 0; games[i].Title != NULL; i++) {
        if (games[i].gameID == record.id) {
            free(Game);
            Game = &games[i];
            break;
        }
    }

    if (!Game || Game->Title == NULL) {
        Fatal("Game not found!");
    }

    if (record.copysource != 0) {
        result = CopyData(record.copydest, record.copysource, sf, *extent,
            record.copysize);
        if (result) {
            *extent = result;
        }
    }

    writeToFile("/Users/administrator/Desktop/TaylorC64Decompressed", *sf, *extent);

    return CurrentGame;
}
