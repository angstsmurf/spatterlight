
/* spat-mg1.c
 *
 * Converts MCGA .mg1 image files to the z_image intermediary
 * format. MG1 is the graphics format used by Infocom's V6 Z-machine
 * games (Arthur, Shogun, Journey, Zork Zero) for VGA/MCGA display.
 * Each .mg1 file contains a header, an image directory, optional
 * per-image colormaps, and LZW-compressed pixel data.
 *
 * The names of the header fields and of the three special LZW codes
 * are those of Infocom's own IBM interpreter sources (struct PF_HEADER
 * in pic.h, the "Trivia" routine in graphics.asm). The same format is
 * read by terps/bocfel/z6 (extract_image_data.cpp, decompress_vga.cpp).
 *
 * This file is based on drilbo-mg1.h, part of Fizmo.
 *
 * Copyright (c) 2010-2012 Christoph Ender.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * 
 * This module is an adapted version of Mark Howell's pix2gif utility from
 * the ztools package version 7.3.1, available from:
 * http://www.ifarchive.org/if-archive/infocom/tools/ztools/ztools731.tar.gz
 * 
 */


#ifndef mg1_c_INCLUDED
#define mg1_c_INCLUDED

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include "spat-mg1.h"

#define IMAGE_TYPE_RGB 1

/* The file is read into memory whole. The real ones are well under a
   megabyte; anything much larger is not a graphics file. */
#define MAX_FILE_SIZE (16 * 1024 * 1024)

/* Size of the file header, which the image directory follows. */
#define FILE_HEADER_SIZE 16

/* Size of a directory entry: without a colormap address (the EGA and CGA
   files) or with one (the MCGA files). Infocom's other platforms have
   entries of 8 and 16 bytes too, but their pictures are not LZW-compressed. */
#define ENTRY_SIZE_NO_COLORMAP 12
#define ENTRY_SIZE_COLORMAP 14

/* No real picture is larger than 640 x 200. */
#define MAX_DIMENSION 4096

#define NOF_COLORS 16
/* A picture's own colormap replaces the default colors from this one on. */
#define FIRST_CUSTOM_COLOR 2

/* Directory entry flag: the picture has a transparent color, whose index
   is in the top four bits of the flags. */
#define FLAG_TRANSPARENT 0x01
#define TRANSPARENT_COLOR_SHIFT 12

#define LZW_MIN_CODE_WIDTH 9
#define LZW_MAX_CODE_WIDTH 12
#define LZW_TABLE_SIZE (1 << LZW_MAX_CODE_WIDTH)

/* Codes below LZW_CLEAR_CODE are literal pixels. */
#define LZW_CLEAR_CODE 256      /* Start over with an empty table */
#define LZW_END_CODE 257        /* End of the compressed stream */
#define LZW_FIRST_FREE_CODE 258 /* First code the table can define */


/* MG1 file header: describes the graphics file as a whole. All words are
   little-endian. Only the image count and the entry size matter here.

     Byte  0:     disk_part
     Byte  1:     flags
     Bytes 2-3:   global_huffman_offset (0: the IBM files are not
                  Huffman-coded)
     Bytes 4-5:   image_count
     Bytes 6-7:   global_directory_offset (only set in the first file of
                  the two-disk EGA games)
     Byte  8:     directory_entry_size
     Byte  9:     padding
     Bytes 10-11: checksum
     Bytes 12-13: header_version (always the bytes 01 00)
     Bytes 14-15: version
     Bytes 16+:   image directory entries */
struct mg1_header
{
    uint8_t disk_part;            /* Which disk this file belongs to (1-based) */
    uint8_t flags;                /* File-level flags */
    uint16_t image_count;         /* Number of images in the directory */
    uint8_t directory_entry_size; /* Size of each directory entry (12 or 14) */
    uint16_t checksum;
    uint16_t version;             /* Release number of the graphics file */
};

/* Per-image directory entry: describes one image's dimensions,
   flags, and file offsets for its pixel data and optional colormap. */
struct mg1_image_entry
{
    uint16_t number;        /* Picture number (used to look up images) */
    uint16_t width;
    uint16_t height;
    uint16_t flags;         /* Bit 0: transparency; bits 12-15: transparent color index */
    uint32_t data_addr;     /* File offset of LZW-compressed pixel data */
    uint32_t colormap_addr; /* File offset of custom colormap (0 = use EGA default) */
};

struct mg1_color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

/* The default EGA color palette. Infocom's interpreter has the same
   colors in Mcga_palette (data.c), as 6-bit values. */
