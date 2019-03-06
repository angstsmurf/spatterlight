
/* debugger.c
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2011-2017 Christoph Ender.
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


#ifndef debugger_c_INCLUDED 
#define debugger_c_INCLUDED

#define DEBUGGER_INPUT_BUFFER_SIZE 1024

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "../tools/tracelog.h"
#include "../tools/list.h"
#include "debugger.h"
#include "zpu.h"
#include "streams.h"
#include "text.h"
#include "fizmo.h"
#include "variable.h"
#include "config.h"
#include "stack.h"

#define BUFFER_SIZE 256 // Must not be set below 256.

// Pure PCs are saved until the story is available, meaning until the
// function "debugger_story_has_been_loaded" has been called. From then on,
// breakspoints are stored as pointers relative to z_mem in order to
// speed up searching.
list *pcs = NULL;
list *breakpoints = NULL;
bool story_has_been_loaded = false;
int sockfd;
struct sockaddr_in serv_addr;
int newsockfd = -1;
struct sockaddr_in cli_addr;
socklen_t clilen;
char buffer[BUFFER_SIZE];


static void debugger_output(int socked_fd, char *text)
{
  write(socked_fd, text, strlen(text));
}


void add_breakpoint(uint32_t breakpoint_pc)
{
  static uint32_t *new_element;

  if (story_has_been_loaded == false)
  {
    if (pcs == NULL)
      pcs = create_list();
    new_element = malloc(sizeof(uint32_t));
    *new_element = breakpoint_pc;
    add_list_element(pcs, new_element);
  }
  else
  {
    if (breakpoints == NULL)
      breakpoints = create_list();
    add_list_element(breakpoints, z_mem + breakpoint_pc);
  }
}


void debugger_story_has_been_loaded()
{
  size_t index, len;
  uint32_t *element;
  char prefix_string[] = { FIZMO_COMMAND_PREFIX, 0 };
  int flags;

  //add_breakpoint(0x200d0);
  story_has_been_loaded = true;

  if (pcs != NULL)
  {
    len = get_list_size(pcs);
    breakpoints = create_list();
    for (index=0; index<len; index++)
    {
      element = (uint32_t*)get_list_element(pcs, index);
      // TODO: Verify breakpoints.
      add_list_element(breakpoints, z_mem + *element);
      free(element);
    }
    delete_list(pcs);
    pcs = NULL;
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) 
    exit(-1);
  bzero((char *) &serv_addr, sizeof(struct sockaddr_in));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(DEBUGGER_PORT);
  inet_pton(AF_INET, DEBUGGER_IP_ADDRESS, &serv_addr.sin_addr.s_addr);
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    exit(-1);
  listen(sockfd,5);
  clilen = sizeof(cli_addr);

  streams_latin1_output("\nPlease connect to ");
  streams_latin1_output(DEBUGGER_IP_ADDRESS);
  sprintf(buffer, ":%d", DEBUGGER_PORT);
  streams_latin1_output(buffer);
  streams_latin1_output(" to start.\n");

  while (newsockfd < 0) 
  {
    if ((newsockfd = accept(sockfd, 
          (struct sockaddr *) &cli_addr, 
          &clilen)) >= 0)
      break;
    if (errno != EINTR) 
    {
      perror("accept");
      exit(-1);
    }
  }

  flags = fcntl(newsockfd, F_GETFL, 0);
  fcntl(newsockfd, F_SETFL, flags | O_NONBLOCK);

  debugger_output(newsockfd, "\nFizmo debugger, libfizmo version ");
  debugger_output(newsockfd, LIBFIZMO_VERSION);
  debugger_output(newsockfd, ".\n");
  debugger_output(newsockfd, "Enter \"");
  debugger_output(newsockfd, prefix_string);
  debugger_output(newsockfd, "debug\" as story input to start debugging.\n\n");
}



void do_breakpoint_actions()
{
  if (breakpoints == NULL)
    return;

  if (list_contains_element(breakpoints, pc) == false)
    return;

  //debugger_output("\nReached breakpoint.\n");
  debugger();
}


void debugger()
{
  int n, i;
  fd_set input_set;
  uint8_t dbg_z_instr;
  uint8_t dbg_z_instr_form;
  uint8_t dbg_number_of_operands;
  uint8_t *dbg_pc = pc;

  debugger_output(newsockfd, "\nEntering debugger.\n");

  for(;;)
  {
    parse_opcode(
        &dbg_z_instr,
        &dbg_z_instr_form,
        &dbg_number_of_operands,
        &dbg_pc);
    sprintf(buffer, "\n: %6lx: %d %d %d\n", (long)(pc - z_mem),
        dbg_z_instr, dbg_z_instr_form, dbg_number_of_operands);
    debugger_output(newsockfd, buffer);
    for (i=0; i<number_of_locals_active; i++)
    {
      if (i != 0)
        debugger_output(newsockfd, " ");
      sprintf(buffer, "L%02d:%x", i, local_variable_storage_index[i]);
      debugger_output(newsockfd, buffer);
    }
    debugger_output(newsockfd, "\n# ");

    FD_ZERO(&input_set);
    FD_SET(newsockfd, &input_set);
    while (select(newsockfd+1, &input_set, NULL, NULL, NULL) == -1)
    {
      if (errno != EINTR) 
      {
        debugger_output(newsockfd, "select() socket error.\n");
        perror("select");
        break;
      }
    }

    n = read(newsockfd, buffer, BUFFER_SIZE-1);
    while (n < 0)
    {
      if (errno != EINTR) 
      {
        debugger_output(newsockfd, "read() socket error.\n");
        break;
      }
    }
    if (n > 2)
      buffer[n-2]=0;
    else
      *buffer = 0;

    debugger_output(newsockfd, "\n");
    if ( (strcmp(buffer, "exit") == 0) || (strcmp(buffer, "quit") == 0) )
    {
      break;
    }
    else if (strcmp(buffer, "help") == 0)
    {
      debugger_output(newsockfd, "Valid commands:\n");
      debugger_output(newsockfd, " - stack:       Dump stack contents.\n");
      debugger_output(newsockfd, 
          " - story:       Print story file information.\n");
      debugger_output(newsockfd, " - exit, quit:  Leave debugger.\n");
    }
    else if (strcmp(buffer, "stack") == 0)
    {
      i = 0;
      n = z_stack_index - z_stack;
      while (i < n)
      {
        if ( (i % 8) == 0)
        {
          if (i > 0)
          {
            debugger_output(newsockfd, "\n");
          }
          sprintf(buffer, "%06x:", i);
          debugger_output(newsockfd, buffer);
        }
        sprintf(buffer, " %04x", z_stack[i]);
        debugger_output(newsockfd, buffer);
        i++;
      }
      debugger_output(newsockfd, "\n");
    }
    else if (strcmp(buffer, "story") == 0)
    {
      sprintf(buffer, "Z-Story version: %d.\n", active_z_story->version);
      debugger_output(newsockfd, buffer);
      sprintf(buffer, "Release code: %d.\n", active_z_story->release_code);
      debugger_output(newsockfd, buffer);
      sprintf(buffer, "Serial code: %s.\n", active_z_story->serial_code);
      debugger_output(newsockfd, buffer);
      sprintf(buffer, "Checksum: %d.\n", active_z_story->checksum);
      debugger_output(newsockfd, buffer);
      sprintf(buffer, "Dynamic memory end: $%lx.\n",
          (long)(active_z_story->dynamic_memory_end - z_mem));
      debugger_output(newsockfd, buffer);
      sprintf(buffer, "Static memory end: $%lx.\n",
          (long)(active_z_story->static_memory_end - z_mem));
      debugger_output(newsockfd, buffer);
      sprintf(buffer, "High memory: $%lx.\n",
          (long)(active_z_story->high_memory - z_mem));
      debugger_output(newsockfd, buffer);
      sprintf(buffer, "High memory end: $%lx.\n",
          (long)(active_z_story->high_memory_end - z_mem));
      debugger_output(newsockfd, buffer);
    }
    else
    {
      debugger_output(newsockfd, "Unknown command \"");
      debugger_output(newsockfd, buffer);
      debugger_output(newsockfd, "\".\n");
    }
  }
 
  debugger_output(newsockfd, "Leaving debugger.\n");

  //close(newsockfd);
  return; 
}


void debugger_interpreter_stopped()
{
  int nof_elements;
  uint32_t **elements;
  int i;

  close(newsockfd);
  close(sockfd);

  if (pcs != NULL)
  {
    nof_elements = get_list_size(pcs);
    elements = (uint32_t**)delete_list_and_get_ptrs(pcs);
    for (i=0; i<nof_elements; i++)
      free(elements[i]);
  }

  if (breakpoints != NULL)
  {
    nof_elements = get_list_size(breakpoints);
    elements = (uint32_t**)delete_list_and_get_ptrs(breakpoints);
    for (i=0; i<nof_elements; i++)
      free(elements[i]);
  }
}

#endif /* debugger_c_INCLUDED */

