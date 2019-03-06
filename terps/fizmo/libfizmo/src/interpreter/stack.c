
/* stack.c
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2009-2017 Christoph Ender.
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


#ifndef stack_c_INCLUDED 
#define stack_c_INCLUDED

#include <stdlib.h>
#include <string.h>

#include "../tools/tracelog.h"
#include "../tools/i18n.h"
#include "stack.h"
#include "config.h"
#include "routine.h"
#include "variable.h"
#include "fizmo.h"
#include "../locales/libfizmo_locales.h"


size_t current_z_stack_size = 0;
/*@null@*/ /*@owned@*/ uint16_t *z_stack = NULL;
/*@null@*/ /*@dependent@*/ uint16_t *z_stack_index = NULL;
/*@null@*/ /*@dependent@*/ uint16_t *behind_z_stack = NULL;
int stack_words_from_active_routine = 0;


// This function is called by the z_stack_push_word function and the
// "allocate_z_stack_words" function in case the capacity of the
// current stack is too small to hold additional data. This function
// will enlarge the current stack by a number of uint16_t words given
// in the "added_capacity" parameter.
static void resize_z_stack(int32_t added_capacity)
{
  int index_position;
  size_t offset;

  if (added_capacity == 0)
    return;

  TRACE_LOG("Enlarging stack capacity by %d entries.\n", (int)added_capacity);

  // We'll have to remember the count of elements currently on the stack.
  // This is required since a call to realloc might move the stack to
  // some other place in memory. Keeping the current index number in mind
  // will allow us to create a corrent pointer to a possibly new location.
  index_position = z_stack_index - z_stack;

  TRACE_LOG("current stack size: %d.\n", current_z_stack_size);
  current_z_stack_size += added_capacity;
  offset = local_variable_storage_index - z_stack;

  TRACE_LOG("z_stack: %p, z_stack_index: %p, behind_z_stack: %p.\n",
      z_stack, z_stack_index, behind_z_stack);

  // Initially, z_stack is NULL. If realloc() is called with a pointer to
  // null it works like malloc() which suits just fine.
  z_stack = (uint16_t*)fizmo_realloc(
          z_stack,
          current_z_stack_size * sizeof(uint16_t));

  behind_z_stack = z_stack + current_z_stack_size;
  local_variable_storage_index = z_stack + offset;

  if ((z_stack_index = z_stack + index_position) >= behind_z_stack)
  {
    // In case the stack has been shrunk the index may have to be re-adjusted.
    z_stack_index = behind_z_stack - 1;
  }

  TRACE_LOG("Z-Stack now at %p (element behind: %p, z_stack_index: %p).\n",
      z_stack, behind_z_stack, z_stack_index);
  TRACE_LOG("new stack size: %d.\n", current_z_stack_size);
}


// This function will push the given 16-bit-word on the Z-Stack. In case the
// current stack is too small, it is enlarged by the size of the constant
// Z_STACK_INCREMENT_SIZE which is defined in config.h
void z_stack_push_word(uint16_t data)
{
  TRACE_LOG("Pushing %x on stack.\n", data);
  TRACE_LOG("Stack pointer before operation: %ld.\n",
    (long int)(z_stack_index - z_stack));
  if (z_stack_index == behind_z_stack)
    resize_z_stack(Z_STACK_INCREMENT_SIZE);
  *(z_stack_index++) = data;
  TRACE_LOG("Stack pointer after operation: %ld.\n",
    (long int)(z_stack_index - z_stack));
}


// The "z_stack_pull_word" function will return the current topmost word
// from the stack. In case the stack is empty, it will quit via a call to
// "i18n_translate_and_exit".
uint16_t z_stack_pull_word()
{
  uint16_t result;
  if (z_stack_index == z_stack)
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_CANNOT_PULL_FROM_EMPTY_STACK,
        -1);

  TRACE_LOG("Pulling %x from stack.\n", *(z_stack_index-1));
  TRACE_LOG("Stack pointer before operation: %ld.\n",
    (long int)(z_stack_index - z_stack));
  result = *(--z_stack_index);
  TRACE_LOG("Stack pointer after operation: %ld.\n",
    (long int)(z_stack_index - z_stack));
  return result;
}


// The "z_stack_peek_word" function will return the topmost stack entry
// without altering the stack contents. In case the stack is empty, it
// will quit via a call to "i18n_translate_and_exit".
uint16_t z_stack_peek_word()
{
  if (z_stack_index == z_stack)
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_CANNOT_PULL_FROM_EMPTY_STACK,
        -1);

  return *(z_stack_index - 1);
}


// This function is used by the restart-opcode and the save-game
// functionality. It simply decreases the stack by a number of words
// given in the "word_counter" and calls the "i18n_translate_and_exit"
// function in case there are not enough words on the stack.
void drop_z_stack_words(int word_counter)
{
  if (z_stack_index - z_stack < word_counter)
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_CANNOT_DROP_P0D_WORDS_FROM_STACK_NOT_ENOUGH_WORDS_STORED,
        -1,
        (long int)word_counter);

  z_stack_index -= word_counter;
}


