//
//  decompresstext.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Implements the text decompression scheme used by Robin of Sherwood
//  and Seas of Blood.
//
//  Created by Petter Sjölund on 2022-01-10.
//

#include <ctype.h>
#include <string.h>

#include "decompresstext.h"

#include "scott.h"

#define DECOMPRESS_BUF_SIZE 256

char *DecompressText(uint8_t *source, int stringindex)
{
    // Lookup table
    const char *alphabet = " abcdefghijklmnopqrstuvwxyz'\x01,.";

    int c, uppercase, i, j;
    uint8_t decompressed[DECOMPRESS_BUF_SIZE];
    int idx = 0;

    // Find the start of the compressed message
    for (i = 0; i < stringindex; i++) {
        source += *source & 0x7F;
        if (source >= entire_file + file_length)
            return NULL;
    };

    uppercase = ((*source & 0x40) == 0); // Test bit 6

    source++;
    do {
        if (source >= entire_file + file_length)
            return NULL;
        // Get eight characters squeezed into five bytes
        uint64_t fortybits = ((uint64_t)source[0] << 32) | ((uint64_t)source[1] << 24) | ((uint64_t)source[2] << 16) | ((uint64_t)source[3] << 8) | source[4];
        source += 5;
        for (j = 35; j >= 0; j -= 5) {
            // Decompress one character:
            int next = (fortybits >> j) & 0x1f;
            c = alphabet[next];

            if (c == 0x01) {
                uppercase = 1;
                c = ' ';
            }

            if (c >= 'a' && uppercase) {
                c = toupper(c);
                uppercase = 0;
            }
            decompressed[idx++] = c;

            if (idx == DECOMPRESS_BUF_SIZE - 1)
                c = 0; // We've gone too far, return result

            if (c == 0) {
                char *result = MemAlloc(idx);
                memcpy(result, decompressed, idx);
                return result;
            } else if (c == '.' || c == ',') {
                if (c == '.')
                    uppercase = 1;
                decompressed[idx++] = ' ';
            }
        }
    } while (idx < DECOMPRESS_BUF_SIZE);
    return NULL;
}
