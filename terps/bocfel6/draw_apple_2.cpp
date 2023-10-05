//
//  draw_apple_2.c
//  A2InfocomWozExtractor
//
//  Created by Administrator on 2023-07-14.
//

#include <stdlib.h>
#include <string.h>

#include "draw_apple_2.h"

struct rgb_struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

typedef struct rgb_struct rgb_t;

static const rgb_t apple2_palette[] =
{
    {0x0,  0x0,  0x0 }, /* Black */
    {0xa7, 0x0b, 0x40}, /* Dark Red */
    {0x40, 0x63, 0x00}, /* Brown */
    {0xe6, 0x6f, 0x00}, /* Orange */
    {0x00, 0x74, 0x40}, /* Dark Green */
    {0x80, 0x80, 0x80}, /* Dark Gray */
    {0x19, 0xd7, 0x00}, /* Light Green */
    {0xbf, 0xe3, 0x08}, /* Yellow */
    {0x40, 0x1c, 0xf7}, /* Dark Blue */
    {0xe6, 0x28, 0xff}, /* Purple */
    {0x80, 0x80, 0x80}, /* Light Grey */
    {0xff, 0x8b, 0xbf}, /* Pink */
    {0x19, 0x90, 0xff}, /* Medium Blue */ //0x0c
    {0xbf, 0x9c, 0xff}, /* Light Blue */
    {0x58, 0xf4, 0xbf}, /* Aquamarine */
    {0xff, 0xff, 0xff}  /* White */
};

static uint8_t *deindex(uint8_t *data, size_t size, ImageStruct *image) {
    uint8_t *pixmap = (uint8_t *)calloc(1, size * 4);
    int pixpos = 0;
    for (int i = 0; i < size; i++) {
        rgb_t color;
        uint8_t wordval = data[i];
        if (!(image->transparency && image->transparent_color == wordval)) {
            color = apple2_palette[wordval];
            if (pixpos >= size * 4)
                break;
            uint8_t *ptr = &pixmap[pixpos];
            *ptr++ = color.red;
            *ptr++ = color.green;
            *ptr++ = color.blue;
            *ptr++ = 0xff;
        }
        pixpos += 4;
    }
    free(data);
    return pixmap;
}

static uint8_t *decompress_apple2(ImageStruct *image) {
    if (image == nullptr || image->data == nullptr)
        return nullptr;

    uint8_t bytevalue, repeats, counter;

    uint8_t *ptr = image->data + 3;
    size_t finalsize = image->width * image->height;
    uint8_t *result = (uint8_t *)calloc(1, finalsize);
    if (result == nullptr)
        exit(1);

    size_t writtenbytes = 0;

    do {
        // Check if we are out of bounds
        long bytesread = ptr - image->data;
        if (bytesread >= image->datasize)
            return result;

        bytevalue = *ptr++;
        // Check if we are out of bounds again
        if (bytesread + 1 >= image->datasize || *ptr <= 0xf) {
            repeats = 0xf;
        } else {
            repeats = *ptr++;
        }

        for (counter = repeats - 0xe; counter != 0; counter--) {
            result[writtenbytes] = bytevalue;
            if (writtenbytes >= image->width) {
                result[writtenbytes] = result[writtenbytes] ^ result[writtenbytes - image->width];
            }
            writtenbytes++;
            if (writtenbytes == finalsize) {
                // Shave off any excess bytes
                if (ptr - image->data < image->datasize) {
                    size_t newsize = ptr - image->data;
                    uint8_t *temp = (uint8_t *)malloc(newsize);
                    if (temp == nullptr) {
                        fprintf(stderr, "Out of memory\n");
                        exit(1);
                    }
                    memcpy(temp, image->data, newsize);
                    free(image->data);
                    image->data = temp;
                    fprintf(stderr, "Shaved off %lu bytes. New size: %lu\n", image->datasize - newsize, newsize);
                    image->datasize = newsize;
                }
                return result;
            }
        }

    } while (writtenbytes < finalsize);
    return result;
}

uint8_t *draw_apple2(ImageStruct *image) {
    uint8_t *result = decompress_apple2(image);
    return deindex(result, image->width * image->height, image);
}
