
/* streams.c
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


#ifndef streams_c_INCLUDED
#define streams_c_INCLUDED

#include <string.h>

#include "../screen_interface/screen_interface.h"
#include "../tools/types.h"
#include "../tools/tracelog.h"
#include "../tools/i18n.h"
#include "../tools/z_ucs.h"
#include "../tools/unused.h"
#include "../tools/filesys.h"
#include "streams.h"
#include "config.h"
#include "fizmo.h"
#include "wordwrap.h"
#include "text.h"
#include "zpu.h"
#include "output.h"
#include "../locales/libfizmo_locales.h"

#ifndef DISABLE_BLOCKBUFFER
#include "blockbuf.h"
#endif /* DISABLE_BLOCKBUFFER */

#ifndef DISABLE_OUTPUT_HISTORY
#include "history.h"
#endif /* DISABLE_OUTPUT_HISTORY */

#define ASCII_TO_Z_UCS_BUFFER_SIZE 64
#define FONT3_CONVERSION_BUF_SIZE 128

//int stream_active[5] = { 0, 1, 0, 0, 0 };
bool stream_1_active = true;
bool stream_2_filename_stored = false;
// stream2 is stored in dynamic memory at z_mem[0x11].
// stream3 is active when stream_3_current_depth >= -1
bool stream_4_active = false;
/*@dependent@*/ /*@null@*/ static z_file *stream_2 = NULL;
/*@only@*/ char *stream_2_filename = NULL;
static size_t stream_2_filename_size = 0;
/*@only@*/ static WORDWRAP *stream_2_wrapper = NULL;
static zscii current_filename_buffer[MAXIMUM_SCRIPT_FILE_NAME_LENGTH+1];
z_ucs last_script_filename[MAXIMUM_SCRIPT_FILE_NAME_LENGTH + 1];
static bool script_wrapper_active = true;
static uint8_t *stream_3_start[MAXIMUM_STREAM_3_DEPTH];
static uint8_t *stream_3_index[MAXIMUM_STREAM_3_DEPTH];
static int stream_3_current_depth = -1;
z_file *stream_4 = NULL;
/*@only@*/ static char *stream_4_filename;
static size_t stream_4_filename_size = 0;
z_ucs last_stream_4_filename[MAXIMUM_SCRIPT_FILE_NAME_LENGTH + 1];
z_ucs last_input_stream_filename[MAXIMUM_SCRIPT_FILE_NAME_LENGTH + 1];
static bool stream_4_init_underway = false;
char *input_stream_1_filename = NULL;
size_t input_stream_1_filename_size = 0;
z_file *input_stream_1 = NULL;
static bool input_stream_init_underway = false;
bool input_stream_1_active = false;
bool input_stream_1_was_already_active = false;
static z_ucs stream_2_preload_buffer[STREAM_2_PRELOAD_BUFFER_SIZE];
static z_ucs *stream_2_preload_buffer_index = stream_2_preload_buffer;
static z_ucs *stream_2_preload_output_start = stream_2_preload_buffer;
static z_ucs *stream_2_preload_buffer_last_index = (
    stream_2_preload_buffer + STREAM_2_PRELOAD_BUFFER_SIZE - 1);

// This flag is used for the timed input. Since the verification routine
// may print some text to the screen, the interpreter must be able to
// restore the input line once it resumes input.
bool stream_output_has_occured = false;
static bool stream_2_init_underway = false;
static bool stream_4_was_already_active = false;
static bool stream_2_wrapping_disabled;
static int stream2margin;



void open_streams()
{
#ifdef ENABLE_TRACING
  turn_on_trace();
#endif /* ENABLE_TRACING */
}


static void stream_2_output_destination(z_ucs *z_ucs_output,
    void *UNUSED(dummy))
{
  if (*z_ucs_output != 0)
  {
    fsi->writeucsstring(z_ucs_output, stream_2);
    if (strcmp(get_configuration_value("sync-transcript"), "true") == 0)
      fsi->flushfile(stream_2);
  }
}


static void flush_stream_2_buffer_output() {
  z_ucs *index;
  z_ucs *newline_index = NULL;
  z_ucs buf;

  TRACE_LOG("stream_2_preload_flush:\n");

  index
    = stream_2_preload_buffer_index == stream_2_preload_buffer
    ? stream_2_preload_buffer_last_index - 1
    : stream_2_preload_buffer_index -1;

  while (index != stream_2_preload_output_start) {
    if (*index == '\n') {
      newline_index = index;
      break;
    }
    if (index == stream_2_preload_buffer) {
      index = stream_2_preload_buffer_last_index;
    }
    index--;
  }

  if (newline_index == NULL) {
    index = stream_2_preload_output_start + (STREAM_2_PRELOAD_BUFFER_SIZE / 2);
    if (index >= stream_2_preload_buffer_last_index) {
      index = stream_2_preload_buffer
        + (index - stream_2_preload_buffer_last_index);
    }
  }
  else {
    if ((index = newline_index + 1) == stream_2_preload_buffer_last_index) {
      index = stream_2_preload_buffer;
    }
  }

  if (stream_2_preload_output_start > index) {
    TRACE_LOG("stream_2_preload in wrap-around.\n");
    TRACE_LOG("flushs2: \"");
    TRACE_LOG_Z_UCS(stream_2_preload_output_start);
    TRACE_LOG("\".\n");
    // In wrap-around
    stream_2_output_destination(stream_2_preload_output_start, NULL);
    stream_2_preload_output_start = stream_2_preload_buffer;
  }

  buf = *index;
  *index = 0;
  TRACE_LOG("flushs2:\"");
  TRACE_LOG_Z_UCS(stream_2_preload_output_start);
  TRACE_LOG("\".\n");
  stream_2_output_destination(stream_2_preload_output_start, NULL);
  *index = buf;
  stream_2_preload_output_start = index;
}


