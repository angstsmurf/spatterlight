
/* zpu.h
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


#ifndef zpu_h_INCLUDED
#define zpu_h_INCLUDED

#include <inttypes.h>

#define OPERAND_TYPE_LARGE_CONSTANT 0
#define OPERAND_TYPE_SMALL_CONSTANT 1
#define OPERAND_TYPE_VARIABLE       2

#define INSTRUCTION_2OP   0x00
#define INSTRUCTION_1OP   0x20
#define INSTRUCTION_0OP   0x30
#define INSTRUCTION_VAR   0x40
#define INSTRUCTION_EXT   0x60
#define INSTRUCTION_UNDEF 0xff

#define INTERPRETER_QUIT_NONE 0
#define INTERPRETER_QUIT_ROUTINE 1
#define INTERPRETER_QUIT_ALL 2
#define INTERPRETER_QUIT_SAVE_BEFORE_READ 3
#define INTERPRETER_QUIT_RESTART 4

#ifndef zpu_c_INCLUDED
extern uint8_t *z_mem;
/*@dependent@*/ extern uint8_t *pc;
extern uint16_t op[8];
extern uint8_t number_of_operands;
extern uint8_t number_of_locals_from_function_call;
extern uint8_t z_res_var;
extern uint8_t *current_instruction_location;
extern int zpu_step_number;
extern uint16_t start_interrupt_routine;

// Splint doesn't recognize that "terminate_interpreter" is used by routine.c.
/*@-exportlocal@*/
extern int terminate_interpreter;
/*@+exportlocal@*/

#endif // zpu_c_INCLUDED

void parse_opcode(uint8_t *z_instr, uint8_t *z_instr_form,
    uint8_t *result_number_of_operands, uint8_t **instr_ptr);
void interpret_from_address(uint32_t start_address);
void interpret_resume();
uint16_t interpret_from_call(uint32_t routine_address);
void interpret_from_call_without_result(uint32_t routine_address);
void read_z_result_variable(void);
uint32_t get_packed_routinecall_address(uint16_t packed_address);
uint32_t get_packed_string_address(uint16_t packed_address);
void evaluate_branch(uint8_t test_result);
void parse_branch_bytes(void);
uint16_t load_word(uint8_t *ptr);
void store_word(uint8_t *dest, uint16_t data);
void init_opcode_functions(void);
void dump_stack(void);
void dump_locals(void);

#ifdef ENABLE_TRACING
void dump_dynamic_memory_to_tracelog();
#endif // ENABLE_TRACING

#endif // zpu_h_INCLUDED

