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

#include "debugprint.h"
extern "C" {
#include "ciderpress.h"
#include "read_le16.h"
#include "woz2nib.h"
}
#include "memory_allocation.hpp"
#include "v6_image.h"
#include "extract_image_data.hpp"
#include "extract_apple_2.h"

// The block table is read from the first 512 (or 1024) bytes of disk 1.
// It describes how logical blocks in the story file map to physical blocks
// across multiple disk sides, and where graphics data begins on each disk.
static uint8_t block_table[1024];

// State for incrementally assembling the story file from disk blocks.
// remaining_bytes counts down from the total story size as blocks are copied;
// write_offset tracks the current position in the output buffer.
static size_t remaining_bytes;
static size_t total_story_size;

static uint8_t *assembled_story;
static size_t write_offset;

static bool table_is_set = false;

static size_t get_file_length(FILE *in) {
    if (fseek(in, 0, SEEK_END) == -1) {
        return 0;
    }
    long length = ftell(in);
    if (length == -1) {
        return 0;
    }
    fseek(in, 0, SEEK_SET);
    return (size_t)length;
}

// Reads an Apple II disk image file and extracts the Infocom V6 data from it.
// Handles the full pipeline: if the file is in WOZ format, it is first converted
// to nibble format, then the CiderPress routines extract the Infocom file data.
// On success, returns a malloc'd buffer with the extracted data and sets *game
// (game identifier), *index (disk side number, 1-based), and *file_length.
static uint8_t *read_infocom_data_from_disk(const char *filename, int *game, int *index, size_t *file_length){
    FILE *fp;

    if ((fp = fopen (filename, "rb")) == nullptr) {
        fprintf(stderr, "Failed to open file \"%s\".\n", filename);
        return nullptr;
    }

    *file_length = get_file_length(fp);
    if (*file_length == 0) {
        fprintf(stderr, "read_infocom_data_from_disk: file length 0!\n");
        fclose(fp);
        return nullptr;
    }

    uint8_t *entire_file = (uint8_t *)MemAlloc(*file_length);

    size_t actual_size = fread(entire_file, 1, *file_length, fp) ;
    if (actual_size != *file_length) {
        fprintf(stderr, "Expected %zu, got %zu\n", *file_length, actual_size);
    }

    fclose(fp);

    if (entire_file[0] == 'W' && entire_file[1] == 'O' && entire_file[2] == 'Z') {
        uint8_t *nib_data = woz2nib(entire_file, file_length);
        if (nib_data) {
            free(entire_file);
            entire_file = nib_data;
            InitNibImage(entire_file, *file_length);
        } else {
            fprintf(stderr, "woz2nib could not convert file to nib format!\n");
            free(entire_file);
            return nullptr;
        }
    }

    uint8_t *extracted_data = ReadInfocomV6File(entire_file, file_length, game, index);
    free(entire_file);
    return extracted_data;
}

static uint8_t *assemble_story_from_disks (size_t *out_story_size);

