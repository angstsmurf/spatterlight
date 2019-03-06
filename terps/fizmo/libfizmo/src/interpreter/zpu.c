
/* zpu.c
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


#ifndef zpu_c_INCLUDED
#define zpu_c_INCLUDED

#include "../tools/tracelog.h"
#include "../tools/i18n.h"
#include "zpu.h"
#include "fizmo.h"
#include "savegame.h"
#include "misc.h"
#include "routine.h"
#include "variable.h"
#include "property.h"
#include "object.h"
#include "mathemat.h"
#include "text.h"
#include "streams.h"
#include "output.h"
#include "sound.h"
#include "stack.h"
#include "table.h"
#include "undo.h"
#include "../locales/libfizmo_locales.h"

#ifdef ENABLE_DEBUGGER
#include "debugger.h"
#endif // ENABLE_DEBUGGER


uint8_t *z_mem;
/*@dependent@*/ uint8_t *pc;
static uint8_t z_instr;
static uint8_t z_instr_form;
uint8_t number_of_operands;
uint16_t op[8];
uint8_t z_res_var;
int terminate_interpreter;
uint8_t number_of_locals_from_function_call;
uint8_t *current_instruction_location;
int zpu_step_number = 0;
//uint16_t start_interrupt_routine = 0;


typedef void /*@null@*/ (*z_opcode_function)(void);
static z_opcode_function z_opcode_functions[0x7d];


void dump_locals()
{
  uint8_t i;

  TRACE_LOG("Locals");

  if (number_of_locals_active != 0)
  {
  TRACE_LOG(" (at stack index %ld): ",
    (long int)(local_variable_storage_index - z_stack));
    for (i=0; i<number_of_locals_active; i++)
    {
#ifdef ENABLE_TRACING
      if (i != 0)
        TRACE_LOG(", ");
#endif /* ENABLE_TRACING */
      TRACE_LOG("L0%d:%x", i, local_variable_storage_index[i]);
    }
  }
  else
  {
    TRACE_LOG(": (Routine has no locals)");
  }
  TRACE_LOG(".\n");
}


void dump_stack(void)
{
  uint16_t *start = z_stack_index - stack_words_from_active_routine;
  uint16_t *index = start;

  TRACE_LOG("Routine's stack contents (at stack index %d): ",
      (int)(z_stack_index - z_stack - stack_words_from_active_routine));

  if (index == z_stack_index)
  {
    TRACE_LOG("(empty)");
  }
  else
    while (index != z_stack_index)
    {
      if (index != start)
      {
        TRACE_LOG(", ");
      }
      TRACE_LOG("%x", *(index++));
    }
  TRACE_LOG(".\n");
}


