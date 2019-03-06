
/* screen_interface.h
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


#ifndef screen_interface_h_INCLUDED
#define screen_interface_h_INCLUDED

#include <inttypes.h>
#include <stdio.h>
#include "../tools/types.h"

struct z_screen_interface
{
  char* (*get_interface_name)();

  bool (*is_status_line_available)();
  bool (*is_split_screen_available)();
  bool (*is_variable_pitch_font_default)();
  bool (*is_colour_available)();
  bool (*is_picture_displaying_available)();
  bool (*is_bold_face_available)();
  bool (*is_italic_available)();
  bool (*is_fixed_space_font_available)();
  bool (*is_timed_keyboard_input_available)();
  bool (*is_preloaded_input_available)();
  bool (*is_character_graphics_font_availiable)();
  bool (*is_picture_font_availiable)();
  uint16_t (*get_screen_height_in_lines)();
  uint16_t (*get_screen_width_in_characters)();
  uint16_t (*get_screen_width_in_units)();
  uint16_t (*get_screen_height_in_units)();
  uint8_t (*get_font_width_in_units)();
  uint8_t (*get_font_height_in_units)();
  z_colour (*get_default_foreground_colour)();
  z_colour (*get_default_background_colour)();
  uint8_t (*get_total_width_in_pixels_of_text_sent_to_output_stream_3)();

  int (*parse_config_parameter)(char *key, char *value);
  char* (*get_config_value)(char *key);
  char** (*get_config_option_names)();

  // This function is called from the interpreter once the story has been
  // read into memory and variables like version etc habe been initialized.
  // This allows the interface to check if the version is supported at all,
  // allocate the right number of windows, and do other version- and story-
  // dependent stuff.
  void (*link_interface_to_story)(struct z_story *story);

  // "reset_interface" Called in case of a restart.
  void (*reset_interface)();

  // close_interface is called if either the game is quit by the "QUIT"
  // opcode or in case an error has occured. In case of a regular quit,
  // the "error_message" is null.
  int (*close_interface)(/*@null@*/ z_ucs *error_message);

  void (*set_buffer_mode)(uint8_t new_buffer_mode);
  void (*z_ucs_output)(z_ucs *z_ucs_output);

  // For version <= 4 games, only the first to parameters are used. For version
  // 5+ games, both "tenth_seconds" and "verification_routine" are set in
  // case input-timeout is active. This "read_line" routine must return the
  // final number of chars in the input or -1 in case timeout-input was
  // active and the verification_routine returned true.
  int16_t (*read_line)(zscii *dest, uint16_t maximum_length,
      uint16_t tenth_seconds, uint32_t verification_routine,
      uint8_t preloaded_input, int *tenth_seconds_elapsed,
      bool disable_command_history, bool return_on_escape);

  // Returns the input char in the uint8_t range and -1 for timed out input.
  int (*read_char)(uint16_t tenth_seconds, uint32_t verification_routine,
      int *tenth_seconds_elapsed);

  void (*show_status)(z_ucs *room_description, int status_line_mode,
      int16_t parameter1, int16_t parameter2);
  void (*set_text_style)(z_style text_style);
  void (*set_colour)(z_colour foreground, z_colour background, int16_t window);
  void (*set_font)(z_font font_type);
  void (*split_window)(int16_t nof_lines);
  void (*set_window)(int16_t window_number);
  void (*erase_window)(int16_t window_number);

  // Must allow moving of cursor in lower window to make print_table work.
  void (*set_cursor)(int16_t line, int16_t column, int16_t window);

  uint16_t (*get_cursor_row)();
  uint16_t (*get_cursor_column)();
  void (*erase_line_value)(uint16_t start_position);
  void (*erase_line_pixels)(uint16_t start_position);
  void (*output_interface_info)();
  bool (*input_must_be_repeated_by_story)();
  void (*game_was_restored_and_history_modified)(); // interface might want
  // to redraw the screen
  int (*prompt_for_filename)(char *filename_suggestion, z_file **result_file,
      char *directory, int filetype_or_mode, int fileaccess); // optional
  // UI-specific filename dialog. If not implemented, return -3. Return >=0 on
  // k, -1 on error or -2 in case user cancelled (ESC or similar).

  // interface custom procedures for autosave (at @read time) and restore
  int (*do_autosave)(); // optional
  int (*restore_autosave)(z_file *savefile); // optional
};

#endif /* screen_interface_h_INCLUDED */

