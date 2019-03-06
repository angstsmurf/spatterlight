
/* wordwrap.h
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


#ifndef wordwrap_h_INCLUDED
#define wordwrap_h_INCLUDED

#include "../tools/types.h"


struct wordwrap_metadata
{
  long output_index;
  void (*metadata_output_function)(void *ptr_parameter, uint32_t int_parameter);
  void *ptr_parameter;
  uint32_t int_parameter;
};


typedef struct
{
  int line_length;
  void (*wrapped_text_output_destination)(z_ucs *output, void *parameter);
  void *destination_parameter;
  bool add_newline_after_full_line;
  int left_side_padding;
  bool flush_after_newline;
  bool enable_hyphenation;

  z_ucs *input_buffer;
  long input_buffer_size;
  long input_index;
  long chars_already_on_line;

  struct wordwrap_metadata *metadata;
  int metadata_size;
  int metadata_index;

  z_ucs *padding_buffer;
  /*
  void (*padding_starts)();
  void (*padding_ends)();
  void *destination_parameter;
  z_ucs *output_buffer;
  z_ucs *output_buffer_index;
  size_t output_buffer_size;
  bool add_newline_after_full_line;
  int left_side_padding;
  bool newline_was_inserted;
  bool last_line_was_completely_filled;
  bool flush_after_newline;
  */
} WORDWRAP;


WORDWRAP *wordwrap_new_wrapper(size_t line_length,
    void (*wrapped_text_output_destination)(z_ucs *output, void *parameter),
    void *destination_parameter, bool add_newline_after_full_line,
    int left_side_padding, bool flush_after_newline, bool enable_hyphenation);
void wordwrap_destroy_wrapper(WORDWRAP *wrapper_to_destroy);
void wordwrap_wrap_z_ucs(WORDWRAP *wrapper, z_ucs *input);
void wordwrap_flush_output(WORDWRAP *wrapper);
void wordwrap_insert_metadata(WORDWRAP *wrapper,
    void (*metadata_output)(void *ptr_parameter, uint32_t int_parameter),
    void *ptr_parameter, uint32_t int_parameter);
void wordwrap_set_line_index(WORDWRAP *wrapper, int new_line_index);
//void wordwrap_output_left_padding(WORDWRAP *wrapper);
void wordwrap_adjust_line_length(WORDWRAP *wrapper, size_t new_line_length);
// To remove chars from the end of the current(!) line:
void wordwrap_remove_chars(WORDWRAP *wrapper, size_t num_chars_to_remove);

#endif /* wordwrap_h_INCLUDED */

