
/* output.c
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


#ifndef output_c_INCLUDED
#define output_c_INCLUDED

#include "../tools/tracelog.h"
#include "../tools/types.h"
#include "../tools/i18n.h"
#include "output.h"
#include "fizmo.h"
#include "zpu.h"
#include "variable.h"
#include "config.h"

#ifndef DISABLE_BLOCKBUFFER
#include "blockbuf.h"
#endif //DISABLE_BLOCKBUFFER

#ifndef DISABLE_OUTPUT_HISTORY
#include "history.h"
#endif //DISABLE_OUTPUT_HISTORY


bool lower_window_buffering_active = true;
z_style current_style = Z_STYLE_ROMAN;
z_style upper_window_style = Z_STYLE_ROMAN;
//z_style lower_window_style = Z_STYLE_ROMAN;
z_colour current_foreground_colour;
z_colour current_background_colour;
z_colour default_foreground_colour = Z_COLOUR_WHITE;
z_colour default_background_colour = Z_COLOUR_BLACK;
z_colour upper_window_foreground_colour;
z_colour upper_window_background_colour;
z_colour lower_window_foreground_colour;
z_colour lower_window_background_colour;

// The lower two values are initalized by the init in "fizmo.c".
int16_t active_window_number;
z_font current_font = Z_FONT_NORMAL;
z_font current_resulting_font = Z_FONT_NORMAL;
z_font lower_window_font = Z_FONT_NORMAL;
static int upper_window_height = 0;


void opcode_split_window(void)
{
  TRACE_LOG("Opcode: SPLIT_WINDOW\n");
  TRACE_LOG("Number of upper window lines: %d.\n", op[0]);

  upper_window_height = (int)op[0];

#ifndef DISABLE_BLOCKBUFFER
  if ((ver >= 3) && (ver != 6))
  {
    blockbuf_resize(
        upper_window_buffer,
        (int)active_interface->get_screen_width_in_characters(),
        (int)op[0]);
  }
#endif // DISABLE_BLOCKBUFFER

  active_interface->split_window((int16_t)op[0]);
}


void opcode_set_window(void)
{
  TRACE_LOG("Opcode: SET_WINDOW\n");
  TRACE_LOG("New window number: %d.\n", op[0]);

  if (op[0] == 1) {
    if (
        (upper_window_height == 0)
        &&
        (bool_equal(auto_open_upper_window, true))
       )
    {
      // Zokoban.z5 does set_window(1) without splitting the screen.

      //height = (int)(active_interface->get_screen_height() / 2);
      // -> If the height is larger than 1 or 2, the code above gives
      //    a wrong cursor position after restoring "Lost Pig".

      TRACE_LOG("Opening upper window with 1 line for set_window.\n");

#ifndef DISABLE_BLOCKBUFFER
      blockbuf_resize(
          upper_window_buffer,
          (int)active_interface->get_screen_width_in_characters(),
          1);
#endif // DISABLE_BLOCKBUFFER

      active_interface->split_window((int16_t)1);

      upper_window_height = 1;
    }

#ifndef DISABLE_BLOCKBUFFER
    if (active_window_number != 1) {
      set_blockbuf_cursor(upper_window_buffer, 0, 0);
    }
#endif // DISABLE_BLOCKBUFFER
  }

  active_window_number = (int16_t)op[0];
  active_interface->set_window((int16_t)op[0]);
}


void opcode_erase_window(void)
{
  int window_id = (int16_t)op[0];
  TRACE_LOG("Opcode: ERASE_WINDOW\n");
  TRACE_LOG("Window to erase: %d.\n", (int16_t)op[0]);
  //
  //FIXME: Implement -2

  if (window_id == -1)
  {
    active_interface->split_window(0);
    active_window_number = 0;
    active_interface->set_window(0);
    active_interface->erase_window(0);
    upper_window_height = 0;
  }
  else
    active_interface->erase_window((int16_t)op[0]);
}


void opcode_erase_line_value(void)
{
  TRACE_LOG("Opcode: ERASE_LINE_VALUE\n");
  active_interface->erase_line_value(op[0]);
}


void opcode_erase_line_pixels(void)
{
  TRACE_LOG("Opcode: ERASE_LINE_PIXELS\n");
  active_interface->erase_line_pixels(op[0]);
}


void process_set_cursor(int16_t y, int16_t x, int16_t window)
{
  /*
   * From zmach06e.ps:
   * Flush the buffer and set the cursor for the given window (i.e., window
   * properties 4 and 5) at position (s, x). It is an error in V4-5 to use
   * this instruction when window 0 is selected. [Question: Is the window
   * operand optional or not?] Exception: The first argument can be negative.
   * If it is − 2 (and the second argument is 0), the screen cursor is turned
   * on. If it is − 1, it is turned off; the second argument is ignored in
   * that case.
   */

  TRACE_LOG("Processing set cursor to %d, %d, %d.\n", y, x, window);
  if (ver == 6)
  {
    active_interface->set_cursor(x, y, window);
  }
  else
  {
    if (active_window_number == 1)
    {
#ifndef DISABLE_BLOCKBUFFER
      if (
          (ver >= 3)
          &&
          (upper_window_buffer != NULL)
          &&
          (x >= 1)
          &&
          (y >= 1)
         )
        set_blockbuf_cursor(upper_window_buffer, x-1, y-1);
#endif // DISABLE_BLOCKBUFFER

      if (
          (y >= upper_window_height)
          &&
          (bool_equal(auto_adapt_upper_window, true))
         )
      {
        TRACE_LOG("Resizing upper window to %d lines for cursor movement.\n",y);

#ifndef DISABLE_BLOCKBUFFER
        blockbuf_resize(
            upper_window_buffer,
            (int)active_interface->get_screen_width_in_characters(),
            (int)y);
#endif // DISABLE_BLOCKBUFFER

        active_interface->split_window(y);

        upper_window_height = (int)y;
      }
    }

    active_interface->set_cursor(y, x, window);
  }
}