void parse_opcode(
    uint8_t *z_instr,
    uint8_t *z_instr_form,
    uint8_t *result_number_of_operands,
    uint8_t **instr_ptr)
{
  // uint32_t for "operand_types" to be sure we have at least 18 bits
  // capacity. 16 are used to encode the possible 8 operand types
  // using 2 bits each, and after that 2 more bits ('11') are required
  // to terminate the parser loop.
  uint32_t operand_types;
  uint8_t current_operand_type;
  uint8_t operand_index;
  uint16_t variable_number;
  uint8_t instrbyte0 = *((*instr_ptr)++);
  uint16_t current_operand_value = 0; // compiler complains, init not required

  *z_instr_form = INSTRUCTION_UNDEF;

  TRACE_LOG("Instruction-Byte 0: %x.\n", instrbyte0);

  if ((instrbyte0 & 0xc0) == 0xc0)
  {
    TRACE_LOG("Instruction has variable form.\n");
    *z_instr = instrbyte0 & 0x1f;

    if ((instrbyte0 == 236) || (instrbyte0 == 250))
    {
      // 8 operands, so interpret 2nd operand byte, too.
      *z_instr_form = INSTRUCTION_VAR;
      operand_types  = (*((*instr_ptr)++) << 24);
      operand_types |= (*((*instr_ptr)++) << 16);
      operand_types |= 0xffff;
    }
    else if ((instrbyte0 & 0x20) == 0)
    {
      // 4.3.3 In variable form, if bit 5 is 0 then the count is 2OP; [...]
      *z_instr_form = INSTRUCTION_2OP;
      operand_types = (*((*instr_ptr)++) << 24) | 0xffffff;
    }
    else
    {
      // ... if it is 1, then the count is VAR. The opcode number is given
      // in the bottom 5 bits.
      *z_instr_form = INSTRUCTION_VAR;
      operand_types = (*((*instr_ptr)++) << 24) | 0xffffff;
    }
  }

  else if ((instrbyte0 & 0xc0) == 0x80)
  {
    // 4.3.1 In short form, bits 4 and 5 of the opcode byte give an
    // operand type as above. If this is $11 then the operand count
    // is 0OP; otherwise, 1OP. In either case the opcode number is
    // given in the bottom 4 bits.

    // (except $be extended opcode given in next byte)
    if ((instrbyte0 == 0xbe) && (active_z_story->version >= 5))
    {
      TRACE_LOG("Instruction has extended form.\n");

      *z_instr_form = INSTRUCTION_EXT;
      *z_instr = *((*instr_ptr)++);
      operand_types = (*((*instr_ptr)++) << 24) | 0xffffff;
    }
    else
    {
      TRACE_LOG("Instruction has short form.\n");

      *z_instr = instrbyte0 & 0xf;

      if ((instrbyte0 & 0x30) == 0x30)
      {
        *z_instr_form = INSTRUCTION_0OP;
        operand_types = 0xffffffff;
      }
      else
      {
        *z_instr_form = INSTRUCTION_1OP;
        operand_types = (uint32_t)(0x3fffffff | ((instrbyte0 & 0x30) << 26));
      }
    }
  }

  else
  {
    TRACE_LOG("Instruction has long form.\n");

    // 4.3.2 In long form the operand count is always 2OP. The opcode
    // number is given in the bottom 5 bits.

    *z_instr_form = INSTRUCTION_2OP;
    *z_instr = instrbyte0 & 0x1f;

    // 4.4.2 In long form, bit 6 of the opcode gives the type of the
    // first operand, bit 5 of the second. A value of 0 means a small
    // constant and 1 means a variable. (If a 2OP instruction needs a
    // large constant as operand, then it should be assembled in
    // variable rather than long form.)
    if ((instrbyte0 & 0x40) != 0)
      operand_types = 0x8fffffff;
    else
      operand_types = 0x4fffffff;

    if ((instrbyte0 & 0x20) != 0)
      operand_types |= 0x20000000;
    else
      operand_types |= 0x10000000;
  }

  // Now, somewhere in the if/else lines above, the instruction_form might 
  // not have been set. Just to be sure, we check that here.
  if (*z_instr_form == INSTRUCTION_UNDEF)
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_INSTRUCTION_FORM_NOT_INITIALIZED,
        -1);

  // 4.4.3: Example: $$00101111 means large constant followed by
  // variable (and no third or fourth opcode).

  TRACE_LOG("Parsing Operands by code %x.\n", (unsigned)operand_types);
  operand_index = 0;
  while ((current_operand_type = (operand_types &0xc0000000) >> 30) != 0x3)
  {
    TRACE_LOG("Current Operand code: %x.\n", current_operand_type);
    //current_instruction.operand_type[operand_index] = current_operand;

    if (current_operand_type == OPERAND_TYPE_LARGE_CONSTANT)
    {
      TRACE_LOG("Reading large constant.\n");
      current_operand_value  = (**instr_ptr << 8);
      current_operand_value |= *(++(*instr_ptr));
    }
    else if (current_operand_type == OPERAND_TYPE_SMALL_CONSTANT)
    {
      TRACE_LOG("Reading small constant.\n");
      current_operand_value = **instr_ptr;
    }
    else if (current_operand_type == OPERAND_TYPE_VARIABLE)
    {
      variable_number = **instr_ptr;
      TRACE_LOG("Reading variable with code %x.\n", variable_number);
      current_operand_value = get_variable(variable_number, false);
    }
    else
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_UNKNOWN_OPERAND_TYPE_P0D,
          -1,
          (long int)current_operand_type);

    (*instr_ptr)++;
    TRACE_LOG("op[%d] = %x.\n", operand_index, current_operand_value);
    op[operand_index++] = current_operand_value;

    operand_types <<= 2;
  }

  TRACE_LOG("Opcode code: %x.\n", *z_instr);

  *result_number_of_operands = operand_index;
}


