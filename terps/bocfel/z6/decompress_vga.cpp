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

#include "decompress_vga.hpp"

static constexpr int kLzwMinCodeSize = 8;
static constexpr int kLzwMaxCodeWidth = 12;
static constexpr int kLzwTableSize = 1 << kLzwMaxCodeWidth;

// One entry of the code table: the string of `prefix` followed by `pixel`.
// A prefix of kLzwTableSize means "no prefix" (a root/literal code).
struct LzwEntry {
    uint16_t prefix;
    uint16_t pixel;
};

// The low `bits` bits set.
static constexpr unsigned low_bits(unsigned bits) {
    return (1u << bits) - 1;
}

// Persistent state for reading variable-width LZW codes from a bitstream.
typedef struct lzw_state_s {
    uint16_t next_table_code;       // Next available code to add to the table
    uint16_t buffer_bits_remaining; // Unread bits remaining in the read buffer
    uint16_t buffer_bit_position;   // Current bit offset within the read buffer
    uint16_t code_bit_width;        // Current code width in bits (9-12)
} lzw_state_t;


// Reads the next variable-width LZW code from the compressed bitstream.
// Refills the read buffer from *ptr in kLzwTableSize-byte chunks as needed.
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
            size_t chunk_size = kLzwTableSize;
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
        unsigned int masked = shifted & low_bits(bits_available);
        code |= masked << bits_assembled;

        bits_needed -= bits_available;
        bits_assembled += bits_available;
        state->buffer_bits_remaining -= bits_available;
        state->buffer_bit_position += bits_available;
    }

    // When the next code to be added would exceed the current bit width,
    // increase the width (up to the 12-bit maximum).
    if (state->code_bit_width < kLzwMaxCodeWidth && state->next_table_code == low_bits(state->code_bit_width))
        state->code_bit_width++;

    return code;
}


// Follow the prefix chain of `code` to the root (literal) code its string
// starts with. Returns -1 if the chain leaves the table or does not end,
// which only corrupt data can cause.
static int find_root(const LzwEntry *table, uint16_t code) {
    for (int chain_limit = kLzwTableSize; chain_limit > 0; chain_limit--) {
        if (code >= kLzwTableSize)
            return -1;
        if (table[code].prefix == kLzwTableSize)
            return code;
        code = table[code].prefix;
    }
    return -1;
}

// Decompresses LZW-encoded pixel data from an ImageStruct.
// Returns a freshly allocated buffer of width * height bytes containing palette
// indices, or nullptr on bad dimensions, allocation failure, or corrupt data.
uint8_t *decompress_vga(const ImageStruct *image) {
    lzw_state_t state;
    uint8_t read_buffer[kLzwTableSize];

    size_t output_size = 0;
    ImageBuf output(image_alloc(image->width, image->height, 1, &output_size));
    if (output == nullptr)
        return nullptr;

    // The clear code resets the table; end code (clear_code + 1) signals
    // the end of the compressed stream. First two codes after the 256
    // literal pixel codes are reserved for these special codes.
    uint16_t clear_code = 1 << kLzwMinCodeSize;
    state.next_table_code = clear_code + 2;
    state.buffer_bits_remaining = 0;
    state.buffer_bit_position = 0;
    state.code_bit_width = kLzwMinCodeSize + 1;

    // pixel_stack is used to reverse the prefix chain (which is stored
    // root-last) into the correct output order.
    uint8_t pixel_stack[kLzwTableSize];

    LzwEntry lzw_table[kLzwTableSize];

    for (int i = 0; i < kLzwTableSize; i++) {
        lzw_table[i].prefix = kLzwTableSize;
        lzw_table[i].pixel = (uint16_t)i;
    }

    size_t output_pos = 0;
    const uint8_t *read_ptr = image->data;
    uint16_t previous_code = 0;
    for (;;) {
        int16_t raw_code = read_lzw_code(&read_ptr, image->datasize - (read_ptr - image->data), &state, read_buffer);
        if (raw_code < 0 || raw_code == clear_code + 1)
            break;
        if (raw_code >= kLzwTableSize)
            return nullptr;
        uint16_t code = raw_code;

        if (code == clear_code) {
            // Reset the code table to its initial state.
            state.code_bit_width = kLzwMinCodeSize + 1;
            state.next_table_code = clear_code + 2;
            raw_code = read_lzw_code(&read_ptr, image->datasize - (read_ptr - image->data), &state, read_buffer);
            if (raw_code < 0 || raw_code >= kLzwTableSize)
                return nullptr;
            code = raw_code;
        } else {
            // Walk the prefix chain to find the root pixel of this code's
            // string. For the special case where code == next_table_code
            // (the "KwKwK" case), start from the previous code instead.
            int root_code = find_root(lzw_table, (code == state.next_table_code) ? previous_code : code);
            if (root_code < 0)
                return nullptr;

            // Add a new entry: previous code's string + this string's root pixel.
            if (state.next_table_code < kLzwTableSize) {
                lzw_table[state.next_table_code].prefix = previous_code;
                lzw_table[state.next_table_code].pixel = lzw_table[root_code].pixel;
                state.next_table_code++;
            }
        }
        previous_code = code;

        // Walk the prefix chain for this code, pushing each pixel onto the
        // stack (they come out in reverse order), then pop them into the
        // output buffer in the correct forward order.
        int stack_depth = 0;
        do {
            if (stack_depth >= kLzwTableSize || code >= kLzwTableSize)
                return nullptr;
            pixel_stack[stack_depth++] = (uint8_t)lzw_table[code].pixel;
        } while ((code = lzw_table[code].prefix) != kLzwTableSize);

        do {
            if (output_pos >= output_size)
                break;
            output[output_pos++] = pixel_stack[--stack_depth];
        } while (stack_depth > 0);
    }
    return output.release();
}
