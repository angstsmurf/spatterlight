
/* glk_filesys_if.c
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2011-2017 Andrew Plotkin and Christoph Ender.
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

#ifndef glk_filesys_c_INCLUDED 
#define glk_filesys_c_INCLUDED 

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <strings.h>
#include <string.h>

#include "glk.h"
//#include "glkint.h"
#include "glkstart.h" /* This comes with the Glk library. */

#include <interpreter/fizmo.h>
#include <interpreter/text.h>
#include <interpreter/streams.h>
#include <tools/unused.h>
#include <tools/types.h>
#include <tools/i18n.h>
#include <tools/filesys.h>
#include <tools/filesys_c.h>
#include <tools/tracelog.h>
#include "../locales/libglkif_locales.h"

#include "glk_filesys_if.h"
#include "glk_interface.h"



static char *vsnprintf_buf = NULL;
static size_t vsnprintf_buf_size = 0;


static z_file *glkint_openfile(char *filename, int filetype, int fileaccess)
{
  frefid_t fileref = NULL;
  strid_t str = NULL;
  glui32 usage, fmode;
  z_file *result;
  int implementation = -1;

  TRACE_LOG("openfile: %s\n", filename);

  if (filename == NULL)
    return NULL;

  if (filetype == FILETYPE_SAVEGAME)
  {
    usage = fileusage_SavedGame | fileusage_BinaryMode;
    implementation = FILE_IMPLEMENTATION_GLK;
  }
  else if (filetype == FILETYPE_TRANSCRIPT)
  {
    usage = fileusage_Transcript | fileusage_TextMode;
    implementation = FILE_IMPLEMENTATION_GLK;
  }
  else if (filetype == FILETYPE_INPUTRECORD)
  {
    usage = fileusage_InputRecord | fileusage_TextMode;
    implementation = FILE_IMPLEMENTATION_GLK;
  }
  else if (filetype == FILETYPE_DATA)
  {
    usage = fileusage_Data | fileusage_BinaryMode;
    implementation = FILE_IMPLEMENTATION_STDIO;
  }
  else if (filetype == FILETYPE_TEXT)
  {
    usage = fileusage_Data | fileusage_TextMode;
    implementation = FILE_IMPLEMENTATION_STDIO;
  }
  else
    return NULL;

  if (implementation == FILE_IMPLEMENTATION_GLK)
  {
    if (fileaccess == FILEACCESS_READ)
      fmode = filemode_Read;
    else if (fileaccess == FILEACCESS_WRITE)
      fmode = filemode_Write;
    else if (fileaccess == FILEACCESS_APPEND)
      fmode = filemode_WriteAppend;
    else
      return NULL;

    fileref = glk_fileref_create_by_name(usage, filename, 0);

    if (!fileref)
      return NULL;

    TRACE_LOG("new open file: %s\n", filename);
    str = glk_stream_open_file(fileref, fmode, 2);
    /* Dispose of the fileref, whether the stream opened successfully
     * or not. */
    glk_fileref_destroy(fileref);

    if (!str)
    {
      TRACE_LOG("Couldn't GLK-open: %s\n", filename);
      return NULL;
    }

    if ((result = malloc(sizeof(z_file))) == NULL)
      return NULL;

    result = zfile_from_glk_strid(str, filename, filetype, fileaccess);
  }
  else if (implementation == FILE_IMPLEMENTATION_STDIO)
  {
    if ((result = z_filesys_interface_c.openfile(
            filename, filetype, fileaccess)) != NULL)
      result->implementation = FILE_IMPLEMENTATION_STDIO;
  }
  else
    return NULL;

  return result;
}

int glkint_closefile(z_file *file_to_close)
{
  if (file_to_close == NULL)
    return -1;

  if (file_to_close->implementation == FILE_IMPLEMENTATION_STDIO)
    return z_filesys_interface_c.closefile(file_to_close);
  else
  {
    if (file_to_close->file_object)
      glk_stream_close((strid_t)file_to_close->file_object, NULL);
    if (file_to_close->filename)
      free(file_to_close->filename);
    file_to_close->file_object = NULL;
    file_to_close->filename = NULL;
    free(file_to_close);
    return 0;
  }
}

