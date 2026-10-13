//
//  writetiff.cpp
//  bocfel6
//
//  Created by Administrator on 2023-05-22.
//
//  Minimal TIFF writer that emits a single-strip, uncompressed, big-endian
//  baseline TIFF containing an RGBA image. The layout is fixed: an 8-byte
//  header, the raw pixel data, then a 15-entry IFD describing it, then the
//  four per-channel arrays the IFD points to.
//

#include <cstdio>

#include "writetiff.hpp"

// TIFFs here are emitted as MM (big-endian) regardless of host byte order.
static void write16BE(FILE *fptr, uint16_t value) {
    putc(value >> 8, fptr);
    putc(value & 0xff, fptr);
}

static void write32BE(FILE *fptr, uint32_t value) {
    putc(value >> 24, fptr);
    putc((value >> 16) & 0xff, fptr);
    putc((value >> 8) & 0xff, fptr);
    putc(value & 0xff, fptr);
}

// IFD field types.
static const uint16_t kShort = 3;
static const uint16_t kLong = 4;

static const uint32_t kHeaderBytes = 8;
static const uint16_t kIfdEntries = 15;
// Entry count, twelve bytes per entry, offset of the next IFD.
static const uint32_t kIfdBytes = 2 + kIfdEntries * 12 + 4;
// Each of the per-channel arrays after the IFD holds four SHORTs.
static const uint32_t kChannels = 4;
static const uint32_t kChannelArrayBytes = kChannels * 2;

// An IFD entry is 12 bytes: tag id (2), field type (2), count (4), and a
// 4-byte field that holds the value itself, left-justified, when it fits,
// and otherwise the file offset of the values.
static void write_short_entry(FILE *fptr, uint16_t tag, uint16_t value) {
    write16BE(fptr, tag);
    write16BE(fptr, kShort);
    write32BE(fptr, 1);
    write16BE(fptr, value);
    write16BE(fptr, 0);
}

static void write_long_entry(FILE *fptr, uint16_t tag, uint32_t value) {
    write16BE(fptr, tag);
    write16BE(fptr, kLong);
    write32BE(fptr, 1);
    write32BE(fptr, value);
}

// An entry for one SHORT per channel, stored at file offset `offset`.
static void write_channel_array_entry(FILE *fptr, uint16_t tag, uint32_t offset) {
    write16BE(fptr, tag);
    write16BE(fptr, kShort);
    write32BE(fptr, kChannels);
    write32BE(fptr, offset);
}

static void write_channel_array(FILE *fptr, uint16_t value) {
    for (uint32_t i = 0; i < kChannels; i++)
        write16BE(fptr, value);
}

// Write a complete baseline TIFF describing an RGBA bitmap:
//   - `pixarray` points to width * height interleaved RGBA bytes
//     (4 bytes per pixel), totaling `pixarraysize` bytes.
//   - `width` is in pixels; height is derived as pixarraysize / (width * 4).
// Writes nothing if the dimensions cannot be represented, and stops early
// if the pixel write fails.
void writetiff(FILE *fptr, const uint8_t *pixarray, uint32_t pixarraysize, int width) {

    // The width and height are written as 16-bit SHORTs, so neither may
    // exceed 65535; bail rather than emit a truncated, malformed file. A
    // zero or negative width would also divide by zero below.
    if (width <= 0 || width > 0xffff)
        return;
    uint32_t full_height = pixarraysize / ((uint32_t)width * 4);
    if (full_height > 0xffff)
        return;
    uint16_t height = full_height;

    // Header: big-endian byte order mark, the TIFF identifier (42), and the
    // offset of the IFD, which follows the pixel data.
    write16BE(fptr, 0x4d4d);
    write16BE(fptr, 42);
    write32BE(fptr, kHeaderBytes + pixarraysize);

    if (fwrite(pixarray, 1, pixarraysize, fptr) != pixarraysize)
        return;

    // The per-channel arrays follow the IFD, in this order.
    const uint32_t bits_per_sample_offset = kHeaderBytes + pixarraysize + kIfdBytes;
    const uint32_t min_sample_offset = bits_per_sample_offset + kChannelArrayBytes;
    const uint32_t max_sample_offset = min_sample_offset + kChannelArrayBytes;
    const uint32_t sample_format_offset = max_sample_offset + kChannelArrayBytes;

    // The IFD. Entries must be in ascending tag order.
    write16BE(fptr, kIfdEntries);
    write_short_entry(fptr, 0x0100, (uint16_t)width);   // ImageWidth
    write_short_entry(fptr, 0x0101, height);            // ImageLength
    write_channel_array_entry(fptr, 0x0102, bits_per_sample_offset); // BitsPerSample
    write_short_entry(fptr, 0x0103, 1);                 // Compression: none
    write_short_entry(fptr, 0x0106, 2);                 // PhotometricInterpretation: RGB
    write_long_entry(fptr, 0x0111, kHeaderBytes);       // StripOffsets
    write_short_entry(fptr, 0x0112, 1);                 // Orientation: top left
    write_short_entry(fptr, 0x0115, kChannels);         // SamplesPerPixel
    write_short_entry(fptr, 0x0116, height);            // RowsPerStrip
    write_long_entry(fptr, 0x0117, pixarraysize);       // StripByteCounts
    write_channel_array_entry(fptr, 0x0118, min_sample_offset); // MinSampleValue
    write_channel_array_entry(fptr, 0x0119, max_sample_offset); // MaxSampleValue
    write_short_entry(fptr, 0x011c, 1);                 // PlanarConfiguration: chunky
    write_short_entry(fptr, 0x0152, 1);                 // ExtraSamples: associated alpha
    write_channel_array_entry(fptr, 0x0153, sample_format_offset); // SampleFormat
    write32BE(fptr, 0);                                 // No further IFDs.

    write_channel_array(fptr, 8);       // BitsPerSample
    write_channel_array(fptr, 0);       // MinSampleValue
    write_channel_array(fptr, 0xff);    // MaxSampleValue
    write_channel_array(fptr, 1);       // SampleFormat: unsigned integer
}