void opcode_set_cursor(void)
{
  int16_t y = (int16_t)op[0];
  int16_t x = (int16_t)op[1];

  // These two adjustments make "painter.z5" work.
  if (y == 0)
    y = 1;

  if (x == 0)
    x = 1;

  if (ver == 6)
  {
    TRACE_LOG("Opcode: SET_CURSOR_LCW\n");
    process_set_cursor(x, y, op[2]);
  }
  else
  {
    TRACE_LOG("Opcode: SET_CURSOR_LC\n");
    process_set_cursor(y, x, active_window_number);
  }
}


void opcode_get_cursor_array(void)
{
  TRACE_LOG("Opcode: GET_CURSOR\n");
  store_word(z_mem + op[0]    , active_interface->get_cursor_row());
  store_word(z_mem + op[0] + 1, active_interface->get_cursor_column());
}


void opcode_set_text_style(void)
{
  z_style new_z_style;

  TRACE_LOG("Opcode: SET_TEXT_STYLE\n");

  if (ver >= 4)
  {
    TRACE_LOG("Setting text style to %d.\n", op[0]);

    new_z_style = (int16_t)op[0];

    // REVISIT: Implement warning.
    if ( (new_z_style < 0) || (new_z_style > 15) )
      return;

    if (current_style != new_z_style) {

      // Since it's possible to have combinations of styles we'll handle
      // storing metadata and interface updates right here.
#ifndef DISABLE_OUTPUT_HISTORY
      store_metadata_in_history(
          outputhistory[0],
          HISTORY_METADATA_TYPE_STYLE,
          new_z_style);
#endif // DISABLE_OUTPUT_HISTORY
      //lower_window_style = current_style;
      active_interface->set_text_style(new_z_style);
      current_style = new_z_style;
    }
  }
}


void opcode_buffer_mode(void)
{
  TRACE_LOG("Opcode: BUFFER_MODE.\n");
  lower_window_buffering_active = ((int16_t)op[0] != 0 ? true : false);
  active_interface->set_buffer_mode(op[0]);
}


