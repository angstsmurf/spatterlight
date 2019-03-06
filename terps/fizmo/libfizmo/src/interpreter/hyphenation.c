
/* hyphenation.c
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2010-2017 Christoph Ender.
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

// Doing TeX hyphenation; see The TeXbook, Appendix H

#ifndef hyphenation_c_INCLUDED
#define hyphenation_c_INCLUDED

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../tools/tracelog.h"
#include "../tools/z_ucs.h"
#include "../tools/list.h"
#include "../tools/i18n.h"
#include "../tools/filesys.h"
#include "../locales/libfizmo_locales.h"
#include "config.h"
#include "fizmo.h"
#include "hyphenation.h"


static z_ucs *last_pattern_locale = NULL;
static z_ucs *pattern_data;
static z_ucs **patterns;
static int nof_patterns = 0;
static z_ucs *search_path = NULL;
//static z_ucs *subword_buffer = NULL;
//static int subword_buffer_size = 0;
//static z_ucs *word_buf = NULL;
//static int word_buf_len = 0;

// From tools/i18n.c:
extern char default_search_path[];



static z_ucs input_char(z_file *in)
{
  z_ucs input;

  if ((input = parse_utf8_char_from_file(in)) == UEOF)
  {
    TRACE_LOG("Premature end of file.\n");
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_ERROR_WHILE_READING_FILE_P0S,
        -1,
        "<hyphenation>");
  }

  return (z_ucs)input;
}



// Returns > 0 in case s1 > s2, 0 if equal and < 0 otherwise.
static int cmp_pattern(z_ucs *s1, z_ucs *s2)
{
  while ( (*s1 != 0) || (*s2 != 0) )
  {
    if ( (*s1 != 0) && (isdigit(*s1) != 0) )
      s1++;

    if ( (*s2 != 0) && (isdigit(*s2) != 0) )
      s2++;

    if (*s1 != *s2)
      return *s2 - *s1;

    s1++;
    s2++;
  }

  return 0;
}


static int divide(int start, int end)
{
  int i=start, j=end;
  z_ucs *pattern;
  z_ucs *pivot = patterns[end];

  do
  {
    while ( (i < end) && (cmp_pattern(patterns[i], pivot) >= 0 ) )
    {
      //TRACE_LOG("i: %d\n", i);
      i++;
    }

    while ( (j >= start) && (cmp_pattern(patterns[j], pivot) <= 0 ) )
    {
      //TRACE_LOG("j: %d\n", j);
      j--;
    }

    if (i < j)
    {
      pattern = patterns[i];
      patterns[i] = patterns[j];
      patterns[j] = pattern;
    }
  }
  while (i < j);

  if (cmp_pattern(patterns[i], pivot) < 0)
  {
    pattern = patterns[i];
    patterns[i] = patterns[end];
    patterns[end] = pattern;
  }

  return i;
}


static void sort_patterndata(int start, int end)
{
  int div;

  if (start < end)
  {
    div = divide(start, end);
    sort_patterndata(start, div - 1);
    sort_patterndata(div + 1, end);
  }
}


static int load_patterns()
{
  list *lines;
  char *path_as_latin1;
  char *filename = "patterns.txt", *testfilename;
  z_ucs *colon_index, *current_locale;
  size_t bufsize = 0, len, filename_len, locale_len;
  z_ucs *zucs_buf = NULL, *filename_as_zucs;
  z_file *patternfile = NULL;
  size_t nof_zucs_chars;
  z_ucs *linestart;
  z_ucs input;
  int in_char;
  z_ucs *data;
  size_t nof_comments;
#ifdef ENABLE_TRACING
  int i;
#endif

  if (search_path == NULL)
  {
    if ((path_as_latin1 = get_configuration_value("i18n-search-path")) == NULL)
    {
      TRACE_LOG("Using default search path.\n");
      path_as_latin1 = default_search_path;
    }

    if ((search_path=dup_latin1_string_to_zucs_string(path_as_latin1)) == NULL)
      return -2;
  }

  current_locale = get_current_locale_name();
  if (last_pattern_locale != NULL)
    free(last_pattern_locale);
  last_pattern_locale = z_ucs_dup(current_locale);
  locale_len = z_ucs_len(current_locale);

  TRACE_LOG("pattern path: \"");
  TRACE_LOG_Z_UCS(search_path);
  TRACE_LOG("\".\n");

  filename_len = strlen(filename);
  filename_as_zucs = fizmo_malloc(sizeof(z_ucs) * strlen(filename) + 1);
  utf8_string_to_zucs_string(filename_as_zucs, filename, filename_len + 1);

  while (*search_path != 0)
  {
    colon_index = z_ucs_chr(search_path, Z_UCS_COLON);

    len
      = colon_index == NULL
      ? z_ucs_len(search_path)
      : (size_t)(colon_index - search_path);

    TRACE_LOG("len: %ld\n", (long)len);

    if (len > 0)
    {
      if (bufsize < len + filename_len + locale_len + 3)
      {
        TRACE_LOG("realloc buf for %ld bytes.\n",
            (long)sizeof(z_ucs) * (len + filename_len + locale_len + 3));

        // open-resource:
        if ((zucs_buf = realloc(zucs_buf,
                sizeof(z_ucs) * (len + filename_len + locale_len + 3))) == NULL)
        {
          // exit-point:
          TRACE_LOG("realloc() returned NULL.\n");
          free(filename_as_zucs);
          free(zucs_buf);
          return -5;
        }

        bufsize = len + filename_len + locale_len + 3;
      }

      z_ucs_ncpy(zucs_buf, search_path, len);
      zucs_buf[len] = Z_UCS_SLASH;
      z_ucs_cpy(zucs_buf + len + 1, current_locale);
      *(zucs_buf + len + locale_len + 1) = Z_UCS_SLASH;
      z_ucs_cpy(zucs_buf + len + locale_len + 2, filename_as_zucs);

      if ((testfilename = dup_zucs_string_to_utf8_string(zucs_buf)) == NULL)
      {
        free(zucs_buf);
        free(filename_as_zucs);
        return -3;
      }

      TRACE_LOG("Trying pattern file: \"%s\".\n", testfilename);

      patternfile = fsi->openfile(testfilename, FILETYPE_DATA, FILEACCESS_READ);
      free(testfilename);

      if (patternfile != NULL)
        break;
    }

    search_path += len + (colon_index != NULL ? 1 : 0);
  }

  free(zucs_buf);
  free(filename_as_zucs);

  if (patternfile != NULL)
  {
    TRACE_LOG("File found.\n");

    // We'll now measure the filesize in order to be able to allocate a
    // single block of memory before reading data. This is necessary, since
    // we're storing pattern indexes while reading and a realloc might
    // invalidate these.

    nof_zucs_chars = 0;
    for(;;)
    {
      // Parse line.
      input = parse_utf8_char_from_file(patternfile);
      if (input == UEOF)
        break;

      if (input == (z_ucs)'%')
      {
        do
        {
          input = parse_utf8_char_from_file(patternfile);
        }
        while (input != Z_UCS_NEWLINE);
      }
      else
      {
        nof_zucs_chars++;
        while (input != Z_UCS_NEWLINE)
        {
          nof_zucs_chars++;
          input = parse_utf8_char_from_file(patternfile);
        }
      }
    }

    if (fsi->setfilepos(patternfile, 0, SEEK_SET) == -1)
    {
      // exit-point:
      TRACE_LOG("setfilepos() returned -1.\n");
      fsi->closefile(patternfile);
      return -6;
    }

    TRACE_LOG("Allocating space for %ld z_ucs chars.\n", nof_zucs_chars);

    // open-resource:
    if ((data = malloc(nof_zucs_chars * sizeof(z_ucs))) == NULL)
    {
      // exit-point:
      TRACE_LOG("malloc(%ld) returned NULL.\n", nof_zucs_chars * sizeof(z_ucs));
      fsi->closefile(patternfile);
      return -7;
    }
    pattern_data = data;

    nof_comments = 0;
    lines = create_list();
    //printf("new list created: %p\n", lines);

    in_char = fsi->readchar(patternfile);
    while (in_char != EOF)
    {
      if (in_char == '%')
      {
        TRACE_LOG("Start comment.\n");
        do
        {
          input = input_char(patternfile);
        }
        while (input != Z_UCS_NEWLINE);
        nof_comments++;
      }
      else
      {
        // Found a new line.
        fsi->unreadchar(in_char, patternfile);

        linestart = data;

        TRACE_LOG("Start pattern.\n");
        for (;;)
        {
          input = input_char(patternfile);

          if (input == Z_UCS_NEWLINE)
          {
            *data ++ = 0;
            TRACE_LOG("New line at %p.\n", linestart);
            add_list_element(lines, (void*)linestart);
            break;
          }
          else
          {
            // Here we've found some "normal" output.
            *data++ = (z_ucs)input;
          }
        }

        //messages_processed++;
      }

      in_char = fsi->readchar(patternfile);
    }
    fsi->closefile(patternfile);
    nof_patterns = get_list_size(lines);
    patterns = (z_ucs**)delete_list_and_get_ptrs(lines);
    TRACE_LOG("Read %d patterns, %ld comments.\n", nof_patterns, nof_comments);

    sort_patterndata(0, nof_patterns - 1);

#ifdef ENABLE_TRACING
    TRACE_LOG("sorted:\n");

    for (i=0; i<nof_patterns; i++)
    {
      TRACE_LOG_Z_UCS(patterns[i]);
      TRACE_LOG("\n");
    }
#endif

    return 0;
  }
  else
  {
    nof_patterns = -1;
    return -1;
  }
}


/*
static int factorial(int n)
{
  return n == 1 ? 1: n * factorial(n - 1);
}
*/


