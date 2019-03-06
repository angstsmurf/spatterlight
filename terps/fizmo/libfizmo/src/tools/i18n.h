
/* i18n.h
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


#ifndef i18n_h_INCLUDED
#define i18n_h_INCLUDED

#include "types.h"

// 2^64 results in a number containing 20 digits.
#define MAXIMUM_FORMATTED_PARAMTER_LENGTH 20

#define i18n_OUTPUT_MODE_DEV_NULL 0
#define i18n_OUTPUT_MODE_STREAMS 1
#define i18n_OUTPUT_MODE_STRING 2

#include <stdarg.h>

typedef struct
{
  z_ucs *locale_data;
  z_ucs *module_name;
  z_ucs **messages;
  int nof_messages;
} locale_module;


void register_i18n_stream_output_function(
    int (*new_stream_output_function)(z_ucs *output));
void register_i18n_abort_function(
    void (*new_abort_function)(int exit_code, z_ucs *error_message));
size_t _i18n_va_translate(z_ucs *module_name, int string_code, va_list ap);
size_t i18n_translate(z_ucs *module_name, int string_code, ...);
void i18n_translate_and_exit(z_ucs *module_name, int string_code,
    int exit_code, ...);
size_t i18n_message_length(z_ucs *module_name, int string_code, ...);
z_ucs *i18n_translate_to_string(z_ucs *module_name, int string_code, ...);

char *get_i18n_search_path();
int set_i18n_search_path(char *path);

z_ucs *get_current_locale_name();
char *get_current_locale_name_in_utf8();
int set_current_locale_name(char *new_locale_name);
char **get_available_locale_names();
char *get_i18n_default_search_path(void);
void free_i18n_memory();

#endif /* i18n_h_INCLUDED */

