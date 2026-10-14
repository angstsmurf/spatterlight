//
//  extract_apple_2.cpp
//  bocfel
//
//  Created by Administrator on 2023-07-16.
//
//  Partially inspired by ZCut by Stefan Jokisch

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>

#include <algorithm>
#include <string>

#include "debugprint.h"
#include "ciderpress.h"
#include "read_le16.h"
#include "woz2nib.h"
#include "memory_allocation.hpp"
#include "v6_image.h"
#include "extract_image_data.hpp"
#include "extract_apple_2.h"

static const size_t kProDOSBlockSize = 512;
static const int kMaxDiskSides = 5;
static const size_t kMaxV6StorySize = 0x80000;

static const int kZHeaderSize = 64;
static const int kZHeaderFileLengthOffset = 26;
static const int kV6FileLengthMultiplier = 8;

static const size_t kBlockTableSize = 1024;
static const size_t kBlockTableHalfSize = 512;

// Block table layout offsets. Infocom's Apple II interpreter sources call
// this the segment table (the SGT* equates in zip.equ). It starts with its
// own length in words, not counting that first word, then the number of
// disks, then 16 unused bytes, and then one section per disk side.
static const int kBTDiskCountOffset = 2;
static const int kBTFirstDiskOffset = 20;
static const int kBTDiskHeaderSize = 8;
// A disk's section header is four words: a checksum, the block number
// where its graphics data starts, how many range entries follow the
// header, and the block number of the global picture directory, which says
// which sides each picture is on. Only side 2 of Arthur, Journey and Shogun
// has a global directory, and we have no use for it, as we read the
// pictures of every side up front.
static const int kBTGraphicsOffset = 2;
static const int kBTEntryCountOffset = 4;
static const int kBTRangeEntrySize = 6;

// Everything read from one game's set of disk sides.
struct AppleDiskSet {
    // Extracted Infocom file data for each disk side, indexed 0-4 for
    // sides 1-5.
    uint8_t *images[kMaxDiskSides];
    size_t image_lengths[kMaxDiskSides];
    // How many sides the game spans. Zero until the sides have been loaded.
    int side_count;

    // The block table is read from the first 512 (or 1024) bytes of disk 1.
    // It describes how logical blocks in the story file map to physical blocks
    // across multiple disk sides, and where graphics data begins on each disk.
    uint8_t block_table[kBlockTableSize];
    bool table_is_set;
};

static AppleDiskSet disks;

// Reads an Apple II disk image file and extracts the Infocom V6 data from it.
// Handles the full pipeline: if the file is in WOZ format, it is first converted
// to nibble format, then the CiderPress routines extract the Infocom file data.
// On success, returns a malloc'd buffer with the extracted data and sets *game
// (game identifier), *index (disk side number, 1-based), and *file_length.
static uint8_t *read_infocom_data_from_disk(const char *filename, int *game, int *index, size_t *file_length) {
    struct stat st;
    if (stat(filename, &st) != 0 || st.st_size <= 0) {
        fprintf(stderr, "Failed to stat file \"%s\".\n", filename);
        return nullptr;
    }
    *file_length = (size_t)st.st_size;

    FILE *fp = fopen(filename, "rb");
    if (fp == nullptr) {
        fprintf(stderr, "Failed to open file \"%s\".\n", filename);
        return nullptr;
    }

    uint8_t *entire_file = (uint8_t *)MemAlloc(*file_length);

    size_t actual_size = fread(entire_file, 1, *file_length, fp);
    fclose(fp);

    if (actual_size != *file_length) {
        fprintf(stderr, "Read error: expected %zu, got %zu\n", *file_length, actual_size);
        free(entire_file);
        *file_length = 0;
        return nullptr;
    }

    if (is_woz_image(entire_file, *file_length)) {
        size_t woz_length = *file_length;
        uint8_t *nib_data = woz2nib(entire_file, &woz_length);
        if (nib_data) {
            free(entire_file);
            entire_file = nib_data;
            *file_length = woz_length;
            InitNibImage(entire_file, *file_length);
        } else {
            fprintf(stderr, "woz2nib could not convert file to nib format!\n");
            free(entire_file);
            *file_length = 0;
            return nullptr;
        }
    }

    uint8_t *extracted_data = ReadInfocomV6File(entire_file, file_length, game, index);
    free(entire_file);
    return extracted_data;
}