static z_ucs *get_pattern(z_ucs *subword)
{
  int bottom = 0; // lowest not yet search element
  int top = nof_patterns-1; // highest not yet searched element
  int index, cmp;

  TRACE_LOG("Looking for pattern to subword: \"");
  TRACE_LOG_Z_UCS(subword);
  TRACE_LOG("\".\n");

  while (top - bottom >= 0)
  {
    //TRACE_LOG("start: bottom: %d, top: %d.\n", bottom, top);
    index = bottom + ( (top - bottom) / 2 );

    //TRACE_LOG("Compare (%d): \"", index);
    //TRACE_LOG_Z_UCS(patterns[index]);
    //TRACE_LOG("\".\n");

    if ((cmp = cmp_pattern(subword, patterns[index])) == 0)
      return patterns[index];
    else if (cmp > 0)
      top = index - 1;
    else bottom = index + 1;

    //TRACE_LOG("end : bottom: %d, top: %d.\n", bottom, top);
  }

  return NULL;
}


z_ucs *hyphenate(z_ucs *word_to_hyphenate)
{
  int i, j, k, l, start_offset, end_offset;
  int word_len, process_index;
  z_ucs *ptr;
  int word_to_hyphenate_len, score, max_score;
  z_ucs buf;
  z_ucs *word_buf, *result_buf, *result_ptr;

  if (
      (word_to_hyphenate == NULL)
      ||
      ((word_to_hyphenate_len = z_ucs_len(word_to_hyphenate)) < 1)
     )
  {
    TRACE_LOG("hyph input empty.\n");
    return NULL;
  }

  if (
      (last_pattern_locale == NULL)
      ||
      (z_ucs_cmp(last_pattern_locale, get_current_locale_name()) != 0)
     )
  {
    if (load_patterns() < 0)
    {
      TRACE_LOG("Couldn't load patterns.\n");
      return NULL;
    }
  }

  if ((result_buf = malloc(
          sizeof(z_ucs) * (word_to_hyphenate_len * 2 + 1))) == NULL)
    return NULL;

  if (z_ucs_len(word_to_hyphenate) < 4)
  {
    z_ucs_cpy(result_buf, word_to_hyphenate);
    return result_buf;
  }

  if ((word_buf = malloc(
          sizeof(z_ucs) * (word_to_hyphenate_len + 3))) == NULL)
  {
    free(result_buf);
    return NULL;
  }

  *word_buf = '.';
  z_ucs_cpy(word_buf + 1, word_to_hyphenate);
  word_buf[word_to_hyphenate_len+1] = '.';
  word_buf[word_to_hyphenate_len+2] = 0;
  word_len = word_to_hyphenate_len + 2;

  TRACE_LOG("Hyphenate: \"");
  TRACE_LOG_Z_UCS(word_buf);
  TRACE_LOG("\".\n");

  result_ptr = result_buf;

  *(result_ptr++) = *word_to_hyphenate;

  // Process all inter-letter positions. From the TeXbook, page 453:
  // "[...] except that plain TeX blocks hyphens after the very first
  // letter or before the last or second-last letter of a word." Thus,
  // we'll simply process entirely the same range here to avoid strange
  // hyphenations.
  for (i=1; i<word_to_hyphenate_len-2; i++)
  {
#ifdef ENABLE_TRACING
    buf = word_buf[i+2];
    word_buf[i+2] = 0;
    TRACE_LOG("Processing: \"");
    TRACE_LOG_Z_UCS(word_buf+i);
    TRACE_LOG("\".\n");
    word_buf[i+2] = buf;
#endif // ENABLE_TRACING

    start_offset = i;
    end_offset = i + 1;
    process_index = 1;
    max_score = 0;

    for (j=1; j<=word_len; j++)
    {
      if (end_offset > word_len - j)
        end_offset = word_len - j;

      TRACE_LOG("j: %d, start: %d, end: %d, pi: %d\n",
          j, start_offset, end_offset, process_index);

      for (k=start_offset; k<=end_offset; k++)
      {
        buf = word_buf[k + j];
        word_buf[k + j] = 0;

        if ((ptr = get_pattern(word_buf + k)) != NULL)
        {
          TRACE_LOG("Found (%d): \"", k);
          TRACE_LOG_Z_UCS(ptr);
          TRACE_LOG("\".\n");

          l = 0;
          while (l < process_index - (k - start_offset))
          {
            TRACE_LOG("l: %d, process_index: %d.\n", l, process_index);
            if (isdigit(*ptr) == 0)
              l++;
            ptr++;
          }

          score = isdigit(*ptr) ? *ptr - '0' : 0;
          if (score > max_score)
            max_score = score;
          TRACE_LOG("score (max: %d): %d.\n", max_score, score);
        }

        word_buf[k + j] = buf;
      }

      if (start_offset != 0)
        start_offset--;
      if (process_index <= i)
        process_index++;
    }

    TRACE_LOG("Finished for position %d (%c).\n", i, word_buf[i]);

    if (i > 1)
    {
      *(result_ptr++) = word_buf[i];
      if ((max_score & 1) != 0)
      {
        *(result_ptr++) = Z_UCS_SOFT_HYPEN;
        TRACE_LOG("Found hyph point.\n");
      }
    }
  }

  *(result_ptr++) = word_buf[i];
  *(result_ptr++) = word_buf[i+1];
  *(result_ptr++) = word_buf[i+2];
  *result_ptr = 0;

  TRACE_LOG("Result: \"");
  TRACE_LOG_Z_UCS(result_buf);
  TRACE_LOG("\".\n");

  free(word_buf);

  return result_buf;
}


void free_hyphenation_memory(void)
{
  if (last_pattern_locale != NULL)
  {
    free(last_pattern_locale);
    last_pattern_locale = NULL;
  }

  if (pattern_data != NULL)
  {
    free(pattern_data);
    pattern_data = NULL;
  }

  if (patterns != NULL)
  {
    free(patterns);
    patterns = NULL;
  }

  if (search_path == NULL)
  {
    free(search_path);
    search_path = NULL;
  }
}

#endif /* hyphenation_c_INCLUDED */