static void stream_2_buffer_output(z_ucs *z_ucs_output)
{
  //z_ucs buf;

  while (*z_ucs_output != 0) {
    TRACE_LOG("stream_2_preload_buffer_index: %x\n",
        stream_2_preload_buffer_index);
    TRACE_LOG("stream_2_preload_buffer_last_index: %x\n",
        stream_2_preload_buffer_last_index);
    TRACE_LOG("stream_2_preload_output_start: %x\n",
        stream_2_preload_output_start);

    if (stream_2_preload_buffer_index == stream_2_preload_buffer_last_index) {
      // output-start following buffer_index in wrap-around:
      if (stream_2_preload_output_start == stream_2_preload_buffer) {
        flush_stream_2_buffer_output();
      }
      stream_2_preload_buffer_index = stream_2_preload_buffer;
    }

    // output-start following buffer_index as non-wrap-around:
    if (stream_2_preload_buffer_index + 1 == stream_2_preload_output_start) {
      flush_stream_2_buffer_output();
    }

    TRACE_LOG("Writing %c to %x.\n",
        *z_ucs_output, stream_2_preload_buffer_index);
    *stream_2_preload_buffer_index = *z_ucs_output;
    stream_2_preload_buffer_index++;
    z_ucs_output++;
  }
}


int stream_2_buffer_remove_chars(size_t nof_chars_to_remove) {
  while (nof_chars_to_remove > 0) {

    if (stream_2_preload_buffer_index == stream_2_preload_output_start) {
      TRACE_LOG("remove-s2 hit output start.\n");
      return -1;
    }

    if (stream_2_preload_buffer_index == stream_2_preload_buffer) {
      stream_2_preload_buffer_index = stream_2_preload_buffer_last_index - 1;
    }
    else {
      stream_2_preload_buffer_index--;
    }
    TRACE_LOG("Removing %c.\n", *stream_2_preload_buffer_index);
    TRACE_LOG("stream_2_preload_buffer_index: %x\n",
        stream_2_preload_buffer_index);
    TRACE_LOG("stream_2_preload_buffer_last_index: %x\n",
        stream_2_preload_buffer_last_index);
    TRACE_LOG("stream_2_preload_output_start: %x\n",
        stream_2_preload_output_start);

    nof_chars_to_remove--;
  }
  return 0;
}


void init_streams()
{
  char *src;
  size_t bytes_required;
  char *stream_2_line_width = get_configuration_value("stream-2-line-width");
  char *stream_2_left_margin = get_configuration_value("stream-2-left-margin");
  stream_2_wrapping_disabled
    = (strcmp(get_configuration_value("disable-stream-2-wrap"), "true") == 0);
  int stream2width;

  stream2width
    = stream_2_line_width != NULL
    ? atoi(stream_2_line_width)
    : DEFAULT_STREAM_2_LINE_WIDTH;

  stream2margin
    = stream_2_left_margin != NULL
    ? atoi(stream_2_left_margin)
    : DEFAULT_STREAM_2_LEFT_PADDING;

  if (stream_2_wrapping_disabled == false)
    stream_2_wrapper = wordwrap_new_wrapper(
        stream2width,
        &stream_2_output_destination,
        NULL,
        true,
        stream2margin,
        (strcmp(get_configuration_value("sync-transcript"), "true") == 0
         ? true
         : false),
        (strcmp(get_configuration_value("disable-stream-2-hyphenation")
                ,"true") == 0
         ? false
         : true));

  *stream_2_preload_buffer_last_index = 0;

  if (strcmp(get_configuration_value("start-script-when-story-starts"),
        "true") == 0)
    z_mem[0x11] |= 1;

  if (strcmp(get_configuration_value(
          "start-command-recording-when-story-starts"), "true") == 0)
    stream_4_active = true;

  if ((src = get_configuration_value("transcript-filename")) != NULL)
  {
    bytes_required = strlen(src) + 1;
    if ((stream_2_filename == NULL) || (bytes_required>stream_2_filename_size))
    {
      stream_2_filename
        = (char*)fizmo_realloc(stream_2_filename, bytes_required);
      stream_2_filename_size = bytes_required;
    }

    strcpy(stream_2_filename, src);
    stream_2_filename_stored = true;
  }
  else
    src = DEFAULT_TRANSCRIPT_FILE_NAME;

  (void)latin1_string_to_zucs_string(
      last_script_filename,
      src,
      strlen(src) + 1);

  TRACE_LOG("Converted script default filename: '");
  TRACE_LOG_Z_UCS(last_script_filename);
  TRACE_LOG("'.\n");

  if ((src = get_configuration_value("input-command-filename")) != NULL)
  {
    bytes_required = strlen(src) + 1;
    if ((input_stream_1_filename == NULL)
        || (bytes_required > input_stream_1_filename_size))
    {
      input_stream_1_filename
        = (char*)fizmo_realloc(input_stream_1_filename, bytes_required);
      input_stream_1_filename_size = bytes_required;
    }

    strcpy(input_stream_1_filename, src);
    input_stream_1_was_already_active = true;
  }
  else
    src = DEFAULT_INPUT_COMMAND_FILE_NAME;

  (void)latin1_string_to_zucs_string(
      last_input_stream_filename,
      src,
      strlen(src) + 1);

  if ((src = get_configuration_value("record-command-filename")) != NULL)
  {
    bytes_required = strlen(src) + 1;
    if ((stream_4_filename == NULL) || (bytes_required>stream_4_filename_size))
    {
      stream_4_filename
        = (char*)fizmo_realloc(stream_4_filename, bytes_required);
      stream_4_filename_size = bytes_required;
    }

    strcpy(stream_4_filename, src);
    stream_4_was_already_active = true;
  }
  else
    src = DEFAULT_RECORD_COMMAND_FILE_NAME;

  (void)latin1_string_to_zucs_string(
      last_stream_4_filename,
      src,
      strlen(src) + 1);

  if (strcmp(get_configuration_value(
          "start-file-input-when-story-starts"), "true") == 0)
    input_stream_1_active = true;
}

z_file *get_stream_2(void)
{
  return stream_2;
}


/* Accept a new open stream as the current stream_2. The previous stream_2
   is closed. Pass in NULL to just close the previous stream_2.
   The interpreter's transcript bit is set appropriately.
 */
void restore_stream_2(z_file *str)
{
  if (stream_2) {
    (void)fsi->closefile(stream_2);
    stream_2 = NULL;
    z_mem[0x11] &= 0xfe;
  }
  if (str) {
    stream_2 = str;
    z_mem[0x11] |= 1;
  }
}

