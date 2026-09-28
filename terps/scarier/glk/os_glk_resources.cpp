/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * os_glk_resources.cpp: ADRIFT <=4 sound and graphics resources, and the
 * title window.  Split out of os_glk.cpp; see os_glk_internal.h.
 */

#include "os_glk_internal.h"

/*---------------------------------------------------------------------*/
/*  Glk resource handling functions                                    */
/*---------------------------------------------------------------------*/

#ifdef GLK_MODULE_GARGLK_FILE_RESOURCES
schanid_t sound_channel;

void
os_play_sound (const scr_char *filepath,
               scr_int offset, scr_int length, scr_bool is_looping)
{
  if (sound_channel == NULL) {
    sound_channel = glk_schannel_create(0);
  }

  if (sound_channel == NULL) {
    return;
  }

  glui32 id = garglk_add_resource_from_file(giblorb_ID_Snd, gamefile, offset, length);
  if (id != 0) {
    glk_schannel_play_ext(sound_channel, id, is_looping ? 0xffffffff : 1, 0);
  }
}

void
os_stop_sound (void)
{
  if (sound_channel != NULL) {
    glk_schannel_stop(sound_channel);
  }
}
#elif defined(SPATTERLIGHT)
/*
 * Spatterlight resource handling.  Pre-load a chunk of the game file into
 * the Spatterlight Glk image/sound cache via win_loadimage/win_loadsound
 * and then dispatch through the standard Glk drawing/playing routines.
 */
extern "C" char *gli_game_path;
extern "C" int  win_findimage (int resno);
extern "C" void win_loadimage (int resno, const char *filename, int offset, int reslen);
extern "C" int  win_findsound (int resno);
extern "C" void win_loadsound (int resno, char *filename, int offset, int reslen);

static glui32
gsc_resource_id (scr_int offset, scr_int length)
{
  /* Synthesize a stable id from offset and length so repeat calls for the
   * same resource hit the cache instead of re-loading from disk.  Fold the
   * full 64-bit offset into 32 bits rather than shift-then-truncate: a plain
   * "(offset << 12) ^ length" cast to glui32 drops the high bits of any
   * offset >= 2^20, so distinct chunks in a large game could collide on one
   * cached id and cause the wrong image or sound to be drawn or played. */
  unsigned long long hash;

  hash = (unsigned long long) (scr_uint) offset;
  hash = (hash ^ (unsigned long long) (scr_uint) length) * 0x9E3779B97F4A7C15ULL;
  return (glui32) (hash ^ (hash >> 32));
}

schanid_t sound_channel;

void
os_play_sound (const scr_char *filepath,
               scr_int offset, scr_int length, scr_bool is_looping)
{
  glui32 id;
  (void) filepath;

  if (length <= 0 || gli_game_path == NULL)
    return;

  if (sound_channel == NULL)
    sound_channel = glk_schannel_create (0);
  if (sound_channel == NULL)
    return;

  id = gsc_resource_id (offset, length);
  if (!win_findsound ((int) id))
    win_loadsound ((int) id, gli_game_path, (int) offset, (int) length);
  glk_schannel_play_ext (sound_channel, id, is_looping ? 0xffffffff : 1, 0);
}

void
os_stop_sound (void)
{
  if (sound_channel != NULL)
    glk_schannel_stop (sound_channel);
}
#else
/*
 * os_play_sound()
 * os_stop_sound()
 *
 * Stub functions.  The unused variables defeat gcc warnings.
 */
void
os_play_sound (const scr_char *filepath,
               scr_int offset, scr_int length, scr_bool is_looping)
{
  (void) filepath;
  (void) offset;
  (void) length;
  (void) is_looping;
}

void
os_stop_sound (void)
{
}
#endif


#ifdef GSC_HAVE_TITLE_WINDOW
/*
 * Title/cover graphic support.  Adrift games can carry an "IntroRes" cover
 * image, shown by the engine before the game's first turn.  Adrift intros
 * routinely clear the main window with <cls> tags -- "To Hell in a Hamper"
 * emits one as its very first output -- so drawing the cover inline would wipe
 * it immediately.  Instead, any graphic requested before the player's first
 * input is shown in a temporary graphics window at the top of the display,
 * closed on that first keypress.  This mirrors the original Runner, which
 * shows the title in a separate picture pane that text clears don't touch.
 */
