/*
 * WOZ to NIB converter.
 *
 * Converts WOZ 1.x and 2.x disk image files into raw nibble (.nib) format.
 * WOZ is a modern Apple II disk image format that stores raw flux/bit data;
 * NIB is the simpler 6656-bytes-per-track nibble format used by emulators.
 *
 * The conversion process:
 *   1. Parse the WOZ container (INFO, TMAP, TRKS chunks)
 *   2. For each track, locate sync byte sequences to find sector alignment
 *   3. Extract nibble bytes from the raw bitstream
 *   4. Pad or trim to exactly 6656 nibbles per track
 *
 * Created by Petter Sjölund on 2022-09-09.
 * Based on woz2dsk.pl by LEE Sau Dan <leesaudan@gmail.com>
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debugprint.h"
#include "minmax.h"
#include "read_le16.h"

#include "woz2nib.h"

/* Standard Apple II 5.25" disk geometry */
#define STANDARD_TRACKS_PER_DISK 35
#define NIBBLES_PER_TRACK 6656

/* WOZ file chunk types */
typedef enum {
    INFO,   // disk metadata (version, type, creator)
    TMAP,   // track map (maps quarter-tracks to TRKS entries)
    TRKS,   // track bitstream data
    WRIT,   // write-protect info (ignored)
    META,   // key-value metadata strings
    NUM_CHUNK_TYPES
} ChunkType;

static const char *chunktypes[NUM_CHUNK_TYPES + 1] = { "INFO", "TMAP", "TRKS", "WRIT", "META", NULL };

/* WOZ container format constants */
#define WOZ_MAGIC_SIZE 4
#define WOZ_HEADER_SIZE 12          /* 4 magic + 4 fixed + 4 CRC */
#define WOZ_CHUNK_HEADER_SIZE 8     /* 4 type ID + 4 size */
#define WOZ_BLOCK_SIZE 512

/* WOZ quarter-track limits */
#define MAX_QUARTER_TRACKS 160
#define TMAP_EMPTY_TRACK 0xFF

/* WOZ 1.x track record layout */
#define WOZ1_TRACK_DATA_SIZE 6646   /* bitstream data portion */
#define WOZ1_TRACK_RECORD_SIZE 6656 /* data + 10-byte trailer */

/* WOZ 2.x TRKS entry layout */
#define WOZ2_TRKS_ENTRY_SIZE 8

/* INFO chunk layout */
#define INFO_CHUNK_SIZE 60
#define INFO_CREATOR_OFFSET 5
#define INFO_CREATOR_MAX_LENGTH 32
#define INFO_DISK_SIDES_OFFSET 37

/* Sync pattern lengths in bits (used to rotate past the pattern) */
#define SYNC16_BIT_LENGTH 50
#define SYNC13_BIT_LENGTH 72

/* Per-track location of the raw bitstream within the WOZ file.
 * Both WOZ 1.x and 2.x store their track data inside the caller's buffer; we
 * record where each track begins and copy it out on demand at conversion time. */
typedef struct {
    size_t data_offset;         // byte offset into woz_data where the bitstream begins
    uint32_t bit_count;         // total number of valid bits in the track
} trksdata;

