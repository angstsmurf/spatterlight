
/* history.h
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2009-2012 Christoph Ender.
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
 */


#ifndef history_h_INCLUDED 
#define history_h_INCLUDED

#include "../tools/tracelog.h"
#include "../tools/types.h"
#include "../tools/z_ucs.h"

#define HISTORY_METADATA_ESCAPE 0

#define HISTORY_METADATA_TYPE_FONT 1
#define HISTORY_METADATA_TYPE_STYLE 2
#define HISTORY_METADATA_TYPE_COLOUR 3

#define HISTORY_METADATA_DATA_OFFSET 13

#define Z_HISTORY_INCREMENT_SIZE 8*1024 // given in units of z_ucs
#define Z_HISTORY_MAXIMUM_SIZE 128*1024*1024 // given in units of z_ucs
#define Z_HISTORY_METADATA_STATE_BLOCK_SIZE 16*1024 // given in units of z_ucs


typedef struct
{
  int window_number;

  size_t z_history_buffer_size;
  size_t z_history_maximum_buffer_size; // unit is z_ucs.
  size_t z_history_buffer_increment_size;

  z_ucs *z_history_buffer_start; // this will be the result of malloc()
  z_ucs *z_history_buffer_end;

  // In general, z_history_buffer_front_index denotes the next write position.
  // In case z_history_buffer_front_index == z_history_buffer_back_index,
  // wrapped_around has to be checked to distinguish between an empty buffer
  // and a full, wrapped-around buffer.
  z_ucs *z_history_buffer_front_index;
  z_ucs *z_history_buffer_back_index;
  bool wrapped_around;

  long int last_metadata_block_index;

  // These four values are set when fizmo opens the interface (and when the
  // values are used in the current version). They keep the current state of
  // the back-end index of the history queue. This is necessary, since
  // for example the command to set colour to a given value has already
  // been overwritten in the history, but the text using the colour is still
  // in the buffer.
  z_font history_buffer_back_index_font;
  z_style history_buffer_back_index_style;
  z_colour history_buffer_back_index_foreground;
  z_colour history_buffer_back_index_background;

  z_font history_buffer_front_index_font;
  z_style history_buffer_front_index_style;
  z_colour history_buffer_front_index_foreground;
  z_colour history_buffer_front_index_background;
} OUTPUTHISTORY;

typedef struct
{
  void (*set_text_style)(z_style text_style);
  void (*set_colour)(z_colour foreground, z_colour background, int16_t window);
  void (*set_font)(z_font font_type);
  void (*z_ucs_output)(z_ucs *z_ucs_output);
} history_output_target;


typedef struct
{
  OUTPUTHISTORY *history;

  history_output_target *target;
  z_ucs *current_paragraph_index;
  bool has_wrapped;
  bool found_end_of_buffer;
  bool first_iteration_done;

  bool metadata_at_index_evaluated;
  z_font font_at_index;
  z_style style_at_index;
  z_colour foreground_at_index;
  z_colour background_at_index;

  // In case the last metadata-evaluation for the last paragraph had to use
  // the metadata-state-block for any metadata-element, these values are
  // stored in the "last_user_metadata_state_*" variables. In case some
  // values were not used, -1 should is stored for the current block.
  long int last_rewinded_paragraphs_block_index;
  z_font last_used_metadata_state_font;
  z_style last_used_metadata_state_style;
  z_colour last_used_metadata_state_foreground;
  z_colour last_used_metadata_state_background;

  // Storage for state-saving:
  z_ucs *saved_current_paragraph_index;
  bool saved_has_wrapped;
  bool saved_found_end_of_buffer;
  bool saved_first_iteration_done;
  bool saved_metadata_at_index_evaluated;
  z_font saved_font_at_index;
  z_style saved_style_at_index;
  z_colour saved_foreground_at_index;
  z_colour saved_background_at_index;
  long int saved_last_rewinded_paragraphs_block_index;
  z_font saved_last_used_metadata_state_font;
  z_style saved_last_used_metadata_state_style;
  z_colour saved_last_used_metadata_state_foreground;
  z_colour saved_last_used_metadata_state_background;
} history_output;


typedef OUTPUTHISTORY *outputhistory_ptr;

#ifndef history_c_INCLUDED
extern outputhistory_ptr outputhistory[];
#endif

OUTPUTHISTORY *create_outputhistory(int window_number,
    size_t maximum_buffer_size, size_t buffer_increment_size,
    z_colour foreground_colour, z_colour background_color, z_font font,
    z_style style);
void destroy_outputhistory(OUTPUTHISTORY *history);
void store_z_ucs_output_in_history(OUTPUTHISTORY *history,
    z_ucs *z_ucs_output);
int store_metadata_in_history(OUTPUTHISTORY *history, int metadata_type, ...);
void store_data_in_history(OUTPUTHISTORY *h, z_ucs *data, size_t len,
    bool evaluate_state_block);
int remove_chars_from_history(OUTPUTHISTORY *history, int nof_chars);
z_ucs *get_current_line(OUTPUTHISTORY *history);
history_output *init_history_output(OUTPUTHISTORY *h, history_output_target *t);
int output_rewind_paragraph(history_output *output);
int output_repeat_paragraphs(history_output *output, int n,
    bool include_metadata, bool advance_history_pointer);
void destroy_history_output(history_output *output);
void remember_history_output_position(history_output *output);
void restore_history_output_position(history_output *output);
size_t get_allocated_text_history_size(OUTPUTHISTORY *h);

#endif /* history_h_INCLUDED */

