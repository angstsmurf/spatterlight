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

void ldir(uint8_t *mem, uint16_t DE, uint16_t HL, uint16_t BC)
{
    // Cannot use memcpy here because the rollover of 16-bit values is crucial
    for (int i = 0; i < BC; i++)
        mem[DE++] = mem[HL++];
}

void lddr(uint8_t *mem, uint16_t DE, uint16_t HL, uint16_t BC)
{
    // Cannot use memcpy here because the rollover of 16-bit values is crucial
    for (int i = 0; i < BC; i++)
        mem[DE--] = mem[HL--];
}

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

uint8_t *DeAlkatraz(uint8_t *raw_data, uint8_t *target, size_t offset, uint16_t IX, uint16_t DE, uint8_t *loacon, uint8_t add1, uint8_t add2, int selfmodify)
{
    if (target == NULL) {
        target = MemAlloc(0x10000);
        memset(target, 0, 0x10000);
    }

    for (size_t i = offset; DE != 0; i++) {
        uint8_t val = *loacon;
        uint8_t D = (DE >> 8) & 0xff;
        uint8_t E = DE & 0xff;
        val ^= D;
        val ^= E;
        val ^= (IX >> 8) & 0xff;
        val ^= IX & 0xff;
        val ^= raw_data[i];
        target[IX] = val;
        if (selfmodify) {
            if (IX == 0xe022)
                add1 = val;
            if (IX == 0xe02f)
                add2 = val;
        }
        if (E & 0x10) {
            *loacon = *loacon + add1 + E - D;
        }
        *loacon += add2;
        IX++;
        DE--;
    }

    return target;
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

/* add_block: unified helper for TAP/TZX block copying.

 For TAP (is_tzx == 0) it uses find_tap_block and does not free the returned pointer.
 For TZX (is_tzx == 1) it uses GetTZXBlock and frees the returned block after copying. */

static uint8_t *add_block(uint8_t *image, uint8_t *outbuf, size_t origlen, int blocknum, size_t offset, int is_tzx)
{
    size_t len = origlen;
    uint8_t *block = NULL;

    if (is_tzx) {
        block = GetTZXBlock(blocknum, image, &len);
        if (!block) return NULL;
        if (len >= 2)
            memcpy(outbuf + offset, block + 1, len - 2);
        free(block);
    } else {
        block = find_tap_block(blocknum, image, &len);
        if (!block) return NULL;
        memcpy(outbuf + offset, block, len);
        /* find_tap_block returns a pointer into the image data; do not free it */
    }
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
    size_t decomp_offset;
    uint16_t dealkatraz_DE;
    uint8_t add1;
    uint8_t add2;
    uint16_t lddr_DE;
    uint16_t lddr_HL;
    uint16_t lddr_BC;
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
    uint8_t *uncompressed = DeAlkatraz(block, NULL, spec->decomp_offset, SPECTRUM_PRINTER_BUFFER, spec->dealkatraz_DE, &loacon, spec->add1, spec->add2, 0);
    if (!uncompressed) {
        free(block);
        fprintf(stderr, "DeAlkatraz failed for block %d\n", spec->block_index);
        return image;
    }

    lddr(uncompressed, spec->lddr_DE, spec->lddr_HL, spec->lddr_BC);
    DeshuffleAlkatraz(uncompressed, spec->deshuffle_repeats, spec->deshuffle_IX, spec->deshuffle_store);

    free(block);
    uint8_t *shrunk = ShrinkToSnaSize(uncompressed, image, length);
    if (!shrunk) {
        /* ShrinkToSnaSize already freed uncompressed; return original image */
        return image;
    }
    return shrunk;
}

uint8_t *ProcessFile(uint8_t *image, size_t *length)
{
    size_t origlen = *length;

    uint8_t *uncompressed = DecompressZ80(image, length);
    if (uncompressed != NULL) {
        free(image);
        image = uncompressed;
    } else {
        /* Specs for repeated TZX extraction pattern */
        const ExtractSpec templeA = {
            .block_index = 4,
            .loacon_init = 0x9a,
            .decomp_offset = 0x1b0c,
            .dealkatraz_DE = 0x9f64,
            .add1 = 0xcf,
            .add2 = 0xcd,
            .lddr_DE = 0xffff,
            .lddr_HL = 0xfc85,
            .lddr_BC = 0x9f8e,
            .deshuffle_repeats = 05,
            .deshuffle_IX = 0x5b8b,
            .deshuffle_store = 0xfdd6
        };
        const ExtractSpec kayleth = {
            .block_index = 4,
            .loacon_init = 0xce,
            .decomp_offset = 0x172e,
            .dealkatraz_DE = 0xa004,
            .add1 = 0xeb,
            .add2 = 0x8f,
            .lddr_DE = 0xfd93,
            .lddr_HL = 0xfb02,
            .lddr_BC = 0x9e38,
            .deshuffle_repeats = 0x17,
            .deshuffle_IX = 0x5bf2,
            .deshuffle_store = 0xfd90
        };
        const ExtractSpec terraquake = {
            .block_index = 3,
            .loacon_init = 0xe7,
            .decomp_offset = 0x17eb,
            .dealkatraz_DE = 0x9f64,
            .add1 = 0x75,
            .add2 = 0x55,
            .lddr_DE = 0xfc80,
            .lddr_HL = 0xfa32,
            .lddr_BC = 0x9d38,
            .deshuffle_repeats = 0x03,
            .deshuffle_IX = 0x5b90,
            .deshuffle_store = 0xfc80
        };

        uint8_t *out = NULL;
        int failure = 0;
        int isTZX = 0;
        int TZXBlocknums[5] = {8, 12, 14, 16, 18};
        int TAPBlocknums[5] = {11, 15, 17, 19, 21};
        int *blocknums = NULL;

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
            case 0x10428: // Blizzard Pass tzx
                isTZX = 1;
                blocknums = TZXBlocknums;
            case 0x104c4: { // Blizzard Pass TAP
                if (isTZX == 0)
                    blocknums = TAPBlocknums;
                out = MemAlloc(0x2001f);
                if (!add_block(image, out, origlen, blocknums[0], 0x1801f, isTZX)) {
                    failure = 1;
                } else if (!add_block(image, out, origlen, blocknums[1], 0x801b, isTZX)) {
                    failure = 1;
                } else if (!add_block(image, out, origlen, blocknums[2], 0x401b, isTZX)) {
                    failure = 1;
                } else if (!add_block(image, out, origlen,  blocknums[3], 0x1ee6, isTZX)) {
                    failure = 1;
                } else if (!add_block(image, out, origlen, blocknums[4], 0xbff7, isTZX)) {
                    failure = 1;
                }
                if (failure == 1) {
                    free(out);
                    return image;
                } else {
                    *length = 0x2001f;
                    free(image);
                    return out;
                }
            }
            default:
                break;
        }
    }

    // This z80 file (Kayleth) was captured in the middle of
    // decompression, so it needs extra care.
    if (origlen == 0xb4bb) {
        uint8_t *mem = MemAlloc(0x10000);
        memcpy(mem + 0x4000, image, 0xc000);
        lddr(mem, 0xf906, 0xf8fe, 0x1ed5);
        mem[0xda2a] = 0;
        ldir(mem, 0xda2b, 0xda2a, 7);
        DeshuffleAlkatraz(mem, 0x13, 0x5bde, 0xfdcf);
        free(image);
        image = mem;
        *length = 0x10000;
    }
    return image;
}
