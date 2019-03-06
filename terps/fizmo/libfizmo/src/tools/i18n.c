
/* i18n.c
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


#ifndef i18n_c_INCLUDED
#define i18n_c_INCLUDED

#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>

#include "i18n.h"
#include "tracelog.h"
#include "z_ucs.h"
#include "stringmap.h"
#include "list.h"
#include "filesys.h"
#include "../locales/libfizmo_locales.h"

#define LATIN1_TO_Z_UCS_BUFFER_SIZE 64

static z_ucs i18n_fallback_error_message[] = {
   'U', 'n', 'k', 'n', 'o', 'w', 'n', ' ',
   'i', '1', '8', 'n', ' ',
   'e', 'r', 'r', 'o', 'r', '.',
   0 };

static char *locale_aliases[4][6] =
{
  { "en_GB", "en-GB", "en_US", "en-US", "en", NULL },
  { "de_DE", "de", NULL },
  { "fr_FR", "fr", NULL },
  { NULL }
};

//z_ucs *active_locale = NULL;
static z_ucs *current_locale_name = NULL;
static char *current_locale_name_in_utf8 = NULL;
static char *default_locale_name_in_utf8 = NULL;
static int (*stream_output_function)(z_ucs *output) = NULL;
static void (*abort_function)(int exit_code, z_ucs *error_message) = NULL;
static char *locale_search_path = NULL;
static stringmap *locale_modules = NULL; // indexed by locale_name

#ifdef I18N_DEFAULT_SEARCH_PATH
char default_search_path[] = I18N_DEFAULT_SEARCH_PATH;
#else
char default_search_path[] = ".";
#endif // I18N_DEFAULT_SEARCH_PATH

void register_i18n_stream_output_function(
    int (*new_stream_output_function)(z_ucs *output))
{
  stream_output_function = new_stream_output_function;
}


void register_i18n_abort_function(
    void (*new_abort_function)(int exit_code, z_ucs *error_message))
{
  abort_function = new_abort_function;
}


static int i18n_send_output(z_ucs *z_ucs_data, int output_mode,
    z_ucs **string_target)
{
  char *output;

  if (z_ucs_data == NULL)
  {
    return 0;
  }
  else if (output_mode == i18n_OUTPUT_MODE_STREAMS)
  {
    if (stream_output_function == NULL)
    {
      if ((output = dup_zucs_string_to_utf8_string(z_ucs_data)) == NULL)
        return -1;
      fputs(output, stdout);
      free(output);
      return 0;
    }
    else
      return stream_output_function(z_ucs_data);
  }
  else if (output_mode == i18n_OUTPUT_MODE_STRING)
  {
    (void)z_ucs_cpy(*string_target, z_ucs_data);
    *string_target += z_ucs_len(z_ucs_data);
    return 0;
  }
  else if (output_mode == i18n_OUTPUT_MODE_DEV_NULL)
  {
    return 0;
  }
  else
  {
    return -1;
  }
}


static z_ucs input_char(z_file *in)
{
  z_ucs input;

  if ((input = parse_utf8_char_from_file(in)) == UEOF)
  {
    TRACE_LOG("Premature end of file.\n");
    exit(-1);
  }

  return (z_ucs)input;
}


static locale_module *parse_locale_file(z_ucs *module_name,
    char* locale_dir_name, char* module_name_utf8)
{
  char *filename;
  z_file *in;
  z_ucs *locale_data;
  locale_module *result;
  int in_char;
  long nof_zucs_chars;
  z_ucs input;
  z_ucs *linestart;
  list *lines;

  TRACE_LOG("locale_dir_name: \"%s\".\n", locale_dir_name);
  TRACE_LOG("module_name_utf8: \"%s\".\n", module_name_utf8);

  // open-resource:
  if ((filename
        = malloc(strlen(locale_dir_name) + strlen(module_name_utf8) + 2))
      == NULL)
  {
    // exit-point:
    TRACE_LOG("malloc() returned NULL.\n");
    return NULL;
  }

  TRACE_LOG("locale_dir_name: \"%s\".\n", locale_dir_name);
  TRACE_LOG("module_name_utf8: \"%s\".\n", module_name_utf8);

  strcpy(filename, locale_dir_name);
  strcat(filename, "/");
  strcat(filename, module_name_utf8);

  TRACE_LOG("Parsing locale file \"%s\".\n", filename);

  // open-resource:
  if ((in = fsi->openfile(filename, FILETYPE_DATA, FILEACCESS_READ))
      == NULL)
  {
    // exit-point:
    TRACE_LOG("openfile(\"%s\") returned NULL.\n", filename);
    free(filename);
    return NULL;
  }

  nof_zucs_chars = 0;
  while ((parse_utf8_char_from_file(in)) != UEOF)
    nof_zucs_chars++;
  nof_zucs_chars++; // Add space for terminating zero (yes, really required).

  if (fsi->setfilepos(in, 0, SEEK_SET) == -1)
  {
    // exit-point:
    TRACE_LOG("setfilepos() returned -1.\n");
    fsi->closefile(in);
    free(filename);
    return NULL;
  }

  TRACE_LOG("Allocating space for %ld z_ucs chars.\n", nof_zucs_chars);

  // open-resource:
  if ((locale_data = malloc(nof_zucs_chars * sizeof(z_ucs))) == NULL)
  {
    // exit-point:
    TRACE_LOG("malloc(%ld) returned NULL.\n", nof_zucs_chars * sizeof(z_ucs));
    fsi->closefile(in);
    free(filename);
    return NULL;
  }

  TRACE_LOG("Locale data at %p, ends at %p.\n",
      locale_data, locale_data+nof_zucs_chars);

  // open-resource:
  if ((result = malloc(sizeof(locale_module))) == NULL)
  {
    // exit-point:
    free(locale_data);
    fsi->closefile(in);
    free(filename);
    return NULL;
  }

  TRACE_LOG("New module at %p.\n", result);

  result->locale_data = locale_data;
  TRACE_LOG("Locale data starts at %p.\n", locale_data);

  /* --- */

  // open-resource:
  lines = create_list();
  //printf("new list created: %p\n", lines);

  in_char = fsi->readchar(in);
  while (in_char != EOF)
  {
    linestart = locale_data;

    // Found a new line.
    fsi->unreadchar(in_char, in);

    for (;;)
    {
      input = input_char(in);

      if (input == Z_UCS_BACKSLASH)
      {
        //*locale_data++ = input;

        input = input_char(in);

        if (input == Z_UCS_BACKSLASH)
        {
          *locale_data++ = Z_UCS_BACKSLASH;
        }
        else if (input == 'n')
        {
          *locale_data++ = Z_UCS_NEWLINE;
        }
        else if (input == '{')
        {
          *locale_data++ = Z_UCS_BACKSLASH;
          *locale_data++ = (z_ucs)'{';

          input = input_char(in);

          if ((input < 0x30) && (input > 0x39))
          {
            fprintf(stderr, "Variable number expected.\n");
            exit(EXIT_FAILURE);
          }

          *locale_data++ = input;

          input = input_char(in);

          if (
              (input != (z_ucs)'s')
              &&
              (input != (z_ucs)'d')
              &&
              (input != (z_ucs)'x')
              &&
              (input != (z_ucs)'z')
             )
          {
            fprintf(stderr, "Invalid parameter type.\n");
            exit(EXIT_FAILURE);
          }

          *locale_data++ = input;

          input = input_char(in);

          if (input != (z_ucs)'}')
          {
            fprintf(stderr, "Expected '}'.\n");
            exit(EXIT_FAILURE);
          }

          *locale_data++ = (z_ucs)'}';
        }
        else
        {
          fprintf(stderr, "Undefined control sequence \\%c.\n",(char)input);
          exit(EXIT_FAILURE);
        }
      }
      else if (input == Z_UCS_NEWLINE)
      {
        *locale_data++ = 0;
        TRACE_LOG("New line at %p.\n", linestart);
        add_list_element(lines, (void*)linestart);

        //TRACE_LOG_Z_UCS(linestart);

        break;
      }
      else
      {
        // Here we've found some "normal" output.
        *locale_data++ = (z_ucs)input;
      }
    }

    //messages_processed++;
    in_char = fsi->readchar(in);
  }

  *locale_data = 0;
  TRACE_LOG("Wirte last byte at %p.\n", locale_data);

  /* --- */

  TRACE_LOG("Read %d lines.\n", get_list_size(lines));

  // close-resource(l), open-resource(result->messages):
  result->nof_messages = get_list_size(lines);
  result->messages = (z_ucs**)delete_list_and_get_ptrs(lines);

  TRACE_LOG("Messages at %p.\n", result->messages);

  TRACE_LOG("First msg at %p.\n", result->messages[0]);

  if ((result->module_name = malloc(sizeof(z_ucs) * (z_ucs_len(module_name)+1)))
      == NULL)
  {
    // exit-point:
    free(result->messages);
    free(result);
    free(locale_data);
    fsi->closefile(in);
    free(filename);
    return NULL;
  }

  z_ucs_cpy(result->module_name, module_name);

  // close-resource:
  fsi->closefile(in);

  // close-resource:
  free(filename);

  TRACE_LOG("Returning new module at %p.\n", result);

  return result;
}


