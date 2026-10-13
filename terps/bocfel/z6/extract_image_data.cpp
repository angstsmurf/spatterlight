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
#include "read_le16.h"
}

#include "types.h"
#include "memory_allocation.hpp"
#include "v6_image.h"
#include "draw_png.h"

#include "extract_image_data.hpp"

// Infocom image directory entry sizes (bytes per entry)
static const int kCompactEntrySize = 8;
// 12-byte entries have no colour map: id, width, height, flags, pixel data
// offset, and one unused trailing byte. Used by Zork Zero's Mac B/W
// (PIC.DATA), EGA (.EG1) and CGA (.CG1) files.
static const int kNoColorMapEntrySize = 12;
static const int kStandardEntrySize = 14;
static const int kExtendedEntrySize = 16;

// Size in bytes of the header at the start of an Infocom graphics file
static const size_t kFileHeaderSize = 16;

// Header flags value indicating monochrome Mac graphics
static const uint8_t kMacBWFlags = 0x0e;

// Huffman decoding tree: 256 entries, one per possible byte value
static const size_t kHuffmanTreeSize = 256;

// Color palette: 256 entries × 2 bytes each
static const size_t kColorPaletteSize = 512;

// Per-image Huffman tree offsets in the directory are stored as half their
// actual byte offset (i.e. in 2-byte units), so multiply by 2 to get bytes
static const int kHuffmanOffsetScale = 2;

// Transparency: bit 0 of the flags field indicates transparency is enabled.
// The transparent color index is stored in the upper bits, shifted by 4 in
// compact entries or 12 in standard/extended entries.
static const uint16_t kTransparencyFlag = 0x01;
static const int kCompactTransparentColorShift = 4;
static const int kExtendedTransparentColorShift = 12;

// Blorb APal (Adaptive Palette) chunk: each entry is 4 bytes with the
// image ID as a big-endian 16-bit value at offset 2
static const glui32 kAPalEntrySize = 4;
static const glui32 kAPalImageIdOffset = 2;

// Minimum meaningful Blorb resource size (must contain at least a header)
static const glui32 kMinBlorbResourceSize = 8;

