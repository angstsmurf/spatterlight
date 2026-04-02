//
//  decompress_amiga.cpp
//  bocfel6
//
//  Created by Administrator on 2023-08-02.
//
//  Decompresses Infocom V6 Amiga/Mac image data using a Huffman-based
//  run-length encoding scheme. Each pixel is encoded as a Huffman code
//  in the bitstream, where leaf nodes >= 0x90 represent run lengths
//  (repeat the current color N times) and leaf nodes < 0x10 represent
//  a new color index (used once). After decoding, a vertical XOR pass
//  reconstructs the final image from delta-encoded scanlines.

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

    int j = 6;     // Byte offset into compressed data (first 6 bytes are header)
    int bit = 7;   // Current bit position within the current byte (7 = MSB)

    for (int i = 0; i < finalsize; i++, repeats--) {
        if (repeats == 0) {
            // Walk the Huffman tree bit by bit until reaching a leaf node
            // (indicated by the high bit being set, i.e. value >= 0x80).
            // Each interior node at index N has children at 2*N (left/0-bit)
            // and 2*N+1 (right/1-bit).
            while (repeats < 0x80) {
                // Note: the bit sense is inverted compared to ms_extract1()
                // in the Magnetic interpreter. Here, a set bit follows the
                // right child; in Magnetic's code, an unset bit does.
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

            // The leaf value's low 7 bits encode either a run length or a
            // color index. Values >= 0x10 mean "repeat the current color
            // (value - 0x0f) times". Values < 0x10 set a new color index
            // with an implicit repeat count of 1.
            repeats &= 0x7f;
            if (repeats >= 0x10) {
                // Run length: subtract 0x0f (one less than Magnetic's 0x10
                // because the outer loop's post-decrement accounts for one).
                repeats -= 0xf;
            }  else {
                color_index = repeats;
                repeats = 1;
            }
        }

        buffer[i] = color_index;
    }

    // Delta decode: each scanline is XOR'd with the one above it.
    // The first scanline is stored literally; subsequent scanlines store
    // only the differences from the previous line.
    for (int i = image->width; i < finalsize; i++) {
        buffer[i] ^= buffer[i - image->width];
    }

    return buffer;
}
