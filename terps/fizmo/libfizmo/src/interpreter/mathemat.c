
/* mathemat.c
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
 *
 *
 * This file was named "math.h" before but renamed to "mathemat.h" to avoid
 * collisions with the standard math.h include.
 */


#ifndef mathemat_h_INCLUDED
#define mathemat_h_INCLUDED

#include <time.h>
#include <sys/time.h>
#include <time.h>
#include <math.h> // for rint (see above)
#include <stdlib.h> // for [s]random on Mac OS X
#include <string.h>

#include "../tools/tracelog.h"
#include "../tools/i18n.h"
#include "math.h"
#include "config.h"
#include "mt19937ar.h"
#include "variable.h"
#include "zpu.h"
#include "../locales/libfizmo_locales.h"


static int16_t predictable_upper_border = 999;
static int16_t last_predictable_random = 0;
static unsigned uninitialized_unsigned;


void opcode_and(void)
{
  TRACE_LOG("Opcode: AND.\n");
  read_z_result_variable();
  TRACE_LOG("ANDing %x and %x to %x.\n", op[0], op[1], op[0] & op[1]);
  set_variable(z_res_var, op[0] & op[1], false);
}


void opcode_add(void)
{
  TRACE_LOG("Opcode: ADD.\n");
  read_z_result_variable();
  TRACE_LOG("Adding %d and %d.\n", (int16_t)op[0], (int16_t)op[1]);
  set_variable(z_res_var, (uint16_t)(((int16_t)op[0]) + ((int16_t) op[1])),
      false);
}


void opcode_sub(void)
{
  TRACE_LOG("Opcode: SUB.\n");
  read_z_result_variable();
  // (Updates / Clarifications): Opcode operands are always evaluated
  // from first to last -- this order is important when the stack
  // pointer appears as an argument. Thus "@sub sp sp" subtracts the
  // second-from-top stack item from the topmost stack item.
  TRACE_LOG("Subtracting %x from %x.\n", (int16_t)op[0], (int16_t)op[1]);
  set_variable(z_res_var, (uint16_t)(((int16_t)op[0]) - ((int16_t)op[1])),
      false);
}


void opcode_je(void)
{
  uint8_t result = 0;
  uint8_t i;

  TRACE_LOG("Opcode: JE.\n");
  // je can take between 2 and 4 operands. je with just 1 operand
  // is not permitted.

  if (number_of_operands == 1)
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_OPCODE_JE_WITH_ONLY_1_OPERAND_IS_ILLEGAL,
        -1);

  TRACE_LOG("Number of operands for JE: %d.\n", number_of_operands);

  for (i=1; i<number_of_operands; i++)
  {
    TRACE_LOG("Comparing %x and %x.\n", op[0], op[i]);

    if (op[0] == op[i])
    {
      result = 1;
      break;
    }
  }

  evaluate_branch(result == 1 ? (uint8_t)1 : (uint8_t)0);
}


void opcode_jz(void)
{
  TRACE_LOG("Opcode: JZ.\n");
  TRACE_LOG("Checking whether %x is equal zero.\n", op[0]);
  evaluate_branch(op[0] == 0 ? (uint8_t)1 : (uint8_t)0);
}


void opcode_jg(void)
{
  TRACE_LOG("Opcode: JG.\n");
  TRACE_LOG("Jump if %d is greater than %d.\n", (int16_t)op[0], (int16_t)op[1]);
  evaluate_branch(
      ((int16_t)op[0]) > ((int16_t)op[1]) ? (uint8_t)1 : (uint8_t)0);
}


