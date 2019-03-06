
/* cmd_hst.c
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


#ifndef cmd_hst_c_INCLUDED
#define cmd_hst_c_INCLUDED

#define _XOPEN_SOURCE_EXTENDED 1
#define _XOPEN_SOURCE 1
// __STDC_ISO_10646__

#include <stdlib.h>
#include <string.h>

#include "cmd_hst.h"
#include "fizmo.h"
#include "../tools/tracelog.h"
#include "../tools/types.h"
#include "../tools/i18n.h"


// The array "command_history_entries" stores pointer to the latest commands
// entered by the user. In case the user has entered more commands than
// NUMBER_OF_REMEMBERED_COMMANDS, pointers in the array are not copied,
// instead the "start_index" and "end_index" are adjusted accordingly.
static zscii *command_history_entries[NUMBER_OF_REMEMBERED_COMMANDS];

// If wrap_around_is_active == false, start_index == end_index means that
// the buffer is empty, otherwise full.
bool wrap_around_is_active = false;

// "start_index" denotes the position of the oldest stored command.
static unsigned int start_index = 0;

// "end_index" is one step ahead of the latest stored command.
static unsigned int end_index = 0;


void store_command_in_history(zscii *new_command) {
  zscii *ptr = (zscii*)fizmo_strdup((char*)new_command);

  if (start_index == end_index) {
    if (wrap_around_is_active == true) {
      free(command_history_entries[start_index]);
      TRACE_LOG("Storing command %p at %d.\n", ptr, end_index);
      command_history_entries[end_index] = ptr;
      if (++end_index == NUMBER_OF_REMEMBERED_COMMANDS) {
        end_index = 0;
      }
      start_index = end_index;
    }
    else {
      // The buffer ist still empty.
      TRACE_LOG("Storing command %p at 0.\n");
      command_history_entries[0] = ptr;
      end_index = 1;
    }
  }
  else {
    // There's still space to store new commands.
    TRACE_LOG("Storing command %p at %d.\n", ptr, end_index);
    command_history_entries[end_index] = ptr;
    if (++end_index == NUMBER_OF_REMEMBERED_COMMANDS) {
      end_index = 0;
      start_index = 0;
      wrap_around_is_active = true;
    }
  }

  TRACE_LOG("start_index: %d, end_index: %d.\n", start_index, end_index);
}


int get_number_of_stored_commands() {
  return
    wrap_around_is_active == false
    ? end_index - start_index
    : NUMBER_OF_REMEMBERED_COMMANDS;
}


zscii *get_command_from_history(unsigned int command_index) {
  unsigned int history_index;

  TRACE_LOG("requested: %d.\n", command_index);
  TRACE_LOG("stored: %d.\n", get_number_of_stored_commands());

  if (command_index < end_index) {
    history_index = end_index - command_index - 1;
  }
  else if (wrap_around_is_active == true) {
    history_index
      = (NUMBER_OF_REMEMBERED_COMMANDS - (command_index - end_index)) - 1;
  }
  else {
    return NULL;
  }

  TRACE_LOG("history_index: %d.\n", history_index);
  return command_history_entries[history_index];
}

#endif // cmd_hist_c_INCLUDED

