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
 * os_glk_autosave.cpp: Spatterlight autosave and autorestore, engine side
 * (the file plumbing is scarier-autosave.mm).  Split out of os_glk.cpp;
 * see os_glk_internal.h.
 */

#include "os_glk_internal.h"

#ifdef SPATTERLIGHT
/*---------------------------------------------------------------------*/
/*  Spatterlight autosave/autorestore support                          */
/*                                                                     */
/*  The file/plist plumbing lives in scarier-autosave.mm (app target   */
/*  only); this section owns everything engine-side: the state         */
/*  containers written into autosave.glksave, the stash/recover of the */
/*  Glk object globals above, and the per-prompt gsc_autosave().       */
/*---------------------------------------------------------------------*/

/* autosave.glksave is a small line-framed container: a magic line naming the
   engine, then length-prefixed chunks.  Both engines carry their own state
   serialization plus the undo history, so UNDO still works across an
   autorestore (as Bocfel carries its save stacks in its autosave). */
static const char *const GSC_SC_CONTAINER_MAGIC = "SCARAUTO4\n";
static const char *const GSC_A5_CONTAINER_MAGIC = "SCARAUTO5\n";

static void
gsc_container_put_chunk (std::string &out, const char *data, size_t length)
{
  out += std::to_string (length);
  out += '\n';
  out.append (data, length);
}

/* Read "<decimal>\n<bytes>" at *pos; false on malformed framing. */
static bool
gsc_container_get_chunk (const std::string &data, size_t *pos,
                         std::string *chunk)
{
  size_t eol = data.find ('\n', *pos);
  if (eol == std::string::npos)
    return false;
  unsigned long length = strtoul (data.c_str () + *pos, NULL, 10);
  *pos = eol + 1;
  if (length > data.size () - *pos)
    return false;
  chunk->assign (data, *pos, length);
  *pos += length;
  return true;
}

static bool
gsc_container_get_count (const std::string &data, size_t *pos, long *count)
{
  size_t eol = data.find ('\n', *pos);
  if (eol == std::string::npos)
    return false;
  *count = strtol (data.c_str () + *pos, NULL, 10);
  *pos = eol + 1;
  return *count >= 0;
}

/* Serialization callbacks bridging the engines' byte streams to strings. */
static void
gsc_state_append (void *opaque, const scr_byte *buffer, scr_int length)
{
  ((std::string *) opaque)->append ((const char *) buffer, (size_t) length);
}

struct GscStateCursor
{
  const std::string *data;
  size_t pos;
};

static scr_int
gsc_state_read (void *opaque, scr_byte *buffer, scr_int length)
{
  GscStateCursor *cursor = (GscStateCursor *) opaque;
  size_t left = cursor->data->size () - cursor->pos;
  size_t bytes = (size_t) length < left ? (size_t) length : left;

  memcpy (buffer, cursor->data->data () + cursor->pos, bytes);
  cursor->pos += bytes;
  return (scr_int) bytes;
}

/*
 * gsc_sc_serialize_all()
 * gsc_sc_apply_all()
 *
 * The ADRIFT <=4 container: the engine's TAS-format state, the memo undo
 * ring (oldest first), and the one-turn-back undo buffer when available.
 */
