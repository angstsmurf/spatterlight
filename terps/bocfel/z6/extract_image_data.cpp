//
//  extract_image_data.cpp
//  bocfel
//
//  Created by Administrator on 2023-07-17.
//
//  Extracts image data from Infocom-format graphics files (used by V6 Z-machine
//  games like Zork Zero, Arthur, Shogun, and Journey) and from Blorb resource
//  archives. The Infocom format stores images as compressed pixel data with
//  per-image or shared Huffman trees and color palettes.
//
//  Parts are inspired by Mark Howell's pix2gif utility


#include <stdlib.h>
#include <string.h>

extern "C" {
#include "glkimp.h"
#include "gi_blorb.h"
}

#include "types.h"
#include "v6_image.h"
#include "draw_png.h"

#include "extract_image_data.hpp"

// Header structure at the start of an Infocom graphics file.
// Multi-disk games split images across several files, each identified by disk_part.
typedef struct InfocomImageFileHeader {
    uint8_t disk_part;            // Which disk this file belongs to (1-based)
    uint8_t flags;                // Format flags; 0x0e indicates monochrome Mac graphics
    uint16_t unknown_1;
    uint16_t image_count;         // Total number of images described in the directory
    uint16_t unknown_2;
    uint8_t directory_entry_size; // Size in bytes of each directory entry (8, 14, or 16)
    uint8_t unknown_3;
    uint16_t checksum;
    uint16_t unknown_4;
    uint16_t version;             // Graphics format version number
} InfocomImageFileHeader;

// Per-image entry in the directory that follows the file header.
// Each entry describes one image's dimensions, location, and optional
// color map / Huffman tree within the graphics data.
typedef struct ImageDirectoryEntry {
    uint16_t id;              // Image resource number used by the game
    uint16_t width;
    uint16_t height;
    uint16_t flags;               // Bit 0: has transparency; upper bits encode transparent color index
    uint32_t pixel_data_offset;         // Byte offset of compressed pixel data within the file
    uint32_t next_entry_data_offset;    // Offset of the next image's data (used to compute this image's data length)
    uint32_t color_map_offset;          // Byte offset of this image's color palette (0 if shared/absent)
    uint32_t huffman_tree_offset;       // Byte offset of this image's Huffman decoding tree (0 if using default)
} ImageDirectoryEntry;


// Read a single byte from *ptr and advance the pointer by one.
static uint8_t read_byte_and_advance(uint8_t **ptr)
{
    uint8_t value = **ptr;
    *ptr += 1;
    return value;
}

// Read a 16-bit big-endian word from *ptr and advance by two bytes.
static uint16_t read_big_endian_word(uint8_t **ptr)
{
    uint16_t word = (uint16_t)read_byte_and_advance(ptr) << 8;
    word += (uint16_t)read_byte_and_advance(ptr);
    return word;
}

// Swap the bytes of a 16-bit value if should_swap is true.
// PC-format (VGA/EGA/CGA) graphics files store multi-byte fields
// in the opposite byte order from Amiga/Mac files.
static uint16_t conditional_byte_swap(bool should_swap, uint16_t value)
{
    if (should_swap)
        return ((value >> 8) | ((value & 0xff) << 8));
    else
        return value;
}

// Read a 24-bit value stored in big-endian order and return it as a uint32_t.
// Used for reading 3-byte file offsets in the image directory.
static uint32_t read_24_big_endian_bits(uint8_t **ptr)
{
    uint8_t upper_byte = read_byte_and_advance(ptr);
    uint8_t middle_byte = read_byte_and_advance(ptr);
    uint8_t lower_byte = read_byte_and_advance(ptr);

    return (uint32_t)(upper_byte << 16 | middle_byte << 8 | lower_byte);
}

// Allocate memory or terminate on failure.
static uint8_t *allocate_or_die(size_t size)
{
    uint8_t *buffer = (uint8_t *)malloc(size);
    if (buffer == nullptr) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    return buffer;
}

// Shared Huffman decoding tree used by images that don't specify their own tree.
// Populated from the data immediately following the image directory.
uint8_t default_huffman_tree[256];