static void process_set_colour_opcode(uint16_t op0, uint16_t op1, uint16_t op2)
{
  z_colour new_foreground_colour;
  z_colour new_background_colour;
  int16_t window_number;

  if (active_interface->is_colour_available() == false)
    return;

  new_foreground_colour = (int16_t)op0;
  new_background_colour = (int16_t)op1;
  window_number = (int16_t)op2;

  // REVISIT: Implement warning
  if ( (new_foreground_colour < -1) || (new_foreground_colour > 12) )
    new_foreground_colour = 0;

  // REVISIT: Implement warning
  if ( (new_background_colour < -1) || (new_background_colour > 12) )
    new_background_colour = 0;

  if (ver == 6)
  {
    if ( (new_foreground_colour < -1) || (new_foreground_colour > 12) )
      return;

    if ( (new_background_colour < -1) || (new_background_colour > 12) )
      return;
  }
  else
  {
    if ( (new_foreground_colour < 0) || (new_foreground_colour > 9) )
      return;

    if ( (new_background_colour < 0) || (new_background_colour > 9) )
      return;
  }

  if (new_foreground_colour == Z_COLOUR_CURRENT)
    new_foreground_colour = current_foreground_colour;
  else if (new_foreground_colour == Z_COLOUR_DEFAULT)
    new_foreground_colour = default_foreground_colour;

  if (new_background_colour == Z_COLOUR_CURRENT)
    new_background_colour = current_background_colour;
  else if (new_background_colour == Z_COLOUR_DEFAULT)
    new_background_colour = default_background_colour;

  current_foreground_colour = new_foreground_colour;
  current_background_colour = new_background_colour;

  TRACE_LOG("evaluated new foreground col: %d.\n", new_foreground_colour);
  TRACE_LOG("evaluated new background col: %d.\n", new_background_colour);

  active_interface->set_colour(
      new_foreground_colour,
      new_background_colour,
      window_number);
}


void opcode_set_colour_fb(void)
{
  TRACE_LOG("Opcode: SET_COLOUR_FB\n");

  TRACE_LOG("fg:%d, bg:%d.\n", op[0], op[1]);
  process_set_colour_opcode(op[0], op[1], (uint16_t)-1);
}


void opcode_set_colour_fbw(void)
{
  TRACE_LOG("Opcode: SET_COLOUR_FBW\n");
  process_set_colour_opcode(op[0], op[1], op[2]);
}


void opcode_set_font(void)
{
  z_font new_z_font;

  // If the requested font is available, then it is chosen for the
  // current window, and the store value is the font ID of the previous
  // font (which is always positive). If the font is unavailable,
  // nothing will happen and the store value is 0.

  // Please note: I consider the "normal" -- code 1 -- and the courier,
  // fixed font -- code 4 -- to be always available. For simple interfaces,
  // these are probably the same. The availability of the character graphics
  // and picture font are read from interface definition.

  TRACE_LOG("Opcode: SET_FONT\n");
  read_z_result_variable();
  new_z_font = (int16_t)op[0];

  // REVISIT: Implement warning.
  if (
      ( (new_z_font < 0) || (new_z_font > 4) )
      ||
      (
       (new_z_font == Z_FONT_PICTURE)
       &&
       (active_interface->is_picture_font_availiable() != true)
      )
      ||
      (
       (new_z_font == Z_FONT_CHARACTER_GRAPHICS)
       &&
       (active_interface->is_character_graphics_font_availiable() != true)
      )
     )
  {
    set_variable(z_res_var, 0, false);
    return;
  }

  /*
  // REVISIT: Is it correct to not double-set fonts?
  if (new_z_font != current_font)
  {
    if (active_window_number == 0)
    {
#ifndef DISABLE_OUTPUT_HISTORY
      store_metadata_in_history(
          outputhistory[0],
          HISTORY_METADATA_TYPE_FONT,
          new_z_font);
#endif // DISABLE_OUTPUT_HISTORY
    }

    else if (active_window_number == 1)
    {
#ifndef DISABLE_BLOCKBUFFER
      if (ver >= 5)
      {
        set_blockbuf_font(upper_window_buffer, new_z_font);
      }
#endif // DISABLE_BLOCKBUFFER
    }

    active_interface->set_font(new_z_font);
    set_variable(z_res_var, (uint16_t)current_font, false);
    current_font = new_z_font;
    TRACE_LOG("New font is: %d\n", current_font);
  }
  else
  {
    set_variable(z_res_var, (uint16_t)current_font, false);
  }
  */

  current_font = new_z_font;
  set_variable(z_res_var, (uint16_t)current_font, false);
}

#endif /* output_c_INCLUDED */

