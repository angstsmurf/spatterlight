
/* filelist.c
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


#include <string.h>
#include <ctype.h>
#include <inttypes.h>

#include "filelist.h"
#include "fizmo.h"
#include "babel.h"
#include "iff.h"
#include "config.h"
#include "../tools/tracelog.h"
#include "../tools/types.h"
#include "../tools/unused.h"
#include "../tools/filesys.h"

static int nof_files_found;
static int nof_files_searched;
static int nof_directories_searched;

static char *serial_input = NULL;
static int serial_input_size = 0;
static char *title_input = NULL;
static int title_input_size = 0;
static char *author_input = NULL;
static int author_input_size = 0;
static char *language_input = NULL;
static int language_input_size = 0;
static char *description_input = NULL;
static int description_input_size = 0;
static char *filename_input = NULL;
static int filename_input_size = 0;
static char *blorbfile_input = NULL;
static int blorbfile_input_size = 0;
static char *filetype_input = NULL;
static int filetype_input_size = 0;

static char *unquoted_serial_input = NULL;
static char *unquoted_title_input = NULL;
static char *unquoted_author_input = NULL;
static char *unquoted_language_input = NULL;
static char *unquoted_description_input = NULL;
static char *unquoted_filename_input = NULL;
static char *unquoted_blorbfile_input = NULL;
static char *unquoted_filetype_input = NULL;

static char filetype_raw[] = "raw";
static char filetype_zblorb[] = "zblorb";

static uint16_t release_input;
static long storyfile_timestamp_input;
static int length_input;
static uint16_t checksum_input;
static int version_input;
static z_file *in;
static bool in_zfile_open = false;
static int nof_files_searched;
static bool show_progress = false;


// 1,268,210 files in 243,656 directories.

void new_file_searched(char *filename, char *UNUSED(dirname))
{
  float percentage;

  if (filename != NULL)
  {
    nof_files_searched++;
    percentage = ((double)nof_files_searched / (double)nof_files_found) * 100;
    printf("\rUpdating story-list, scanning %d files, %3.0lf%% done.",
        nof_files_found, percentage);
    fflush(stdout);
    //printf("file:%s\n", filename);
  }
  else
  {
    nof_directories_searched++;
    //printf("%s (%d)\n", dirname, nof_directories_searched);
  }

  if ( (nof_files_searched != 0) && (nof_files_searched % 100 == 0) )
  {
    //printf("%d files.\n", nof_files);
  }
}


void free_unquote_buffers()
{
  if (unquoted_serial_input != NULL)
  {
    free(unquoted_serial_input);
    unquoted_serial_input = NULL;
  }
  if (unquoted_title_input != NULL)
  {
    free(unquoted_title_input);
    unquoted_title_input = NULL;
  }
  if (unquoted_author_input != NULL)
  {
    free(unquoted_author_input);
    unquoted_author_input = NULL;
  }
  if (unquoted_language_input != NULL)
  {
    free(unquoted_language_input);
    unquoted_language_input = NULL;
  }
  if (unquoted_description_input != NULL)
  {
    free(unquoted_description_input);
    unquoted_description_input = NULL;
  }
  if (unquoted_filename_input != NULL)
  {
    free(unquoted_filename_input);
    unquoted_filename_input = NULL;
  }
  if (unquoted_blorbfile_input != NULL)
  {
    free(unquoted_blorbfile_input);
    unquoted_blorbfile_input = NULL;
  }
  if (unquoted_filetype_input != NULL)
  {
    free(unquoted_filetype_input);
    unquoted_filetype_input = NULL;
  }
}


void abort_entry_input()
{
  if (serial_input != NULL)
  {
    free(serial_input);
    serial_input = NULL;
  }
  serial_input_size = 0;

  if (title_input != NULL)
  {
    free(title_input);
    title_input = NULL;
  }
  title_input_size = 0;

  if (author_input != NULL)
  {
    free(author_input);
    author_input = NULL;
  }
  author_input_size = 0;

  if (language_input != NULL)
  {
    free(language_input);
    language_input = NULL;
  }
  language_input_size = 0;

  if (description_input != NULL)
  {
    free(description_input);
    description_input = NULL;
  }
  description_input_size = 0;

  if (filename_input != NULL)
  {
    free(filename_input);
    filename_input = NULL;
  }
  filename_input_size = 0;

  if (blorbfile_input != NULL)
  {
    free(blorbfile_input);
    blorbfile_input = NULL;
  }
  blorbfile_input_size = 0;

  if (filetype_input != NULL)
  {
    free(filetype_input);
    filetype_input = NULL;
  }
  filetype_input_size = 0;

  free_unquote_buffers();

  if (in_zfile_open == true)
  {
    fsi->closefile(in);
    in_zfile_open = false;
  }
}


void free_z_story_list_entry(struct z_story_list_entry *entry)
{
  if (entry->serial != NULL)
    free(entry->serial);
  if (entry->title != NULL)
    free(entry->title);
  if (entry->author != NULL)
    free(entry->author);
  if (entry->language != NULL)
    free(entry->language);
  if (entry->description != NULL)
    free(entry->description);
  if (entry->filename != NULL)
    free(entry->filename);
  if (entry->blorbfile != NULL)
    free(entry->blorbfile);
  if (entry->filetype != NULL)
    free(entry->filetype);
  free(entry);
}


void free_z_story_list(struct z_story_list *story_list)
{
  if (story_list == NULL)
    return;

  while (story_list->nof_entries > 0)
  {
    free_z_story_list_entry(story_list->entries[story_list->nof_entries - 1]);
    story_list->nof_entries--;
  }

  free(story_list->entries);
  free(story_list);
}


int parse_next_story_entry()
{
  int index;
  int data;
  long offset;
  long size;

  index = 0;
  release_input = 0;
  while (index < 5)
  {
    if ((data = fsi->readchar(in)) == EOF)
    {
      if (index == 0) break;
      else { abort_entry_input(); TRACE_LOG("#0\n"); return -1; }
    }
    if (data == '\t') break;
    if (isdigit(data) == 0) { break; }
    release_input *= 10;
    release_input += (data - '0');
    index++;
  }

  if ( (index == 5) && (fsi->readchar(in) != '\t') )
  { abort_entry_input(); TRACE_LOG("#1\n"); return -1; }

  offset = fsi->getfilepos(in);
  while ((data = fsi->readchar(in)) != '\t')
    if (data == EOF)
    { abort_entry_input(); TRACE_LOG("#2\n"); return -1; }
  size = fsi->getfilepos(in) - offset - 1;
  if (ensure_mem_size(&serial_input, &serial_input_size, size + 2) == -1)
  { abort_entry_input(); TRACE_LOG("#3\n"); return -1; }
  if (size > 0)
  {
    fsi->setfilepos(in, -(size+1), SEEK_CUR);
    if (fsi->readchars(serial_input, size+1, in) != (size_t)size+1)
    { abort_entry_input(); TRACE_LOG("#4\n"); return -1; }
  }
  serial_input[size] = '\0';
  unquoted_serial_input = unquote_special_chars(serial_input);

  index = 0;
  length_input = 0;
  do
  {
    if ((data = fsi->readchar(in)) == EOF)
    { abort_entry_input(); TRACE_LOG("#5\n"); return -1; }
    if (data != '\t')
    {
      if (isdigit(data) == 0) { break; }
      length_input *= 10;
      length_input += (data - '0');
    }
  }
  while (data != '\t');

  index = 0;
  checksum_input = 0;
  while (index < 5)
  {
    if ((data = fsi->readchar(in)) == EOF)
    { abort_entry_input(); TRACE_LOG("#6\n"); return -1; }
    if (data == '\t') break;
    if (isdigit(data) == 0) { TRACE_LOG("#7\n"); return -1; }
    checksum_input *= 10;
    checksum_input += (data - '0');
    index++;
  }
  TRACE_LOG("cs:%d\n", checksum_input);

  if ( (index == 5) && (fsi->readchar(in) != '\t') )
  { abort_entry_input(); TRACE_LOG("#8\n"); return -1; }

  if ((data = fsi->readchar(in)) == EOF)
  { abort_entry_input(); TRACE_LOG("#9\n"); return -1; }
  TRACE_LOG("data:%c\n", data);
  if (isdigit(data) == 0)
  { abort_entry_input(); TRACE_LOG("#10\n"); return -1; }
  version_input = data - '0';
  TRACE_LOG("versioninput:%c\n", version_input);

  if (fsi->readchar(in) != '\t')
  { abort_entry_input(); TRACE_LOG("#11\n"); return -1; }

  offset = fsi->getfilepos(in);
  while ((data = fsi->readchar(in)) != '\t')
    if (data == EOF)
    { abort_entry_input(); TRACE_LOG("#12\n"); return -1; }
  size = fsi->getfilepos(in) - offset - 1;
  if (ensure_mem_size(&title_input, &title_input_size, size + 2) == -1)
  { abort_entry_input(); TRACE_LOG("#13\n"); return -1; }
  if (size > 0)
  {
    fsi->setfilepos(in, -(size+1), SEEK_CUR);
    if (fsi->readchars(title_input, size+1, in) != (size_t)size+1)
    { abort_entry_input(); TRACE_LOG("#14\n"); return -1; }
  }
  title_input[size] = '\0';
  unquoted_title_input = unquote_special_chars(title_input);
  //printf("title:[%s]\n", title_input);

  offset = fsi->getfilepos(in);
  while ((data = fsi->readchar(in)) != '\t')
    if (data == EOF) { abort_entry_input(); TRACE_LOG("#15\n"); return -1; }
  size = fsi->getfilepos(in) - offset - 1;
  if (ensure_mem_size(&author_input, &author_input_size, size + 2) == -1)
  { abort_entry_input(); TRACE_LOG("#16\n"); return -1; }
  if (size > 0)
  {
    fsi->setfilepos(in, -(size+1), SEEK_CUR);
    if (fsi->readchars(author_input, size+1, in) != (size_t)size+1)
    { abort_entry_input(); TRACE_LOG("#17\n"); return -1; }
  }
  author_input[size] = '\0';
  unquoted_author_input = unquote_special_chars(author_input);
  //printf("author:[%s]\n", author_input);

  offset = fsi->getfilepos(in);
  while ((data = fsi->readchar(in)) != '\t')
    if (data == EOF) { abort_entry_input(); TRACE_LOG("#15\n"); return -1; }
  size = fsi->getfilepos(in) - offset - 1;
  if (ensure_mem_size(&language_input, &language_input_size, size + 2) == -1)
  { abort_entry_input(); TRACE_LOG("#16\n"); return -1; }
  if (size > 0)
  {
    fsi->setfilepos(in, -(size+1), SEEK_CUR);
    if (fsi->readchars(language_input, size+1, in) != (size_t)size+1)
    { abort_entry_input(); TRACE_LOG("#17\n"); return -1; }
  }
  language_input[size] = '\0';
  unquoted_language_input = unquote_special_chars(language_input);
  //printf("language:[%s]\n", language_input);

  offset = fsi->getfilepos(in);
  while ((data = fsi->readchar(in)) != '\t')
    if (data == EOF)
    { abort_entry_input(); TRACE_LOG("#18\n"); return -1; }
  size = fsi->getfilepos(in) - offset - 1;
  if (ensure_mem_size(&description_input, &description_input_size, size+2)==-1)
  { abort_entry_input(); TRACE_LOG("#19\n"); return -1; }
  if (size > 0)
  {
    fsi->setfilepos(in, -(size+1), SEEK_CUR);
    if (fsi->readchars(description_input, size+1, in) != (size_t)size+1)
    { abort_entry_input(); TRACE_LOG("#20\n"); return -1; }
  }
  description_input[size] = '\0';
  unquoted_description_input = unquote_special_chars(description_input);
  //printf("desc:[%s]\n", description_input);

  offset = fsi->getfilepos(in);
  while ((data = fsi->readchar(in)) != '\t')
    if (data == EOF)
    { abort_entry_input(); TRACE_LOG("#21\n"); return -1; }
  size = fsi->getfilepos(in) - offset - 1;
  if (ensure_mem_size(&filename_input, &filename_input_size, size + 2) == -1)
  { abort_entry_input(); TRACE_LOG("#22\n"); return -1; }
  if (size > 0)
  {
    fsi->setfilepos(in, -(size+1), SEEK_CUR);
    if (fsi->readchars(filename_input, size+1, in) != (size_t)size+1)
    { abort_entry_input(); TRACE_LOG("#23\n"); return -1; }
  }
  filename_input[size] = '\0';
  unquoted_filename_input = unquote_special_chars(filename_input);

  offset = fsi->getfilepos(in);
  while ((data = fsi->readchar(in)) != '\t')
    if (data == EOF)
    { abort_entry_input(); TRACE_LOG("#24\n"); return -1; }
  size = fsi->getfilepos(in) - offset - 1;
  if (ensure_mem_size(&blorbfile_input, &blorbfile_input_size, size + 2) == -1)
  { abort_entry_input(); TRACE_LOG("#25\n"); return -1; }
  if (size > 0)
  {
    fsi->setfilepos(in, -(size+1), SEEK_CUR);
    if (fsi->readchars(blorbfile_input, size+1, in) != (size_t)size+1)
    { abort_entry_input(); TRACE_LOG("#26\n"); return -1; }
  }
  blorbfile_input[size] = '\0';
  unquoted_blorbfile_input = unquote_special_chars(blorbfile_input);

  offset = fsi->getfilepos(in);
  while ((data = fsi->readchar(in)) != '\t')
    if (data == EOF)
    { abort_entry_input(); TRACE_LOG("#27\n"); return -1; }
  size = fsi->getfilepos(in) - offset - 1;
  if (ensure_mem_size(&filetype_input, &filetype_input_size, size + 2) == -1)
  { abort_entry_input(); TRACE_LOG("#28\n"); return -1; }
  if (size > 0)
  {
    fsi->setfilepos(in, -(size+1), SEEK_CUR);
    if (fsi->readchars(filetype_input, size+1, in) != (size_t)size+1)
    { abort_entry_input(); TRACE_LOG("#29\n"); return -1; }
  }
  filetype_input[size] = '\0';
  unquoted_filetype_input = unquote_special_chars(filetype_input);

  index = 0;
  storyfile_timestamp_input = 0;
  while (index < 16)
  {
    if ((data = fsi->readchar(in)) == EOF)
    { abort_entry_input(); TRACE_LOG("#30\n"); return -1; }
    if (data == '\n')
    { fsi->unreadchar(data, in); break; }
    if (isdigit(data) == 0) { break; }
    storyfile_timestamp_input *= 10;
    storyfile_timestamp_input += (data - '0');
    index++;
  }
  //printf("filename:[%s]\n", filename_input);

  /*
  printf("%d|%s|%d|%s|%s|%s|%s\n", release_input, serial_input,
      checksum_input, title_input, author_input, description_input,
      filename_input);
  */

  while ((data = fsi->readchar(in)) != '\n')
    if (data == EOF) break;

  return 0;
}