static void delete_locale_module(locale_module *module)
{
  TRACE_LOG("Deleting locale module at %p.\n", module);

  free(module->messages);
  free(module->locale_data);
  free(module->module_name);
  free(module);
}


// Returns malloc()ed string which needs to be free()d later.
static char *get_path_for_locale(z_ucs *locale_name)
{
  z_dir *dir;
  char *search_path;
  z_ucs *search_path_zucs, *path_ptr, *colon_index, *zucs_ptr;
  z_ucs *zucs_buf = NULL;
  size_t bufsize = 0, len, dirname_len;
  struct z_dir_ent z_dir_entry;
  char *dirname;
  char *locale_name_utf8, *locale_dir_name = NULL;

  search_path
    = locale_search_path == NULL
    ? default_search_path
    : locale_search_path;

  search_path_zucs = dup_utf8_string_to_zucs_string(search_path);
  path_ptr = search_path_zucs;

  TRACE_LOG("Search path: \"");
  TRACE_LOG_Z_UCS(search_path_zucs);
  TRACE_LOG("'.\n");

  // open-resource:
  locale_name_utf8 = dup_zucs_string_to_utf8_string(locale_name);

  while (*path_ptr != 0)
  {
    colon_index = z_ucs_chr(path_ptr, Z_UCS_COLON);

    len
      = colon_index == NULL
      ? z_ucs_len(path_ptr)
      : (size_t)(colon_index - path_ptr);

    TRACE_LOG("len: %ld\n", (long)len);

    if (len > 0)
    {
      if (bufsize < len + 1)
      {
        TRACE_LOG("realloc buf for %ld chars / %ld bytes.\n",
            (long)len + 1, (long)sizeof(z_ucs) * (len + 1));

        // open-resource:
        if ((zucs_ptr = realloc(zucs_buf, sizeof(z_ucs) * (len + 1))) == NULL)
        {
          // exit-point:
          TRACE_LOG("realloc() returned NULL.\n");
          free(zucs_buf);
          free(locale_name_utf8);
          free(path_ptr);
          return NULL;
        }

        bufsize = len + 1;
        zucs_buf = zucs_ptr;
      }

      z_ucs_ncpy(zucs_buf, path_ptr, len);
      zucs_buf[len] = 0;

      // open-resource:
      if ((dirname = dup_zucs_string_to_utf8_string(zucs_buf)) == NULL)
      {
        // exit-point:
        TRACE_LOG("dup_zucs_string_to_utf8_string() returned NULL.\n");
        free(zucs_buf);
        free(locale_name_utf8);
        free(path_ptr);
        return NULL;
      }

      TRACE_LOG("Path: '%s'\n", dirname);

      // open-resource:
      if ((dir = fsi->open_dir(dirname)) != NULL)
      {
        while (fsi->read_dir(&z_dir_entry, dir) == 0)
        {
          TRACE_LOG("Processing \"%s\".\n", z_dir_entry.d_name);
          if (strcasecmp(locale_name_utf8, z_dir_entry.d_name) == 0)
          {
            dirname_len = strlen(dirname) + strlen(z_dir_entry.d_name);
            TRACE_LOG("dirname_len: %ld.\n", (long)dirname_len);
            // open-resource:
            if ((locale_dir_name = malloc(dirname_len + 2)) == NULL)
            {
              // exit-point:
              TRACE_LOG("malloc() returned NULL.\n");
              fsi->close_dir(dir);
              free(dirname);
              free(zucs_buf);
              free(locale_name_utf8);
              free(path_ptr);
              return NULL;
            }

            strcpy(locale_dir_name, dirname);
            strcat(locale_dir_name, "/");
            strcat(locale_dir_name, z_dir_entry.d_name);
            break;
          }
        }

        // close-resource:
        fsi->close_dir(dir);
      }

      // close-resource:
      free(dirname);
    }

    if (locale_dir_name != NULL)
      break;

    path_ptr += len + (colon_index != NULL ? 1 : 0);
  }

  free(search_path_zucs);

  // close-resource:
  free(locale_name_utf8);

  // close-resource:
  free(zucs_buf);

  //TRACE_LOG("res:'");
  //TRACE_LOG_Z_UCS(locale_dir_name);
  //TRACE_LOG("\n");

  return locale_dir_name;
}


