
/* stringmap.c
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


#ifndef stringmap_c_INCLUDED 
#define stringmap_c_INCLUDED

#include <stdlib.h>

#include "stringmap.h"
#include "types.h"
#include "z_ucs.h"
#include "tracelog.h"


stringmap *create_stringmap()
{
  stringmap *result;

  if ((result = malloc(sizeof(stringmap))) == NULL)
    return NULL;

  result->elements = NULL;
  result->nof_elements_stored = 0;
  result->space_available = 0;

  return result;
}

        
static int get_stringmap_value_index(stringmap *map, z_ucs *name)
{
  size_t i;

  TRACE_LOG("Looking for value with key '");
  TRACE_LOG_Z_UCS(name);
  TRACE_LOG("'.\n");

  if ( (map == NULL) || (name == NULL) )
    return -1;

  for (i=0; i<map->nof_elements_stored; i++)
  {
    TRACE_LOG("Found '");
    TRACE_LOG_Z_UCS(map->elements[i]->name);
    TRACE_LOG("'.\n");

    if (z_ucs_cmp(map->elements[i]->name, name) == 0)
    {
      TRACE_LOG("Successfully found key at index %ld.\n", (long int)i);
      return i;
    }
  }

  return -1;
}


int add_stringmap_element(stringmap *map, z_ucs *name, void *value)
{
  stringmap_element *element_ptr;
  stringmap_element **element_ptr_ptr;

  if (get_stringmap_value_index(map, name) >= 0)
    return -1;

  if (map->nof_elements_stored == map->space_available)
  {
    if ((element_ptr_ptr = realloc(
            map->elements,
            sizeof(stringmap_element) * map->space_available + MAP_INC_SIZE))
        == NULL)
      return -1;

    map->elements = element_ptr_ptr;
    map->space_available += MAP_INC_SIZE;
  }

  if ((element_ptr = malloc(sizeof(stringmap_element))) == NULL)
    return -1;

  if ((element_ptr->name = z_ucs_dup(name)) == NULL)
  {
    free(element_ptr);
    return -1;
  }

  element_ptr->value = value;

  map->elements[map->nof_elements_stored] = element_ptr;

  TRACE_LOG("Stored new value with key '");
  TRACE_LOG_Z_UCS(element_ptr->name);
  TRACE_LOG("'.\n");

  map->nof_elements_stored++;

  return 0;
}


void *get_stringmap_value(stringmap *map, z_ucs *name)
{
  int result_index = get_stringmap_value_index(map, name);

  return result_index >= 0
    ? map->elements[result_index]->value
    : NULL;
}


z_ucs **get_names_in_stringmap(stringmap *map)
{
  z_ucs **result;
  size_t i;

  if (map->nof_elements_stored == 0)
    return NULL;

  if ((result = malloc(sizeof(z_ucs*) * (map->nof_elements_stored+1))) == NULL)
    return NULL;

  for (i=0; i<map->nof_elements_stored; i++)
    result[i] = map->elements[i]->name;
  result[i] = NULL;

  return result;
}


void delete_stringmap(stringmap *map)
{
  size_t i;

  if (map == NULL)
    return;

  for (i=0; i<map->nof_elements_stored; i++)
    free(map->elements[i]);

  free(map->elements);
}

#endif /* stringmap_c_INCLUDED */