void opcode_inc_chk(void)
{
  int16_t value;

  // FIXME: Is all this signed? I hope so, because JG is singed.

  TRACE_LOG("Opcode: INC_CHK.\n");

  //Indirect variable references
  // In the seven opcodes that take indirect variable references
  // (inc, dec, inc_chk, dec_chk, load, store, pull), an indirect
  // reference to the stack pointer does not push or pull the top
  // item of the stack -- it is read or written in place.

  value = (int16_t)get_variable(op[0], false);

  TRACE_LOG("Incrementing variable with code %d from %d to %d.\n",
      op[0], value, value+1);

  value++;
  set_variable(op[0], (uint16_t)value, false);

  TRACE_LOG("Checking whether %d > %d.\n", value, (int16_t)op[1]);

  evaluate_branch(value > ((int16_t)op[1]) ? (uint8_t)1 : (uint8_t)0);
}


void opcode_test(void)
{
  TRACE_LOG("Opcode: TEST.\n");
  TRACE_LOG("Testing if %x in %x is set.\n", op[0], op[1]);
  evaluate_branch(((op[0] & op[1]) == op[1]) ? (uint8_t)1 : (uint8_t)0);
}


void opcode_jl(void)
{
  // Jump if a < b (using a signed 16-bit comparison).
  TRACE_LOG("Opcode: JL.\n");
  TRACE_LOG("Testing if %i is smaller than %i.\n",
      (int16_t)op[0], (int16_t)op[1]);
  evaluate_branch(
      (((int16_t)op[0]) < ((int16_t)op[1])) ? (uint8_t)1: (uint8_t)0);
}


void opcode_dec_chk(void)
{
  int16_t value;

  // FIXME: Is all this signed? I hope so, because JG is singed.

  TRACE_LOG("Opcode: DEC_CHK.\n");

  value = (int16_t)get_variable(op[0], false);

  TRACE_LOG("Decrementing variable with code %d from %d to %d.\n",
      op[0], value, value-1);

  value--;
  set_variable(op[0], (uint16_t)value, false);

  TRACE_LOG("Checking whether %d < %d.\n", value, (int16_t)op[1]);

  evaluate_branch(value < ((int16_t)op[1]) ? (uint8_t)1 : (uint8_t)0);
}


void opcode_mul(void)
{
  TRACE_LOG("Opcode: MUL.\n");

  read_z_result_variable();

  TRACE_LOG("Multiplying %d and %d.\n", (int16_t)op[0], (int16_t)op[1]);

  set_variable(z_res_var, (uint16_t)(((int16_t)op[0]) * ((int16_t) op[1])),
      false);
}


void seed_random_generator(void)
{
  unsigned long init[RANDOM_SEED_SIZE];
  time_t seconds;
  int i;

  if ((seconds = time(NULL)) == (time_t)-1)
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_FUNCTION_CALL_TIME_RETURNED_NEG_1,
        -1);

  srand(((unsigned)seconds) ^ uninitialized_unsigned);

  // Init parameters are alternately seeded with a pseudo-random value and
  // the uninitialzed stack memory.
  for (i=0; i<RANDOM_SEED_SIZE; i+=2)
    init[i] = (unsigned long)rand();

  init_by_array(init, RANDOM_SEED_SIZE);
}


