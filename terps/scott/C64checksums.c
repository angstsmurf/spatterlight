//
//  C64checksums.c
//  scott
//
//  Created by Administrator on 2022-01-30.
//
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "definitions.h"
#include "gameinfo.h"

#include "C64checksums.h"
#include "detectgame.h"
#include "diskimage.h"
#include "sagadraw.h"
#include "scott.h"

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

static struct c64rec c64_registry[] = {
    { ADVENTURELAND_C64, 0x6a10, 0x1910, TYPE_T64,
        1 }, // Adventureland C64 (T64) CruelCrunch v2.2
    { ADVENTURELAND_C64, 0x6a10, 0x1b10, TYPE_T64, 1, NULL, NULL, 0, 0,
        0 }, // Adventureland C64 (T64) alt CruelCrunch v2.2
    { ADVENTURELAND_C64, 0x2ab00, 0x6638, TYPE_D64, 1, NULL, NULL, 0, 0,
        0 }, // Adventureland C64 (D64) CruelCrunch v2.2
    { ADVENTURELAND_C64, 0x2adab, 0x751f, TYPE_D64, 0, NULL, NULL, 0, 0,
        0 }, // Adventureland C64 (D64) alt
    { ADVENTURELAND_C64, 0x2adab, 0x64a4, TYPE_D64, 0, NULL, "SAG1PIC", -0xa53,
        0, 0, 0, 0x65af }, // Adventureland C64 (D64) alt 2

    { SECRET_MISSION_C64, 0x88be, 0xa122, TYPE_T64, 1, NULL, NULL, 0, 0,
        0 }, // Secret Mission  C64 (T64) Section8 Packer
    { SECRET_MISSION_C64, 0x2ab00, 0x04d6, TYPE_D64, 0, NULL, NULL, 0, 0, 0, 0,
        -0x1bff }, // Secret Mission  C64 (D64)

    { CLAYMORGUE_C64, 0x6ff7, 0xe4ed, TYPE_T64, 3, NULL, NULL, 0, 0x855, 0x7352,
        0x20 }, // Sorcerer Of Claymorgue Castle C64 (T64), MasterCompressor / Relax
    // -> ECA Compacker -> MegaByte Cruncher v1.x Missing 17 pictures
    { CLAYMORGUE_C64, 0x912f, 0xa69f, TYPE_T64, 1, NULL, NULL, 0, 0x855, 0x7352,
        0x20 }, // Sorcerer Of Claymorgue Castle C64 (T64) alt, MegaByte Cruncher
    // v1.x Missing 17 pictures
    { CLAYMORGUE_C64, 0xc0dd, 0x3701, TYPE_T64, 1, NULL, NULL, 0, 0, 0, 0,
        -0x7fe }, // Sorcerer Of Claymorgue Castle C64 (T64) alt 2, Trilogic Expert
    // v2.7
    { CLAYMORGUE_C64, 0xbc5f, 0x492c, TYPE_T64, 1, NULL, NULL, 0, 0x855, 0x7352,
        0x20 }, // Sorcerer Of Claymorgue Castle C64 (T64) alt 3, , Section8 Packer
    { CLAYMORGUE_C64, 0x2ab00, 0xfd67, TYPE_D64, 1, NULL, NULL, 0, 0x855, 0x7352,
        0x20 }, // Sorcerer Of Claymorgue Castle C64 (D64), Section8 Packer

    { HULK_C64, 0x2ab00, 0xcdd8, TYPE_D64, 0, NULL, NULL, 0, 0x1806, 0xb801,
        0x307 }, // Questprobe 1 - The Hulk C64 (D64)
    { SPIDERMAN_C64, 0x2ab00, 0xde56, TYPE_D64, 0, NULL, NULL, 0, 0x1801, 0xa801,
        0x2000 }, // Spiderman C64 (D64)
    { SPIDERMAN_C64, 0x08e72, 0xb2f4, TYPE_T64, 3, NULL, NULL, 0, 0,
        0 }, // Spiderman C64 (T64) MasterCompressor / Relax -> ECA Compacker ->
    // Section8 Packer