static locale_module *create_locale_module(z_ucs *locale_name,
    z_ucs *module_name)
{
  z_dir *dir;
  locale_module *result = NULL;
  char *ptr;
  struct z_dir_ent z_dir_entry;
  char *module_name_utf8, *locale_dir_name = NULL;
  stringmap *module_map;

  TRACE_LOG("Creating module '");
  TRACE_LOG_Z_UCS(module_name);
  TRACE_LOG("' for locale '");
  TRACE_LOG_Z_UCS(locale_name);
  TRACE_LOG("'.\n");

  if ((locale_dir_name = get_path_for_locale(locale_name)) == NULL)
  {
    // No directory containing an entry named equal to local name was foudn.
    TRACE_LOG("locale_dir_name is NULL.\n");
    return NULL;
  }

  // open-resource:
  module_name_utf8 = dup_zucs_string_to_utf8_string(module_name);
  if ((ptr = realloc(module_name_utf8, strlen(module_name_utf8) + 9)) == NULL)
  {
    TRACE_LOG("realloc() returned NULL.\n");
    free(module_name_utf8);
    free(locale_dir_name);
    return NULL;
  }

  module_name_utf8 = ptr;
  strcat(module_name_utf8, "_i18n.txt");

  // open-resource:
  if ((dir = fsi->open_dir(locale_dir_name)) != NULL)
  {
    TRACE_LOG("Trying file \"%s\".\n", module_name_utf8);
    while (fsi->read_dir(&z_dir_entry, dir) == 0)
    {
      if (strcasecmp(module_name_utf8, z_dir_entry.d_name) == 0)
      {
        TRACE_LOG("File matches \"%s\".\n", module_name_utf8);
        if ((result = parse_locale_file(
                module_name,
                locale_dir_name,
                module_name_utf8)) == NULL)
        {
          TRACE_LOG("parse_locale_file() returned NULL.\n");
          fsi->close_dir(dir);
          free(module_name_utf8);
          free(locale_dir_name);
          return NULL;
        }
      }
    }

    // close-resource:
    fsi->close_dir(dir);
  }

  // close-resource:
  free(module_name_utf8);

  // close-resource:
  free(locale_dir_name);

  if (result == NULL)
  {
    TRACE_LOG("Could not find module'");
    TRACE_LOG_Z_UCS(module_name);
    TRACE_LOG("'.\n");
    return NULL;
  }

  if (locale_modules == NULL)
    locale_modules = create_stringmap();

  if ((module_map
        = (stringmap*)get_stringmap_value(locale_modules, locale_name))
      == NULL)
  {
    module_map = create_stringmap();
    if ((add_stringmap_element(locale_modules, locale_name, module_map)) != 0)
    {
      TRACE_LOG("add_stringmap_element() failed.\n");
      delete_locale_module(result);
    }
  }

  if (add_stringmap_element(module_map, module_name, result) != 0)
  {
    TRACE_LOG("add_stringmap_element() failed.\n");
    delete_locale_module(result);
  }

  return result;
}


