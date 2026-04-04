
/* spat-mg1.c
 *
 * Converts MCGA .mg1 image files to the z_image intermediary
 * format. MG1 is the graphics format used by Infocom's V6 Z-machine
 * games (Arthur, Shogun, Journey, Zork Zero) for VGA/EGA display.
 * Each .mg1 file contains a header, an image directory, optional
 * per-image colormaps, and LZW-compressed pixel data.
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
#include <inttypes.h>
#include <stdio.h>

#include "spat-mg1.h"

#define LZW_MIN_CODE_SIZE 8
#define LZW_TABLE_SIZE 4096
#define READ_BUFFER_SIZE 512 /* Must be less than or equal to LZW_TABLE_SIZE */
#define PREFIX 0    /* Index into code table entry for the prefix code */
#define PIXEL 1     /* Index into code table entry for the pixel value */

#define IMAGE_TYPE_RGB 1
#define IMAGE_TYPE_GRAYSCALE 2


/* MG1 file header: describes the graphics file as a whole.
   Fields are read in order but with gaps (unknown/reserved bytes
   are skipped via fseek). */
struct mg1_header
{
  uint8_t disk_part;         /* Part/disk number */
  uint8_t flags;        /* File-level flags */
  //uint16_t unknown1;
  uint16_t nof_images;  /* Number of images in the file */
  //uint16_t unknown2;
  uint8_t dir_size;     /* Size of each directory entry (11 or 14 bytes) */
  //uint8_t unknown3;
  uint16_t checksum;
  //uint16_t unknown4;
  uint16_t version;     /* Graphics format version */
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

struct mg1_colormap {
  uint8_t nof_colors;
  struct mg1_color colors[16];
};

// The colormap data for the default EGA color palette was taken from
// http://en.wikipedia.org/wiki/Enhanced_Graphics_Adapter.
static const uint8_t ega_colormap[16][3] =
{
  {   0,  0,  0 },
  {   0,  0,170 },
  {   0,170,  0 },
  {   0,170,170 },
  { 170,  0,  0 },
  { 170,  0,170 },
  { 170, 85,  0 }, // -> This is { 170,170, 0 } in pix2gif (?).
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

/* Precomputed bitmasks for extracting N bits: bit_masks[n] = (1 << n) - 1. */
static const short bit_masks[16] = { 
  0x0000, 0x0001, 0x0003, 0x0007,
  0x000f, 0x001f, 0x003f, 0x007f,
  0x00ff, 0x01ff, 0x03ff, 0x07ff,
  0x0fff, 0x1fff, 0x3fff, 0x7fff
};

/* Persistent state for reading variable-width LZW codes from a bitstream. */
typedef struct lzw_state_s {
  short next_table_code;       /* Next available code to add to the table */
  short buffer_bits_remaining; /* Unread bits remaining in the read buffer */
  short buffer_bit_position;   /* Current bit offset within the read buffer */
  short code_bit_width;        /* Current code width in bits (9-12) */
} lzw_state_t;

/* LZW code table: each entry stores [PREFIX, PIXEL].
   LZW_TABLE_SIZE as a prefix sentinel means "no prefix" (root code). */
static short lzw_table[LZW_TABLE_SIZE][2];

/* Stack used to reverse the LZW prefix chain into correct output order. */
static unsigned char pixel_stack[LZW_TABLE_SIZE];

static FILE *mg1_file = NULL;
static struct mg1_header file_header;
static struct mg1_image_entry *image_directory = NULL;

/* Most recently decoded RGB pixel buffer (freed on next decode or cleanup). */
uint8_t *rgb_pixel_data = NULL;
/* Most recently returned z_image wrapper (freed on next decode or cleanup). */
z_image *output_image = NULL;


/* Reads a single byte from the file. Returns the byte value (0-255)
   on success, or -1 on read failure / EOF. */
static int read_file_byte(FILE *in)
{
    uint8_t byte;
    if (fread(&byte, 1, 1, in) != 1)
        return -1;
    return byte;
}

/* Reads up to len bytes into ptr. Returns the number of bytes actually read. */
static size_t read_file_bytes(void *ptr, size_t len, FILE *fileref)
{
    size_t bytes_read;
    bytes_read = fread(ptr, 1, len, fileref);
    return bytes_read;
}

/* Reads a single uint8 from the file. Returns 0 on success, -1 on failure. */
static int read_uint8(FILE *in, uint8_t *byte)
{
  int data;

  if ((data = read_file_byte(in)) == -1)
  { fclose(in); return -1; }

  *byte = (uint8_t)data;

  return 0;
}


/* Reads a little-endian uint16 from the file. Returns 0 on success, -1 on failure. */
static int read_uint16_le(FILE *in, uint16_t *word)
{
  int lower, upper;

  if ((lower = read_file_byte(in)) == -1)
  { fclose(in); return -1; }

  if ((upper = read_file_byte(in)) == -1)
  { fclose(in); return -1; }

  *word = (uint16_t)((lower & 0xff) | ((upper & 0xff) << 8));

  return 0;
}


/* Reads a big-endian 24-bit unsigned integer from the file,
   stored in a uint32. Returns 0 on success, -1 on failure. */
static int read_uint24_be(FILE *in, uint32_t *data)
{
  int lower, middle, upper;

  if ((upper = read_file_byte(in)) == -1)
  { fclose(in); return -1; }

  if ((middle = read_file_byte(in)) == -1)
  { fclose(in); return -1; }

  if ((lower = read_file_byte(in)) == -1)
  { fclose(in); return -1; }

  *data = (uint32_t)((lower & 0xff) | ((middle & 0xff) << 8) | ((upper & 0xff) << 16));

  return 0;
}


/* Releases all resources allocated by init_mg1_graphics and get_mg1_picture.
   Safe to call multiple times. */
int end_mg1_graphics(void)
{
    if (mg1_file != NULL) {
        fclose(mg1_file);
        mg1_file = NULL;
    }

    if (rgb_pixel_data != NULL) {
        free(rgb_pixel_data);
        rgb_pixel_data = NULL;
    }

    if (image_directory != NULL) {
        free(image_directory);
        image_directory = NULL;
    }
    
    if (output_image != NULL) {
        free(output_image);
        output_image = NULL;
    }

    return 0;
}


/* Opens an MG1 graphics file, reads the file header and the image
   directory into memory. The file is kept open for later image
   decoding via get_mg1_picture(). Returns 0 on success, -1 on failure.
   
   MG1 file layout:
     Bytes 0-1:   part, flags
     Bytes 2-3:   unknown (skipped)
     Bytes 4-5:   number of images (LE)
     Bytes 6-7:   unknown (skipped)
     Byte  8:     directory entry size (11 or 14)
     Byte  9:     unknown (skipped)
     Bytes 10-11: checksum (LE)
     Bytes 12-13: unknown (skipped)
     Bytes 14-15: version (LE)
     Bytes 16+:   image directory entries */
int init_mg1_graphics(const char *mg1_filename)
{
  int image_index;
  struct mg1_image_entry *entry;

  if ((mg1_file = fopen(mg1_filename, "rb"))
      == NULL)
    return -1;

  if ((read_uint8(mg1_file, &file_header.disk_part)) == -1)
    return -1;

  if ((read_uint8(mg1_file, &file_header.flags)) == -1)
    return -1;

  if (fseek(mg1_file, 2, SEEK_CUR) == -1)
  { fclose(mg1_file); return -1; }

  if ((read_uint16_le(mg1_file, &file_header.nof_images)) == -1)
    return -1;

  if (fseek(mg1_file, 2, SEEK_CUR) == -1)
  { fclose(mg1_file); return -1; }

  if ((read_uint8(mg1_file, &file_header.dir_size)) == -1)
    return -1;

  if (fseek(mg1_file, 1, SEEK_CUR) == -1)
  { fclose(mg1_file); return -1; }

  if ((read_uint16_le(mg1_file, &file_header.checksum)) == -1)
    return -1;

  if (fseek(mg1_file, 2, SEEK_CUR) == -1)
  { fclose(mg1_file); return -1; }

  if ((read_uint16_le(mg1_file, &file_header.version)) == -1)
    return -1;

  if ((image_directory
        = malloc(sizeof(struct mg1_image_entry) * file_header.nof_images)) == NULL)
  { fclose(mg1_file); return -1; }

  /* Read each image directory entry. Each entry is dir_size bytes:
     11 bytes for the base fields, or 14 bytes if a colormap address
     (3 extra bytes, big-endian) is included. */
  for (image_index=0; image_index<file_header.nof_images; image_index++)
  {
    entry = &(image_directory[image_index]);

    if ((read_uint16_le(mg1_file, &entry->number)) == -1)
    { free(image_directory); return -1; }

    if ((read_uint16_le(mg1_file, &entry->width)) == -1)
    { free(image_directory); return -1; }

    if ((read_uint16_le(mg1_file, &entry->height)) == -1)
    { free(image_directory); return -1; }

    if ((read_uint16_le(mg1_file, &entry->flags)) == -1)
    { free(image_directory); return -1; }

    if ((read_uint24_be(mg1_file, &entry->data_addr)) == -1)
    { free(image_directory); return -1; }

    if (file_header.dir_size == 14)
    {
      if ((read_uint24_be(mg1_file, &entry->colormap_addr)) == -1)
      { free(image_directory); return -1; }
    }
    else
      entry->colormap_addr = 0;
  }

  return 0;
}


/* Returns the number of images in the loaded MG1 file, or 0 if none loaded. */
uint16_t get_number_of_mg1_images(void)
{
  return mg1_file == NULL ? 0 : file_header.nof_images;
}


/* Returns a malloc'd array of all picture numbers in the loaded MG1 file.
   The caller is responsible for freeing the returned array. */
uint16_t *get_mg1_picture_numbers(void)
{
  uint16_t *result;
  int image_index;

  if (mg1_file == NULL)
    return NULL;

  if ((result = (uint16_t*)malloc(sizeof(uint16_t) * file_header.nof_images))
      == NULL)
    return NULL;

  for (image_index = 0; image_index < file_header.nof_images; image_index++)
  {
    result[image_index] = image_directory[image_index].number;
  }

  return result;
}


/* Searches the image directory for a picture with the given number.
   Returns its index (0-based), or -1 if not found. */
static int find_image_index(int picture_number)
{
  int image_index;

  if (mg1_file == NULL)
    return -1;

  for (image_index = 0; image_index < file_header.nof_images; image_index++)
  {
    if (image_directory[image_index].number == picture_number)
      return image_index;
  }

  return -1;
}


/* Reads the next variable-width LZW code from the compressed bitstream.
   Refills the read buffer from the file in READ_BUFFER_SIZE-byte chunks
   as needed. Automatically increases code_bit_width when the table is
   about to overflow the current width. Returns -1 if the input data
   is exhausted before a complete code can be read. */
static short read_lzw_code(FILE *fp, lzw_state_t *state, unsigned char *read_buffer)
{
  short code, bits_available, bits_needed, bits_assembled;

  code = 0;
  bits_needed = state->code_bit_width;
  bits_assembled = 0;

  /* Assemble the code one byte-boundary fragment at a time, since codes
     may straddle byte boundaries in the packed bitstream. */
  while (bits_needed)
  {
    /* Refill the read buffer when all buffered bits have been consumed. */
    if (state->buffer_bits_remaining == 0)
    {
      if ((state->buffer_bits_remaining = (short)read_file_bytes(read_buffer, READ_BUFFER_SIZE, fp)) == 0)
        return -1;
      state->buffer_bits_remaining *= 8;
      state->buffer_bit_position = 0;
    }

    /* Extract bits from the current byte up to the next byte boundary,
       or fewer if that's all we need to complete the code. */
    bits_available = ((state->buffer_bit_position + 8) & ~7) - state->buffer_bit_position;
    bits_available = (bits_needed > bits_available) ? bits_available : bits_needed;
    code |= (((unsigned int) read_buffer[state->buffer_bit_position >> 3] >> (state->buffer_bit_position & 7)) & (unsigned int)bit_masks[bits_available]) << bits_assembled;

    bits_needed -= bits_available;
    bits_assembled += bits_available;
    state->buffer_bits_remaining -= bits_available;
    state->buffer_bit_position += bits_available;
  }

  /* When the next code to be added would exceed the current bit width,
     increase the width (up to the 12-bit maximum). */
  if ((state->next_table_code == bit_masks[state->code_bit_width]) && (state->code_bit_width < 12))
    state->code_bit_width++;

  return (code);
}


/* Decodes and returns a single picture from the MG1 file as an RGB z_image.
   
   The decoding process:
   1. Look up the image directory entry by picture number
   2. Initialize the colormap: start with the default EGA palette, then
      override with the image's custom colormap if one exists
   3. If the transparency flag (bit 0) is set, force the transparent
      color (index from bits 12-15) to black
   4. Seek to the compressed pixel data and LZW-decompress it
   5. Map each palette index through the colormap to produce RGB triples
   
   The returned z_image and its pixel data are owned by this module;
   they remain valid until the next call to get_mg1_picture() or
   end_mg1_graphics(). Returns NULL on failure. */
z_image *get_mg1_picture(int picture_number)
{
  int image_index;
  struct mg1_colormap colormap;
  struct mg1_image_entry *entry;
  int i;
  uint8_t *write_ptr;
  short code, previous_code = 0, root_code, clear_code;
  lzw_state_t state;
  unsigned char read_buffer[LZW_TABLE_SIZE];
  int color_index;

  image_index = find_image_index(picture_number);

  if (image_index < 0)
    return NULL;

  entry = &image_directory[image_index];

  /* Start with the default 16-color EGA palette. */
  for (i = 0; i < 16; i++)
  {
    colormap.colors[i].red = ega_colormap[i][0];
    colormap.colors[i].green = ega_colormap[i][1];
    colormap.colors[i].blue = ega_colormap[i][2];
  }

  /* If the image has a custom colormap, read it from the file.
     The colormap replaces entries starting at index 2 (black and
     white at indices 0-1 are always kept from the EGA palette). */
  if (entry->colormap_addr != 0)
  {
    if (fseek(mg1_file, entry->colormap_addr, SEEK_SET) != 0)
      return NULL;

    if (read_uint8(mg1_file, &colormap.nof_colors) == -1)
      return NULL;

    // Fix for some buggy _Arthur_ pictures.
    if (colormap.nof_colors > 14)
      colormap.nof_colors = 14;

    colormap.nof_colors += 2;

    for (i=2; i<colormap.nof_colors; i++)
    {
      if (read_uint8(mg1_file, &colormap.colors[i].red) == -1)
        return NULL;
      if (read_uint8(mg1_file, &colormap.colors[i].green) == -1)
        return NULL;
      if (read_uint8(mg1_file, &colormap.colors[i].blue) == -1)
        return NULL;
    }
  }

  /* If the transparency flag (bit 0) is set, force the transparent
     color index (stored in bits 12-15) to black. */
  if (entry->flags & 1)
  {
    colormap.colors[entry->flags >> 12].red = 0;
    colormap.colors[entry->flags >> 12].green = 0;
    colormap.colors[entry->flags >> 12].blue = 0;
  }

  if (fseek(mg1_file, entry->data_addr, SEEK_SET) != 0)
    return NULL;

  if (rgb_pixel_data != NULL)
      free(rgb_pixel_data);

  /* Allocate the output buffer: 3 bytes (RGB) per pixel. */
  size_t pixel_data_size = (size_t)entry->width * entry->height * 3;
  if ((rgb_pixel_data = (uint8_t*)malloc(pixel_data_size)) == NULL)
    return NULL;

  uint8_t *write_end = rgb_pixel_data + pixel_data_size;
  write_ptr = rgb_pixel_data;

  /* Initialize the LZW decoder. The clear code resets the table;
     the end code (clear_code + 1) terminates the stream. Codes
     start at 9 bits wide and grow up to 12 bits. */
  clear_code = 1 << LZW_MIN_CODE_SIZE;
  state.next_table_code = clear_code + 2;
  state.buffer_bits_remaining = 0;
  state.buffer_bit_position = 0;
  state.code_bit_width = LZW_MIN_CODE_SIZE + 1;

  for (i = 0; i < LZW_TABLE_SIZE; i++)
  {
    lzw_table[i][PREFIX] = LZW_TABLE_SIZE;
    lzw_table[i][PIXEL] = (short)i;
  }

  /* Main LZW decompression loop. Unlike decompress_vga.cpp in terps/bocfel/z6
     (which outputs palette indices), this loop maps each pixel through the
     colormap immediately, writing RGB triples to the output buffer. */
  for (;;)
  {
    code = read_lzw_code(mg1_file, &state, read_buffer);
    if (code < 0 || code == clear_code + 1)
      break;
    if (code >= LZW_TABLE_SIZE) {
      free(rgb_pixel_data);
      rgb_pixel_data = NULL;
      return NULL;
    }

    if (code == clear_code)
    {
      /* Reset the code table to its initial state. */
      state.code_bit_width = LZW_MIN_CODE_SIZE + 1;
      state.next_table_code = clear_code + 2;
      code = read_lzw_code(mg1_file, &state, read_buffer);
      if (code < 0 || code >= LZW_TABLE_SIZE) {
        free(rgb_pixel_data);
        rgb_pixel_data = NULL;
        return NULL;
      }
    }
    else
    {
      /* Walk the prefix chain to find the root pixel of this code's
         string. For the "KwKwK" case (code == next_table_code),
         start from the previous code instead. */
      root_code = (code == state.next_table_code) ? previous_code : code;

      int chain_limit = LZW_TABLE_SIZE;
      while (lzw_table[root_code][PREFIX] != LZW_TABLE_SIZE && --chain_limit > 0) {
        root_code = lzw_table[root_code][PREFIX];
        if (root_code < 0 || root_code >= LZW_TABLE_SIZE) {
          free(rgb_pixel_data);
          rgb_pixel_data = NULL;
          return NULL;
        }
      }

      /* Add a new entry: previous code's string + this string's root pixel. */
      if (state.next_table_code < LZW_TABLE_SIZE) {
        lzw_table[state.next_table_code][PREFIX] = previous_code;
        lzw_table[state.next_table_code][PIXEL] = lzw_table[root_code][PIXEL];
        state.next_table_code++;
      }
    }
    previous_code = code;

    /* Walk the prefix chain, pushing each pixel onto the stack
       (they come out in reverse order). */
    i = 0;
    int chain_limit = LZW_TABLE_SIZE;
    do {
      if (i >= LZW_TABLE_SIZE || code < 0 || code >= LZW_TABLE_SIZE)
        break;
      pixel_stack[i++] = (unsigned char) lzw_table[code][PIXEL];
    } while ((code = lzw_table[code][PREFIX]) != LZW_TABLE_SIZE && --chain_limit > 0);

    /* Pop pixels from the stack in correct order, mapping each
       through the colormap to produce RGB output. */
    do
    {
      color_index = pixel_stack[--i];
      if (color_index >= 16)
        color_index = 0;
      if (write_ptr + 3 > write_end)
        break;
      *(write_ptr++) = colormap.colors[color_index].red;
      *(write_ptr++) = colormap.colors[color_index].green;
      *(write_ptr++) = colormap.colors[color_index].blue;
    }
    while (i > 0);
  }

  /* Wrap the decoded pixel data in a z_image struct for the caller. */
  if (output_image != NULL)
      free(output_image);

  if ((output_image = (z_image*)malloc(sizeof(z_image))) == NULL)
  { free(rgb_pixel_data); rgb_pixel_data = NULL; return NULL; }

    output_image->bits_per_sample = 8;
    output_image->width = entry->width;
    output_image->height = entry->height;
    output_image->image_type = IMAGE_TYPE_RGB;
    output_image->data = rgb_pixel_data;

  return output_image;
}

#endif /* mg1_c_INCLUDED */
