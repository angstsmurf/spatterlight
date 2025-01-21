//
//  decompress_amiga.cpp
//  bocfel6
//
//  Created by Administrator on 2023-08-02.
//

#include <stdlib.h>
#include "decompress_amiga.hpp"

// This is similar to the image compression used by Magnetic Scrolls.
// The code below is based on ms_extract1() in the Magnetic interpreter.
uint8_t *decompress_amiga(ImageStruct *image) {
    uint8_t repeats = 0;
    uint8_t color_index = 0;

    size_t finalsize = image->width * image->height;
    uint8_t *buffer = (uint8_t *)calloc(finalsize, 1);
    if (buffer == nullptr)
        exit(1);

    // Skip first 6 bytes
    int j = 6;
    int bit = 7;

    for (int i = 0; i < finalsize; i++, repeats--) {
        if (repeats == 0) {
            // Repeat while bit 7 of count is unset
            while (repeats < 0x80) {
                // This conditional is inverted in
                // the Magnetic code in ms_extract1().
                // That code will add 1 and read from the subsequent byte in the array if the bit is *unset*.
                // Here we do that if the bit is *set*.
                if (image->data[j] & (1 << bit)) {
                    repeats = image->huffman_tree[2 * repeats + 1];
                } else {
                    repeats = image->huffman_tree[2 * repeats];
                }

                if (!bit)
                    j++;

                if (j > image->datasize) {
                    fprintf(stderr, "decompress_amiga: Read %d bytes, which is out of range (expected data size: %zu). Something is wrong.\n", j, image->datasize);
                    free(buffer);
                    return nullptr;
                }

                bit = bit ? bit - 1 : 7;
            }

            repeats &= 0x7f;
            if (repeats >= 0x10) {
                // We subtract 1 less here than the
                // Magnetic code in ms_extract1()
                repeats -= 0xf;
            }  else {
                color_index = repeats;
                repeats = 1;
            }
        }

        buffer[i] = color_index;
    }

    // XOR with the corresponding byte in the line above
    for (int i = image->width; i < finalsize; i++) {
        buffer[i] ^= buffer[i - image->width];
    }

    return buffer;
}