static char *get_filelist_name()
{
  char *dir_name = NULL;
  char *filename;

#ifndef DISABLE_CONFIGFILES
  dir_name = get_fizmo_config_dir_name();
#endif // DISABLE_CONFIGFILES

  if (dir_name == NULL)
    return NULL;

  filename = malloc(strlen(dir_name) + 16);
  strcpy(filename, dir_name);
  strcat(filename, "/story-list.txt");

  return filename;
}


static z_file *open_story_list(bool write_enabled)
{
  char *filename;
  z_file *result;

  if ((filename = get_filelist_name()) == NULL)
    return NULL;

  result = fsi->openfile(filename, FILETYPE_DATA,
      write_enabled == true ? FILEACCESS_WRITE : FILEACCESS_READ);
  free(filename);
  return result;
}


struct z_story_list_entry *store_current_entry()
{
  struct z_story_list_entry *result;

  if ((result = malloc(sizeof(struct z_story_list_entry))) == NULL)
    return NULL;

  //printf("storing %s, %d, %d\n", serial_input, release_input, length_input);

  result->release_number = release_input;
  result->serial = fizmo_strdup(unquoted_serial_input);
  result->z_code_version = version_input;
  result->length = length_input;
  result->checksum = checksum_input;
  result->title = fizmo_strdup(unquoted_title_input);
  result->author = fizmo_strdup(unquoted_author_input);
  result->language = fizmo_strdup(unquoted_language_input);
  result->description = fizmo_strdup(unquoted_description_input);
  result->filename = fizmo_strdup(unquoted_filename_input);
  result->blorbfile = fizmo_strdup(unquoted_blorbfile_input);
  result->filetype = fizmo_strdup(unquoted_filetype_input);
  //result->entry_line_index = line_index;
  result->storyfile_timestamp = storyfile_timestamp_input;