static const struct mg1_color ega_colormap[NOF_COLORS] =
{
    {   0,  0,  0 },
    {   0,  0,170 },
    {   0,170,  0 },
    {   0,170,170 },
    { 170,  0,  0 },
    { 170,  0,170 },
    { 170, 85,  0 }, /* pix2gif has { 170,170, 0 }; Infocom's table agrees with this. */
    { 170,170,170 },
    {  85, 85, 85 },
    {  85, 85,255 },
    {  85,255, 85 },
    {  85,255,255 },
    { 255, 85, 85 },
    { 255, 85,255 },
    { 255,255, 85 },
    { 255,255,255 }
};

/* One entry of the LZW code table: the string of `prefix` followed by
   `pixel`. A prefix of LZW_TABLE_SIZE means "no prefix" (a literal code). */
struct lzw_entry {
    uint16_t prefix;
    uint8_t pixel;
};

/* State for reading variable-width LZW codes from the file data. */
typedef struct lzw_state_s {
    size_t bit_position;      /* Offset in bits, from the start of the file, of the next code */
    uint16_t next_table_code; /* Next available code to add to the table */
    uint16_t code_bit_width;  /* Current code width in bits (9-12) */
} lzw_state_t;

/* The whole file, and what init_mg1_graphics() read from it. */
static uint8_t *file_data = NULL;
static size_t file_size = 0;
static struct mg1_header file_header;
static struct mg1_image_entry *image_directory = NULL;

/* Most recently decoded RGB pixel buffer (freed on next decode or cleanup). */
static uint8_t *rgb_pixel_data = NULL;
/* Most recently returned z_image wrapper (freed on next decode or cleanup). */
static z_image *output_image = NULL;


static uint16_t read_uint16_le(const uint8_t *ptr)
{
    return (uint16_t)(ptr[0] | (ptr[1] << 8));
}

static uint32_t read_uint24_be(const uint8_t *ptr)
{
    return ((uint32_t)ptr[0] << 16) | ((uint32_t)ptr[1] << 8) | ptr[2];
}


/* Releases all resources allocated by init_mg1_graphics and get_mg1_picture.
   Safe to call multiple times. */
int end_mg1_graphics(void)
{
    free(file_data);
    file_data = NULL;
    file_size = 0;

    free(rgb_pixel_data);
    rgb_pixel_data = NULL;

    free(image_directory);
    image_directory = NULL;

    free(output_image);
    output_image = NULL;

    return 0;
}


/* Reads the file into file_data. Returns 0 on success, -1 on failure. */
static int load_file(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
        return -1;

    long length = -1;
    if (fseek(file, 0, SEEK_END) == 0)
        length = ftell(file);
    if (length < FILE_HEADER_SIZE || length > MAX_FILE_SIZE ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }

    uint8_t *data = malloc((size_t)length);
    if (data == NULL || fread(data, 1, (size_t)length, file) != (size_t)length) {
        free(data);
        fclose(file);
        return -1;
    }
    fclose(file);

    file_data = data;
    file_size = (size_t)length;
    return 0;
}


/* Reads an MG1 graphics file into memory and parses its header and image
   directory. Pictures are then decoded by get_mg1_picture(). Returns 0 on
   success; on failure returns -1 and leaves nothing loaded. */
int init_mg1_graphics(const char *mg1_filename)
{
    end_mg1_graphics();

    if (load_file(mg1_filename) != 0)
        return -1;

    file_header.disk_part = file_data[0];
    file_header.flags = file_data[1];
    file_header.image_count = read_uint16_le(file_data + 4);
    file_header.directory_entry_size = file_data[8];
    file_header.checksum = read_uint16_le(file_data + 10);
    file_header.version = read_uint16_le(file_data + 14);

    size_t entry_size = file_header.directory_entry_size;
    if ((entry_size != ENTRY_SIZE_NO_COLORMAP && entry_size != ENTRY_SIZE_COLORMAP) ||
        file_header.image_count == 0 ||
        FILE_HEADER_SIZE + file_header.image_count * entry_size > file_size) {
        end_mg1_graphics();
        return -1;
    }

    image_directory = calloc(file_header.image_count, sizeof(struct mg1_image_entry));
    if (image_directory == NULL) {
        end_mg1_graphics();
        return -1;
    }

    for (int image_index = 0; image_index < file_header.image_count; image_index++) {
        const uint8_t *ptr = file_data + FILE_HEADER_SIZE + image_index * entry_size;
        struct mg1_image_entry *entry = &image_directory[image_index];

        entry->number = read_uint16_le(ptr);
        entry->width = read_uint16_le(ptr + 2);
        entry->height = read_uint16_le(ptr + 4);
        entry->flags = read_uint16_le(ptr + 6);
        entry->data_addr = read_uint24_be(ptr + 8);
        if (entry_size == ENTRY_SIZE_COLORMAP)
            entry->colormap_addr = read_uint24_be(ptr + 11);
    }

    return 0;
}