static std::string
gsc_sc_serialize_all (void)
{
  const scr_gameref_t game = (scr_gameref_t) gsc_game;
  const scr_memo_setref_t memento = gs_get_memento (game);
  std::string engine_state, undo_game;
  scr_int ring_count, index_;

  scr_save_game_to_callback (gsc_game, gsc_state_append, &engine_state);
  if (engine_state.empty ())
    return std::string ();

  std::string out = GSC_SC_CONTAINER_MAGIC;
  gsc_container_put_chunk (out, engine_state.data (), engine_state.size ());

  /* What the engine keeps between two prompts that no ADRIFT save file
     carries: the line `again` repeats, the history, pronouns, an open
     question, the player's settings and name -- see run_session_state().
     "S" then its chunk; a container from before this section has a digit
     here instead. */
  {
    const std::string session = run_session_state (game);

    out += 'S';
    gsc_container_put_chunk (out, session.data (), session.size ());
  }

  ring_count = memo_get_undo_count (memento);
  out += std::to_string ((long) ring_count);
  out += '\n';
  for (index_ = 0; index_ < ring_count; index_++)
    {
      scr_int length = 0;
      const scr_byte *blob = memo_get_undo (memento, index_, &length);

      gsc_container_put_chunk (out, (const char *) blob, (size_t) length);
    }

  if (scr_save_undo_game_to_callback (gsc_game, gsc_state_append, &undo_game)
      && !undo_game.empty ())
    {
      out += "1\n";
      gsc_container_put_chunk (out, undo_game.data (), undo_game.size ());
    }
  else
    out += "0\n";
  return out;
}

bool
gsc_sc_apply_all (const std::string &data)
{
  const scr_gameref_t game = (scr_gameref_t) gsc_game;
  const std::string magic = GSC_SC_CONTAINER_MAGIC;
  std::string chunk, session;
  bool has_session = false;
  long ring_count, has_undo_game, index_;

  if (data.compare (0, magic.size (), magic) != 0)
    return false;
  size_t pos = magic.size ();

  if (!gsc_container_get_chunk (data, &pos, &chunk))
    return false;
  {
    GscStateCursor cursor = { &chunk, 0 };
    if (!scr_load_game_from_callback (gsc_game, gsc_state_read, &cursor))
      return false;
  }

  if (pos < data.size () && data[pos] == 'S')
    {
      pos++;
      if (!gsc_container_get_chunk (data, &pos, &session))
        return false;
      has_session = true;
    }

  /* A malformed or unreadable undo history just means no UNDO past the
     restore point; the restored game state above stays good. */
  do
    {
      if (!gsc_container_get_count (data, &pos, &ring_count))
        break;
      for (index_ = 0; index_ < ring_count; index_++)
        {
          if (!gsc_container_get_chunk (data, &pos, &chunk))
            break;
          memo_append_undo (gs_get_memento (game),
                            (const scr_byte *) chunk.data (),
                            (scr_int) chunk.size ());
        }
      if (index_ < ring_count
          || !gsc_container_get_count (data, &pos, &has_undo_game))
        break;
      if (has_undo_game && gsc_container_get_chunk (data, &pos, &chunk))
        {
          GscStateCursor cursor = { &chunk, 0 };
          scr_load_undo_game_from_callback (gsc_game, gsc_state_read,
                                            &cursor);
        }
    }
  while (false);

  /* Last: loading the undo buffer above resets that copy's pronouns. */
  return !has_session || run_restore_session_state (game, session);
}

/*
 * gsc_a5_serialize_all()
 * gsc_a5_apply_all()
 *
 * The ADRIFT 5 container: the engine's save XML (a5run_save, RNG state
 * included), the last turn's composed output, the undo snapshot stack
 * (oldest first) with its parallel turn texts, and last the parser's
 * cross-turn continuation (a5run_pending_save: an open "Which X?" question
 * or a remembered bare verb), which the save format itself does not carry.
 */
static std::string
gsc_a5_serialize_all (void)
{
  size_t length = 0;
  char *blob = a5run_save (gsc_a5_run, &length);
  const char *text;
  size_t text_length;
  int depth, i;

  if (blob == NULL)
    return std::string ();

  std::string out = GSC_A5_CONTAINER_MAGIC;
  gsc_container_put_chunk (out, blob, length);
  free (blob);

  a5run_get_turn_text (gsc_a5_run, &text, &text_length);
  gsc_container_put_chunk (out, text, text_length);

  depth = a5run_undo_depth (gsc_a5_run);
  out += std::to_string ((long) depth);
  out += '\n';
  for (i = 0; i < depth; i++)
    {
      const char *turn_text;
      size_t blob_length = 0, turn_length = 0;
      const char *undo_blob = a5run_undo_peek (gsc_a5_run, i, &blob_length,
                                               &turn_text, &turn_length);

      gsc_container_put_chunk (out, undo_blob, blob_length);
      gsc_container_put_chunk (out, turn_text, turn_length);
    }

  length = 0;
  blob = a5run_pending_save (gsc_a5_run, &length);
  gsc_container_put_chunk (out, blob == NULL ? "" : blob, length);
  free (blob);
  return out;
}

