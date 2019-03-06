
/* blockbuf.c
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


#ifndef blockbuf_c_INCLUDED 
#define blockbuf_c_INCLUDED

#include <stdlib.h>
#include <string.h>

#include "blockbuf.h"
#include "fizmo.h"
#include "../tools/types.h"
#include "../tools/tracelog.h"
#include "../tools/i18n.h"
#include "../tools/z_ucs.h"


BLOCKBUF *create_blockbuffer(z_style default_style, z_font default_font,
    z_colour default_foreground_colour, z_colour default_background_colour)
{
  BLOCKBUF *result;

  TRACE_LOG("New blockbuffer, foreground:%d, background: %d.\n",
      default_foreground_colour, default_background_colour);

  result = (BLOCKBUF*)fizmo_malloc(sizeof(BLOCKBUF));

  result->width = 0;
  result->height = 0;
  result->xpos = 0;
  result->ypos = 0;
  result->content = NULL;
  result->default_style = default_style;
  result->default_font = default_font;
  result->default_foreground_colour = default_foreground_colour;
  result->default_background_colour = default_background_colour;
  result->current_style = default_style;
  result->current_font = default_font;
  result->current_foreground_colour = default_foreground_colour;
  result->current_background_colour = default_background_colour;

  return result;
}


void destroy_blockbuffer(BLOCKBUF *blockbuffer)
{
  if (blockbuffer->content != NULL)
    free(blockbuffer->content);
  free(blockbuffer);
}


void store_z_ucs_output_in_blockbuffer(BLOCKBUF *buffer, z_ucs *z_ucs_output)
{
  struct blockbuf_char *ptr
    = buffer->content + (buffer->ypos * buffer->width) + buffer->xpos;

  TRACE_LOG("Writing to blockbuffer at %d/%d: \"", buffer->xpos, buffer->ypos);
  TRACE_LOG_Z_UCS(z_ucs_output);
  TRACE_LOG("\"\n");

  while (*z_ucs_output != 0)
  {
    if (*z_ucs_output == Z_UCS_NEWLINE)
    {
      buffer->xpos = 0;
      if (buffer->ypos < buffer->height - 1)
      {
        buffer->ypos++;
      }
      ptr = buffer->content + (buffer->ypos * buffer->width) + buffer->xpos;
    }
    else
    {
      if (buffer->xpos >= buffer->width)
      {
        TRACE_LOG("Terminating blockbuffer output prematurely at x:%d.\n",
            buffer->xpos);
        break;
      }

      ptr->character = *z_ucs_output;
      ptr->font = buffer->current_font;
      ptr->style = buffer->current_style;
      ptr->foreground_colour = buffer->current_foreground_colour;
      ptr->background_colour = buffer->current_background_colour;

      ptr++;
      buffer->xpos++;
    }
    z_ucs_output++;
  }
}


void set_blockbuf_cursor(BLOCKBUF *buffer, int x, int y)
{
  TRACE_LOG("Set blockbuffer-cursor to x:%d, y:%d.\n", x, y);

  if (x < 0)
    buffer->xpos = 0;
  else if (x < buffer->width)
    buffer->xpos = x;
  else
    buffer->xpos = buffer->width - 1;

  if (y < 0)
    buffer->ypos = 0;
  else if (y < buffer->height)
    buffer->ypos = y;
  else
    buffer->ypos = buffer->height - 1;
}


void set_blockbuf_style(BLOCKBUF *buffer, z_style style)
{
  TRACE_LOG("Set blockbuffer-style to %d.\n", style);
  buffer->current_style = style;
}


void set_blockbuf_foreground_colour(BLOCKBUF *buffer, z_colour new_colour)
{
  TRACE_LOG("Set blockbuffer-foreground-colour to %d.\n", new_colour);
  buffer->current_foreground_colour = new_colour;
}


void set_blockbuf_background_colour(BLOCKBUF *buffer, z_colour new_colour)
{
  TRACE_LOG("Set blockbuffer-background-colour to %d.\n", new_colour);
  buffer->current_background_colour = new_colour;
}


void set_blockbuf_font(BLOCKBUF *buffer, z_font font)
{
  TRACE_LOG("Set blockbuffer-font to %d.\n", font);
  buffer->current_font= font;
}


// The buffer will only enlarge itself, but never shrink. This is due to
// the fact that many older games are not designed for resizes, and will
// keep printing into the old sized lines. If we only enlarge the buffer
// we'll make sure we don't lose anything, and restoring the screen by
// making the window larger will allow the user to see text the game has
// just printed.
void blockbuf_resize(BLOCKBUF *buffer, int new_width, int new_height)
{
  int x,y;
  size_t new_buffer_size;
  struct blockbuf_char *ptr;

  if (new_width < buffer->width)
  {
    new_width = buffer->width;
  }

  if (new_height < buffer->height)
  {
    new_height = buffer->height;
  }

  if ((new_width == buffer->width) && (new_height == buffer->height))
  {
    return;
  }

  new_buffer_size = sizeof(struct blockbuf_char) * new_width * new_height;

  TRACE_LOG("Resizing blockbuffer to %d*%d (%zd bytes).\n",
      new_width, new_height, new_buffer_size);

  buffer->content = (struct blockbuf_char*)fizmo_realloc(
      buffer->content, new_buffer_size);

  // Realign existing lines.
  if (buffer->width < new_width)
  {
    for (y=buffer->height-1; y>=0; y--)
    {
      TRACE_LOG("Copying %ld bytes from %p to %p (line %d/%d/%d).\n",
          sizeof(struct blockbuf_char) * (buffer->width),
          buffer->content + (y * buffer->width),
          buffer->content + (y * new_width),
          y,
          buffer->width,
          new_width);

      memmove(
          buffer->content + (y * new_width),
          buffer->content + (y * buffer->width),
          sizeof(struct blockbuf_char) * (buffer->width));

      ptr = buffer->content + (y * new_width) + buffer->width;
      for (x=buffer->width; x<new_width; x++)
      {
        TRACE_LOG("Filling x:%d/y:%d with defaults.\n", x, y);

        ptr->character = Z_UCS_SPACE;
        ptr->foreground_colour = buffer->default_foreground_colour;
        ptr->background_colour = buffer->default_background_colour;
        ptr->style = buffer->default_style;
        ptr->font = buffer->default_font;
        ptr++;
      }
    }
  }

  // Fill new lower lines if buffer got larger.
  ptr = buffer->content + (new_width * buffer->height);
  for (y=buffer->height; y<new_height; y++)
  {
    TRACE_LOG("Filling new line %d.\n", y);

    for (x=0; x<new_width; x++)
    {
      TRACE_LOG("Filling x:%d/y:%d with defaults.\n", x, y);

      ptr->character = Z_UCS_SPACE;
      ptr->foreground_colour = buffer->default_foreground_colour;
      ptr->background_colour = buffer->default_background_colour;
      ptr->style = buffer->default_style;
      ptr->font = buffer->default_font;
      ptr++;
    }
  }

  if (new_width > buffer->width) {
    buffer->width = new_width;
  }

  if (new_height > buffer->height) {
    buffer->height = new_height;
  }

  TRACE_LOG("New blockbuffer dimensions: %d * %d.\n",
      buffer->width,  buffer->height);
}


size_t count_allocated_blockbuf_memory(BLOCKBUF *buffer)
{
  return sizeof(BLOCKBUF)
    + sizeof(struct blockbuf_char) * buffer->height * buffer->width;
}

#endif // blockbuf_c_INCLUDED