/* Returns the number of images in the loaded MG1 file, or 0 if none loaded. */
uint16_t get_number_of_mg1_images(void)
{
    return file_data == NULL ? 0 : file_header.image_count;
}


/* Returns a malloc'd array of all picture numbers in the loaded MG1 file.
   The caller is responsible for freeing the returned array. */
uint16_t *get_mg1_picture_numbers(void)
{
    if (file_data == NULL)
        return NULL;

    uint16_t *result = malloc(sizeof(uint16_t) * file_header.image_count);
    if (result == NULL)
        return NULL;

    for (int image_index = 0; image_index < file_header.image_count; image_index++)
        result[image_index] = image_directory[image_index].number;

    return result;
}


/* Searches the image directory for a picture with the given number.
   Returns its entry, or NULL if not found. */
static const struct mg1_image_entry *find_image(int picture_number)
{
    if (file_data == NULL)
        return NULL;

    for (int image_index = 0; image_index < file_header.image_count; image_index++) {
        if (image_directory[image_index].number == picture_number)
            return &image_directory[image_index];
    }

    return NULL;
}


/* Fills in the 16 colors of a picture: the default EGA palette, with the
   picture's own colormap, if it has one, replacing the colors from index 2
   on (black and white at indices 0-1 are always kept), and the transparent
   color, if there is one, forced to black. Returns 0 on success, -1 if the
   colormap is not inside the file. */
static int read_colormap(const struct mg1_image_entry *entry, struct mg1_color *colors)
{
    memcpy(colors, ega_colormap, sizeof(ega_colormap));

    if (entry->colormap_addr != 0) {
        /* A count, then that many red, green and blue bytes. */
        if (entry->colormap_addr >= file_size)
            return -1;
        size_t nof_colors = file_data[entry->colormap_addr];

        /* Some Arthur pictures claim more colors than there is room for.
           Infocom's interpreters clamp the count in the same way. */
        if (nof_colors > NOF_COLORS - FIRST_CUSTOM_COLOR)
            nof_colors = NOF_COLORS - FIRST_CUSTOM_COLOR;

        if (entry->colormap_addr + 1 + nof_colors * 3 > file_size)
            return -1;

        const uint8_t *ptr = file_data + entry->colormap_addr + 1;
        for (size_t i = 0; i < nof_colors; i++, ptr += 3) {
            colors[FIRST_CUSTOM_COLOR + i].red = ptr[0];
            colors[FIRST_CUSTOM_COLOR + i].green = ptr[1];
            colors[FIRST_CUSTOM_COLOR + i].blue = ptr[2];
        }
    }

    if (entry->flags & FLAG_TRANSPARENT) {
        struct mg1_color *transparent = &colors[entry->flags >> TRANSPARENT_COLOR_SHIFT];
        transparent->red = transparent->green = transparent->blue = 0;
    }

    return 0;
}


/* Reads the next variable-width LZW code from the file data. Codes are
   packed starting at the low bit of each byte. The code width grows by one
   bit when the next free code reaches 512, 1024 and 2048, and stays at 12
   bits until a clear code comes through. Returns -1 at the end of the file. */
static int read_lzw_code(lzw_state_t *state)
{
    unsigned width = state->code_bit_width;
    if (state->bit_position + width > file_size * 8)
        return -1;

    /* A code may straddle up to three bytes. The check above guarantees
       that every byte holding a bit of it is inside the file. */
    size_t byte_offset = state->bit_position >> 3;
    unsigned shift = state->bit_position & 7;
    uint32_t bits = 0;
    for (unsigned i = 0; i * 8 < shift + width; i++)
        bits |= (uint32_t)file_data[byte_offset + i] << (i * 8);
    int code = (bits >> shift) & ((1u << width) - 1);
    state->bit_position += width;

    /* The caller is about to define next_table_code. If that is the last
       code that fits in the current width, the code after this one is a
       bit wider (up to the 12-bit maximum). */
    if (state->code_bit_width < LZW_MAX_CODE_WIDTH &&
        state->next_table_code == (1u << state->code_bit_width) - 1)
        state->code_bit_width++;

    return code;
}


/* LZW-decompresses the pixel data at data_addr, mapping each pixel through
   `colors` and writing it to `rgb` as a red, green and blue byte, until
   pixel_count pixels are written or the data ends. This is the same decoding
   as decompress_vga.cpp in terps/bocfel/z6 does, except that that one outputs
   palette indices. Returns 0 on success, -1 on corrupt data. */
