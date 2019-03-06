
/* glk_screen_if.c
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2011-2017 Andrew Plotkin and Christoph Ender.
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

#ifndef glk_screen_if_c_INCLUDED
#define glk_screen_if_c_INCLUDED

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <strings.h>

#include "glk.h"
#include "glkstart.h" /* This comes with the Glk library. */

#include "glk_interface.h"
#include "glk_screen_if.h"

#include <interpreter/fizmo.h>
#include <interpreter/text.h>
#include <interpreter/streams.h>
#include <interpreter/zpu.h>
#include <tools/unused.h>
#include <tools/types.h>
#include <tools/i18n.h>
#include <tools/filesys.h>
#include <tools/tracelog.h>

static char* interface_name = "glk-screen";
static char* interface_version = "0.2.5";

static z_file *(*game_open_interface)(z_file *) = NULL;
static z_file *story_stream = NULL;

static winid_t mainwin = NULL;
static winid_t statusline = NULL;
static winid_t statuswin = NULL;
static bool instatuswin = false;
static int statuscurheight = 0; /* what the VM thinks the height is */
static int statusmaxheight = 0; /* height including possible quote box */
static int statusseenheight = 0; /* last height the user saw */
static int screenestwidth = 0; /* width of screen in status-line characters */
static int screenestheight = 0; /* height of screen in status-line characters */
static int inputbuffer_size = 0;
static glui32 *inputbuffer = NULL;

static void glkint_resolve_status_height(void);
static void glkint_estimate_screen_size(void);
static char *glkint_get_game_id(void);

z_file *glkint_open_interface(z_file *(*game_open_func)(z_file *))
{
  /* Clear all of the static variables. We might be restarting (on iOS)
     so we can't rely on the static initializers. */
  mainwin = NULL;
  statusline = NULL;
  statuswin = NULL;
  instatuswin = false;
  statuscurheight = 0; 
  statusmaxheight = 0;
  statusseenheight = 0;
  screenestwidth = 0;
  screenestheight = 0;
  /* Skip inputbuffer; that's just a malloced block and a size, so it can 
     persist across restarts. */

  /* Set up the game-ID hook. (This is ifdeffed because not all Glk
     libraries have this call.) */
#ifdef GI_DISPA_GAME_ID_AVAILABLE
  gidispatch_set_game_id_hook(&glkint_get_game_id);
#endif /* GI_DISPA_GAME_ID_AVAILABLE */

  /* The awkward nature of iOS autosave-restore means that we need to
     retain a reference to the story_stream, and possibly fix it up
     later on. If this isn't iOS, just ignore this juggling. */
  game_open_interface = game_open_func;
  story_stream = game_open_interface(NULL);
  if (!story_stream) {
    return NULL;
  }

  mainwin = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 1);
  glk_set_window(mainwin);
  instatuswin = false;

  if (1) {
      /* First approximation of the screen size has to be based on the
         story window. When a status window is opened, we'll refine
         this. */
      glui32 width, height;
      glk_window_get_size(mainwin, &width, &height);
      screenestwidth = width;
      screenestheight = height;
  }
  
  return story_stream;
}

char *glkint_get_interface_name()
{ return interface_name; }

bool glkint_return_false()
{ return false; }

bool glkint_return_true()
{ return true; }

uint8_t glkint_return_0()
{ return 0; }

uint8_t glkint_return_1()
{ return 1; }

/* This is called after an autosave-restore (iOS only). We've just
   pulled a new Glk library state from disk. We need to go through it
   and set mainwin, statuswin, etc appropriately.

   Note that at this point, the Glk streams opened by the interpreter
   (the original story file, and a transcript stream if any) have been
   closed. We'll need to update the interpreter with replacements.
 */
