
/* routine.h
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


#ifndef routine_h_INCLUDED
#define routine_h_INCLUDED

#include "../tools/types.h"

#ifndef routine_c_INCLUDED
extern int16_t number_of_stack_frames;
#endif /* routine_c_INCLUDED */

void return_from_routine(int16_t result_value);
void call_routine(uint32_t target_routine_address,
    uint8_t result_variable_number, bool discard_result,
    uint8_t number_of_arguments);
void opcode_call(void);
void opcode_ret(void);
void opcode_rfalse(void);
void opcode_rtrue(void);
void opcode_jump(void);
void opcode_ret_popped(void);
void opcode_quit(void);
void opcode_call_2s(void);
void opcode_call_2n(void);
void opcode_call_1s(void);
void opcode_call_1n(void);
void opcode_nop(void);
void opcode_call_vn(void);
void opcode_throw(void);
void opcode_catch(void);
void opcode_call_vs2(void);
void opcode_call_vn2(void);

#endif /* routine_h_INCLUDED */

