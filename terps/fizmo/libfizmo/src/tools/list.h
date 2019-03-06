
/* list.h
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
 *
 *
 * The "list" represents an arbitrary amount of pointers. In order to
 * generalize it and make it useable for multiple types of data the
 * data is stored as pointers to void. It should be noted that although void
 * pointers may be arbitrarily converted into other types, wild type
 * conversions on not-aligned objects may cause errors. Thus, the stored
 * objects should be malloc()ed in their correct type, then cast to void
 * and may then be safely stored in a "list". For usage, the list elements
 * have to be retrieved and then converted back to their original type.
 *
 */

#ifndef list_h_INCLUDED 
#define list_h_INCLUDED

#include "types.h"

#define LIST_INC_SIZE 32


typedef struct
{
  void** elements;
  size_t nof_elements_stored;
  size_t space_available;
} list;


list *create_list();
int add_list_element(list *l, void *value);
int get_list_size(list *l);
void *get_list_element(list *l, int list_index);
bool list_contains_element(list *haystack, void *needle);
void delete_list(list *l);
void **delete_list_and_get_ptrs(list *l);
void **delete_list_and_get_null_terminated_ptrs(list *l);


#endif /* list_h_INCLUDED */