void ask_for_input_stream_filename(void)
{
  bool stream_1_active_buf;
  int16_t input_length;
  int16_t i;
  size_t bytes_required;
  z_ucs *ptr;
  int len;

  input_stream_init_underway = true;

  stream_1_active_buf = stream_1_active;
  stream_1_active = true;
  (void)i18n_translate(
      libfizmo_module_name,
      i18n_libfizmo_PLEASE_ENTER_NAME_FOR_COMMANDFILE);

  (void)streams_latin1_output("\n>");

  (void)streams_z_ucs_output(last_input_stream_filename);
  len = z_ucs_len(last_input_stream_filename);
  stream_2_remove_chars(len);
#ifndef DISABLE_OUTPUT_HISTORY
  remove_chars_from_history(outputhistory[active_window_number], len);
#endif /* DISABLE_OUTPUT_HISTORY */

  for (i=0; i<len; i++)
  {
    current_filename_buffer[i]
      = unicode_char_to_zscii_input_char(last_input_stream_filename[i]);
  }

  do
  {
    input_length
      = active_interface->read_line(
          (zscii*)current_filename_buffer,
          MAXIMUM_SCRIPT_FILE_NAME_LENGTH,
          0,
          0,
          len,
          NULL,
          true,
          false);

    if (input_length == 0)
    {
      if (streams_latin1_output("\n") != 0)
        i18n_translate_and_exit(
            libfizmo_module_name,
            i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
            -0x0100,
            "streams_latin1_output");

      if (i18n_translate(
            libfizmo_module_name,
            i18n_libfizmo_FILENAME_MUST_NOT_BE_EMPTY) == (size_t)-1)
        i18n_translate_and_exit(
            libfizmo_module_name,
            i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
            -0x0100,
            "i18n_translate");

      if (streams_latin1_output("\n") != 0)
        i18n_translate_and_exit(
            libfizmo_module_name,
            i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
            -0x0100,
            "streams_latin1_output");
    }
  }
  while (input_length == 0);

  for (i=0; i<input_length; i++)
    last_script_filename[i]
      = zscii_input_char_to_z_ucs(current_filename_buffer[i]);
  last_script_filename[i] = 0;

  stream_1_active = active_interface->input_must_be_repeated_by_story();
  (void)streams_z_ucs_output_user_input(last_script_filename);
  (void)streams_z_ucs_output_user_input(z_ucs_newline_string);

  stream_1_active = stream_1_active_buf;

  TRACE_LOG("From ZSCII translated filename: \"");
  TRACE_LOG_Z_UCS(last_script_filename);
  TRACE_LOG("\".\n");

  ptr = last_script_filename;
  bytes_required = (size_t)zucs_string_to_utf8_string(NULL, &ptr, 0);

  if (
      (input_stream_1_filename == NULL)
      ||
      (bytes_required > input_stream_1_filename_size)
     )
  {
    TRACE_LOG("(Re-)allocating %zd bytes.\n", bytes_required);

    input_stream_1_filename
      = (char*)fizmo_realloc(input_stream_1_filename, bytes_required);

    input_stream_1_filename_size = bytes_required;
  }

  ptr = last_script_filename;

  TRACE_LOG("From ZSCII translated filename: \"");
  TRACE_LOG_Z_UCS(ptr);
  TRACE_LOG("\".\n");

  // FIXME: Charsets may differ on operating systems.
  (void)zucs_string_to_utf8_string(
      input_stream_1_filename,
      &ptr,
      bytes_required);

  TRACE_LOG("Converted filename: '%s'.\n", input_stream_1_filename);

  input_stream_init_underway = false;
}


void ask_for_stream2_filename()
{
  bool stream_1_active_buf;
  int16_t input_length;
  int16_t i;
  size_t bytes_required;
  z_ucs *ptr;
  int len;

  stream_2_init_underway = true;

  stream_1_active_buf = stream_1_active;
  stream_1_active = true;
  (void)i18n_translate(
      libfizmo_module_name,
      i18n_libfizmo_PLEASE_ENTER_SCRIPT_FILENAME);

  (void)streams_latin1_output("\n>");

  if (streams_z_ucs_output(last_script_filename) != 0)
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
        -0x0100,
        "streams_z_ucs_output");

  len = z_ucs_len(last_script_filename);
  stream_2_remove_chars(len);
#ifndef DISABLE_OUTPUT_HISTORY
  remove_chars_from_history(outputhistory[active_window_number], len);
#endif /* DISABLE_OUTPUT_HISTORY */

  for (i=0; i<len; i++)
  {
    current_filename_buffer[i]
      = unicode_char_to_zscii_input_char(last_script_filename[i]);
  }

  do
  {
    input_length
      = active_interface->read_line(
          (zscii*)current_filename_buffer,
          MAXIMUM_SCRIPT_FILE_NAME_LENGTH,
          0,
          0,
          len,
          NULL,
          true,
          false);

    if (input_length == 0)
    {
      if (streams_latin1_output("\n") != 0)
        i18n_translate_and_exit(
            libfizmo_module_name,
            i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
            -0x0100,
            "streams_latin1_output");

      if (i18n_translate(
            libfizmo_module_name,
            i18n_libfizmo_FILENAME_MUST_NOT_BE_EMPTY) == (size_t)-1)
        i18n_translate_and_exit(
            libfizmo_module_name,
            i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
            -0x0100,
            "i18n_translate");

      if (streams_latin1_output("\n") != 0)
        i18n_translate_and_exit(
            libfizmo_module_name,
            i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
            -0x0100,
            "streams_latin1_output");
    }
  }
  while (input_length == 0);

  for (i=0; i<input_length; i++)
    last_script_filename[i]
      = zscii_input_char_to_z_ucs(current_filename_buffer[i]);
  last_script_filename[i] = 0;

  stream_1_active = active_interface->input_must_be_repeated_by_story();
  (void)streams_z_ucs_output_user_input(last_script_filename);
  (void)streams_z_ucs_output_user_input(z_ucs_newline_string);

  stream_1_active = stream_1_active_buf;

  TRACE_LOG("From ZSCII translated filename: \"");
  TRACE_LOG_Z_UCS(last_script_filename);
  TRACE_LOG("\".\n");

  ptr = last_script_filename;
  bytes_required = (size_t)zucs_string_to_utf8_string(NULL, &ptr, 0);

  if (
      (stream_2_filename == NULL)
      ||
      (bytes_required > stream_2_filename_size)
     )
  {
    TRACE_LOG("(Re-)allocating %zd bytes.\n", bytes_required);

    stream_2_filename
      = (char*)fizmo_realloc(stream_2_filename, bytes_required);

    stream_2_filename_size = bytes_required;
  }

  ptr = last_script_filename;

  TRACE_LOG("From ZSCII translated filename: \"");
  TRACE_LOG_Z_UCS(ptr);
  TRACE_LOG("\".\n");

  // FIXME: Charsets may differ on operating systems.
  (void)zucs_string_to_utf8_string(
      stream_2_filename,
      &ptr,
      bytes_required);

  TRACE_LOG("Converted filename: '%s'.\n", stream_2_filename);

  stream_2_filename_stored = true;
  stream_2_init_underway = false;
}