static int decompress_to_rgb(uint32_t data_addr, const struct mg1_color *colors, uint8_t *rgb, size_t pixel_count)
{
    /* Used to reverse the prefix chain (which is stored root-last) into the
       correct output order. */
    uint8_t pixel_stack[LZW_TABLE_SIZE];
    struct lzw_entry table[LZW_TABLE_SIZE];

    for (int i = 0; i < LZW_TABLE_SIZE; i++) {
        table[i].prefix = LZW_TABLE_SIZE;
        table[i].pixel = (uint8_t)i;
    }

    lzw_state_t state;
    state.bit_position = (size_t)data_addr * 8;
    state.next_table_code = LZW_FIRST_FREE_CODE;
    state.code_bit_width = LZW_MIN_CODE_WIDTH;

    size_t pixels_written = 0;
    int previous_code = 0;
    while (pixels_written < pixel_count) {
        int code = read_lzw_code(&state);
        if (code < 0 || code == LZW_END_CODE)
            break;

        if (code == LZW_CLEAR_CODE) {
            /* Reset the code table to its initial state. */
            state.code_bit_width = LZW_MIN_CODE_WIDTH;
            state.next_table_code = LZW_FIRST_FREE_CODE;
            code = read_lzw_code(&state);
            if (code < 0)
                return -1;
        } else {
            /* Walk the prefix chain to find the root pixel of this code's
               string. For the "KwKwK" case (code == next_table_code),
               start from the previous code instead. */
            int root_code = (code == state.next_table_code) ? previous_code : code;
            int chain_limit = LZW_TABLE_SIZE;
            while (table[root_code].prefix != LZW_TABLE_SIZE) {
                if (--chain_limit == 0)
                    return -1;
                root_code = table[root_code].prefix;
            }

            /* Add a new entry: previous code's string + this string's root pixel. */
            if (state.next_table_code < LZW_TABLE_SIZE) {
                table[state.next_table_code].prefix = (uint16_t)previous_code;
                table[state.next_table_code].pixel = table[root_code].pixel;
                state.next_table_code++;
            }
        }
        previous_code = code;

        /* Walk the prefix chain, pushing each pixel onto the stack
           (they come out in reverse order). */
        int stack_depth = 0;
        do {
            if (stack_depth >= LZW_TABLE_SIZE)
                return -1;
            pixel_stack[stack_depth++] = table[code].pixel;
        } while ((code = table[code].prefix) != LZW_TABLE_SIZE);

        /* Pop pixels from the stack in correct order, mapping each
           through the colormap to produce RGB output. */
        while (stack_depth > 0 && pixels_written < pixel_count) {
            unsigned color_index = pixel_stack[--stack_depth];
            if (color_index >= NOF_COLORS)
                color_index = 0;
            *rgb++ = colors[color_index].red;
            *rgb++ = colors[color_index].green;
            *rgb++ = colors[color_index].blue;
            pixels_written++;
        }
    }
    return 0;
}


/* Decodes and returns a single picture from the MG1 file as an RGB z_image.

   The returned z_image and its pixel data are owned by this module;
   they remain valid until the next call to get_mg1_picture() or
   end_mg1_graphics(). Returns NULL on failure. */
z_image *get_mg1_picture(int picture_number)
{
    const struct mg1_image_entry *entry = find_image(picture_number);
    if (entry == NULL)
        return NULL;

    free(rgb_pixel_data);
    rgb_pixel_data = NULL;
    free(output_image);
    output_image = NULL;

    /* A picture number may be in the directory without having any data. */
    if (entry->data_addr == 0 || entry->data_addr >= file_size ||
        entry->width == 0 || entry->height == 0 ||
        entry->width > MAX_DIMENSION || entry->height > MAX_DIMENSION)
        return NULL;

    struct mg1_color colors[NOF_COLORS];
    if (read_colormap(entry, colors) != 0)
        return NULL;

    /* 3 bytes (RGB) per pixel. Whatever the data doesn't fill stays black. */
    size_t pixel_count = (size_t)entry->width * entry->height;
    uint8_t *pixels = calloc(pixel_count, 3);
    z_image *image = malloc(sizeof(z_image));
    if (pixels == NULL || image == NULL ||
        decompress_to_rgb(entry->data_addr, colors, pixels, pixel_count) != 0) {
        free(pixels);
        free(image);
        return NULL;
    }

    image->bits_per_sample = 8;
    image->width = entry->width;
    image->height = entry->height;
    image->image_type = IMAGE_TYPE_RGB;
    image->data = pixels;

    rgb_pixel_data = pixels;
    output_image = image;
    return output_image;
}

#endif /* mg1_c_INCLUDED */