    { SAVAGE_ISLAND_C64, 0x2ab00, 0x8801, TYPE_D64, 1, "-f86 -d0x1793",
        "SAVAGEISLAND1+", 1, 0, 0 }, // Savage Island part 1 C64 (D64)
    { SAVAGE_ISLAND_C64, 0x2ab00, 0xc361, TYPE_D64, 1, "-f86 -d0x1793",
        "SAVAGE ISLAND P1", 1, 0, 0 }, // Savage Island part 1 C64 (D64) alt
    { SAVAGE_ISLAND2_C64, 0x2ab00, 0x8801, TYPE_D64, 1, "-f86 -d0x178b",
        "SAVAGEISLAND2+", 1, 0, 0 }, // Savage Island part 2 C64 (D64)
    { SAVAGE_ISLAND2_C64, 0x2ab00, 0xc361, TYPE_D64, 1, NULL, "SAVAGE ISLAND P2",
        0, 0, 0 }, // Savage Island part 2  C64 (D64) alt

    { ROBIN_OF_SHERWOOD_C64, 0x2ab00, 0xcf9e, TYPE_D64, 1, NULL, NULL, 0, 0x1802,
        0xbd27, 0x2000 }, // Robin Of Sherwood D64 * unknown packer
    { ROBIN_OF_SHERWOOD_C64, 0xb2ef, 0x7c44, TYPE_T64, 1, NULL, NULL, 0, 0x9702,
        0x9627, 0x2000 }, // Robin Of Sherwood C64 (T64) * TCS Cruncher v2.0
    { ROBIN_OF_SHERWOOD_C64, 0xb690, 0x7b61, TYPE_T64, 1, NULL, NULL, 0, 0x9702,
        0x9627, 0x2000 }, // Robin Of Sherwood C64 (T64) alt * TCS Cruncher v2.0
    { ROBIN_OF_SHERWOOD_C64, 0x8db6, 0x7853, TYPE_T64, 1, NULL, NULL, 0, 0xd7fb,
        0xbd20, 0x2000 }, // Robin Of Sherwood T64 alt 2 * PUCrunch

    { GREMLINS_C64, 0xdd94, 0x25a8, TYPE_T64, 1, NULL, NULL,
        0 }, // Gremlins C64 (T64) version * Action Replay v4.x
    { GREMLINS_C64, 0x2ab00, 0xc402, TYPE_D64, 0, NULL, "G1",
        -0x8D }, // Gremlins C64 (D64) version
    { GREMLINS_C64, 0x2ab00, 0x3ccf, TYPE_D64, 0, NULL, "G1",
        -0x8D }, // Gremlins C64 (D64) version 2
    { GREMLINS_C64, 0x2ab00, 0xabf8, TYPE_D64, 2, "-e0x1255", NULL,
        2 }, // Gremlins C64 (D64) version alt * ByteBoiler, Exomizer
    { GREMLINS_C64, 0x2ab00, 0xa265, TYPE_D64, 2, "-e0x1255", NULL,
        2 }, // Gremlins C64 (D64)  version alt 2 * ByteBoiler, Exomizer
    { GREMLINS_GERMAN_C64, 0xc003, 0x558c, TYPE_T64, 1, NULL, NULL, 0, 0xd801,
        0xc6c0,
        0x1f00 }, // German Gremlins C64 (T64) version * TBC Multicompactor v2.x
    { GREMLINS_GERMAN_C64, 0x2ab00, 0x6729, TYPE_D64, 2, NULL, NULL, 0, 0xdc02,
        0xcac1, 0x1f00 }, // German Gremlins C64 (D64) version * Exomizer

    { SUPERGRAN_C64, 0x726f, 0x0901, TYPE_T64, 1, NULL, NULL, 0, 0xd802, 0xc623,
        0x1f00 }, // Super Gran C64 (T64) PUCrunch Generic Hack

    { SEAS_OF_BLOOD_C64, 0xa209, 0xf115, TYPE_T64, 6, "-e0x1000", NULL, 3,
        0xd802, 0xb07c,
        0x2000 }, // Seas of Blood C64 (T64) MasterCompressor / Relax -> ECA
    // Compacker -> Unknown -> MasterCompressor / Relax -> ECA
    // Compacker -> CCS Packer
    { SEAS_OF_BLOOD_C64, 0x2ab00, 0x5c1d, TYPE_D64, 1, NULL, NULL, 0, 0xd802,
        0xb07c, 0x2000 }, // Seas of Blood C64 (D64) CCS Packer
    { SEAS_OF_BLOOD_C64, 0x2ab00, 0xe308, TYPE_D64, 1, NULL, NULL, 0, 0xd802,
        0xb07c, 0x2000 }, // Seas of Blood C64 (D64) alt CCS Packer
};

static uint16_t checksum(unsigned char *sf, uint32_t extent)
{
    uint16_t c = 0;
    for (int i = 0; i < extent; i++)
        c += sf[i];
    return c;
}

int DecrunchC64(uint8_t **sf, size_t *extent, struct c64rec entry);

