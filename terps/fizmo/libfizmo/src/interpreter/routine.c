
/* routine.c
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


#ifndef routine_c_INCLUDED
#define routine_c_INCLUDED

#include <string.h>

#include "../tools/tracelog.h"
#include "../tools/i18n.h"
#include "routine.h"
#include "stack.h"
#include "variable.h"
#include "zpu.h"
#include "fizmo.h"
#include "../locales/libfizmo_locales.h"


int16_t number_of_stack_frames = 0;
static int last_result_var;


static void unwind_stack_frame(int16_t result_value, bool force_discard_result)
{
  uint16_t stack_word;
  bool discard_result;
  uint8_t argument_mask;

  TRACE_LOG("Stack pointer: %ld.\n", (long int)(z_stack_index - z_stack));
  TRACE_LOG("Dropping stack (%d words) and locals (%d words).\n",
      (unsigned)stack_words_from_active_routine,
      (unsigned)number_of_locals_active);

  // First, we drop the active routine's stack contents and the local
  // variables.
  drop_z_stack_words(
      (int)(stack_words_from_active_routine + number_of_locals_active));
  number_of_stack_frames--;
  TRACE_LOG("Number of stack frames: %d.\n", number_of_stack_frames);

  stack_word = z_stack_pull_word();

  last_result_var = stack_word & 0xff;

  pc = (z_mem + ((z_stack_pull_word() << 8) | (stack_word >> 8)));

  stack_words_from_active_routine = (int)z_stack_pull_word();
  TRACE_LOG("Stack word in this routine: %d.\n",
      (int)stack_words_from_active_routine);

  stack_word = z_stack_pull_word();

  number_of_locals_active = stack_word & 0xf;
  discard_result = ((stack_word & 0x10) != 0 ? true : false);
  argument_mask = (stack_word >> 8);

  number_of_locals_from_function_call = 0;
  while (argument_mask != 0)
  {
    number_of_locals_from_function_call++;
    argument_mask >>= 1;
  }

  TRACE_LOG("Locals active in this routine: %d.\n",
      number_of_locals_active);

  local_variable_storage_index
    = z_stack_index
    - stack_words_from_active_routine
    - number_of_locals_active;

  TRACE_LOG("Locals from function call: %d.\n",
      number_of_locals_from_function_call);

  TRACE_LOG("Returning to %lx.\n", (unsigned long int)(pc - z_mem));

  if ( 
      (bool_equal(discard_result, false))
      &&
      (bool_equal(force_discard_result, false))
     )
  {
    TRACE_LOG("Storing routine call result %x in variable code %x.\n",
        result_value, last_result_var);

    set_variable(last_result_var, (uint16_t)result_value, false);
  }
}


void return_from_routine(int16_t result_value)
{
  unwind_stack_frame(result_value, false);
}


void call_routine(
    uint32_t target_routine_address,
    uint8_t result_variable_number,
    bool discard_result,
    uint8_t number_of_arguments)
{
  uint8_t i;

#ifdef ENABLE_TRACING
  TRACE_LOG("Calling routine at %x.\n", (unsigned)target_routine_address);

  if (discard_result != 0)
  {
    TRACE_LOG("Result will be discarded.\n");
  }
  else
    TRACE_LOG("Result will be stored in variable code %x.\n",
        result_variable_number);
#endif /* ENABLE_TRACING */

  if (target_routine_address == 0)
  {
    // When the address 0 is called as a routine, nothing happens and
    // the return value is false

    // 6.4.5: The return value of a routine can be any Z-machine number.
    // Returning 'false' means returning 0; returning 'true' means
    // returning 1.

    TRACE_LOG("Target address is 0, returning false(0).\n");

    if (discard_result == 0)
      set_variable(result_variable_number, 0, false);
    return;
  }

  TRACE_LOG("Return address: %lx.\n",
    (unsigned long int)(pc - z_mem));

  store_followup_stack_frame_header(
      number_of_locals_active,
      discard_result,
      number_of_locals_from_function_call,
      stack_words_from_active_routine,
      pc - z_mem,
      result_variable_number);

  pc = z_mem + target_routine_address;

  number_of_locals_active = *(pc++);

  if (number_of_locals_active > 15)
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_MORE_THAN_15_LOCALS_ARE_NOT_ALLOWED,
        -1);

  TRACE_LOG("Routine has %d locals.\n", number_of_locals_active);

  local_variable_storage_index
    = allocate_z_stack_words(number_of_locals_active);

  // For Z-Machines version <= 4, the contens of the local variables are
  // defined by the following bytes which can be copied over directly.
  // For version >= 5, the initial values are all set to zero.
  if (ver <= 4)
  {
    // memcpy usage poses certain problem when used on a little endian
    // machine (i386 and the like). Thus, in order to avoid all the
    // fuss with little/big-endian detection, i'll not use memcpy
    // but copy stuff by hand.
    //memcpy((uint8_t*)local_variable_storage_index, pc,
    //number_of_locals_active*2);

    for (i=0; i<number_of_locals_active; i++)
    {
      local_variable_storage_index[i] = load_word(pc);
      pc += 2;
    }
  }
  else
    memset(local_variable_storage_index, 0, number_of_locals_active*2);

  TRACE_LOG("Number of call arguments: %d.\n", number_of_arguments);

  for (i=0; i<number_of_arguments; i++)
  {
    TRACE_LOG("Storing argument value %x at local variable L0%x.\n",op[i+1],i);
    local_variable_storage_index[i] = op[i+1];
  }

  stack_words_from_active_routine = 0;
  number_of_locals_from_function_call = number_of_arguments;
  number_of_stack_frames++;

  TRACE_LOG("Stack index: %ld.\n",
    (long int)(z_stack_index - z_stack));
  TRACE_LOG("Number of stack frames: %d.\n", number_of_stack_frames);
}