static void stream_2_output_write(z_ucs *z_ucs_output)
{
  if (stream_2_wrapping_disabled == true)
    stream_2_buffer_output(z_ucs_output);
  else
    wordwrap_wrap_z_ucs(stream_2_wrapper, z_ucs_output);
}


static void stream_2_print_header()
{
  z_ucs dashes[] = { '-', '-', '-', '\n', '\n', 0 };

  stream_2_output_write(z_ucs_newline_string);
  stream_2_output_write(dashes);
}


void stream_2_remove_chars(size_t nof_chars_to_remove) {
  if (stream_2 != NULL) {
    if (stream_2_wrapping_disabled == true) {
      stream_2_buffer_remove_chars(nof_chars_to_remove);
    }
    else {
      wordwrap_remove_chars(stream_2_wrapper, nof_chars_to_remove);
    }
  }
}


static void stream_2_output(z_ucs *z_ucs_output)
{
  int return_code;
  z_file *transcript_stream = NULL;

  if (
      (active_interface == NULL)
      ||
      (active_window_number != 0)
      ||
      (stream_2_init_underway == true)
     )
    return ;

  if (stream_2 == NULL)
  {
    if (stream_2_filename_stored == false)
    {
      return_code = active_interface->prompt_for_filename(
          "transcript",
          &transcript_stream,
          NULL,
          FILETYPE_TRANSCRIPT,
          FILEACCESS_APPEND);

      if (return_code == -3)
      {
        // No support for "prompt_for_filename" in screen_interface, default
        // to built-in method.
        ask_for_stream2_filename();
        // This method will not open a file, but instead store the filename in
        //"stream_2_filename".
        TRACE_LOG("Opening script-file '%s' for writing.\n", stream_2_filename);
        stream_2 = fsi->openfile(
            stream_2_filename, FILETYPE_TRANSCRIPT, FILEACCESS_APPEND);
      }
      else if (return_code < 0)
      {
        /* The user cancelled out. We'll have to silently turn off stream 2.
           Not the best option, but the best option I can see how to
           do. */
        z_mem[0x11] &= 0xfe;
        return;
      }
      else
      {
        stream_2 = transcript_stream;
      }
    }
    else
    {
      stream_2 = fsi->openfile(
          stream_2_filename, FILETYPE_TRANSCRIPT, FILEACCESS_APPEND);
    }

    stream_2_print_header();
  }

  if (bool_equal(lower_window_buffering_active, true))
  {
    if (script_wrapper_active == false)
    {
      flush_stream_2_buffer_output();
      script_wrapper_active = true;
    }
    stream_2_output_write(z_ucs_output);
  }
  else
  {
    if (bool_equal(script_wrapper_active, true))
    {
      if (stream_2_wrapping_disabled == false)
        wordwrap_flush_output(stream_2_wrapper);
      script_wrapper_active = false;
    }
    stream_2_buffer_output(z_ucs_output);
  }
}


void ask_for_stream4_filename_if_required(void)
{
  bool stream_1_active_buf;
  int16_t input_length;
  int16_t i;
  size_t bytes_required;
  z_ucs *ptr;
  int len;
  int return_code;

  if (
      (bool_equal(stream_4_active, true))
      &&
      (stream_4_was_already_active == false)
      &&
      (stream_4_init_underway == false)
     )
  {
    stream_4_init_underway = true;

    return_code = active_interface->prompt_for_filename(
        "transcript",
        &stream_4,
        NULL,
        FILETYPE_INPUTRECORD,
        FILEACCESS_APPEND);

    if (return_code >= 0)
    {
      // Success.
      stream_4_was_already_active = true;
      stream_4_init_underway = false;
    }
    else if ( (return_code == -1) || (return_code == -2) )
    {
      stream_4_active = false;
    }
    else if (return_code == -3)
    {
      // No support for UI-specific filename prompt.

      stream_1_active_buf = stream_1_active;
      stream_1_active = true;

      do
      {
        (void)streams_latin1_output("\n");

        (void)i18n_translate(
            libfizmo_module_name,
            i18n_libfizmo_PLEASE_ENTER_NAME_FOR_COMMANDFILE);

        (void)streams_latin1_output("\n>");

        (void)streams_z_ucs_output(last_stream_4_filename);

        len = z_ucs_len(last_stream_4_filename);
        stream_2_remove_chars(len);
#ifndef DISABLE_OUTPUT_HISTORY
        remove_chars_from_history(outputhistory[active_window_number], len);
#endif /* DISABLE_OUTPUT_HISTORY */

        for (i=0; i<len; i++)
        {
          current_filename_buffer[i]
            = unicode_char_to_zscii_input_char(last_stream_4_filename[i]);
        }

        input_length
          = active_interface->read_line(
              (zscii*)current_filename_buffer,
              MAXIMUM_SCRIPT_FILE_NAME_LENGTH,
              0,
              0,
              len,
              NULL,
              true,
              false);

        if (input_length == 0)
        {
          if (streams_latin1_output("\n") != 0)
            i18n_translate_and_exit(
                libfizmo_module_name,
                i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
                -0x0100,
                "streams_latin1_output");

          if (i18n_translate(
                libfizmo_module_name,
                i18n_libfizmo_FILENAME_MUST_NOT_BE_EMPTY) == (size_t)-1)
            i18n_translate_and_exit(
                libfizmo_module_name,
                i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
                -0x0100,
                "i18n_translate");

          if (streams_latin1_output("\n") != 0)
            i18n_translate_and_exit(
                libfizmo_module_name,
                i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
                -0x0100,
                "streams_latin1_output");
        }
      }
      while (input_length == 0);

      stream_1_active = stream_1_active_buf;

      for (i=0; i<input_length; i++)
        last_stream_4_filename[i]
          = zscii_input_char_to_z_ucs(current_filename_buffer[i]);
      last_stream_4_filename[i] = 0;

      (void)streams_z_ucs_output(last_stream_4_filename);
      (void)streams_latin1_output("\n");

      TRACE_LOG("From ZSCII translated filename: \"");
      TRACE_LOG_Z_UCS(last_stream_4_filename);
      TRACE_LOG("\".\n");

      ptr = last_stream_4_filename;
      bytes_required = (size_t)zucs_string_to_utf8_string(NULL, &ptr, 0);

      if (
          (stream_4_filename == NULL)
          ||
          (bytes_required > stream_4_filename_size)
         )
      {
        TRACE_LOG("(Re-)allocating %zd bytes.\n", bytes_required);

        stream_4_filename
          = (char*)fizmo_realloc(stream_4_filename, bytes_required);

        stream_4_filename_size = bytes_required;
      }

      // FIXME: Charsets may differ on operating systems.
      ptr = last_stream_4_filename;
      (void)zucs_string_to_utf8_string(
          stream_4_filename,
          &ptr,
          bytes_required);

      stream_4_was_already_active = true;
      stream_4_init_underway = false;
    }
  }
}