// Extracted Infocom file data for each disk side (up to 5 sides).
// Indexed 0-4, corresponding to disk sides 1-5.
uint8_t *disk_images[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
size_t disk_image_lengths[5] = { 0, 0, 0, 0, 0 };


static off_t find_graphics_data_offset(int disk);

// Reads the block mapping table from the first 512 bytes of disk 1.
// If the entry count (first word) exceeds 256, the table spans 1024 bytes.
static void initialize_block_table(void) {
    write_offset = 0;
    uint8_t *first_disk = disk_images[0];
    if (first_disk == nullptr) {
        fprintf(stderr, "Apple 2 disk data not initialized!\n");
        return;
    }
    memset(block_table, 0, 1024);
    if (disk_image_lengths[0] < 512)
        return;
    memcpy(block_table, disk_images[0], 512);

    if (READ_BE_UINT16(block_table) > 256 && disk_image_lengths[0] >= 1024) {
        memcpy(block_table + 512, first_disk + 512, 512);
    }
    table_is_set = true;
}

static void free_disk_images(void) {
    for (int i = 0; i < 5; i++) {
        if (disk_images[i] != nullptr) {
            free(disk_images[i]);
            disk_images[i] = nullptr;
        }
        disk_image_lengths[i] = 0;
    }
    table_is_set = false;
}

static int global_numdisks = 0;

// Loads all disk sides for the game, starting from the given filename.
// Extracts the first disk, determines how many sides the game spans,
// then locates and loads the remaining sides by replacing the disk number
// digit in the filename (e.g. "Shogun - Side 1.woz" → "Shogun - Side 2.woz").
static int populate_disk_images(const char *filename, size_t *file_length) {
    if (disk_images[0] != nullptr)
        return global_numdisks;
    size_t filenamelen = strlen(filename);
    int game_id = -1, disk_index;

    uint8_t *extracted_disk = read_infocom_data_from_disk(filename, &game_id, &disk_index, file_length);

    global_numdisks = 5 - (game_id == 0);

    if (extracted_disk) {
        if (disk_index < 1 || disk_index > 5) {
            fprintf(stderr, "Invalid disk index %d\n", disk_index);
            free(extracted_disk);
            return 0;
        }
        disk_images[disk_index - 1] = extracted_disk;
        disk_image_lengths[disk_index - 1] = *file_length;

        int separator_pos = 0;
        for (int i = (int)filenamelen - 1; i >= 0; i--) {
            char c = filename[i];
            if (c == '/' || c == '\\' || c == ':') {
                separator_pos = i;
                break;
            }
        }

        int disk_number_pos = -1;
        for (int i = separator_pos; i < filenamelen; i++) {
            if (filename[i] - '0' == disk_index) {
                disk_number_pos = i;
                break;
            }
        }

        char alternate_path[1024];

        if (filenamelen >= sizeof(alternate_path))
            return 0;
        memcpy(alternate_path, filename, filenamelen + 1);

        if (disk_number_pos >= 0) {
            for (int i = 0; i < global_numdisks; i++) {
                if (i != disk_index - 1) {
                    alternate_path[disk_number_pos] = '1' + i;
                    int parsed_index;
                    if (disk_images[i] == nullptr) {
                        disk_images[i] = read_infocom_data_from_disk(alternate_path, nullptr, &parsed_index, file_length);
                        disk_image_lengths[i] = *file_length;
                    }
                    if (disk_images[i] == nullptr) {
                        fprintf(stderr, "Could not extract file from disk %d!\n", i + 1);
                        free_disk_images();
                        return 0;
                    }
                }
            }
        }
        return global_numdisks;
    }
    return 0;
}

// Extracts graphics images from all disk sides and merges them into a single
// flat array. Each disk may contain its own set of images at a different offset.
// The per-disk ImageStruct arrays are freed after their contents are copied out.
static int collect_images_from_disks(ImageStruct **output_images, int *version, int numdisks) {
    ImageStruct *per_disk_images[5];

    int per_disk_count[5];
    int total_count = 0;

    for (int i = 0; i < numdisks; i++) {
        off_t offset = find_graphics_data_offset(i+1);
        if (offset != 0) {
            // Trying to extract images from disk i + 1
            GraphicsType type = kGraphicsTypeApple2;
            per_disk_count[i] = extract_images(disk_images[i], disk_image_lengths[i], i + 1, offset, &per_disk_images[i], version, &type);
            total_count += per_disk_count[i];
        } else {
            per_disk_count[i] = 0;
            per_disk_images[i] = nullptr;
        }
    }

    int output_index = 0;
    if (total_count == 0) {
        for (int i = 0; i < numdisks; i++)
            free(per_disk_images[i]);
        return 0;
    }
    *output_images = (ImageStruct *)MemCalloc(total_count *  sizeof(ImageStruct));
    for (int i = 0; i < numdisks; i++) {
        if (per_disk_count[i] > 0) {
            for (int j = 0; j < per_disk_count[i]; j++) {
                (*output_images)[output_index] = per_disk_images[i][j];
                (*output_images)[output_index].type = kGraphicsTypeApple2;
                output_index++;
            }
        }
        if (per_disk_images[i] != nullptr)
            free(per_disk_images[i]);
    }
    return output_index;
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

    initialize_block_table();

    int num_images = collect_images_from_disks(rawimg, version, numdisks);

    return num_images;
}

static uint16_t read_block_table_word (int offset) {
    if (offset < 0 || offset + 1 >= (int)sizeof(block_table))
        return 0;
    return READ_BE_UINT16(block_table + offset);
}


// Maps a logical block number (sequential position in the story file) to a
// physical block on a specific disk side. Walks the block table's per-disk
// range entries, each describing a contiguous range of logical blocks and the
// corresponding physical block base on that disk.
static void resolve_logical_block (uint16_t logical_block, int *disk, uint16_t *physical_block) {
    int table_offset = 20;
    uint16_t range_start = 0;
    uint16_t range_end = 0;

    for (*disk = 0; *disk < read_block_table_word(2); (*disk)++) {
        uint16_t entries = read_block_table_word(table_offset + 4);
        table_offset += 8;

        while (entries--) {
            range_start = read_block_table_word(table_offset + 0);
            range_end = read_block_table_word(table_offset + 2);
            uint16_t physical_base = read_block_table_word(table_offset + 4);
            table_offset += 6;

            if (range_start <= logical_block && logical_block <= range_end) {
                *physical_block = logical_block + physical_base - range_start;
                return;
            }
        }
    }
}

// Returns the byte offset where graphics data begins on the given disk side.
// For disk 1, this is stored directly in the block table. For other disks,
// it's found by skipping past each preceding disk's block range entries.
static off_t find_graphics_data_offset(int disk) {
    if (disk == 1) {
        return read_block_table_word(22) * 0x200;
    }

    int table_offset = 20;

    for (int current_disk = 1; current_disk < read_block_table_word(2); current_disk++) {

        uint16_t entries = read_block_table_word(table_offset + 4);

        table_offset += 8 + 6 * entries;

        if (current_disk + 1 == disk)
            return read_block_table_word(table_offset + 2) * 0x200;
    }
    return 0;
}

// Copies one physical block (512 bytes) from a disk image into the assembled
// story buffer. The remaining_bytes counter tracks progress in 256-byte units
// (matching the Z-machine header's file size field), while actual disk I/O
// operates on 512-byte blocks. Handles partial copies at disk/story boundaries.
static void transfer_block(int disk, int physical_block) {
    if (disk < 0 || disk >= 5 || disk_images[disk] == nullptr)
        return;
    int transfer_size = 256;

    if (remaining_bytes < transfer_size)
        transfer_size = (int) remaining_bytes;

    if (transfer_size != 0) {
        size_t read_offset = 512 * (physical_block);
        if (read_offset + 512 >= disk_image_lengths[disk] || write_offset + 512 >= total_story_size) {
            if (read_offset < disk_image_lengths[disk]) {
                size_t bytes_to_copy = disk_image_lengths[disk] - read_offset;
                if (write_offset + bytes_to_copy >= total_story_size) {
                    if (write_offset >= total_story_size)
                        return;
                    bytes_to_copy = total_story_size - write_offset;
                }
                memcpy(assembled_story + write_offset, disk_images[disk] + read_offset, bytes_to_copy);
                write_offset += bytes_to_copy;
            }
            return;
        }
        memcpy(assembled_story + write_offset, disk_images[disk] + read_offset, 512);
    }
    write_offset += 512;
    remaining_bytes -= transfer_size;
}

// Assembles the complete Z-machine story file by reading logical blocks in
// order, resolving each to a physical block on one of the disk sides, and
// copying them into a contiguous output buffer. The story size is read from
// the Z-machine header at offset 26 (file length in 256-byte units).
static uint8_t *assemble_story_from_disks(size_t *out_story_size) {
    if (!table_is_set)
        initialize_block_table();

    uint8_t story_header[64];

    int disk;
    uint16_t physical_block = 0;

    resolve_logical_block(0, &disk, &physical_block);

    memcpy(story_header, disk_images[disk] + 512 * physical_block, 64);
    remaining_bytes = READ_BE_UINT16(story_header + 26) * 8;

    if (remaining_bytes > 0x80000)
        fprintf(stderr, "Disk image has bad format. Story size %zx. Maxsize 0x80000\n", remaining_bytes);

    assembled_story = (uint8_t *)MemAlloc(remaining_bytes);
    total_story_size = remaining_bytes;
    *out_story_size = remaining_bytes;

    int num_disks = read_block_table_word (2);

    uint16_t logical_block = 0;

    while (remaining_bytes > 0) {
        resolve_logical_block(logical_block++, &disk, &physical_block);
        
        if (disk > num_disks - 1)
            break;

        transfer_block(disk, physical_block);
    }

    return assembled_story;
}