bool
gsc_a5_apply_all (const std::string &data)
{
  const std::string magic = GSC_A5_CONTAINER_MAGIC;
  std::string chunk, turn_chunk;
  long depth, i;

  if (data.compare (0, magic.size (), magic) != 0)
    return false;
  size_t pos = magic.size ();

  if (!gsc_container_get_chunk (data, &pos, &chunk))
    return false;
  if (!a5run_restore (gsc_a5_run, chunk.data (), chunk.size ()))
    return false;

  /* As on the scare side, a truncated undo tail is not fatal. */
  if (!gsc_container_get_chunk (data, &pos, &chunk))
    return true;
  a5run_set_turn_text (gsc_a5_run, chunk.data (), chunk.size ());

  if (!gsc_container_get_count (data, &pos, &depth))
    return true;
  for (i = 0; i < depth; i++)
    {
      if (!gsc_container_get_chunk (data, &pos, &chunk)
          || !gsc_container_get_chunk (data, &pos, &turn_chunk))
        return true;
      a5run_undo_push_blob (gsc_a5_run, chunk.data (), chunk.size (),
                            turn_chunk.data (), turn_chunk.size ());
    }

  /* Absent from an autosave written before the question was carried; the
     game then resumes at a fresh prompt, as it always did. */
  if (gsc_container_get_chunk (data, &pos, &chunk))
    a5run_pending_restore (gsc_a5_run, chunk.data (), chunk.size ());
  return true;
}

/* The state struct's channel arrays are sized by hand in its header. */
static_assert (sizeof (ScarierGlkFrontendState::a5_channeltags)
               / sizeof (int) == GSC_A5_MAX_CHANNELS,
               "ScarierGlkFrontendState channel arrays != GSC_A5_MAX_CHANNELS");

/*
 * gsc_stash_frontend_state()
 * gsc_recover_frontend_state()
 *
 * Capture the Glk object globals as serialization tags for the library
 * plist, and re-point them at the restored objects after the library state
 * has been rebuilt (called from scarier-autosave.mm between the library's
 * main and "late" restore passes).
 */
void
gsc_stash_frontend_state (ScarierGlkFrontendState *st)
{
  int ch;

  st->mainwintag = gsc_main_window ? gsc_main_window->tag : 0;
  st->statuswintag = gsc_status_window ? gsc_status_window->tag : 0;
  st->sidewintag = gsc_a5_side_window ? gsc_a5_side_window->tag : 0;
  st->mapwintag = gsc_map_window ? gsc_map_window->tag : 0;
  st->gfxwintag = gsc_graphics_window ? gsc_graphics_window->tag : 0;
  st->transcripttag = gsc_transcript_stream ? gsc_transcript_stream->tag : 0;
  st->inputlogtag = gsc_inputlog_stream ? gsc_inputlog_stream->tag : 0;
  st->readlogtag = gsc_readlog_stream ? gsc_readlog_stream->tag : 0;
  st->soundchanneltag = sound_channel ? sound_channel->tag : 0;
  for (ch = 0; ch < GSC_A5_MAX_CHANNELS; ch++)
    {
      st->a5_channeltags[ch] = gsc_a5_channels[ch]
                               ? gsc_a5_channels[ch]->tag : 0;
      st->a5_chan_sound[ch] = gsc_a5_chan_sound[ch];
    }
  st->seen_input = gsc_seen_input;
  st->title_image = gsc_title_image;
  st->title_offset = gsc_title_offset;
  st->title_length = gsc_title_length;
  st->map_shown = gsc_map_shown;
  st->map_at_top = gsc_map_at_top;
  st->map_zoom = gsc_map_zoom;
  st->map_follow = gsc_map_follow;
  st->map_cx = gsc_map_cam.cx;
  st->map_cy = gsc_map_cam.cy;
  st->map_page = gsc_map_cam.page;
  st->map_colourful = gsc_map_colourful;
  st->colour_on = gsc_colour_enabled;

  /* The exact RNG state (which generator is active plus the xoshiro words),
     so a deterministic session's randomness continues where it left off. */
  {
    int usenative = 1;
    glui32 *words = NULL;
    int count = 0;

    erkyrath_random_get_detstate (&usenative, &words, &count);
    st->rng_usenative = usenative;
    for (ch = 0; ch < 4; ch++)
      st->rng_state[ch] = (count == 4 && words) ? words[ch] : 0;
  }
  /* The Runner-compatible generator keeps its words apart from those. */
  {
    scr_uint words[4] = { 0, 0, 0, 0 }, draws = 0;

    st->rng_runner = scr_get_runner_random_state (words, &draws) ? 1 : 0;
    for (ch = 0; ch < 4; ch++)
      st->rng_runner_state[ch] = st->rng_runner ? words[ch] : 0;
    st->rng_runner_draws = st->rng_runner ? draws : 0;
  }
}