void glkint_recover_library_state(library_state_data *dat)
{
  strid_t storystream = NULL;
  strid_t transcriptstream = NULL;
  z_file *transcriptzfile = NULL;
  winid_t win;
  strid_t str;
  glui32 rock;

  mainwin = NULL;
  statusline = NULL;
  statuswin = NULL;
  instatuswin = false;
  zfile_replace_glk_strid(story_stream, NULL);

  /* Close the old transcript stream, if there was one. */
  transcriptzfile = get_stream_2();
  /* This is a little bit fiddly, because the underlying Glk stream is
     already gone. We mark this by removing the pointer to it, and
     then the z_file can be closed safely. */
  if (transcriptzfile) {
    zfile_replace_glk_strid(transcriptzfile, NULL);
    restore_stream_2(NULL);
  }

  statuscurheight = 0;
  statusmaxheight = 0;
  statusseenheight = 0;

  win = NULL;
  while ((win=glk_window_iterate(win, &rock)) != NULL) {
    if (rock == 1)
      mainwin = win;
    else if (rock == 2)
      statuswin = win;
    else if (rock == 3)
      statusline = win;
  }

  if (statuswin) {
    glui32 truewidth, trueheight;
    glk_window_get_size(statuswin, &truewidth, &trueheight);
    statuscurheight = trueheight;
    statusmaxheight = trueheight;
    statusseenheight = trueheight;
  }

  str = NULL;
  while ((str=glk_stream_iterate(str, &rock)) != NULL) {
    if (rock == 1)
      storystream = str;
    else if (rock == 4)
      transcriptstream = str;
  }

  /* Close the old story stream which we found in the library state. (We're
     about to open a fresh version.) */
  if (storystream) {
    glk_stream_close(storystream, NULL);
  }

  game_open_interface(story_stream);

  /* If we found a transcript stream, pass it to the library. */
  if (transcriptstream) {
    transcriptzfile = zfile_from_glk_strid(transcriptstream, NULL,
      FILETYPE_TRANSCRIPT, FILEACCESS_APPEND);
    restore_stream_2(transcriptzfile);
  }

  if (dat && dat->active) {
    statuscurheight = dat->statuscurheight;
    statusmaxheight = dat->statusmaxheight;
    statusseenheight = dat->statusseenheight;
  }

  glkint_estimate_screen_size();
}

/* This is called during an autosave (iOS only). It exports the
 fizmo-specific data which will be needed for the autosave.
 */
void glkint_stash_library_state(library_state_data *dat)
{
    if (dat) {
        dat->active = true;
        dat->statuscurheight = statuscurheight;
        dat->statusmaxheight = statusmaxheight;
        dat->statusseenheight = statusseenheight;
    }
}

uint16_t glkint_get_screen_height()
{
  return screenestheight;
}

uint16_t glkint_get_screen_width()
{
  return screenestwidth;
}

z_colour glkint_get_default_foreground_colour()
{ return Z_COLOUR_BLACK; }

z_colour glkint_get_default_background_colour()
{ return Z_COLOUR_WHITE; }

int glkint_parse_config_parameter(char *UNUSED(key), char *UNUSED(value))
{ return 1; }

char *glkint_get_config_value(char *UNUSED(key))
{
  return NULL;
}

char **glkint_get_config_option_names()
{
  return NULL;
}

void glkint_link_interface_to_story(struct z_story *UNUSED(story))
{
  if (ver <= 3)
  {
    if (statusline) {
      glk_window_close(statusline, NULL);
    }
    statusline = glk_window_open(
        mainwin, winmethod_Above | winmethod_Fixed, 1, wintype_TextGrid, 3);
    /*
    glk_set_window(statusline);
    glk_set_style(style_Normal | stylehint_ReverseColor);
    glk_window_clear(statusline);
    glk_set_window(mainwin);
    */
  }
}

/* Called at @restart time. */
void glkint_reset_interface()
{
  if (statuswin) {
    glk_window_close(statuswin, NULL);
    statuswin = NULL;
  }

  instatuswin = false;
  glk_set_window(mainwin);
  glk_set_style(style_Normal);
  glk_window_clear(mainwin);
  
  /* Leave the screen-size estimates alone. Closing the status window
     doesn't give us new information. */
}

