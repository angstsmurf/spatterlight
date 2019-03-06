
/* config.c
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


#ifndef config_c_INCLUDED 
#define config_c_INCLUDED

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#if !defined (__WIN32__)
#include <pwd.h>
#endif // !defined (__WIN32__)

#include "../tools/tracelog.h"
#include "config.h"
#include "fizmo.h"
#include "mathemat.h"
#include "output.h"
#include "../tools/types.h"
#include "../tools/i18n.h"
#include "../locales/libfizmo_locales.h"

#define BUFSIZE 5

//int system_charset = SYSTEM_CHARSET_ASCII;
bool auto_adapt_upper_window = true;
bool auto_open_upper_window = true;
bool skip_active_routines_stack_check_warning = false;
char config_true_value[] = "true";
char config_false_value[] = "false";
char empty_string[] = "";
bool user_homedir_initialized = false;
char *user_homedir = NULL;


struct configuration_option configuration_options[] = {

  // String values:
  { "autosave-filename", NULL },
  { "background-color", NULL },
  { "foreground-color", NULL },
  { "i18n-search-path", NULL },
  { "input-command-filename", NULL },
  { "locale", NULL },
  { "max-undo-steps", NULL },
  { "random-mode", NULL },
  { "record-command-filename", NULL },
  { "save-text-history-paragraphs", NULL },
  { "savegame-default-filename", NULL },
  { "savegame-path", NULL },
  { "stream-2-left-margin", NULL },
  { "stream-2-line-width", NULL },
  { "transcript-filename", NULL },
  { "z-code-path", NULL },
  { "z-code-root-path", NULL },

  // Boolean values:
  { "disable-external-streams", NULL },
  { "disable-restore", NULL },
  { "disable-save", NULL },
  { "disable-sound", NULL },
  { "disable-stream-2-hyphenation", NULL },
  { "disable-stream-2-wrap", NULL },
  { "dont-set-locale-from-config", NULL },
  { "enable-font3-conversion", NULL },
  { "quetzal-umem", NULL },
  { "restore-after-save-and-quit-file-before-read", NULL },
  { "save-and-quit-file-before-read", NULL },
  { "set-tandy-flag", NULL },
  { "start-command-recording-when-story-starts", NULL },
  { "start-file-input-when-story-starts", NULL },
  { "start-script-when-story-starts", NULL },
  { "sync-transcript", NULL },
  { "flush-output-on-newline", NULL },

  // NULL terminates the option list.
  { NULL, NULL }

};


static char *expand_configuration_value(char *unexpanded_value)
{
  static char *homedir;
  static int homedir_len;
  char *ptr = unexpanded_value;
  int resultlen;
  char *result, *resultindex;
  char *var_name;
  char buf;

  if (unexpanded_value == NULL)
    return NULL;

  if ((homedir = get_user_homedir()) == NULL)
    homedir = empty_string;
  homedir_len = strlen(homedir);

  TRACE_LOG("Value to expand: \"%s\".\n", unexpanded_value);

  resultlen = 0;
  while (*ptr != 0)
  {
    if (*ptr == '$')
    {
      ptr++;
      if (*ptr == '(')
      {
        ptr++;
        var_name = ptr;
        while ( (*ptr != 0) && (*ptr != ')') )
          ptr++;
        if (*ptr != ')')
          return NULL;
        buf = *ptr;
        *ptr = 0;

        if (strcmp(var_name, "HOME") == 0)
        {
          resultlen += strlen(homedir);
        }
        else
        {
          *ptr = buf;
          return NULL;
        }

        *ptr = buf;
        ptr++;
      }
      else
        return NULL;
    }
    else
    {
      ptr++;
      resultlen++;
    }
  }

  TRACE_LOG("result len: %d.\n", resultlen);
  result = fizmo_malloc(resultlen + 1);
  resultindex = result;

  ptr = unexpanded_value;
  while (*ptr != 0)
  {
    if (*ptr == '$')
    {
      ptr++;
      if (*ptr == '(')
      {
        ptr++;
        var_name = ptr;
        while ( (*ptr != 0) && (*ptr != ')') )
          ptr++;
        if (*ptr != ')')
        {
          free(result);
          return NULL;
        }
        buf = *ptr;
        *ptr = 0;

        if (strcmp(var_name, "HOME") == 0)
        {
          strcpy(resultindex, homedir);
          resultindex += homedir_len;
        }
        else
        {
          *ptr = buf;
          // Can't ever reach this point due to loop 1.
        }

        *ptr = buf;
        ptr++;
      }
    }
    else
    {
      *resultindex = *ptr;
      ptr++;
      resultindex++;
    }
  }

  *resultindex = 0;

  TRACE_LOG("result expanded value: %s / %p.\n", result, result);
  return result;
}


int append_path_value(char *key, char *value_to_append)
{
  char *str, *str2;
  int result;

  if ((str = get_configuration_value(key)) != NULL)
  {
    str2 = fizmo_malloc(strlen(str) + strlen(value_to_append) + 2);
    strcpy(str2, str);
    strcat(str2, ":");
    strcat(str2, value_to_append);
    TRACE_LOG("Appended path: %s.\n", str2);
    result = set_configuration_value(key, str2);
    free(str2);
    return result;
  }
  else
    return set_configuration_value(key, value_to_append);
}


int set_configuration_value(char *key, char* new_unexpanded_value)
{
  int i, return_code, result;
  char *new_value = NULL;
  char buf[BUFSIZE];
  short color_code;
  char *endptr;


  if (key == NULL)
    return -1;

  if (new_unexpanded_value != NULL)
    if ((new_value = expand_configuration_value(new_unexpanded_value)) == NULL)
      return -1;

#ifdef ENABLE_TRACING
  TRACE_LOG("Setting configuration key \"%s\".\n", key);
  if (new_value != NULL)
  {
    TRACE_LOG("New value: %s at %p.\n", new_value, new_value);
  }
#endif //ENABLE_TRACING

  i = 0;

  while (configuration_options[i].name != NULL)
  {
    TRACE_LOG("i:%d, name:%s.\n", i, configuration_options[i].name);

    if (strcmp(configuration_options[i].name, key) == 0)
    {
      // Parse option values which cannot be simply copied:

      if (strcmp(key, "locale") == 0)
      {
        TRACE_LOG("Trying to set locale to \"%s\".\n", new_value);
        return_code
          = (strcmp(get_configuration_value(
                  "dont-set-locale-from-config"), "true") == 0)
          ? 0
          : set_current_locale_name(new_value);
        free(new_value);
        return return_code;
      }
      else if (strcmp(key, "random-mode") == 0)
      {
        if (new_value == NULL)
          return -1;
        else if (strcmp(new_value, "random") == 0)
        {
          if (configuration_options[i].value != NULL)
            free(configuration_options[i].value);
          configuration_options[i].value = new_value;
          seed_random_generator();
          return 0;
        }
        else if (strcmp(new_value, "predictable") == 0)
        {
          if (configuration_options[i].value != NULL)
            free(configuration_options[i].value);
          configuration_options[i].value = new_value;
          TRACE_LOG("stored value: %p.\n", configuration_options[i].value);
          seed_random_generator();
          return 0;
        }
        else
          return -1;
      }
      else if (strcmp(key, "i18n-search-path") == 0)
      {
        // Forward to i18n, since this is in tools and cannot access the
        // "config.c" file.
        return_code = set_i18n_search_path(new_value);
        free(new_value);
        return return_code;
      }

      // Options for values which can simply be copied.
      else if (
          (strcmp(key, "z-code-path") == 0)
          ||
          (strcmp(key, "z-code-root-path") == 0)
          ||
          (strcmp(key, "autosave-filename") == 0)
          ||
          (strcmp(key, "savegame-path") == 0)
          ||
          (strcmp(key, "savegame-default-filename") == 0)
          ||
          (strcmp(key, "transcript-filename") == 0)
          ||
          (strcmp(key, "input-command-filename") == 0)
          ||
          (strcmp(key, "record-command-filename") == 0)
          )
      {
        if (configuration_options[i].value != NULL)
          free(configuration_options[i].value);
        configuration_options[i].value = new_value;
        return 0;
      }

      // Integer values
      else if (
          (strcmp(key, "stream-2-line-width") == 0)
          ||
          (strcmp(key, "stream-2-left-margin") == 0)
          ||
          (strcmp(key, "max-undo-steps") == 0)
          ||
          (strcmp(key, "save-text-history-paragraphs") == 0)
          )
      {
        if (new_value == NULL)
          return -1;
        if (strlen(new_value) == 0)
        {
          free(new_value);
          return -1;
        }
        strtol(new_value, &endptr, 10);
        if (*endptr != 0)
        {
          free(new_value);
          return -1;
        }
        if (configuration_options[i].value != NULL)
          free(configuration_options[i].value);
        configuration_options[i].value = new_value;
        return 0;
      }

      // Color options
      else if (strcmp(key, "foreground-color") == 0)
      {
        if (new_value == NULL)
          return -1;
        color_code = color_name_to_z_colour(new_value);
        free(new_value);
        if (color_code == -1)
          return -1;
        if (snprintf(buf, BUFSIZE, "%d", color_code) >= BUFSIZE)
          return -1;
        if (configuration_options[i].value != NULL)
          free(configuration_options[i].value);
        configuration_options[i].value = fizmo_strdup(buf);
        default_foreground_colour = color_code;
        return 0;
      }
      else if (strcmp(key, "background-color") == 0)
      {
        if (new_value == NULL)
          return -1;
        color_code = color_name_to_z_colour(new_value);
        free(new_value);
        if (color_code == -1)
          return -1;
        if (snprintf(buf, BUFSIZE, "%d", color_code) >= BUFSIZE)
          return -1;
        if (configuration_options[i].value != NULL)
          free(configuration_options[i].value);
        configuration_options[i].value = fizmo_strdup(buf);
        default_background_colour = color_code;
        return 0;
      }

      // Boolean options
      else if (
          (strcmp(key, "disable-external-streams") == 0)
          ||
          (strcmp(key, "disable-restore") == 0)
          ||
          (strcmp(key, "disable-save") == 0)
          ||
          (strcmp(key, "disable-sound") == 0)
          ||
          (strcmp(key, "enable-font3-conversion") == 0)
          ||
          (strcmp(key, "quetzal-umem") == 0)
          ||
          (strcmp(key, "random-mode") == 0)
          ||
          (strcmp(key, "restore-after-save-and-quit-file-before-read") == 0)
          ||
          (strcmp(key, "save-and-quit-file-before-read") == 0)
          ||
          (strcmp(key, "set-tandy-flag") == 0)
          ||
          (strcmp(key, "start-command-recording-when-story-starts") == 0)
          ||
          (strcmp(key, "start-script-when-story-starts") == 0)
          ||
          (strcmp(key, "start-file-input-when-story-starts") == 0)
          ||
          (strcmp(key, "disable-stream-2-hyphenation") == 0)
          ||
          (strcmp(key, "disable-stream-2-wrap") == 0)
          ||
          (strcmp(key, "sync-transcript") == 0)
          ||
          (strcmp(key, "dont-set-locale-from-config") == 0)
          ||
          (strcmp(key, "flush-output-on-newline") == 0)
          )
      {
        if (
            (new_value == NULL)
            ||
            (*new_value == 0)
            ||
            (strcmp(new_value, config_true_value) == 0)
           )
        {
          if (new_value != NULL)
            free(new_value);
          if (configuration_options[i].value != NULL)
            free(configuration_options[i].value);
          configuration_options[i].value = fizmo_strdup(config_true_value);
          return 0;
        }
        else if ((new_value != NULL)
            && (strcmp(new_value, config_false_value)==0))
        {
          free(new_value);
          if (configuration_options[i].value != NULL)
            free(configuration_options[i].value);
          configuration_options[i].value = fizmo_strdup(config_false_value);
          return -1;
        }
        else
        {
          free(new_value);
          return -1;
        }
      }
      else
      {
        i18n_translate_and_exit(
            libfizmo_module_name,
            i18n_libfizmo_UNKNOWN_CONFIGURATION_OPTION_P0S,
            -0x0101,
            key);
      } 
    }

    i++;
  }

  if (active_interface != NULL)
  {
    result = active_interface->parse_config_parameter(key, new_value);
    if (result == -1)
    {
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_INVALID_VALUE_P0S_FOR_PARAMETER_P1S,
          -0x0101,
          key,
          new_value);
    }
  }

  if (active_sound_interface == NULL)
    return -2;
  else
  {
    result = active_sound_interface->parse_config_parameter(key, new_value);
    if (result == -1)
    {
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_INVALID_VALUE_P0S_FOR_PARAMETER_P1S,
          -0x0101,
          key,
          new_value);
      }

  }

  return result;
}


char *get_configuration_value(char *key)
{
  int i = 0;
  char **interface_option_names;

  if (key == NULL)
    return NULL;

  TRACE_LOG("Retrieving config value: %s.\n", key);

  if (strcmp(key, "i18n-search-path") == 0)
  {
    // Forward to i18n, since this is in tools and cannot access the
    // "config.c" file.
    return get_i18n_search_path();
  }
  else if (strcmp(key, "locale") == 0)
  {
    return get_current_locale_name_in_utf8();
  }
  else if (
      (strcmp(key, "background-color-name") == 0)
      ||
      (strcmp(key, "foreground-color-name") == 0)
      )
  {
    return z_colour_names[
      atoi(
          get_configuration_value(
            ( strcmp(key, "background-color-name") == 0
              ? "background-color"
              : "foreground-color")))];
  }
  else
  {
    while (configuration_options[i].name != NULL)
    {
      TRACE_LOG("i:%d, name:%s.\n", i, configuration_options[i].name);

      if (strcmp(configuration_options[i].name, key) == 0)
      {
        // Boolean options
        if (
            (strcmp(key, "disable-external-streams") == 0)
            ||
            (strcmp(key, "disable-restore") == 0)
            ||
            (strcmp(key, "disable-save") == 0)
            ||
            (strcmp(key, "disable-sound") == 0)
            ||
            (strcmp(key, "enable-font3-conversion") == 0)
            ||
            (strcmp(key, "quetzal-umem") == 0)
            ||
            (strcmp(key, "restore-after-save-and-quit-file-before-read") == 0)
            ||
            (strcmp(key, "save-and-quit-file-before-read") == 0)
            ||
            (strcmp(key, "set-tandy-flag") == 0)
            ||
            (strcmp(key, "start-command-recording-when-story-starts") == 0)
            ||
            (strcmp(key, "start-script-when-story-starts") == 0)
            ||
            (strcmp(key, "start-file-input-when-story-starts") == 0)
            ||
            (strcmp(key, "disable-stream-2-hyphenation") == 0)
            ||
            (strcmp(key, "disable-stream-2-wrap") == 0)
            ||
            (strcmp(key, "sync-transcript") == 0)
            ||
            (strcmp(key, "dont-set-locale-from-config") == 0)
            ||
            (strcmp(key, "flush-output-on-newline") == 0)
           )
        {
          if (configuration_options[i].value == NULL)
          {
            TRACE_LOG("return \"false\".\n");
            return config_false_value;
          }
          else
          {
            TRACE_LOG("return \"%s\".\n", configuration_options[i].value);
            return configuration_options[i].value;
          }
        }

        // String options
        else if (
            (strcmp(key, "random-mode") == 0)
            ||
            (strcmp(key, "z-code-path") == 0)
            ||
            (strcmp(key, "z-code-root-path") == 0)
            ||
            (strcmp(key, "autosave-filename") == 0)
            ||
            (strcmp(key, "savegame-path") == 0)
            ||
            (strcmp(key, "savegame-default-filename") == 0)
            ||
            (strcmp(key, "transcript-filename") == 0)
            ||
            (strcmp(key, "input-command-filename") == 0)
            ||
            (strcmp(key, "record-command-filename") == 0)
            ||
            (strcmp(key, "background-color") == 0)
            ||
            (strcmp(key, "foreground-color") == 0)
            ||
            (strcmp(key, "save-text-history-paragraphs") == 0)
            ||
            (strcmp(key, "stream-2-line-width") == 0)
            ||
            (strcmp(key, "stream-2-left-margin") == 0)
            ||
            (strcmp(key, "max-undo-steps") == 0)
            )
        {
          TRACE_LOG("Returning value at %p.\n", configuration_options[i].value);
          return configuration_options[i].value;
        }

        else
        {
          // Internal error: config-key was found in configuration_options[],
          // but not in the hardcoded keys above.
          TRACE_LOG("Unknown config key: \"%s\".", key);
          return NULL;
        }
      }

      i++;
    }

    if (active_interface != NULL)
    {
      if ((interface_option_names
            = active_interface->get_config_option_names()) != NULL)
      {
        i = 0;
        while (interface_option_names[i] != NULL)
          if (strcmp(interface_option_names[i++], key) == 0)
            return active_interface->get_config_value(key);
      }
    }

    if (active_sound_interface != NULL)
    {
      if ((interface_option_names
            = active_sound_interface->get_config_option_names()) != NULL)
      {
        i = 0;
        while (interface_option_names[i] != NULL)
          if (strcmp(interface_option_names[i++], key) == 0)
            return active_sound_interface->get_config_value(key);
      }
    }

    TRACE_LOG("Unknown config key: \"%s\".", key);
    return NULL;
  }
}


/*
char **get_valid_configuration_options(char *key, ...)
{
  char **result;
  int result_index;
  size_t memory_size;
  va_list ap;
  char *ptr;
  int i;

  if (strcmp(key, "locale") == 0)
  {
    va_start(ap, key);
    ptr = va_arg(ap, char*);
    va_end(ap);

    if (ptr == NULL)
      return NULL;

    result_index = 0;
    for (i=0; i<NUMBER_OF_LOCALES; i++)
      if (strcmp(locales[i].module_name, "fizmo") == 0)
        result_index++;

    memory_size = (result_index + 1) * sizeof(char*);

    result = (char**)fizmo_malloc(memory_size);

    result_index = 0;
    for (i=0; i<NUMBER_OF_LOCALES; i++)
      if (strcmp(locales[i].module_name, "fizmo") == 0)
        result[result_index++] = fizmo_strdup(locales[i].language_code);

    result[result_index] = NULL;

    return result;
  }
  else
    return NULL;
}
*/


bool is_valid_libfizmo_config_key(char *key)
{
  int i = 0;

  while (configuration_options[i].name != NULL)
  {
    if (strcmp(configuration_options[i++].name, key) == 0)
      return true;
  }
  return false;
}


char *get_user_homedir()
{
#if !defined (__WIN32__)
  struct passwd *pw_entry;
#endif // !defined (__WIN32__)

  if (user_homedir_initialized == false)
  {
#if !defined (__WIN32__)
    pw_entry = getpwuid(getuid());
    if (pw_entry->pw_dir == NULL)
      user_homedir = NULL;
    else
      user_homedir = strdup(pw_entry->pw_dir);
#else
    if ((user_homedir = getenv("HOME")) == NULL)
      user_homedir = getenv("HOMEPATH");
#endif // !defined (__WIN32__)
    user_homedir_initialized = true;
  }

  return user_homedir;
}


void init_config_default_values() {
  set_configuration_value("save-text-history-paragraphs", "1000");
}


#endif /* config_c_INCLUDED */

