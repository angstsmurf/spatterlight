//
//  sagagraphics.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-07.
//

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "common_file_utils.h"
#include "glk.h"
#include "scott.h"

#include "sagagraphics.h"

int pixel_size;
int x_offset = 0;
/* The y_offset global is only used by the title image.
 We always position the in-game graphics
 relative to the graphics window top.
 Images have an internal "y origin" value
 that determines their vertical position. */
int y_offset = 0;
int right_margin;
int left_margin = 0;

USImage *USImages = NULL;

int has_graphics(void)
{
    return (USImages != NULL);
}

USImage *new_image(void)
{
    USImage *new = MemAlloc(sizeof(USImage));
    new->index = -1;
    new->datasize = 0;
    new->imagedata = NULL;
    new->systype = 0;
    new->usage = 0;
    new->cropleft = 0;
    new->cropright = 0;
    new->previous = NULL;
    new->next = NULL;
    return new;
}

void SetColor(int32_t index, const RGB *color)
{
    pal[index][0] = (*color)[0];
    pal[index][1] = (*color)[1];
    pal[index][2] = (*color)[2];
}

void PutPixel(glsi32 xpos, glsi32 ypos, int32_t color)
{
    if (!Graphics || xpos < 0 || xpos > right_margin || xpos < left_margin) {
        return;
    }
    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    glk_window_fill_rect(Graphics, glk_color, xpos * pixel_size + x_offset,
        ypos * pixel_size + y_offset, pixel_size, pixel_size);
}

void PutDoublePixel(glsi32 xpos, glsi32 ypos, int32_t color)
{
    if (xpos < 0 || xpos > right_margin || xpos < left_margin) {
        return;
    }
    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    glk_window_fill_rect(Graphics, glk_color, xpos * pixel_size + x_offset,
        ypos * pixel_size + y_offset, pixel_size * 2, pixel_size);
}

void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
              int32_t color)
{
    if (!Graphics)
        return;
    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    glk_window_fill_rect(Graphics, glk_color, x * pixel_size + x_offset,
                         y * pixel_size + y_offset, width * pixel_size,
                         height * pixel_size);
}

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

extern char *DirPath;

uint8_t *FindImageFile(const char *shortname, size_t *datasize)
{
    *datasize = 0;
    uint8_t *data = NULL;
    size_t pathlen = strlen(DirPath) + strlen(shortname) + 5;
    char *filename = MemAlloc(pathlen);
    int n = snprintf(filename, pathlen, "%s%s.PAK", DirPath, shortname);
    if (n > 0) {
        data = ReadFileIfExists(filename, datasize);
        if (data == NULL) {
            fprintf(stderr, "Could not find or read image file %s\n", filename);
        }
    }
    free(filename);
    return data;
}
