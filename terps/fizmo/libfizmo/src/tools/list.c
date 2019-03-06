
/* list.c
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


#ifndef list_c_INCLUDED 
#define list_c_INCLUDED

#include <stdlib.h>

#include "list.h"


list *create_list()
{
  list *result;

  if ((result = malloc(sizeof(list))) == NULL)
    return NULL;

  result->elements = NULL;
  result->nof_elements_stored = 0;
  result->space_available = 0;

  return result;
}


int add_list_element(list *l, void *value)
{
  void **ptr;

  //printf("list: %d / %d\n", 
  //    l->nof_elements_stored, l->space_available);

  if (l->nof_elements_stored == l->space_available)
  {
    if ((ptr = realloc(l->elements,
            sizeof(void*) * (l->space_available + LIST_INC_SIZE)))
        == NULL)
      return -1;

    l->elements = ptr;
    l->space_available += LIST_INC_SIZE;
  }

  l->elements[l->nof_elements_stored] = value;
  l->nof_elements_stored++;
  //printf("new-nof: %d\n", l->nof_elements_stored);

  return 0;
}


int get_list_size(list *l)
{
  return l->nof_elements_stored;
}


void *get_list_element(list *l, int list_index)
{
  return l->elements[list_index];
}


bool list_contains_element(list *haystack, void *needle)
{
  size_t index = 0;
  while (index < haystack->nof_elements_stored)
  {
    if (haystack->elements[index] == needle)
      return true;
    index++;
  }
  return false;
}


void delete_list(list *l)
{
  free(l->elements);
  free(l);
}


void **delete_list_and_get_ptrs(list *l)
{
  void **result = l->elements;

  if (l->space_available > l->nof_elements_stored)
    if ((result = realloc(l->elements, sizeof(void*) * l->nof_elements_stored))
        == NULL)
      result = l->elements;

  free(l);
  return result;
}


void **delete_list_and_get_null_terminated_ptrs(list *l)
{
  void **result = l->elements;

  if (get_list_size(l) > 0) {
    if (l->space_available > l->nof_elements_stored + 1)
      if ((result = realloc(l->elements,
              sizeof(void*) * (l->nof_elements_stored + 1)))
          == NULL)
        result = l->elements;
    result[l->nof_elements_stored] = NULL;

    free(l);
  }
  return result;
}


#endif /* list_c_INCLUDED */