  return result;
}


struct z_story_list_entry *add_entry_to_story_list(
    struct z_story_list *story_list, char *title, char *author, char *language,
    char *description, char *serial, int version, int length,
    uint16_t checksum, uint16_t release, char *story_filename,
    char *story_blorbfile, char *story_filetype, long storyfile_timestamp)
//, int line_index)
{
  void *ptr;
  struct z_story_list_entry *result;
  int insert_index;

  //printf("Adding %d:%s\n", story_list->nof_entries, story_filename);

  TRACE_LOG("Adding new story entry #%d: %s, \"%s\".\n",
      story_list->nof_entries,
      story_filename,
      description != NULL ? description : "");

  if (story_list->nof_entries == story_list->nof_entries_allocated)
  {
    if ((ptr = (struct z_story_list_entry**)fizmo_realloc(
            story_list->entries,
            sizeof(struct z_story_list_entry*)
            * (story_list->nof_entries_allocated + 10))) == NULL)
      {
        TRACE_LOG("Cannot realloc.\n");
        return NULL;
      }

    story_list->entries = ptr;
    story_list->nof_entries_allocated += 10;
  }

  if ((result = malloc(sizeof(struct z_story_list_entry))) == NULL)
  {
    TRACE_LOG("Cannot malloc.\n");
    return NULL;
  }

  result->release_number = release;
  result->serial = fizmo_strdup(serial);
  result->z_code_version = version;
  result->length = length;
  result->checksum = checksum;
  result->title = fizmo_strdup(title);
  result->author = fizmo_strdup(author);
  result->language = fizmo_strdup(language);
  result->description = fizmo_strdup(description);
  result->filename = fizmo_strdup(story_filename);
  result->blorbfile
    = (story_blorbfile != NULL ? fizmo_strdup(story_blorbfile) : NULL);
  result->filetype = fizmo_strdup(story_filetype);
  result->storyfile_timestamp = storyfile_timestamp;

  insert_index = 0;
  while (insert_index < story_list->nof_entries)
  {
    if (strcmp(result->title, story_list->entries[insert_index]->title) < 0)
      break;
    insert_index++;
  }
  TRACE_LOG("Insert index: %d.\n", insert_index);

  if (insert_index < story_list->nof_entries)
  {
    TRACE_LOG("Move to %p from %p.\n",
        story_list->entries + insert_index + 1,
        story_list->entries + insert_index);

    memmove(
        story_list->entries + insert_index + 1,
        story_list->entries + insert_index,
        (story_list->nof_entries - insert_index)
        * sizeof(struct z_story_list_entry*));
  }

  story_list->entries[insert_index] = result;
  story_list->nof_entries++;

  return result;
}


