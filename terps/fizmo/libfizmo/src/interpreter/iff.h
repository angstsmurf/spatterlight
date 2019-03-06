
/* iff.h
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


#ifndef iff_h_INCLUDED 
#define iff_h_INCLUDED

#include <stdio.h>

#include "../tools/types.h"

#define IFF_MODE_READ 0
#define IFF_MODE_READ_SAVEGAME 1
#define IFF_MODE_WRITE 2
#define IFF_MODE_WRITE_SAVEGAME 3

bool detect_simple_iff_stream(z_file *iff_file);
int init_empty_file_for_iff_write(z_file *file_to_init);
z_file *open_simple_iff_file(char *filename, int mode);
int start_new_chunk(char *id, z_file *iff_file);
int end_current_chunk(z_file *iff_file);
int close_simple_iff_file(z_file *iff_file);
int find_chunk(char *id, z_file *iff_file);
int read_chunk_length(z_file *iff_file);
int get_last_chunk_length();
int write_four_byte_number(uint32_t number, z_file *iff_file);
uint32_t read_four_byte_number(z_file *iff_file);
char *read_form_type(z_file *iff_file);
bool is_form_type(z_file *iff_file, char* form_type);

#endif /* iff_h_INCLUDED */

