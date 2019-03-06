
/* blockbuf.h
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


#ifndef blockbuf_h_INCLUDED 
#define blockbuf_h_INCLUDED

#include "../tools/types.h"


struct blockbuf_char
{
  z_ucs character;
  z_font font;
  z_style style;
  z_colour foreground_colour;
  z_colour background_colour;
};


typedef struct 
{
  int width;
  int height;
  int xpos;
  int ypos;

  struct blockbuf_char *content;
  z_style current_style;
  z_font current_font;
  z_colour current_foreground_colour;
  z_colour current_background_colour;

  z_style default_style;
  z_font default_font;
  z_colour default_foreground_colour;
  z_colour default_background_colour;
} BLOCKBUF;


BLOCKBUF *create_blockbuffer(z_style default_style, z_font default_font,
    z_colour default_foreground_colour, z_colour default_background_colour);

void destroy_blockbuffer(BLOCKBUF *blockbuffer);
void store_z_ucs_output_in_blockbuffer(BLOCKBUF *buffer, z_ucs *z_ucs_output);
void set_blockbuf_cursor(BLOCKBUF *buffer, int x, int y);
void set_blockbuf_style(BLOCKBUF *buffer, z_style style);
void set_blockbuf_foreground_colour(BLOCKBUF *buffer, z_colour new_colour);
void set_blockbuf_background_colour(BLOCKBUF *buffer, z_colour new_colour);
void set_blockbuf_font(BLOCKBUF *buffer, z_font font);
void blockbuf_resize(BLOCKBUF *buffer, int new_width, int new_height);
size_t count_allocated_blockbuf_memory(BLOCKBUF *buffer);

#endif // blockbuf_h_INCLUDED