int glkint_readchar(z_file *fileref)
{
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    return z_filesys_interface_c.readchar(fileref);
  else
  {
    int ch = glk_get_char_stream((strid_t)fileref->file_object);
    if (ch < 0)
      return -1;
    return ch;
  }
}

size_t glkint_readchars(void *ptr, size_t len, z_file *fileref)
{
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    return z_filesys_interface_c.readchars(ptr, len, fileref);
  else
    return glk_get_buffer_stream((strid_t)fileref->file_object, ptr, len);
}

int glkint_writechar(int ch, z_file *fileref)
{
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    return z_filesys_interface_c.writechar(ch, fileref);
  else
  {
    glk_put_char_stream((strid_t)fileref->file_object, ch);
    return ch;
  }
}

size_t glkint_writechars(void *ptr, size_t len, z_file *fileref)
{
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    return z_filesys_interface_c.writechars(ptr, len, fileref);
  else
  {
    glk_put_buffer_stream((strid_t)fileref->file_object, ptr, len);
    return len;
  }
}

int glkint_writestring(char *s, z_file *fileref)
{
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    return z_filesys_interface_c.writestring(s, fileref);
  else
    return glkint_writechars(s, strlen(s), fileref);
}


int glkint_writeucsstring(z_ucs *s, z_file *fileref)
{
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    return z_filesys_interface_c.writeucsstring(s, fileref);
  else {
    int len;
    for (len=0; s[len]; len++) { };
    glk_put_buffer_stream_uni((strid_t)fileref->file_object, s, len);
    return len;
  }
}


int glkint_vfileprintf(z_file *fileref, char *format, va_list ap)
{
  long ret_val;
  size_t new_size;
  char *new_buf;
  va_list ap_copy;

  do
  {
    va_copy(ap_copy, ap);
    ret_val = vsnprintf(vsnprintf_buf, vsnprintf_buf_size, format, ap_copy);
    if ((ret_val == -1) || (ret_val > (int)vsnprintf_buf_size))
    {
      new_size = vsnprintf_buf_size + 8192;
      if ((new_buf = realloc(vsnprintf_buf, new_size)) == NULL)
        return -1;
      vsnprintf_buf = new_buf;
      vsnprintf_buf_size = new_size;
      ret_val = -1;
    }
  }
  while (ret_val == -1);

  TRACE_LOG("fileprintf: \"%s\".\n", vsnprintf_buf);
  return glkint_writestring(vsnprintf_buf, fileref);
}

int glkint_fileprintf(z_file *fileref, char *format, ...)
{
  va_list args;
  int result;

  va_start(args, format);
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    result = z_filesys_interface_c.vfileprintf(fileref, format, args);
  else
    result = glkint_vfileprintf(fileref, format, args);
  va_end(args);
  return result;
}

int glkint_vfilescanf(z_file *fileref, char *format, va_list ap)
{
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    return z_filesys_interface_c.vfilescanf(fileref, format, ap);
  else
  {
    i18n_translate_and_exit(
        libglkif_module_name,
        i18n_libglkif_NOT_YET_IMPLEMENTED_IN_LIBGLKIF,
        -0x010d);
    return -1;
  }
}

int glkint_filescanf(z_file *fileref, char *format, ...)
{
  va_list args;
  int result;

  va_start(args, format);
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    result = z_filesys_interface_c.vfilescanf(fileref, format, args);
  else
    result = glkint_vfilescanf(fileref, format, args);
  va_end(args);
  return result;
}

long glkint_getfilepos(z_file *fileref)
{
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    return z_filesys_interface_c.getfilepos(fileref);
  else
    return glk_stream_get_position((strid_t)fileref->file_object);
}

int glkint_setfilepos(z_file *fileref, long seek, int whence)
{
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    return z_filesys_interface_c.setfilepos(fileref, seek, whence);
  else
  {
    if (whence == SEEK_SET)
      glk_stream_set_position(
          (strid_t)fileref->file_object, seek, seekmode_Start);
    else if (whence == SEEK_CUR)
      glk_stream_set_position(
          (strid_t)fileref->file_object, seek, seekmode_Current);
    else if (whence == SEEK_END)
      glk_stream_set_position(
          (strid_t)fileref->file_object, seek, seekmode_End);
    return 0;
  }
}


