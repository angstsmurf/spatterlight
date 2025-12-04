//
//  extracttape.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-11.
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "decompressz80.h"
#include "decrypttotloader.h"
#include "taylor.h"
#include "utility.h"

#include "extracttape.h"

#define SPECTRUM_PRINTER_BUFFER 0x5b00

void ldir(uint8_t *mem, uint16_t target, uint16_t source, uint16_t size)
{
    // Z80 block copy function.
    // Cannot use memcpy() here because the original code might rely
    // on overlap behavior.
    for (int i = 0; i < size; i++)
        mem[target++] = mem[source++];
}

void lddr(uint8_t *mem, uint16_t target, uint16_t source, uint16_t size)
{
    // Z80 block copy function (backwards ldir).
    // Cannot use memcpy() here because the original code might rely
    // on overlap behavior.
    for (int i = 0; i < size; i++)
        mem[target--] = mem[source--];
}

// This moves the decrypted data into place.
// IX holds the memory offset of a list of values
// (length and count of move operations.)
void DeshuffleAlkatraz(uint8_t *mem, uint8_t repeats, uint16_t IX, uint16_t store)
{
    uint16_t count, length;
    for (int i = 0; i < repeats; i++) {
        length = mem[IX - 2] + mem[IX - 1] * 0x100 + 1;
        count = mem[IX - 4] + mem[IX - 3] * 0x100;
        store += length;
        lddr(mem, store, store - length, store - count + 1);
        mem[count] = mem[IX];
        ldir(mem, count + 1, count, length - 1);
        IX -= 5;
    }
}

// This is the main Alkatraz decryption function.
// The unencrypted source bytes are XORed with a semi-arbitrary value (val)
// which is iself transformed on every iteration.

// The values fed as parameters to this function are mostly "magic numbers"
// determined by looking at the memory / register contents of a ZX Spectrum
// emulator running the decryption code of the original games.

// The exception is the text-only version of Temple of Terror, which uses
// a stronger encryption. See code in decrypttotloader.c and loadtotpicture.c

uint8_t *DeAlkatraz(uint8_t *encrypted_source, uint8_t *decrypted_target, size_t source_offset, uint16_t target_offset, uint16_t bytes_remaining, uint8_t *loacon, uint8_t add1, uint8_t add2, int selfmodify)
{
    // If no pointer to target memory is provided, we create it.
    if (decrypted_target == NULL) {
        decrypted_target = MemAlloc(0x10000);
        memset(decrypted_target, 0, 0x10000);
    }

    while (bytes_remaining != 0) {
        uint8_t val = *loacon;
        uint8_t D = (bytes_remaining >> 8) & 0xff;
        uint8_t E = bytes_remaining & 0xff;
        val ^= D;
        val ^= E;
        val ^= (target_offset >> 8) & 0xff;
        val ^= target_offset & 0xff;
        val ^= encrypted_source[source_offset];
        decrypted_target[target_offset] = val;
        if (selfmodify) {
            if (target_offset == 0xe022)
                add1 = val;
            if (target_offset == 0xe02f)
                add2 = val;
        }
        if (E & 0x10) {
            *loacon = *loacon + add1 - D + E;
        }
        *loacon += add2;
        source_offset++;
        target_offset++;
        bytes_remaining--;
    }

    return decrypted_target;
}

static uint8_t *ShrinkToSnaSize(uint8_t *uncompressed, uint8_t *old_image, size_t *length)
{
    if (uncompressed == NULL)
        return NULL;

    uint8_t *sna = MemAlloc(0xc01b);
    memcpy(sna, uncompressed + 0x3fe5, 0xc01b);

    free(uncompressed);
    if (old_image) free(old_image);
    *length = 0xc01b;
    return sna;
}

/* add_block: unified helper for TAP/TZX block copying. */

static uint8_t *add_block(uint8_t *image, uint8_t *outbuf, size_t origlen, int blocknum, size_t offset, int is_tzx)
{
    size_t len = origlen;
    uint8_t *block = NULL;

    if (is_tzx) {
        block = GetTZXBlock(blocknum, image, &len);
        if (!block) return NULL;
        if (len >= 2)
            memcpy(outbuf + offset, block + 1, len - 2);
    } else {
        block = GetTAPBlock(blocknum, image, &len);
        if (!block) return NULL;
        memcpy(outbuf + offset, block, len);
    }
    free(block);
    return outbuf;
}

/* Helper used by multiple ProcessFile cases that extract a TZX block and run the same sequence:
 - GetTZXBlock
 - DeAlkatraz with given params
 - lddr with given params
 - DeshuffleAlkatraz with given params
 - ShrinkToSnaSize
 */

typedef struct {
    int block_index;
    uint8_t loacon_init;
    size_t source_offset;
    uint16_t size;
    uint8_t add1;
    uint8_t add2;
    uint16_t lddr_target;
    uint16_t lddr_source;
    uint16_t lddr_size;
    uint8_t deshuffle_repeats;
    uint16_t deshuffle_IX;
    uint16_t deshuffle_store;
} ExtractSpec;

