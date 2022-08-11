//
//  c64detect.c
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

#include "c64detect.h"
#include "c64diskimage.h"

#define MAX_LENGTH 300000
#define MIN_LENGTH 24

size_t writeToFile(const char *name, uint8_t *data, size_t size)
{
    FILE *fptr = fopen(name, "w");

    size_t result = fwrite(data, 1, size, fptr);

    fclose(fptr);
    return result;
}


int issagaimg(const char *name) {
    if (name == NULL)
        return 0;
    size_t len = strlen(name);
    if (len < 4)
        return 0;
    char c = name[0];
    if (c == 'R' || c == 'B' || c == 'S') {
        for(int i = 1; i < 4; i++)
            if (!isdigit(name[i]))
                return 0;
        return 1;
    }
    return 0;
}

int FindPatternInFile(uint8_t *ptr, size_t offset, int previous) {
    FILE *f = fopen("/Users/administrator/Desktop/Apple2ReorderedData", "r");
    if (f == NULL)
        Fatal("Cannot open game");
    fseek(f, 0, SEEK_END);
    size_t memlen = ftell(f);
    if (memlen == -1) {
        fclose(f);
        glk_exit();
    }

    fseek(f, 0, SEEK_SET);
    uint8_t *mem = MemAlloc(memlen);
    memlen = fread(mem, 1, memlen, f);

    for (int j = 0; j < memlen - 10; j++) {
        if (mem[j] == ptr[offset]) {
            int found = 1;
            for (int i = 1; i < 10; i++) {
                if (mem[j + i] != ptr[offset + i]) {
                    found = 0;
                    break;
                }
            }
            if (found) {
                fprintf(stderr, "0x%04x, ", j);
//                int diff = j - previous;
//                fprintf(stderr, "Diff from previous (%x): %d\n", previous, diff);

                fclose(f);
                return j;
            }
        }
    }

    fprintf(stderr, "0, ");

    fclose(f);
    return 0;
}

static uint8_t *get_file_named(uint8_t *data, size_t length, size_t *newlength,
    const char *name)
{
    uint8_t *file = NULL;
    *newlength = 0;
    unsigned char rawname[100];
    DiskImage *d64 = di_create_from_data(data, (int)length);
    di_rawname_from_name(rawname, name);
    if (d64) {
        ImageFile *c64file = di_open(d64, rawname, 0xc2, "rb");
        if (c64file) {
            uint8_t buf[0xffff];
            *newlength = di_read(c64file, buf, 0xffff);
            file = MemAlloc(*newlength);
            memcpy(file, buf, *newlength);
        }
        int numfiles;
        char **filenames = get_all_file_names(d64, &numfiles);

        if (filenames) {
            int imgindex = 0;
            char *imagefiles[numfiles];
            for (int i = 0; i < numfiles; i++) {
                if (issagaimg(filenames[i])) {
                    imagefiles[imgindex++] = filenames[i];
                } else {
                    free(filenames[i]);
                }
            }
            free(filenames);

            int previous = 0;
            Images = MemAlloc((imgindex + 1) * sizeof(struct imgrec));
            for (int i = 0; i < imgindex; i++) {
                Images[i].filename = imagefiles[i];
                di_rawname_from_name(rawname, imagefiles[i]);
                c64file = di_open(d64, rawname, 0xc2, "rb");
                if (c64file) {
                    uint8_t buf[0xffff];
                    Images[i].size = di_read(c64file, buf, 0xffff);
                    Images[i].data = MemAlloc(Images[i].size);
                    memcpy(Images[i].data, buf, Images[i].size);
                    fprintf(stderr, "\n{ \"%s\", ", Images[i].filename);
                    //                    PrintFirstTenBytes(Images[i].data, 2);
                    int found = FindPatternInFile(Images[i].data, 5, previous);
                    if (found)
                        previous = found;
                    fprintf(stderr, "0x%04zx },\n", Images[i].size);
                }
            }
            Images[imgindex].filename = NULL;
        }


    }
    return file;
}

int DetectC64(uint8_t **sf, size_t *extent)
{
    if (*extent > MAX_LENGTH || *extent < MIN_LENGTH)
        return 0;

    size_t newlength;
    uint8_t *datafile = get_file_named(*sf, *extent, &newlength, "DATA");

    if (datafile) {
        free(*sf);
        *sf = datafile;
        *extent = newlength;
        CurrentSys = SYS_C64;
        return 1;
    }
    //            writeToFile("/Users/administrator/Desktop/C64PlusData", *sf, newlength);
    return 0;
}
