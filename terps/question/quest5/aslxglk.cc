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

// aslxglk.cc -- Glk frontend for the native Quest 5 (.aslx / .quest) engine.
//
// This is the "second engine sharing the Glk frontend" (see test/quest5/README.md):
// questionglk.cc's glk_main sniffs the story file and hands Quest 5 games to
// aslx_glk_main() below, while .asl/.cas keep going to the classic runner.
//
// The interaction model is the QuestViva pending-callback port (§3): the
// engine never blocks mid-script -- `get input` / `show menu` / `ask` / `wait`
// register callbacks and the prompt loop here asks the engine what it is
// waiting for (pending_menu / pending_question / pending_wait / else a
// command line) and resolves it with the next player input.  The expression
// forms of ShowMenu and Ask are the genuinely synchronous prompts (both AWAIT
// mid-expression); they get a nested input loop via Interp::menu_provider and
// Interp::ask_provider.
//
// Output arrives as Quest's constrained HTML subset (Core's OutputText emits
// <span style=".."> text </span><br/> chunks; game text adds <b>/<i>/<u> and
// {command:}/{object:} links).  render_html() maps it onto Glk styles and
// hyperlinks: <br/> is the newline, bold/italic/underline come from both the
// tags and the span-style CSS, cmdlinks become Glk hyperlinks whose click
// sends their data-command (or fires their ASLEvent) as if typed.  Everything
// else -- fonts, colours, alignment -- is deliberately dropped (§4: don't
// write a browser).  Images and sounds are presentation-milestone work.

#include "aslxglk-internal.hh"

