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
/*
 * Sounds and graphics go through garglk_add_resource_from_file(): Gargoyle's
 * own, or glkimp's in Spatterlight.  Either way it takes a file in the game's
 * directory, named without a path, and returns a resource number for the
 * standard Glk drawing and playing routines.
 */

/*
 * gsc_load_resource()
 *
 * Common part of os_play_sound() and os_show_graphic(): register a resource,
 * embedded or external, and return its number, or zero if there is nothing to
 * load.
 *
 * An embedded resource is a chunk of the game file.  A game with "Embedded"
 * off, like Druggy Lane, instead names each sound or graphic by the full path
 * it had on the author's machine -- "C:\My Documents\Adrift\...\day.wav" --
 * and ships the files beside the .taf; that is the path's last component, in
 * the game's directory.  Spatterlight's garglk_add_resource_from_file() finds
 * it whatever its case -- a Windows author's capitals need not match the
 * files on disk -- and takes a zero length as the whole file.  Gargoyle's
 * makes no such promises, so there external resources stay unsupported.
 */
static glui32
gsc_load_resource (const scr_char *filepath, scr_int offset, scr_int length,
                   int is_sound)
{
  glui32 usage = is_sound ? giblorb_ID_Snd : giblorb_ID_Pict;

  if (length > 0)
    return garglk_add_resource_from_file (usage, gsc_gamefile,
                                          (glui32) offset, (glui32) length);
#ifdef SPATTERLIGHT
  {
    const char *name, *p;

    if (scr_strempty (filepath))
      return 0;
    /* The name is whatever follows the last separator, of either kind. */
    name = filepath;
    for (p = filepath; *p != '\0'; p++)
      {
        if (*p == '\\' || *p == '/' || *p == ':')
          name = p + 1;
      }
    if (*name == '\0')
      return 0;
    return garglk_add_resource_from_file (usage, name, 0, 0);
  }
#else
  (void) filepath;
  return 0;
#endif
}

#ifdef SPATTERLIGHT
/*
 * gsc_load_external_resource()
 *
 * The ADRIFT 5 side's way in: load a media file named by an <img>/<audio>
 * src from beside the game, returning its resource number, or zero if there
 * is no such file.
 */
glui32
gsc_load_external_resource (const char *filepath, int is_sound)
{
  return gsc_load_resource (filepath, 0, 0, is_sound);
}
#endif

schanid_t sound_channel;

void
os_play_sound (const scr_char *filepath,
               scr_int offset, scr_int length, scr_bool is_looping)
{
  glui32 id;

  if (sound_channel == NULL)
    sound_channel = glk_schannel_create (0);
  if (sound_channel == NULL)
    return;

  id = gsc_load_resource (filepath, offset, length, TRUE);
  if (id == 0)
    return;
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
 * Register the requested image, a chunk of the game file or a file beside it
 * (see gsc_load_resource).  Before the player's first input, show it as a
 * title image in a dedicated graphics window; afterwards, draw it inline in
 * the main window.
 */
void
os_show_graphic (const scr_char *filepath, scr_int offset, scr_int length)
{
  glui32 id;

  if (gsc_main_window == NULL)
    return;

  id = gsc_load_resource (filepath, offset, length, FALSE);
  if (id == 0)
    return;

  if (!gsc_seen_input && gsc_show_title_graphic (id))
    {
#ifdef SPATTERLIGHT
      /* An external title file can't be re-loaded from the game file after
         an autorestore, so record no chunk for it; that restore then simply
         skips the re-load, as before for any external graphic. */
      gsc_title_offset = length > 0 ? offset : 0;
      gsc_title_length = length > 0 ? length : 0;
#endif
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