/* This is called from two points: abort_interpreter() with an error message,
   and close_streams() with no error message.

   For the abort case, we call glkint_fatal_error_handler(), which does a
   glk_exit. This is necessary because abort_interpreter() is going to do
   a libc exit(), and we need to get Glk shut down first.

   However, close_streams() is different -- that's the normal interpreter
   exit (a @quit opcode). For that case, it's better to return and do nothing,
   so that close_streams() can flush fizmo's buffers. It will immediately
   exit through the end of fizmo_main(), which is the end of glk_main(),
   so we'll get a normal Glk shut down anyway.
   */
int glkint_close_interface(z_ucs *error_message)
{
  if (error_message)
    glkint_fatal_error_handler(NULL, error_message, NULL, 0, 0);
  return 0;
}

void glkint_set_buffer_mode(uint8_t UNUSED(new_buffer_mode))
{ }

void glkint_interface_output_z_ucs(z_ucs *z_ucs_output)
{
  /* Conveniently, z_ucs is the same as glui32. */
  glk_put_string_uni(z_ucs_output);
}

int16_t glkint_interface_read_line(zscii *dest, uint16_t maximum_length,
    uint16_t tenth_seconds, uint32_t verification_routine,
    uint8_t preloaded_input, int *tenth_seconds_elapsed,
    bool UNUSED(disable_command_history), bool UNUSED(return_on_escape))
{
  int ix;
  zscii zch;
  int i;

  glkint_resolve_status_height();

  if (!inputbuffer) {
    inputbuffer_size = maximum_length+16;
    inputbuffer = malloc(inputbuffer_size * sizeof(glui32));
  }
  else if (maximum_length > inputbuffer_size) {
    inputbuffer_size = maximum_length+16;
    inputbuffer = realloc(inputbuffer, inputbuffer_size * sizeof(glui32));
  }

  for (i=0; i<preloaded_input; i++)
    inputbuffer[i] = (glui32)zscii_input_char_to_z_ucs(dest[i]);
  //input_buffer[i] = 0;

  event_t event;
  int timercount = 0;
  int timed_routine_retval;
  winid_t win = (instatuswin ? statuswin : mainwin);

  if (win) {
    glk_request_line_event_uni(win, inputbuffer, maximum_length,
        preloaded_input);
  }

  if (tenth_seconds) {
    glk_request_timer_events(tenth_seconds * 100);
  }

  while (true) {
    glk_select(&event);

    if (event.type == evtype_LineInput)
      break;

    if (event.type == evtype_Arrange) {
      glkint_estimate_screen_size();
      continue;
    }

    if (event.type == evtype_Timer) {
      timercount += 1;
      timed_routine_retval = interpret_from_call(verification_routine);
      if (timed_routine_retval) {
        glk_request_timer_events(0);
        if (tenth_seconds_elapsed)
          *tenth_seconds_elapsed = (timercount * tenth_seconds);

        if (win) {
          glk_cancel_line_event(win, &event);
        }
        else {
          event.val1 = 0;
        }

        break;
      }
    }
  }

  int count = event.val1;
  count = glk_buffer_to_lower_case_uni(inputbuffer, inputbuffer_size, count);
  for (ix=0; ix<count; ix++) {
    zch = unicode_char_to_zscii_input_char(inputbuffer[ix]);
    dest[ix] = zch;
  }

  return count;
}


int glkint_interface_read_char(uint16_t tenth_seconds,
    uint32_t verification_routine, int *tenth_seconds_elapsed)
{
  event_t event;
  int timercount = 0;
  int timed_routine_retval;
  winid_t win;

  glkint_resolve_status_height();

  win = (instatuswin ? statuswin : mainwin);

  if (win) {
    glk_request_char_event_uni(win);
  }

  if (tenth_seconds) {
    glk_request_timer_events(tenth_seconds * 100);
  }

  while (true) {
    glk_select(&event);

    if (event.type == evtype_CharInput)
      break;

    if (event.type == evtype_Arrange) {
      glkint_estimate_screen_size();
      continue;
    }

    if (event.type == evtype_Timer) {
      timercount += 1;
      timed_routine_retval = interpret_from_call(verification_routine);
      if (timed_routine_retval) {
        glk_request_timer_events(0);
        if (tenth_seconds_elapsed)
          *tenth_seconds_elapsed = (timercount * tenth_seconds);
        return 0;
      }
    }
  }

  if (tenth_seconds) {
    glk_request_timer_events(0);
  }

  glui32 ch = event.val1;
  if (ch >= (0x100000000 - keycode_MAXVAL)) {
    switch (ch) {
      case keycode_Left: return 131;
      case keycode_Right: return 132;
      case keycode_Up: return 129;
      case keycode_Down: return 130;
      case keycode_Delete: return 8;
      case keycode_Return: return 13;
      case keycode_Func1: return 133;
      case keycode_Func2: return 134;
      case keycode_Func3: return 135;
      case keycode_Func4: return 136;
      case keycode_Func5: return 137;
      case keycode_Func6: return 138;
      case keycode_Func7: return 139;
      case keycode_Func8: return 140;
      case keycode_Func9: return 141;
      case keycode_Func10: return 142;
      case keycode_Func11: return 143;
      case keycode_Func12: return 144;
      case keycode_Escape: return 27;
      default: return -1;
    }
  }

  return unicode_char_to_zscii_input_char(ch);
}