namespace aslxglk {

/* Core's CapFirst is .NET ToUpper on the first CHARACTER, not the first
 * ASCII byte -- Greek pane labels ("τόξο") and room names must capitalize
 * ("Τόξο") exactly as under QuestViva.  questglk::cap_first stays ASCII-only
 * for the Quest 4 frontend (Question strings are Latin-1-ish, passthrough there);
 * here the engine's .NET-invariant simple case map is at hand, so this shadows
 * the shared one rather than importing it. */
std::string cap_first(const std::string &s)
{
    return aslx::utf8_case(s, true, true);
}

/* Arm / disarm hyperlink input on both clickable windows.  Every event loop
 * here wants the pair -- the main transcript's cmdlinks and the side pane's
 * object and compass links -- and a Glk that reports no hyperlink support
 * gets neither. */
void request_hyperlinks()
{
    if (!g_hyperlinks)
        return;
    g_links_requested = true;
    glk_request_hyperlink_event(gwin);
    if (gobjwin)
        glk_request_hyperlink_event(gobjwin);
}

void cancel_hyperlinks()
{
    if (!g_hyperlinks)
        return;
    g_links_requested = false;
    glk_cancel_hyperlink_event(gwin);
    if (gobjwin)
        glk_cancel_hyperlink_event(gobjwin);
}

#ifdef SPATTERLIGHT

void turn_start_changed() { g_turn_start.valid = false; g_boot_turn = false; }

#endif

/* One evtype_Timer arrived.  While the map marker glides, the shared timer
 * runs at GM_ANIM_TICK_MS: advance the glide (graphics-window output only,
 * so it is legal even with a line request pending) and report whether a
 * whole engine second has also elapsed; on the plain 1-second heartbeat
 * every event is an engine second. */
bool timer_event_second(Interp &in)
{
    if (g_timer_ms == 0 || g_timer_ms >= 1000)
        return true;
    grid_map_anim_tick();
    redraw_grid_map();
    g_timer_frac_ms += (int) g_timer_ms;
    bool due = g_timer_frac_ms >= 1000;
    if (due)
        g_timer_frac_ms -= 1000;
    update_timer_request(in);  /* glide over -> heartbeat cadence, or off */
    return due;
}

/* ------------------------------------------------------------------ core -- */

/* Locate the bundled Core libraries: $ASLX_CORE, else next to the terp
 * binary (../Resources/aslx-core inside the app bundle; ./aslx-core for
 * development builds). */
std::string core_dir_path()
{
    const char *env = getenv("ASLX_CORE");
    if (env && *env)
        return env;
#ifdef __APPLE__
    char exe[4096];
    uint32_t size = sizeof exe;
    if (_NSGetExecutablePath(exe, &size) == 0) {
        char real[4096];
        if (realpath(exe, real)) {
            std::string dir = real;
            size_t slash = dir.rfind('/');
            if (slash != std::string::npos)
                dir.erase(slash);
            std::string cands[] = {
                dir + "/../Resources/aslx-core",   /* app bundle */
                dir + "/aslx-core",                /* dev layout */
            };
            for (const std::string &c : cands)
                if (access((c + "/Core.aslx").c_str(), R_OK) == 0)
                    return c;
        }
    }
#endif
    return "";
}

/* Print any warnings the engine queued since the last check ("'play sound'
 * is not supported yet" etc.), bracketed and emphasized. */
void flush_warnings(World &w, size_t &seen)
{
    /* A replayed turn's warnings were printed the first time round. */
    if (g_replaying)
        seen = w.warnings.size();
    for (; seen < w.warnings.size(); seen++) {
        glk_set_style(style_Emphasized);
        glk_put_string((char *) "[");
        put_uni_string(w.warnings[seen]);
        glk_put_string((char *) "]\n");
        glk_set_style(style_Normal);
    }
}

/* Undo a turn after the story ended, putting the player back in play.
 *
 * QuestViva simply stops when a game finishes -- there is no undo to reach,
 * because there is no prompt left to type it at.  The classic runner has
 * offered a post-death undo for as long as it has had the menu, and it is the
 * one choice there that does not throw the session away, so the two ports now
 * agree on offering it.  Everything below the frontend is Core's own undo:
 * rollback_transaction is what the `undo` command's UndoScript runs, prints
 * the game's own UndoTurn template, and honours whatever the game logged.
 * Only clearing `finished` is ours -- `finish` sets a runtime flag, not an
 * element field, so no undo action recorded it. */
bool post_game_undo(Interp &in)
{
    /* A game stopped by the script-error breaker has not "ended" so much as
     * broken; rolling one turn back would just run into the same wall. */
    if (in.script_errors_fatal() || !in.undo_available()) {
        glk_put_string((char *) NOTHING_TO_UNDO);
        return false;
    }
    Context ctx;
    in.rollback_transaction(ctx);
    in.world().finished = false;
    in.drain_on_ready();
    return true;
}

/* After the game ends: undo, restore, restart or quit -- the same four
 * choices, in the same words, as the classic runner's menu (the text and the
 * matching are shared, in questglk-common.cc). */
SessionEnd post_game_menu(Interp &in, std::string &restore_data)
{
    post_game_menu_print();
    for (;;) {
        InResult r = read_line(in, true, "> ");
        if (r.kind != InEnd::Line)
            continue;
        switch (post_game_menu_match(r.text)) {
        case POSTGAME_UNDO:
            if (post_game_undo(in))
                return SessionEnd::Resume;
            break;              /* nothing to undo; ask again */
        case POSTGAME_RESTORE:
            if (do_restore_ui(restore_data))
                return SessionEnd::Restore;
            break;              /* cancelled or unreadable; ask again */
        case POSTGAME_RESTART:
            return SessionEnd::Restart;
        case POSTGAME_QUIT:
            glk_put_string((char *) QUIT_FAREWELL);
            return SessionEnd::Quit;
        default:
            post_game_menu_reprompt();
            break;
        }
    }
}

/* Arm the shared Glk timer to the fastest cadence anyone needs: the map
 * marker glide's fast tick while one is in flight, else the 1-second
 * heartbeat while any engine timer is enabled, else off.  Gated on the
 * real-time preference like the classic runner, so the deterministic import
 * tests see no wall-clock events (a glide never starts without it either --
 * grid_map_command snaps). */
void update_timer_request(Interp &in)
{
    bool heart = gli_sa_delays && in.next_timer_seconds() > 0;
    glui32 want = grid_map_animating() ? GM_ANIM_TICK_MS
                : heart ? 1000 : 0;
    if (want != g_timer_ms) {
        glk_request_timer_events(want);
        if (g_timer_ms == 0)
            g_timer_frac_ms = 0;  /* fresh cadence, fresh second */
        g_timer_ms = want;
    }
}

/* The EXPRESSION forms of ShowMenu/ask: the engine blocks inside eval
 * until the provider returns, so the half-run script is on the C++ stack
 * and not in any snapshot.  A timer tick or a link click can open one
 * while the parser prompt's autosave hook is armed, so suspend it for the
 * duration -- see AutosaveSuspend.  (The script-command forms are pending
 * engine state and are safe; they run from the prompt loop.)
 *
 * Each answer is also recorded for the turn (g_turn_answers), and a
 * replay answers from the record instead.  The turn's own state is not
 * the parser prompt's, though, and these do autosave on its terms: the
 * state the turn started from plus the answers so far (see
 * g_turn_start). */
static bool provide_menu(Interp &in, const MenuData &m, std::string &key)
{
    [[maybe_unused]] AutosaveSuspend no_autosave;
    bool resumed = false;
#ifdef SPATTERLIGHT
    std::string a;
    if (g_replaying) {
        if (replay_answer("kc", &a)) {
            key = a.substr(1);
            return a[0] == 'k';
        }
        resumed = replay_reached(in, "a menu");
    }
    arm_prompt_autosave(in, resumed);
#endif
    bool picked = run_menu_ui(in, m, key, resumed);
#ifdef SPATTERLIGHT
    g_menu_autosave_interp = nullptr;
    g_turn_answers.push_back(picked ? "k" + key : "c");
#endif
    return picked;
}

/* The expression form of `ask` -- see provide_menu. */
static bool provide_answer(Interp &in, const std::string &q, bool &answer)
{
    [[maybe_unused]] AutosaveSuspend no_autosave;
    bool resumed = false;
#ifdef SPATTERLIGHT
    std::string a;
    if (g_replaying) {
        if (replay_answer("yn", &a)) {
            answer = a[0] == 'y';
            return true;
        }
        resumed = replay_reached(in, "a question");
    }
    arm_prompt_autosave(in, resumed);
#endif
    bool answered = run_question_ui(in, q, answer, resumed);
#ifdef SPATTERLIGHT
    g_menu_autosave_interp = nullptr;
    if (answered)
        g_turn_answers.push_back(answer ? "y" : "n");
#endif
    return answered;
}

/* The expression form of `get input` -- see provide_menu. */
static bool provide_input(Interp &in, std::string &text)
{
    [[maybe_unused]] AutosaveSuspend no_autosave;
    bool resumed = false;
#ifdef SPATTERLIGHT
    std::string a;
    if (g_replaying) {
        if (replay_answer("i", &a)) {
            text = a.substr(1);
            return true;
        }
        resumed = replay_reached(in, "an input line");
    }
    arm_prompt_autosave(in, resumed);
#endif
    bool got = run_input_ui(in, text, resumed);
#ifdef SPATTERLIGHT
    g_menu_autosave_interp = nullptr;
    if (got)
        g_turn_answers.push_back("i" + text);
#endif
    return got;
}

/* A taken page choice retires the options above it -- see g_links_spent.
 * Called before the new page prints, so the links it is about to add are
 * above the mark and stay live. */
static void retire_command_links()
{
    /* A spent link is never dispatched again (val1 > g_links_spent), so
     * its payload strings are dead -- release them, or a long gamebook
     * session grows the vector's heap without bound. */
    for (size_t i = g_links_spent; i < g_links.size(); i++)
        g_links[i] = LinkAction{};
    g_links_spent = g_links.size();
}

/* The JS callback bridge: queue only -- fire_js_events runs them between
 * turns, so a callback never lands in the middle of the script that
 * triggered it (the browser's setTimeout would not either). */
static void queue_js_event(const std::string &fn, const std::string &arg)
{
    /* JS.HookClicks: the game has installed a click-anywhere handler --
     * see g_click_hook. */
    if (fn == "HookClicks") {
        g_click_hook = true;
        return;
    }
    /* Spondre-style: the callback name rides in the call's last argument. */
    if (!arg.empty()) {
        g_js_events.push_back(std::make_pair(fn, arg));
        return;
    }
    /* Moquette-style: a zero-arg JS.func() whose bundled body fires the
     * ASLEvent(s).  Queue them in the same (func, "name param") shape
     * fire_js_events splits, so the two paths converge there. */
    auto it = g_js_callbacks.find(fn);
    if (it == g_js_callbacks.end())
        return;
    for (const auto &ev : it->second)
        g_js_events.push_back(std::make_pair(
            fn, ev.second.empty() ? ev.first
                                  : ev.first + " " + ev.second));
}

/* Wire every host hook the engine offers to this frontend.  Split out of
 * run_session, which otherwise reads as a hundred lines of assignment before
 * the boot and turn loop it is really about; the hooks are installed once, at
 * the top of a session, and nothing below re-reads them.
 *
 * `restart_requested` is the one piece of session state a hook writes: Core's
 * `restart` (via JS.eval window.location.reload) has to end the session from
 * inside a turn, and the turn loop checks the flag at its next safe point. */
void install_host_hooks(Interp &in, bool &restart_requested)
{
    /* The synchronous prompts -- see provide_menu. */
    in.menu_provider = [&in](const MenuData &m, std::string &key) {
        return provide_menu(in, m, key);
    };
    in.ask_provider = [&in](const std::string &q, bool &answer) {
        return provide_answer(in, q, answer);
    };
    in.resource_provider = resource_bytes;
    in.input_provider = [&in](std::string &text) { return provide_input(in, text); };
    in.request_save = [&in] { do_save_ui(in); };
    /* Core's `restart` command (via JS.eval window.location.reload).  The
     * turn finishes normally -- under a browser reload the server-side turn
     * also runs to completion -- and the session ends at the next loop
     * check, rebooting through the same teardown as the post-game menu. */
    in.request_restart = [&restart_requested] { restart_requested = true; };
    /* A custom status panel's cells: "$('#hits-span').html('12/20')".  Only
     * while there is a panel -- a write to an element that is not on the
     * page yet is lost in the browser as well. */
    in.js_eval = [](const std::string &js) {
        std::string id, value;
        if (g_cstatus.rows.empty() ||
            !aslxform::parse_jquery_html(js, id, value))
            return;
        g_cstatus_vals[id] = plain_text(value);
        g_pane_dirty = true;
    };
    /* Pane lists (UpdateListsAsync): only subscribe when this Glk could open
     * a side pane at startup -- an unset hook skips the whole scope
     * computation, like QuestViva with no UpdateList listener.  The hook just
     * snapshots; redraw_side_pane repaints at the next safe point. */
    if (g_use_objpane) {
        in.update_list = [](const char *type, const std::vector<ListData> &items) {
            if (strcmp(type, "inventory") == 0)
                g_pane_inv = items;
            else if (strcmp(type, "placesobjects") == 0)
                g_pane_places = items;
            else if (strcmp(type, "exits") == 0)
                g_pane_exits = items;
            g_pane_dirty = true;
        };
    }
    /* Status attributes land in the banner, next to the room name -- or in
     * the pane, when the banner is too narrow (see update_banner). */
    in.update_status = [](const std::string &html) {
        std::string line = plain_text(html, " | ");
        if (line != g_status_line && g_status_in_pane)
            g_pane_dirty = true;
        g_status_line = line;
    };
    in.update_location = [](const std::string &text) {
        g_location_line = plain_text(text);
    };
    /* The command box's visibility -- see g_command_bar. */
    in.show_command_bar = [](bool shown) { g_command_bar = shown; };
    /* The panes' visibility -- see g_panes_visible.  Marking the lists dirty
     * makes the next safe-point redraw apply the change even when no list
     * content moved. */
    in.panes_visible = [](bool shown) {
        g_panes_visible = shown;
        g_pane_dirty = true;
    };
    /* TextFX text is transcript text -- the engine hands over the reference
     * player's end-state span, printed through the ordinary engine-output
     * path (PromptBreak retraction and all). */
    in.textfx_text = [&in](const std::string &html) { in.print(html); };
#ifdef SPATTERLIGHT
    /* ClearScreen wipes the story window; the hook runs inline in script
     * order, so the texts before and after the clear land on the right sides
     * of it.  Spatterlight only: CheapGlk's window_clear is a flood of
     * newlines, and the oracle's transcripts keep the cleared text -- leaving
     * the hook unset keeps the smoke harness byte-stable with both. */
    in.clear_screen = clear_screen_ui;
#endif
    /* A taken page choice retires the options above it. */
    in.disable_command_links = retire_command_links;
    /* Output sections -- the reference player's hideable <div>s, retracted
     * here with garglk_unput_string_count_uni. */
    in.start_output_section = [](const std::string &n) { start_output_section(n); };
    in.end_output_section = [](const std::string &n) { end_output_section(n); };
    in.hide_output_section = [](const std::string &n) { hide_output_section(n); };
    /* The JS callback bridge -- queue only, see queue_js_event. */
    in.js_event_bridge = queue_js_event;
    /* Script errors go to stderr, not the page -- the reference web player
     * keeps them in the browser's JavaScript console, off the transcript. The
     * engine still logs every one (Interp::errors()), and headless output is
     * untouched: only a host that installs this hook diverts them. */
    in.script_error = [](const std::string &message) {
        fprintf(stderr, "%s\n", message.c_str());
    };
    /* The grid map needs only filled rectangles, so plain gestalt_Graphics
     * gates it.  Unset (CheapGlk: the smoke harness), the engine never even
     * evaluates the paint arguments, keeping headless transcripts intact. */
    if (glk_gestalt(gestalt_Graphics, 0))
        in.grid_draw = grid_map_command;
    /* Pre-540 `picture` shows the image through the UI directly (no <img>
     * print), and JS.setPanelContents is the picture frame. Only claim them
     * when this Glk can actually draw -- unset, the engine keeps its
     * one-time "not supported" warning / silent ignore. */
    if (glk_gestalt(gestalt_Graphics, 0) &&
        glk_gestalt(gestalt_DrawImage, wintype_TextBuffer)) {
        in.show_picture = [](const std::string &filename) {
            if (draw_image_inline(filename))
                glk_put_char('\n');
        };
        g_panel_last.clear();   /* a fresh session redraws its frame */
        in.set_panel_contents = panel_contents;
    }
#ifdef SPATTERLIGHT
    /* Sounds play through a Glk sound channel; a synchronous `play sound`
     * blocks in the hook until the finish notification (or a keypress).
     * Installed unconditionally so an unplayable sound "finishes" instantly
     * and the turn always continues -- only the hook-less headless engine
     * (the oracle-parity harnesses) keeps the abandoned-turn semantics. */
    in.play_sound = [&in](const std::string &f, bool sync, bool loop) {
        play_sound_ui(in, f, sync, loop);
    };
    in.stop_sound = stop_sound_ui;
#endif

    /* The pre-v540/v550 blocking `request (Wait)` / `request (Pause, ms)`
     * (Core's WaitForKeyPress / Pause). Standard Glk keypress + timer events,
     * so wired on every host; both self-suppress in deterministic mode, so the
     * smoke harness never blocks. */
    in.do_wait = [&in] { do_wait_ui(in); };
    in.do_pause = [&in](int ms) { do_pause_ui(in, ms); };
}

#ifdef SPATTERLIGHT
/* A Spatterlight autosave: the engine state first (restore_game overlays
 * the snapshot onto this freshly loaded world -- silent), then the Glk
 * library from the plist, then the frontend globals from the blob.  The
 * boot then continues down the saved-game path: InitInterface re-runs
 * (repainting the grid map and panes from restored engine state),
 * StartGame and begin_timers are skipped.  The app restores the window
 * contents from its own GUI snapshot.
 *
 * Returns whether the autosave was taken under a prompt of the start script
 * (a boot replay) rather than at a state the engine could snapshot. */
static bool boot_from_autosave(Interp &in)
{
    g_autorestore_pending = false;
    AutosaveRecord rec;
    std::string blob, err;
    bool have = question_autosave_read(&rec);
    /* Taken under a prompt of the start script: there is no state, the
     * game boots as new -- muted, and answering from the record, up to
     * that prompt. */
    bool boot_replay = have && rec.boot_replay();
    bool ok = have &&
              (boot_replay || in.restore_game(rec.state, err)) &&
              question_autosave_restore_library(&blob) &&
              aslx_recover_frontend(blob);
    if (!ok) {
        /* Bad autosave: delete it and restart in a fresh process rather
         * than continuing from half-applied state. */
        question_autosave_discard();
        win_reset();
        exit(0);
    }
    question_autosave_restore_library_late();
    g_autorestore_replay = rec.replay;
    g_replay_command = rec.command;
    g_replay_answers = rec.answers;
    /* The undo history goes back on while the world is exactly as saved;
     * it starts logging again once the boot below is over.  One that will
     * not read back costs the player their UNDOs, not the session. */
    if (!boot_replay)
        in.restore_undo_history(rec.undo);
    /* The library snapshot re-arms the Glk timer interval that was in
     * force when the autosave was taken (TempLibrary restores
     * gtimerinterval) -- typically the map glide's 33 ms cadence, since
     * the snapshot is taken at the prompt right after a move started a
     * glide.  This process's mirror of that interval starts at 0, where
     * timer_event_second reads EVERY tick as a whole engine second: the
     * prompt loop would then cancel and re-request line input 30 times a
     * second, and Spatterlight scrolls the buffer to the bottom on each
     * request -- the window fights any attempt to scroll back.  Cancel
     * the restored interval and let update_timer_request below arm the
     * cadence this session actually needs. */
    glk_request_timer_events(0);
    g_timer_ms = 0;
    g_timer_frac_ms = 0;
    /* A verb menu was up when the snapshot was taken: the window ends at
     * the menu's own prompt, not the parser's, so re-enter the menu and
     * leave the parser-prompt reprint suppression alone -- the next parser
     * prompt is a fresh one, printed after the chosen command has run. */
    if (!g_pending_menu_alias.empty())
        g_autorestore_menu_resume = true;
    /* Taken under a disambiguation menu: the engine state is the turn's
     * start, and the turn loop replays the turn back to the menu, whose
     * prompt (not the parser's) ends the window. */
    else if (!g_autorestore_replay)
        g_autorestore_reentry = true;
    if (boot_replay) {
        g_autorestore_replay = false;
        g_autorestore_rngs.clear();
        start_replay(in);
    } else {
        g_boot_turn = false;
        g_turn_start.state = std::move(rec.state);
        g_turn_start.rngs = g_autorestore_rngs;
        g_turn_start.undo = std::move(rec.undo);
        g_turn_start.valid = true;
    }
    return boot_replay;
}
#endif

/* The three things a script can leave pending instead of a command line: a
 * menu, a yes/no question and a wait.  Each asks the player (or, replaying
 * an autosaved turn, the record) and hands the answer to the engine. */
static void answer_pending_menu(Interp &in, const MenuData &m)
{
    std::string key;
    bool resumed = false;
#ifdef SPATTERLIGHT
    if (g_replaying) {
        std::string a;
        if (replay_answer("kc", &a)) {
            key = a.substr(1);
            in.set_menu_response(a[0] == 'k' ? &key : nullptr);
            return;
        }
        /* The record is used up: this is the menu the autosave was
         * taken under, already on screen.  Just wait for a choice. */
        resumed = replay_reached(in, "a menu");
    }
    arm_prompt_autosave(in, resumed);
#endif
    bool picked = run_menu_ui(in, m, key, resumed);
#ifdef SPATTERLIGHT
    g_menu_autosave_interp = nullptr;
    g_turn_answers.push_back(picked ? "k" + key : "c");
#endif
    in.set_menu_response(picked ? &key : nullptr);
}

static void answer_pending_question(Interp &in, const std::string &q)
{
    bool answer = false;
    bool resumed = false;
#ifdef SPATTERLIGHT
    if (g_replaying) {
        std::string a;
        if (replay_answer("yn", &a)) {
            in.set_question_response(a[0] == 'y');
            return;
        }
        resumed = replay_reached(in, "a question");
    }
    arm_prompt_autosave(in, resumed);
#endif
    bool answered = run_question_ui(in, q, answer, resumed);
#ifdef SPATTERLIGHT
    g_menu_autosave_interp = nullptr;
    if (answered)
        g_turn_answers.push_back(answer ? "y" : "n");
#endif
    if (answered)
        in.set_question_response(answer);
}

static void answer_pending_wait(Interp &in)
{
    bool resumed = false;
#ifdef SPATTERLIGHT
    if (g_replaying) {
        if (replay_answer("w")) {
            in.finish_wait();
            return;
        }
        /* The wait the autosave was taken under. */
        resumed = replay_reached(in, "a wait");
    }
#endif
    read_keypress(in, resumed);
    if (in.pending_wait()) {
#ifdef SPATTERLIGHT
        g_turn_answers.push_back("w");
#endif
        in.finish_wait();
    }
}

/* How a parser prompt ended: with a turn (or a metaverb) to refresh after,
 * with nothing typed, or with a RESTORE that ends the session. */
enum class TurnEnd { Done, Idle, Restore };

/* One command line: prompt, read, and either handle it as a frontend
 * metaverb or send it to the engine.  `replay_sent` is whether a replayed
 * turn's command has gone out yet; `restore_data` is filled by RESTORE. */
static TurnEnd run_parser_prompt(Interp &in, std::string &restore_data,
                                 [[maybe_unused]] bool &replay_sent)
{
    /* Parser-bound lines.  Prompt-first (g_prompt_first): print a
     * standard "> " prompt at the input point, host-echo the accepted
     * line, and arm g_swallow so the game side's own echo of the
     * command ("> cmd" -- Core's echocommand, or the engine for
     * pre-v520) is dropped instead of doubling it.  Legacy mode (no
     * line-input echo control, e.g. CheapGlk): no prompt, no host
     * echo -- the game-side echo is the only record, like the
     * reference player, whose input box leaves no other trace.  A
     * pending `get input` is host-owned either way: the engine
     * consumes the line without any game-side echo. */
    bool host_owned = in.command_override();
    /* With the command box hidden (a gamebook, or any game with
     * showcommandbar off) the page and its links are the interface,
     * so no prompt is drawn.  A line is still accepted, though: the
     * reference player backs a hidden box with a click hook this
     * engine cannot emulate (spondre's glue.js sends a command for
     * ANY click in the output, which is the only way past its title),
     * and a host that both hides the prompt and refuses a line leaves
     * no way in at all.  Hiding the box must cost the prompt, not the
     * player's last input path.  A `get input` is host-owned and
     * re-shows the box, so it prompts as usual. */
    bool bar_shown = g_command_bar || host_owned;
    bool prompted = (g_prompt_first || host_owned) && bar_shown;
    const char *prompt = prompted ? "\n> " : nullptr;
    bool prompt_on_screen = false;
#ifdef SPATTERLIGHT
    /* The first prompt after an autorestore is already on screen --
     * the GUI snapshot was taken just after it was printed.  Keep
     * the prompt text (an interrupting tick must still know what to
     * retract and reprint); just skip printing it again. */
    if (g_autorestore_reentry) {
        g_autorestore_reentry = false;
        prompt_on_screen = prompt != nullptr;
    }
    /* Autosave while the parser prompt is up.  A pending `get
     * input` has a callback that is not in the engine snapshot: it
     * autosaves as a prompt of the turn that asked. */
    if (!host_owned)
        g_autosave_interp = &in;
    if (g_replaying && host_owned && g_replay_answers.empty()) {
        /* The `get input` the autosave was taken under. */
        end_replay(in);
        prompt_on_screen = prompt != nullptr;
    } else if (host_owned) {
        arm_prompt_autosave(in, false);
    }
#endif
    InResult r;
#ifdef SPATTERLIGHT
    std::string a;
    if (g_replaying && !host_owned && !replay_sent) {
        replay_sent = true;
        r = {InEnd::Line, g_replay_command};
    } else if (g_replaying && host_owned && replay_answer("i", &a)) {
        r = {InEnd::Line, a.substr(1)};
    } else {
        /* The turn ended, or asked for something else, short of
         * the prompt it was recorded under. */
        if (g_replaying)
            replay_diverged(in, "the prompt");
#endif
        r = read_line(in, prompted, prompt,
                      /*want_line=*/true, prompt_on_screen);
#ifdef SPATTERLIGHT
    }
    g_autosave_interp = nullptr;
    g_menu_autosave_interp = nullptr;
#endif
    if (r.kind == InEnd::State || r.kind == InEnd::Event)
        { /* re-dispatch on the new engine state */ }
    else {
        std::string cmd = trim(r.text);
        if (cmd.empty())
            return TurnEnd::Idle;
        /* RESTORE is the frontend's own: Quest has no restore command
         * (loading is a UI action, like the classic desktop player's
         * menu), while `save` stays with Core's, via request_save. */
        if (match_restore_command(cmd, host_owned)) {
            echo_metaverb_command(cmd);
            if (do_restore_ui(restore_data))
                return TurnEnd::Restore;
        } else if (!handle_transcript_command(cmd) &&
                   !handle_help_command(cmd) &&
                   !handle_sidebar_command(in, cmd) &&
                   /* Not under a `get input`: unlike the metaverbs
                    * above, STATUS is a plain word a game may well be
                    * waiting to hear as an answer. */
                   !(!host_owned && handle_status_command(cmd)) &&
                   !handle_verbs_command(in, cmd)) {
            /* Arm the echo swallow unconditionally: whether the game
             * side echoes cannot be predicted from game.echocommand,
             * because a game may carry its own copy of Core's
             * HandleCommand that echoes regardless (First Times, ASL
             * 520, sets no echocommand yet still prints "> cmd").
             * Detecting the echo as it arrives covers both -- an
             * unechoed turn just disarms on its first chunk. */
            if (g_prompt_first && !host_owned) {
                /* A line typed with no prompt drawn (hidden box) has
                 * nothing on screen to show for it, and the swallow
                 * below drops the game's echo too -- so record it
                 * here.  A CLICKED link needs no such record: the
                 * link itself is the visible act, and echoing it
                 * would put "> Page2" through a gamebook that is
                 * meant to read as prose. */
                if (!bar_shown && r.kind == InEnd::Line &&
                    !g_replaying)
                    echo_metaverb(cmd);
                g_swallow = 2;
                g_swallow_cmd = cmd;
            }
#ifdef SPATTERLIGHT
            /* A typed answer to Core's ShowMenu: the page shows no
             * echo for it and hides the option list, which can only
             * be retracted together with the prompt line under it
             * (see InlinePromptTrail). */
            if (prompted && g_manual_echo && !host_owned &&
                !g_replaying && !g_sections.empty() &&
                game_menu_pending(in)) {
                OutChunk c;
                c.text = u32_from_utf8(std::string(prompt) + r.text +
                                       "\n");
                g_out_log.push_back(c);
                trim_out_log();
            }
            /* A line under a `get input` answers it; any other
             * starts a turn. */
            if (host_owned)
                g_turn_answers.push_back("i" + cmd);
            else
                note_turn_start(in, cmd);
#endif
            in.send_command(cmd);
            g_swallow = 0;
            /* Drain first: the hint must land after the game's help
             * text, not in front of output still queued behind it. */
            in.drain_on_ready();
            /* Only for a real parser turn: with the command box hidden
             * (gamebook) HELP is not a command, and a `get input`
             * override makes it an answer to the game's question. */
            hint_system_commands(cmd, g_command_bar && !host_owned &&
                                      !g_replaying);
        }
    }
    return TurnEnd::Done;
}

/* Run one full game session, booting fresh or from a validated snapshot
 * (which do_restore_ui already probe-applied). `restore_data` is consumed on
 * entry and re-filled when the session ends with a RESTORE. */
SessionEnd run_session(const char *storyfile, std::string &restore_data)
{
    World w;
    if (!load_file(storyfile, w, core_dir_path())) {
        glk_put_string((char *) "Sorry, this Quest 5 game could not be loaded:\n");
        for (const std::string &e : w.errors) {
            glk_put_string((char *) "  ");
            put_uni_string(e);
            glk_put_char('\n');
        }
        return SessionEnd::Quit;
    }

    /* Index the game's bundled JS so a zero-arg JS.func() can fire the
     * ASLEvent completions its body would have (see index_bundled_js). */
    index_bundled_js(w);

    Interp in(w);
    in.print = render_html;
    /* Set by Core's `restart` command from inside a turn; the loop below acts
     * on it at its next safe point (see install_host_hooks). */
    bool restart_requested = false;
    install_host_hooks(in, restart_requested);

#ifdef GLK_MODULE_GARGLKTEXT
    {
        /* Game names may carry markup ("<b><i> DRACULA </b></i>"); the
         * window title wants plain text.  game_name itself must stay raw --
         * it is the save-state identity. */
        std::string title = trim(plain_text(w.game_name));
        if (!title.empty())
            garglk_set_story_title(title.c_str());
    }
#endif

    size_t warnings_seen = 0;
    /* What every turn boundary brings up to date: queued JS callbacks, new
     * warnings, the banner, the panes, the map and the timer cadence. */
    auto refresh = [&] {
        fire_js_events(in);
        flush_warnings(w, warnings_seen);
        update_banner(in);
        redraw_side_pane(in);
        redraw_grid_map();
        update_timer_request(in);
    };

    /* Saved-game boot, mirroring WorldModel.BeginInternalAsync with
     * _loadedFromSaved: timers are not re-armed, InitInterface re-runs,
     * StartGame does not, and the reference's no-transcript fallback message
     * is printed. */
    bool restored = false;
    bool autorestored = false;
#ifdef SPATTERLIGHT
    /* Nothing of an earlier session is this one's turn. */
    g_turn_start.valid = false;
    g_turn_command.clear();
    g_turn_answers.clear();
    g_boot_turn = restore_data.empty();
    bool boot_replay = false;
#endif
    if (!restore_data.empty()) {
        std::string err;
        restored = in.restore_game(restore_data, err);
        restore_data.clear();
        if (!restored) {
            /* Should not happen -- the probe validated it -- but never boot
             * half-applied state silently. */
            glk_put_string((char *) "Restore failed (");
            put_uni_string(err);
            glk_put_string((char *) "); starting a new game.\n\n");
            return SessionEnd::Restart;
        }
    }
#ifdef SPATTERLIGHT
    else if (g_autorestore_pending) {
        boot_replay = boot_from_autosave(in);
        restored = !boot_replay;
        autorestored = true;
    }
#endif

    Context boot;
    if (!restored)
        in.begin_timers();
    else
        seed_custom_status(in);
    try {
        if (w.find("InitInterface")) in.call_function("InitInterface", {}, &boot);
        if (!restored && w.find("StartGame"))
            in.call_function("StartGame", {}, &boot);
        /* BeginInternalAsync ends with UpdateListsAsync: first pane fill +
         * status attributes (a park above skips it, like the await chain). */
        in.update_lists();
    } catch (const TurnSuspended &) {
        /* synchronous `play sound` during boot: rest of Begin abandoned */
    }
    if (restored && !autorestored) {
        glk_put_string((char *) "Loaded saved game\n");
        update_banner(in);
    }
#ifdef SPATTERLIGHT
    if (autorestored)
        panel_restore_picture();
#endif
    in.drain_on_ready();
    refresh();

    /* The start script put up a character dialog: answer it before the first
     * prompt.  (A restored game has been through it already.) */
    if (g_form_pending) {
        g_form_pending = false;
#ifdef SPATTERLIGHT
        /* The dialog's answers are in no replay record: nothing after it is
         * autosaved before the first prompt, and a replay that meets one was
         * not recorded here. */
        g_boot_turn = false;
        if (g_replaying)
            replay_diverged(in, "a character dialog");
#endif
        if (!restored && w.find_function(g_form.event)) {
            run_character_dialog(in);
            refresh();
        }
    }

#ifdef SPATTERLIGHT
    /* Put every RNG stream back where the autosave left it, now that the
     * boot (InitInterface, list refills, repaints) has taken its own draws
     * -- so the restored prompt's randomness is exactly the saved one. */
    if (!g_autorestore_rngs.empty()) {
        in.restore_rng_streams(g_autorestore_rngs);
        g_autorestore_rngs.clear();
    }
    if (autorestored && !boot_replay)
        in.resume_undo_logging();
    if (g_autorestore_replay) {
        g_autorestore_replay = false;
        start_replay(in);
    }
#endif
    /* Whether a replayed turn's command has been sent; a replayed boot has
     * none to send. */
    bool replay_sent = false;
#ifdef SPATTERLIGHT
    replay_sent = boot_replay;
#endif

    /* Turn loop, then the end-of-story menu -- which may undo back into play,
     * in which case the whole thing runs again. */
    bool fatal_reported = false;
    for (;;) {
    while (!w.finished) {
        if (restart_requested)
            return SessionEnd::Restart;
        if (const MenuData *m = in.pending_menu()) {
            answer_pending_menu(in, *m);
        } else if (const std::string *q = in.pending_question()) {
            answer_pending_question(in, *q);
        } else if (in.pending_wait()) {
            answer_pending_wait(in);
        } else {
            TurnEnd end = run_parser_prompt(in, restore_data, replay_sent);
            if (end == TurnEnd::Restore)
                return SessionEnd::Restore;
            if (end == TurnEnd::Idle)
                continue;
        }
        in.drain_on_ready();
        refresh();
    }

    if (in.script_errors_fatal() && !fatal_reported) {
        fatal_reported = true;
        glk_set_style(style_Emphasized);
        glk_put_string((char *) "\n[The game has stopped because too many script"
                                " errors occurred.]\n");
        glk_set_style(style_Normal);
    }
    /* finish + reload in one turn: the browser reload wins over any menu. */
    if (restart_requested)
        return SessionEnd::Restart;
    SessionEnd ending = post_game_menu(in, restore_data);
    if (ending != SessionEnd::Resume)
        return ending;

    /* Undone back into play: repaint what the turn loop keeps current, since
     * the rolled-back turn may have moved the player or changed the lists. */
    refresh();
    }
}

/* Between sessions (RESTART/RESTORE): fresh world, fresh screen -- QuestViva
 * reloads from disk, and a restore then applies its snapshot in run_session.
 * Everything the frontend accumulated over a session goes with it: no sound
 * survives (a looping one would play over the new game), no link, pane entry,
 * frame picture or queued JS callback belongs to the game now booting, and the
 * reloaded game re-declares its command box and click handler at
 * InitInterface. */
void reset_frontend_state()
{
    stop_sound_ui();
    glk_window_clear(gwin);
    g_bold = g_italic = g_under = 0;
    apply_style();

    g_links.clear();
    g_links_spent = 0;
    g_out_log.clear();          /* the cleared window holds nothing to retract */
    g_sections.clear();

    close_side_pane();
    g_pane_inv.clear();
    g_pane_places.clear();
    g_pane_exits.clear();
    g_pane_links.clear();
    g_pane_expanded.clear();
    g_pane_dirty = false;
    g_pending_menu_alias.clear();
    g_pending_menu_verbs.clear();

    grid_map_reset();
    panel_close();
    g_status_line.clear();
    g_location_line.clear();
    g_form_pending = false;
    g_cstatus = aslxform::StatusPanel();
    g_cstatus_vals.clear();

    g_command_bar = true;
    g_panes_visible = true;
    g_click_hook = false;
    g_js_events.clear();
}

}  // namespace aslxglk