// Parse an Infocom-format graphics file and extract all images into an ImageStruct array.
//
// Parameters:
//   data       - pointer to the entire graphics file loaded into memory
//   datasize   - total byte length of the graphics file
//   disk       - expected disk part number (files for the wrong disk are skipped)
//   offset     - byte offset within data where the header begins
//   image_list - receives the allocated array of ImageStruct results
//   version    - receives the graphics format version from the file header
//   type       - in/out graphics type; may be reclassified (e.g. Amiga -> Mac BW) based on header flags
//
// Returns the number of images extracted, or 0 if the file is for the wrong disk.
int extract_images(uint8_t *data, size_t datasize, int disk, off_t offset, ImageStruct **image_list, int *version, GraphicsType *type)
{
    int entry_index;
    uint8_t *ptr = data;
    InfocomImageFileHeader file_header;
    ImageDirectoryEntry *directory;

    ptr = data + offset;
    file_header.disk_part = read_byte_and_advance(&ptr);
    *image_list = nullptr;

    // Verify this file is for the requested disk; multi-disk games have
    // separate graphics files for each disk.
    if (file_header.disk_part != disk) {
        return 0;
    }

    // PC-format files (VGA/EGA/CGA) use swapped byte order for multi-byte fields
    bool needs_byte_swap = (*type == kGraphicsTypeVGA || *type == kGraphicsTypeEGA || *type == kGraphicsTypeCGA);

    file_header.flags = read_byte_and_advance(&ptr);

    // A file named "pic.data" may contain colour Amiga graphics or monochrome Mac graphics,
    // but the flags always seem to equal 0xe (14) if the graphics are monochrome.
    // (Amiga graphics data and Mac colour graphics data are identical.)
    if (file_header.flags == 0xe) {
        if (*type == kGraphicsTypeAmiga)
            *type = kGraphicsTypeMacBW;
    } else if (*type == kGraphicsTypeMacBW) {
        *type = kGraphicsTypeAmiga;
    }

    // Read remaining header fields, byte-swapping as needed for PC formats
    file_header.unknown_1 = conditional_byte_swap(needs_byte_swap, read_big_endian_word(&ptr));
    file_header.image_count = conditional_byte_swap(needs_byte_swap, read_big_endian_word(&ptr));
    file_header.unknown_2 = conditional_byte_swap(needs_byte_swap, read_big_endian_word(&ptr));
    file_header.directory_entry_size = read_byte_and_advance(&ptr);
    file_header.unknown_3 = read_byte_and_advance(&ptr);
    file_header.checksum = conditional_byte_swap(needs_byte_swap, read_big_endian_word(&ptr));
    file_header.unknown_4 = conditional_byte_swap(needs_byte_swap, read_big_endian_word(&ptr));
    file_header.version = conditional_byte_swap(needs_byte_swap, read_big_endian_word(&ptr));

    if ((directory = (ImageDirectoryEntry *)calloc((size_t)file_header.image_count, sizeof(ImageDirectoryEntry))) == nullptr) {
        fprintf(stderr, "Insufficient memory\n");
        exit(1);
    }

    // Parse the image directory. Entry size varies by format version:
    //   8 bytes:  compact format (single-byte width/height/flags)
    //  14 bytes:  standard format with color map offset
    //  16 bytes:  extended format with color map and per-image Huffman tree offset
    for (entry_index = 0; entry_index < file_header.image_count; entry_index++) {
        directory[entry_index].id = conditional_byte_swap(needs_byte_swap, read_big_endian_word(&ptr));
        if (file_header.directory_entry_size == 8) {
            // Compact entries store dimensions and flags as single bytes
            directory[entry_index].width = read_byte_and_advance(&ptr);
            directory[entry_index].height = read_byte_and_advance(&ptr);
            directory[entry_index].flags = read_byte_and_advance(&ptr);
        } else {
            directory[entry_index].width = conditional_byte_swap(needs_byte_swap, read_big_endian_word(&ptr));
            directory[entry_index].height = conditional_byte_swap(needs_byte_swap, read_big_endian_word(&ptr));
            directory[entry_index].flags = conditional_byte_swap(needs_byte_swap, read_big_endian_word(&ptr));
        }

        directory[entry_index].pixel_data_offset = read_24_big_endian_bits(&ptr);

        // Default the end-of-data boundary to end of file; this will be
        // refined below if a subsequent image's data starts after this one.
        if (directory[entry_index].pixel_data_offset != 0) {
            directory[entry_index].next_entry_data_offset = (uint32_t)datasize;
        }
        if (file_header.directory_entry_size == 14) {
            directory[entry_index].color_map_offset = read_24_big_endian_bits(&ptr);
        } else if (file_header.directory_entry_size == 16 ){
            directory[entry_index].color_map_offset = read_24_big_endian_bits(&ptr);
            // Huffman tree offset is stored as a word that must be doubled
            directory[entry_index].huffman_tree_offset = read_big_endian_word(&ptr) * 2;
        } else if (file_header.directory_entry_size != 8) {
            directory[entry_index].color_map_offset = 0;
            read_byte_and_advance(&ptr);  // skip unknown trailing byte
        }
    }

    // The 256-byte default Huffman tree immediately follows the directory.
    // Images without their own tree use this shared one.
    if (ptr + 256 <= data + datasize) {
        memcpy(default_huffman_tree, ptr, 256);
    } else {
        memset(default_huffman_tree, 0, 256);
    }

    // The file format doesn't store explicit data lengths for each image.
    // We compute each image's data size by finding the next image whose pixel data
    // starts at a higher offset. The last image uses the end-of-file as its boundary.
    for (entry_index = 0; entry_index < file_header.image_count - 1; entry_index++) {
        if (directory[entry_index].pixel_data_offset != 0) {
            for (int next_index = entry_index + 1; next_index < file_header.image_count; next_index++) {
                if (directory[next_index].pixel_data_offset > directory[entry_index].pixel_data_offset) {
                    directory[entry_index].next_entry_data_offset = directory[next_index].pixel_data_offset;
                    break;
                }
            }
        }
    }

    // Allocate the output ImageStruct array
    *image_list = (ImageStruct *)calloc(file_header.image_count, sizeof(ImageStruct));
    if (*image_list == nullptr) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    // Populate each ImageStruct with metadata and a copy of the raw pixel data,
    // along with optional per-image Huffman tree and color palette.
    for (entry_index = 0; entry_index < file_header.image_count; entry_index++) {
        (*image_list)[entry_index].index = directory[entry_index].id;
        (*image_list)[entry_index].width = directory[entry_index].width;
        (*image_list)[entry_index].height = directory[entry_index].height;
        (*image_list)[entry_index].type = *type;

        // Skip images with no pixel data or zero dimensions
        if (directory[entry_index].pixel_data_offset == 0 || directory[entry_index].width == 0 || directory[entry_index].height == 0)
            continue;

        uint32_t pixel_data_length = directory[entry_index].next_entry_data_offset - directory[entry_index].pixel_data_offset;

        // Validate that pixel data lies within the file bounds
        if (directory[entry_index].pixel_data_offset + pixel_data_length > datasize)
            continue;

        (*image_list)[entry_index].datasize = pixel_data_length;
        (*image_list)[entry_index].data = allocate_or_die(pixel_data_length);

        memcpy((*image_list)[entry_index].data, data + directory[entry_index].pixel_data_offset, pixel_data_length);

        // Bit 0 of image_flags indicates the image has a transparent color.
        // The transparent color index is stored in the upper bits of the flags field,
        // shifted by 4 bits in compact (8-byte) entries or 12 bits in larger entries.
        if (directory[entry_index].flags & 1) {
            (*image_list)[entry_index].transparency = 1;
            if (file_header.directory_entry_size == 8) {
                (*image_list)[entry_index].transparent_color = directory[entry_index].flags >> 4;
            } else {
                (*image_list)[entry_index].transparent_color = directory[entry_index].flags >> 12;
            }
        }

        // Use the per-image Huffman tree if one is specified, otherwise fall back
        // to the shared default tree read from after the directory.
        if (directory[entry_index].huffman_tree_offset != 0 &&
            directory[entry_index].huffman_tree_offset + 256 <= datasize) {
            (*image_list)[entry_index].huffman_tree = allocate_or_die(256);
            memcpy((*image_list)[entry_index].huffman_tree, data + directory[entry_index].huffman_tree_offset, 256);
        } else {
            (*image_list)[entry_index].huffman_tree = default_huffman_tree;
        }

        // Copy the per-image color palette if present (512 bytes = 256 color entries x 2 bytes each)
        if (directory[entry_index].color_map_offset != 0 &&
            directory[entry_index].color_map_offset + 512 <= datasize) {
            (*image_list)[entry_index].palette = allocate_or_die(512);
            memcpy((*image_list)[entry_index].palette, data + directory[entry_index].color_map_offset, 512);
        }
    }

    free(directory);
    *version = file_header.version;

    return file_header.image_count;
}

