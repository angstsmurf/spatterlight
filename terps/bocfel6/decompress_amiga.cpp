//
//  decompress_amiga.cpp
//  bocfel6
//
//  Created by Administrator on 2023-08-02.
//

#include <stdlib.h>
#include "decompress_amiga.hpp"

static int isbit7set(uint8_t val) {
    return val >> 7;
}

uint8_t *decompress_amiga(ImageStruct *image) {
    uint8_t compressed_byte = 0;
    uint8_t bytevalue;
    uint8_t color_index = 0;
    int index = 0;
    int total_bytes_read = 0;

    // Skip first 6 bytes
    uint8_t *ptr = image->data + 6;

    size_t finalsize = image->width * image->height;
    uint8_t *buffer = (uint8_t *)malloc(finalsize);
    if (buffer == nullptr)
        exit(1);

    int counter = 1;

    do {
        bytevalue = 0;
        int found = 0;

        while (!found) {
            if (--counter <= 0) {
                counter = 8;
                compressed_byte = *ptr++;
                total_bytes_read++;
                if (total_bytes_read > finalsize) {
                    fprintf(stderr, "Read %d bytes, which is too many (expected final size: %zu). Something is wrong.\n", total_bytes_read, finalsize);
                    free(buffer);
                    return nullptr;
                }

            }
            do {
                bytevalue = image->lookup[(bytevalue << 1) + isbit7set(compressed_byte)];
                compressed_byte <<= 1;
                found = isbit7set(bytevalue);
            } while (!found && --counter);
        }

        bytevalue -= 0x90;
        if (isbit7set(bytevalue)) {
            color_index = bytevalue + 0x10;
            buffer[index++] = color_index;
        } else for (int i = (bytevalue & 0x017f) + 1; i > 0 && index < finalsize; i--) {
            // Repeat previous value (bytevalue - bit 7 + 1) number of times
            buffer[index++] = color_index;
        }
    } while (index < finalsize);

    // Every pixel value (palette index) from the second line down
    // has to be XORed with the corresponding pixel in the line above it
    for (int i = image->width; i < finalsize; i++) {
        buffer[i] ^= buffer[i - image->width];
    }
    return buffer;
}