// Header structure at the start of an Infocom graphics file.
// Multi-disk games split images across several files, each identified by disk_part.
typedef struct InfocomImageFileHeader {
    uint8_t  disk_part; // Which disk this file belongs to (1-based)
    uint8_t  flags; // Format flags; 0x0e indicates monochrome Mac graphics
    uint16_t unknown_1;
    uint16_t image_count;// Total number of images described in the directory
    uint16_t unknown_2;
    uint8_t  directory_entry_size; // Size in bytes of each directory entry (8, 12, 14, or 16)
    uint8_t  unknown_3;
    uint16_t checksum;
    uint16_t unknown_4;
    uint16_t version; // Graphics format version number
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

// Shared Huffman decoding tree used by images that don't specify their own tree.
// Populated from the data immediately following the image directory.
uint8_t default_huffman_tree[kHuffmanTreeSize];

// Read the header at `ptr`, which the caller has checked holds at least
// kFileHeaderSize bytes. PC-format files (VGA/EGA/CGA) store multi-byte
// fields in the opposite byte order from Amiga/Mac files.
static InfocomImageFileHeader read_file_header(uint8_t **ptr, bool needs_byte_swap)
{
    auto read_word = [&]() {
        return conditional_byte_swap(needs_byte_swap, READ_BE_UINT16_AND_ADVANCE(ptr));
    };

    InfocomImageFileHeader header;
    header.disk_part = read_byte_and_advance(ptr);
    header.flags = read_byte_and_advance(ptr);
    header.unknown_1 = read_word();
    header.image_count = read_word();
    header.unknown_2 = read_word();
    header.directory_entry_size = read_byte_and_advance(ptr);
    header.unknown_3 = read_byte_and_advance(ptr);
    header.checksum = read_word();
    header.unknown_4 = read_word();
    header.version = read_word();
    return header;
}

// Read one directory entry of `entry_size` bytes. Entry size varies by
// format version:
//   8 bytes:  compact format (single-byte width/height/flags)
//  12 bytes:  no color map; one unused trailing byte
//  14 bytes:  standard format with color map offset
//  16 bytes:  extended format with color map and per-image Huffman tree offset
static ImageDirectoryEntry read_directory_entry(uint8_t **ptr, int entry_size, bool needs_byte_swap)
{
    auto read_word = [&]() {
        return conditional_byte_swap(needs_byte_swap, READ_BE_UINT16_AND_ADVANCE(ptr));
    };

    ImageDirectoryEntry entry = {};
    entry.id = read_word();
    if (entry_size == kCompactEntrySize) {
        // Compact entries store dimensions and flags as single bytes
        entry.width = read_byte_and_advance(ptr);
        entry.height = read_byte_and_advance(ptr);
        entry.flags = read_byte_and_advance(ptr);
    } else {
        entry.width = read_word();
        entry.height = read_word();
        entry.flags = read_word();
    }

    entry.pixel_data_offset = read_24_big_endian_bits(ptr);

    if (entry_size == kStandardEntrySize) {
        entry.color_map_offset = read_24_big_endian_bits(ptr);
    } else if (entry_size == kExtendedEntrySize) {
        entry.color_map_offset = read_24_big_endian_bits(ptr);
        // Huffman tree offset is stored as a word that must be doubled
        entry.huffman_tree_offset = READ_BE_UINT16_AND_ADVANCE(ptr) * kHuffmanOffsetScale;
    } else if (entry_size == kNoColorMapEntrySize) {
        read_byte_and_advance(ptr);  // skip unknown trailing byte
    }
    return entry;
}

// The file format doesn't store explicit data lengths for each image.
// We compute each image's data size by finding the next image whose pixel data
// starts at a higher offset. An image with no such successor runs to the end
// of the file.
static void set_data_boundaries(ImageDirectoryEntry *directory, int count, size_t datasize)
{
    for (int entry_index = 0; entry_index < count; entry_index++) {
        ImageDirectoryEntry &entry = directory[entry_index];
        if (entry.pixel_data_offset == 0)
            continue;
        entry.next_entry_data_offset = (uint32_t)datasize;
        for (int next_index = entry_index + 1; next_index < count; next_index++) {
            if (directory[next_index].pixel_data_offset > entry.pixel_data_offset) {
                entry.next_entry_data_offset = directory[next_index].pixel_data_offset;
                break;
            }
        }
    }
}

// Fill in `image` from its directory entry: metadata, a copy of the raw
// pixel data, and the optional per-image Huffman tree and color palette.
// An image with no usable pixel data gets its metadata only.
static void load_image(ImageStruct *image, const ImageDirectoryEntry &entry, int entry_size, GraphicsType type, const uint8_t *data, size_t datasize)
{
    image->index = entry.id;
    image->width = entry.width;
    image->height = entry.height;
    image->type = type;

    // Skip images with no pixel data or zero dimensions
    if (entry.pixel_data_offset == 0 || entry.width == 0 || entry.height == 0)
        return;

    // Guard against underflow when next_entry_data_offset < pixel_data_offset
    if (entry.next_entry_data_offset <= entry.pixel_data_offset)
        return;

    uint32_t pixel_data_length = entry.next_entry_data_offset - entry.pixel_data_offset;

    // Validate that pixel data lies within the file bounds (use size_t to avoid 32-bit overflow)
    if ((size_t)entry.pixel_data_offset + (size_t)pixel_data_length > datasize)
        return;

    image->datasize = pixel_data_length;
    image->data = (uint8_t *)MemAlloc(pixel_data_length);
    memcpy(image->data, data + entry.pixel_data_offset, pixel_data_length);

    // Bit 0 of the flags indicates the image has a transparent color.
    // The transparent color index is stored in the upper bits of the flags field,
    // shifted by 4 bits in compact (8-byte) entries or 12 bits in larger entries.
    if (entry.flags & kTransparencyFlag) {
        image->transparency = 1;
        if (entry_size == kCompactEntrySize) {
            image->transparent_color = entry.flags >> kCompactTransparentColorShift;
        } else {
            image->transparent_color = entry.flags >> kExtendedTransparentColorShift;
        }
    }

    // Use the per-image Huffman tree if one is specified, otherwise fall back
    // to the shared default tree read from after the directory.
    if (entry.huffman_tree_offset != 0 &&
        (size_t)entry.huffman_tree_offset + kHuffmanTreeSize <= datasize) {
        image->huffman_tree = (uint8_t *)MemAlloc(kHuffmanTreeSize);
        memcpy(image->huffman_tree, data + entry.huffman_tree_offset, kHuffmanTreeSize);
    } else {
        image->huffman_tree = default_huffman_tree;
    }

    // Copy the per-image color palette if present (512 bytes = 256 color entries x 2 bytes each)
    if (entry.color_map_offset != 0 &&
        (size_t)entry.color_map_offset + kColorPaletteSize <= datasize) {
        image->palette = (uint8_t *)MemAlloc(kColorPaletteSize);
        memcpy(image->palette, data + entry.color_map_offset, kColorPaletteSize);
    }
}

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
    *image_list = nullptr;

    if (data == nullptr || (size_t)offset + kFileHeaderSize > datasize)
        return 0;

    uint8_t *ptr = data + offset;
    uint8_t *end = data + datasize;

    bool needs_byte_swap = (*type == kGraphicsTypeVGA || *type == kGraphicsTypeEGA || *type == kGraphicsTypeCGA);
    InfocomImageFileHeader file_header = read_file_header(&ptr, needs_byte_swap);

    // Verify this file is for the requested disk; multi-disk games have
    // separate graphics files for each disk.
    if (file_header.disk_part != disk) {
        return 0;
    }

    // A file named "pic.data" may contain colour Amiga graphics or monochrome Mac graphics,
    // but the flags always seem to equal 0xe (14) if the graphics are monochrome.
    // (Amiga graphics data and Mac colour graphics data are identical.)
    if (file_header.flags == kMacBWFlags) {
        if (*type == kGraphicsTypeAmiga)
            *type = kGraphicsTypeMacBW;
    } else if (*type == kGraphicsTypeMacBW) {
        *type = kGraphicsTypeAmiga;
    }

    if (file_header.image_count == 0)
        return 0;

    // read_directory_entry() reads a fixed byte count that depends on the
    // entry size. Any other value would make the directory-fits check below
    // disagree with the bytes actually consumed, allowing a small/odd size
    // to pass the check yet read past `end`.
    if (file_header.directory_entry_size != kCompactEntrySize &&
        file_header.directory_entry_size != kNoColorMapEntrySize &&
        file_header.directory_entry_size != kStandardEntrySize &&
        file_header.directory_entry_size != kExtendedEntrySize) {
        return 0;
    }

    // Verify the directory fits within the file
    size_t directory_bytes = (size_t)file_header.image_count * file_header.directory_entry_size;
    if (ptr + directory_bytes > end)
        return 0;

    ImageDirectoryEntry *directory = (ImageDirectoryEntry *)MemCalloc((size_t)file_header.image_count * sizeof(ImageDirectoryEntry));

    for (int entry_index = 0; entry_index < file_header.image_count; entry_index++)
        directory[entry_index] = read_directory_entry(&ptr, file_header.directory_entry_size, needs_byte_swap);

    // The 256-byte default Huffman tree immediately follows the directory.
    // Images without their own tree use this shared one.
    if (ptr + kHuffmanTreeSize <= end) {
        memcpy(default_huffman_tree, ptr, kHuffmanTreeSize);
    } else {
        memset(default_huffman_tree, 0, kHuffmanTreeSize);
    }

    set_data_boundaries(directory, file_header.image_count, datasize);

    *image_list = (ImageStruct *)MemCalloc(file_header.image_count * sizeof(ImageStruct));

    for (int entry_index = 0; entry_index < file_header.image_count; entry_index++)
        load_image(&(*image_list)[entry_index], directory[entry_index], file_header.directory_entry_size, *type, data, datasize);

    free(directory);
    *version = file_header.version;

    return file_header.image_count;
}

