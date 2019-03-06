
/* z_ucs.h
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


#ifndef z_ucs_h_INCLUDED
#define z_ucs_h_INCLUDED

#include <string.h>
#include <stdio.h>

#include "types.h"

#define UEOF ((z_ucs)-1)
#define UTF_8_MB_LEN 6
#define Z_UCS_NEWLINE ((z_ucs)'\n')
#define Z_UCS_SPACE ((z_ucs)' ')
#define Z_UCS_BACKSLASH ((z_ucs)'\\')
#define Z_UCS_SLASH ((z_ucs)'/')
#define Z_UCS_COLON ((z_ucs)':')
#define Z_UCS_DOT ((z_ucs)'.')
#define Z_UCS_MINUS ((z_ucs)'-')
#define Z_UCS_SOFT_HYPEN ((z_ucs)0xad)
#define Z_UCS_COMMA ((z_ucs)',')

typedef struct
{
  z_ucs *data;
  size_t len;
  size_t bytes_allocated;
} zucs_string;

size_t z_ucs_len(z_ucs *string);
z_ucs *z_ucs_cpy(z_ucs *dst, z_ucs *src);
void z_ucs_ncpy(z_ucs *dst, z_ucs *src, size_t len);
int z_ucs_cmp(z_ucs *s1, z_ucs *s2);
z_ucs *z_ucs_chr(z_ucs *s1, z_ucs chr);
z_ucs *z_ucs_chrs(z_ucs *s1, z_ucs *chars);
z_ucs *z_ucs_rchr(z_ucs *s1, z_ucs chr);
z_ucs *z_ucs_rchrs(z_ucs *s1, z_ucs *chars);
z_ucs *z_ucs_cat(z_ucs *dst, z_ucs *src);
z_ucs *z_ucs_dup(z_ucs *src);

z_ucs latin1_char_to_zucs_char(char c);
char *latin1_string_to_zucs_string(z_ucs *dest, char *src,
    size_t max_dest_size);
char zucs_char_to_latin1_char(z_ucs src);
z_ucs *z_ucs_cat_latin1(z_ucs *dst, char *src);
int z_ucs_cmp_latin1(z_ucs *s1, char *s2);
z_ucs *dup_latin1_string_to_zucs_string(char *src);

z_ucs parse_utf8_char_from_file(z_file *fileref);
z_ucs utf8_char_to_zucs_char(char **src);
char *utf8_string_to_zucs_string(z_ucs *dest, char *src, int max_dest_size);
z_ucs *dup_utf8_string_to_zucs_string(char *src);
int zucs_string_to_utf8_string(char *dst, z_ucs **src, size_t max_dst_size);
char *dup_zucs_string_to_utf8_string(z_ucs *src);

//void z_ucs_output_number(z_ucs *dst, long int number); // dead?

#endif // z_ucs_h_INCLUDED