int glkint_unreadchar(int c, z_file *fileref)
{
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    return z_filesys_interface_c.unreadchar(c, fileref);
  else
  {
    i18n_translate_and_exit(
        libglkif_module_name,
        i18n_libglkif_NOT_YET_IMPLEMENTED_IN_LIBGLKIF,
        -0x010d);
    return -1;
  }
}


int glkint_flushfile(z_file *fileref)
{
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    return z_filesys_interface_c.flushfile(fileref);
  else
  {
    i18n_translate_and_exit(
        libglkif_module_name,
        i18n_libglkif_NOT_YET_IMPLEMENTED_IN_LIBGLKIF,
        -0x010d);
    return -1;
  }
}


time_t glkint_get_last_file_mod_timestamp(z_file *fileref)
{
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    return z_filesys_interface_c.get_last_file_mod_timestamp(fileref);
  else
  {
    i18n_translate_and_exit(
        libglkif_module_name,
        i18n_libglkif_NOT_YET_IMPLEMENTED_IN_LIBGLKIF,
        -0x010d);
    return -1;
  }
}


int glkint_get_fileno(z_file *fileref)
{
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    return z_filesys_interface_c.get_fileno(fileref);
  else
  {
    i18n_translate_and_exit(
        libglkif_module_name,
        i18n_libglkif_NOT_YET_IMPLEMENTED_IN_LIBGLKIF,
        -0x010d);
    return -1;
  }
}


FILE* glkint_get_stdio_stream(z_file *fileref)
{
  if (fileref->implementation == FILE_IMPLEMENTATION_STDIO)
    return z_filesys_interface_c.get_stdio_stream(fileref);
  else
  {
    i18n_translate_and_exit(
        libglkif_module_name,
        i18n_libglkif_NOT_YET_IMPLEMENTED_IN_LIBGLKIF,
        -0x010d);
    return NULL;
  }
}


char *glkint_get_cwd()
{
  return getcwd(NULL, 0);
}


int glkint_ch_dir(char* dirname)
{
  return chdir(dirname);
}


z_dir* glkint_open_dir(char *dirname)
{
  z_dir *result;

  if ((result = malloc(sizeof(z_dir))) == NULL)
    return NULL;

  if ((result->dir_object = opendir(dirname)) == NULL)
  {
    free(result);
    return NULL;
  }

  return result;
}


int glkint_close_dir(z_dir *dirref)
{
  int result;

  result = closedir((DIR*)dirref->dir_object);
  free(dirref);
  return result;
}


int glkint_read_dir(struct z_dir_ent *result, z_dir *dirref)
{
  struct dirent *dir_ent;

  if ((dir_ent = readdir((DIR*)dirref->dir_object)) == NULL)
    return -1;

  result->d_name = dir_ent->d_name;
  return 0;
}


static int glkint_make_dir(char *path)
{
  return z_filesys_interface_c.make_dir(path);
}


bool glkint_is_filename_directory(char *filename)
{
  int filedes;
  struct stat stat_buf;
  bool result;

  if ((filedes = open(filename, O_RDONLY)) < 0)
    return false;

  if (fstat(filedes, &stat_buf) != 0)
    return false;

  result = stat_buf.st_mode & S_IFDIR
    ? true
    : false;

  close(filedes);
  return result;
}


struct z_filesys_interface glkint_filesys_interface =
{
  &glkint_openfile,
  &glkint_closefile,
  &glkint_readchar,
  &glkint_readchars,
  &glkint_writechar,
  &glkint_writechars,
  &glkint_writestring,
  &glkint_writeucsstring,
  &glkint_fileprintf,
  &glkint_vfileprintf,
  &glkint_filescanf,
  &glkint_vfilescanf,
  &glkint_getfilepos,
  &glkint_setfilepos,
  &glkint_unreadchar,
  &glkint_flushfile,
  &glkint_get_last_file_mod_timestamp,
  &glkint_get_fileno,
  &glkint_get_stdio_stream,
  &glkint_get_cwd,
  &glkint_ch_dir,
  &glkint_open_dir,
  &glkint_close_dir,
  &glkint_read_dir,
  &glkint_make_dir,
  &glkint_is_filename_directory
};

#endif // glk_filesys_c_INCLUDED 

