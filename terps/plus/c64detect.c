//
//  c64detect.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-30.
//

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "c64diskimage.h"
#include "common.h"
#include "definitions.h"
#include "gameinfo.h"
#include "graphics.h"

#include "c64detect.h"

int issagaimg(const char *name)
{
    if (name == NULL)
        return 0;
    size_t len = strlen(name);
    if (len < 4)
        return 0;
    char c = name[0];
    if (c == 'R' || c == 'B' || c == 'S') {
        for (int i = 1; i < 4; i++)
            if (!isdigit(name[i]))
                return 0;
        return 1;
    }
    return 0;
}

static uint8_t *get_files_from_d64(uint8_t *data, size_t length, size_t *newlength)
{
    uint8_t *file = NULL;
    *newlength = 0;
    unsigned char rawname[100];
    DiskImage *d64 = di_create_from_data(data, (int)length);
    di_rawname_from_name(rawname, "DATA");
    if (d64) {
        ImageFile *c64file = di_open(d64, rawname, 0xc2, "rb");
        if (c64file) {
            uint8_t buf[0xffff];
            *newlength = di_read(c64file, buf, 0xffff);
            file = MemAlloc(*newlength);
            memcpy(file, buf, *newlength);
            free(c64file);
        }
        int numfiles;
        char **filenames = di_get_all_file_names(d64, &numfiles);

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

            Images = MemAlloc((imgindex + 1) * sizeof(imgrec));
            for (int i = 0; i < imgindex; i++) {
                Images[i].Filename = imagefiles[i];
                di_rawname_from_name(rawname, imagefiles[i]);
                c64file = di_open(d64, rawname, 0xc2, "rb");
                if (c64file) {
                    uint8_t buf[0xffff];
                    Images[i].Size = di_read(c64file, buf, 0xffff);
                    Images[i].Data = MemAlloc(Images[i].Size);
                    memcpy(Images[i].Data, buf, Images[i].Size);
                    free(c64file);
                }
            }
            Images[imgindex].Filename = NULL;
        }
        free(d64);
    }
    return file;
}

int DetectC64(uint8_t **sf, size_t *extent)
{
    if (*extent > MAX_LENGTH || *extent < MIN_LENGTH)
        return 0;

    size_t newlength;
    uint8_t *datafile = get_files_from_d64(*sf, *extent, &newlength);

    if (datafile) {
        free(*sf);
        *sf = datafile;
        *extent = newlength;
        CurrentSys = SYS_C64;
        ImageWidth = 280;
        ImageHeight = 160;
        return 1;
    }
    return 0;
}