static locale_module *get_locale_module(z_ucs *locale_name,
    z_ucs *module_name)
{
  stringmap *module_map;
  locale_module *result = NULL;

  TRACE_LOG("Getting module '");
  TRACE_LOG_Z_UCS(module_name);
  TRACE_LOG("' for locale '");
  TRACE_LOG_Z_UCS(locale_name);
  TRACE_LOG("'.\n");

  if (
      (locale_modules != NULL)
      &&
      ((module_map
        = (stringmap*)get_stringmap_value(locale_modules, locale_name))
       != NULL)
     )
    result = (locale_module*)get_stringmap_value(module_map, module_name);

  if (result == NULL)
    result = create_locale_module(locale_name, module_name);

  return result;
}


static void i18n_exit(int exit_code, z_ucs *error_message)
{
  char *output;

  if ( (error_message == NULL)
      || (*error_message == 0) )
    error_message = i18n_fallback_error_message;

  if (abort_function != NULL)
  {
    TRACE_LOG("Aborting using custom abort function.\n");
    abort_function(exit_code, error_message);
  }
  else
  {
    TRACE_LOG("Aborting using default method: fputs and exit(code).\n");
    if ((output = dup_zucs_string_to_utf8_string(error_message)) != NULL)
    {
      fputs(output, stderr);
      free(output);
    }
  }

  exit(exit_code);
}


char *get_default_locale_name()
{
  if (default_locale_name_in_utf8 == NULL)
    if ((default_locale_name_in_utf8
          = dup_zucs_string_to_utf8_string(default_locale_name)) == NULL)
      return NULL;

  return default_locale_name_in_utf8;
}