static uint16_t read_block_table_word(int offset) {
    if (offset < 0 || offset + 1 >= (int)sizeof(disks.block_table))
        return 0;
    return READ_BE_UINT16(disks.block_table + offset);
}

// Reads the block mapping table from the first 512 bytes of disk 1.
// The first word is the length in words of the rest of the table. If that
// doesn't fit in 512 bytes, the table spans 1024 bytes.
static bool initialize_block_table(void) {
    uint8_t *first_disk = disks.images[0];
    if (first_disk == nullptr) {
        fprintf(stderr, "Apple 2 disk data not initialized!\n");
        return false;
    }
    memset(disks.block_table, 0, kBlockTableSize);
    if (disks.image_lengths[0] < kBlockTableHalfSize)
        return false;
    memcpy(disks.block_table, first_disk, kBlockTableHalfSize);

    size_t table_length = 2 + (size_t)READ_BE_UINT16(disks.block_table) * 2;
    if (table_length > kBlockTableHalfSize && disks.image_lengths[0] >= kBlockTableSize) {
        memcpy(disks.block_table + kBlockTableHalfSize, first_disk + kBlockTableHalfSize, kBlockTableHalfSize);
    }
    disks.table_is_set = true;
    return true;
}

static void free_disk_images(void) {
    for (int i = 0; i < kMaxDiskSides; i++) {
        free(disks.images[i]);
        disks.images[i] = nullptr;
        disks.image_lengths[i] = 0;
    }
    disks.table_is_set = false;
}

// Find the side number `side` in a file name. Search from the end, so that
// e.g. the year in "Arthur 1989 side 1.woz" isn't picked, and prefer a
// lone digit over one that is part of a longer number. Only the last path
// component is searched. Returns std::string::npos if the digit is not there.
static size_t find_side_digit_pos(const std::string &path, int side) {
    size_t separator_pos = path.find_last_of("/\\:");
    size_t name_start = separator_pos == std::string::npos ? 0 : separator_pos + 1;

    size_t found = std::string::npos;
    for (size_t i = path.size(); i > name_start; i--) {
        size_t pos = i - 1;
        if (path[pos] - '0' != side)
            continue;
        bool digit_before = pos > 0 && isdigit((unsigned char)path[pos - 1]);
        bool digit_after = pos + 1 < path.size() && isdigit((unsigned char)path[pos + 1]);
        if (!digit_before && !digit_after)
            return pos;
        if (found == std::string::npos)
            found = pos;
    }
    return found;
}

// Loads all disk sides for the game, starting from the given filename.
// Extracts the first disk, determines how many sides the game spans,
// then locates and loads the remaining sides by replacing the disk number
// digit in the filename (e.g. "Shogun - Side 1.woz" → "Shogun - Side 2.woz").
static int populate_disk_images(const char *filename, size_t *file_length) {
    if (disks.images[0] != nullptr)
        return disks.side_count;
    int game_id = -1, disk_index;

    uint8_t *extracted_disk = read_infocom_data_from_disk(filename, &game_id, &disk_index, file_length);
    if (extracted_disk == nullptr)
        return 0;

    // Game 0 (Zork Zero) comes on four sides, the others on five.
    disks.side_count = (game_id == 0) ? kMaxDiskSides - 1 : kMaxDiskSides;

    if (disk_index < 1 || disk_index > kMaxDiskSides) {
        fprintf(stderr, "Invalid disk index %d\n", disk_index);
        free(extracted_disk);
        return 0;
    }
    disks.images[disk_index - 1] = extracted_disk;
    disks.image_lengths[disk_index - 1] = *file_length;

    std::string alternate_path = filename;
    size_t disk_number_pos = find_side_digit_pos(alternate_path, disk_index);
    if (disk_number_pos == std::string::npos)
        return disks.side_count;

    for (int i = 0; i < disks.side_count; i++) {
        if (i == disk_index - 1 || disks.images[i] != nullptr)
            continue;
        alternate_path[disk_number_pos] = '1' + i;
        int parsed_index;
        disks.images[i] = read_infocom_data_from_disk(alternate_path.c_str(), nullptr, &parsed_index, file_length);
        if (disks.images[i] == nullptr) {
            fprintf(stderr, "Could not extract file from disk %d!\n", i + 1);
            free_disk_images();
            return 0;
        }
        if (parsed_index != i + 1) {
            fprintf(stderr, "Disk %d contains side %d, expected side %d\n", i + 1, parsed_index, i + 1);
        }
        disks.image_lengths[i] = *file_length;
    }
    return disks.side_count;
}

