
/* z_ucs.c
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


#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "z_ucs.h"
#include "types.h"
#include "filesys.h"


size_t z_ucs_len(z_ucs *string)
{
  z_ucs *start_index = string;

  if (string == NULL)
    return 0;

  while (*string != 0)
  {
    string++;
  }

  return (size_t)(string - start_index);
}


z_ucs *z_ucs_cpy(z_ucs *dst, z_ucs *src)
{
  while (*src != 0)
  {
    *dst = *src;
    dst++;
    src++;
  }

  *dst = 0;

  return dst;
}


void z_ucs_ncpy(z_ucs *dst, z_ucs *src, size_t len)
{
  memmove(dst, src, len * sizeof(z_ucs));
}


// Returns > 0 in case s1 > s2, 0 if equal and < 0 otherwise.
int z_ucs_cmp(z_ucs *s1, z_ucs *s2)
{
  while ( (*s1 != 0) || (*s2 != 0) )
  {
    if (*s1 != *s2)
      return *s1 - *s2;

    s1++;
    s2++;
  }

  return 0;
}


z_ucs *z_ucs_chr(z_ucs *s1, z_ucs chr)
{
  while (*s1 != 0)
  {
    if (*s1 == chr)
      return s1;

    s1++;
  }

  return NULL;
}


z_ucs *z_ucs_chrs(z_ucs *s1, z_ucs *chars)
{
  z_ucs *ptr;

  while (*s1 != 0)
  {
    if ((ptr = z_ucs_chr(chars, *s1)) != NULL)
      return s1;

    s1++;
  }

  return NULL;
}


z_ucs *z_ucs_rchr(z_ucs *s1, z_ucs chr)
{
  z_ucs *ptr = s1 + z_ucs_len(s1);

  while (ptr >= s1)
  {
    if (*ptr == chr)
      return ptr;

    ptr--;
  }

  return NULL;
}


z_ucs *z_ucs_rchrs(z_ucs *s1, z_ucs *chars)
{
  z_ucs *ptr = s1 + z_ucs_len(s1), *ptr2;

  while (ptr >= s1)
  {
    if ((ptr2 = z_ucs_chr(chars, *ptr)) != NULL)
      return ptr;

    ptr--;
  }

  return NULL;
}


z_ucs *z_ucs_cat(z_ucs *dst, z_ucs *src)
{
  dst += z_ucs_len(dst);

  while (*src != 0)
    *(dst++) = *(src++);
  *dst = 0;

  return dst;
}


z_ucs *z_ucs_dup(z_ucs *src)
{
  z_ucs *result;

  if ((result = malloc(sizeof(z_ucs) * (z_ucs_len(src) + 1))) == NULL)
    return NULL;

  z_ucs_cpy(result, src);

  return result;
}


z_ucs latin1_char_to_zucs_char(char c)
{
  return (z_ucs)((unsigned char)c & 0xff);
}


char *latin1_string_to_zucs_string(z_ucs *dest, char *src, size_t max_dest_size)
{
  if (max_dest_size < 2)
    return NULL;

  while (*src != (char)0)
  {
    if (max_dest_size == 1)
    {
      *dest = 0;
      return src;
    }

    *dest = (z_ucs)(*src & 0xff);

    dest++;
    src++;
  }

  *dest = 0;
  return NULL;
}


char zucs_char_to_latin1_char(z_ucs src)
{
  if ((src & 0xffffff00) == 0)
    return (char)(src & 0xff);
  else
    return (char)('?');
}



z_ucs *z_ucs_cat_latin1(z_ucs *dst, char *src)
{
  while (*src != 0)
    *(dst++) = latin1_char_to_zucs_char(*(src++));
  *dst = 0;

  return dst;
}


int z_ucs_cmp_latin1(z_ucs *s1, char *s2)
{
  while ( (*s1 != 0) || (*s2 != 0) )
  {
    if ((char)*s1 != *s2)
      return -1;

    if ((*s1 & 0xffffff00) != 0)
      return -1;

    s1++;
    s2++;
  }

  if (*s2 != '\0')
    return -1;
  else
    return 0;
}


z_ucs *dup_latin1_string_to_zucs_string(char *src)
{
  z_ucs *result;
  int len, i;

  if (src == NULL)
    return NULL;

  len = strlen(src);
  if ((result = malloc(sizeof(z_ucs) * (len + 1))) == NULL)
    return NULL;

  for (i=0; i<len; i++)
    result[i] = latin1_char_to_zucs_char(src[i]);
  result[i] = 0;

  return result;
}


z_ucs parse_utf8_char_from_file(z_file *in)
{
  // FIXME: Fail on overlong UTF-8 characters.
  int current_char;
  int len;
  int i;
  int cmp_value1;
  int cmp_value2;
  z_ucs result;

  if ((current_char = fsi->readchar(in)) == EOF)
    return UEOF;

  if ((current_char & 0x80) == 0)
    return (z_ucs)current_char & 0x7f;

  len = 2;
  cmp_value1 = 0xe0;
  cmp_value2 = 0xc0;

  while (len <= 6)
  {
    if ((current_char & cmp_value1) == cmp_value2)
    {
      cmp_value1 = 1;

      for (i=0; i<6-len; i++)
      {
        // cmp_value1 is never negative.
        cmp_value1 <<= 1;
        cmp_value1 |= 1;
      }

      result = (z_ucs)(current_char & cmp_value1);

      // Found a sequence of length len.
      for (i=0; i<len-1; i++)
      {
        if ((current_char = fsi->readchar(in)) == EOF)
          return UEOF;

        result <<= 6;
        result |= (current_char & 0x3f);
      }

      return result;
    }

    cmp_value1 >>= 1;
    cmp_value1 |= 0x80;

    cmp_value2 >>= 1;
    cmp_value2 |= 0x80;

    len++;
  }

  return UEOF;
}


z_ucs utf8_char_to_zucs_char(char **src)
{
  // FIXME: Fail on overlong UTF-8 characters.
  int len;
  int i;
  int cmp_value1;
  int cmp_value2;
  z_ucs result;

  if ((**src & 0x80) == (char)0)
  {
    result = (z_ucs)**src & 0x7f;
    (*src)++;
    return result;
  }

  len = 2;
  cmp_value1 = 0xe0;
  cmp_value2 = 0xc0;

  while (len <= 6)
  {
    if (((int)(**src) & cmp_value1) == cmp_value2)
    {
      // Found a sequence of length len.

      cmp_value1 = 1;

      for (i=0; i<6-len; i++)
      {
        cmp_value1 <<= 1;
        cmp_value1 |= 1;
      }

      result = (z_ucs)(**src & cmp_value1);
      (*src)++;

      for (i=0; i<len-1; i++)
      {
        result <<= 6;
        result |= (**src & 0x3f);
        (*src)++;
      }

      return result;
    }

    cmp_value1 >>= 1;
    cmp_value1 |= 0x80;

    cmp_value2 >>= 1;
    cmp_value2 |= 0x80;

    len++;
  }

  return -1;
}


char *utf8_string_to_zucs_string(z_ucs *dest, char *src, int max_dest_size)
{
  if (max_dest_size < 1)
    return NULL;

  while (*src != (char)0)
  {
    if (max_dest_size == 1)
    {
      *dest = 0;
      return src;
    }

    *dest = utf8_char_to_zucs_char(&src);
    max_dest_size--;
    dest++;
  }

  *dest = 0;

  return NULL;
}


z_ucs *dup_utf8_string_to_zucs_string(char *src)
{
  int len = 0;
  char *ptr = src;
  z_ucs *result;

  while (*ptr != 0)
  {
    utf8_char_to_zucs_char(&ptr);
    len++;
  }
  len++;

  if ((result = malloc(sizeof(z_ucs) * len)) == NULL)
    return NULL;

  utf8_string_to_zucs_string(result, src, len);

  return result;
}


static int get_utf8_code_length(z_ucs zucs_char)
{
  if (     (zucs_char & 0xffffff80) == 0)
    return 1;
  else if ((zucs_char & 0xfffff800) == 0)
    return 2;
  else if ((zucs_char & 0xffff0000) == 0)
    return 3;
  else if ((zucs_char & 0xffe00000) == 0)
    return 4;
  else if ((zucs_char & 0xfc000000) == 0)
    return 5;
  else if ((zucs_char & 0x80000000) == 0)
    return 6;
  else
    return -1;
}


int zucs_string_to_utf8_string(char *dst, z_ucs **src, size_t max_dst_size)
{
  int len = 0;
  int char_len;
  int i;
  z_ucs mask1;
  z_ucs mask2;
  z_ucs src_val;

  if ( (max_dst_size < 1) && (dst != NULL) )
  {
    return -1;
  }

  while (**src != 0)
  {
    char_len = get_utf8_code_length(**src);
    if (char_len == -1)
    {
      printf("Illegal code: %c/%x\n", (char)**src, (unsigned)**src);
      printf("&0xfffff800: %x.\n", (unsigned)(**src & 0xfffff800));
      return -1;
    }

    if (dst != NULL)
    {
      if (max_dst_size < (size_t)(char_len + 1))
      {
        *dst = (char)0;
        return len + 1;
      }

      if (char_len == 1)
      {
        *dst = (char)**src;
        dst++;
      }
      else
      {
        mask1 = 0x01;
        mask2 = 0xfc;
        for (i=0; i<6-char_len; i++)
        {
          mask1 <<= 1;
          mask1 |= 1;
          mask2 <<= 1; 
        }
        mask2 &= 0xff;

        dst += char_len - 1;
        src_val = **src;

        for (i=0; i<char_len-1; i++)
        {
          *dst = (char)((src_val & 0x3f) | 0x80);
          src_val >>= 6;
          dst--;
        }

        *dst = (char)((src_val & mask1) | mask2);

        dst += char_len;
      }

      max_dst_size -= char_len;
    }

    (*src)++;
    len += char_len;
  }

  if (dst != NULL)
    *dst = (char)0;

  return len + 1;
}


char *dup_zucs_string_to_utf8_string(z_ucs *src)
{
  char *dst;
  z_ucs *src_ptr;
  size_t len = 0;

  src_ptr = src;
  while (*src_ptr != 0)
  {
    len += get_utf8_code_length(*src_ptr);
    src_ptr++;
  }

  if ((dst = malloc(len + 1)) == NULL)
    return NULL;

  src_ptr = src;
  if ((zucs_string_to_utf8_string(dst, &src_ptr, len + 1)) != (int)(len + 1))
  {
    free(dst);
    return NULL;
  }

  return dst;
}