uint8_t *get_largest_file(uint8_t *data, int length, int *newlength)
{
    uint8_t *file = NULL;
    *newlength = 0;
    DiskImage *d64 = di_create_from_data(data, length);
    if (d64) {
        RawDirEntry *largest = find_largest_file_entry(d64);
        if (largest) {
            ImageFile *c64file = di_open(d64, largest->rawname, largest->type, "rb");
            if (c64file) {
                int expectedsize = largest->sizelo + largest->sizehi * 0x100;
                file = MemAlloc(expectedsize);
                *newlength = di_read(c64file, file, 0xffff);
            }
        }
        di_free_image(d64);
    }
    return file;
}

uint8_t *get_file_named(uint8_t *data, int length, int *newlength,
    const char *name)
{
    uint8_t *file = NULL;
    *newlength = 0;
    DiskImage *d64 = di_create_from_data(data, length);
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
        //        di_free_image(d64);
    }
    return file;
}

uint8_t *save_island_appendix_1 = NULL;
int save_island_appendix_1_length = 0;
uint8_t *save_island_appendix_2 = NULL;
int save_island_appendix_2_length = 0;

int savage_island_menu(uint8_t **sf, size_t *extent, int recindex)
{
    Output("This disk image contains two games. Select one.\n\n1. Savage Island "
           "part I\n2. Savage Island part II");

    glk_request_char_event(Bottom);

    event_t ev;
    int result = 0;
    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            if (ev.val1 == '1' || ev.val1 == '2') {
                result = ev.val1 - '0';
            } else {
                glk_request_char_event(Bottom);
            }
        }
    } while (result == 0);

    glk_window_clear(Bottom);

    if (result == 2) {
        if (recindex == 15)
            recindex = 17;
        else
            recindex = 18;
    }

    struct c64rec rec = c64_registry[recindex];
    int length;
    uint8_t *file = get_file_named(*sf, *extent, &length, rec.appendfile);

    if (file != NULL) {
        if (recindex == 16) {
            save_island_appendix_1 = get_file_named(
                *sf, *extent, &save_island_appendix_1_length, "SI1PC1");
            save_island_appendix_2 = get_file_named(
                *sf, *extent, &save_island_appendix_2_length, "SI1PC2");
        } else if (recindex == 18) {
            save_island_appendix_1 = get_file_named(
                *sf, *extent, &save_island_appendix_1_length, "SI2PIC");
        }
        *sf = file;
        *extent = length;
        if (save_island_appendix_1_length > 2)
            save_island_appendix_1_length -= 2;
        if (save_island_appendix_2_length > 2)
            save_island_appendix_2_length -= 2;
        return DecrunchC64(sf, extent, rec);
    } else {
        fprintf(stderr, "Failed loading file %s\n", rec.appendfile);
        return 0;
    }
}

void appendSIfiles(uint8_t **sf, size_t *extent)
{
    int total_length = *extent + save_island_appendix_1_length + save_island_appendix_2_length;

    uint8_t *megabuf = MemAlloc(total_length);
    memcpy(megabuf, *sf, *extent);
    free(*sf);
    int offset = 0x6202;

    if (save_island_appendix_1) {
        //    fprintf(stderr, "Appending file 2, length %d\n",
        //            save_island_appendix_1_length);
        memcpy(megabuf + offset, save_island_appendix_1 + 2,
            save_island_appendix_1_length);
        free(save_island_appendix_1);
    }
    if (save_island_appendix_2) {
        //    fprintf(stderr, "Appending file 3, length %d\n",
        //            save_island_appendix_2_length);
        memcpy(megabuf + offset + save_island_appendix_1_length,
            save_island_appendix_2 + 2, save_island_appendix_2_length);
        free(save_island_appendix_2);
    }
    *sf = megabuf;
    *extent = offset + save_island_appendix_1_length + save_island_appendix_2_length;
}

size_t CopyData(size_t dest, size_t source, uint8_t **data, size_t datasize,
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
    //  fprintf(stderr,
    //          "Copied %zu bytes from offset %zx to offset %zx. Old size: %zu New
    //          " "size: %zu\n", bytestomove, source, dest, datasize, newsize);
    return newsize;
}

