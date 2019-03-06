
/* variable.c
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


#ifndef variable_c_INCLUDED 
#define variable_c_INCLUDED

#include "../tools/tracelog.h"
#include "../tools/i18n.h"
#include "fizmo.h"
#include "stack.h"
#include "zpu.h"
#include "config.h"
#include "../locales/libfizmo_locales.h"


/*@dependent@*/ uint16_t *local_variable_storage_index;
uint8_t number_of_locals_active;


void set_variable(uint8_t variable_number, uint16_t data,
    bool keep_stack_index)
{

  if (variable_number == 0)
  {
    if (stack_words_from_active_routine == MAXIMUM_STACK_ENTRIES_PER_ROUTINE)
      i18n_translate_and_exit(
         libfizmo_module_name,
         i18n_libfizmo_MAXIMUM_NUMBER_OF_STACK_ENTRIES_PER_ROUTINE_P0D_EXCEEDED,
         -1,
         (long int)MAXIMUM_STACK_ENTRIES_PER_ROUTINE);

    if (bool_equal(keep_stack_index, true))
    {
      z_stack_pull_word();
      z_stack_push_word(data);
    }
    else
    {
      z_stack_push_word(data);
      stack_words_from_active_routine++;
    }
  }
  else if (variable_number < 0x10)
  {
    if (variable_number > number_of_locals_active)
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_TRYING_TO_STORE_VARIABLE_L_P0D_BUT_ONLY_P1D_VARIABLES_ACTIVE,
          -1,
         (long int)variable_number-1,
         (long int)number_of_locals_active);

    variable_number--;
    TRACE_LOG("Storing %x in L0%x.\n", data, variable_number);
    local_variable_storage_index[variable_number] = data;
  }
  else
  {
    variable_number -= 0x10;
    TRACE_LOG("Setting global variable G%02x to %x.\n", variable_number, data);
    store_word(
        /*@-nullderef@*/ active_z_story->global_variables /*@-nullderef@*/
        +(variable_number*2),
        data);
  }
}


uint16_t get_variable(uint8_t variable_number, bool keep_stack_index)
{
  uint16_t result;

  if (variable_number == 0)
  {
    if (stack_words_from_active_routine == 0)
    {
      if (bool_equal(skip_active_routines_stack_check_warning, false))
        i18n_translate_and_exit(
            libfizmo_module_name,
            i18n_libfizmo_NOT_ENOUGH_STACK_WORDS_FROM_LOCAL_ROUTINE_ON_STACK,
            -1);
      else
        return 0;
    }

    if (bool_equal(keep_stack_index, true))
      result = z_stack_peek_word();
    else
    {
      result = z_stack_pull_word();
      stack_words_from_active_routine--;
    }
    return result;
  }
  else if (variable_number < 0x10)
  {
    if (variable_number > number_of_locals_active)
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_TRYING_TO_STORE_VARIABLE_L_P0D_BUT_ONLY_P1D_VARIABLES_ACTIVE,
          -1,
          (long int)variable_number-1,
          (long int)number_of_locals_active);

    variable_number--;
    result = local_variable_storage_index[variable_number];
    TRACE_LOG("Reading %x from L0%x.\n", result, variable_number);
    return result;
  }
  else
  {
    variable_number -= 0x10;
    result = load_word(active_z_story->global_variables+(variable_number*2));
    TRACE_LOG("Reading %x from global variable G%02x.\n",
        result, variable_number);
    return result;
  }
}


void opcode_pull(void)
{
  uint16_t value = 0;
  uint16_t spare_slots;
  uint8_t *user_stack;

  TRACE_LOG("Opcode: PULL.\n");

  if (ver == 6)
    (void)read_z_result_variable();

  if ( (ver != 6) || (number_of_operands < 1) )
  {
    if (
        (stack_words_from_active_routine == 0)
        &&
        (bool_equal(skip_active_routines_stack_check_warning, false))
       )
    {
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_NOT_ENOUGH_STACK_WORDS_FROM_LOCAL_ROUTINE_ON_STACK, -1);
    }
    else
    {
      TRACE_LOG("Pulling to variable %x.\n", op[0]);
      value = z_stack_pull_word();
      stack_words_from_active_routine--;
    }
  }
  else
  {
    user_stack = z_mem + (uint16_t)op[0];
    spare_slots = load_word(user_stack);
    spare_slots++;
    value = load_word(user_stack + spare_slots);
    store_word(user_stack, spare_slots);
  }

  if (ver == 6)
    set_variable(z_res_var, value, true);
  else
    set_variable(op[0], value, true);

}


void opcode_push(void)
{
  TRACE_LOG("Opcode: PUSH.\n");
  TRACE_LOG("Pushing %x to stack.\n", op[0]);

  if (stack_words_from_active_routine == MAXIMUM_STACK_ENTRIES_PER_ROUTINE)
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_MAXIMUM_NUMBER_OF_STACK_ENTRIES_PER_ROUTINE_P0D_EXCEEDED,
        -1,
        (long int)MAXIMUM_STACK_ENTRIES_PER_ROUTINE);

  z_stack_push_word(op[0]);
  stack_words_from_active_routine++;
}


