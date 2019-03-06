
/* glk_blorb_if.c
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


#ifndef glk_blorb_c_INCLUDED 
#define glk_blorb_c_INCLUDED 

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
#include <blorb_interface/blorb_interface.h>
#include <tools/unused.h>
#include <tools/types.h>
#include <tools/i18n.h>
#include <tools/tracelog.h>


//#include "glk.h"
//#include "glkstart.h" /* This comes with the Glk library. */
#include "gi_blorb.h"


z_blorb_map* glkint_init_blorb_map(z_file *blorb_file)
{
  z_blorb_map *result;
  giblorb_map_t *map_ptr;

  if ((result = malloc(sizeof(z_blorb_map))) == NULL)
    return NULL;

  giblorb_create_map((strid_t)blorb_file->file_object, &map_ptr);
  result->blorb_map_implementation = map_ptr;

  return result;
}


long glkint_get_blorb_offset(z_blorb_map *blorb_map, int usage, int resnum)
{
  giblorb_map_t *map_ptr = (giblorb_map_t*)blorb_map->blorb_map_implementation;
  giblorb_result_t res;
  glui32 glk_usage;

  if (usage == Z_BLORB_TYPE_PICT)
    glk_usage = giblorb_ID_Pict;
  else if (usage == Z_BLORB_TYPE_SOUND)
    glk_usage = giblorb_ID_Snd;
  else if (usage == Z_BLORB_TYPE_EXEC)
    glk_usage = giblorb_ID_Exec;
  else
    return -1;

  if (giblorb_load_resource(
        map_ptr, giblorb_method_FilePos, &res, glk_usage, resnum)
      == giblorb_err_None)
    return res.data.startpos;
  else
    return -1;
}


int glkint_get_frontispiece_resource_number(z_blorb_map *blorb_map)
{
  glui32 frontispiece_type = giblorb_make_id('F', 's', 'p', 'c');
  giblorb_map_t *map_ptr = (giblorb_map_t*)blorb_map->blorb_map_implementation;
  giblorb_result_t res;
  uint8_t *ptr;

  giblorb_load_chunk_by_type(
      map_ptr, giblorb_method_Memory, &res, frontispiece_type, 0);

  if (res.length != 4)
    return -1;
  else
  {
    ptr = res.data.ptr;
    return (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | (ptr[3]);
  }
}


int glkint_free_blorb_map(z_blorb_map *blorb_map)
{
  giblorb_destroy_map((giblorb_map_t*)blorb_map->blorb_map_implementation);
  return 0;
}


struct z_blorb_interface glkint_blorb_interface =
{
  &glkint_init_blorb_map,
  &glkint_get_blorb_offset,
  &glkint_get_frontispiece_resource_number,
  &glkint_free_blorb_map
};

#endif // glk_blorb_c_INCLUDED 