// This function is not called directly, but invoked from a call
// to "interpret_from_address", "interpret_from_call" or from
// "interpret_from_call_without_result".
static void interpret(/*@null@*/ int frame_index_to_quit_on)
{
  z_opcode_function current_z_opcode_function;
  uint16_t start_interrupt_routine;
  //uint16_t start_interrupt_routine_buf;
  //int i;

  /*
  // The lower four "old.*" variables are used to store the state of the
  // interpreter before entering this function. This state is restored when
  // the function ends in order to allow the "parent interpreter level" to
  // continue correctly.
  uint8_t old_number_of_operands;
  uint16_t old_op[7];
  uint8_t old_z_instr;
  uint8_t old_z_instr_form;

  // Store the current interpreter's status.
  old_z_instr = z_instr;
  old_z_instr_form = z_instr_form;
  for (i=0; i<7; i++)
    old_op[i] = op[i];
  old_number_of_operands = number_of_operands;
  number_of_operands = 0;
  */

  if (active_z_story == NULL)
    return;

  TRACE_LOG("Starting interpreting at %lx.\n",
    (unsigned long int)(pc - z_mem));

  if ((frame_index_to_quit_on != -1)
      && (frame_index_to_quit_on >= number_of_stack_frames))
    terminate_interpreter = INTERPRETER_QUIT_ALL;

  while (terminate_interpreter == INTERPRETER_QUIT_NONE)
  {
    /*
    if (start_interrupt_routine != 0)
    {
      TRACE_LOG("\nInvoking sound interrupt routine at %x.\n",
          get_packed_routinecall_address(start_interrupt_routine));

      start_interrupt_routine_buf = start_interrupt_routine;
      start_interrupt_routine = 0;
      interpret_from_call_without_result(
          get_packed_routinecall_address(start_interrupt_routine_buf));
    }
    */

    if (active_sound_interface != NULL)
    {
      if ((start_interrupt_routine
            = active_sound_interface->get_next_sound_end_routine()) != 0)
      {
        TRACE_LOG("\nInvoking sound interrupt routine at %x.\n",
            get_packed_routinecall_address(start_interrupt_routine));

        interpret_from_call_without_result(
            get_packed_routinecall_address(start_interrupt_routine));
      }
    }

    TRACE_LOG("\nPC: %lx.\n", (unsigned long int)(pc - z_mem));

#ifdef ENABLE_DEBUGGER
    do_breakpoint_actions();
#endif // ENABLE_DEBUGGER

#ifdef ENABLE_TRACING
    TRACE_LOG("Step #%d.\n", zpu_step_number);

    dump_locals();
    dump_stack();
    //dump_stack_to_tracelog();
#endif /* ENABLE_TRACING */
    zpu_step_number++;

    // Remember PC for output of warnings and save-on-read.
    current_instruction_location = pc;

    parse_opcode(
        &z_instr,
        &z_instr_form,
        &number_of_operands,
        &pc);

    current_z_opcode_function = z_opcode_functions[z_instr_form + z_instr];

    if (current_z_opcode_function == NULL)
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_OPCODE_P0D_IN_FORM_P1D_NOT_IMPLEMENTED,
          -1,
          (long int)z_instr,
          (long int)z_instr_form);

    current_z_opcode_function();

    if ((frame_index_to_quit_on != -1)
        && (frame_index_to_quit_on >= number_of_stack_frames))
      terminate_interpreter = INTERPRETER_QUIT_ROUTINE;
  }

  TRACE_LOG("End of interpreting, PC now at %lx.\n",
    (unsigned long int)(pc - z_mem));

  /*
  // Restore the interpreter's state as it was before this function was
  // called.
  number_of_operands = old_number_of_operands;
  z_instr = old_z_instr;
  z_instr_form = old_z_instr_form;
  for (i=0; i<7; i++)
    op[i] = old_op[i];
  */

  // In case we quit because of a return to the routine the interpreter
  // has called, we don't have to quit the whole program. Thus, we set
  // the flag to "no-quit" if we "only" quit because of a routine-return.
  if (terminate_interpreter == INTERPRETER_QUIT_ROUTINE)
    terminate_interpreter = INTERPRETER_QUIT_NONE;
}