int DetectC64(uint8_t **sf, size_t *extent)
{
    if (*extent > MAX_LENGTH || *extent < MIN_LENGTH)
        return 0;

    for (int i = 0; c64_registry[i].length != 0; i++) {
        if (*extent == c64_registry[i].length && checksum(*sf, *extent) == c64_registry[i].chk) {
            if (c64_registry[i].id == SAVAGE_ISLAND_C64) {
                return savage_island_menu(sf, extent, i);
            }
            if (c64_registry[i].type == TYPE_D64) {
                int newlength;
                uint8_t *largest_file = get_largest_file(*sf, *extent, &newlength);
                uint8_t *appendix = NULL;
                int appendixlen = 0;

                if (c64_registry[i].appendfile != NULL) {
                    appendix = get_file_named(*sf, *extent, &appendixlen,
                        c64_registry[i].appendfile);
                    if (appendix == NULL)
                        fprintf(stderr, "Appending file failed!\n");
                    appendixlen -= 2;
                }

                uint8_t *megabuf = MemAlloc(newlength + appendixlen);
                memcpy(megabuf, largest_file, newlength);
                if (appendix != NULL) {
                    memcpy(megabuf + newlength + c64_registry[i].parameter, appendix + 2,
                        appendixlen);
                    newlength += appendixlen;
                }

                if (largest_file) {
                    *sf = megabuf;
                    *extent = newlength;
                }

            } else if (c64_registry[i].type == TYPE_T64) {
                uint8_t *file_records = *sf + 64;
                int number_of_records = (*sf)[36] + (*sf)[37] * 0x100;
                int offset = file_records[8] + file_records[9] * 0x100;
                int start_addr = file_records[2] + file_records[3] * 0x100;
                int end_addr = file_records[4] + file_records[5] * 0x100;
                int size;
                if (number_of_records == 1)
                    size = *extent - offset;
                else
                    size = end_addr - start_addr;
                uint8_t *first_file = MemAlloc(size + 2);
                memcpy(first_file + 2, *sf + offset, size);
                memcpy(first_file, file_records + 2, 2);
                *sf = first_file;
                *extent = size + 2;
            }
            return DecrunchC64(sf, extent, c64_registry[i]);
        }
    }
    return 0;
}

size_t writeToFile(const char *name, uint8_t *data, size_t size)
{
    FILE *fptr = fopen(name, "w");

    if (fptr == NULL) {
        Fatal("File open error!");
    }

    size_t result = fwrite(data, 1, size, fptr);

    fclose(fptr);
    return result;
}

int DecrunchC64(uint8_t **sf, size_t *extent, struct c64rec record)
{
    uint8_t *uncompressed = NULL;
    file_length = *extent;

    size_t decompressed_length = *extent;

    uncompressed = MemAlloc(0xffff);

    char *switches[3];
    int numswitches = 0;

    if (record.switches != NULL) {
        char string[100];
        strcpy(string, record.switches);
        switches[numswitches] = strtok(string, " ");

        while (switches[numswitches] != NULL)
            switches[++numswitches] = strtok(NULL, " ");
    }

    size_t result = 0;

    for (int i = 1; i <= record.decompress_iterations; i++) {
        /* We only send switches on the iteration specified by parameter */
        if (i == record.parameter && record.switches != NULL) {
            result = unp64(entire_file, file_length, uncompressed,
                &decompressed_length, switches, numswitches);
        } else
            result = unp64(entire_file, file_length, uncompressed,
                &decompressed_length, NULL, 0);
        if (result) {
            if (entire_file != NULL)
                free(entire_file);
            entire_file = MemAlloc(decompressed_length);
            memcpy(entire_file, uncompressed, decompressed_length);
            file_length = decompressed_length;
        } else {
            free(uncompressed);
            break;
        }
    }

    for (int i = 0; i < NUMGAMES; i++) {
        if (games[i].gameID == record.id) {
            GameInfo = &games[i];
            break;
        }
    }

    if (GameInfo->Title == NULL) {
        Fatal("Game not found!");
    }

    size_t offset;

    DictionaryType dictype = GetId(&offset);
    if (dictype != GameInfo->dictionary) {
        Fatal("Wrong game?");
    }

    if (!TryLoading(*GameInfo, offset, 0)) {
        Fatal("Game could not be read!");
    }

    if (save_island_appendix_1 != NULL) {
        appendSIfiles(sf, extent);
    }

    if (record.copysource != 0) {
        result = CopyData(record.copydest, record.copysource, sf, *extent,
            record.copysize);
        if (result) {
            *extent = result;
        }
    }

    if (CurrentGame == CLAYMORGUE_C64 && record.copysource == 0x855) {
        result = CopyData(0x1531a, 0x2002, sf, *extent, 0x2000);
        if (result) {
            *extent = result;
        }
    }

    SagaSetup(record.imgoffset);

    return CurrentGame;
}