void
gsc_recover_frontend_state (const ScarierGlkFrontendState *st)
{
  int ch;

  gsc_main_window = gli_window_for_tag (st->mainwintag);
  gsc_status_window = gli_window_for_tag (st->statuswintag);
  gsc_a5_side_window = gli_window_for_tag (st->sidewintag);
  gsc_map_window = gli_window_for_tag (st->mapwintag);
  gsc_graphics_window = gli_window_for_tag (st->gfxwintag);
  gsc_transcript_stream = gli_stream_for_tag (st->transcripttag);
  gsc_inputlog_stream = gli_stream_for_tag (st->inputlogtag);
  gsc_readlog_stream = gli_stream_for_tag (st->readlogtag);
  sound_channel = gli_schan_for_tag (st->soundchanneltag);
  for (ch = 0; ch < GSC_A5_MAX_CHANNELS; ch++)
    {
      gsc_a5_channels[ch] = gli_schan_for_tag (st->a5_channeltags[ch]);
      gsc_a5_chan_sound[ch] = st->a5_chan_sound[ch];
    }
  gsc_seen_input = st->seen_input;
  gsc_title_image = (glui32) st->title_image;
  gsc_title_offset = (scr_int) st->title_offset;
  gsc_title_length = (scr_int) st->title_length;
  /* The app-side image cache does not survive a relaunch; register the title
     image chunk again so a post-restore resize can still redraw the cover.
     It comes back under the number it had, unless the autosave predates the
     shared numbering, so take the number given. */
  if (gsc_graphics_window != NULL && gsc_title_image != 0
      && gsc_title_length > 0)
    gsc_title_image = garglk_add_resource_from_file
      (giblorb_ID_Pict, gsc_gamefile,
       (glui32) gsc_title_offset, (glui32) gsc_title_length);
  gsc_map_shown = st->map_shown;
  gsc_map_at_top = st->map_at_top;
  gsc_map_zoom = st->map_zoom;
  gsc_map_follow = st->map_follow;
  gsc_map_cam.cx = st->map_cx;
  gsc_map_cam.cy = st->map_cy;
  gsc_map_cam.page = st->map_page;
  gsc_map_last_player[0] = '\0';
  /* The renderer is a fresh process's, at its default; the scheme has to be
     named again or the restored map would come back in the standard colours. */
  gsc_map_set_colourful (st->map_colourful);
  /* The restored streams still carry the zcolors colour mode set on them, and
     the restored windows their black background, so taking the flag back is
     all it takes to pick the mode up where it left off.  (An autosave written
     before this field existed decodes as 0, which is what those sessions
     restored as anyway.) */
  gsc_colour_enabled = st->colour_on;
  /* A map that was wanted but still waiting for somewhere to draw was archived
     closed, and looks exactly like one the player shut; what the map would do
     from a standing start -- the stored preference, or failing that the
     layout the game shipped -- is what tells the two apart, so the wait
     survives the autosave. */
  {
    int pref = gsc_map_pref_read (NULL, NULL);

    gsc_map_want = gsc_map_shown || pref == 1
                   || (pref < 0 && gsc_map_default_shown ());
  }
  /* The restored map window holds the app's snapshot pixels, not ours:
     force the next redraw to flush the whole surface. */
  gsc_map_screen_drop ();

  /* Restore the RNG to its saved position.  The silent autorestore boot
     re-seeded and may have drawn from it; this puts it back exactly where
     the autosave left it.  In native (non-deterministic) mode the flag
     keeps the native generator selected and the words are inert. */
  if (st->rng_usenative >= 0)
    {
      glui32 words[4] = { st->rng_state[0], st->rng_state[1],
                          st->rng_state[2], st->rng_state[3] };
      erkyrath_random_set_detstate (st->rng_usenative, words, 4);
    }
  if (st->rng_runner == 1)
    {
      scr_uint words[4] = { st->rng_runner_state[0], st->rng_runner_state[1],
                            st->rng_runner_state[2], st->rng_runner_state[3] };
      scr_set_runner_random_state (words, st->rng_runner_draws);
    }
  /* Not stashed: gsc_map_taken, assist flags and locale are re-derived at
     startup; the walk-in-progress state is deliberately dropped (a restored
     session simply stops walking); gsc_map itself is authored data (ADRIFT
     5, reloaded at boot) or rebuilt every redraw (ADRIFT 4); and the restart
     count gsc_map_notice_restart watches is per-process on both sides of the
     comparison, so a fresh process starts it and the engine's at zero
     together. */
}