void interpret_from_address(uint32_t start_address)
{
  store_first_stack_frame();
  number_of_stack_frames = 1;
  pc = z_mem + start_address;
  interpret(-1);
}


void interpret_resume()
{
  // Used for restore-on-start.
  interpret(-1);
}


void interpret_from_call_without_result(uint32_t routine_address)
{
  call_routine(routine_address, 0, true, 0);
  interpret(number_of_stack_frames - 1);
}


uint16_t interpret_from_call(uint32_t routine_address)
{
  call_routine(routine_address, 0, false, 0);
  interpret(number_of_stack_frames - 1);
  return get_variable(0, false);
}


void read_z_result_variable(void)
{
  TRACE_LOG("Reading z_result var from %lx.\n",
    (unsigned long int)((pc - z_mem)));
  z_res_var = *(pc++);
  TRACE_LOG("Current result variable code is %x.\n", z_res_var);
}


// This function is called with the result of a test and evaluates the
// results according to 4.7: Instructions which test a condition are called
// "branch" instructions. The branch information is stored in one or two
// bytes, indicating what to do with the result of the test. If bit 7 of
// the first byte is 0, a branch occurs when the condition was false; if 1,
// then branch is on true. If bit 6 is set, then the branch occupies 1 byte
// only, and the "offset" is in the range 0 to 63, given in the bottom 6
// bits. If bit 6 is clear, then the offset is a signed 14-bit number
// given in bits 0 to 5 of the first byte followed by all 8 of the second.
// An offset of 0 means "return false from the current routine", and 1
// means "return true from the current routine".
// Otherwise, a branch moves execution to the instruction at address
// Address after branch data + Offset - 2.
void evaluate_branch(uint8_t test_result)
{
  uint8_t branchbyte0 = *(pc++);
  int16_t branch_offset;

  if ((branchbyte0 & 0x40) != 0)
  {
    branch_offset = (int16_t)(branchbyte0 & 0x3f);
  }
  else
  {
    branch_offset = (int16_t)(branchbyte0 & 0x1f);
    TRACE_LOG("Branch offset is: $%x.\n", branch_offset);
    /*@-shiftimplementation@*/
    branch_offset <<= 8;
    /*@+shiftimplementation@*/
    TRACE_LOG("Branch offset is: $%x.\n", branch_offset);
    branch_offset |= *(pc++);
    TRACE_LOG("Branch offset is: $%x.\n", branch_offset);

    if ((branchbyte0 & 0x20) != 0)
    {
      branch_offset |= 0xe000;
      TRACE_LOG("Branch offset is: $%x.\n", branch_offset);
    }
  }

  TRACE_LOG("Test result is: %d.\n", test_result);
  TRACE_LOG("Branch offset is: %d.\n", branch_offset);

  if ((((branchbyte0 & 0x80) != 0) && (test_result == 1))
      || (((branchbyte0 & 0x80) == 0) && (test_result == 0)))
  {
    // 4.7.1 An offset of 0 means "return false from the current routine",
    // and 1 means "return true from the current routine".
    if ((branch_offset & 0xfffe) == 0)
    {
      TRACE_LOG("Returning from routine with result: %x\n", branch_offset);
      return_from_routine(branch_offset);
    }
    else
    {
      TRACE_LOG("Branching to: %lx.\n",
        (unsigned long int)(pc + branch_offset - 2 - z_mem));
      pc += branch_offset - 2;
    }
  }
  else
  {
    TRACE_LOG("Not branching.\n");
  }
}