void opcode_random(void)
{
  unsigned long int random_number;
  double multiplier;
  char *ptr;

  TRACE_LOG("Opcode: RANDOM.\n");

  // If range is positive, returns a uniformly random number between
  // 1 and range. If range is negative, the random number generator
  // is seeded to that value and the return value is 0. Most interpreters
  // consider giving 0 as range illegal (because they attempt a division
  // with remainder by the range), but correct behaviour is to reseed the
  // generator in as random a way as the interpreter can (e.g. by using
  // the time in milliseconds).

  read_z_result_variable();

  if (op[0] == 0)
  {
    seed_random_generator();
  }
  else if ((int16_t)op[0] < 0)
  {
    // If range is negative, the random number generator is seeded to
    // that value and the return value is 0.

    ptr = get_configuration_value("random-mode");
    if (
        (ptr == NULL)
        ||
        (strcmp(ptr, "predictable") != 0)
        ||
        ((int16_t)op[0] <= -1000)
       )
    {
      init_genrand((unsigned long)(int16_t)op[0]);
    }
    else
    {
      predictable_upper_border = abs((int16_t)op[0]);

      TRACE_LOG("Setting predictable_upper_border to %d.\n",
          predictable_upper_border);
    }

    set_variable(z_res_var, 0, false);
  }
  else
  {
    // If range is positive, returns a uniformly random number between 1
    // and range.

    ptr = get_configuration_value("random-mode");
    if (
        (ptr == NULL)
        ||
        (strcmp(ptr, "predictable") != 0)
        ||
        (predictable_upper_border >= 1000)
       )
    {
      TRACE_LOG("Result should be >= 1 and <= %d.\n", op[0]);

      random_number = genrand_int32();

      TRACE_LOG("Random number drawn: %ld.\n", random_number);

      multiplier = (double)(op[0] - 1) / (double)GENRAND_INT32_MAX;
      // Subtract 1 from op[0] since we want to cover the range from 1 to op[0],
      // not 0 to op[0].

      TRACE_LOG("Result: %d.\n",
          ((uint16_t)rint((double)random_number * multiplier)) + 1);

      set_variable(
          z_res_var,
          ((uint16_t)rint((double)random_number * multiplier)) + 1,
          false);
    }
    else
    {
      TRACE_LOG("Random generator in predictable mode.\n");

      last_predictable_random++;

      if (last_predictable_random > predictable_upper_border)
        last_predictable_random = 1;

      TRACE_LOG("Returning random value %d\n", last_predictable_random);

      set_variable(
          z_res_var,
          (uint16_t)last_predictable_random,
          false);
    }
  }
}


void opcode_div(void)
{
  TRACE_LOG("Opcode: DIV.\n");

  read_z_result_variable();

  if (op[1] == 0)
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_CANNOT_DIVIDE_BY_ZERO,
        -1);

  TRACE_LOG("Dividing %d by %d.\n", (int16_t)op[0], (int16_t)op[1]);

  set_variable(z_res_var, (uint16_t)(((int16_t)op[0]) / ((int16_t)op[1])),
      false);
}


void opcode_or(void)
{
  TRACE_LOG("Opcode: OR.\n");
  read_z_result_variable();
  TRACE_LOG("ORing %x and %x to %x.\n", op[0], op[1], op[0] | op[1]);
  set_variable(z_res_var, op[0] | op[1], false);
}


void opcode_mod(void)
{
  TRACE_LOG("Opcode: MOD.\n");

  if (op[1] == 0)
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_CANNOT_DIVIDE_BY_ZERO,
        -1);

  read_z_result_variable();

  TRACE_LOG("MODing %d and %d to %d.\n",
      (int16_t)op[0],
      (int16_t)op[1],
      (uint16_t)((int16_t)op[0] % (int16_t)op[1]));

  set_variable(z_res_var, (uint16_t)((int16_t)op[0] % (int16_t)op[1]), false);
}


void opcode_not(void)
{
  TRACE_LOG("Opcode: NOT.\n");
  read_z_result_variable();
  TRACE_LOG("NOTing %x to %x.\n", op[0], ~op[0]);
  set_variable(z_res_var, ~op[0], false);
}


void opcode_art_shift(void)
{
  int16_t result = (int16_t)op[0];
  int16_t shift_places = (int16_t)op[1];

  TRACE_LOG("Opcode: ART_SHIFT.\n");

  read_z_result_variable();

  /*@-shiftnegative@*/
  /*@-shiftimplementation@*/

  if (shift_places > 0)
    result <<= shift_places;
  else if (shift_places < 0)
    result >>= (-shift_places);

  /*@+shiftimplementation@*/
  /*@+shiftnegative@*/

  set_variable(z_res_var, (uint16_t)result, false);
}


void opcode_log_shift(void)
{
  uint16_t result = (uint16_t)op[0];
  int16_t shift_places = (int16_t)op[1];

  read_z_result_variable();

  if (shift_places > 0)
    result <<= shift_places;
  else if (shift_places < 0)
    result >>= (-shift_places);

  set_variable(z_res_var, result, false);
}


#endif /* mathemat_h_INCLUDED */