// Extract all picture resources from a Blorb archive into an ImageStruct array.
//
// Parameters:
//   image_list - receives the allocated array of ImageStruct results
//
// Returns the total number of picture resources found, or 0 if none.
int extract_images_from_blorb(ImageStruct **image_list)
{

    giblorb_map_t *resource_map = giblorb_get_resource_map();

    if (resource_map == nullptr) {
        return 0;
    }

    // Query the Blorb map for the range of picture resource IDs
    glui32 total_image_count, first_resource_id, last_resource_id;
    giblorb_count_resources(resource_map, giblorb_ID_Pict,
                            &total_image_count, &first_resource_id, &last_resource_id);
    if (total_image_count == 0)
        return 0;

    *image_list = (ImageStruct *)calloc(total_image_count, sizeof(ImageStruct));
    if (*image_list == nullptr) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    giblorb_result_t blorb_result;
    int output_index = 0;

    // Iterate over all possible resource IDs in the range and load each image.
    // Not every ID in the range necessarily exists, so we only populate entries
    // for resources that load successfully.
    for (glui32 resource_id = first_resource_id; resource_id <= last_resource_id; resource_id++) {
        if (output_index >= (int)total_image_count)
            break;
        if (giblorb_load_resource(resource_map, giblorb_method_Memory, &blorb_result, giblorb_ID_Pict, resource_id) == giblorb_err_None) {
            glui32 image_width, image_height;
            // Make the window process purge any cached images
            // (to make switching between graphics formats on-the-fly work)
            win_purgeimage(resource_id, 0, 0);
            glk_image_get_info(resource_id, &image_width, &image_height);
            (*image_list)[output_index].index = resource_id;
            (*image_list)[output_index].type = kGraphicsTypeBlorb;
            (*image_list)[output_index].width = image_width;
            (*image_list)[output_index].height = image_height;
            if (blorb_result.data.ptr != nullptr && blorb_result.length > 8) {
                // Make a private copy of the image data
                // as the Blorb may be purged from memory during
                // autorestore or when switching graphics type.
                void *image_data_copy = malloc(blorb_result.length);
                if (image_data_copy == nullptr) {
                    fprintf(stderr, "Out of memory\n");
                    exit(1);
                }
                memcpy(image_data_copy, blorb_result.data.ptr, blorb_result.length);
                (*image_list)[output_index].data = (uint8_t *)image_data_copy;
                (*image_list)[output_index].datasize = blorb_result.length;
                (*image_list)[output_index].palette = extract_palette_from_png_data((*image_list)[output_index].data, (*image_list)[output_index].datasize);
            }
            output_index++;
        }
    }

    // Check for an "APal" (Adaptive Palette) chunk. This optional Blorb chunk
    // lists images that should adapt their palette to the current display rather
    // than using a fixed palette. For each listed image, we discard any
    // extracted palette so the renderer knows to use adaptive coloring.
    if (giblorb_load_chunk_by_type(resource_map,
                                   giblorb_method_Memory, &blorb_result, giblorb_make_id('A', 'P', 'a', 'l'),
                                   0) == giblorb_err_None) {
        // Each APal entry is 4 bytes; the image number is a big-endian 16-bit
        // value stored at bytes 2-3 of each entry.
        for (int byte_offset = 0; byte_offset < blorb_result.length; byte_offset += 4) {
            int adaptive_palette_image_id = *((uint8_t *)blorb_result.data.ptr + byte_offset + 3) + *((uint8_t *)blorb_result.data.ptr + byte_offset + 2) * 0x100;
            for (int image_index = 0; image_index < output_index; image_index++) {
                if ((*image_list)[image_index].index == adaptive_palette_image_id) {
                    if ((*image_list)[image_index].palette != nullptr) {
                        free((*image_list)[image_index].palette);
                        (*image_list)[image_index].palette = nullptr;
                    }
                    break;
                }
            }
        }
    }
    return output_index;
}