using namespace aslxglk;

/* ------------------------------------------------------------- entrypoints */

/* Sniff: a PK zip containing game.aslx, or XML whose root element is <asl>.
 * Cheap enough to run on every Question start (only the zip case reads the whole
 * file, and only after the 4-byte magic matched). */
extern "C" int aslx_is_quest5_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    unsigned char head[2048];
    size_t n = fread(head, 1, sizeof head, f);
    if (n >= 4 && memcmp(head, "PK\x03\x04", 4) == 0) {
        /* .quest package: claim it only if game.aslx is really inside.  A
         * failed ftell (-1) would otherwise become a (size_t)-1 vector. */
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (len <= 0) {
            fclose(f);
            return 0;
        }
        std::vector<uint8_t> buf((size_t) len);
        size_t got = fread(buf.data(), 1, (size_t) len, f);
        fclose(f);
        std::string out;
        return got == (size_t) len &&
               aslx::extract_game_aslx(buf.data(), got, out);
    }
    fclose(f);
    /* Raw .aslx: skip BOM/whitespace, require markup, then find the <asl
     * root tag (possibly after <?xml?> and comments). */
    size_t i = 0;
    if (n >= 3 && head[0] == 0xef && head[1] == 0xbb && head[2] == 0xbf)
        i = 3;
    while (i < n && isspace(head[i]))
        i++;
    if (i >= n || head[i] != '<')
        return 0;
    for (; i + 4 < n; i++)
        if (memcmp(head + i, "<asl", 4) == 0 &&
            (head[i + 4] == ' ' || head[i + 4] == '>' ||
             head[i + 4] == '\t' || head[i + 4] == '\r' || head[i + 4] == '\n'))
            return 1;
    return 0;
}

