//
//  writetiff.cpp
//  bocfel6
//
//  Created by Administrator on 2023-05-22.
//
//  Minimal TIFF writer that emits a single-strip, uncompressed, big-endian
//  baseline TIFF containing an RGBA image. The layout is fixed: an 8-byte
//  header, the raw pixel data, then a 15-entry IFD describing it. All IFD
//  tags are hard-coded as hex strings so the file structure mirrors the
//  TIFF 6.0 spec layout almost line-for-line.
//

#include "writetiff.hpp"

// Emit a sequence of bytes from a string of even-length ASCII hex digits.
// e.g. "4d4d002a" -> 0x4d 0x4d 0x00 0x2a. Used to write fixed TIFF tag
// constants whose values never change between calls.
static int hexnibble(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return 0;
}

static void writehexstring(FILE *fptr, const char *s)
{
    // These are all compile-time hex constants; decode two nibbles at a time
    // directly instead of paying full sscanf() machinery (and a per-iteration
    // strlen()) per byte.
    for (size_t i = 0; s[i] != '\0' && s[i + 1] != '\0'; i += 2) {
        int c = (hexnibble(s[i]) << 4) | hexnibble(s[i + 1]);
        putc(c, fptr);
    }
}

// Write a 32-bit unsigned integer in big-endian (network) byte order. TIFFs
// here are emitted as MM (big-endian) regardless of host byte order.
static void write32BE(FILE *fptr, uint32_t offset) {
    putc(offset >> 24, fptr);
    putc((offset >> 16) & 0xff, fptr);
    putc((offset >> 8) & 0xff, fptr);
    putc(offset & 0xff, fptr);
}

// Write a complete baseline TIFF describing an RGBA bitmap:
//   - `pixarray` points to width * height interleaved RGBA bytes
//     (4 bytes per pixel), totaling `pixarraysize` bytes.
//   - `width` is in pixels; height is derived as pixarraysize / (width * 4).
// The file is laid out as: 8-byte header, pixel data, 15-entry IFD, then
// a few trailing arrays referenced by the IFD (bits-per-sample, min/max
// values, sample formats). Silently returns if the pixel write fails.
void writetiff(FILE *fptr, uint8_t *pixarray, uint32_t pixarraysize, int width) {

    uint16_t height = pixarraysize / (width * 4);

    /* Write the header */
    writehexstring(fptr, "4d4d002a");    /* Big endian & TIFF identifier */
    // Offset to the IFD: it follows the 8-byte header + the pixel data.
    uint32_t offset = pixarraysize + 8;
    write32BE(fptr, offset);

    /* Write the binary data */
    if (fwrite(pixarray, 1, pixarraysize, fptr) != pixarraysize)
        return;

    // ---- IFD ----
    // Each TIFF tag is 12 bytes: tag id (2), data type (2), count (4),
    // value-or-offset (4). The hex strings below encode the first 8 bytes
    // of each tag; the trailing 4 bytes (the value/offset) are written
    // either inline or via write32BE depending on the tag.

    /* Write the footer */
    writehexstring(fptr, "000f");  /* The number of directory entries (15) */

    /* Width tag, short int */
    writehexstring(fptr, "0100000300000001");
    fputc((width >> 8) & 0xff, fptr);    /* Image width */
    fputc(width & 0xff, fptr);
    writehexstring(fptr, "0000");

    /* Height tag, short int */
    writehexstring(fptr, "0101000300000001");
    fputc((height >> 8) & 0xff, fptr);    /* Image height */
    fputc(height & 0xff, fptr);
    writehexstring(fptr, "0000");

    /* Bits per sample tag, short int */
    writehexstring(fptr, "0102000300000004");

    /* Offset of first byte after directory entries */
    offset = pixarraysize + 194;
    write32BE(fptr, offset);

    /* Compression flag, short int */
    writehexstring(fptr, "010300030000000100010000");

    /* Photometric interpolation tag, short int */
    writehexstring(fptr, "010600030000000100020000");

    /* Strip offset tag, long int */
    writehexstring(fptr, "011100040000000100000008");

    /* Orientation flag, short int */
    writehexstring(fptr, "011200030000000100010000");

    /* Sample per pixel tag, short int */
    writehexstring(fptr, "011500030000000100040000");

    /* Rows per strip tag, short int */
    writehexstring(fptr, "0116000300000001");
    fputc((height >> 8) & 0xff, fptr);    /* Image height */
    fputc(height & 0xff, fptr);
    writehexstring(fptr, "0000");

    /* Strip byte count flag, long int */
    writehexstring(fptr, "0117000400000001");
    write32BE(fptr, pixarraysize);

    /* Minimum sample value flag, short int */
    writehexstring(fptr, "0118000300000004");
    offset += 8;
    write32BE(fptr, offset);

    /* Maximum sample value tag, short int */
    writehexstring(fptr, "0119000300000004");
    offset += 8;
    write32BE(fptr, offset);

    /* Planar configuration tag, short int */
    writehexstring(fptr, "011c00030000000100010000");

    /* Extra samples tag, short int */
    writehexstring(fptr, "015200030000000100010000");

    /* Sample format tag, short int */
    writehexstring(fptr, "0153000300000004");
    offset += 8;
    write32BE(fptr, offset);

    /* End of the directory entry */
    // Next-IFD offset of zero — no further IFDs follow.
    writehexstring(fptr, "00000000");

    // ---- Trailing data arrays referenced by IFD offsets ----
    // BitsPerSample (tag 0x0102): one 16-bit value per channel, four
    // channels at 8 bits each (R, G, B, A).
    writehexstring(fptr, "0008000800080008");

    // MinSampleValue (tag 0x0118): minimum sample value per channel.
    writehexstring(fptr, "0000000000000000");

    // MaxSampleValue (tag 0x0119): maximum sample value per channel (0xff).
    writehexstring(fptr, "00ff00ff00ff00ff");

    // SampleFormat (tag 0x0153): one 16-bit value per channel; "1" =
    // unsigned integer for each of the four channels.
    writehexstring(fptr, "0001000100010001");
}