/* CRC-32 lookup table for WOZ file integrity verification */
static const uint32_t crc32_tab[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static uint32_t crc32(uint32_t crc, const void *buf, size_t size)
{
    const uint8_t *p;
    p = buf;
    crc = ~crc;
    while (size--)
        crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

static void FatalError(char *format, ...)
{
    va_list argp;

    fprintf(stderr, "\nFatal: ");

    va_start(argp, format);
    vfprintf(stderr, format, argp);
    va_end(argp);

    fprintf(stderr, "\n");

    exit(1);
}

static void *MemAlloc(size_t size)
{
    void *t = (void *)malloc(size);
    if (t == NULL)
        FatalError("Out of memory");
    return (t);
}

/* Rotate a byte left by one bit, shifting in the carry from a previous rotation */
static int rotate_left_with_carry(uint8_t *byte, int last_carry)
{
    int carry = ((*byte & 0x80) > 0);
    *byte = *byte << 1;
    if (last_carry)
        *byte |= 0x1;
    return carry;
}

/*
 * Extract the next nibble byte from a bitstream.
 *
 * Scans forward from *pos, skipping zero bits until a '1' bit is found,
 * then collects that bit plus the next 7 to form an 8-bit nibble.
 * The consumed bits are zeroed out in the bitstream. Advances *pos.
 */
static uint8_t extract_nibble(uint8_t *bitstream, int total_bits, int *pos)
{
    int total_bytes = total_bits / 8 + (total_bits % 8 != 0);
    // skip any initial '0'.
    while (*pos < total_bytes && bitstream[*pos] == 0) {
        (*pos)++;
    }

    if (*pos >= total_bits / 8) {
        // bitstream contained only zeros until no full byte left.
        return 0;
    }

    int carry = 0;
    int shift_count = 0;
    while ((bitstream[*pos] & 0x80) == 0) {
        carry = rotate_left_with_carry(&bitstream[*pos + 1], carry);
        carry = rotate_left_with_carry(&bitstream[*pos], carry);
        shift_count++;
    }
    uint8_t nibble = bitstream[*pos];
    bitstream[*pos] = 0;
    bitstream[*pos + 1] = bitstream[*pos + 1] >> shift_count;
    return nibble;
}

/*
 * Rotate a bitstream by moving 'head_bits' bits from the front to the end.
 *
 * Used to align the track bitstream so that it starts at a sector boundary
 * (just after sync bytes). This is a circular rotation at the bit level,
 * handling non-byte-aligned boundaries.
 */
static uint8_t *swap_head_and_tail(uint8_t *bitstream, int head_bits, int total_bits)
{

    int trailing_bits = total_bits % 8;
    int has_trailing_bits = (trailing_bits != 0);
    int total_bytes = total_bits / 8 + has_trailing_bits;
    int head_trailing = head_bits % 8;
    int has_head_trailing = (head_trailing != 0);
    int tail_bits = total_bits - head_bits;
    int tail_trailing = tail_bits % 8;
    int head_bytes = head_bits / 8 + has_head_trailing;

    uint8_t last_byte = bitstream[total_bytes - 1];

    if (has_trailing_bits)
        last_byte = (bitstream[total_bytes - 2] << trailing_bits) | (last_byte >> (8 - trailing_bits));

    int tail_bytes = tail_bits / 8 + 2;

    // if there are spillover head bits, the tail copy must
    // include the last head byte,
    // then be left shifted (head_trailing) times

    uint8_t *tail = MemAlloc(tail_bytes);

    if (!has_head_trailing) {
        // If the number of head bits are cleanly divisible by 8 we don't need to do any shifting
        memcpy(tail, bitstream + head_bytes, tail_bytes - 1);
    } else {
        for (int i = 0; i < tail_bytes && head_bytes + i < total_bytes; i++) {
            tail[i] = (bitstream[head_bytes - 1 + i] << head_trailing) | (bitstream[head_bytes + i] >> (8 - head_trailing));
        }
    }

    // if there are spillover tail bits, the head copy must
    // then be right shifted (tail_trailing) times,
    // while shifting in the original last tail bits
    // we calculated as last_byte above

    uint8_t *head = MemAlloc(head_bytes + 1);

    head[0] = (bitstream[0] >> tail_trailing) | (last_byte << (8 - tail_trailing));
    for (int i = 1; i <= head_bytes; i++) {
        head[i] = (bitstream[i] >> tail_trailing) | (bitstream[i - 1] << (8 - tail_trailing));
    }

    memcpy(bitstream, tail, tail_bytes);
    memcpy(bitstream + tail_bits / 8, head, head_bytes + 1);

    free(head);
    free(tail);

    return bitstream;
}

/* Result of searching for sector sync byte patterns */
typedef enum {
    FOUND_NONE,
    FOUND16,    // 16-sector (DOS 3.3) sync pattern
    FOUND13,    // 13-sector (DOS 3.2) sync pattern
} SearchResultType;

// clang-format off

/*
 * Sync byte patterns that precede sector address fields.
 * These are sequences of self-sync bytes (10-bit patterns that always
 * produce valid nibble bytes regardless of bit alignment).
 */
static const uint8_t sync_pattern_16[7] = {
    0xff, //    11111111
    0x3f, //    00111111
    0xcf, //    11001111
    0xf3, //    11110011
    0xfc, //    11111100
    0xff, //    11111111
    0x00  //    00
};

// clang-format on

static const uint8_t sync_pattern_13[9] = {
    0xff, //    11111111
    0x7f, //    01111111
    0xbf, //    10111111
    0xdf, //    11011111
    0xef, //    11101111
    0xf7, //    11110111
    0xfb, //    11111011
    0xfd, //    11111101
    0xfe, //    11111110
};

#define SYNC16_PATTERN_LENGTH (int)(sizeof(sync_pattern_16))
#define SYNC13_PATTERN_LENGTH (int)(sizeof(sync_pattern_13))
#define MAX_SYNC_PATTERN_LENGTH SYNC13_PATTERN_LENGTH

/*
 * Check if the bytes at 'bitstream' match either the 16-sector or
 * 13-sector sync byte pattern. Assumes at least MAX_SYNC_PATTERN_LENGTH
 * bytes are available.
 */
static SearchResultType match_sync_pattern(uint8_t *bitstream)
{
    int may_be_16 = 1;
    int may_be_13 = 1;
    for (int i = 1; i < MAX_SYNC_PATTERN_LENGTH; i++) {
        // We know that the first byte is a matching 0xff
        bitstream++;
        if (may_be_13) {
            if (sync_pattern_13[i] != *bitstream) {
                may_be_13 = 0;
            } else if (i == SYNC13_PATTERN_LENGTH - 1) {
                return FOUND13;
            }
        }
        if (may_be_16) {
            if (i == SYNC16_PATTERN_LENGTH - 1 && (*bitstream & 0xc0) == 0) {
                return FOUND16;
            }
            if (i >= SYNC16_PATTERN_LENGTH || sync_pattern_16[i] != *bitstream) {
                may_be_16 = 0;
            }
        }
        if (may_be_16 + may_be_13 == 0)
            return FOUND_NONE;
    }
    return FOUND_NONE;
}

/*
 * Search the entire track bitstream for sync byte patterns.
 *
 * First tries byte-aligned matches (fast path), then falls back to
 * checking every possible bit offset (slow path). Sets *found_bit_pos
 * to the bit position where the pattern was found.
 */
static SearchResultType find_syncbytes(uint8_t *bitstream, int bit_count, int *found_bit_pos)
{
    int total_bytes = bit_count / 8;
    int search_limit = total_bytes - MAX_SYNC_PATTERN_LENGTH;
    SearchResultType result = FOUND_NONE;
    // Both sync byte sequences begin with 0xff, so first we do
    // a simple loop looking for that, without shifting any bits.
    // This is enough to find what we're looking for
    // in the majority of cases.
    for (int i = 0; i < search_limit; i++) {
        if (bitstream[i] == 0xff) {
            result = match_sync_pattern(&bitstream[i]);
            if (result != FOUND_NONE) {
                *found_bit_pos = i * 8;
                return result;
            }
        }
    }

    uint8_t shifted_bytes[MAX_SYNC_PATTERN_LENGTH];
    // Otherwise we will have to look harder, for byte pairs that can be left shifted to 0xff
    for (int i = 0; i < search_limit; i++) {
        uint8_t current_byte = bitstream[i];
        // Skip any bytes where the rightmost bit is unset
        // or the next byte has the leftmost bit unset
        if ((current_byte & 0x01) != 0 && (bitstream[i + 1] & 0x80) != 0) {
            for (int shift = 1; shift < 8; shift++) {
                // Check if this byte left-shifted (together with the following byte)
                // shift places becomes 0xff
                if ((uint8_t)(((bitstream[i] << shift) | (bitstream[i + 1] >> (8 - shift)))) == 0xff) {
                    // If so, copy the following bytes to a buffer left-shifted by shift positions,
                    // and compare them to the sync-byte sequences.
                    for (int k = MAX_SYNC_PATTERN_LENGTH - 1; k > 0; k--) {
                        shifted_bytes[k] = ((bitstream[i + k] << shift) | (bitstream[i + k + 1] >> (8 - shift)));
                    }
                    result = match_sync_pattern(shifted_bytes);
                    if (result != FOUND_NONE) {
                        *found_bit_pos = i * 8 + shift;
                        return result;
                    }
                }
            }
        }
    }
    return result;
}

/*
 * Convert a WOZ disk image (1.x or 2.x) to raw nibble format.
 *
 * On entry, woz_data points to the WOZ file data and *data_size is its size.
 * On return, *data_size is set to the output size (35 tracks x 6656 bytes).
 * Returns a newly allocated buffer containing the .nib data, or NULL on error.
 */
uint8_t *woz2nib(uint8_t *woz_data, size_t *data_size)
{
    int output_pos = 0;
    int nr_tracks = STANDARD_TRACKS_PER_DISK;
    size_t woz_size = *data_size;
    size_t read_pos = WOZ_HEADER_SIZE;
    char magic[WOZ_MAGIC_SIZE];
    uint8_t fixed[WOZ_MAGIC_SIZE];
    uint32_t stored_crc = 0;
    for (int i = 0; i < WOZ_MAGIC_SIZE; i++) {
        magic[i] = woz_data[i];
        fixed[i] = woz_data[WOZ_MAGIC_SIZE + i];
        stored_crc |= (uint32_t)woz_data[WOZ_MAGIC_SIZE * 2 + i] << (8 * i);
    }

    /* Verify WOZ magic number: "WOZ1" or "WOZ2" */
    if (magic[0] != 'W' || magic[1] != 'O' || magic[2] != 'Z' || (magic[3] != '1' && magic[3] != '2')) {
        debug_print("WOZ header not found\n");
        return NULL;
    }

    /* Fixed header bytes: 0xFF 0x0A 0x0D 0x0A (high-bit set LF CR LF) */
    if (fixed[0] != 0xff || fixed[1] != 0x0a || fixed[2] != 0x0d || fixed[3] != 0x0a) {
        debug_print("bad header\n");
        return NULL;
    }

    uint32_t computed_crc32 = crc32(0, woz_data + read_pos, woz_size - read_pos);

    if (computed_crc32 != stored_crc) {
        debug_print("CRC mismatch (stored=%08x; computed=%08x)\n", stored_crc, computed_crc32);
        return NULL;
    }

    uint8_t tmap[STANDARD_TRACKS_PER_DISK];
    int info_found = 0;
    int tmap_found = 0;
    int trks_found = 0;
    trksdata trks[MAX_QUARTER_TRACKS] = { 0 };

    /* Parse WOZ chunks sequentially */
    while (read_pos < woz_size) {
        char chunk_type_str[WOZ_MAGIC_SIZE + 1];
        uint32_t chunk_size = 0;
        for (int i = 0; i < WOZ_MAGIC_SIZE; i++) {
            chunk_type_str[i] = woz_data[read_pos + i];
            chunk_size += woz_data[read_pos + WOZ_MAGIC_SIZE + i] << (8 * i);
        }
        chunk_type_str[WOZ_MAGIC_SIZE] = '\0';
        if (read_pos + WOZ_CHUNK_HEADER_SIZE + (size_t)chunk_size > woz_size) {
            debug_print("chunk %s size %u extends beyond end of file\n", chunk_type_str, chunk_size);
            return NULL;
        }
        uint8_t *chunk_data = MemAlloc(chunk_size);
        read_pos += WOZ_CHUNK_HEADER_SIZE;
        size_t chunk_payload_offset = read_pos;   // file offset of this chunk's payload
        memcpy(chunk_data, woz_data + read_pos, chunk_size);
        read_pos += chunk_size;

        ChunkType chunk_id = NUM_CHUNK_TYPES;
        for (int i = 0; i < NUM_CHUNK_TYPES; i++) {
            if (strncmp(chunk_type_str, chunktypes[i], WOZ_MAGIC_SIZE) == 0) {
                chunk_id = i;
                break;
            }
        }

        switch (chunk_id) {
        case INFO:
            if (chunk_size != INFO_CHUNK_SIZE) {
                debug_print("INFO chunk size is not %d but %d bytes\n", INFO_CHUNK_SIZE, chunk_size);
                free(chunk_data);
                return NULL;
            }
            info_found = 1;
            uint8_t version = chunk_data[0];
            uint8_t disk_type = chunk_data[1];
            if (disk_type != 1) {
                debug_print("Only 5.25\" disks are supported\n");
                free(chunk_data);
                return NULL;
            }
            char creator[INFO_CREATOR_MAX_LENGTH];
            memcpy(creator, chunk_data + INFO_CREATOR_OFFSET, INFO_CREATOR_MAX_LENGTH);
            creator[INFO_CREATOR_MAX_LENGTH - 1] = '\0';
            if (version >= 2) {
                uint8_t disk_sides = chunk_data[INFO_DISK_SIDES_OFFSET];
                if (disk_sides != 1) {
                    debug_print("Only 1-sided images are supported\n");
                    free(chunk_data);
                    return NULL;
                }
            }
            debug_print("INFO: ver %d, type ", version);
            switch (disk_type) {
            case 1:
                debug_print("5.25\"");
                break;
            case 2:
                debug_print("3.5\"");
                break;
            default:
                debug_print("unknown");
                break;
            }
            debug_print(", creator: %s\n", creator);
            debug_print("WOZ file version %d\n", version);
            break;
        case TMAP:
            tmap_found = 1;
            if (chunk_size != MAX_QUARTER_TRACKS) {
                debug_print("TMAP chunk size is not %d but %d bytes\n", MAX_QUARTER_TRACKS, chunk_size);
                free(chunk_data);
                return NULL;
            }
            uint8_t quarter_track_map[MAX_QUARTER_TRACKS];
            memcpy(quarter_track_map, chunk_data, MAX_QUARTER_TRACKS);
            debug_print("TMAP: ");
            for (int i = 0; i < nr_tracks; i++) {
                tmap[i] = quarter_track_map[i * 4];
                if (i > 0)
                    debug_print(",");
                debug_print("%d", tmap[i]);
            }
            debug_print("\n");
            break;
        case TRKS:
            trks_found = 1;
            debug_print("TRKS: %d bytes\n", chunk_size);
            if (magic[3] == '2') {
                /* WOZ 2.x: an array of 8-byte entries; each gives the track's
                 * starting block (absolute, from the start of the file) and bit count. */
                int entry_offset = 0;
                for (int i = 0; i < MAX_QUARTER_TRACKS; i++, entry_offset += WOZ2_TRKS_ENTRY_SIZE) {
                    uint16_t starting_block = READ_LE_UINT16(chunk_data + entry_offset);
                    trks[i].data_offset = (size_t)starting_block * WOZ_BLOCK_SIZE;
                    trks[i].bit_count = chunk_data[entry_offset + 4] + (chunk_data[entry_offset + 5] << 8) + (chunk_data[entry_offset + 6] << 16) + (chunk_data[entry_offset + 7] << 24);
                }
            } else {
                if (magic[3] != '1')
                    FatalError("bug");
                /* WOZ 1.x: fixed-size track records laid out back-to-back in the
                 * chunk; the bitstream sits at the start of each record, with the
                 * bit count in the trailer. Reference the data in place. */
                int remaining_bytes = chunk_size;
                int track_index = 0;
                size_t rec_offset = 0;
                while (remaining_bytes >= WOZ1_TRACK_RECORD_SIZE && track_index < MAX_QUARTER_TRACKS) {
                    trks[track_index].data_offset = chunk_payload_offset + rec_offset;
                    trks[track_index].bit_count = READ_LE_UINT16(chunk_data + rec_offset + WOZ1_TRACK_DATA_SIZE + 2);
                    rec_offset += WOZ1_TRACK_RECORD_SIZE;
                    remaining_bytes -= WOZ1_TRACK_RECORD_SIZE;
                    track_index++;
                }
            }
            break;
        case WRIT:
            debug_print("WRIT: %d bytes\n", chunk_size);
            // ignore WRIT chunks
            break;
        case META:
            debug_print("META: %d bytes\n", chunk_size);
            for (uint32_t i = 0; i < chunk_size; i++) {
                debug_print("%c", chunk_data[i]);
            }
            break;
        default:
            debug_print("ignoring unknown chunk: %s; size=%dbytes\n", chunk_type_str, chunk_size);
            break;
        }
        free(chunk_data);
    }

    if (info_found == 0 || tmap_found == 0 || trks_found == 0) {
        debug_print("missing required chunk (INFO=%d TMAP=%d TRKS=%d)\n", info_found, tmap_found, trks_found);
        return NULL;
    }

    size_t out_file_size = nr_tracks * NIBBLES_PER_TRACK;
    *data_size = out_file_size;

    uint8_t *outfile = MemAlloc(out_file_size);

    /* Convert each track's bitstream to nibble bytes */
    for (int track = 0; track < nr_tracks; track++) {
        int trks_index = tmap[track];
        if (trks_index == TMAP_EMPTY_TRACK) {
            debug_print("T[%02x]: empty track\n", track);
            memset(outfile + output_pos, 0, NIBBLES_PER_TRACK);
            output_pos += NIBBLES_PER_TRACK;
            continue;
        }
        if (trks_index >= MAX_QUARTER_TRACKS) {
            debug_print("T[%02x]: bad TMAP value %d\n", track, trks_index);
            free(outfile);
            return NULL;
        }

        trksdata *trk = &trks[trks_index];
        int bit_count = trk->bit_count;
        int total_bytes = bit_count / 8;
        size_t stream_bytes = (bit_count + 7) / 8;  // bytes spanned by the valid bits

        /* A track too short to hold even the sync pattern can't yield sectors,
         * and the rotation maths below assume a non-trivial bit count, so treat
         * it (and any unmapped/zero TRKS entry) as an empty track. */
        if (bit_count <= SYNC13_BIT_LENGTH) {
            debug_print("T[%02x]: track %d too short (%d bits)\n", track, trks_index, bit_count);
            memset(outfile + output_pos, 0, NIBBLES_PER_TRACK);
            output_pos += NIBBLES_PER_TRACK;
            continue;
        }

        if (trk->data_offset + stream_bytes > woz_size) {
            debug_print("track %02x bitstream (offset %zu, %zu bytes) extends beyond end of file (%zu)\n",
                        track, trk->data_offset, stream_bytes, woz_size);
            free(outfile);
            return NULL;
        }

        /* Copy the track into a private buffer so the destructive bit-rotation
         * and extraction below operate on our own scratch space and never touch
         * the caller's WOZ data. The +2 zeroed trailing bytes let the bit
         * scanners safely read one byte past the last valid byte. */
        uint8_t *bitstream = MemAlloc(stream_bytes + 2);
        memcpy(bitstream, woz_data + trk->data_offset, stream_bytes);
        bitstream[stream_bytes] = 0;
        bitstream[stream_bytes + 1] = 0;

        /* Find sync bytes and rotate bitstream so extraction starts at a sector boundary */
        int found_bit_pos = -1;
        SearchResultType result = find_syncbytes(bitstream, trk->bit_count, &found_bit_pos);
        if (result == FOUND_NONE) {
            /* Try again after rotating to expose a different part of the circular track */
            bitstream = swap_head_and_tail(bitstream, bit_count - SYNC13_BIT_LENGTH, bit_count);
            result = find_syncbytes(bitstream, trk->bit_count, &found_bit_pos);
        }

        switch (result) {
        case FOUND16:
            /* Rotate past the 16-sector sync pattern */
            bitstream = swap_head_and_tail(bitstream, found_bit_pos + SYNC16_BIT_LENGTH, bit_count);
            break;
        case FOUND13:
            /* Rotate past the 13-sector sync pattern */
            bitstream = swap_head_and_tail(bitstream, found_bit_pos + SYNC13_BIT_LENGTH, bit_count);
            break;
        default:
            debug_print("Found no sync bytes in track %d bitstream\n", track);
            break;
        }

        /* Extract nibble bytes from the aligned bitstream straight into the output */
        uint8_t *track_out = outfile + output_pos;
        int extract_pos = 0;
        int nibble_count = 0;
        do {
            track_out[nibble_count++] = extract_nibble(bitstream, bit_count, &extract_pos);
        } while (extract_pos < total_bytes && nibble_count < NIBBLES_PER_TRACK);

        if (nibble_count < NIBBLES_PER_TRACK) {
            memset(track_out + nibble_count, 0, NIBBLES_PER_TRACK - nibble_count);
            debug_print("padded to %d nibbles\n", NIBBLES_PER_TRACK);
        } else {
            debug_print("(just made!)\n");
        }

        free(bitstream);
        output_pos += NIBBLES_PER_TRACK;
    }

    return outfile;
}