void opcode_push_user_stack(void)
{
  uint16_t spare_slots;
  uint8_t *user_stack;

  TRACE_LOG("Opcode: PUSH_USER_STACK.\n");

  (void)read_z_result_variable();
  user_stack = z_mem + (uint16_t)op[1];

  if ((spare_slots = load_word(user_stack)) > 0)
  {
    store_word(user_stack + spare_slots, (uint16_t)op[0]);
    spare_slots--;
    store_word(user_stack, spare_slots);
    evaluate_branch((uint8_t)1);
  }
  else
  {
    evaluate_branch((uint8_t)0);
  }
}


void opcode_store(void)
{
  TRACE_LOG("Opcode: STORE.\n");
  TRACE_LOG("Writing %x to variable %x.\n", op[1], op[0]);

  set_variable(op[0], op[1], true);
}


void opcode_storew(void)
{
  uint8_t *address = z_mem + (uint16_t)(op[0] + ((int16_t)op[1])*2);

  TRACE_LOG("Opcode: STOREW.\n");

  if (address > active_z_story->dynamic_memory_end)
  {
    TRACE_LOG("Trying to storew to %x which is above dynamic memory.\n",
        address);
  }
  else
  {
    TRACE_LOG("Storing %x to %x.\n", op[2], address);
    store_word(z_mem + (uint16_t)(op[0] + ((int16_t)op[1])*2), op[2]);
  }
}


void opcode_loadw(void)
{
  uint16_t value;
  uint8_t *address = z_mem + (uint16_t)(op[0] + ((int16_t)op[1])*2);

  TRACE_LOG("Opcode: LOADW.\n");

  read_z_result_variable();

  if (address > active_z_story->static_memory_end)
  {
    TRACE_LOG("ERROR: Trying to loadw from %x which is above static memory.\n",
        address);
    set_variable(z_res_var, 0, false);
  }
  else
  {
    value = load_word(address);
    TRACE_LOG("Loading %x from %x var %x.\n", value, address, z_res_var);
    set_variable(z_res_var, value, false);
  }
}


void opcode_storeb(void)
{
  uint8_t *address = z_mem + (uint16_t)(op[0] + ((int16_t)op[1]));

  TRACE_LOG("Opcode: STOREB.\n");

  if (address > active_z_story->dynamic_memory_end)
  {
    TRACE_LOG("Trying to storeb to %x which is above dynamic memory.", address);
  }
  else
  {
    TRACE_LOG("Storing %x to %x.\n", op[2], address);
    *(z_mem + (uint16_t)(op[0] + (int16_t)op[1])) = op[2];
  }
}


void opcode_loadb(void)
{
  uint8_t *address = z_mem + (uint16_t)(op[0] + (int16_t)op[1]);

  TRACE_LOG("Opcode: LOADB.\n");

  read_z_result_variable();

  if (address > active_z_story->static_memory_end)
  {
    TRACE_LOG("Static memory end: %x.\n", active_z_story->static_memory_end);
    TRACE_LOG("Trying to loadb from %x which is above static memory.\n",
        address);
    set_variable(z_res_var, 0, false);
  }
  else
  {
    TRACE_LOG("Loading from %x to var %x.\n",
        *address,
        z_res_var);
    set_variable(z_res_var, *address, false);
  }
}


void opcode_inc(void)
{
  int16_t value;
  
  TRACE_LOG("Opcode: INC.\n");
  value = (int16_t)get_variable(op[0], false);
  TRACE_LOG("Incrementing variable %d from %d to %d.\n", op[0], value, value+1);

  set_variable(op[0], (uint16_t)(value + 1), false);
}


void opcode_dec(void)
{
  int16_t value;
  
  TRACE_LOG("Opcode: DEC.\n");
  value = (int16_t)get_variable(op[0], false);
  TRACE_LOG("Decrementing variable %d from %d to %d.\n",
      op[0], value, value-1);

  set_variable(op[0], (uint16_t)(value - 1), false);
}


void opcode_load(void)
{
  TRACE_LOG("Opcode: LOAD.\n");

  read_z_result_variable();
  
  TRACE_LOG("Loading variable with code %d to variable with code %d.\n",
      op[0], z_res_var);

  set_variable(z_res_var, get_variable(op[0], true), false);
}


void opcode_pop(void)
{
  TRACE_LOG("Opcode: POP.\n");

  if (stack_words_from_active_routine == 0)
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_NOT_ENOUGH_STACK_WORDS_FROM_LOCAL_ROUTINE_ON_STACK,
        -1);

  (void)z_stack_pull_word();
  stack_words_from_active_routine--;
}


void opcode_check_arg_count(void)
{
  TRACE_LOG("Opcode: CHECK_ARG_COUNT.\n");

  evaluate_branch(
      (uint8_t)
      ( ((int16_t)op[0]) <= ((int16_t)number_of_locals_from_function_call)
        ? 1 : 0) );
}

#endif /* variable_c_INCLUDED */

