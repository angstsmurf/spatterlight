//
//  extracttape.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Handles extraction and decryption of game data from ZX Spectrum tape images
//  (TZX and TAP formats) and Z80 memory snapshots. Most Adventure Soft UK games
//  used the "Alkatraz" copy protection scheme (by Appleby Associates), which
//  encrypts game data using XOR with a self-modifying key stream, then shuffles
//  memory blocks to obscure the final layout. This file provides the tools to
//  reverse that process.
//
//  Created by Petter Sjölund on 2022-04-11.
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "decompressz80.h"
#include "decrypttotloader.h"
#include "loading_screen.h"
#include "taylor.h"
#include "utility.h"

#include "extracttape.h"

// Start of the ZX Spectrum printer buffer area, used as a convenient
// staging address for decrypted data before it is relocated.
#define SPECTRUM_PRINTER_BUFFER 0x5b00

// The Alkatraz block-copy (ldir/lddr), decryption (DeAlkatraz) and block
// unshuffle (DeshuffleAlkatraz) primitives now live in the shared decompressz80
// library (decompressz80.c/.h), so the scott interpreter can reuse them too.

// Extracts the game-relevant portion from a full 64 KB memory image.
// Copies 0xC01B bytes starting at offset 0x3FE5 (just below the 0x4000
// screen boundary), producing a buffer in .SNA snapshot format that the
// interpreter can load directly. Frees both the full-size buffer and any
// previous image.
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

// Extracts a single data block from a tape image (TAP or TZX format) and
// copies its payload into outbuf at the given offset. For TZX blocks, the
// first and last bytes (flag byte and checksum) are stripped; TAP blocks
// are copied as-is. Returns outbuf on success, or NULL if block extraction fails.
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

// Parameters for the common TZX extraction pipeline used by several games.
// Each game that uses Alkatraz protection follows the same sequence:
//   1. Extract a TZX block
//   2. Decrypt via DeAlkatraz into the Spectrum printer buffer area
//   3. Relocate data upward via LDDR
//   4. Unshuffle memory blocks via DeshuffleAlkatraz
//   5. Trim to SNA-sized output
// This struct captures the game-specific magic numbers for each step.
typedef struct {
    int block_index;           // Which TZX block to extract
    uint8_t loacon_init;       // Initial rolling key for DeAlkatraz
    size_t source_offset;      // Starting offset within the extracted block
    uint16_t size;             // Number of bytes to decrypt
    uint8_t add1;              // First key evolution constant
    uint8_t add2;              // Second key evolution constant
    uint16_t lddr_target;      // LDDR destination address
    uint16_t lddr_source;      // LDDR source address
    uint16_t lddr_size;        // LDDR byte count
    uint8_t deshuffle_repeats; // Number of deshuffle table entries
    uint16_t deshuffle_IX;     // Address of the deshuffle table (end)
    uint16_t deshuffle_store;  // Base store address for deshuffle
} ExtractSpec;

// Executes the standard Alkatraz extraction pipeline for a given game's
// parameters. Returns the SNA-sized game data, or the original image on failure.
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

// Special-case handler for a broken Kayleth Z80 snapshot that was captured
// mid-decompression. The snapshot contains partially-relocated data that
// needs the remaining LDDR and DeshuffleAlkatraz steps to be completed
// before the game data is usable.
uint8_t *FixBrokenKayleth(uint8_t *image, size_t *length) {
    uint8_t *mem = MemAlloc(0x10000);
    memcpy(mem + ZX_RAM_BASE, image, 0xc000);
    lddr(mem, 0xf906, 0xf8fe, 0x1ed5);
    mem[0xda2a] = 0;
    ldir(mem, 0xda2b, 0xda2a, 7);
    DeshuffleAlkatraz(mem, 0x13, 0x5bde, 0xfdcf);
    free(image);
    *length = 0x10000;
    return mem;
}