// The block table holds one section per disk side: an 8-byte header followed
// by that side's range entries. Returns the offset in the table of the header
// for side `disk` (0-based), or -1 if the table has no such side.
static int disk_table_offset(int disk) {
    if (disk < 0 || disk >= read_block_table_word(kBTDiskCountOffset))
        return -1;
    int table_offset = kBTFirstDiskOffset;
    for (int i = 0; i < disk; i++) {
        uint16_t entries = read_block_table_word(table_offset + kBTEntryCountOffset);
        table_offset += kBTDiskHeaderSize + kBTRangeEntrySize * entries;
    }
    return table_offset;
}

// Maps a logical block number (sequential position in the story file) to a
// physical block on a specific disk side. Walks the block table's per-disk
// range entries, each describing a contiguous range of logical blocks and the
// corresponding physical block base on that disk. If no range holds the
// block, *disk is set to the number of disks in the table.
static void resolve_logical_block(uint16_t logical_block, int *disk, uint16_t *physical_block) {
    int disk_count = read_block_table_word(kBTDiskCountOffset);

    for (*disk = 0; *disk < disk_count; (*disk)++) {
        int table_offset = disk_table_offset(*disk);
        uint16_t entries = read_block_table_word(table_offset + kBTEntryCountOffset);
        table_offset += kBTDiskHeaderSize;

        while (entries--) {
            uint16_t range_start = read_block_table_word(table_offset + 0);
            uint16_t range_end = read_block_table_word(table_offset + 2);
            uint16_t physical_base = read_block_table_word(table_offset + 4);
            table_offset += kBTRangeEntrySize;

            if (range_start <= logical_block && logical_block <= range_end) {
                *physical_block = logical_block + physical_base - range_start;
                return;
            }
        }
    }
}

// Returns the byte offset where graphics data begins on the given disk side
// (1-based), or 0 if there is none. It is in the header of that disk's
// section of the block table.
static off_t find_graphics_data_offset(int disk) {
    int table_offset = disk_table_offset(disk - 1);
    if (table_offset < 0)
        return 0;
    return read_block_table_word(table_offset + kBTGraphicsOffset) * kProDOSBlockSize;
}

// Extracts graphics images from all disk sides and merges them into a single
// flat array. Each disk may contain its own set of images at a different offset.
// The per-disk ImageStruct arrays are freed after their contents are copied out.
// *version is the release number of the first side that has one: sides 3-5
// of Journey have 0 there, where side 2 has the real number.
static int collect_images_from_disks(ImageStruct **output_images, int *version, int numdisks) {
    ImageStruct *per_disk_images[kMaxDiskSides];

    int per_disk_count[kMaxDiskSides];
    int total_count = 0;
    int first_version = 0;

    for (int i = 0; i < numdisks; i++) {
        off_t offset = find_graphics_data_offset(i+1);
        if (offset != 0) {
            // Trying to extract images from disk i + 1
            GraphicsType type = kGraphicsTypeApple2;
            int disk_version = 0;
            per_disk_count[i] = extract_images(disks.images[i], disks.image_lengths[i], i + 1, offset, &per_disk_images[i], &disk_version, &type);
            total_count += per_disk_count[i];
            if (first_version == 0)
                first_version = disk_version;
        } else {
            per_disk_count[i] = 0;
            per_disk_images[i] = nullptr;
        }
    }

    *version = first_version;

    int output_index = 0;
    if (total_count == 0) {
        for (int i = 0; i < numdisks; i++)
            free(per_disk_images[i]);
        return 0;
    }
    *output_images = (ImageStruct *)MemCalloc(total_count *  sizeof(ImageStruct));
    for (int i = 0; i < numdisks; i++) {
        for (int j = 0; j < per_disk_count[i]; j++) {
            (*output_images)[output_index] = per_disk_images[i][j];
            (*output_images)[output_index].type = kGraphicsTypeApple2;
            output_index++;
        }
        free(per_disk_images[i]);
    }
    return output_index;
}