struct z_story_list *get_empty_z_story_list()
{
  struct z_story_list *result;

  if ((result = malloc(sizeof(struct z_story_list))) == NULL)
    return NULL;

  result->nof_entries_allocated = 0;
  result->nof_entries = 0;
  result->entries = NULL;

  return result;
}


struct z_story_list *get_z_story_list()
{
  int data;
  struct z_story_list *result = get_empty_z_story_list();

  if ((in = open_story_list(false)) == NULL)
    return result;
  in_zfile_open = true;

  for(;;)
  {
    if ((data = fsi->readchar(in)) == EOF)
    {
      abort_entry_input();
      return result;
    }
    else
      fsi->unreadchar(data, in);

    if (parse_next_story_entry() == -1)
    {
      abort_entry_input();
      return result;
    }

    add_entry_to_story_list(
        result,
        unquoted_title_input,
        unquoted_author_input,
        unquoted_language_input,
        unquoted_description_input,
        unquoted_serial_input,
        version_input,
        length_input,
        checksum_input,
        release_input,
        unquoted_filename_input,
        unquoted_blorbfile_input,
        unquoted_filetype_input,
        storyfile_timestamp_input);
  }
}


 // This method reads story info from a parsed list. For other purposes, see
// function "get_z_story_entry_from_list".
static struct z_story_list_entry *get_z_story_entry(char *serial,
    uint16_t release, int length, struct z_story_list *story_list)
{
  struct z_story_list_entry *entry;
  int i = 0;

  if (story_list == NULL)
    return NULL;

  //printf("looking for: %s, %d, %d\n", serial, release, length);

  while (i < story_list->nof_entries)
  {
    entry = story_list->entries[i];
    /*
    printf("cmp(%d): %s/%s, %d/%d, %d/%d\n", i,entry->serial, serial,
        entry->release_number, release,
        entry->length, length);
    */

    if (
        (strcmp(entry->serial, serial) == 0)
        &&
        (entry->release_number == release)
        &&
        (entry->length == length)
       )
    {
      //printf("Found\n");
      return entry;
    }

    i++;
  }

  return NULL;
}