// Top-level entry point for processing a raw game file. Detects the file
// format and applies the appropriate extraction/decryption pipeline.
//
// The file may be a Z80 snapshot (handled by DecompressZ80), a TZX tape
// image, or a TAP tape image. TZX/TAP files are identified by their size,
// which is unique to each known game release. Returns a buffer containing
// the processed game data ready for the interpreter.
uint8_t *ProcessFile(uint8_t *image, size_t *length)
{
    size_t origlen = *length;

    // Capture a ZX loading screen to show as a title image. For .tap/.tzx
    // tapes the SCREEN$ is a data block in the raw image, so grab it now,
    // before the snapshot/tape extraction below transforms the buffer.
    int is_tzx = (origlen > 8 && memcmp(image, "ZXTape!\x1a", 8) == 0);
    int looks_like_tap = (!is_tzx && origlen > 21 &&
        image[0] == 0x13 && image[1] == 0x00 && image[2] == 0x00);
    if (is_tzx || looks_like_tap)
        ZXLoadingScreen = FindTapeLoadingScreen(image, origlen, is_tzx);

    // First, try to decompress as a Z80 snapshot format
    uint8_t *uncompressed = DecompressZ80(image, length);
    if (uncompressed != NULL) {
        free(image);
        image = uncompressed;
        // In a decompressed 48K snapshot, offset 0 maps to Spectrum address
        // 0x4000 (the display file): the first 6912 bytes are the screen
        // visible when the snapshot was saved.
        if (ZXLoadingScreen == NULL && *length >= ZX_SCREEN_SIZE) {
            ZXLoadingScreen = MemAlloc(ZX_SCREEN_SIZE);
            memcpy(ZXLoadingScreen, image, ZX_SCREEN_SIZE);
        }
    } else {
        // Not a Z80 snapshot — try TZX/TAP extraction, identified by file size.
        // Each ExtractSpec holds the Alkatraz decryption parameters for one game.
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

        // Block numbers differ between TZX and TAP for Blizzard Pass;
        // both formats contain the same 5 data blocks at different indices.
        uint8_t *out = NULL;
        int isTZX = 0;
        int TZXblocknums[5] = {8, 12, 14, 16, 18};
        int TAPblocknums[5] = {11, 15, 17, 19, 21};
        int *blocknums = TAPblocknums;

        // Dispatch by file size to identify which game/format we have
        switch (*length) {
            case 0xa000:  // Temple of Terror, Side B (text-only, TZX)
                image = DecryptToTSideB(image, length);
                image = ShrinkToSnaSize(image, uncompressed, length);
                break;
            case 0xcadc:  // Kayleth (TZX)
                image = process_tzx_extract(image, length, &kayleth);
                break;
            case 0xccca:  // Temple of Terror, Side A (TZX)
                image = process_tzx_extract(image, length, &templeA);
                break;
            case 0xcd15:  // Terraquake (TZX, variant 1)
            case 0xcd17:  // Terraquake (TZX, variant 2)
                image = process_tzx_extract(image, length, &terraquake);
                break;
            case 0x10428: // Blizzard Pass (TZX)
                isTZX = 1;
                blocknums = TZXblocknums;
                // Fallthrough
            case 0x104c4: { // Blizzard Pass (TAP)
                // Blizzard Pass isn't Alkatraz-encrypted — just concatenate
                // 5 data blocks into a single buffer at known offsets.
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

    // Post-processing: one known Kayleth Z80 snapshot (0xB4BB bytes) was
    // captured mid-decompression and needs its relocation completed.
    if (origlen == 0xb4bb) {
        image = FixBrokenKayleth(image, length);
    }

    // Snapshots saved at a text prompt (e.g. "Resume a saved game?") have
    // no picture, just black text on white. Don't show those as a title.
    if (ZXLoadingScreen != NULL && ZXScreenIsBlackOnWhite(ZXLoadingScreen)) {
        free(ZXLoadingScreen);
        ZXLoadingScreen = NULL;
    }

    return image;
}
