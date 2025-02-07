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

// This palette is hand-crafted to approximate the output colours of the AppleWin emulator
static const rgb_t apple2_palette[] =
{
    {0x0,  0x0,  0x0 }, /* Black */
    {0xd4, 0x3a, 0x48}, /* Dark Red */
    {0x91, 0x67, 0x21}, /* Brown */
    {0xe6, 0x6f, 0x00}, /* Orange */
    {0x39, 0x82, 0x38}, /* Dark Green */
    {0x68, 0x68, 0x68}, /* Dark Gray */
    {0x67, 0xdf, 0x42}, /* Light Green */
    {0xff, 0xff, 0x00}, /* Yellow */
    {0x0c, 0x1c, 0xa2}, /* Dark Blue */
    {0xe6, 0x28, 0xff}, /* Purple */
    {0xb8, 0xb8, 0xb8}, /* Light Grey */
    {0xf3, 0xb3, 0xa2}, /* Pink */
    {0x2f, 0x3e, 0xe5}, /* Medium Blue */ //0x0c
    {0x79, 0xa6, 0xde}, /* Light Blue */
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
                    fprintf(stderr, "decompress_apple2: Shaved off %lu bytes. New size: %lu\n", image->datasize - newsize, newsize);
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
    if (result)
        return deindex(result, image->width * image->height, image);
    return nullptr;
}