// This method will parse entries in the story-list until a matching one
// has been found. It will discard all non-matches and return only a single
// entry, making it useful for retrieving data about a single story when
// the list is not needed. To get an entry from an already parsed list, see
// "get_z_story_entry" instead.
struct z_story_list_entry *get_z_story_entry_from_list(char *serial,
    uint16_t release, uint16_t checksum)
{
  struct z_story_list_entry *result;
  int data;

  if ((in = open_story_list(false)) == NULL)
    return NULL;
  in_zfile_open = true;

  for(;;)
  {
    if ((data = fsi->readchar(in)) == EOF)
    {
      abort_entry_input();
      return NULL;
    }
    else
      fsi->unreadchar(data, in);

    if (parse_next_story_entry() == -1)
    {
      abort_entry_input();
      return NULL;
    }

    if (
        (strcmp(serial_input, serial) == 0)
        && (release_input == release)
        && (checksum_input == checksum) )
    {
      result = store_current_entry(); //line_index);
      abort_entry_input();
      return result;
    }
  }
}


int remove_entry_from_list(struct z_story_list *story_list,
    struct z_story_list_entry *entry)
{
  int entry_index = 0;

  while (entry_index < story_list->nof_entries)
  {
    if (story_list->entries[entry_index] == entry)
      break;
    entry_index++;
  }

  if (entry_index >= story_list->nof_entries)
    return -1;

  if (entry_index + 1 < story_list->nof_entries)
  {
    memmove(
        story_list->entries + entry_index,
        story_list->entries + entry_index + 1,
        sizeof(struct z_story_list_entry*)
        * (story_list->nof_entries - entry_index - 1));
  }

  story_list->nof_entries--;

  return 0;
}


static int detect_and_add_z_file(char *filename, char *blorb_filename,
    struct babel_info *babel, struct z_story_list *story_list)
{
  z_file *infile;
  uint8_t buf[30];
  uint32_t val;
  char serial[7];
  int version;
  uint16_t checksum;
  uint16_t release;
  struct babel_story_info *b_info = NULL;
  char *title;
  char *author;
  char *language;
  char *description;
  char *ptr, *ptr2;
  int length;
  time_t storyfile_timestamp;
  char *empty_string = "";
  struct z_story_list_entry *entry;
  int chunk_length = -1;
  struct babel_info *file_babel = NULL;
  bool file_is_zblorb;
  char *cwd = NULL;
  char *abs_filename = NULL;

  if (filename == NULL)
    return -1;

  if (filename[0] != '/')
  {
    cwd = fsi->get_cwd();
    abs_filename = fizmo_malloc(strlen(cwd) + strlen(filename) + 2);
    sprintf(abs_filename, "%s/%s", cwd, filename);
  }
  else
    abs_filename = filename;

  if ((infile = fsi->openfile(abs_filename, FILETYPE_DATA, FILEACCESS_READ))
      == NULL)
  {
    if (cwd != NULL)
    {
      free(cwd);
      free(abs_filename);
    }
    return -1;
  }

  if ((storyfile_timestamp = fsi->get_last_file_mod_timestamp(infile)) < 0)
  {
    fsi->closefile(infile);
    if (cwd != NULL)
    {
      free(cwd);
      free(abs_filename);
    }
    return -1;
  }

  if (fsi->readchars(buf, 30, infile) != 30)
  {
    fsi->closefile(infile);
    if (cwd != NULL)
    {
      free(cwd);
      free(abs_filename);
    }
    return -1;
  }

  if (memcmp(buf, "FORM", 4) == 0)
  {
    // IFF file.

    if (
        (is_form_type(infile, "IFRS") != true)
        ||
        (find_chunk("ZCOD", infile) == -1)
       )
    {
      fsi->closefile(infile);
      if (cwd != NULL)
      {
        free(cwd);
        free(abs_filename);
      }
      return -1;
    }

    file_is_zblorb = true;

    if (find_chunk("IFmd", infile) == 0)
    {
      read_chunk_length(infile);
      chunk_length = get_last_chunk_length();
      file_babel = load_babel_info_from_blorb(
          infile, chunk_length, abs_filename, storyfile_timestamp);
      babel = file_babel;
    }

    find_chunk("ZCOD", infile);
    read_chunk_length(infile);
    length = get_last_chunk_length();

    if (fsi->readchars(buf, 30, infile) != 30)
    {
      fsi->closefile(infile);
      if (cwd != NULL)
      {
        free(cwd);
        free(abs_filename);
      }
      return -1;
    }
  }
  else
  {
    fsi->setfilepos(infile, 0, SEEK_END);
    length = fsi->getfilepos(infile);
    file_is_zblorb = false;
  }
  fsi->closefile(infile);

