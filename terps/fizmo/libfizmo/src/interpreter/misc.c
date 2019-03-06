
/* misc.c
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


#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>

#include "../tools/tracelog.h"
#include "../tools/i18n.h"
#include "../tools/filesys.h"
#include "misc.h"
#include "fizmo.h"
#include "streams.h"
#include "stack.h"
#include "zpu.h"
#include "routine.h"
#include "variable.h"
#include "../locales/libfizmo_locales.h"

#ifdef ENABLE_DEBUGGER
#include "debugger.h"
#endif // ENABLE_DEBUGGER

#if !defined(__WIN32__)
static struct sigaction fizmo_sigactions;
#endif // defined(__WIN32__)


void opcode_restart(void)
{
  TRACE_LOG("Opcode: RESTART.\n");

  terminate_interpreter = INTERPRETER_QUIT_RESTART;
}


void opcode_verify(void)
{
  uint16_t file_length = (z_mem[0x1a] << 8) | z_mem[0x1b];
  uint16_t header_checksum = (z_mem[0x1c] << 8) | z_mem[0x1d];
  uint16_t calculated_checksum = 0;
  int i, input, scale;

  TRACE_LOG("Opcode: VERIFY.\n");

  if ( (file_length == 0) && (header_checksum == 0) )
    evaluate_branch((uint8_t)1);

  if (ver <= 3)
    scale = 2;
  else if (ver <= 5)
    scale = 4;
  else
    scale = 8;

  if (file_length * scale - 1 >= 0x40)
  {
    if (fsi->setfilepos(
          active_z_story->z_story_file,
          0x40 + active_z_story->story_file_exec_offset,
          SEEK_SET) != 0)
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_FATAL_ERROR_READING_STORY_FILE,
          -1);

    i = 0x40;
    while (i < file_length * scale)
    {
      if ((input = fsi->readchar(active_z_story->z_story_file)) == EOF)
        i18n_translate_and_exit(
            libfizmo_module_name,
            i18n_libfizmo_FATAL_ERROR_READING_STORY_FILE,
            -1);

      calculated_checksum += input;
      i++;
    }
  }

  evaluate_branch(
      calculated_checksum == header_checksum ? (uint8_t)1 : (uint8_t)0);
}


void opcode_piracy(void)
{
  TRACE_LOG("Opcode: PIRACY.\n");

  evaluate_branch(1);
}


void abort_interpreter(int exit_code, z_ucs *error_message)
{
#ifdef THROW_SIGFAULT_ON_ERROR
  int x;
#endif // THROW_SIGFAULT_ON_ERROR

  deactivate_signal_handlers();

#ifdef ENABLE_DEBUGGER
  streams_latin1_output("\nAborting due to:\n");
  streams_z_ucs_output(error_message);
  streams_latin1_output("\n");
  debugger();
  debugger_interpreter_stopped();
#endif // ENABLE_DEBUGGER

  TRACE_LOG("Aborting interpreter.\n");
  if (error_message != NULL)
  {
    TRACE_LOG("Abort with exit message \"");
    TRACE_LOG_Z_UCS(error_message);
    TRACE_LOG("\"\n");
  }

  if (active_sound_interface != NULL)
  {
    TRACE_LOG("Sound interface at %p.\n", active_sound_interface);
    TRACE_LOG("Closing sound interface.\n");
    active_sound_interface->close_sound();
  }

  TRACE_LOG("Closing streams.\n");
  close_streams(error_message);

  // From the OpenGroup: The value of status may be 0, EXIT_SUCCESS,
  // EXIT_FAILURE, [CX] [Option Start]  or any other value, though only
  // the least significant 8 bits (that is, status & 0377) shall be
  // available to a waiting parent process. [Option End]
  //  -> Thus, we print the exit code seperately.
  TRACE_LOG("Output exit code.\n");
  //fprintf(stderr, "Exit code: %d.\n", exit_code);

  //(void)signal(SIGSEGV, SIG_DFL);

  TRACE_LOG("Exit.\n");

#ifdef THROW_SIGFAULT_ON_ERROR
  x = *((int*)NULL);
#endif

#if defined(__WIN32__)
  exit_code = 0;
#endif // !defined(__WIN32__)
  exit(exit_code);
}


#if !defined(__WIN32__)
static void catch_signal_and_abort(int sig_num)
{
  TRACE_LOG("Caught signal %d.\n", sig_num);

  i18n_translate_and_exit(
      libfizmo_module_name,
      i18n_libfizmo_CAUGHT_SIGNAL_P0D_ABORTING_INTERPRETER,
      -1,
      (long)sig_num);
}
#endif // defined(__WIN32__)


void init_signal_handlers(void)
{
#if !defined(__WIN32__)
  TRACE_LOG("Initialiazing signal handlers.\n");

  sigemptyset(&fizmo_sigactions.sa_mask);
  fizmo_sigactions.sa_flags = 0;
  fizmo_sigactions.sa_handler = &catch_signal_and_abort;

  sigaction(SIGSEGV, &fizmo_sigactions, NULL);
  sigaction(SIGTERM, &fizmo_sigactions, NULL);
  sigaction(SIGINT, &fizmo_sigactions, NULL);
  sigaction(SIGQUIT, &fizmo_sigactions, NULL);
  sigaction(SIGBUS, &fizmo_sigactions, NULL);
  sigaction(SIGILL, &fizmo_sigactions, NULL);
#endif // !defined(__WIN32__)
}


void deactivate_signal_handlers(void)
{
#if !defined(__WIN32__)
  TRACE_LOG("Deactivating signal handlers.\n");

  sigemptyset(&fizmo_sigactions.sa_mask);
  fizmo_sigactions.sa_flags = 0;
  fizmo_sigactions.sa_handler = NULL;

  sigaction(SIGSEGV, &fizmo_sigactions, NULL);
  sigaction(SIGTERM, &fizmo_sigactions, NULL);
  sigaction(SIGINT, &fizmo_sigactions, NULL);
  sigaction(SIGQUIT, &fizmo_sigactions, NULL);
  sigaction(SIGBUS, &fizmo_sigactions, NULL);
  sigaction(SIGILL, &fizmo_sigactions, NULL);
#endif // !defined(__WIN32__)
}