extern "C" void aslx_glk_main(const char *storyfile)
{
    gwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 1);
    if (!gwin)
        return;
    glk_set_window(gwin);

    glk_stylehint_set(wintype_TextGrid, style_User1, stylehint_ReverseColor, 1);
    gbanner = glk_window_open(gwin, winmethod_Above | winmethod_Fixed, 1,
                              wintype_TextGrid, 0);

    /* Prompt-first needs the library to suppress line-input echo (the host
     * echoes instead).  Without it (CheapGlk) keep the reference player's
     * game-side-echo presentation -- the smoke harness's transcripts depend
     * on it.  ASLXGLK_PROMPT_FIRST=1 forces the new mode there for testing:
     * under CheapGlk the typed line never reaches the output stream, so
     * manual echo is right for a piped harness too. */
    g_echo_control = glk_gestalt(gestalt_LineInputEcho, 0) != 0;
    g_manual_echo = g_echo_control ||
                    getenv("ASLXGLK_PROMPT_FIRST") != nullptr;
    g_prompt_first = g_manual_echo;
    g_hyperlinks = glk_gestalt(gestalt_Hyperlinks, 0) &&
                   glk_gestalt(gestalt_HyperlinkInput, wintype_TextBuffer);

    /* Probe for side-pane support (CheapGlk can't split), then close it:
     * redraw_side_pane manages the pane dynamically, and the engine only
     * computes the pane lists when the probe succeeded. */
    g_use_objpane = true;
    ensure_pane_open();
    g_use_objpane = (gobjwin != nullptr);
    close_side_pane();

    g_storyfile = storyfile;
    init_resources(storyfile);
    std::string restore_data;
#ifdef SPATTERLIGHT
    /* A complete autosave means this launch is an autorestore; run_session
     * applies it on its way into the first session. */
    g_autorestore_pending = question_autosave_exists();
#endif
    while (run_session(storyfile, restore_data) != SessionEnd::Quit)
        reset_frontend_state();
}
