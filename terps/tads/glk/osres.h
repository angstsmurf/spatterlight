/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 by Petter Sjölund.                                      *
 *                                                                            *
 * This file is free software; you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * This file is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this file; if not, write to the Free Software                   *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 ******************************************************************************/

/* osres.h -- a game's images and sounds, as Glk resources */

#ifndef OSRES_H
#define OSRES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "glk.h"

struct appctxdef;

/*
 *   The engines tell us where the resources are.  File 0 is the game file;
 *   the others are its .rsN (TADS 2) or .3rN (TADS 3) files.  An offset is
 *   from the file's base, which TADS 2 sets to where its resource map ends
 *   (and may set after adding the resources), and which is 0 in TADS 3.
 */
void osres_set_file(int fileno, const char *path);
void osres_set_base(int fileno, unsigned long base);
void osres_add(int fileno, unsigned long ofs, unsigned long siz,
               const char *name, size_t namelen);

/* a resource that is a whole local file (a debug build's link) */
void osres_add_link(const char *path, size_t pathlen,
                    const char *name, size_t namelen);

/* the TADS 2 host context, whose callbacks do the above */
struct appctxdef *osres_t2_appctx(void);

/*
 *   The Glk resource number of the image (giblorb_ID_Pict) or sound
 *   (giblorb_ID_Snd) with this URL-style name, or 0 if there is none or
 *   the library can't have it.  A name not in the game's resource maps is
 *   looked for as a file in the game's folder.
 */
glui32 osres_find(const char *name, size_t len, glui32 usage);

/* does the resource exist, as a map entry or a file? */
int osres_exists(const char *name, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* OSRES_H */