/* Centralized sequence used by several TZX-extraction cases. */
static uint8_t *process_tzx_extract(uint8_t *image, size_t *length, const ExtractSpec *spec)
{
    uint8_t *block = GetTZXBlock(spec->block_index, image, length);
    if (!block) {
        fprintf(stderr, "TZX extract: Could not extract block %d\n", spec->block_index);
        return image;
    }

    uint8_t loacon = spec->loacon_init;
    uint8_t *uncompressed = DeAlkatraz(block, NULL, spec->source_offset, SPECTRUM_PRINTER_BUFFER, spec->size, &loacon, spec->add1, spec->add2, 0);
    if (!uncompressed) {
        free(block);
        fprintf(stderr, "DeAlkatraz failed for block %d\n", spec->block_index);
        return image;
    }

    lddr(uncompressed, spec->lddr_target, spec->lddr_source, spec->lddr_size);
    DeshuffleAlkatraz(uncompressed, spec->deshuffle_repeats, spec->deshuffle_IX, spec->deshuffle_store);

    free(block);
    uint8_t *shrunk = ShrinkToSnaSize(uncompressed, image, length);
    if (!shrunk) {
        /* ShrinkToSnaSize already freed uncompressed; return original image */
        return image;
    }
    return shrunk;
}

// Extra massaging for a Z80 memory snapshot that was captured
// from an emulator in the midst of decompression.
uint8_t *FixBrokenKayleth(uint8_t *image, size_t *length) {
    uint8_t *mem = MemAlloc(0x10000);
    memcpy(mem + 0x4000, image, 0xc000);
    lddr(mem, 0xf906, 0xf8fe, 0x1ed5);
    mem[0xda2a] = 0;
    ldir(mem, 0xda2b, 0xda2a, 7);
    DeshuffleAlkatraz(mem, 0x13, 0x5bde, 0xfdcf);
    free(image);
    *length = 0x10000;
    return mem;
}

uint8_t *ProcessFile(uint8_t *image, size_t *length)
{
    size_t origlen = *length;

    uint8_t *uncompressed = DecompressZ80(image, length);
    if (uncompressed != NULL) {
        free(image);
        image = uncompressed;
    } else {
        /* Specs for repeated TZX decryption pattern */
        const ExtractSpec templeA = {
            .block_index = 4,
            .loacon_init = 0x9a,
            .source_offset = 0x1b0c,
            .size = 0x9f64,
            .add1 = 0xcf,
            .add2 = 0xcd,
            .lddr_target = 0xffff,
            .lddr_source = 0xfc85,
            .lddr_size = 0x9f8e,
            .deshuffle_repeats = 05,
            .deshuffle_IX = 0x5b8b,
            .deshuffle_store = 0xfdd6
        };
        const ExtractSpec kayleth = {
            .block_index = 4,
            .loacon_init = 0xce,
            .source_offset = 0x172e,
            .size = 0xa004,
            .add1 = 0xeb,
            .add2 = 0x8f,
            .lddr_target = 0xfd93,
            .lddr_source = 0xfb02,
            .lddr_size = 0x9e38,
            .deshuffle_repeats = 0x17,
            .deshuffle_IX = 0x5bf2,
            .deshuffle_store = 0xfd90
        };
        const ExtractSpec terraquake = {
            .block_index = 3,
            .loacon_init = 0xe7,
            .source_offset = 0x17eb,
            .size = 0x9f64,
            .add1 = 0x75,
            .add2 = 0x55,
            .lddr_target = 0xfc80,
            .lddr_source = 0xfa32,
            .lddr_size = 0x9d38,
            .deshuffle_repeats = 0x03,
            .deshuffle_IX = 0x5b90,
            .deshuffle_store = 0xfc80
        };

        uint8_t *out = NULL;
        int isTZX = 0;
        int TZXblocknums[5] = {8, 12, 14, 16, 18};
        int TAPblocknums[5] = {11, 15, 17, 19, 21};
        int *blocknums = TAPblocknums;

        switch (*length) {
            case 0xa000:
                // This is the Temple of Terror side B tzx
                image = DecryptToTSideB(image, length);
                image = ShrinkToSnaSize(image, uncompressed, length);
                break;
            case 0xcadc:
                image = process_tzx_extract(image, length, &kayleth);
                break;
            case 0xccca:
                image = process_tzx_extract(image, length, &templeA);
                break;
            case 0xcd15:
            case 0xcd17:
                image = process_tzx_extract(image, length, &terraquake);
                break;
            case 0x10428: // Blizzard Pass TZX
                isTZX = 1;
                blocknums = TZXblocknums;
                // Fallthrough
            case 0x104c4: { // Blizzard Pass TAP
                out = MemAlloc(0x2001f);
                int BlizzardPassBlockOffsets[5] = {0x1801f, 0x801b, 0x401b, 0x1ee6, 0xbff7};
                for (int i = 0; i < 5; i++) {
                    if (!add_block(image, out, origlen, blocknums[i], BlizzardPassBlockOffsets[i], isTZX)) {
                        free(out);
                        return image;
                    }
                }
                *length = 0x2001f;
                free(image);
                return out;
            }
            default:
                break;
        }
    }

    if (origlen == 0xb4bb) {
        // This z80 file (Kayleth) was captured in the middle of
        // decompression, so it needs extra care.
        image = FixBrokenKayleth(image, length);
    }
    return image;
}