  val = (buf[16] << 24) | (buf[17] << 16) | (buf[18] << 8) | (buf[19]);
  if (
      ((val & 0xbe00f0f0) != 0x3030)
      ||
      (*buf < 1)
      ||
      (*buf > 8)
     )
  {
    if (cwd != NULL)
    {
      free(cwd);
      free(abs_filename);
    }
    return -2;
  }

  version = *buf;
  memcpy(serial, buf + 0x12, 6);
  serial[6] = '\0';
  checksum = (buf[0x1c] << 8) | buf[0x1d];
  release = (buf[2] << 8) | buf[3];

  if ((entry = get_z_story_entry(serial, release, length, story_list))
      != NULL)
  {
    // We already have the story in our story-list. If we have a raw file
    // we can just quit if the support-blorbfilename is the same (raw files
    // don't contain metadata which might have changed).
    if (
        (file_is_zblorb == false)
        &&
        (
         ( (entry->blorbfile == NULL) && (blorb_filename != NULL) )
         // ||  (Don't delete blorb file)
         // ( (blorb_filename == NULL) && (entry->blorbfile != NULL) )
         ||
         (
          (entry->blorbfile != NULL)
          &&
          (blorb_filename != NULL)
          &&
          (strcmp(blorb_filename, entry->blorbfile) == 0)
         )
        )
       )
    {
      if (cwd != NULL)
      {
        free(cwd);
        free(abs_filename);
      }
      return -3;
    }

    //printf("%ld / %ld\n", storyfile_timestamp, entry->storyfile_timestamp);

    // In case new file is a zblorb and we have save a raw file, remove the
    // raw and keep the blorb (so we can get images and sound). We'll also
    // re-read the file contents if the file has changed (metadata might
    // have been altered).
    if (
        (strcmp(entry->filetype, filetype_raw) == 0)
        ||
        (storyfile_timestamp > entry->storyfile_timestamp)
       )
    {
      remove_entry_from_list(story_list, entry);
      //printf("%s...\n", abs_filename);
    }
    else
    {
      if (cwd != NULL)
      {
        free(cwd);
        free(abs_filename);
      }
      return -4;
    }
  }

  ptr2 = NULL;

  if ((b_info = get_babel_story_info(
          release, serial, checksum, babel, file_is_zblorb)) != NULL)
  {
    title = (b_info->title == NULL ? empty_string : b_info->title);
    author = (b_info->author == NULL ? empty_string : b_info->author);
    language = (b_info->language == NULL ? empty_string : b_info->language);
    description
      = (b_info->description != NULL)
      ? b_info->description
      : empty_string;
  }
  else
  {
    if ((title = strrchr(abs_filename, '/')) == NULL)
      title = abs_filename;
    else
      title++;

    if ((ptr = strrchr(title, '.')) != NULL)
    {
      TRACE_LOG("strdup: %s\n", title);
      ptr2 = fizmo_strdup(title);
      ptr = strrchr(ptr2, '.');

      if ( ( (strlen(ptr) == 3) && (ptr[1] == 'z') && (isdigit(ptr[2]) != 0) )
          ||
          (strcasecmp(ptr, ".dat") == 0)
          ||
          (strcasecmp(ptr, ".zblorb") == 0)
         )
        *ptr = '\0';

      *ptr2 = toupper(*ptr2);

      title = ptr2;
    }

    author = empty_string;
    language = empty_string;
    description = empty_string;
  }

  add_entry_to_story_list(
      story_list,
      title,
      author,
      language,
      description,
      serial,
      version,
      length,
      checksum,
      release,
      abs_filename,
      file_is_zblorb ? NULL : blorb_filename,
      file_is_zblorb ? filetype_zblorb : filetype_raw,
      storyfile_timestamp);

  if (b_info != NULL)
    free_babel_story_info(b_info);

  if (ptr2 != NULL)
    free(ptr2);

  if (file_babel != NULL)
    free_babel_info(file_babel);

  if (cwd != NULL)
  {
    free(cwd);
    free(abs_filename);
  }

  return 0;
}


static int count_files(char *abs_dir_name, bool recursive)
{
  z_dir *current_dir;
  struct z_dir_ent z_dir_entry;
  char *dirname = NULL;
  int dirname_size = 0;
  int len;
  int result = 0;
  char *cwd = fsi->get_cwd(NULL, 0);

  if ((fsi->ch_dir(abs_dir_name)) == -1)
  {
    free(cwd);
    return 0;
  }

  TRACE_LOG("Counting files for \"%s\".\n", abs_dir_name);
  if ((current_dir = fsi->open_dir(".")) == NULL)
  {
    printf("\"%s\":\n", abs_dir_name);
    perror("could not opendir");
    fsi->ch_dir(cwd);
    free(cwd);
    return 0;
  }

  while (fsi->read_dir(&z_dir_entry, current_dir) == 0)
  {
    if (
        (strcmp(z_dir_entry.d_name, ".") == 0)
        ||
        (strcmp(z_dir_entry.d_name, "..") == 0)
       )
      continue;

    len = strlen(abs_dir_name) + strlen(z_dir_entry.d_name) + 2;
    if (len > dirname_size)
    {
      dirname = (char*)fizmo_realloc(dirname, len);
      dirname_size = len;
    }

    strcpy(dirname, abs_dir_name);
    if (dirname[strlen(dirname) - 1] != '/')
      strcat(dirname, "/");
    strcat(dirname, z_dir_entry.d_name);

    if (fsi->is_filename_directory(z_dir_entry.d_name) == true)
    {
      if (recursive == true)
        result += count_files(dirname, true);
    }
    else
      result++;
  }

  if (dirname != NULL)
    free(dirname);

  fsi->close_dir(current_dir);

  fsi->ch_dir(cwd);
  free(cwd);

  //printf("result:%d\n", result);

  TRACE_LOG("count-result: %d\n", result);

  return result;
}


