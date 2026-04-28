//
//  decompress_vga.cpp
//  bocfel6
//
//  Created by Administrator on 2023-08-03.
//
//  Based on drilbo-mg1.h from Fizmo by Christoph Ender,
//  which in turn is based on Mark Howell's pix2gif utility
//
//  Implements LZW decompression for Infocom V6 VGA/EGA/CGA image data.
//  The compressed data uses variable-width codes (starting at 9 bits,
//  growing up to 12 bits) with a code table of up to 4096 entries.

#include <stdlib.h>
#include <string.h>

#include <memory>

#include "decompress_vga.hpp"

#define LZW_MIN_CODE_SIZE 8
#define LZW_TABLE_SIZE 4096
#define PREFIX 0    // Index into code table entry for the prefix code
#define PIXEL 1     // Index into code table entry for the pixel value

// Precomputed bitmasks for extracting N bits: bit_masks[n] = (1 << n) - 1.
static const uint16_t bit_masks[16] = {
    0x0000, 0x0001, 0x0003, 0x0007,
    0x000f, 0x001f, 0x003f, 0x007f,
    0x00ff, 0x01ff, 0x03ff, 0x07ff,
    0x0fff, 0x1fff, 0x3fff, 0x7fff
};

// Persistent state for reading variable-width LZW codes from a bitstream.
typedef struct lzw_state_s {
    uint16_t next_table_code;       // Next available code to add to the table
    uint16_t buffer_bits_remaining; // Unread bits remaining in the read buffer
    uint16_t buffer_bit_position;   // Current bit offset within the read buffer
    uint16_t code_bit_width;        // Current code width in bits (9-12)
} lzw_state_t;


// Reads the next variable-width LZW code from the compressed bitstream.
// Refills the read buffer from *ptr in LZW_TABLE_SIZE-byte chunks as needed.
// Automatically increases code_bit_width when the table is about to overflow
// the current width. Returns -1 if the input data is exhausted.
static int16_t read_lzw_code(const uint8_t **ptr, size_t bytes_remaining, lzw_state_t *state, uint8_t *read_buffer)
{
    uint16_t code = 0;
    uint16_t bits_needed = state->code_bit_width;
    uint16_t bits_assembled = 0;

    // Assemble the code one byte-boundary fragment at a time, since codes
    // may straddle byte boundaries in the packed bitstream.
    while (bits_needed)
    {
        // Refill the read buffer when all buffered bits have been consumed.
        if (state->buffer_bits_remaining == 0)
        {
            size_t chunk_size = LZW_TABLE_SIZE;
            if (bytes_remaining < chunk_size)
                chunk_size = bytes_remaining;
            if (chunk_size == 0)
                return -1;
            memcpy(read_buffer, *ptr, chunk_size);
            *ptr += chunk_size;
            bytes_remaining -= chunk_size;
            state->buffer_bits_remaining = chunk_size * 8;
            state->buffer_bit_position = 0;
        }

        // Extract bits from the current byte up to the next byte boundary,
        // or fewer if that's all we need to complete the code.
        uint16_t bits_available = 8 - (state->buffer_bit_position & 7);
        if (bits_needed < bits_available)
            bits_available = bits_needed;

        unsigned int byte_val = read_buffer[state->buffer_bit_position >> 3];
        unsigned int shifted = byte_val >> (state->buffer_bit_position & 7);
        unsigned int masked = shifted & bit_masks[bits_available];
        code |= masked << bits_assembled;

        bits_needed -= bits_available;
        bits_assembled += bits_available;
        state->buffer_bits_remaining -= bits_available;
        state->buffer_bit_position += bits_available;
    }

    // When the next code to be added would exceed the current bit width,
    // increase the width (up to the 12-bit maximum).
    if (state->code_bit_width < 12 && state->next_table_code == bit_masks[state->code_bit_width])
        state->code_bit_width++;

    return code;
}