void stream_4_latin1_output(char *latin1_output)
{
  if (
      (active_interface == NULL)
      ||
      (active_window_number != 0)
     )
    return ;

  // We'll ask at this point for the filename, since asking directly when
  // the OUTPUT_STREAM opcode is processed might garble then screen output.
  // So we'll just wait until the user has finished with the input and ask
  // for the filename to save to once he's finished.
  ask_for_stream4_filename_if_required();

  if (stream_4_init_underway == false)
  {
    if (stream_4 == NULL)
    {
      TRACE_LOG("Opening script-file '%s' for writing.\n", stream_4_filename);
      stream_4 = fsi->openfile(stream_4_filename, FILETYPE_INPUTRECORD,
          FILEACCESS_APPEND);
    }

    fsi->writechars(latin1_output, strlen(latin1_output), stream_4);
  }
}


void stream_4_z_ucs_output(z_ucs *z_ucs_output)
{
  char buf[128];

  while (*z_ucs_output != 0)
  {
    zucs_string_to_utf8_string(buf, &z_ucs_output, 128);
    stream_4_latin1_output(buf);
  }
}


static void close_script_file()
{
  if (stream_2 != NULL)
  {
    TRACE_LOG("Closing script-file.\n");

    if (stream_2_wrapping_disabled == false)
      wordwrap_flush_output(stream_2_wrapper);
    else
      flush_stream_2_buffer_output();
    fsi->writechar('\n', stream_2);
    (void)fsi->closefile(stream_2);
    stream_2 = NULL;
  }
}


static void send_to_stream1_targets(z_ucs *z_ucs_output)
{
#ifndef DISABLE_OUTPUT_HISTORY
  if ((active_window_number == 0) && (outputhistory[0] != NULL) ) {
    store_z_ucs_output_in_history(
        outputhistory[0],
        z_ucs_output);
  }
#endif /* DISABLE_OUTPUT_HISTORY */
#ifndef DISABLE_BLOCKBUFFER
  if (active_window_number == 1) {
    store_z_ucs_output_in_blockbuffer(
        upper_window_buffer, z_ucs_output);
  }
#endif /* DISABLE_BLOCKBUFFER */
  if (active_interface != NULL) {
    active_interface->z_ucs_output(z_ucs_output);
  }
}


