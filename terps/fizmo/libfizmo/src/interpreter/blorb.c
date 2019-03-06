
/* blorb.c
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


#ifndef blorb_c_INCLUDED 
#define blorb_c_INCLUDED

#include <errno.h>
#include <string.h>

#include "../tools/types.h"
#include "../tools/filesys.h"
#include "../tools/i18n.h"
#include "../tools/tracelog.h"
#include "../locales/libfizmo_locales.h"
#include "blorb.h"
#include "fizmo.h"
#include "iff.h"


struct fizmo_blorb_struct
{
  int resource_number;
  int type;
  long offset;
  int v3_number_of_loops;
};

typedef struct fizmo_blorb_struct fizmo_blorb;



struct fizmo_blorb_map_struct
{
  z_file *blorb_file;
  fizmo_blorb **blorbs;
  int frontispiece_image_no;
};

typedef struct fizmo_blorb_map_struct fizmo_blorb_map;



static fizmo_blorb *fizmo_get_blorb(z_blorb_map *blorb_map, int usage,
    int resnum)
{
  int i = 0;
  fizmo_blorb_map *map;
  fizmo_blorb *blorb;

  if (blorb_map == NULL)
    return NULL;

  map = (fizmo_blorb_map*)blorb_map->blorb_map_implementation;

  blorb = map->blorbs[i++];
  while (blorb != NULL)
  {
    if (
        (blorb->resource_number == resnum)
        &&
        (blorb->type == usage)
       )
      return blorb;

    blorb = map->blorbs[i++];
  }

  return NULL;
}


static z_blorb_map *fizmo_blorb_init(z_file *blorb_file)
{
  z_blorb_map *result_wrapper;
  fizmo_blorb_map *result;
  fizmo_blorb *blorb;
  int resource_chunk_size;
  char buf[5];
  int nof_resources, nof_loops, blorb_index, resource_number;

  if (find_chunk("RIdx", blorb_file) == -1)
  {
    fsi->closefile(blorb_file);
    return NULL;
  }

  if (read_chunk_length(blorb_file) == -1)
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_FUNCTION_CALL_P0S_RETURNED_ERROR_CODE_P1D,
        -0x0101,
        "read_chunk_length",
        errno);

  result = fizmo_malloc(sizeof(fizmo_blorb_map));
  result->blorb_file = blorb_file;
  result_wrapper = fizmo_malloc(sizeof(z_blorb_map));
  result_wrapper->blorb_map_implementation = result;

  resource_chunk_size = get_last_chunk_length();
  nof_resources = (resource_chunk_size - 4) / 12;

  // Skip next number of resources.
  if ((fsi->setfilepos(result->blorb_file, 4, SEEK_CUR)) != 0)
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_FUNCTION_CALL_P0S_RETURNED_ERROR_CODE_P1D,
        -0x0101,
        "setfilepos",
        errno);

  TRACE_LOG("Number of resources in blorb file: %d.\n", nof_resources);

  // Count number of images and sounds.
  result->blorbs = fizmo_malloc(sizeof(fizmo_blorb*) * (nof_resources+1));

  buf[4] = '\0';
  blorb_index = 0;
  while (nof_resources > 0)
  {
    blorb = fizmo_malloc(sizeof(fizmo_blorb));

    if (fsi->readchars(buf, 4, result->blorb_file) != 4)
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_FUNCTION_CALL_P0S_ABORTED_DUE_TO_ERROR,
          -0x0106,
          "readchars");

    TRACE_LOG("Type descriptor: %s\n", buf);
    if (strcmp(buf, "Pict") == 0)
      blorb->type = Z_BLORB_TYPE_PICT;
    else if (strcmp(buf, "Snd ") == 0)
      blorb->type = Z_BLORB_TYPE_SOUND;
    else if (strcmp(buf, "Exec") == 0)
      blorb->type = Z_BLORB_TYPE_EXEC;
    else
      // Unknown resource.
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_UNKNOWN_ERROR_CASE,
          -1);

    blorb->resource_number = read_four_byte_number(result->blorb_file);
    blorb->offset = read_four_byte_number(result->blorb_file) + 8;
    blorb->v3_number_of_loops = -1;

    result->blorbs[blorb_index++] = blorb;

    nof_resources--;
  }

  result->blorbs[blorb_index] = NULL;

  if (find_chunk("Fspc", result->blorb_file) == 0)
  {
    if ((fsi->setfilepos(result->blorb_file, 4, SEEK_CUR)) != 0)
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_FUNCTION_CALL_P0S_RETURNED_ERROR_CODE_P1D,
          -0x0101,
          "setfilepos",
          errno);

    result->frontispiece_image_no = read_four_byte_number(result->blorb_file);
  }
  else
    result->frontispiece_image_no = -1;

  if (ver < 5)
  {
    if (find_chunk("Loop", result->blorb_file) == 0)
    {
      nof_resources = read_four_byte_number(result->blorb_file) / 8;
      TRACE_LOG("Number of loop entries: %d.\n", nof_resources);

      while (nof_resources > 0)
      {
        resource_number = read_four_byte_number(result->blorb_file);
        nof_loops = read_four_byte_number(result->blorb_file);

        TRACE_LOG("Trying to find resource #%d.\n", resource_number);
        if ((blorb = fizmo_get_blorb(
                result_wrapper, Z_BLORB_TYPE_SOUND, resource_number)) != NULL)
        {
          TRACE_LOG("Resource found, setting nof_loops to %d.\n",
              nof_loops);
          blorb->v3_number_of_loops = nof_loops;
        }

        nof_resources--;
      }
    }
  }

  return result_wrapper;
}


static long fizmo_get_blorb_offset(z_blorb_map *blorb_map, int usage,
    int resnum)
{
  fizmo_blorb *result_blorb;

  if ((result_blorb = fizmo_get_blorb(blorb_map, usage, resnum)) == NULL)
    return -1;
  else
    return result_blorb->offset;
}


static int fizmo_get_frontispiece_resource_number(z_blorb_map *blorb_map)
{
  fizmo_blorb_map *map;

  if (blorb_map == NULL)
    return -1;
  map = (fizmo_blorb_map*)blorb_map->blorb_map_implementation;
  return map->frontispiece_image_no;
}


static int fizmo_free_blorb_map(z_blorb_map *blorb_map)
{
  int i = 0;
  fizmo_blorb_map *map;
  fizmo_blorb *blorb;

  if (blorb_map == NULL)
    return -1;

  map = (fizmo_blorb_map*)blorb_map->blorb_map_implementation;

  blorb = map->blorbs[i++];
  while (blorb != NULL)
  {
    free(blorb);
    blorb = map->blorbs[i++];
  }

  free(map->blorbs);
  free(map);
  free(blorb_map);

  return 0;
}


int get_v3_sound_loops_from_blorb_map(z_blorb_map *blorb_map, int resnum)
{
  fizmo_blorb *result_blorb;

  if ((result_blorb = fizmo_get_blorb(blorb_map, Z_BLORB_TYPE_SOUND, resnum))
      == NULL)
    return -1;
  else
    return result_blorb->v3_number_of_loops;
}


struct z_blorb_interface fizmo_blorb_interface =
{
  &fizmo_blorb_init,
  &fizmo_get_blorb_offset,
  &fizmo_get_frontispiece_resource_number,
  &fizmo_free_blorb_map
};


struct z_blorb_interface *active_blorb_interface = &fizmo_blorb_interface;

#endif /* blorb_c_INCLUDED */