// Read the dimensions of a Blorb picture resource straight out of its data:
// the IHDR chunk of a PNG, or a Rect placeholder (two big-endian 32-bit
// values, width then height). Returns false for anything else.
static bool blorb_picture_size(const giblorb_result_t *resource, glui32 *width, glui32 *height)
{
    static const uint8_t kPNGHeader[16] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
    const uint8_t *data = (const uint8_t *)resource->data.ptr;
    if (data == nullptr)
        return false;

    auto be32 = [](const uint8_t *p) {
        return ((glui32)p[0] << 24) | ((glui32)p[1] << 16) | ((glui32)p[2] << 8) | (glui32)p[3];
    };

    glui32 w, h;
    if (resource->chunktype == giblorb_make_id('R', 'e', 'c', 't') && resource->length >= 8) {
        w = be32(data);
        h = be32(data + 4);
    } else if (resource->length >= sizeof kPNGHeader + 8 && memcmp(data, kPNGHeader, sizeof kPNGHeader) == 0) {
        w = be32(data + 16);
        h = be32(data + 20);
    } else {
        return false;
    }

    if (w > (glui32)kMaxImageDimension || h > (glui32)kMaxImageDimension)
        return false;
    *width = w;
    *height = h;
    return true;
}

// Extract all picture resources from a Blorb archive into an ImageStruct array.
//
// Parameters:
//   image_list - receives the allocated array of ImageStruct results
//
// Returns the total number of picture resources found, or 0 if none.
int extract_images_from_blorb(ImageStruct **image_list)
{
    *image_list = nullptr;

    giblorb_map_t *resource_map = giblorb_get_resource_map();
    if (resource_map == nullptr)
        return 0;

    // Query the Blorb map for the range of picture resource IDs
    glui32 total_image_count, first_resource_id, last_resource_id;
    giblorb_count_resources(resource_map, giblorb_ID_Pict,
                            &total_image_count, &first_resource_id, &last_resource_id);
    if (total_image_count == 0)
        return 0;

    *image_list = (ImageStruct *)MemCalloc(total_image_count * sizeof(ImageStruct));
    giblorb_result_t blorb_result;
    int output_index = 0;

    // Iterate over all possible resource IDs in the range and load each image.
    // Not every ID in the range necessarily exists, so we only populate entries
    // for resources that load successfully.
    for (glui32 resource_id = first_resource_id; ; resource_id++) {
        if (output_index >= (int)total_image_count)
            break;
        if (giblorb_load_resource(resource_map, giblorb_method_Memory, &blorb_result, giblorb_ID_Pict, resource_id) == giblorb_err_None) {
            glui32 image_width = 0, image_height = 0;
            // Make the window process purge any cached images
            // (to make switching between graphics formats on-the-fly work)
            win_purgeimage(resource_id, 0, 0);
            // Read the size from the resource itself where we can. Asking Glk
            // only works while graphics are enabled, and the games lay out
            // their windows from these sizes whether or not anything is drawn.
            if (!blorb_picture_size(&blorb_result, &image_width, &image_height))
                glk_image_get_info(resource_id, &image_width, &image_height);
            (*image_list)[output_index].index = resource_id;
            (*image_list)[output_index].type = kGraphicsTypeBlorb;
            (*image_list)[output_index].width = image_width;
            (*image_list)[output_index].height = image_height;
            if (blorb_result.data.ptr != nullptr && blorb_result.length > kMinBlorbResourceSize) {
                void *image_data_copy = MemAlloc(blorb_result.length);
                memcpy(image_data_copy, blorb_result.data.ptr, blorb_result.length);
                (*image_list)[output_index].data = (uint8_t *)image_data_copy;
                (*image_list)[output_index].datasize = blorb_result.length;
                (*image_list)[output_index].palette = extract_palette_from_png_data((*image_list)[output_index].data, (*image_list)[output_index].datasize);
            }
            output_index++;
        }
        if (resource_id == last_resource_id)
            break;
    }

    // Check for an "APal" (Adaptive Palette) chunk. This optional Blorb chunk
    // lists images that should adapt their palette to the current display rather
    // than using a fixed palette. For each listed image, we discard any
    // extracted palette so the renderer knows to use adaptive coloring.
    if (giblorb_load_chunk_by_type(resource_map,
                                   giblorb_method_Memory, &blorb_result, giblorb_make_id('A', 'P', 'a', 'l'),
                                   0) == giblorb_err_None) {
        uint8_t *apal = (uint8_t *)blorb_result.data.ptr;
        // Each APal entry is 4 bytes; the image number is a big-endian 16-bit
        // value stored at bytes 2-3 of each entry.
        for (glui32 byte_offset = 0; byte_offset + kAPalEntrySize - 1 < blorb_result.length; byte_offset += kAPalEntrySize) {
            int adaptive_palette_image_id = READ_BE_UINT16(&apal[byte_offset + kAPalImageIdOffset]);
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
