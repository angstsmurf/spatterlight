
/* stack.h
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


#ifndef stack_h_INCLUDED 
#define stack_h_INCLUDED

#include <inttypes.h>
#include <stdbool.h>

#define MAXIMUM_STACK_ENTRIES_PER_ROUTINE 65535

#ifndef stack_c_INCLUDED 
/*@null@*/ /*@owned@*/ extern uint16_t *z_stack;
extern size_t current_z_stack_size;
/*@null@*/ /*@dependent@*/ extern uint16_t *z_stack_index;
/*@null@*/ /*@dependent@*/ extern uint16_t *behind_z_stack;
extern int stack_words_from_active_routine;
#endif /* stack_c_INCLUDED */

#ifdef ENABLE_TRACING
void dump_stack_to_tracelog();
#endif // ENABLE_TRACING


struct z_stack_container
{
  size_t current_z_stack_size;
  /*@owned@*/ /*@null@*/ uint16_t *z_stack;
  /*@dependent@*/ /*@null@*/ uint16_t *z_stack_index;
  /*@dependent@*/ /*@null@*/ uint16_t *behind_z_stack;
  int stack_words_from_active_routine;
};

void z_stack_push_word(uint16_t data);
uint16_t z_stack_pull_word();
uint16_t z_stack_peek_word();
void drop_z_stack_words(int byte_counter);
/*@only@*/ struct z_stack_container *create_new_stack();
void delete_stack_container(struct z_stack_container *stack_data);
/*@dependent@*/ uint16_t *allocate_z_stack_words(uint32_t byte_counter);
void restore_old_stack(/*@only@*/ struct z_stack_container *old_stack_data);
void store_first_stack_frame();
void store_followup_stack_frame_header(uint8_t number_of_locals,
    bool discard_result, uint8_t nof_arguments_supplied,
    uint16_t stack_words_from_routine, uint32_t return_pc,
    uint8_t result_var_number);
void ensure_z_stack_size(uint32_t minimum_size);

#endif /* stack_h_INCLUDED */

