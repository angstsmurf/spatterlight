/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * Write a rendered map surface out as a binary PPM, for the two headless map
 * dumpers (adrift4/harness/scmap_dump.cpp and adrift5/harness/a5map_dump.cpp)
 * whose output the map tests diff against their goldens.
 */

#ifndef TEST_MAP_PPM_H
#define TEST_MAP_PPM_H

#include <stdio.h>

#include "../mapdraw.h"

/* Returns 0 if `path` cannot be opened for writing, 1 once it is written. */
static inline int
map_write_ppm (const map_surface_t *surf, const char *path)
{
  FILE *f = fopen (path, "wb");
  int i;

  if (f == NULL)
    return 0;
  fprintf (f, "P6\n%d %d\n255\n", surf->w, surf->h);
  for (i = 0; i < surf->w * surf->h; i++)
    {
      unsigned int p = surf->px[i];
      unsigned char rgb[3];
      rgb[0] = (unsigned char) ((p >> 16) & 0xFF);
      rgb[1] = (unsigned char) ((p >> 8) & 0xFF);
      rgb[2] = (unsigned char) (p & 0xFF);
      fwrite (rgb, 1, 3, f);
    }
  fclose (f);
  return 1;
}

#endif
