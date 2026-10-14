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
//
//  This is the same decoding as the "Trivia" routine in Infocom's IBM
//  interpreter sources (graphics.asm) does. The names of the three special
//  codes below are Infocom's (Clr_Token, Eof_Token, FF_Token).

#include <stdlib.h>
#include <string.h>

#include "decompress_vga.hpp"

static constexpr int kLzwMinCodeWidth = 9;
static constexpr int kLzwMaxCodeWidth = 12;
static constexpr int kLzwTableSize = 1 << kLzwMaxCodeWidth;

// Codes below kClearCode are literal pixels.
static constexpr uint16_t kClearCode = 256;     // Start over with an empty table
static constexpr uint16_t kEndCode = 257;       // End of the compressed stream
static constexpr uint16_t kFirstFreeCode = 258; // First code the table can define

// One entry of the code table: the string of `prefix` followed by `pixel`.
// A prefix of kLzwTableSize means "no prefix" (a root/literal code).
struct LzwEntry {
    uint16_t prefix;
    uint8_t pixel;
};

// The low `bits` bits set.
static constexpr unsigned low_bits(unsigned bits) {
    return (1u << bits) - 1;
}

// Persistent state for reading variable-width LZW codes from a bitstream.
typedef struct lzw_state_s {
    const uint8_t *data;      // The compressed data
    size_t datasize;          // Its length in bytes
    size_t bit_position;      // Offset in bits of the next code
    uint16_t next_table_code; // Next available code to add to the table
    uint16_t code_bit_width;  // Current code width in bits (9-12)
} lzw_state_t;


// Reads the next variable-width LZW code from the compressed bitstream.
// Codes are packed starting at the low bit of each byte.
// The code width grows by one bit when the next free code reaches 512, 1024
// and 2048, and stays at 12 bits until a clear code comes through. Returns
// -1 if the input data is exhausted.
static int16_t read_lzw_code(lzw_state_t *state)
{
    unsigned width = state->code_bit_width;
    if (state->bit_position + width > state->datasize * 8)
        return -1;

    // A code may straddle up to three bytes. The check above guarantees
    // that every byte holding a bit of it is inside the data.
    size_t byte_offset = state->bit_position >> 3;
    unsigned shift = state->bit_position & 7;
    uint32_t bits = 0;
    for (unsigned i = 0; i * 8 < shift + width; i++)
        bits |= (uint32_t)state->data[byte_offset + i] << (i * 8);
    uint16_t code = (bits >> shift) & low_bits(width);
    state->bit_position += width;

    // The caller is about to define next_table_code. If that is the last
    // code that fits in the current width, the code after this one is a bit
    // wider (up to the 12-bit maximum).
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
    size_t output_size = 0;
    ImageBuf output(image_alloc(image->width, image->height, 1, &output_size));
    if (output == nullptr)
        return nullptr;

    lzw_state_t state;
    state.data = image->data;
    state.datasize = image->datasize;
    state.bit_position = 0;
    state.next_table_code = kFirstFreeCode;
    state.code_bit_width = kLzwMinCodeWidth;

    // pixel_stack is used to reverse the prefix chain (which is stored
    // root-last) into the correct output order.
    uint8_t pixel_stack[kLzwTableSize];

    LzwEntry lzw_table[kLzwTableSize];

    for (int i = 0; i < kLzwTableSize; i++) {
        lzw_table[i].prefix = kLzwTableSize;
        lzw_table[i].pixel = (uint8_t)i;
    }

    size_t output_pos = 0;
    uint16_t previous_code = 0;
    for (;;) {
        int16_t raw_code = read_lzw_code(&state);
        if (raw_code < 0 || raw_code == kEndCode)
            break;
        if (raw_code >= kLzwTableSize)
            return nullptr;
        uint16_t code = raw_code;

        if (code == kClearCode) {
            // Reset the code table to its initial state.
            state.code_bit_width = kLzwMinCodeWidth;
            state.next_table_code = kFirstFreeCode;
            raw_code = read_lzw_code(&state);
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
            pixel_stack[stack_depth++] = lzw_table[code].pixel;
        } while ((code = lzw_table[code].prefix) != kLzwTableSize);

        do {
            if (output_pos >= output_size)
                break;
            output[output_pos++] = pixel_stack[--stack_depth];
        } while (stack_depth > 0);
    }
    return output.release();
}