void opcode_call(void)
{
  TRACE_LOG("Opcode: CALL_ROUTINE.\n");
  read_z_result_variable();
  call_routine(
      get_packed_routinecall_address(op[0]),
      z_res_var,
      false,
      number_of_operands - 1);
}


void opcode_ret(void)
{
  TRACE_LOG("Opcode: RET.\n");
  TRACE_LOG("Returning: %x.\n", op[0]);
  return_from_routine((int16_t)op[0]);
}


void opcode_rfalse(void)
{
  TRACE_LOG("Opcode: RFALSE.\n");
  TRACE_LOG("Returning: 0.\n");
  // 4.7.1 An offset of 0 means "return false from the current routine",
  // and 1 means "return true from the current routine".
  return_from_routine((int16_t)0);
}


void opcode_rtrue(void)
{
  TRACE_LOG("Opcode: RTRUE.\n");
  TRACE_LOG("Returning: 1.\n");
  return_from_routine((int16_t)1);
}


void opcode_jump(void)
{
  int16_t offset = (int16_t)op[0];

  TRACE_LOG("Opcode: JUMP.\n");
  TRACE_LOG("Jumping to %lx + %i - 2 = %lx.\n",
      (unsigned long int)(pc - z_mem),
      offset,
      (unsigned long int)((pc - z_mem) + offset - 2));

  pc += offset - 2;
}


void opcode_ret_popped(void)
{
  TRACE_LOG("Opcode: RET_POPPED.\n");

  return_from_routine((int16_t)get_variable(0, false));
}


void opcode_quit(void)
{
  TRACE_LOG("Opcode: QUIT.\n");

  terminate_interpreter = INTERPRETER_QUIT_ALL;
}


void opcode_call_2s(void)
{
  TRACE_LOG("Opcode: CALL_2S.\n");
  read_z_result_variable();
  call_routine(
      get_packed_routinecall_address(op[0]),
      z_res_var,
      false,
      number_of_operands - 1);
}


void opcode_call_2n(void)
{
  TRACE_LOG("Opcode: CALL_2N.\n");
  call_routine(
      get_packed_routinecall_address(op[0]),
      0,
      true,
      number_of_operands - 1);
}


void opcode_call_1n(void)
{
  TRACE_LOG("Opcode: CALL_1N.\n");
  call_routine(
      get_packed_routinecall_address(op[0]),
      0,
      true,
      number_of_operands - 1);
}


void opcode_call_1s(void)
{
  TRACE_LOG("Opcode: CALL_1S.\n");
  read_z_result_variable();
  call_routine(
      get_packed_routinecall_address(op[0]),
      z_res_var,
      false,
      number_of_operands - 1);
}


void opcode_nop(void)
{
  TRACE_LOG("Opcode: NOP.\n");
}


void opcode_call_vn(void)
{
  TRACE_LOG("Opcode: CALL_VN.\n");
  call_routine(
      get_packed_routinecall_address(op[0]),
      0,
      true,
      number_of_operands - 1);
}


void opcode_throw(void)
{
  int16_t dest_stack_frame = (int16_t)op[1];

  TRACE_LOG("Opcode: THROW.\n");

  if ( (dest_stack_frame > number_of_stack_frames) || (dest_stack_frame < 1) )
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_INVALID_THROW_DESTINATION_STACK_INDEX_P0D,
        -1,
        (long)dest_stack_frame);

  while (number_of_stack_frames >= dest_stack_frame)
    unwind_stack_frame(0, true);

  set_variable(last_result_var, op[0], false);
}


void opcode_catch(void)
{
  TRACE_LOG("Opcode: CATCH.\n");
  read_z_result_variable();
  set_variable(z_res_var, (uint16_t)number_of_stack_frames, false);
}


void opcode_call_vs2(void)
{
  TRACE_LOG("Opcode: CALL_VS2.\n");
  read_z_result_variable();
  call_routine(
      get_packed_routinecall_address(op[0]),
      z_res_var,
      false,
      number_of_operands - 1);
}


void opcode_call_vn2(void)
{
  TRACE_LOG("Opcode: CALL_VN2.\n");
  call_routine(
      get_packed_routinecall_address(op[0]),
      0,
      true,
      number_of_operands - 1);
}

#endif /* routine_c_INCLUDED */