void glkint_show_status(z_ucs *room_description,
    int status_line_mode, int16_t parameter1, int16_t parameter2)
{
  char buf[128];

  glk_set_window(statusline);
  glk_window_clear(statusline);
  glk_set_style(style_Subheader);
  glk_put_string_uni(room_description);

  glk_set_style(style_Normal);
  if (status_line_mode == SCORE_MODE_SCORE_AND_TURN)
  {
    snprintf(buf, 128, "  Score: %d, Turns: %d", parameter1, parameter2);
    glk_put_string(buf);
  }
  else if (status_line_mode == SCORE_MODE_TIME)
  {
    snprintf(buf, 128, "%02d:%02d", parameter1, parameter2);
    glk_put_string(buf);
  }

  glk_set_window(mainwin);
}

void glkint_set_text_style(z_style text_style)
{
  if (text_style & Z_STYLE_FIXED_PITCH) {
    glk_set_style(style_Preformatted);
  }
  else if (text_style & Z_STYLE_ITALIC) {
    glk_set_style(style_Emphasized);
  }
  else if (text_style & Z_STYLE_BOLD) {
    glk_set_style(style_Subheader);
  }
  else {
    glk_set_style(style_Normal);
  }
}

void glkint_set_colour(z_colour UNUSED(foreground),
    z_colour UNUSED(background), int16_t UNUSED(window))
{ }

void glkint_set_font(z_font font_type)
{
  if (instatuswin)
    return;
  if (font_type == Z_FONT_COURIER_FIXED_PITCH)
    glk_set_style(style_Preformatted);
  else
    glk_set_style(style_Normal);
}

void glkint_split_window(int16_t nof_lines)
{
  int oldvmheight = statuscurheight;
  statuscurheight = nof_lines;

  /* We do not decrease the height at this time -- it can only increase.
     This ensures that quote boxes don't vanish. */
  if (statuscurheight > statusmaxheight)
    statusmaxheight = statuscurheight;

  /* However, if the VM thinks it's increasing the height, we must be
     careful to clear the "newly created" space. */
  if (statuswin && statuscurheight > oldvmheight) {
    strid_t stream = glk_window_get_stream(statuswin);
    int ix, jx;
    glui32 truewidth, trueheight;
    glk_window_get_size(statuswin, &truewidth, &trueheight);
    for (jx=oldvmheight; jx<(int)trueheight; jx++) {
      glk_window_move_cursor(statuswin, 0, jx);
      for (ix=0; ix<(int)truewidth; ix++) {
        glk_put_char_stream(stream, ' ');
      }
    }
    glk_window_move_cursor(statuswin, 0, 0);
  }

  if (!statuswin) {
    statuswin = glk_window_open(mainwin, winmethod_Above | winmethod_Fixed, statusmaxheight, wintype_TextGrid, 2);
  }
  else {
    glk_window_set_arrangement(glk_window_get_parent(statuswin), winmethod_Above | winmethod_Fixed, statusmaxheight, statuswin);
  }
  
  glkint_estimate_screen_size();
}

/* If the status height is too large because of last turn's quote box,
   shrink it down now. */
