
/* streams.h
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


#ifndef streams_h_INCLUDED
#define streams_h_INCLUDED

#include "../tools/types.h"

void init_streams();
void open_input_stream_1(void);
void close_input_stream_1(void);
int streams_z_ucs_output(z_ucs *z_ucs_output);
int streams_z_ucs_output_user_input(z_ucs *z_ucs_output);
int streams_latin1_output(char *latin1_output);
void opcode_output_stream(void);
void open_streams(void);
void close_streams(/*@null@*/ z_ucs *error_message);
void opcode_input_stream(void);
z_file *get_stream_2(void);
void restore_stream_2(z_file *str);
void stream_2_remove_chars(size_t nof_chars_to_remove);
void ask_for_stream2_filename(void);
void ask_for_stream4_filename_if_required(void);
void stream_4_latin1_output(char *latin1_output);
void stream_4_z_ucs_output(z_ucs *z_ucs_output);
void ask_for_input_stream_filename(void);

#ifndef streams_c_INCLUDED
extern bool stream_1_active;
extern bool stream_4_active;
extern bool input_stream_1_active;
extern bool input_stream_1_was_already_active;
extern z_file *input_stream_1;
extern size_t input_stream_1_filename_size;
extern char *input_stream_1_filename;
extern z_ucs last_script_filename[];
/*@-exportlocal@*/
extern bool stream_output_has_occured;
/*@+exportlocal@*/
// Since this flag is used by the screen interface, splint can not now about
// it.
#endif // streams_c_INCLUDED

#endif // streams_h_INCLUDED