// "string_code" is one of the codes defined in "utf8.h".
// "ap" is the va_list initialized in the various i18n-methods.
// "output_mode" is either "i18n_OUTPUT_MODE_DEV_NULL" for no output
//  at all (useful for string length measuring), "i18n_OUTPUT_MODE_STREAMS"
//  for sending output to "streams_utf8_output" and "i18n_OUTPUT_MODE_STRING"
//  to write the output to a string.
static long i18n_translate_from_va_list(z_ucs *module_name, int string_code,
    va_list ap, int output_mode, z_ucs *string_target)
{
  z_ucs *locale_name;
  locale_module *module;
  z_ucs *index;
  char parameter_types[11]; // May each contain 's', 'z' or 'd'. Using 11
                            // instead of ten so that a null byte may be
                            // placed after a char to print the error
                            // message as string.
  char *string_parameters[10]; // pointers to the parameters.
  z_ucs *z_ucs_string_parameters[10];
  char formatted_parameters[10][MAXIMUM_FORMATTED_PARAMTER_LENGTH + 1];
  z_ucs *start;
  uint8_t match_stage;
  int i,k;
  int n;
  z_ucs buf;
  size_t length;
  z_ucs *start_index;
  char index_char;
  z_ucs z_ucs_buffer[LATIN1_TO_Z_UCS_BUFFER_SIZE];
  char *ptr;

  locale_name
    = current_locale_name != NULL
    ? current_locale_name
    : default_locale_name;

  TRACE_LOG("Trying to get module '");
  TRACE_LOG_Z_UCS(module_name);
  TRACE_LOG("' for locale '");
  TRACE_LOG_Z_UCS(locale_name);
  TRACE_LOG("'.\n");

  if ((module = get_locale_module(locale_name, module_name)) == NULL)
  {
    TRACE_LOG("Module not found.\n");
    i18n_exit(-1, NULL);
  }

  TRACE_LOG("Got module at %p with %d messages at %p.\n",
      module, module->nof_messages, module->messages);

  if (string_code >= module->nof_messages)
  {
    TRACE_LOG("String %d code too large, exiting.\n", string_code);
    i18n_exit(-1, NULL);
  }

  index = module->messages[string_code];

  if (index == NULL)
    return -1;

  TRACE_LOG("Translating string code %d at %p.\n", string_code, index);

  n = 0;
  while ((index = z_ucs_chr(index, (z_ucs)'\\')) != NULL)
  {
    index++;
    start_index = index;
    index_char = zucs_char_to_latin1_char(*index);

    if (index_char == '{')
    {
      TRACE_LOG("'{' found.\n");
      index++;
      index_char = zucs_char_to_latin1_char(*index);
    }
    else
    {
      index = start_index;
      continue;
    }

    if ((index_char >= '0') && (index_char <= '9'))
    {
      TRACE_LOG("'[0-9]' found.\n");
      index++;
      index_char = zucs_char_to_latin1_char(*index);
    }
    else
    {
      index = start_index;
      continue;
    }

    if (
        (index_char == 's')
        ||
        (index_char == 'z')
        ||
        (index_char == 'd')
        ||
        (index_char == 'x')
       )
    {
      TRACE_LOG("'[szdx]' found.\n");
      parameter_types[n] = index_char;
      index++;
      index_char = zucs_char_to_latin1_char(*index);
    }
    else
    {
      index = start_index;
      continue;
    }

    if (index_char == '}')
    {
      TRACE_LOG("'}' found.\n");
      index++;
      index_char = zucs_char_to_latin1_char(*index);
      n++;
    }
    else
    {
      index = start_index;
      continue;
    }
  }

  TRACE_LOG("Found %d parameter codes.\n", n);

  if (n == 0)
  {
    TRACE_LOG("No parameter.\n");
    // In case we don't have a single parameter, we can just print
    // everything right away and quit.

    if (i18n_send_output(
          module->messages[string_code],
          output_mode,
          (string_target != NULL ? &string_target : NULL)) != 0)
      return -1;
    else
      return z_ucs_len(module->messages[string_code]);
  }

  length = 0;

  for (i=0; i<n; i++)
  {
    // parameter_types[0-n] are always defined, thus using "usedef" is okay.

    if (parameter_types[i] == 's')
    {
      string_parameters[i] = va_arg(ap, char*);
      TRACE_LOG("p#%d: %s\n", i, string_parameters[i]);
      length += strlen(string_parameters[i]);
    }
    else if (parameter_types[i] == 'z')
    {
      z_ucs_string_parameters[i] = va_arg(ap, z_ucs*);
      length += z_ucs_len(z_ucs_string_parameters[i]);
    }
    else if (parameter_types[i] == 'd')
    {
      (void)snprintf(formatted_parameters[i],
          MAXIMUM_FORMATTED_PARAMTER_LENGTH,
          "%ld",
          (long)va_arg(ap, long));
      TRACE_LOG("p#%d: %s\n", i, formatted_parameters[i]);
      length += strlen(formatted_parameters[i]);
    }
    else if (parameter_types[i] == 'x')
    {
      (void)snprintf(formatted_parameters[i],
          MAXIMUM_FORMATTED_PARAMTER_LENGTH,
          "%lx",
          (unsigned long)va_arg(ap, long));
      length += strlen(formatted_parameters[i]);
    }
    else
    {
      TRACE_LOG("Invalid parameter type: %c.\n", parameter_types[i]);
      parameter_types[i+1] = '\0';
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_INVALID_PARAMETER_TYPE_P0S,
          -1,
          parameter_types+i);
    }
  }

  TRACE_LOG("Length: %zd.\n", length);

  start = module->messages[string_code];
  i = 0;
  match_stage = 0;

  while (start[i] != 0)
  {
    if (match_stage == 1)
    {
      // We've already found a leading backslash.

      if ((start[i] == Z_UCS_BACKSLASH) && (match_stage == 1))
      {
        // Found another backslash, so output.
        (void)latin1_string_to_zucs_string(
            z_ucs_buffer,
            "\\",
            LATIN1_TO_Z_UCS_BUFFER_SIZE);

        if (i18n_send_output(z_ucs_buffer,
              output_mode,
              (string_target != NULL ? &string_target : NULL))
            != 0)
          i18n_translate_and_exit(
              libfizmo_module_name,
              i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
              -1,
              "i18n_send_output");

        match_stage = 0;
        length++;
        i++;
      }

      else if (start[i] == (z_ucs)'{')
      {
        // Here we've found a parameter. First, output everything up to
        // the parameter excluding "\{". In order to achive that, we'll
        // replace the first byte that shouldn't be printed with the
        // string-terminating 0 and restore it after that for the next
        // use of this message.
        buf = start[i-1];
        start[i-1] = 0;
        if (i18n_send_output(
              start,
              output_mode,
              (string_target != NULL ? &string_target : NULL))
            != 0)
          i18n_translate_and_exit(
              libfizmo_module_name,
              i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
              -1,
              "i18n_send_output");

        length += z_ucs_len(start);
        start[i-1] = buf;

        // After that, output parameter (subtract 0x30 == ASCII:'0')
        k = (int)(start[i+1] - 0x30);

        if (parameter_types[k] == 's')
        {
          ptr = string_parameters[k];
          TRACE_LOG("%s\n", ptr);
          while (ptr != NULL)
          {
            ptr = latin1_string_to_zucs_string(
                z_ucs_buffer,
                ptr,
                LATIN1_TO_Z_UCS_BUFFER_SIZE);

            if (i18n_send_output(
                  z_ucs_buffer,
                  output_mode,
                  (string_target != NULL ? &string_target : NULL))
                !=0 )
              i18n_translate_and_exit(
                  libfizmo_module_name,
                  i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
                  -1,
                  "i18n_send_output");
          }
        }
        else if (parameter_types[k] == 'z')
        {
          if (i18n_send_output(
                z_ucs_string_parameters[k],
                output_mode,
                (string_target != NULL ? &string_target : NULL))
              != 0)
            i18n_translate_and_exit(
                libfizmo_module_name,
                i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
                -1,
                "i18n_send_output");
        }
        else
        {
          ptr = formatted_parameters[k];
          while (ptr != NULL)
          {
            ptr = latin1_string_to_zucs_string(
                z_ucs_buffer,
                ptr,
                LATIN1_TO_Z_UCS_BUFFER_SIZE);

            if (i18n_send_output(
                  z_ucs_buffer,
                  output_mode,
                  (string_target != NULL ? &string_target : NULL))
                != 0)
              i18n_translate_and_exit(
                  libfizmo_module_name,
                  i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
                  -1,
                  "i18n_send_output");
          }
        }

        start += i + 4;
        i = 0;
        match_stage = 0;
      }
      else
      {
        i18n_translate_and_exit(
            libfizmo_module_name,
            i18n_libfizmo_INVALID_BACKSLASH_SEQUENCE_IN_LOCALIZATION_DATA,
            -1);
      }
    }
    else
    {
      if ((start[i] == Z_UCS_BACKSLASH) && (match_stage == 0))
      {
        // Found leading backslash;
        match_stage = 1;
        i++;
      }
      else
      {
        // Found nothing, next char (non memchar since operating on z_ucs)
        i++;
      }
    }
  }

  if (i != 0)
  {
    if (i18n_send_output(
          start,
          output_mode,
          (string_target != NULL ? &string_target : NULL))
        != 0)
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
          -1,
          "i18n_send_output");
  }

  length += z_ucs_len(start);

  TRACE_LOG("Final length:%zd.\n", length);

  return length;
}