static void glkint_resolve_status_height()
{
  if (statusseenheight == statusmaxheight)
    statusmaxheight = statuscurheight;

  if (statuswin) {
    if (statusmaxheight == 0) {
      glk_window_close(statuswin, NULL);
      statuswin = NULL;
    }
    else {
      glk_window_set_arrangement(glk_window_get_parent(statuswin), winmethod_Above | winmethod_Fixed, statusmaxheight, statuswin);
    }
  }

  statusseenheight = statusmaxheight;
  statusmaxheight = statuscurheight;

  glkint_estimate_screen_size();
}

/* Construct an estimate of the current screen size, and pass it to the VM.
 
   The estimate uses the width (in status chars) of the status window --
   that's what's important for 99% of games. For the height, we add
   the number of lines in the status and story windows; this won't be
   perfect (because the fonts are different) but it's close enough.
 */
static void glkint_estimate_screen_size()
{
    glui32 width, height;
    
    if (!statuswin) {
        /* Leave the screen-size estimates alone. Closing the status
           window doesn't give us new information. It's better to keep
           using an old exact measurement of the status window than to
           approximate it in the story window. */
        return;
    }
    
    glk_window_get_size(statuswin, &width, &height);
    screenestwidth = width;
    screenestheight = height;
    
    glk_window_get_size(mainwin, &width, &height);
    screenestheight += height;
    
    fizmo_new_screen_size(screenestwidth, screenestheight);
}

/* 1 is the status window; 0 is the story window. */
void glkint_set_window(int16_t window_number)
{
  if (!window_number) {
    glk_set_window(mainwin);
    instatuswin = false;
  }
  else {
    if (statuswin)
      glk_set_window(statuswin);
    else
      glk_set_window(NULL);
    instatuswin = true;
  }
}

void glkint_erase_window(int16_t window_number)
{
  if (!window_number) {
    glk_window_clear(mainwin);
  }
  else {
    if (statuswin)
      glk_window_clear(statuswin);
  }
}

void glkint_set_cursor(int16_t line, int16_t column,
    int16_t window)
{
  if (window && statuswin)
    glk_window_move_cursor(statuswin, column-1, line-1);
}

/* Glk doesn't support this. */
uint16_t glkint_get_cursor_row()
{ return 0; }

/* Glk doesn't support this. */
uint16_t glkint_get_cursor_column()
{ return 0;}

void glkint_erase_line_value(uint16_t UNUSED(start_position))
{ }

void glkint_erase_line_pixels(uint16_t UNUSED(start_position))
{ }

void glkint_output_interface_info()
{
  (void)streams_latin1_output(interface_name);
  (void)streams_latin1_output(" interface version ");
  (void)streams_latin1_output(interface_version);
  (void)streams_latin1_output("\n");
}

void glkint_game_was_restored_and_history_modified()
{ }

int glkint_prompt_for_filename(char *UNUSED(filename_suggestion),
    z_file **result, char *UNUSED(directory), int filetype, int fileaccess)
{
  frefid_t fileref = NULL;
  strid_t str = NULL;
  glui32 usage, fmode, strrock;

  if (filetype == FILETYPE_SAVEGAME)
  {
    usage = fileusage_SavedGame | fileusage_BinaryMode;
    strrock = 3;
  }
  else if (filetype == FILETYPE_TRANSCRIPT)
  {
    usage = fileusage_Transcript | fileusage_TextMode;
    strrock = 4;
  }
  else if (filetype == FILETYPE_INPUTRECORD)
  {
    usage = fileusage_InputRecord | fileusage_TextMode;
    strrock = 5;
  }
  else if (filetype == FILETYPE_DATA)
  {
    usage = fileusage_Data | fileusage_BinaryMode;
    strrock = 6;
  }
  else if (filetype == FILETYPE_TEXT)
  {
    usage = fileusage_Data | fileusage_TextMode;
    strrock = 7;
  }
  else
    return -1;

  if (fileaccess == FILEACCESS_READ)
    fmode = filemode_Read;
  else if (fileaccess == FILEACCESS_WRITE)
    fmode = filemode_Write;
  else if (fileaccess == FILEACCESS_APPEND)
    fmode = filemode_WriteAppend;
  else
    return -1;

  fileref = glk_fileref_create_by_prompt(usage, fmode, 0);

  if (!fileref)
    return -1;

  str = glk_stream_open_file(fileref, fmode, strrock);
  /* Dispose of the fileref, whether the stream opened successfully
   * or not. */
  glk_fileref_destroy(fileref);

  if (!str)
    return -1;

  *result = zfile_from_glk_strid(
      str, NULL, // cannot fetch filename from fileref?
      filetype, fileaccess);

  return 0;
}

