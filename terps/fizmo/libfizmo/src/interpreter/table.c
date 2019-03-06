
/* table.c
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


#include <stdlib.h>
#include <string.h>

#include "../tools/tracelog.h"
#include "../tools/i18n.h"
#include "table.h"
#include "zpu.h"
#include "variable.h"


void opcode_scan_table(void)
{
  // scan_table <needle> <haystack> <haystacksize> [form]
  uint16_t entry_size;
  uint16_t index;
  bool scan_words;
  bool entry_found = false;

  TRACE_LOG("Opcode: SCAN_TABLE.\n");

  if (number_of_operands != 4)
  {
    entry_size = 2;
    scan_words = true;
  }
  else
  {
    entry_size = ((uint16_t)op[3]) & 0x7f;
    scan_words = ((op[3] & 0x80) != 0) ? true : false;
  }

  TRACE_LOG("Scanning table at %d/%d for %d in mode %d with entry size %d.\n",
      (uint16_t)op[1],
      (uint16_t)op[2],
      (uint16_t)op[0],
      scan_words,
      entry_size);

  index = op[1];
  while (index < op[1] + ((uint16_t)op[2]*entry_size))
  {
    if (bool_equal(scan_words, true))
    {
      TRACE_LOG("Found %d at %d.\n", load_word(z_mem + index), index);
      if (load_word(z_mem + index) == (uint16_t)op[0])
      {
        entry_found = true;
        break;
      }
    }
    else
    {
      if (z_mem[index] == (uint16_t)op[0])
      {
        entry_found = true;
        break;
      }
    }

    index += entry_size;
  }

  TRACE_LOG("Scan result: %d.\n", entry_found);

  read_z_result_variable();

  if (bool_equal(entry_found, true))
  {
    set_variable(z_res_var, index, false);
    evaluate_branch(1);
  }
  else
  {
    set_variable(z_res_var, 0, false);
    evaluate_branch(0);
  }
}


void opcode_copy_table(void)
{
  int size =(int16_t)op[2];
  uint8_t *src, *dest, *last;

  TRACE_LOG("Opcode: COPY_TABLE.\n");
  if (op[1] == 0)
  {
    if (size > 0)
    {
      TRACE_LOG("Zeroing first %d bytes from %ud.\n", size, op[0]);
      memset(z_mem + op[0], 0, size);
    }
  }
  else
  {
    src = z_mem + op[0];
    dest  = z_mem + op[1];

    if ( (size < 0) || (op[0] > op[1]) )
    {
      size = abs(size);
      TRACE_LOG("Forward-copying %u bytes from %u to %u.\n",
          size, op[0], op[1]);

      last = src + size - 1;

      while (src <= last)
      {
        //TRACE_LOG("%c from %d to %d.\n", *src, src-z_mem, dest-z_mem);
        *dest = *src;
        src++;
        dest++;
      }
    }
    else
    {
      TRACE_LOG("Backward-copying %u bytes from %u to %u.\n",
          size, op[0], op[1]);

      last = src;
      src += (size - 1);
      dest += (size - 1);

      while (src >= last)
      {
        //TRACE_LOG("%c from %d to %d.\n", *src, src-z_mem, dest-z_mem);
        *dest = *src;
        src--;
        dest--;
      }
    }
  }
}