uint32_t get_packed_routinecall_address(uint16_t packed_address)
{
  // 1.2.3 A packed address specifies where a routine or string begins
  // in high memory. Given a packed address P, the formula to obtain
  // the corresponding byte address B is:
  // 2P           Versions 1, 2 and 3
  // 4P           Versions 4 and 5
  // 4P + 8R_O    Versions 6 and 7, for routine calls
  // 4P + 8S_O    Versions 6 and 7, for print_paddr
  // 8P           Version 8

  if (ver <= 3)
    return ((uint32_t)packed_address) * 2;
  else if (ver <= 5)
    return ((uint32_t)packed_address) * 4;
  else if (ver <= 7)
  {
    // We'll assume "get_packed_routinecall_address" is only called when
    // active_z_story != NULL.
    /*@-nullderef@*/
    return ((uint32_t)packed_address) * 4 + active_z_story->routine_offset;
    /*@+nullderef@*/
  }
  else if (ver <= 8)
    return ((uint32_t)packed_address) * 8;

  i18n_translate_and_exit(
      libfizmo_module_name,
      i18n_libfizmo_UNKNOWN_STORY_VERSION_P0D,
      -1,
      ver);

  // To make compiler happy.
  /*@-unreachable@*/
  return 0;
  /*@+unreachable@*/
}


uint32_t get_packed_string_address(uint16_t packed_address)
{
  // 1.2.3 A packed address specifies [...] (see get_packed_routine_address()).

  if (ver <= 3)
  {
    TRACE_LOG("Decoded address: %ui.\n", ((unsigned)packed_address) * 2);
  }
  else if (ver <= 5)
  {
    TRACE_LOG("Decoded address: %ui.\n", ((unsigned)packed_address) * 4);
  }
  else if (ver <= 7)
  {
    // We'll assume "get_packed_string_address" is only called when
    // active_z_story != NULL.
    /*@-nullderef@*/
    TRACE_LOG("Decoded address: %ui.\n",
        (unsigned)((packed_address) * 4 + active_z_story->string_offset));
    /*@+nullderef@*/
  }
  else if (ver <= 8)
  {
    TRACE_LOG("Decoded address: %ui.\n", ((unsigned)packed_address) * 8);
  }

  if (ver <= 3)
    return ((uint32_t)packed_address) * 2;
  else if (ver <= 5)
    return ((uint32_t)packed_address) * 4;
  else if (ver <= 7)
  {
    // We'll assume "get_packed_string_address" is only called when
    // active_z_story != NULL.
    /*@-nullderef@*/
    return ((uint32_t)packed_address) * 4 + active_z_story->string_offset;
    /*@+nullderef@*/
  }
  else if (ver <= 8)
    return ((uint32_t)packed_address) * 8;

  i18n_translate_and_exit(
      libfizmo_module_name,
      i18n_libfizmo_UNKNOWN_STORY_VERSION_P0D,
      -1,
      ver);

  // To make compiler happy.
  /*@-unreachable@*/
  return 0;
  /*@+unreachable@*/
}


uint16_t load_word(uint8_t *ptr)
{
  return (*ptr << 8) | (*(ptr + 1));
}


void store_word(uint8_t *dest, uint16_t data)
{
  *dest = data >> 8;
  *(dest + 1) = data & 0xff;
}


#ifdef ENABLE_TRACING
void dump_dynamic_memory_to_tracelog()
{
  int i = 0;

  while (i != active_z_story->dynamic_memory_end - z_mem)
  {
    TRACE_LOG("Dynamic-Memory-Dump [%05d]: %x\n", i, z_mem[i]);
    i++;
  }
}
#endif // ENABLE_TRACING