static int search_dir(char *abs_dir_name,
    void (*update_func)(char *filename, char *dirname),
    struct z_story_list *story_list, bool recursive, struct babel_info *babel)
{
  z_dir *current_dir;
  struct z_dir_ent z_dir_entry;
  char *dirname = NULL;
  int dirname_size = 0;
  int len;
  char *cwd = fsi->get_cwd();

  TRACE_LOG("Trying to readdir \"%s\".\n", abs_dir_name);

  if ((fsi->ch_dir(abs_dir_name)) == -1)
  {
    return -1;
  }

  if ((current_dir = fsi->open_dir(".")) == NULL)
  {
    printf("\"%s\":\n", abs_dir_name);
    perror("could not opendir");
    return -1;
  }

  if ( (show_progress == true) && (update_func != NULL) )
    update_func(NULL, abs_dir_name);

  while (fsi->read_dir(&z_dir_entry, current_dir) == 0)
  {
    if (
        (strcmp(z_dir_entry.d_name, ".") == 0)
        ||
        (strcmp(z_dir_entry.d_name, "..") == 0)
       )
      continue;

    len = strlen(abs_dir_name) + strlen(z_dir_entry.d_name) + 2;
    if (len > dirname_size)
    {
      dirname = (char*)fizmo_realloc(dirname, len);
      dirname_size = len;
    }

    strcpy(dirname, abs_dir_name);
    if (dirname[strlen(dirname) - 1] != '/')
      strcat(dirname, "/");
    strcat(dirname, z_dir_entry.d_name);

    if (fsi->is_filename_directory(z_dir_entry.d_name) == true)
    {
      if (recursive == true)
        search_dir(dirname, update_func, story_list, true, babel);
    }
    else
    {
      if ( (show_progress == true) && (update_func != NULL) )
        update_func(z_dir_entry.d_name, NULL);
      detect_and_add_z_file(dirname, NULL, babel, story_list);
      }
  }

  if (dirname != NULL)
    free(dirname);

  fsi->close_dir(current_dir);

  fsi->ch_dir(cwd);
  free(cwd);

  return 0;
}


void build_filelist(char *root_dir, struct z_story_list *story_list,
    bool recursive, struct babel_info *babel)
{
  char *cwd = fsi->get_cwd();
  char *absrootdir;

  if (root_dir == NULL)
  {
    TRACE_LOG("Building filelist for rootdir: \"/\".\n");
    search_dir("/", &new_file_searched, story_list, recursive, babel);
  }
  else
  {
    TRACE_LOG("Building filelist for rootdir: \"%s\".\n", root_dir);

    if ((fsi->ch_dir(root_dir)) == -1)
      detect_and_add_z_file(root_dir, NULL, babel, story_list);
    else
    {
      // Avoid relative names like "./zork1.z3".
      absrootdir = fsi->get_cwd(NULL, 0);
      search_dir(absrootdir, &new_file_searched, story_list, recursive, babel);
      free(absrootdir);
    }
  }

  fsi->ch_dir(cwd);
  free(cwd);
}


void save_story_list(struct z_story_list *story_list)
{
  z_file *out;
  struct z_story_list_entry *entry;
  int i = 0;
  char *quoted_serial = NULL;
  char *quoted_title = NULL;
  char *quoted_author = NULL;
  char *quoted_language = NULL;
  char *quoted_description = NULL;
  char *quoted_filename = NULL;
  char *quoted_blorbname = NULL;
  char *quoted_filetype= NULL;

  if ((out = open_story_list(true)) == NULL)
    return;

  while (i < story_list->nof_entries)
  {
    entry = story_list->entries[i++];

    quoted_serial = quote_special_chars(entry->serial);
    quoted_title = quote_special_chars(entry->title);
    quoted_author = quote_special_chars(entry->author);
    quoted_language = quote_special_chars(entry->language);
    quoted_description = quote_special_chars(entry->description);
    quoted_filename = quote_special_chars(entry->filename);
    quoted_blorbname = quote_special_chars(entry->blorbfile);
    quoted_filetype = quote_special_chars(entry->filetype);

    //printf("%d/%d\n", i, story_list->nof_entries);

    fsi->fileprintf(
        out,
        "%d\t%s\t%d\t%d\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%ld\n",
        entry->release_number,
        quoted_serial,
        entry->length,
        entry->checksum,
        entry->z_code_version,
        (quoted_title == NULL ? "" : quoted_title),
        (quoted_author == NULL ? "" : quoted_author),
        (quoted_language == NULL ? "" : quoted_language),
        (quoted_description == NULL ? "" : quoted_description),
        quoted_filename,
        (quoted_blorbname == NULL ? "" : quoted_blorbname),
        quoted_filetype,
        entry->storyfile_timestamp);

    if (quoted_serial != NULL)
      free(quoted_serial);

    if (quoted_title != NULL)
      free(quoted_title);

    if (quoted_author != NULL)
      free(quoted_author);

    if (quoted_language != NULL)
      free(quoted_language);

    if (quoted_description != NULL)
      free(quoted_description);

    if (quoted_filename != NULL)
      free(quoted_filename);

    if (quoted_blorbname != NULL)
      free(quoted_blorbname);

    if (quoted_filetype != NULL)
      free(quoted_filetype);
  }

  fsi->closefile(out);
}