// This function is used when a new routine is started and space should
// be reserved for new locals. It returns a pointer to the reserved
// space on the stack.
/*@dependent@*/ uint16_t *allocate_z_stack_words(uint32_t word_counter)
{
  uint16_t *result;

  TRACE_LOG("Allocating %d stack words.\n", word_counter);

  while (z_stack_index + word_counter >= behind_z_stack)
  {
    TRACE_LOG("Space on stack: %d.\n", behind_z_stack - z_stack_index);
    resize_z_stack(Z_STACK_INCREMENT_SIZE);
  }

  TRACE_LOG("Space on stack: %d.\n", behind_z_stack - z_stack_index);

  // z_stack_index can't be null, if it was the resize_z_stac would have
  // quit, so -nullpass and -nullret are okay.

  /*@-nullpass@*/
  result = z_stack_index;
  /*@+nullpass@*/

  z_stack_index += word_counter;

  /*@-nullret@*/
  return result;
  /*@+nullret@*/
}


/*@only@*/ struct z_stack_container *create_new_stack()
{
  struct z_stack_container *current_z_stack_data;

  current_z_stack_data = (struct z_stack_container*)fizmo_malloc(
      sizeof(struct z_stack_container));

  current_z_stack_data->current_z_stack_size = current_z_stack_size;
  current_z_stack_data->z_stack = z_stack;
  current_z_stack_data->z_stack_index = z_stack_index;
  current_z_stack_data->behind_z_stack = behind_z_stack;
  current_z_stack_data->stack_words_from_active_routine
    = stack_words_from_active_routine;

  current_z_stack_size = 0;
  z_stack = NULL;
  z_stack_index = NULL;
  behind_z_stack = NULL;
  stack_words_from_active_routine = 0;

  return current_z_stack_data;
}


void delete_stack_container(struct z_stack_container *stack_data)
{
  if (stack_data == NULL)
    return;

  if (stack_data->z_stack != NULL)
    free(stack_data->z_stack);
  free(stack_data);
}


void restore_old_stack(/*@only@*/ struct z_stack_container *old_z_stack_data)
{
  free(z_stack);

  current_z_stack_size = old_z_stack_data->current_z_stack_size;
  z_stack = old_z_stack_data->z_stack;
  z_stack_index = old_z_stack_data->z_stack_index;
  behind_z_stack = old_z_stack_data->behind_z_stack;
  stack_words_from_active_routine
    = old_z_stack_data->stack_words_from_active_routine;

  free(old_z_stack_data);
}


/*
void store_first_stack_frame(bool discard_result,
    uint8_t nof_arguments_supplied, uint32_t return_pc,
    uint8_t result_var_number)
{
  memset((uint8_t*)allocate_z_stack_words(4), 0, 8);
  number_of_stack_frames++;
}
*/


void store_first_stack_frame()
{
  memset((uint8_t*)allocate_z_stack_words(4), 0, 8);
  number_of_stack_frames++;
}


// It's not possible to use a stack in the quetzal format in memory
// directly since we cannot back-step a step-frame without knowing
// how much stack/variable-space was used by the last routine. Thus,
// the following stack format is used:
// --------------------------------------------------
// localvar0 ( <- *local_variable_storage_index before call)
// ...
// localvarn
// stackword0
// ...
// stackwordn
// (routine call, new stack frame starts)
// word1: bits 0-3  : number of local variables in use by last routine
//        bit 4     : discard result
//        bits 8-14 : args to last function call, quetzal-encoded
// word2: bits 0-15 : number of stack words used by last routine
// word3: bits 0-15 : return-PC bits 8-23
// word4: bits 8-15 : return-PC bits 0-7
//        bist 0-7  : result variable number
// localvar0 ( <- *local_variable_storage_index after call)
// ...
// localvarn
// stackword0
// ...
// stackwordn


void store_followup_stack_frame_header(uint8_t number_of_locals,
    bool discard_result, uint8_t nof_arguments_supplied,
    uint16_t stack_words_from_routine, uint32_t return_pc,
    uint8_t result_var_number)
{
  uint8_t argument_mask = 0;
  int i;

  for (i=0; i<nof_arguments_supplied; i++)
  {
    argument_mask <<= 1;
    argument_mask |= 1;
  }

  TRACE_LOG("Storing new stack frame.\n");
  TRACE_LOG("Number of locals: %d.\n", number_of_locals);
  TRACE_LOG("Discard result: %d.\n", discard_result);
  TRACE_LOG("Number of arguments supplied: %d.\n", nof_arguments_supplied);
  TRACE_LOG("Stack words from routine: %d.\n", stack_words_from_routine);
  TRACE_LOG("Return PC: %x.\n", return_pc);
  TRACE_LOG("Result variable number: %d.\n", result_var_number);

  z_stack_push_word(
      (number_of_locals & 0xf)
      |
      (bool_equal(discard_result, true) ? 0x10 : 0)
      |
      (argument_mask << 8) );

  z_stack_push_word(
      stack_words_from_routine);

  z_stack_push_word(
      (return_pc >> 8) & 0xffff);

  z_stack_push_word(
      ((return_pc & 0xff) << 8)
      |
      ((bool_equal(discard_result, true) ? 0 : result_var_number) & 0xff) );
}


void ensure_z_stack_size(uint32_t minimum_size)
{
  while (current_z_stack_size < minimum_size)
    resize_z_stack(Z_STACK_INCREMENT_SIZE);
}


#ifdef ENABLE_TRACING
void dump_stack_to_tracelog()
{
  int i = 0;

  while (i != z_stack_index - z_stack)
  {
    TRACE_LOG("Stack-Dump [%03d]: %x\n", i, z_stack[i]);
    i++;
  }
}
#endif // ENABLE_TRACING

#endif /* stack_c_INCLUDED */

