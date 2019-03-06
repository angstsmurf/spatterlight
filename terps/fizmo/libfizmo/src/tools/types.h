
/* types.h
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2009-2017 Christoph Ender.
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
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef types_h_INCLUDED 
#define types_h_INCLUDED

#include <inttypes.h>
#include <stdbool.h>
#include <time.h> // for time_t
#include <sys/types.h> // for off_t

#define Z_COLOUR_UNDEFINED -2
#define Z_COLOUR_UNDER_CURSOR -1
#define Z_COLOUR_CURRENT 0
#define Z_COLOUR_DEFAULT 1
#define Z_COLOUR_BLACK 2
#define Z_COLOUR_RED 3
#define Z_COLOUR_GREEN 4
#define Z_COLOUR_YELLOW 5
#define Z_COLOUR_BLUE 6
#define Z_COLOUR_MAGENTA 7
#define Z_COLOUR_CYAN 8
#define Z_COLOUR_WHITE 9
#define Z_COLOUR_MSDOS_DARKISH_GREY 10
#define Z_COLOUR_AMIGA_LIGHT_GREY 10
#define Z_COLOUR_MEDIUM_GREY 11
#define Z_COLOUR_DARK_GREY 12

#define Z_INVALID_RGB_COLOUR 0xffffffff;

#define Z_FONT_PREVIOUS_FONT 0
#define Z_FONT_NORMAL 1
#define Z_FONT_PICTURE 2
#define Z_FONT_CHARACTER_GRAPHICS 3
#define Z_FONT_COURIER_FIXED_PITCH 4

#define Z_STYLE_ROMAN 0
#define Z_STYLE_REVERSE_VIDEO 1
#define Z_STYLE_BOLD 2
#define Z_STYLE_ITALIC 4
#define Z_STYLE_FIXED_PITCH 8

#define Z_BLORB_TYPE_PICT 0
#define Z_BLORB_TYPE_SOUND 1
#define Z_BLORB_TYPE_EXEC 2

#define bool_equal(a,b) ((a) ? (b) : !(b))
//#define FIZMO_UNIQUE_EXIT_CODE(filecode) (-(((filecode) << 16) | __LINE__))
//#define FIZMO_UNIQUE_EXIT_CODE(filecode) (__LINE__)

#define NOF_Z_COLOURS 8

typedef uint8_t zscii;
typedef uint32_t z_ucs;
typedef int16_t z_colour;
typedef int16_t z_font;
typedef int16_t z_style;
typedef uint32_t z_rgb_colour;


struct commandline_parameter
{
  char* short_parameter_name;
  char* long_parameter_name;
  int i18n_description_code;
};

struct z_file_struct
{
  void *file_object; // Used to store z_filesys_interface-dependent data.
  char *filename;
  int filetype;
  int fileaccess;
  int implementation; // used by glk_if
};

typedef struct z_file_struct z_file;

struct z_dir_struct
{
  void *dir_object;
};

typedef struct z_dir_struct z_dir;

struct z_dir_ent
{
  // Using only d_name since "this is the only field you can count on in
  // all POSIX systems".
  char *d_name;
};

struct z_blorb_map_struct
{
    void *blorb_map_implementation;
};

typedef struct z_blorb_map_struct z_blorb_map;

struct z_story
{
  uint8_t *memory;
  uint8_t version;
  uint16_t release_code;
  char serial_code[7];
  uint16_t checksum;
  char *title;

  z_file *z_story_file;
  z_file *blorb_file;
  z_blorb_map *blorb_map;
  //char *absolute_directory_name;
  char *absolute_file_name;
  long story_file_exec_offset;
  uint8_t *dynamic_memory_end;
  uint8_t *static_memory;
  uint8_t *static_memory_end;
  uint8_t *high_memory;
  uint8_t *high_memory_end;
  uint32_t routine_offset;
  uint32_t string_offset;
  uint8_t *global_variables;
  uint8_t *abbreviations_table;
  uint8_t *property_defaults;
  uint8_t *object_tree;
  uint8_t object_size;
  uint16_t maximum_object_number;
  uint8_t maximum_property_number;
  uint8_t maximum_attribute_number;
  uint8_t object_node_number_index;
  uint8_t object_property_index;
  uint8_t *alphabet_table;
  uint8_t *dictionary_table;
  uint8_t score_mode;

  /*
  int nof_sounds;
  int nof_images;
  int frontispiece_image_no;

  struct z_story_blorb_sound *blorb_sounds;
  struct z_story_blorb_image *blorb_images;
  */

  int max_nof_color_pairs;
};

extern char* z_colour_names[];

bool is_regular_z_colour(z_colour colour);
short color_name_to_z_colour(char *colour_name);
z_rgb_colour new_z_rgb_colour(uint8_t red, uint8_t green, uint8_t blue);
z_rgb_colour z_to_rgb_colour(z_colour z_colour_to_convert);
uint8_t red_from_z_rgb_colour(z_rgb_colour rgb_colour);
uint8_t green_from_z_rgb_colour(z_rgb_colour rgb_colour);
uint8_t blue_from_z_rgb_colour(z_rgb_colour rgb_colour);

#endif /* types_h_INCLUDED */