void i18n_translate_and_exit(
    z_ucs *module_name,
    int string_code,
    int exit_code,
    ...)
{
  va_list ap;
  size_t message_length;
  z_ucs *error_message;

  TRACE_LOG("Exiting with message code %d.\n", string_code);

  va_start(ap, exit_code);
  message_length = i18n_translate_from_va_list(
      module_name,
      string_code,
      ap,
      i18n_OUTPUT_MODE_DEV_NULL,
      NULL);
  va_end(ap);

  TRACE_LOG("Message length: %zu.\n", message_length);

  if ((error_message = (z_ucs*)malloc((message_length+3) * sizeof(z_ucs)))
      == NULL)
    return;
    
  TRACE_LOG("Error message at: %p.\n", error_message);

  va_start(ap, exit_code);
  // Translate message into "error_message":
  if (i18n_translate_from_va_list(
        module_name,
        string_code,
        ap,
        i18n_OUTPUT_MODE_STRING,
        error_message)
      < 0)
  {
    TRACE_LOG("Code < 0\n");
    free(error_message);
    i18n_exit(exit_code, NULL);
  }
  va_end(ap);

  TRACE_LOG("Exit message: \"");
  // error_message is now defined:
  TRACE_LOG_Z_UCS(error_message);
  TRACE_LOG("\".\n");

  error_message[message_length] = Z_UCS_NEWLINE;
  error_message[message_length+1] = 0;

  i18n_exit(exit_code, error_message);
}


