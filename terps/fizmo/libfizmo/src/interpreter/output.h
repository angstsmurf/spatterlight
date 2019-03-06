
/* output.h
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


#ifndef output_h_INCLUDED
#define output_h_INCLUDED

#define STYLE_ROMAN 0
#define STYLE_REVERSE_VIDEO 1
#define STYLE_BOLD 2
#define STYLE_ITALIC 4
#define STYLE_FIXED_PITCH 8

#ifndef output_c_INCLUDED
extern int16_t active_window_number;
extern z_font current_font;
extern z_font current_resulting_font; // current_font plus 0x11&2-evluation
extern z_style lower_window_font;
extern bool lower_window_buffering_active;
extern z_style current_style;
extern z_style upper_window_style;
extern z_style lower_window_style;
extern z_colour current_foreground_colour;
extern z_colour current_background_colour;
extern z_colour default_foreground_colour;
extern z_colour default_background_colour;
extern z_colour upper_window_foreground_colour;
extern z_colour upper_window_background_colour;
extern z_colour lower_window_foreground_colour;
extern z_colour lower_window_background_colour;
#endif /* output_c_INCLUDED */

void opcode_split_window(void);
void opcode_set_window(void);
void opcode_erase_window(void);
void opcode_erase_line_value(void);
void opcode_erase_line_pixels(void);
void opcode_set_cursor(void);
void opcode_get_cursor_array(void);
void opcode_set_text_style(void);
void opcode_buffer_mode(void);
void opcode_set_colour_fb(void);
void opcode_set_colour_fbw(void);
void opcode_set_font(void);

void process_set_cursor(int16_t y, int16_t x, int16_t window);

#endif /* output_h_INCLUDED */