struct z_story_list *update_fizmo_story_list()
{
#ifdef DISABLE_CONFIGFILES
  return NULL;
#else // DISABLE_CONFIGFILES
  struct z_story_list *result;
  struct z_story_list_entry *entry;
  char *str, *str_copy, *path;
  z_file *file;
  struct babel_info *babel;
  int i;

  nof_files_found = 0;
  nof_files_searched = 0;
  nof_directories_searched = 0;

  ensure_dot_fizmo_dir_exists();

  babel = load_babel_info();

  if (babel_files_have_changed(babel) == true)
  {
    // Don't load current list of story, rebuild index with the newly
    // changed babel data.
    result = get_empty_z_story_list();
    store_babel_info_timestamps(babel);
  }
  else
  {
    // Babel data is the same, use pre-indexed story list.
    result = get_z_story_list();
    i = 0;
    while (i < result->nof_entries)
    {
      entry = result->entries[i];
      if ((file = fsi->openfile(
              entry->filename, FILETYPE_DATA, FILEACCESS_READ)) == NULL)
        remove_entry_from_list(result, entry);
      else
      {
        fsi->closefile(file);
        i++;
      }
    }
  }

  if ((str = getenv("ZCODE_PATH")) == NULL)
    str = getenv("INFOCOM_PATH");
  if (str != NULL)
    set_configuration_value("z-code-path", str);

  if ((str = getenv("ZCODE_ROOT_PATH")) != NULL)
    set_configuration_value("z-code-root-path", str);

  if ((str = get_configuration_value("z-code-path")) != NULL)
  {
    path = strtok(str, ":");
    while (path != NULL)
    {
      TRACE_LOG("Counting for token \"%s\".\n", path);
      nof_files_found += count_files(path, false);
      path = strtok(NULL, ":");
    }
  }

  if ((str = get_configuration_value("z-code-root-path")) != NULL)
  {
    str_copy = strdup(str);
    path = strtok(str_copy, ":");
    while (path != NULL)
    {
      TRACE_LOG("Counting for token \"%s\".\n", path);
      nof_files_found += count_files(path, true);
      path = strtok(NULL, ":");
    }
    free(str_copy);
  }

  TRACE_LOG("nof_files_found: %d, %d\n", nof_files_found,
      NUMBER_OF_FILES_TO_SHOW_PROGRESS_FOR);
  if (nof_files_found >= NUMBER_OF_FILES_TO_SHOW_PROGRESS_FOR)
    show_progress = true;
  else
    show_progress = false;

  //printf("\n"); // newline for \r-progress indicator

  //build_filelist(".", result, false, babel);

  if ((str = get_configuration_value("z-code-path")) != NULL)
  {
    str_copy = strdup(str);
    path = strtok(str_copy, ":");
    while (path != NULL)
    {
      build_filelist(path, result, false, babel);
      path = strtok(NULL, ":");
    }
    free(str_copy);
  }

  if ((str = get_configuration_value("z-code-root-path")) != NULL)
  {
    str_copy = strdup(str);
    path = strtok(str_copy, ":");
    while (path != NULL)
    {
      build_filelist(path, result, true, babel);
      path = strtok(NULL, ":");
    }
    free(str_copy);
  }

  if (show_progress == true)
    printf("\n");

  TRACE_LOG("noffiles: %d\n", result->nof_entries);

  save_story_list(result);
  store_babel_info_timestamps(babel);

  free_babel_info(babel);

  return result;
#endif // DISABLE_CONFIGFILES
}


void detect_and_add_single_z_file(char *input_filename, char *blorb_filename)
{
  struct z_story_list *z_story_list = get_z_story_list();
  struct babel_info *babel = load_babel_info();

  TRACE_LOG("noffiles: %d\n", z_story_list->nof_entries);

  detect_and_add_z_file(input_filename, blorb_filename, babel, z_story_list);

  TRACE_LOG("noffiles: %d\n", z_story_list->nof_entries);

  save_story_list(z_story_list);
  store_babel_info_timestamps(babel);
  free_z_story_list(z_story_list);
  free_babel_info(babel);
}


void search_directory(char *absolute_dirname, bool recursive)
{
  struct z_story_list *z_story_list = get_z_story_list();
  struct babel_info *babel = load_babel_info();

#ifndef DISABLE_CONFIGFILES
  ensure_dot_fizmo_dir_exists();
#endif // DISABLE_CONFIGFILES

  if ((nof_files_found = count_files(absolute_dirname, recursive)) > 0)
  {
    show_progress = true;
    nof_files_searched = 0;
    nof_directories_searched = 0;
    build_filelist(absolute_dirname, z_story_list, recursive, babel);
  }

  //printf("%d, %s\n", z_story_list->nof_entries, absolute_dirname);
  save_story_list(z_story_list);
  store_babel_info_timestamps(babel);
  free_z_story_list(z_story_list);
  free_babel_info(babel);
}