size_t _i18n_va_translate(z_ucs *module_name, int string_code, va_list ap)
{
  return i18n_translate_from_va_list(
      module_name,
      string_code,
      ap,
      i18n_OUTPUT_MODE_STREAMS,
      NULL);
}


size_t i18n_translate(z_ucs *module_name, int string_code, ...)
{
  va_list ap;
  size_t result;
  
  va_start(ap, string_code);
  result = _i18n_va_translate(module_name, string_code, ap); 
  va_end(ap);

  return result;
}


size_t i18n_message_length(z_ucs *module_name, int string_code, ...)
{
  va_list ap;
  size_t result;

  va_start(ap, string_code);

  result
    = i18n_translate_from_va_list(
        module_name,
        string_code,
        ap,
        i18n_OUTPUT_MODE_DEV_NULL,
        NULL);

  va_end(ap);

  return result;
}


// Returnes malloc()ed string which needs to be free()d later on.
z_ucs *i18n_translate_to_string(z_ucs* module_name, int string_code, ...)
{
  va_list ap;
  size_t message_length;
  z_ucs *result;

  va_start(ap, string_code);
  if ((message_length = i18n_translate_from_va_list(
          module_name,
        string_code,
        ap,
        i18n_OUTPUT_MODE_DEV_NULL,
        NULL)) < 1)
    return NULL;
  va_end(ap);

  if ((result = (z_ucs*)malloc((message_length+1) * sizeof(z_ucs))) == NULL)
    return NULL;

  va_start(ap, string_code);
  // The "i18n_translate_from_va_list"-call defines "result".
  if (i18n_translate_from_va_list(
        module_name,
        string_code,
        ap,
        i18n_OUTPUT_MODE_STRING,
        result) == -1)
  {
    free(result);
    return NULL;
  }
  va_end(ap);

  return result;
}


char *get_i18n_search_path()
{
  return locale_search_path == NULL
    ? default_search_path
    : locale_search_path;
}


int set_i18n_search_path(char *path)
{
  char *path_dup;

  if ((path_dup = strdup(path)) == NULL)
    return -1;

  if (locale_search_path != NULL)
    free(locale_search_path);

  locale_search_path = path_dup;

  TRACE_LOG("New search path: \"%s\".\n", locale_search_path);

  return 0;
}


char **get_available_locale_names()
{
  z_dir *dir;
  char *search_path;
  z_ucs *search_path_zucs, *path_ptr, *colon_index, *zucs_ptr;
  z_ucs *zucs_buf = NULL;
  size_t bufsize = 0, len;
  struct z_dir_ent z_dir_entry;
  char *dirname;
  list *result_list = create_list();
  char *new_locale_name;
  int j;

  search_path
    = locale_search_path == NULL
    ? default_search_path
    : locale_search_path;

  search_path_zucs = dup_utf8_string_to_zucs_string(search_path);

  TRACE_LOG("Search path: \"");
  TRACE_LOG_Z_UCS(search_path_zucs);
  TRACE_LOG("'.\n");

  path_ptr = search_path_zucs;
  while (*path_ptr != 0)
  {
    colon_index = z_ucs_chr(path_ptr, Z_UCS_COLON);

    len
      = colon_index == NULL
      ? z_ucs_len(path_ptr)
      : (size_t)(colon_index - path_ptr);

    TRACE_LOG("len: %ld\n", (long)len);

    if (len > 0)
    {
      if (bufsize < len + 1)
      {
        TRACE_LOG("realloc buf for %ld chars / %ld bytes.\n",
            (long)len + 1, (long)sizeof(z_ucs) * (len + 1));

        // open-resource:
        if ((zucs_ptr = realloc(zucs_buf, sizeof(z_ucs) * (len + 1))) == NULL)
        {
          // exit-point:
          TRACE_LOG("realloc() returned NULL.\n");
          free(zucs_buf);
          free(path_ptr);
          for (j=0; j<get_list_size(result_list); j++)
            free(get_list_element(result_list, j));
          delete_list(result_list);
          return NULL;
        }

        bufsize = len + 1;
        zucs_buf = zucs_ptr;
      }

      z_ucs_ncpy(zucs_buf, path_ptr, len);
      zucs_buf[len] = 0;

      // open-resource:
      if ((dirname = dup_zucs_string_to_utf8_string(zucs_buf)) == NULL)
      {
        // exit-point:
        TRACE_LOG("dup_zucs_string_to_utf8_string() returned NULL.\n");
        free(zucs_buf);
        free(path_ptr);
        for (j=0; j<get_list_size(result_list); j++)
          free(get_list_element(result_list, j));
        delete_list(result_list);
        return NULL;
      }

      TRACE_LOG("Path: '%s'\n", dirname);

      // open-resource:
      if ((dir = fsi->open_dir(dirname)) != NULL)
      {
        while (fsi->read_dir(&z_dir_entry, dir) == 0)
        {
          TRACE_LOG("Processing \"%s\".\n", z_dir_entry.d_name);

          if (
              (strcmp(".", z_dir_entry.d_name) != 0)
              &&
              (strcmp("..", z_dir_entry.d_name) != 0)
             )
          {
            for (j=0; j<get_list_size(result_list); j++)
            {
              if (strcmp(
                    (char*)get_list_element(result_list, j),
                    z_dir_entry.d_name) == 0)
                break;
            }

            if (j == get_list_size(result_list))
            {
              if ((new_locale_name = strdup(z_dir_entry.d_name)) == NULL)
              {
                TRACE_LOG("strdup() returned NULL.\n");
                free(zucs_buf);
                free(path_ptr);
                for (j=0; j<get_list_size(result_list); j++)
                  free(get_list_element(result_list, j));
                delete_list(result_list);
                return NULL;
              }
              add_list_element(result_list, new_locale_name);
            }
          }
        }

        // close-resource:
        fsi->close_dir(dir);
      }

      // close-resource:
      free(dirname);
    }

    path_ptr += len + (colon_index != NULL ? 1 : 0);
  }

  free(search_path_zucs);

  // close-resource:
  free(zucs_buf);

  //TRACE_LOG("res:'");
  //TRACE_LOG_Z_UCS(locale_dir_name);
  //TRACE_LOG("\n");

  return (char**)delete_list_and_get_null_terminated_ptrs(result_list);
}


