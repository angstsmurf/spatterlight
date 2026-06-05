//
//  decompressz80.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-01-24.
//

#ifndef decompressz80_h
#define decompressz80_h

#include <stdint.h>
#include <stdlib.h>

/* Will return NULL on error or 0xc000 (49152) bytes
 of uncompressed raw data on success */
uint8_t *DecompressZ80(uint8_t *raw_data, size_t *length);

/* Decompress a .z80 snapshot and return the raw RAM pages in page-index
 * order (pages[5] is always 0x4000-0x7FFF, pages[2] is 0x8000-0xBFFF, the
 * rest are banked pages for 128K snapshots).
 *
 * Returns a single malloc'd buffer of 8 * 0x4000 bytes on success, NULL on
 * error. Each 0x4000-byte slice corresponds to one Spectrum RAM page. Pages
 * not present in the snapshot are zero-filled.
 *
 * If 'is_128k' is non-NULL, it is set to 1 for 128K snapshots and 0 for 48K.
 * If 'banked_page' is non-NULL, it is set to the page index currently
 * mapped at 0xC000 according to the snapshot's 0x7FFD port value (only
 * meaningful for 128K). */
uint8_t *DecompressZ80Pages(uint8_t *raw_data, size_t length,
    int *is_128k, int *banked_page);

uint8_t *GetTAPBlock(int wantedindex, const uint8_t *buffer,
    size_t *length);
uint8_t *GetTZXBlock(int blockno, uint8_t *srcbuf, size_t *length);

/* Concatenate the payloads of every standard/turbo data block in a .tzx
 * (is_tzx != 0) or .tap (is_tzx == 0) image into one buffer, stripping the
 * leading flag byte and trailing XOR checksum from each block. This is a
 * tolerant walker for "extract the loaded game image" use: it handles the
 * common block types and stops at anything it doesn't recognise, rather than
 * failing the whole tape the way the full libspectrum parser can.
 *
 * Returns a malloc'd buffer (caller frees) and sets *out_len, or NULL on
 * error / no data blocks. If block_offsets, block_lengths and num_blocks are
 * non-NULL, the offset and length of each emitted payload are recorded there
 * (up to max_blocks entries) so callers can locate individual blocks within
 * the result. */
uint8_t *ExtractTapePayloads(const uint8_t *raw, size_t raw_len, int is_tzx,
    size_t *out_len, size_t *block_offsets, size_t *block_lengths,
    int *num_blocks, int max_blocks);

/* --- Alkatraz copy-protection decryption (Appleby Associates) ---
 * Shared by the ScottFree (scott) and TaylorMade (taylor) interpreters. See
 * the implementations in decompressz80.c for details.
 *
 * ldir/lddr emulate the Z80 block-copy instructions on a memory image;
 * DeAlkatraz reverses the rolling-key XOR (selfmodify!=0 for Temple of Terror);
 * DeshuffleAlkatraz unscrambles the relocated blocks. */
void ldir(uint8_t *mem, uint16_t target, uint16_t source, uint16_t size);
void lddr(uint8_t *mem, uint16_t target, uint16_t source, uint16_t size);
uint8_t *DeAlkatraz(uint8_t *src, uint8_t *dst, size_t src_offset,
    uint16_t target, uint16_t count, uint8_t *key, uint8_t key_adjust,
    uint8_t key_step, int selfmodify);
void DeshuffleAlkatraz(uint8_t *mem, uint8_t repeats, uint16_t ix, uint16_t store);

#endif /* decompressz80_h */