static int _streams_z_ucs_output(z_ucs *z_ucs_output, bool is_user_input)
{
  int16_t conversion_result;
  zscii zscii_char;
  z_ucs *src;
  uint16_t len;
  z_ucs font3_conversion_buf[FONT3_CONVERSION_BUF_SIZE];
  z_ucs char_to_convert, converted_char;
  int font3_buf_index;
  z_ucs *processed_output, *output_pos, *processed_output_pos;
  z_ucs *next_newline_pos;
  //int size;
  int parameter1, parameter2;
  bool font_conversion_active
    = (
        (current_font == Z_FONT_CHARACTER_GRAPHICS)
        &&
        (strcmp(get_configuration_value("enable-font3-conversion"),"true") == 0)
      ) ? true : false;

  TRACE_LOG("Streams-output of \"");
  TRACE_LOG_Z_UCS(z_ucs_output);
  TRACE_LOG("\".\n");

  if (
      (stream_3_current_depth != -1)
      &&
      (bool_equal(is_user_input, false))
     )
  {
    src = z_ucs_output;

    while (*src != 0)
    {
      conversion_result
        = (int16_t)unicode_char_to_zscii_input_char(*src);

      if (conversion_result == 10)
        zscii_char = (zscii)13;
      else if (conversion_result == -1)
        zscii_char = (zscii)'?';
      else
        zscii_char = (zscii)conversion_result;

      *(stream_3_index[stream_3_current_depth]++) = zscii_char;
      TRACE_LOG("Writing ZSCII '%c' / %d to %ld.\n",
          zscii_char,
          zscii_char,
          (long int)(stream_3_index[stream_3_current_depth] - z_mem));

      src++;
    }

    len = load_word(stream_3_start[stream_3_current_depth]);
    TRACE_LOG("Loaded current stream-3-length %d.\n", len);

    len += (src - z_ucs_output);

    store_word(stream_3_start[stream_3_current_depth], len);
    TRACE_LOG("Stored current stream-3-length %d.\n", len);
  }
  else
  {
    if (bool_equal(is_user_input, false))
      stream_output_has_occured = true;

    if (
        (active_z_story != NULL)
        &&
        ((z_mem[0x11] & 0x1) != 0)
        &&
        (ver != 6)
        &&
        (strcasecmp(get_configuration_value(
           "disable-external-streams"), "true") != 0)
       )
    {
      stream_2_output(z_ucs_output);
    }
    else if (stream_2 != NULL)
    {
      close_script_file();
    }

    output_pos = z_ucs_output;
    processed_output = font_conversion_active == true
      ? font3_conversion_buf : z_ucs_output;
    while (*output_pos != 0)
    {
      if (font_conversion_active == true)
      {
        TRACE_LOG("Converting to font 3.\n");

        // In case we're converting we'll process a maximum of
        // (FONT3_CONVERSION_BUF_SIZE) chars in one iteration.
        font3_buf_index = 0;
        char_to_convert = *output_pos;
        while ( (char_to_convert != 0)
           && (font3_buf_index < FONT3_CONVERSION_BUF_SIZE) )
        {
          if ( (char_to_convert == 32) || (char_to_convert == 37) ) // empty
            converted_char = ' ';
          if (char_to_convert == 33) // arrow left
            converted_char = 0x2190;
          else if (char_to_convert == 34) // arrow right
            converted_char = 0x2192;
          else if (char_to_convert == 35) // diagonal
            //converted_char = 0x2571;
            converted_char = '/';
          else if (char_to_convert == 36) // diagonal
            //converted_char = 0x2572;
            converted_char = '\\';
          else if (char_to_convert == 38) // horizontal
            converted_char = 0x2500;
          else if (char_to_convert == 39) // horizontal
            converted_char = 0x2500;
          else if (char_to_convert == 40)
            converted_char = 0x2502;
          else if (char_to_convert == 41)
            converted_char = 0x2502;
          else if (char_to_convert == 42)
            converted_char = 0x2534;
          else if (char_to_convert == 43)
            converted_char = 0x252C;
          else if (char_to_convert == 44)
            converted_char = 0x251C;
          else if (char_to_convert == 45)
            converted_char = 0x2524;
          else if (char_to_convert == 46)
            converted_char = 0x230A;
          else if (char_to_convert == 47)
            converted_char = 0x2308;
          else if (char_to_convert == 48)
            converted_char = 0x2309;
          else if (char_to_convert == 49)
            converted_char = 0x230B;
          else if (char_to_convert == 50) // FIXME: Better symbol
            converted_char = 0x2534;
          else if (char_to_convert == 51) // FIXME: Better symbol
            converted_char = 0x252C;
          else if (char_to_convert == 52) // FIXME: Better symbol
            converted_char = 0x251C;
          else if (char_to_convert == 53) // FIXME: Better symbol
            converted_char = 0x2524;
          else if (char_to_convert == 54) // full block
            converted_char = 0x2588;
          else if (char_to_convert == 55) // upper half block
            converted_char = 0x2580;
          else if (char_to_convert == 56) // lower half block
            converted_char = 0x2584;
          else if (char_to_convert == 57) // left half block
            converted_char = 0x258C;
          else if (char_to_convert == 58) // right half block
            converted_char = 0x2590;
          else if (char_to_convert == 59) // FIXME: Better symbol
            converted_char = 0x2584;
          else if (char_to_convert == 60) // FIXME: Better symbol
            converted_char = 0x2580;
          else if (char_to_convert == 61) // FIXME: Better symbol
            converted_char = 0x258C;
          else if (char_to_convert == 62) // FIXME: Better symbol
            converted_char = 0x2590;
          else if (char_to_convert == 63) // upper right block
            converted_char = 0x259D;
          else if (char_to_convert == 64) // lower right block
            converted_char = 0x2597;
          else if (char_to_convert == 65) // lower left block
            converted_char = 0x2596;
          else if (char_to_convert == 66) // upper left block
            converted_char = 0x2598;
          else if (char_to_convert == 67) // FIXME: Better symbol
            converted_char = 0x259D;
          else if (char_to_convert == 68) // FIXME: Better symbol
            converted_char = 0x2597;
          else if (char_to_convert == 69) // FIXME: Better symbol
            converted_char = 0x2596;
          else if (char_to_convert == 70) // FIXME: Better symbol
            converted_char = 0x2598;
          else if (char_to_convert == 71) // dot upper right, FIXME: Better symbol
            converted_char = '+';
          else if (char_to_convert == 72) // dot lower right, FIXME: Better symbol
            converted_char = '+';
          else if (char_to_convert == 73) // dot lower left, FIXME: Better symbol
            converted_char = '+';
          else if (char_to_convert == 74) // dot upper left, FIXME: Better symbol
            converted_char = '+';
          else if (char_to_convert == 75) // line top
            converted_char = 0x2594;
          else if (char_to_convert == 76) // line bottom
            converted_char = '_';
          else if (char_to_convert == 77) // line left
            converted_char = 0x23B9;
          else if (char_to_convert == 78) // line right
            converted_char = 0x2595;
          else if (char_to_convert == 79) // status bar, filled 0/8
            converted_char = ' ';
          else if (char_to_convert == 80) // status bar, filled 1/8
            converted_char = 0x258F;
          else if (char_to_convert == 81) // status bar, filled 2/8
            converted_char = 0x258E;
          else if (char_to_convert == 82) // status bar, filled 3/8
            converted_char = 0x258D;
          else if (char_to_convert == 83) // status bar, filled 4/8
            converted_char = 0x258C;
          else if (char_to_convert == 84) // status bar, filled 5/8
            converted_char = 0x258B;
          else if (char_to_convert == 85) // status bar, filled 6/8
            converted_char = 0x258A;
          else if (char_to_convert == 86) // status bar, filled 7/8
            converted_char = 0x2589;
          else if (char_to_convert == 87) // status bar, filled 8/8
            converted_char = 0x2588;
          else if (char_to_convert == 88) // status bar, padding right
            converted_char = 0x2595;
          else if (char_to_convert == 89) // status bar, padding left
            converted_char = 0x258F;
          else if (char_to_convert == 90) // diagonal cross
            converted_char = 0x2573;
          else if (char_to_convert == 91) // vertical / horizontal cross
            converted_char = 0x253C;
          else if (char_to_convert == 92) // arrow up
            converted_char = 0x2191;
          else if (char_to_convert == 93) // arrow down
            converted_char = 0x2193;
          else if (char_to_convert == 94) // arrow up-down
            converted_char = 0x2195;
          else if (char_to_convert == 95) // square
            char_to_convert =0x25A2;
          else if (char_to_convert == 96) // question mark
            converted_char = '?';

          // runic

          else if (char_to_convert == 97)
            converted_char = 0x16aa;
          else if (char_to_convert == 98)
            converted_char = 0x16d2;
          else if (char_to_convert == 99)
            converted_char = 0x16c7;
          else if (char_to_convert == 100)
            converted_char = 0x16de;
          else if (char_to_convert == 101)
            converted_char = 0x16d6;
          else if (char_to_convert == 102)
            converted_char = 0x16a0;
          else if (char_to_convert == 103)
            converted_char = 0x16b7;
          else if (char_to_convert == 104)
            converted_char = 0x16bb;
          else if (char_to_convert == 105)
            converted_char = 0x16c1;
          else if (char_to_convert == 106)
            converted_char = 0x16e8;
          else if (char_to_convert == 107)
            converted_char = 0x16e6;
          else if (char_to_convert == 108)
            converted_char = 0x16da;
          else if (char_to_convert == 109)
            converted_char = 0x16d7;
          else if (char_to_convert == 110)
            converted_char = 0x16be;
          else if (char_to_convert == 111)
            converted_char = 0x16a9;
          else if (char_to_convert == 112) // FIXME: better symbol?
            converted_char = 0x16b3;
          else if (char_to_convert == 113)
            converted_char = 'h';         // FIXME: better symbol?;
          else if (char_to_convert == 114)
            converted_char = 0x16b1;
          else if (char_to_convert == 115)
            converted_char = 0x16cb;
          else if (char_to_convert == 116)
            converted_char = 0x16cf;
          else if (char_to_convert == 117)
            converted_char = 0x16a2;
          else if (char_to_convert == 118)
            converted_char = 0x16e0;
          else if (char_to_convert == 119)
            converted_char = 0x16b9;
          else if (char_to_convert == 120)
            converted_char = 0x16c9;
          else if (char_to_convert == 121)
            converted_char = 0x16a5;
          else if (char_to_convert == 122)
            converted_char = 0x16df;

          // inverted

          else if (char_to_convert == 123) // inverted arrow up, FIXME: symbol
            converted_char = 0x2191;
          else if (char_to_convert == 124) // inverted arrow down, FIXME: symbol
            converted_char = 0x2193;
          else if (char_to_convert == 125) // inverted arrow up-down, FIXME: symbol
            converted_char = 0x2195;
          else if (char_to_convert == 126) // inverted question mark, FIXME: symbol
            converted_char = '?';

          // no match

          else
            converted_char = char_to_convert;

          font3_conversion_buf[font3_buf_index] = converted_char;
          font3_buf_index++;
          output_pos++;
          char_to_convert = *output_pos;
        }
        font3_conversion_buf[font3_buf_index] = 0;
      }

      if (bool_equal(stream_1_active, true))
      {
#ifndef DISABLE_OUTPUT_HISTORY
        if (active_window_number == 0)
        {
          // outputhistory[0] is always defined if DISABLE_OUTPUT_HISTORY is not
          // defined (see fizmo.c).

          if (
              (lower_window_foreground_colour != current_foreground_colour)
              ||
              (lower_window_background_colour != current_background_colour)
             )
          {
            store_metadata_in_history(
                outputhistory[0],
                HISTORY_METADATA_TYPE_COLOUR,
                current_foreground_colour,
                current_background_colour);

            lower_window_foreground_colour = current_foreground_colour;
            lower_window_background_colour = current_background_colour;
          }
        }

        if ( (active_z_story != NULL) && (z_mem[0x11] & 2) ) {
          current_resulting_font = Z_FONT_COURIER_FIXED_PITCH;
          //printf("force fixed on: %d\n", current_resulting_font);
        }
        else {
          current_resulting_font = current_font;
          //printf("force fixed off: %d\n", current_resulting_font);
        }

        if (lower_window_font != current_resulting_font) {
          store_metadata_in_history(
              outputhistory[0],
              HISTORY_METADATA_TYPE_FONT,
              current_resulting_font);
          lower_window_font = current_resulting_font;
          active_interface->set_font(current_resulting_font);
        }
#endif /* DISABLE_OUTPUT_HISTORY */

#ifndef DISABLE_BLOCKBUFFER
        if ((active_window_number == 1) && (upper_window_buffer != NULL))
        {
          if (upper_window_style != current_style)
          {
            set_blockbuf_style(upper_window_buffer, current_style);
            upper_window_style = current_style;
          }

          if (upper_window_foreground_colour != current_foreground_colour)
          {
            set_blockbuf_foreground_colour(
                upper_window_buffer, current_foreground_colour);
            upper_window_foreground_colour = current_foreground_colour;
          }

          if (upper_window_background_colour != current_background_colour)
          {
            set_blockbuf_background_colour(
                upper_window_buffer, current_background_colour);
            upper_window_background_colour = current_background_colour;
          }
        }
#endif /* DISABLE_BLOCKBUFFER */

        if (strcasecmp(get_configuration_value("flush-output-on-newline"),
                      "true") != 0) {
          // No split required, process all at once.
          send_to_stream1_targets(processed_output);
        }
        else {
          // Split output in newlines and process one by one.
          processed_output_pos = processed_output;
          next_newline_pos = NULL;
          // We'll try to find a newline in the current string. In case this
          // is the first iteration, next_newline_pos == NULL and we'll start
          // looking from the front. In case we've already found a newline
          // we'll start at the char behind it. This helps to ensure that
          // the newline is actually included in the next output.
          while ((next_newline_pos
                = z_ucs_chr(processed_output_pos, Z_UCS_NEWLINE)) != NULL) {
            *next_newline_pos = 0;
            send_to_stream1_targets(processed_output_pos);
            *next_newline_pos = Z_UCS_NEWLINE;
            processed_output_pos = next_newline_pos;
#ifndef DISABLE_OUTPUT_HISTORY
            if (paragraph_attribute_function != NULL) {
              //paragraph_attribute_function(&parameter1, &parameter2);

              // Write paragraph attribute metadata dummy block. It is
              // written to make sure the block is placed before the
              // paragraph's final newline, but we'll replace it's contents
              // later (since we can only know the correct contents once the
              // newline char has been flushed).
              if (outputhistory[0] != NULL) {
                store_metadata_in_history(
                    outputhistory[0],
                    HISTORY_METADATA_TYPE_PARAGRAPHATTRIBUTE,
                    0,
                    0);
              }

              // Write newline and advance stream accordingly.
              send_to_stream1_targets(z_ucs_newline_string);
              processed_output_pos++;

              paragraph_attribute_function(&parameter1, &parameter2);

              if (outputhistory[0] != NULL) {
                alter_last_written_paragraph_attributes(
                    outputhistory[0],
                    parameter1,
                    parameter2);
              }
            }
#endif /* DISABLE_OUTPUT_HISTORY */
          }
          send_to_stream1_targets(processed_output_pos);
        }
      }

      // In case we're not converting and input == output we know that
      // once we arrive here the entire output has been processed, so
      // we can quit right away.
      if (font_conversion_active == false)
        break;
    }

    if (bool_equal(is_user_input, true))
    {
      if (
          (bool_equal(stream_4_active, true))
          &&
          (strcasecmp(get_configuration_value("disable-external-streams"),
                      "true") != 0)
         )
        stream_4_z_ucs_output(z_ucs_output);
      else
      {
        if (stream_4 != NULL)
        {
          (void)fsi->closefile(stream_4);
          stream_4 = NULL;
	      }
      }
    }
  }

  /*@-globstate@*/
  return 0;
  /*@+globstate@*/

  // For some reason splint thinks that comparison with NULL might set
  // active_z_story or active_interface to NULL. The "globstate" annotations
  // above inhibit these warnings.
}