winid_t gsc_graphics_window = NULL;
glui32 gsc_title_image = 0;
#ifdef SPATTERLIGHT
/* The title image's chunk in the game file, so an autorestore can re-load it
   into the app-side image cache (the cache does not survive a relaunch). */
scr_int gsc_title_offset = 0;
scr_int gsc_title_length = 0;
#endif
int gsc_seen_input = FALSE;


/*
 * gsc_title_redraw()
 *
 * Draw, or redraw on a resize, the title image scaled to fit within the
 * graphics window, preserving its aspect ratio and centring it.
 */
static void
gsc_title_redraw (void)
{
  glui32 win_width, win_height, img_width, img_height, draw_width, draw_height;

  if (gsc_graphics_window == NULL || gsc_title_image == 0)
    return;

  glk_window_get_size (gsc_graphics_window, &win_width, &win_height);
  if (win_width == 0 || win_height == 0)
    return;
  if (!glk_image_get_info (gsc_title_image, &img_width, &img_height)
      || img_width == 0 || img_height == 0)
    return;

  /* Fit within the pane, limited by width or height, whichever binds first. */
  if (win_width * img_height <= win_height * img_width)
    {
      draw_width = win_width;
      draw_height = img_height * win_width / img_width;
    }
  else
    {
      draw_height = win_height;
      draw_width = img_width * win_height / img_height;
    }

  glk_window_fill_rect (gsc_graphics_window, 0, 0, 0, win_width, win_height);
  glk_image_draw_scaled (gsc_graphics_window, gsc_title_image,
                         (glsi32) ((win_width - draw_width) / 2),
                         (glsi32) ((win_height - draw_height) / 2),
                         draw_width, draw_height);
}

/*
 * gsc_show_title_graphic()
 *
 * Open a temporary graphics window above the main text, sized to the image's
 * aspect ratio, and draw the title image into it.  Returns TRUE on success,
 * FALSE if graphics windows are unavailable, in which case the caller falls
 * back to an inline draw.
 */
int
gsc_show_title_graphic (glui32 image)
{
  glui32 win_width, win_height, img_width, img_height, pane_height;

  if (!glk_gestalt (gestalt_Graphics, 0)
      || !glk_gestalt (gestalt_DrawImage, wintype_Graphics))
    return FALSE;
  if (!glk_image_get_info (image, &img_width, &img_height)
      || img_width == 0 || img_height == 0)
    return FALSE;

  if (gsc_graphics_window == NULL)
    {
      gsc_graphics_window = glk_window_open (gsc_main_window,
                                             winmethod_Above | winmethod_Fixed,
                                             0, wintype_Graphics, 0);
      if (gsc_graphics_window == NULL)
        return FALSE;
    }

  gsc_title_image = image;

  /* Size the pane to fit the image width, preserving its aspect ratio. */
  glk_window_get_size (gsc_graphics_window, &win_width, &win_height);
  if (win_width == 0)
    {
      glk_window_close (gsc_graphics_window, NULL);
      gsc_graphics_window = NULL;
      gsc_title_image = 0;
      return FALSE;
    }
  pane_height = img_height * win_width / img_width;
  glk_window_set_arrangement (glk_window_get_parent (gsc_graphics_window),
                              winmethod_Above | winmethod_Fixed,
                              pane_height, gsc_graphics_window);

  gsc_title_redraw ();
  return TRUE;
}

/*
 * gsc_close_title_graphic()
 *
 * Close the temporary title window, if open, returning its space to the main
 * window.  Called when the player provides their first input.
 */
void
gsc_close_title_graphic (void)
{
  if (gsc_graphics_window != NULL)
    {
      glk_window_close (gsc_graphics_window, NULL);
      gsc_graphics_window = NULL;
      gsc_title_image = 0;
    }
}

/*
 * gsc_title_screen_wait()
 *
 * A cover taller than it is wide, sized to the display's width, can leave the
 * main window no room at all (SS Whore's portrait cover squeezes it to zero
 * height).  The pane is then really a title screen: the player can't see the
 * main window's prompt, so keys typed into it seem lost, and the intro text
 * printed into the hidden window would be scrolled to its end once the pane
 * closed.  So in that case wait here for any key (or click) on the title,
 * then close it, before the engine flushes the intro text into what is once
 * again a full-height main window.
 */
