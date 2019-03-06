
/* filesys_c.c
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2011-2017 Christoph Ender.
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


#ifndef filesys_c_c_INCLUDED 
#define filesys_c_c_INCLUDED

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "filesys_c.h"
#include "tracelog.h"
#include "types.h"
#include "z_ucs.h"
#include "../filesys_interface/filesys_interface.h"

#if defined (__WIN32__)
static char *fileaccess_string[] = { "rb", "wb", "ab" };
#else
static char *fileaccess_string[] = { "r", "w", "a" };
#endif // defined (__WIN32__)


static z_file *openfile_c(char *filename, int filetype, int fileaccess)
{
  z_file *result;
  int access_index;

  if ((result = malloc(sizeof(z_file))) == NULL)
    return NULL;

  if (fileaccess == FILEACCESS_READ)
    access_index = 0;
  else if (fileaccess == FILEACCESS_WRITE)
    access_index = 1;
  else if (fileaccess == FILEACCESS_APPEND)
    access_index = 2;
  else
    return NULL;

  TRACE_LOG("Trying to fopen(\"%s\", \"%s\").\n",
      filename, fileaccess_string[access_index]);

  if ((result->file_object
        = (void*)fopen(filename, fileaccess_string[access_index]))
      == NULL)
  {
    free(result);
    return NULL;
  }
  result->filename = strdup(filename);
  result->filetype = filetype;
  result->fileaccess = fileaccess;

  return result;
}


static int closefile_c(z_file *file_to_close)
{
  // Since the fclose manpage says that even if "fclose" fails no further
  // access to the stream is possible, the "closefile_c" function will always
  // clear up the z_file structure.
  int result = fclose((FILE*)file_to_close->file_object);
  free(file_to_close->filename);
  file_to_close->file_object = NULL;
  file_to_close->filename = NULL;
  free(file_to_close);
  return result;
}


static int readchar_c(z_file *fileref)
{
  int result = fgetc((FILE*)fileref->file_object);
  return result == EOF ? -1 : result;
}


static size_t readchars_c(void *ptr, size_t len, z_file *fileref)
{
  return fread(ptr, 1, len, (FILE*)fileref->file_object);
}


static int writechar_c(int ch, z_file *fileref)
{
  return putc(ch, (FILE*)fileref->file_object);
}


static size_t writechars_c(void *ptr, size_t len, z_file *fileref)
{
  return fwrite(ptr, 1, len, (FILE*)fileref->file_object);
}


int writestring_c(char *s, z_file *fileref)
{
  return writechars_c(s, strlen(s), fileref);
}


int writeucsstring_c(z_ucs *s, z_file *fileref)
{
  char buf[128];
  int len;
  int res = 0;

  // FIMXE: Re-implement for various output charsets.
  while (*s != 0)
  {
    len = zucs_string_to_utf8_string(buf, &s, 128);
    res += writechars_c(buf, len-1, fileref);
  }

  return res;
}


static int fileprintf_c(z_file *fileref, char *format, ...)
{
  va_list args;
  va_start(args, format);
  int result = vfprintf((FILE*)fileref->file_object, format, args);
  va_end(args);
  return result;
}


static int vfileprintf_c(z_file *fileref, char *format, va_list ap)
{
  return vfprintf((FILE*)fileref->file_object, format, ap);
}


static int filescanf_c(z_file *fileref, char *format, ...)
{
  va_list args;
  va_start(args, format);
  int result = vfscanf((FILE*)fileref->file_object, format, args);
  va_end(args);
  return result;
}


static int vfilescanf_c(z_file *fileref, char *format, va_list ap)
{
  return vfscanf((FILE*)fileref->file_object, format, ap);
}


static long getfilepos_c(z_file *fileref)
{
  return ftell((FILE*)fileref->file_object);
}


static int setfilepos_c(z_file *fileref, long seek, int whence)
{
  return fseek((FILE*)fileref->file_object, seek, whence);
}


static int unreadchar_c(int c, z_file *fileref)
{
  return ungetc(c, (FILE*)fileref->file_object);
}


static int flushfile_c(z_file *fileref)
{
  return fflush((FILE*)fileref->file_object);
}


static time_t get_last_file_mod_timestamp_c(z_file *fileref)
{
  struct stat stat_buf;

  if (fstat(fileno((FILE*)fileref->file_object), &stat_buf) != 0)
    return -1;
  else
    return stat_buf.st_ctime;
}


static int get_fileno_c(z_file *fileref)
{
  return fileno((FILE*)fileref->file_object);
}


static FILE* get_stdio_stream_c(z_file *fileref)
{
  return (FILE*)fileref->file_object;
}


static z_dir *open_dir_c(char *dirname)
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


static int close_dir_c(z_dir *dirref)
{
  int result;
  
  result = closedir((DIR*)dirref->dir_object);
  free(dirref);
  return result;
}


static int read_dir_c(struct z_dir_ent *result, z_dir *dirref)
{
  struct dirent *dir_ent;

  if ((dir_ent = readdir((DIR*)dirref->dir_object)) == NULL)
    return -1;

  result->d_name = dir_ent->d_name;
  return 0;
}


static int make_dir_c(char *path)
{
  return mkdir(path
#if !defined (__WIN32__)
    ,S_IRWXU
#endif // !defined (__WIN32__)
  );
}


static char* get_cwd_c()
{
  return getcwd(NULL, 0);
}


static int ch_dir_c(char *dirname)
{
  return chdir(dirname);
}


static bool is_filename_directory_c(char *filename)
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


struct z_filesys_interface z_filesys_interface_c =
{
  &openfile_c,
  &closefile_c,
  &readchar_c,
  &readchars_c,
  &writechar_c,
  &writechars_c,
  &writestring_c,
  &writeucsstring_c,
  &fileprintf_c,
  &vfileprintf_c,
  &filescanf_c,
  &vfilescanf_c,
  &getfilepos_c,
  &setfilepos_c,
  &unreadchar_c,
  &flushfile_c,
  &get_last_file_mod_timestamp_c,
  &get_fileno_c,
  &get_stdio_stream_c,
  &get_cwd_c,
  &ch_dir_c,
  &open_dir_c,
  &close_dir_c,
  &read_dir_c,
  &make_dir_c,
  &is_filename_directory_c
};


#endif /* filesys_c_c_INCLUDED */