/*
 * gsc_autorestore_replace_state()
 *
 * Replace the whole state -- engine and Glk library both -- with the saved
 * one, `apply` being the engine's own state loader.  A bad autosave (which
 * the read deletes) restarts in a fresh process rather than continue from a
 * polluted boot, so this only returns on success.
 */
void
gsc_autorestore_replace_state (bool (*apply) (const std::string &data))
{
  std::string data;
  bool restored = scarier_autosave_read_game (&data)
                  && apply (data)
                  && scarier_autosave_restore_library ();
  if (!restored)
    {
      scarier_autosave_discard ();
      win_reset ();
      exit (0);
    }
}

/* The on-disk game path, for the autosave directory's file-signature hash. */
const char *
gsc_autosave_game_path (void)
{
  return gsc_game_path;
}

/*
 * gsc_autosave()
 *
 * Save the whole game state (engine container + Glk library plist), then
 * ask the window server to snapshot the GUI under the same tag.  Called at
 * every top-level command prompt, after the prompt is printed but before
 * line input is requested, and again after a real-time tick that changed
 * state and reprinted the prompt.  An open "Which X?" question is saved
 * with the game on both engines (which_* / which_offered on the ADRIFT 4
 * side, the pending chunk on the ADRIFT 5 side), so the only prompts
 * skipped are those that would not restore coherently: the pre-intro
 * name/gender prompts and the debugger's.
 */
void
gsc_autosave (void)
{
  std::string state;

  if (gsc_in_debug_read || !scarier_autosave_wanted ())
    return;
  if (gsc_is_a5)
    {
      if (gsc_a5_run == NULL || a5run_is_over (gsc_a5_run))
        return;
      state = gsc_a5_serialize_all ();
    }
  else
    {
      const scr_gameref_t game = (scr_gameref_t) gsc_game;

      if (gsc_game == NULL || !scr_is_game_running (gsc_game))
        return;
      /* Not seen the player's room yet = still inside the startup block
         (the "Please enter your name" prompt): resuming there would replay
         the intro over the restored transcript. */
      if (!gs_room_seen (game, gs_playerroom (game)))
        return;
      state = gsc_sc_serialize_all ();
    }
  if (!state.empty ())
    scarier_autosave_write (state);
}

#endif /* SPATTERLIGHT */