static void
gsc_title_screen_wait (void)
{
  glui32 width, height;
  winid_t key_window;
  int mouse;
  event_t event;

  if (gsc_graphics_window == NULL)
    return;

  /* A main window that still shows a few lines keeps the side-pane layout:
     the player reads the intro under the cover and types as usual. */
  glk_window_get_size (gsc_main_window, &width, &height);
  if (height >= 3)
    return;

  /* Take the key on the title itself where the library allows it; otherwise
     the hidden main window still receives keys, so ask there. */
  key_window = gsc_main_window;
#ifdef gestalt_GraphicsCharInput
  if (glk_gestalt (gestalt_GraphicsCharInput, 0))
    key_window = gsc_graphics_window;
#endif
  glk_request_char_event (key_window);
  mouse = glk_gestalt (gestalt_MouseInput, wintype_Graphics);
  if (mouse)
    glk_request_mouse_event (gsc_graphics_window);

  for (;;)
    {
      glk_select (&event);
      if (event.type == evtype_Arrange || event.type == evtype_Redraw)
        gsc_refresh_windows ();
      else if (event.type == evtype_CharInput && event.win == key_window)
        break;
      else if (event.type == evtype_MouseInput
               && event.win == gsc_graphics_window)
        break;
    }

  if (event.type == evtype_CharInput)
    {
      if (mouse)
        glk_cancel_mouse_event (gsc_graphics_window);
    }
  else
    glk_cancel_char_event (key_window);

  gsc_seen_input = TRUE;
  gsc_close_title_graphic ();
}
#endif /* GSC_HAVE_TITLE_WINDOW */


/*
 * gsc_refresh_windows()
 *
 * Refresh the size-sensitive windows, on Glk Arrange and Redraw events.
 */
void
gsc_refresh_windows (void)
{
  /* An Arrange is how a library preference flip announces itself, and the
     status redraw below asks gsc_colour_visible, so the cache goes first. */
  gsc_normal_measure_state = -1;

  gsc_status_redraw ();
#ifdef GSC_HAVE_TITLE_WINDOW
  gsc_title_redraw ();
#endif
}


#ifdef GLK_MODULE_GARGLK_FILE_RESOURCES
/*
 * os_show_graphic()
 *
 * Use the Gargoyle-specific garglk_add_resource_from_file().  Before the
 * player's first input, show the image as a title in a dedicated graphics
 * window; afterwards, draw it inline in the main window.
 */
void
os_show_graphic (const scr_char *filepath, scr_int offset, scr_int length)
{
  if (length <= 0 || gsc_main_window == NULL)
    return;

  glui32 id = garglk_add_resource_from_file(giblorb_ID_Pict, gamefile, offset, length);
  if (id == 0)
    return;

  if (!gsc_seen_input && gsc_show_title_graphic (id))
    {
      gsc_title_screen_wait ();
      return;
    }

  gsc_draw_inline_graphic(id);
}
#elif defined(SPATTERLIGHT)
/*
 * os_show_graphic()
 *
 * Pre-load the requested image chunk from the game file into the
 * Spatterlight Glk image cache.  Before the player's first input, show it as
 * a title image in a dedicated graphics window; afterwards, draw it inline in
 * the main window.
 */
void
os_show_graphic (const scr_char *filepath, scr_int offset, scr_int length)
{
  glui32 id;
  (void) filepath;

  if (length <= 0 || gsc_main_window == NULL || gli_game_path == NULL)
    return;

  id = gsc_resource_id (offset, length);
  if (!win_findimage ((int) id))
    win_loadimage ((int) id, gli_game_path, (int) offset, (int) length);

  if (!gsc_seen_input && gsc_show_title_graphic (id))
    {
      gsc_title_offset = offset;
      gsc_title_length = length;
      gsc_title_screen_wait ();
      return;
    }

  gsc_draw_inline_graphic (id);
}
#else
/*
 * os_show_graphic()
 *
 * Without Gargoyle's file resources there is no way to show a game graphic,
 * so this is a stub function, with unused variables to defeat gcc warnings.
 */
void
os_show_graphic (const scr_char *filepath, scr_int offset, scr_int length)
{
  (void) filepath;
  (void) offset;
  (void) length;
}
#endif
