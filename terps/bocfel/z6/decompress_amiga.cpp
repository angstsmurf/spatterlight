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
//
//  Infocom's Mac interpreter sources (gfx.c) describe the compression as
//  three steps: each line is XORed with the previous line, which turns
//  whatever repeats from one line to the next into runs of zeros; the
//  result is run-length encoded, with bytes 0-15 being colors and 16-127
//  repeat counts with 15 added; and finally the run-length bytes are
//  optionally Huffman-coded. Every picture in the released files is
//  Huffman-coded, and that is the only kind this decoder reads.

#include <stdio.h>
#include <stdlib.h>
#include "decompress_amiga.hpp"

// This is similar to the image compression used by Magnetic Scrolls.
// The code below is based on ms_extract1() in the Magnetic interpreter.
uint8_t *decompress_amiga(ImageStruct *image) {
    if (image == nullptr || image->data == nullptr || image->huffman_tree == nullptr)
        return nullptr;

    // The picture data starts with two 3-byte lengths: that of the
    // compressed data, and that of the data after Huffman decoding but
    // before run-length decoding. load_image() in extract_image_data.cpp
    // has already used the first to set datasize.
    static const size_t kHeaderSize = 6;
    // A Huffman tree byte with kLeafFlag set is a leaf; its other bits are
    // the value. Values up to kMaxColor are a color index, larger ones a run
    // length stored with kMaxColor added.
    static const uint8_t kLeafFlag = 0x80;
    static const uint8_t kMaxColor = 0x0f;
    if (image->datasize < kHeaderSize)
        return nullptr;

    uint8_t remaining_repeats = 0;
    uint8_t color_index = 0;

    size_t pixel_count = 0;
    ImageBuf pixel_buffer(image_alloc(image->width, image->height, 1, &pixel_count));
    if (pixel_buffer == nullptr)
        return nullptr;

    size_t byte_offset = kHeaderSize;
    int bit_position = 7;  // Current bit within the current byte (7 = MSB)

    for (size_t pixel_index = 0; pixel_index < pixel_count; pixel_index++, remaining_repeats--) {
        if (remaining_repeats == 0) {
            // Walk the Huffman tree bit by bit until reaching a leaf node
            // (indicated by the high bit, kLeafFlag, being set).
            // Each interior node at index N has children at 2*N (left/0-bit)
            // and 2*N+1 (right/1-bit).
            uint8_t tree_node = 0;
            while ((tree_node & kLeafFlag) == 0) {
                if (byte_offset >= image->datasize) {
                    fprintf(stderr, "decompress_amiga: Read past end of data (offset %zu, data size: %zu).\n", byte_offset, image->datasize);
                    return nullptr;
                }

                // Note: the bit sense is inverted compared to ms_extract1()
                // in the Magnetic interpreter. Here, a set bit follows the
                // right child; in Magnetic's code, an unset bit does.
                if (image->data[byte_offset] & (1 << bit_position)) {
                    tree_node = image->huffman_tree[2 * tree_node + 1];
                } else {
                    tree_node = image->huffman_tree[2 * tree_node];
                }

                if (!bit_position)
                    byte_offset++;

                bit_position = bit_position ? bit_position - 1 : 7;
            }

            // The leaf value's low 7 bits encode either a run length or a
            // color index. Values above kMaxColor mean "repeat the current
            // color (value - kMaxColor) times". Other values set a new color
            // index with an implicit repeat count of 1.
            uint8_t leaf_value = tree_node & ~kLeafFlag;
            if (leaf_value > kMaxColor) {
                // Run length: subtract 0x0f (one less than Magnetic's 0x10.)
                remaining_repeats = leaf_value - kMaxColor;
            }  else {
                color_index = leaf_value;
                remaining_repeats = 1;
            }
        }

        pixel_buffer[pixel_index] = color_index;
    }

    // Delta decode: each scanline is XOR'd with the one above it.
    // The first scanline is stored literally; subsequent scanlines store
    // only the differences from the previous line.
    for (size_t pixel_index = image->width; pixel_index < pixel_count; pixel_index++) {
        pixel_buffer[pixel_index] ^= pixel_buffer[pixel_index - image->width];
    }

    return pixel_buffer.release();
}