static char *glkint_get_game_id()
{
  /* This buffer gets rewritten on every call, but that's okay -- the caller
     is supposed to copy out the result. */
  static char buf[32];
  char ch;
  int ix;
  int jx;

  if (!z_mem)
    return NULL;

  /* This is computed in a way similar to the game-identifier chunk 
     for the save file. */
  
  jx = 0;

  /* Release number: */
  for (ix=2; ix<4; ix++) {
    ch = z_mem[ix];
    buf[jx++] = (((ch >> 4) & 0x0F) + 'A');
    buf[jx++] = ((ch & 0x0F) + 'A');
  }

  /* Serial number: */
  for (ix=0x12; ix<0x18; ix++) {
    ch = z_mem[ix];
    buf[jx++] = (((ch >> 4) & 0x0F) + 'A');
    buf[jx++] = ((ch & 0x0F) + 'A');
  }

  /* Checksum: */
  for (ix=0x1C; ix<0x1E; ix++) {
    ch = z_mem[ix];
    buf[jx++] = (((ch >> 4) & 0x0F) + 'A');
    buf[jx++] = ((ch & 0x0F) + 'A');
  }

  buf[jx++] = '\0';
  return buf;
}

struct z_screen_interface glkint_screen_interface =
{
  &glkint_get_interface_name,
  &glkint_return_true, /* is_status_line_available */
  &glkint_return_true, /* is_split_screen_available */
  &glkint_return_true, /* is_variable_pitch_font_default */
  &glkint_return_false, /* is_colour_available */
  &glkint_return_false, /* is_picture_displaying_available */
  &glkint_return_true, /* is_bold_face_available */
  &glkint_return_true, /* is_italic_available */
  &glkint_return_true, /* is_fixed_space_font_available */
  &glkint_return_true, /* is_timed_keyboard_input_available */
  &glkint_return_true, /* is_preloaded_input_available */
  &glkint_return_false, /* is_character_graphics_font_availiable */
  &glkint_return_false, /* is_picture_font_availiable */
  &glkint_get_screen_height,
  &glkint_get_screen_width,
  &glkint_get_screen_height,
  &glkint_get_screen_width,
  &glkint_return_1, /* get_font_width_in_units */
  &glkint_return_1, /* get_font_height_in_units */
  &glkint_get_default_foreground_colour,
  &glkint_get_default_background_colour,
  &glkint_return_0, // get_total_width_in_pixels_of_text_sent_to_output_stream_3
  &glkint_parse_config_parameter,
  &glkint_get_config_value,
  &glkint_get_config_option_names,
  &glkint_link_interface_to_story,
  &glkint_reset_interface,
  &glkint_close_interface,
  &glkint_set_buffer_mode,
  &glkint_interface_output_z_ucs,
  &glkint_interface_read_line,
  &glkint_interface_read_char,
  &glkint_show_status,
  &glkint_set_text_style,
  &glkint_set_colour,
  &glkint_set_font,
  &glkint_split_window,
  &glkint_set_window,
  &glkint_erase_window,
  &glkint_set_cursor,
  &glkint_get_cursor_row,
  &glkint_get_cursor_column,
  &glkint_erase_line_value,
  &glkint_erase_line_pixels,
  &glkint_output_interface_info,
  &glkint_return_false, /* input_must_be_repeated_by_story */
  &glkint_game_was_restored_and_history_modified,
  &glkint_prompt_for_filename,
  NULL, /* do_autosave */
  NULL /* restore_autosave */
};

#endif // glk_screen_if_c_INCLUDED

