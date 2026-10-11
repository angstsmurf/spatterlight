/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * os_glk_platform.cpp: the one piece of Glk linkage that differs by host --
 * opening a game file on disk as a Glk stream.  Windows Glk has no
 * glkunix_stream_open_pathname(); a fileref made from the path with
 * validation off opens the same file.  Every other host has the glkunix
 * call, Gargoyle on Windows included, since it starts up through
 * glkunix_startup_code().  The startup code in os_glk.cpp and the ADRIFT 5
 * resource map in os_glk_a5.cpp both open files through here, so neither
 * carries a _WIN32 branch of its own.
 */

#include "os_glk_internal.h"

#if defined(_WIN32) && !defined(GARGLK)
#include "WinGlk.h"
#else
/* glkstart.h exists only on this side of the #if, which is why it is
 * included here (and in os_glk.cpp's UNIX linkage section) rather than in
 * os_glk_internal.h. */
extern "C" {
#include "glkstart.h"
}
#endif

/*
 * gsc_open_game_path()
 *
 * Open the file at path as a read-only binary Glk stream with the given rock.
 * Returns nullptr if the host cannot reach it: a Glk that cannot reopen files
 * by path, or a game that has moved since startup.
 */
strid_t
gsc_open_game_path (const char *path, glui32 rock)
{
#if defined(_WIN32) && !defined(GARGLK)
  frefid_t fileref;
  strid_t stream;

  fileref = winglk_fileref_create_by_name (fileusage_BinaryMode
                                           | fileusage_Data,
                                           (char *) path, 0, 0);
  if (fileref == nullptr)
    return nullptr;
  stream = glk_stream_open_file (fileref, filemode_Read, rock);
  glk_fileref_destroy (fileref);
  return stream;
#else
  return glkunix_stream_open_pathname ((char *) path, FALSE, rock);
#endif
}
