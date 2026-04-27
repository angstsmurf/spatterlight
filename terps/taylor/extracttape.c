//
//  extracttape.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Handles extraction and decryption of game data from ZX Spectrum tape images
//  (TZX and TAP formats) and Z80 memory snapshots. Most Adventure Soft UK games
//  used the "Alkatraz" copy protection scheme, which encrypts game data using
//  XOR with a self-modifying key stream, then shuffles memory blocks to obscure
//  the final layout. This file provides the tools to reverse that process.
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

// Start of the ZX Spectrum printer buffer area, used as a convenient
// staging address for decrypted data before it is relocated.
#define SPECTRUM_PRINTER_BUFFER 0x5b00

// Emulates the Z80 LDIR instruction: block copy from source to target,
// incrementing both pointers after each byte. Uses a byte-at-a-time loop
// rather than memcpy() because the original Z80 code may rely on
// overlapping source/target regions (e.g., filling a region with a
// repeated byte by copying from address N to address N+1).
void ldir(uint8_t *mem, uint16_t target, uint16_t source, uint16_t size)
{
    for (int i = 0; i < size; i++)
        mem[target++] = mem[source++];
}

// Emulates the Z80 LDDR instruction: block copy from source to target,
// decrementing both pointers after each byte. The backward copy direction
// is needed when relocating data to higher addresses with overlap.
void lddr(uint8_t *mem, uint16_t target, uint16_t source, uint16_t size)
{
    for (int i = 0; i < size; i++)
        mem[target--] = mem[source--];
}

// Unscrambles memory blocks that were shuffled by the Alkatraz loader.
// After decryption, the game data is not yet in its final locations — the
// loader relocates chunks using a table of move operations stored at IX.
//
// The table is traversed backwards (IX decrements by 5 per entry), with
// each 5-byte entry containing:
//   - 2 bytes: destination address (count)
//   - 2 bytes: block length minus 1
//   - 1 byte:  fill value for the destination gap
//
// For each entry, the routine shifts data upward via LDDR to make room,
// then fills the vacated region with a repeated byte value via LDIR.
void DeshuffleAlkatraz(uint8_t *mem, uint8_t repeats, uint16_t IX, uint16_t store)
{
    uint16_t count, length;
    for (int i = 0; i < repeats; i++) {
        count = READ_LE_UINT16(mem + IX - 4);
        length = READ_LE_UINT16(mem + IX - 2) + 1;
        store += length;
        lddr(mem, store, store - length, store - count + 1);
        mem[count] = mem[IX];
        ldir(mem, count + 1, count, length - 1);
        IX -= 5;
    }
}

// Main Alkatraz decryption routine. Decrypts data from encrypted_source into
// decrypted_target (a 64 KB buffer representing ZX Spectrum memory).
//
// Each source byte is XORed with a rolling key value (*loacon) that is itself
// transformed every iteration using the current target address, the remaining
// byte count, and two additive constants (add1, add2). This creates a
// position-dependent key stream that makes static analysis difficult.
//
// Parameters:
//   encrypted_source  - raw data read from the tape image
//   decrypted_target  - 64 KB output buffer (allocated if NULL)
//   source_offset     - starting offset into encrypted_source
//   target_offset     - starting Z80 address in decrypted_target
//   bytes_remaining   - number of bytes to decrypt
//   loacon            - pointer to the rolling key byte (modified in place)
//   add1, add2        - additive constants for key evolution
//   selfmodify        - if true, certain decrypted bytes update add1/add2,
//                       emulating the self-modifying aspect of the original code
//
// The parameter values are "magic numbers" determined by observing Z80 register
// contents in an emulator running the original protection code.
// The text-only Temple of Terror uses a stronger variant — see decrypttotloader.c.
uint8_t *DeAlkatraz(uint8_t *encrypted_source, uint8_t *decrypted_target, size_t source_offset, uint16_t target_offset, uint16_t bytes_remaining, uint8_t *loacon, uint8_t add1, uint8_t add2, int selfmodify)
{
    // Allocate a fresh 64 KB buffer if the caller didn't provide one
    if (decrypted_target == NULL) {
        decrypted_target = MemAlloc(0x10000);
        memset(decrypted_target, 0, 0x10000);
    }

    while (bytes_remaining != 0) {
        // Build the decryption key for this byte by XORing the rolling
        // key with the remaining count, target address, and source byte
        uint8_t val = *loacon;
        uint8_t D = (bytes_remaining >> 8) & 0xff;
        uint8_t E = bytes_remaining & 0xff;
        val ^= D;
        val ^= E;
        val ^= (target_offset >> 8) & 0xff;
        val ^= target_offset & 0xff;
        val ^= encrypted_source[source_offset];
        decrypted_target[target_offset] = val;

        // Self-modification: certain decrypted bytes overwrite the key
        // evolution constants, just as the original Z80 code would modify
        // its own instructions during execution
        if (selfmodify) {
            if (target_offset == 0xe022)
                add1 = val;
            if (target_offset == 0xe02f)
                add2 = val;
        }

        // Evolve the rolling key for the next iteration
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
    memcpy(mem + 0x4000, image, 0xc000);
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

    // First, try to decompress as a Z80 snapshot format
    uint8_t *uncompressed = DecompressZ80(image, length);
    if (uncompressed != NULL) {
        free(image);
        image = uncompressed;
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
    return image;
}