int streams_z_ucs_output(z_ucs *z_ucs_output)
{
  return _streams_z_ucs_output(z_ucs_output, false);
}


int streams_z_ucs_output_user_input(z_ucs *z_ucs_output)
{
  return _streams_z_ucs_output(z_ucs_output, true);
}


int streams_latin1_output(/*@in@*/ char *latin1_output)
{
  z_ucs output_buffer_z_ucs[ASCII_TO_Z_UCS_BUFFER_SIZE];

  //FIXME: latin1 != utf8
  while (latin1_output != NULL)
  {
    latin1_output = utf8_string_to_zucs_string(
        output_buffer_z_ucs,
        latin1_output,
        ASCII_TO_Z_UCS_BUFFER_SIZE);

    if (streams_z_ucs_output(output_buffer_z_ucs) != 0)
      return -1;
  }

  return 0;
}


void opcode_output_stream(void)
{
  int16_t stream_number =(int16_t) op[0];

  TRACE_LOG("Opcode: OUTPUT_STREAM.\n");

  if (stream_number == 0)
    return;

#ifdef ENABLE_TRACING
  if (((int16_t)op[0]) < 0)
  {
    TRACE_LOG("Closing stream ");
  }
  else
  {
    TRACE_LOG("Opening stream ");
  }
  TRACE_LOG("%d.\n", (((int16_t)op[0]) < 0 ? -((int16_t)op[0]) : op[0]));
#endif

  if ((stream_number < -4) || (stream_number > 4))
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_INVALID_OUTPUT_STREAM_NUMBER_P0D,
        -1,
        (long int)stream_number);

  if (stream_number == 1)
    stream_1_active = true;

  else if (stream_number == -1)
    stream_1_active = false;

  else if (stream_number == 2)
    z_mem[0x11] |= 1;

  else if (stream_number == -2)
    z_mem[0x11] &= 0xfe;

  else if (stream_number == 3)
  {
    if (++stream_3_current_depth == MAXIMUM_STREAM_3_DEPTH)
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_MAXIMUM_STREAM_3_DEPTH_P0D_EXCEEDED,
          -1,
          MAXIMUM_STREAM_3_DEPTH);

    stream_3_start[stream_3_current_depth] = z_mem + op[1];
    stream_3_index[stream_3_current_depth] = z_mem + op[1] + 2;

    store_word(stream_3_start[stream_3_current_depth], 0);

    TRACE_LOG("stream-3 depth: %d.\n", stream_3_current_depth);

    TRACE_LOG("Current stream-3 dest is %p.\n",
        stream_3_start[stream_3_current_depth]);
  }

  else if (stream_number == -3)
  {
    if (stream_3_current_depth >= 0)
    {
      TRACE_LOG("stream-3 depth: %d.\n", stream_3_current_depth);
      stream_3_current_depth--;
    }
  }

  else if (stream_number == 4)
    stream_4_active = true;

  else if (stream_number == -4)
    stream_4_active = false;
}


