//
//  c64detect.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-30.
//

#include <stdlib.h>
#include <string.h>

#include "c64diskimage.h"
#include "common.h"
#include "definitions.h"
#include "gameinfo.h"
#include "graphics.h"

#include "c64detect.h"

/* Read the named file from the disk image into a new buffer and set *size
   to its length. Returns NULL, leaving *size alone, if it can't be opened. */
static uint8_t *ReadFileFromD64(DiskImage *d64, const char *name, size_t *size)
{
    unsigned char rawname[100];
    di_rawname_from_name(rawname, name);
    ImageFile *c64file = di_open(d64, rawname, 0xc2, "rb");
    if (c64file == NULL)
        return NULL;
    /* di_read_file() frees c64file */
    uint8_t *file = di_read_file(c64file, size);
    if (file == NULL)
        Fatal("Out of memory");
    return file;
}

static uint8_t *get_files_from_d64(uint8_t *data, size_t length, size_t *newlength)
{
    uint8_t *file = NULL;
    *newlength = 0;
    DiskImage *d64 = di_create_from_data(data, (int)length);
    if (d64) {
        file = ReadFileFromD64(d64, "DATA", newlength);
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

            Images = NewImageList(imgindex);
            /* A file of a damaged disk may be listed and still not be
               readable. Leave it out: a picture without data cannot be drawn. */
            int loaded = 0;
            for (int i = 0; i < imgindex; i++) {
                Images[loaded].Data = ReadFileFromD64(d64, imagefiles[i], &Images[loaded].Size);
                if (Images[loaded].Data == NULL) {
                    free(imagefiles[i]);
                    continue;
                }
                Images[loaded++].Filename = imagefiles[i];
            }
        }
        /* The disk image struct owns nothing: it points into data */
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

    if (datafile == NULL)
        return 0;

    AdoptDataFile(sf, extent, datafile, newlength, SYS_C64);
    return 1;
}

/* Two object pictures of the C64 Spider-Man have a byte in the header
   (at SPIDERMAN_PATCH_OFFSET) that makes them draw in the wrong place. */
#define SPIDERMAN_PATCH_OFFSET 8

void PatchSpidermanImages(void)
{
    static const char *const patched[] = { "B053", "B056" };

    for (int i = 0; Images[i].Filename != NULL; i++) {
        for (size_t j = 0; j < sizeof patched / sizeof *patched; j++) {
            if (strcmp(patched[j], Images[i].Filename) == 0
                && Images[i].Size > SPIDERMAN_PATCH_OFFSET
                && Images[i].Data[SPIDERMAN_PATCH_OFFSET] == 8) {
                Images[i].Data[SPIDERMAN_PATCH_OFFSET] = 7;
                debug_print("Patched image %s\n", Images[i].Filename);
            }
        }
    }
}
