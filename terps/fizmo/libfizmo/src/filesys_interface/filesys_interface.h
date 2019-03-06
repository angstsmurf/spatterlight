
/* filesys_interface.h
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


#ifndef filesys_interface_h_INCLUDED
#define filesys_interface_h_INCLUDED

#include <stdio.h>
#include <stdarg.h>
#include "../tools/types.h"

#define FILETYPE_SAVEGAME 0
#define FILETYPE_TRANSCRIPT 1
#define FILETYPE_INPUTRECORD 2
#define FILETYPE_DATA 3
#define FILETYPE_TEXT 4

#define FILEACCESS_READ 0
#define FILEACCESS_WRITE 1
#define FILEACCESS_APPEND 2


struct z_filesys_interface
{
  z_file* (*openfile)(char *filename, int filetype, int fileaccess);
  int (*closefile)(z_file *file_to_close);

  // Returns -1 on EOF.
  int (*readchar)(z_file *fileref);

  // Returns number of bytes read.
  size_t (*readchars)(void *ptr, size_t len, z_file *fileref);

  int (*writechar)(int ch, z_file *fileref);

  // Returns number of bytes successfully written.
  size_t (*writechars)(void *ptr, size_t len, z_file *fileref);

  int (*writestring)(char *s, z_file *fileref);
  int (*writeucsstring)(z_ucs *s, z_file *fileref);
  int (*fileprintf)(z_file *fileref, char *format, ...);
  int (*vfileprintf)(z_file *fileref, char *format, va_list ap);
  int (*filescanf)(z_file *fileref, char *format, ...);
  int (*vfilescanf)(z_file *fileref, char *format, va_list ap);

  long (*getfilepos)(z_file *fileref);
  int (*setfilepos)(z_file *fileref, long seek, int whence);

  int (*unreadchar)(int c, z_file *fileref);
  int (*flushfile)(z_file *fileref);
  time_t (*get_last_file_mod_timestamp)(z_file *fileref);

  int (*get_fileno)(z_file *fileref);
  FILE* (*get_stdio_stream)(z_file *fileref);

  char* (*get_cwd)();
  int (*ch_dir)(char *dirname);
  z_dir* (*open_dir)(char *dirname);
  int (*close_dir)(z_dir *dirref);
  int (*read_dir)(struct z_dir_ent *dir_ent, z_dir *dirref);
  int (*make_dir)(char *path);

  bool (*is_filename_directory)(char *filename);
};

#endif /* filesys_interface_h_INCLUDED */