// Copies one physical block (512 bytes) from a disk image to `dest`. Clamps
// the copy to the smaller of: the remaining disk data, `space` (the room left
// at dest), and the standard 512-byte block size. Returns the bytes copied.
static size_t transfer_block(int disk, int physical_block, uint8_t *dest, size_t space) {
    if (disk < 0 || disk >= kMaxDiskSides || disks.images[disk] == nullptr)
        return 0;
    if (physical_block < 0)
        return 0;

    size_t read_offset = (size_t)physical_block * kProDOSBlockSize;
    if (read_offset >= disks.image_lengths[disk])
        return 0;

    size_t available_from_disk = disks.image_lengths[disk] - read_offset;
    size_t bytes_to_copy = std::min({kProDOSBlockSize, available_from_disk, space});

    memcpy(dest, disks.images[disk] + read_offset, bytes_to_copy);
    return bytes_to_copy;
}

// Assembles the complete Z-machine story file by reading logical blocks in
// order, resolving each to a physical block on one of the disk sides, and
// copying them into a contiguous output buffer. The story size is read from
// the Z-machine header at offset 26 (file length in 8-byte units).
static uint8_t *assemble_story_from_disks(size_t *out_story_size) {
    *out_story_size = 0;

    if (!disks.table_is_set && !initialize_block_table())
        return nullptr;

    int disk;
    uint16_t physical_block = 0;

    resolve_logical_block(0, &disk, &physical_block);

    if (disk < 0 || disk >= kMaxDiskSides || disks.images[disk] == nullptr)
        return nullptr;

    size_t header_offset = (size_t)physical_block * kProDOSBlockSize;
    if (header_offset + kZHeaderSize > disks.image_lengths[disk])
        return nullptr;

    const uint8_t *story_header = disks.images[disk] + header_offset;
    size_t total_story_size = READ_BE_UINT16(story_header + kZHeaderFileLengthOffset) * kV6FileLengthMultiplier;

    if (total_story_size == 0 || total_story_size > kMaxV6StorySize) {
        fprintf(stderr, "Disk image has bad format. Story size %zx\n", total_story_size);
        return nullptr;
    }

    uint8_t *assembled_story = (uint8_t *)MemAlloc(total_story_size);
    size_t write_offset = 0;

    int num_disks = read_block_table_word(kBTDiskCountOffset);

    for (uint32_t logical_block = 0; write_offset < total_story_size; logical_block++) {
        if (logical_block > 0xffff)
            break;

        resolve_logical_block((uint16_t)logical_block, &disk, &physical_block);

        if (disk >= num_disks)
            break;

        write_offset += transfer_block(disk, physical_block, assembled_story + write_offset, total_story_size - write_offset);
    }

    *out_story_size = write_offset;
    return assembled_story;
}

// Public entry point: extracts the Z-machine story file from Apple II disk
// images, and optionally extracts graphics images as well.
// Returns a malloc'd buffer containing the assembled story data.
uint8_t *extract_apple2_story(const char *filename, size_t *file_length, ImageStruct **rawimg, int *num_images, int *version) {

    int numdisks = populate_disk_images(filename, file_length);
    if (numdisks == 0) {
        free_disk_images();
        return nullptr;
    }
    uint8_t *story_data = assemble_story_from_disks(file_length);

    if (rawimg != nullptr) {
        *num_images = collect_images_from_disks(rawimg, version, numdisks);
    }

    return story_data;
}

// Public entry point: extracts only the graphics images from Apple II disk
// images, without assembling the story file.
int extract_apple2_disk_images(const char *filename, ImageStruct **rawimg, int *version) {
    size_t unused_length;

    int numdisks = populate_disk_images(filename, &unused_length);
    if (numdisks == 0) {
        free_disk_images();
        return 0;
    }

    if (!initialize_block_table()) {
        free_disk_images();
        return 0;
    }

    return collect_images_from_disks(rawimg, version, numdisks);
}