z_ucs *get_current_locale_name()
{
  return current_locale_name != NULL
    ? current_locale_name
    : default_locale_name;
}


char *get_current_locale_name_in_utf8()
{
  return current_locale_name_in_utf8 != NULL
    ? current_locale_name_in_utf8
    : get_default_locale_name();
}


int set_current_locale_name(char *new_locale_name)
{
  char *locale_dir_name = NULL;
  z_ucs *locale_dup;
  char *locale_dup_utf8;
  int i, j;

  if (new_locale_name == NULL)
    return -1;

  // Check if the locale name given was an alias: If, in case "en" was
  // given as locale name, this should be interpreted as "en_GB" instead --
  // see the "locale_aliases" definition above.
  i = 0;
  while (locale_aliases[i][0] != NULL)
  {
    // Test all aliases for current locale name:
    j = 1;
    while (locale_aliases[i][j] != NULL)
    {
      if (strcmp(locale_aliases[i][j], new_locale_name) == 0)
        break;
      j++;
    }

    if (locale_aliases[i][j] != NULL)
    {
      // We've found an alias.
      new_locale_name = locale_aliases[i][0];
      TRACE_LOG("Locale name \"%s\" is an alias for \"%s\".\n",
          new_locale_name, new_locale_name);
      break;
    }

    i++;
  }

  if ((locale_dup =
        dup_utf8_string_to_zucs_string(new_locale_name)) == NULL)
    return -1;

  if ((locale_dup_utf8 =
        strdup(new_locale_name)) == NULL)
  {
    free(locale_dup);
    free(locale_dup_utf8);
    return -1;
  }

  if ((locale_dir_name = get_path_for_locale(locale_dup)) == NULL)
  {
    free(locale_dup);
    free(locale_dup_utf8);
    return -1;
  }

  free(locale_dir_name);

  if (current_locale_name != NULL)
  {
    free(current_locale_name);
    free(current_locale_name_in_utf8);
  }

  current_locale_name = locale_dup;
  current_locale_name_in_utf8 = strdup(locale_dup_utf8);

  TRACE_LOG("New locale name: '");
  TRACE_LOG_Z_UCS(current_locale_name);
  TRACE_LOG("'.\n");
  return 0;
}


char *get_i18n_default_search_path(void)
{
  return default_search_path;
}


void free_i18n_memory(void)
{
  z_ucs **locale_names, **locale_name;
  z_ucs **module_names, **module_name;
  stringmap *module_map;
  locale_module *module= NULL;

  if (locale_modules != NULL)
  {
    locale_names = get_names_in_stringmap(locale_modules);
    locale_name = locale_names;

    while (*locale_name != NULL)
    {
      module_map = (stringmap*)get_stringmap_value(locale_modules,*locale_name);

      module_names = get_names_in_stringmap(module_map);
      module_name = module_names;

      while (*module_name != NULL)
      {
        module = (locale_module*)get_stringmap_value(module_map, *module_name);
        delete_locale_module(module);
        module_name++;
      }
      free(module_names);

      delete_stringmap(module_map);
      locale_name++;
    }

    free(locale_names);
    delete_stringmap(locale_modules);
    locale_modules = NULL;
  }

  if (current_locale_name != NULL)
  {
    free(current_locale_name);
    current_locale_name = NULL;
    free(current_locale_name_in_utf8);
    current_locale_name_in_utf8 = NULL;
  }

  if (default_locale_name_in_utf8 == NULL)
  {
    free(default_locale_name_in_utf8);
    default_locale_name_in_utf8 = NULL;
  }
}

#endif /* i18n_c_INCLUDED */

