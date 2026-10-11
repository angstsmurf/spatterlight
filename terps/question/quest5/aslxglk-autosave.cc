/*
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


/* aslxglk-autosave.cc -- Spatterlight autosave of the Quest 5 Glk frontend
 * state (see aslxglk-internal.hh). */

#include "aslxglk-internal.hh"

namespace aslxglk {

#ifdef SPATTERLIGHT

/* The frontend state blob, carried inside the library plist in questglk's
 * blob encoding.  Window/stream/channel identities cross as glkimp
 * serialization tags.  The transcript hyperlink actions (g_links) must
 * survive because the restored transcript still shows their links; the map,
 * the panes and the banner are repainted from restored ENGINE state on the
 * way into the session (InitInterface re-runs Grid_Redraw and the panel,
 * update_lists refills the pane and status), so none of that is saved. */

/* The chunk log's recorded text is UTF-32 (what went to the window); the blob
 * is bytes. */
std::string utf8_from_u32(const std::u32string &s)
{
    std::vector<glui32> cps(s.begin(), s.end());
    return utf8_from_uni(cps.empty() ? nullptr : cps.data(), (glui32) cps.size());
}

/* `rngs` = the RNG streams that go with the engine state the autosave is
 * written with, when that is not the engine as it stands: the start of the
 * turn for an autosave under one of its prompts, none at all for one under
 * a prompt of the start script, which seeds its own and draws them again. */
std::string aslx_encode_frontend(Interp &in, const RngStreams *turn_rngs)
{
    std::string b;
    blob_str(b, kAslxBlobMagic);
    blob_num(b, gwin ? gwin->tag : 0);
    blob_num(b, gbanner ? gbanner->tag : 0);
    blob_num(b, gobjwin ? gobjwin->tag : 0);
    blob_num(b, gdivider ? gdivider->tag : 0);
    blob_num(b, gpanelwin ? gpanelwin->tag : 0);
    blob_num(b, gmapwin ? gmapwin->tag : 0);
    blob_num(b, gtranscript ? gtranscript->tag : 0);
    blob_num(b, g_schannel ? g_schannel->tag : 0);
    blob_num(b, (long) g_links_spent);
    blob_num(b, (long) g_links.size());
    /* Spent links keep their slot (window link ids index into g_links) but
     * their payloads are never consulted again -- serialize them empty so an
     * autosave's size is bounded by the LIVE tail, not the whole session. */
    for (size_t i = 0; i < g_links.size(); i++) {
        const LinkAction &l = g_links[i];
        const bool spent = i < g_links_spent;
        blob_str(b, spent ? std::string() : l.command);
        blob_str(b, spent ? std::string() : l.event_func);
        blob_str(b, spent ? std::string() : l.event_param);
        blob_str(b, spent ? std::string() : l.toggle_key);
        blob_str(b, spent ? std::string() : l.element);
        blob_num(b, !spent && l.end_wait ? 1 : 0);
        blob_num(b, !spent && l.inner_text ? 1 : 0);
    }
    /* The verb menu on screen, if any -- restored INTO on the next launch. */
    blob_str(b, g_pending_menu_alias);
    blob_num(b, (long) g_pending_menu_verbs.size());
    for (size_t i = 0; i < g_pending_menu_verbs.size(); i++)
        blob_str(b, g_pending_menu_verbs[i]);
    blob_str(b, g_pane_expanded);
    blob_str(b, g_status_line);
    blob_str(b, g_location_line);
    blob_str(b, g_panel_last);
    /* The custom status panel's cells (the rows come back from the game). */
    blob_num(b, (long) g_cstatus_vals.size());
    for (const auto &kv : g_cstatus_vals) {
        blob_str(b, kv.first);
        blob_str(b, kv.second);
    }
    /* The exact RNG streams (fallback + per compiled expression), so a
     * deterministic session's randomness continues across an autorestore. */
    RngStreams rngs;
    if (turn_rngs)
        rngs = *turn_rngs;
    else
        in.capture_rng_streams(rngs);
    blob_num(b, (long) rngs.size());
    for (const auto &entry : rngs) {
        blob_str(b, entry.first);
        for (int i = 0; i < 4; i++)
            blob_num(b, (long) entry.second[i]);
    }
    /* Output sections and the chunk log behind them.  The restored transcript
     * still shows the text they cover, so without these the first hide after
     * an autorestore -- which for a gamebook is the very next click -- would
     * find no section and leave the previous page's options on screen.  The
     * recorded text goes over as UTF-8. */
    blob_num(b, (long) g_out_log.size());
    for (const OutChunk &c : g_out_log) {
        blob_str(b, c.html);
        blob_str(b, utf8_from_u32(c.text));
        blob_num(b, c.image ? 1 : 0);
    }
    blob_num(b, (long) g_sections.size());
    for (const OutSection &s : g_sections) {
        blob_str(b, s.name);
        blob_num(b, (long) s.start);
        blob_num(b, s.end == (size_t) -1 ? -1 : (long) s.end);
    }
    /* SIDEBAR WIDE. */
    blob_num(b, side_pane_is_wide() ? 1 : 0);
    return b;
}

bool aslx_recover_frontend(const std::string &blob)
{
    BlobReader r{blob};
    if (r.str() != kAslxBlobMagic)
        return false;
    gwin = gli_window_for_tag((int) r.num());
    gbanner = gli_window_for_tag((int) r.num());
    gobjwin = gli_window_for_tag((int) r.num());
    gdivider = gli_window_for_tag((int) r.num());
    gpanelwin = gli_window_for_tag((int) r.num());
    gmapwin = gli_window_for_tag((int) r.num());
    gtranscript = gli_stream_for_tag((int) r.num());
    g_schannel = gli_schan_for_tag((int) r.num());
    g_links_spent = (size_t) r.num();
    long nlinks = r.num();
    g_links.clear();
    for (long i = 0; i < nlinks && r.ok; i++) {
        LinkAction l;
        l.command = r.str();
        l.event_func = r.str();
        l.event_param = r.str();
        l.toggle_key = r.str();
        l.element = r.str();
        l.end_wait = r.num() != 0;
        l.inner_text = r.num() != 0;
        g_links.push_back(l);
    }
    g_pending_menu_alias = r.str();
    g_pending_menu_verbs.clear();
    long nverbs = r.num();
    for (long i = 0; i < nverbs && r.ok; i++)
        g_pending_menu_verbs.push_back(r.str());
    g_pane_expanded = r.str();
    g_status_line = r.str();
    g_location_line = r.str();
    g_autorestore_panel = r.str();
    g_cstatus_vals.clear();
    long ncells = r.num();
    for (long i = 0; i < ncells && r.ok; i++) {
        std::string id = r.str();
        g_cstatus_vals[id] = r.str();
    }
    g_autorestore_rngs.clear();
    long nrngs = r.num();
    for (long i = 0; i < nrngs && r.ok; i++) {
        std::string src = r.str();
        std::array<uint32_t, 4> s;
        for (int j = 0; j < 4; j++)
            s[(size_t) j] = (uint32_t) r.num();
        g_autorestore_rngs.emplace_back(src, s);
    }
    g_out_log.clear();
    long nchunks = r.num();
    for (long i = 0; i < nchunks && r.ok; i++) {
        OutChunk c;
        c.html = r.str();
        c.text = u32_from_utf8(r.str());
        c.image = r.num() != 0;
        g_out_log.push_back(c);
    }
    g_sections.clear();
    long nsecs = r.num();
    for (long i = 0; i < nsecs && r.ok; i++) {
        OutSection s;
        s.name = r.str();
        s.start = (size_t) r.num();
        long e = r.num();
        s.end = e < 0 ? (size_t) -1 : (size_t) e;
        g_sections.push_back(s);
    }
    bool wide = r.num() != 0;
    if (!r.ok || !gwin)
        return false;
    /* The restored windows already have their sizes; this is for the pane's
     * next opening. */
    set_side_pane_wide(wide, nullptr, nullptr);

    /* The redraw paths dedup against "what is already on screen"; void
     * those keys so the post-restore repaints actually repaint. */
    g_panel_last.clear();
    g_panel_id = 0;
    gm_screen_drop();
    g_pane_dirty = true;
    /* Sounds are NOT resumed from here: the app archives its sound channels
     * (playing sound, loop count, pause/volume state) in its own GUI
     * snapshot and replays them itself (SoundHandler restartAll) -- and it
     * REPLACES its sound handler wholesale during its restore, so anything
     * the terp started before that point would keep playing as an orphan no
     * stop can ever reach.  The terp's only job is keeping the staged sound
     * files alive across the restart (see stage_temp_file). */
    return true;
}

/* Re-establish the frame picture after an autorestore, unless the game's
 * own InitInterface already set one. */
void panel_restore_picture()
{
    std::string src = g_autorestore_panel;
    g_autorestore_panel.clear();
    if (src.empty() || g_panel_id || !gpanelwin || !panel_band_available())
        return;
    glui32 id = cached_media_id(g_image_ids, src, false);
    if (!id)
        return;
    g_panel_id = id;
    g_panel_last = src;
    panel_relayout();
}

/* Record `state` -- the engine as it stands, saved -- and the RNG streams
 * that go with it as what the next command runs from.  An empty state (the
 * engine could not be saved) leaves the record invalid. */
void capture_turn_start(Interp &in, const std::string &state)
{
    g_turn_start.state = state;
    g_turn_start.rngs.clear();
    in.capture_rng_streams(g_turn_start.rngs);
    g_turn_start.undo = in.capture_undo_history();
    g_turn_start.valid = !state.empty();
}

/* Autosave at the parser prompt: the record (the engine snapshot, in the
 * same v1 format SAVE writes, and the undo history), the frontend blob, the
 * Glk library plist, and the GUI snapshot request, in that order. */
void aslx_do_autosave(Interp &in)
{
    if (!question_autosave_wanted()) {
        /* Not saved, so the next command captures its own start state. */
        g_turn_start.valid = false;
        return;
    }
    AutosaveRecord rec;
    rec.state = in.save_game(story_path());
    if (rec.state.empty())
        return;
    capture_turn_start(in, rec.state);
    rec.undo = g_turn_start.undo;
    question_autosave_write(rec, aslx_encode_frontend(in));
}

/* Just before a command runs: make sure g_turn_start holds the state it
 * runs from, in case a disambiguation menu wants to autosave under it.
 * Usually the prompt's autosave already put it there. */
void note_turn_start(Interp &in, const std::string &cmd)
{
    g_turn_command = cmd;
    g_turn_answers.clear();
    g_boot_turn = false;
    if (!gli_enable_autosave || g_turn_start.valid)
        return;
    capture_turn_start(in, in.save_game(story_path()));
}

/* Under a disambiguation menu: the turn's starting state and the replay
 * record, and a blob with the windows as they are now (menu on screen) but
 * the RNG streams as they were then.  Skipped when the engine has moved on
 * since the turn started other than through the turn itself (a timer tick,
 * a link's ASLEvent): replaying the command would not arrive here.
 * `at_wait` is for a keypress wait, which is saved whatever the last Glk
 * event was: the game reaches it once, with no prompt loop for a timer or a
 * resize to send round again, and the one a timed pause leads up to has a
 * timer event behind it. */
void aslx_do_menu_autosave(Interp &in, bool at_wait)
{
    if (!(at_wait ? gli_enable_autosave != 0 : question_autosave_wanted()))
        return;
    AutosaveRecord rec;
    rec.replay = true;
    rec.answers = g_turn_answers;
    if (g_turn_start.valid) {
        rec.state = g_turn_start.state;
        rec.undo = g_turn_start.undo;
        rec.command = g_turn_command;
        question_autosave_write(rec, aslx_encode_frontend(in, &g_turn_start.rngs));
    } else if (g_boot_turn) {
        const RngStreams none;
        question_autosave_write(rec, aslx_encode_frontend(in, &none));
    }
}

void start_replay(Interp &in)
{
    auto none = [](const std::string &) {};
    mute_hook(in, &Interp::print, none);
    mute_hook(in, &Interp::clear_screen, [] {});
    mute_hook(in, &Interp::disable_command_links, [] {});
    mute_hook(in, &Interp::start_output_section, none);
    mute_hook(in, &Interp::end_output_section, none);
    mute_hook(in, &Interp::hide_output_section, none);
    mute_hook(in, &Interp::show_picture, none);
    mute_hook(in, &Interp::play_sound, [](const std::string &, bool, bool) {});
    mute_hook(in, &Interp::stop_sound, [] {});
    mute_hook(in, &Interp::do_pause, [](int) {});
    g_replaying = true;
}

void end_replay(Interp &in)
{
    if (!g_replaying)
        return;
    for (const auto &unmute : g_replay_unmute)
        unmute(in);
    g_replay_unmute.clear();
    g_replaying = false;
    g_replay_answers.clear();
}

/* The next recorded answer, which must be of `kind` ('k' menu key, 'c' menu
 * cancelled, 'y'/'n' question, 'i' input line, 'w' keypress).  False when the record has
 * run out or does not match -- for a menu the first is where the replay was
 * meant to stop; anything else means the replay went another way. */
bool replay_answer(const char *kinds, std::string *text)
{
    if (g_replay_answers.empty())
        return false;
    const std::string &a = g_replay_answers.front();
    if (a.empty() || !strchr(kinds, a[0]))
        return false;
    if (text)
        *text = a;
    g_turn_answers.push_back(a);
    g_replay_answers.erase(g_replay_answers.begin());
    return true;
}

bool replay_reached(Interp &in, const char *where)
{
    if (!g_replay_answers.empty()) {
        replay_diverged(in, where);
        return false;
    }
    end_replay(in);
    return true;
}

/* A prompt the turn (or the start script) put up: have the read_line that
 * shows it autosave.  Not while a replay is running, nor for the prompt it
 * ends at -- that one is the autosave on disk. */
void arm_prompt_autosave(Interp &in, bool resumed)
{
    g_menu_autosave_interp = resumed || g_replaying ? nullptr : &in;
}

/* The replay went somewhere the recording did not: stop muting and let the
 * player take it from here. */
void replay_diverged(Interp &in, const char *where)
{
    fprintf(stderr, "aslxglk: autosave replay of \"%s\" diverged at %s\n",
            g_replay_command.c_str(), where);
    end_replay(in);
}

/* Core's own ShowMenu (the function, and Ask on top of it) is open: its
 * options sit in an output section the answer will hide. */
bool game_menu_pending(Interp &in)
{
    Element *game = in.world().find("game");
    const Value *v = game ? in.resolve_field(game, "menucallback") : nullptr;
    return v && v->type != Value::Type::Null;
}

#endif /* SPATTERLIGHT */

}  // namespace aslxglk