// Decompresses LZW-encoded pixel data from an ImageStruct.
// Returns a malloc'd buffer of width * height bytes containing palette indices,
// or nullptr on allocation failure or corrupt data.
uint8_t *decompress_vga(const ImageStruct *image) {
    lzw_state_t state;
    uint8_t read_buffer[LZW_TABLE_SIZE];

    if (image->width <= 0 || image->height <= 0)
        return nullptr;
    size_t output_size = (size_t)image->width * (size_t)image->height;
    std::unique_ptr<uint8_t[], decltype(&free)> output(
        (uint8_t *)malloc(output_size), free);
    if (output == nullptr)
        return nullptr;

    // The clear code resets the table; end code (clear_code + 1) signals
    // the end of the compressed stream. First two codes after the 256
    // literal pixel codes are reserved for these special codes.
    uint16_t clear_code = 1 << LZW_MIN_CODE_SIZE;
    state.next_table_code = clear_code + 2;
    state.buffer_bits_remaining = 0;
    state.buffer_bit_position = 0;
    state.code_bit_width = LZW_MIN_CODE_SIZE + 1;

    // pixel_stack is used to reverse the prefix chain (which is stored
    // root-last) into the correct output order.
    uint8_t pixel_stack[LZW_TABLE_SIZE];

    // Each code table entry stores [PREFIX, PIXEL]: the prefix code that
    // precedes this entry's pixel value. LZW_TABLE_SIZE as a prefix
    // sentinel means "no prefix" (i.e., a root/literal code).
    uint16_t lzw_table[LZW_TABLE_SIZE][2];

    for (int i = 0; i < LZW_TABLE_SIZE; i++) {
        lzw_table[i][PREFIX] = LZW_TABLE_SIZE;
        lzw_table[i][PIXEL] = (uint16_t)i;
    }

    size_t output_pos = 0;
    const uint8_t *read_ptr = image->data;
    uint16_t previous_code = 0;
    for (;;) {
        int16_t raw_code = read_lzw_code(&read_ptr, image->datasize - (read_ptr - image->data), &state, read_buffer);
        if (raw_code < 0 || raw_code == clear_code + 1)
            break;
        if (raw_code >= LZW_TABLE_SIZE)
            return nullptr;
        uint16_t code = raw_code;

        if (code == clear_code) {
            // Reset the code table to its initial state.
            state.code_bit_width = LZW_MIN_CODE_SIZE + 1;
            state.next_table_code = clear_code + 2;
            raw_code = read_lzw_code(&read_ptr, image->datasize - (read_ptr - image->data), &state, read_buffer);
            if (raw_code < 0 || raw_code >= LZW_TABLE_SIZE)
                return nullptr;
            code = raw_code;
        } else {
            // Walk the prefix chain to find the root pixel of this code's
            // string. For the special case where code == next_table_code
            // (the "KwKwK" case), start from the previous code instead.
            uint16_t root_code = (code == state.next_table_code) ? previous_code : code;

            int chain_limit = LZW_TABLE_SIZE;
            while (lzw_table[root_code][PREFIX] != LZW_TABLE_SIZE && --chain_limit > 0) {
                root_code = lzw_table[root_code][PREFIX];
                if (root_code >= LZW_TABLE_SIZE)
                    return nullptr;
            }
            if (chain_limit <= 0)
                return nullptr;

            // Add a new entry: previous code's string + this string's root pixel.
            if (state.next_table_code < LZW_TABLE_SIZE) {
                lzw_table[state.next_table_code][PREFIX] = previous_code;
                lzw_table[state.next_table_code][PIXEL] = lzw_table[root_code][PIXEL];
                state.next_table_code++;
            }
        }
        previous_code = code;

        // Walk the prefix chain for this code, pushing each pixel onto the
        // stack (they come out in reverse order), then pop them into the
        // output buffer in the correct forward order.
        int stack_depth = 0;
        int chain_limit = LZW_TABLE_SIZE;
        do {
            if (stack_depth >= LZW_TABLE_SIZE || code >= LZW_TABLE_SIZE)
                return nullptr;
            pixel_stack[stack_depth++] = (uint8_t)lzw_table[code][PIXEL];
        } while ((code = lzw_table[code][PREFIX]) != LZW_TABLE_SIZE && --chain_limit > 0);
        if (chain_limit <= 0)
            return nullptr;

        do {
            if (output_pos >= output_size)
                break;
            output[output_pos++] = pixel_stack[--stack_depth];
        } while (stack_depth > 0);
    }
    return output.release();
}