void close_streams(z_ucs *error_message)
{
  TRACE_LOG("Closing all streams.\n");

  // Close the interface first. This will allow stream 2 to capture
  // any pending output.
  (void)close_interface(error_message);

  if (stream_2 != NULL)
    close_script_file();

  if (stream_2_wrapper != NULL)
  {
    wordwrap_destroy_wrapper(stream_2_wrapper);
    stream_2_wrapper = NULL;
  }

  if (stream_4 != NULL)
  {
    (void)fsi->closefile(stream_4);
    stream_4 = NULL;
  }
}


void open_input_stream_1(void)
{
  if (strcasecmp(get_configuration_value("disable-external-streams"), "true")
      != 0)
  {
    input_stream_1_active = true;
  }
  else
  {
    (void)i18n_translate(
        libfizmo_module_name, i18n_libfizmo_THIS_FUNCTION_HAS_BEEN_DISABLED);
    (void)streams_latin1_output("\n");
  }
}


void close_input_stream_1(void)
{
  if (input_stream_1 != NULL)
  {
    fsi->closefile(input_stream_1);
    input_stream_1 = NULL;
  }
}


void opcode_input_stream(void)
{
  TRACE_LOG("Opcode: INPUT_STREAM.\n");

  if (((int16_t)op[0]) == 1)
  {
    open_input_stream_1();
  }
  else
  {
    close_input_stream_1();
  }
}

#endif // streams_c_INCLUDED