void init_opcode_functions(void)
{
  z_opcode_functions[INSTRUCTION_2OP + 0x00]
    = NULL;

  z_opcode_functions[INSTRUCTION_2OP + 0x01]
    = &opcode_je;

  z_opcode_functions[INSTRUCTION_2OP + 0x02]
    = &opcode_jl;

  z_opcode_functions[INSTRUCTION_2OP + 0x03]
    = &opcode_jg;

  z_opcode_functions[INSTRUCTION_2OP + 0x04]
    = &opcode_dec_chk;

  z_opcode_functions[INSTRUCTION_2OP + 0x05]
    = &opcode_inc_chk;

  z_opcode_functions[INSTRUCTION_2OP + 0x06]
    = &opcode_jin;

  z_opcode_functions[INSTRUCTION_2OP + 0x07]
    = &opcode_test;

  z_opcode_functions[INSTRUCTION_2OP + 0x08]
    = &opcode_or;

  z_opcode_functions[INSTRUCTION_2OP + 0x09]
    = &opcode_and;

  z_opcode_functions[INSTRUCTION_2OP + 0x0a]
    = &opcode_test_attr;

  z_opcode_functions[INSTRUCTION_2OP + 0x0b]
    = &opcode_set_attr;

  z_opcode_functions[INSTRUCTION_2OP + 0x0c]
    = &opcode_clear_attr;

  z_opcode_functions[INSTRUCTION_2OP + 0x0d]
    = &opcode_store;

  z_opcode_functions[INSTRUCTION_2OP + 0x0e]
    = &opcode_insert_obj;

  z_opcode_functions[INSTRUCTION_2OP + 0x0f]
    = &opcode_loadw;

  z_opcode_functions[INSTRUCTION_2OP + 0x10]
    = &opcode_loadb;

  z_opcode_functions[INSTRUCTION_2OP + 0x11]
    = &opcode_get_prop;

  z_opcode_functions[INSTRUCTION_2OP + 0x12]
    = &opcode_get_prop_addr;

  z_opcode_functions[INSTRUCTION_2OP + 0x13]
    = &opcode_get_next_prop;

  z_opcode_functions[INSTRUCTION_2OP + 0x14]
    = &opcode_add;

  z_opcode_functions[INSTRUCTION_2OP + 0x15]
    = &opcode_sub;

  z_opcode_functions[INSTRUCTION_2OP + 0x16]
    = &opcode_mul;

  z_opcode_functions[INSTRUCTION_2OP + 0x17]
    = &opcode_div;

  z_opcode_functions[INSTRUCTION_2OP + 0x18]
    = &opcode_mod;

  z_opcode_functions[INSTRUCTION_2OP + 0x19]
    = ver >= 4 ? &opcode_call_2s : NULL;

  z_opcode_functions[INSTRUCTION_2OP + 0x1a]
    = ver >= 5 ? &opcode_call_2n : NULL;

  z_opcode_functions[INSTRUCTION_2OP + 0x1b]
    = ver >= 5
    ? ver == 6 ? &opcode_set_colour_fbw : &opcode_set_colour_fb
    : NULL;

  z_opcode_functions[INSTRUCTION_2OP + 0x1c]
    = ver >= 5 ? &opcode_throw: NULL;

  z_opcode_functions[INSTRUCTION_2OP + 0x1d]
    = NULL;

  z_opcode_functions[INSTRUCTION_2OP + 0x1e]
    = NULL;

  z_opcode_functions[INSTRUCTION_2OP + 0x1f]
    = NULL;



  z_opcode_functions[INSTRUCTION_1OP + 0x00]
    = &opcode_jz;

  z_opcode_functions[INSTRUCTION_1OP + 0x01]
    = &opcode_get_sibling;

  z_opcode_functions[INSTRUCTION_1OP + 0x02]
    = &opcode_get_child;

  z_opcode_functions[INSTRUCTION_1OP + 0x03]
    = &opcode_get_parent;

  z_opcode_functions[INSTRUCTION_1OP + 0x04]
    = &opcode_get_prop_len;

  z_opcode_functions[INSTRUCTION_1OP + 0x05]
    = &opcode_inc;

  z_opcode_functions[INSTRUCTION_1OP + 0x06]
    = &opcode_dec;

  z_opcode_functions[INSTRUCTION_1OP + 0x07]
    = &opcode_print_addr;

  z_opcode_functions[INSTRUCTION_1OP + 0x08]
    = ver >= 4 ? &opcode_call_1s : NULL;

  z_opcode_functions[INSTRUCTION_1OP + 0x09]
    = &opcode_remove_obj;

  z_opcode_functions[INSTRUCTION_1OP + 0x0a]
    = &opcode_print_obj;

  z_opcode_functions[INSTRUCTION_1OP + 0x0b]
    = &opcode_ret;

  z_opcode_functions[INSTRUCTION_1OP + 0x0c]
    = &opcode_jump;

  z_opcode_functions[INSTRUCTION_1OP + 0x0d]
    = &opcode_print_paddr;

  z_opcode_functions[INSTRUCTION_1OP + 0x0e]
    = &opcode_load;

  z_opcode_functions[INSTRUCTION_1OP + 0x0f]
    = ver >= 5 ? &opcode_call_1n : &opcode_not;



  z_opcode_functions[INSTRUCTION_0OP + 0x00]
    = &opcode_rtrue;

  z_opcode_functions[INSTRUCTION_0OP + 0x01]
    = &opcode_rfalse;

  z_opcode_functions[INSTRUCTION_0OP + 0x02]
    = &opcode_print;

  z_opcode_functions[INSTRUCTION_0OP + 0x03]
    = &opcode_print_ret;

  z_opcode_functions[INSTRUCTION_0OP + 0x04]
    = &opcode_nop;

  z_opcode_functions[INSTRUCTION_0OP + 0x05]
    = (ver <= 4) ? &opcode_save_0op : NULL;

  z_opcode_functions[INSTRUCTION_0OP + 0x06]
    = (ver <= 4) ? &opcode_restore_0op : NULL;

  z_opcode_functions[INSTRUCTION_0OP + 0x07]
    = &opcode_restart;

  z_opcode_functions[INSTRUCTION_0OP + 0x08]
    = &opcode_ret_popped;

  z_opcode_functions[INSTRUCTION_0OP + 0x09]
    = ver >= 5 ? &opcode_catch : opcode_pop;

  z_opcode_functions[INSTRUCTION_0OP + 0x0a]
    = &opcode_quit;

  z_opcode_functions[INSTRUCTION_0OP + 0x0b]
    = &opcode_new_line;

  z_opcode_functions[INSTRUCTION_0OP + 0x0c]
    = ver == 3 ? &opcode_show_status : NULL;

  z_opcode_functions[INSTRUCTION_0OP + 0x0d]
    = ver >= 3 ? &opcode_verify : NULL;

  z_opcode_functions[INSTRUCTION_0OP + 0x0e]
    = NULL; // (Extended opcode)

  z_opcode_functions[INSTRUCTION_0OP + 0x0f]
    = ver >= 5 ? &opcode_piracy : NULL;



  z_opcode_functions[INSTRUCTION_VAR + 0x00]
    = &opcode_call;

  z_opcode_functions[INSTRUCTION_VAR + 0x01]
    = &opcode_storew;

  z_opcode_functions[INSTRUCTION_VAR + 0x02]
    = &opcode_storeb;

  z_opcode_functions[INSTRUCTION_VAR + 0x03]
    = &opcode_put_prop;

  z_opcode_functions[INSTRUCTION_VAR + 0x04]
    = &opcode_read;

  z_opcode_functions[INSTRUCTION_VAR + 0x05]
    = &opcode_print_char;

  z_opcode_functions[INSTRUCTION_VAR + 0x06]
    = &opcode_print_num;

  z_opcode_functions[INSTRUCTION_VAR + 0x07]
    = &opcode_random;

  z_opcode_functions[INSTRUCTION_VAR + 0x08]
    = &opcode_push;

  z_opcode_functions[INSTRUCTION_VAR + 0x09]
    = &opcode_pull;

  z_opcode_functions[INSTRUCTION_VAR + 0x0a]
    = ver >= 3 ? &opcode_split_window : NULL;

  z_opcode_functions[INSTRUCTION_VAR + 0x0b]
    = ver >= 3 ? &opcode_set_window : NULL;

  z_opcode_functions[INSTRUCTION_VAR + 0x0c]
    = ver >= 4 ? &opcode_call_vs2 : NULL;

  z_opcode_functions[INSTRUCTION_VAR + 0x0d]
    = ver >= 4 ? &opcode_erase_window : NULL;

  z_opcode_functions[INSTRUCTION_VAR + 0x0e]
    = ver >= 4
    ? ver == 6 ? &opcode_erase_line_pixels : &opcode_erase_line_value
    : NULL;

  z_opcode_functions[INSTRUCTION_VAR + 0x0f]
    = ver >= 4
    ? &opcode_set_cursor
    : NULL;

  z_opcode_functions[INSTRUCTION_VAR + 0x10]
    = ver >= 4 ? &opcode_get_cursor_array : NULL;

  z_opcode_functions[INSTRUCTION_VAR + 0x11]
    = ver >= 4 ? &opcode_set_text_style : NULL;

  z_opcode_functions[INSTRUCTION_VAR + 0x12]
    = &opcode_buffer_mode;

  z_opcode_functions[INSTRUCTION_VAR + 0x13]
    = &opcode_output_stream;

  z_opcode_functions[INSTRUCTION_VAR + 0x14]
    = ver >= 3 ? &opcode_input_stream : NULL;

  z_opcode_functions[INSTRUCTION_VAR + 0x15]
    = ver >= 3 ? &opcode_sound_effect : NULL;

  z_opcode_functions[INSTRUCTION_VAR + 0x16]
    = &opcode_read_char;

  z_opcode_functions[INSTRUCTION_VAR + 0x17]
    = &opcode_scan_table;

  z_opcode_functions[INSTRUCTION_VAR + 0x18]
    = ver >= 5 ? &opcode_not : NULL;

  z_opcode_functions[INSTRUCTION_VAR + 0x19]
    = &opcode_call_vn;

  z_opcode_functions[INSTRUCTION_VAR + 0x1a]
    = ver >= 5 ? &opcode_call_vn2 : NULL;

  z_opcode_functions[INSTRUCTION_VAR + 0x1b]
    = ver >= 5 ? &opcode_tokenise : NULL;

  z_opcode_functions[INSTRUCTION_VAR + 0x1c]
    = (ver >= 5) ? opcode_encode_text : NULL;

  z_opcode_functions[INSTRUCTION_VAR + 0x1d]
    = opcode_copy_table;

  z_opcode_functions[INSTRUCTION_VAR + 0x1e]
    = opcode_print_table;

  z_opcode_functions[INSTRUCTION_VAR + 0x1f]
    = opcode_check_arg_count;



  z_opcode_functions[INSTRUCTION_EXT + 0x00]
    = ver >=5 ? &opcode_save_ext : NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x01]
    = ver >=5 ? &opcode_restore_ext : NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x02]
    = ver >= 5 ? &opcode_log_shift : NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x03]
    = ver >= 5 ? &opcode_art_shift : NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x04]
    = ver >= 4 ? &opcode_set_font : NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x05]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x06]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x07]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x08]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x09]
    = ver >= 5 ? &opcode_save_undo : NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x0a]
    = ver >= 5 ? &opcode_restore_undo : NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x0b]
    = ver >= 5 ? &opcode_print_unicode : NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x0c]
    = ver >= 5 ? &opcode_check_unicode : NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x0d]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x0e]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x0f]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x0f]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x10]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x11]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x12]
    = opcode_push_user_stack;

  z_opcode_functions[INSTRUCTION_EXT + 0x13]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x14]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x15]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x16]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x17]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x18]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x19]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x1a]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x1b]
    = NULL;

  z_opcode_functions[INSTRUCTION_EXT + 0x1c]
    = NULL;
}

#endif // zpu_c_INCLUDED

