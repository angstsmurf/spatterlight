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


/* aslxglk-input.cc -- line, key and menu input, the character dialog and the
 * metaverbs of the Quest 5 Glk frontend (see
 * aslxglk-internal.hh). */

#include "aslxglk-internal.hh"

namespace aslxglk {

/* ----------------------------------------------------------------- input -- */

void echo_input(const std::string &line)
{
    echo_input_line(line, true);
}

/* Echo a host-owned metaverb (RESTORE, TRANSCRIPT) the way Core's
 * echocommand records a real command -- blank line, "> cmd" -- since the
 * game side never sees it and parser-bound lines are otherwise not
 * host-echoed. */
void echo_metaverb(const std::string &cmd)
{
    glk_put_string((char *) "\n> ");
    echo_input(cmd);
}

/* The same, for a metaverb the host is about to handle: in prompt-first mode
 * read_line has already echoed the accepted line, so only legacy mode (where
 * the game side's echo is the sole record, and the game never sees a metaverb)
 * needs one here. */
void echo_metaverb_command(const std::string &cmd)
{
    if (!g_prompt_first)
        echo_metaverb(cmd);
}

/* Fire an ASLEvent hyperlink (Core menu options): WorldModel.SendEventCore,
 * which also runs FinishTurn (v540..579) and refreshes the panes -- exactly
 * what the reference player does with an onclick.  send_event catches its
 * own throws. */
void run_asl_event(Interp &in, const LinkAction &act)
{
#ifdef SPATTERLIGHT
    /* Before, not after: a prompt the event itself puts up belongs to no
     * turn, and must not autosave as part of the last one. */
    turn_start_changed();
#endif
    in.send_event(act.event_func, act.event_param);
    in.drain_on_ready();
}

/* Run the JS completion callbacks a game's own <js> animations would have
 * fired from setTimeout (see Interp::js_event_bridge).  Only an argument that
 * really names a <function> is fired -- anything else is data that happened to
 * ride along in the last slot, and firing it would invent behaviour.  One
 * completion legitimately chains into the next (spondre's title fades in three
 * stages), so this drains until quiet, with a cap in case a game builds a
 * cycle the browser would have broken with real elapsed time. */
void fire_js_events(Interp &in)
{
    const int kMaxChain = 16;
    for (int fired = 0; !g_js_events.empty(); ) {
        std::pair<std::string, std::string> ev = g_js_events.front();
        g_js_events.erase(g_js_events.begin());
        /* "Func" or "Func param": the split the games' own JS does before
         * calling ASLEvent(args[0], args[1]). */
        std::string func = trim(ev.second), param;
        size_t sp = func.find(' ');
        if (sp != std::string::npos) {
            param = trim(func.substr(sp + 1));
            func.erase(sp);
        }
        if (func.empty())
            continue;
        if (!in.world().find_function(func))
            continue;
        if (++fired > kMaxChain) {
            /* Not silently: past the cap a game is either cycling or doing
             * something this bridge models badly, and the callbacks dropped
             * here are real work the browser would have run.  Says what was
             * refused, so the truncation is diagnosable from the log rather
             * than showing up as a game that mysteriously stops advancing. */
            fprintf(stderr,
                    "JS callback chain exceeded %d in one turn: dropped '%s'"
                    " and %zu more still queued\n",
                    kMaxChain, func.c_str(), g_js_events.size());
            g_js_events.clear();
            break;
        }
        if (getenv("ASLXGLK_JS_TRACE"))
            fprintf(stderr, "[js-event] %s\n", func.c_str());
        in.send_event(func, param);
        in.drain_on_ready();
    }
}

/* True when the engine is waiting for something other than a command line --
 * the prompt loop must re-dispatch. */
bool engine_state_pending(Interp &in)
{
    return in.pending_menu() || in.pending_question() || in.pending_wait() ||
           in.world().finished;
}

/* A click on a link in the transcript, taken while a prompt is up (its line
 * request already cancelled): a "click to continue", a literal command, an
 * inline object name (which pops the object's verb menu) or an ASLEvent.
 * `act` is by value: anything here that prints (a verb menu, an ASLEvent)
 * renders new links into g_links and can reallocate it out from under a
 * reference. */
static InResult transcript_link_clicked(Interp &in, const LinkAction act,
                                        bool echo, const char *prompt)
{
    if (act.end_wait) {
        /* "click to continue".  A pending wait is what the
         * browser's endWait() releases, so release it (this arm
         * is rarely taken: the prompt loop tests pending_wait
         * before the line branch, so a click during a wait is
         * usually consumed by read_keypress). */
        if (in.pending_wait()) {
            PromptBreak pb(in, prompt);
            in.finish_wait();
            in.drain_on_ready();
            return {InEnd::Event, ""};
        }
        /* No wait: in the browser endWait() does nothing and the
         * click bubbles to the game's own click-anywhere handler,
         * which sends a command -- that, not endWait, is what
         * moves spondre off its title.  Stand in for it when the
         * game installed such a handler.  No echo: a click is its
         * own visible act (see the line-echo note below). */
        if (g_click_hook)
            return {InEnd::Command, kClickHookCommand};
        return {InEnd::Event, ""};
    }
    if (!act.command.empty()) {
        if (g_manual_echo && echo)
            echo_input(act.command);
        return {InEnd::Command, act.command};
    }
    if (!act.element.empty()) {
        /* An inline object name.  The reference player pops that
         * object's verb menu on a click -- "Just click on objects
         * and choose an action", as Behind the Door tells the
         * player -- so build the same menu the side pane unfolds,
         * and phrase the chosen verb with the object's ALIAS,
         * which is what the parser resolves.  It matters most when
         * the pane is hidden (game.showpanes off, or a Hide
         * request): then these links are the only handle on an
         * object, and offering just "look at" would put every
         * other verb out of reach. */
        PromptBreak pb(in, prompt);
        ListData d;
        if (!in.verb_menu_for(act.element, d))
            return {InEnd::Command, "look at " + act.element};
        std::vector<std::string> verbs;
        for (const std::string &v : d.verbs) {
            std::string t = plain_text(v);
            if (!t.empty())
                verbs.push_back(t);
        }
        std::string alias = d.display_alias.empty()
            ? act.element : d.display_alias;
        std::string cmd;
        if (verbs.size() > 1) {
            /* Autosave stays ARMED under this menu: it is recorded
             * in the blob (g_pending_menu_alias) and no script is
             * mid-flight, so a close here reopens into the menu. */
            g_pending_menu_alias = alias;
            g_pending_menu_verbs = verbs;
            /* Cancelled: no turn passes, re-dispatch and reprompt. */
            if (!run_verb_menu(in, /*resumed=*/false, cmd))
                return {InEnd::Event, ""};
        } else {
            /* One verb needs no menu, and an object with none at
             * all still answers the old "examine it" default. */
            cmd = (verbs.empty() ? std::string("look at")
                                 : verbs[0]) + " " + alias;
        }
        if (g_manual_echo && echo)
            echo_input(cmd);
        return {InEnd::Command, cmd};
    }
    {
        /* Event output must not land on the prompt line. */
        PromptBreak pb(in, prompt);
        run_asl_event(in, act);
    }
    return {InEnd::Event, ""};
}

InResult read_line(Interp &in, bool echo, const char *prompt,
                   bool want_line, bool on_screen)
{
    /* Per-frame, NOT static: a timer tick below can open an expression-form
     * ShowMenu/Ask -> nested read_line, which with a shared buffer would
     * clobber the player's half-typed line (cancelled into buf, re-requested
     * with it preloaded).  Every return path completes or cancels the
     * request, so the request never outlives the frame. */
    glui32 buf[256];
    /* Links are the only other way in, so a library without them always gets
     * the line request (CheapGlk: the smoke harness). */
    if (!g_hyperlinks)
        want_line = true;
    /* A `get input` registered by a timer tick below must send this frame
     * back to the prompt loop: the loop draws the host-owned prompt a
     * pending override calls for, where this frame's prompt (chosen when the
     * command box may have been hidden -- Onion Hugs hides it for its whole
     * timed intro, then BoundaryUnlock re-shows it in the same tick that
     * runs `get input`) is already printed or already nothing.  Compared
     * against the state at entry, not tested absolutely: a frame SERVING a
     * pending override must keep its request. */
    bool entry_override = in.command_override();
#ifdef SPATTERLIGHT
    /* Autorestored with a verb menu on screen: re-enter it before drawing any
     * parser prompt.  Done here rather than in the prompt loop so the chosen
     * command comes back through the ordinary InEnd::Command path and is
     * dispatched by the same code a click or a typed line goes through. */
    if (g_autorestore_menu_resume) {
        g_autorestore_menu_resume = false;
        std::string cmd;
        if (!run_verb_menu(in, /*resumed=*/true, cmd))
            return {InEnd::Event, ""};   /* cancelled: fall back to the prompt */
        if (g_manual_echo && echo)
            echo_input(cmd);
        return {InEnd::Command, cmd};
    }
#endif
    if (prompt && !on_screen)
        glk_put_string((char *) prompt);
    if (g_echo_control)
        glk_set_echo_line_event(gwin, 0);
#ifdef SPATTERLIGHT
    /* Parser prompt: autosave after the prompt is printed (so the GUI
     * snapshot ends with it) and before input is requested (so the saved
     * windows carry no pending request and a restore re-enters here). */
    if (g_autosave_interp)
        aslx_do_autosave(*g_autosave_interp);
    /* A disambiguation menu's prompt: its autosave, once.  Consumed here so
     * no nested read_line (a verb menu popped by a link click under it) and
     * no re-prompt after a bad choice writes another. */
    if (Interp *mi = g_menu_autosave_interp) {
        g_menu_autosave_interp = nullptr;
        aslx_do_menu_autosave(*mi);
    }
#endif
    /* This frame's line request, when it has one.  Nothing may print while
     * it is live, so whatever interrupts it cancels first -- which hands
     * back how much was typed, for a re-request to preload. */
    auto request_line = [&](glui32 preload) {
        if (want_line)
            glk_request_line_event_uni(gwin, buf, 255, preload);
    };
    auto cancel_line = [&]() -> glui32 {
        return want_line ? questglk::cancel_line_request(gwin) : 0;
    };
    request_line(0);
    request_hyperlinks();
    for (;;) {
        event_t ev;
        glk_select(&ev);
        switch (ev.type) {
        case evtype_LineInput:
            if (ev.win == gwin) {
                std::string line = utf8_from_uni(buf, ev.val1);
                if (g_manual_echo && echo)
                    echo_input(line);
                return {InEnd::Line, line};
            }
            break;
        case evtype_Hyperlink:
            if (ev.win == gwin && ev.val1 > g_links_spent &&
                ev.val1 <= g_links.size()) {
                cancel_line();
                return transcript_link_clicked(in, g_links[ev.val1 - 1],
                                               echo, prompt);
            }
            if (ev.win == gobjwin && ev.val1 >= kPaneLinkBase &&
                ev.val1 - kPaneLinkBase < g_pane_links.size()) {
                /* By value: the toggle path rebuilds g_pane_links. */
                const LinkAction act = g_pane_links[ev.val1 - kPaneLinkBase];
                if (!act.toggle_key.empty()) {
                    /* An object name: fold its verb list open or shut and
                     * repaint the pane.  No turn passes, so the line request
                     * in the main window stays exactly as it was. */
                    g_pane_expanded =
                        g_pane_expanded == act.toggle_key ? "" : act.toggle_key;
                    g_pane_dirty = true;
                    redraw_side_pane(in);
#ifdef SPATTERLIGHT
                    /* The fold state rides in the blob, but toggling passes no
                     * turn, so without a refresh here the newest snapshot
                     * predates it and a relaunch comes back folded.  The line
                     * request has to be cancelled across the write -- an
                     * archived PENDING request would collide with the one a
                     * restore makes -- and re-armed with anything already typed
                     * preloaded, exactly as the timer path does. */
                    if (g_autosave_interp) {
                        glui32 typed = cancel_line();
                        aslx_do_autosave(*g_autosave_interp);
                        request_line(typed);
                    }
#endif
                    request_hyperlinks();
                    break;
                }
                /* A verb or a direction: run its command as if typed. */
                cancel_line();
                if (g_manual_echo && echo)
                    echo_input(act.command);
                return {InEnd::Command, act.command};
            }
            request_hyperlinks();
            break;
        case evtype_Timer: {
            /* A fast tick advancing the map glide leaves the pending line
             * request alone; only a whole engine second runs the tick
             * machinery below. */
            if (!timer_event_second(in))
                break;
            /* Cancel first: Glk forbids output with a live line request.
             * With echo off the cancel is invisible. */
            glui32 typed = cancel_line();
            bool reprompt;
#ifdef SPATTERLIGHT
            /* Before, not after: a prompt the tick itself puts up belongs
             * to no turn, and must not autosave as part of the last one. */
            turn_start_changed();
#endif
            {
                PromptBreak pb(in, prompt);
                in.tick(1);
                in.drain_on_ready();
                reprompt = pb.broke;
            }
            update_banner(in);
            redraw_side_pane(in);
            redraw_grid_map();
            update_timer_request(in);  /* a timer script may have moved the
                                        * player and started a glide */
            if (engine_state_pending(in) ||
                in.command_override() != entry_override)
                return {InEnd::State, ""};
            if (reprompt)
                glk_put_string((char *) prompt);
#ifdef SPATTERLIGHT
            /* The tick changed game state while the prompt was up; refresh
             * the autosave (it skips itself when the autosave-on-timer
             * preference is off). */
            if (g_autosave_interp)
                aslx_do_autosave(*g_autosave_interp);
#endif
            request_line(typed);
            request_hyperlinks();
            break;
        }
        case evtype_Arrange:
        case evtype_Redraw:
            handle_arrange(in);
            break;
        }
    }
}

/* Host chrome for a pending wait: QuestViva's #endWaitLink ("Continue..."),
 * which sits under #divOutput while a keypress is owed and is hidden when the
 * wait ends (playercore.js beginWait / waitEnded).  A Glk buffer has no
 * separate chrome row, so the label is printed as a bold endWait hyperlink at
 * the transcript tail and retracted (best-effort) before the wait resolves --
 * leaving the next msg where the Continue was, like the reference link
 * vanishing rather than joining the scrollback. */
void show_continue_link(Interp &in, bool on_screen)
{
    if (!g_continue_shown.empty())
        return;
    g_continue_shown =
        template_text_or(in.world(), "ContinueLabel", "Continue...");
    /* Autorestored under this wait: the label and its link came back with
     * the window. */
    if (on_screen)
        return;
    put_uni_char('\n');
    LinkAction act;
    act.end_wait = true;
    StylePush p;
    p.b = 1;                    /* #endWaitLink { font-weight: bold } */
    if (g_hyperlinks) {
        g_links.push_back(act);
        glk_set_hyperlink((glui32) g_links.size());
    } else {
        p.u = 1;                /* no hyperlink support: underline only */
    }
    push_style(p);
    put_uni_string(g_continue_shown);
    pop_style(p);
    if (g_hyperlinks)
        glk_set_hyperlink(0);
}

/* Retract "\n" + label while it is still the window tail.  False -- and the
 * label left exactly where it is, still counted as shown -- when the window
 * has moved past it or there is no unput at all (CheapGlk). */
bool lift_continue_link()
{
    if (g_continue_shown.empty())
        return false;
    if (!unput_exact(u32_from_utf8("\n" + g_continue_shown)))
        return false;
    g_continue_shown.clear();
    return true;
}

/* The wait is over: take the chrome away.  Where it cannot be retracted the
 * label stays in the scrollback like a "Press any key" line, so end its line
 * -- otherwise the next msg lands on the same line as "Continue...". */
void hide_continue_link()
{
    if (g_continue_shown.empty())
        return;
    if (!lift_continue_link()) {
        put_uni_char('\n');
        g_continue_shown.clear();
    }
}

/* One keypress (a pending `wait`).  Timer events keep ticking.  A click on a
 * hyperlink -- in the main window or the side pane -- counts as the keypress
 * that dismisses the wait, matching the reference player where any input
 * continues past "Press any key to continue".  `resumed` = autorestored
 * under this very wait, with its chrome on screen. */
void read_keypress(Interp &in, bool resumed)
{
    show_continue_link(in, resumed);
#ifdef SPATTERLIGHT
    /* A cutscene paged out a keypress at a time has no other prompt to
     * save at.  Before the request, as everywhere: the saved window has
     * none pending. */
    if (!resumed)
        aslx_do_menu_autosave(in, true);
#endif
    glk_request_char_event(gwin);
    request_hyperlinks();
    for (;;) {
        event_t ev;
        glk_select(&ev);
        if (ev.type == evtype_CharInput && ev.win == gwin) {
            cancel_hyperlinks();
            hide_continue_link();
            return;
        }
        if (ev.type == evtype_Hyperlink &&
            (ev.win == gwin || ev.win == gobjwin)) {
            glk_cancel_char_event(gwin);
            cancel_hyperlinks();
            hide_continue_link();
            return;
        }
        if (ev.type == evtype_Timer) {
            if (!timer_event_second(in))
                continue;
            /* Lift the chrome first so anything the tick prints lands where
             * it was, then put it back underneath (the reference link sits
             * below #divOutput however much that grows).  Where it cannot be
             * lifted it stays put and show_continue_link is a no-op, rather
             * than a fresh label every second. */
            (void) lift_continue_link();
            in.tick(1);
#ifdef SPATTERLIGHT
            turn_start_changed();
#endif
            update_banner(in);
            redraw_side_pane(in);
            redraw_grid_map();
            update_timer_request(in);
            if (in.world().finished || !in.pending_wait()) {
                glk_cancel_char_event(gwin);
                cancel_hyperlinks();
                hide_continue_link();
                return;
            }
            /* Also covers a ClearScreen during the tick wiping the chrome. */
            show_continue_link(in);
            /* Clearing the pane drops its hyperlink request; re-arm it. */
            if (g_hyperlinks && gobjwin)
                glk_request_hyperlink_event(gobjwin);
        }
        if (ev.type == evtype_Arrange || ev.type == evtype_Redraw)
            handle_arrange(in);
    }
}

/* Present a MenuData and return the selected key, or false for cancel.
 * Shared by the script-command prompt (set_menu_response) and the
 * expression-form provider (menu_provider), which is why it does not touch
 * the pending slot itself. */
bool run_menu_ui(Interp &in, const MenuData &m, std::string &key, bool resumed)
{
    /* Through in.print, not render_html: with a PromptBreak active (a menu
     * opened by a timer tick) the first output must retract the stale prompt
     * first, exactly like every other engine print. */
    /* A v600+ game has already drawn the caption and the numbered options in
     * the transcript itself (Interp::show_inline_prompt). */
    if (!resumed && !in.inline_prompts()) {
        if (!m.caption.empty()) {
            in.print(m.caption);
            glk_put_char('\n');
        }
        for (size_t i = 0; i < m.options.size(); i++) {
            char num[24];
            snprintf(num, sizeof num, "%zu: ", i + 1);
            in.print(num + m.options[i].second);
            glk_put_char('\n');
        }
    }
    /* Resumed: the caption, the options AND the prompt are all still on the
     * autorestored screen, so the first request must not redraw any of them --
     * the same trick g_autorestore_reentry plays for the parser prompt, which
     * only has one line to worry about. */
    bool on_screen = resumed;
    InlinePromptTrail trail(in);
    static const char retry[] = "Please choose one of the numbered options.\n";
    for (;;) {
        char pr[64];
        snprintf(pr, sizeof pr, "\nChoose [1-%zu]%s> ", m.options.size(),
                 m.allow_cancel ? " (or nothing to cancel)" : "");
        InResult r = read_line(in, true, pr, /*want_line=*/true, on_screen);
        on_screen = false;      /* only the first pass is already drawn */
        if (r.kind == InEnd::State)
            return false;                     /* timer ended the world */
        if (r.kind == InEnd::Event) {
            trail.interrupted();
            continue;
        }
        trail.wrote(pr);
        if (g_manual_echo)
            trail.wrote(r.text + "\n");
        else
            trail.interrupted();    /* the library echoed: not ours to match */
        std::string l = trim(r.text);
        if (l.empty() && m.allow_cancel) {
            trail.commit();
            return false;
        }
        /* number, exact key, or display text (the replayer's resolution) */
        char *end = nullptr;
        long n = strtol(l.c_str(), &end, 10);
        if (end && *end == 0 && n >= 1 && (size_t) n <= m.options.size()) {
            key = m.options[n - 1].first;
            trail.commit();
            return true;
        }
        for (auto &kv : m.options)
            if (kv.first == l || qstr::iequal(kv.second, l)) {
                key = kv.first;
                trail.commit();
                return true;
            }
        glk_put_string((char *) retry);
        trail.wrote(retry);
    }
}

/* Run the inline object-link verb menu described by g_pending_menu_*, and
 * build the command its choice makes ("Knock" + "door").  `resumed` = the
 * whole menu is already on the autorestored screen, so nothing is redrawn.
 * The pending state is cleared before returning either way: the command is
 * about to run, and the next prompt's snapshot must not still claim a menu is
 * up.  Returns false when the player cancelled (no turn passes). */
bool run_verb_menu(Interp &in, bool resumed, std::string &cmd)
{
    MenuData m;
    m.caption = cap_first(g_pending_menu_alias);
    m.allow_cancel = true;
    for (const std::string &v : g_pending_menu_verbs)
        m.options.push_back(std::make_pair(v, v));

    std::string chosen;
    bool picked = run_menu_ui(in, m, chosen, resumed);

    std::string alias = g_pending_menu_alias;
    g_pending_menu_alias.clear();
    g_pending_menu_verbs.clear();
    if (!picked)
        return false;
    cmd = chosen + " " + alias;
    return true;
}

/* Present a yes/no question and return the answer.  Before v600 the engine never prints
 * the caption (ShowQuestion is host UI), so render it here.  Shared by the
 * `ask` script-command prompt (set_question_response) and the expression-form
 * provider (ask_provider), which is why it does not touch the pending slot.
 * Returns false when the world ended under the prompt (no answer to give).
 * `resumed` as in run_menu_ui. */
bool run_question_ui(Interp &in, const std::string &q, bool &answer,
                     bool resumed)
{
    /* through the prompt-retract path; see run_menu_ui.  A v600+ game has
     * already drawn the question and its 1: Yes / 2: No links itself. */
    bool drawn = in.inline_prompts();
    if (!drawn && !resumed)
        in.print(q);
    bool on_screen = resumed;
    /* Under the inline list the prompt starts its own line, like a menu's. */
    const char *pr = drawn ? "\nChoose [1-2] (or yes/no)> " : " (yes/no) > ";
    InlinePromptTrail trail(in);
    static const char retry[] = "Please answer YES or NO.";
    for (;;) {
        InResult r = read_line(in, true, pr, /*want_line=*/true, on_screen);
        on_screen = false;
        if (r.kind == InEnd::State)
            return false;                     /* timer ended the world */
        if (r.kind != InEnd::Line && !(drawn && r.kind == InEnd::Command)) {
            trail.interrupted();
            continue;
        }
        trail.wrote(pr);
        if (g_manual_echo)
            trail.wrote(r.text + "\n");
        else
            trail.interrupted();
        std::string a = lower(trim(r.text));
        bool yes = a == "yes" || a == "y" || (drawn && a == "1");
        bool no = a == "no" || a == "n" || (drawn && a == "2");
        if (yes || no) {
            trail.commit();
            answer = yes;
            return true;
        }
        glk_put_string((char *) retry);
        trail.wrote(retry);
    }
}

/* The EXPRESSION form of GetInput: the engine blocks inside eval for the
 * next command line. Host-owned like a pending `get input` (the line never
 * reaches the parser), so it draws the same "> " prompt the prompt loop
 * draws for one, and host-echoes the accepted line. */
bool run_input_ui(Interp &in, std::string &text, bool resumed)
{
    for (;;) {
        InResult r = read_line(in, true, "\n> ", /*want_line=*/true, resumed);
        resumed = false;
        if (r.kind == InEnd::State)
            return false;                     /* timer ended the world */
        if (r.kind == InEnd::Line || r.kind == InEnd::Command) {
            text = r.text;
            return true;
        }
    }
}

/* ------------------------------------------------------ character dialog -- */

void draw_character_form(const aslxform::Layout &lay, bool links)
{
    glk_window_clear(gformwin);
    strid_t s = glk_window_get_stream(gformwin);
    for (const aslxform::Span &sp : lay.spans) {
        glk_window_move_cursor(gformwin, (glui32) std::max(sp.x, 0),
                               (glui32) sp.y);
        glk_set_style_stream(s, sp.focused ? style_User1
                                : sp.heading ? style_Subheader : style_Normal);
        if (sp.link && links)
            glk_set_hyperlink_stream(s, sp.link);
        put_stream_utf8(s, sp.text);
        if (sp.link && links)
            glk_set_hyperlink_stream(s, 0);
    }
    glk_set_style_stream(s, style_Normal);
}

/* The dialog as a text grid above the story window: every control is a
 * hyperlink, and the arrow keys walk and change them.  False when this Glk
 * has no grid to open (CheapGlk), with the form untouched. */
bool run_character_grid(Interp &in, aslxform::CharacterForm &f)
{
    using namespace aslxform;
    glui32 rows = 14;
    gformwin = glk_window_open(gwin, winmethod_Above | winmethod_Fixed, rows,
                               wintype_TextGrid, 0);
    if (!gformwin)
        return false;
    const bool links = glk_gestalt(gestalt_Hyperlinks, 0) &&
                       glk_gestalt(gestalt_HyperlinkInput, wintype_TextGrid);
    size_t focus = 0;
    Layout lay;
    auto redraw = [&] {
        glui32 w = 0, h = 0;
        glk_window_get_size(gformwin, &w, &h);
        lay = layout_character_form(f, (int) w, focus);
        if ((glui32) lay.height != rows) {
            rows = (glui32) lay.height;
            glk_window_set_arrangement(glk_window_get_parent(gformwin),
                                       winmethod_Above | winmethod_Fixed, rows,
                                       gformwin);
        }
        draw_character_form(lay, links);
    };
    /* Line input in the grid, at the name, preloaded with the current one. */
    auto edit_name = [&] {
        glui32 buf[kFormNameMax + 1];
        std::u32string cur = u32_from_utf8(f.name);
        glui32 n = (glui32) std::min(cur.size(), kFormNameMax);
        for (glui32 i = 0; i < n; i++)
            buf[i] = cur[i];
        /* Blank the name as drawn: the input field is laid over the grid,
         * and whatever it no longer covers would show through. */
        glk_window_move_cursor(gformwin, (glui32) lay.name_x,
                               (glui32) lay.name_y);
        glk_set_style_stream(glk_window_get_stream(gformwin), style_Normal);
        put_stream_utf8(glk_window_get_stream(gformwin),
                        std::string(cur.size(), ' '));
        glk_window_move_cursor(gformwin, (glui32) lay.name_x,
                               (glui32) lay.name_y);
        glk_request_line_event_uni(gformwin, buf, (glui32) kFormNameMax, n);
        /* No redraw of the form itself on a resize: Glk forbids output to a
         * window with a line request pending. */
        event_t ev = wait_for_event(
            [](const event_t &e) {
                return e.type == evtype_LineInput && e.win == gformwin;
            },
            [&] { handle_arrange(in); });
        f.set_name(utf8_from_uni(buf, ev.val1));
    };

    redraw();
    bool char_req = false, link_req = false;
    for (;;) {
        if (!char_req) {
            glk_request_char_event_uni(gformwin);
            char_req = true;
        }
        if (links && !link_req) {
            glk_request_hyperlink_event(gformwin);
            link_req = true;
        }
        event_t ev;
        glk_select(&ev);
        FormAct act = FormAct::None;
        bool changed = false;
        switch (ev.type) {
        case evtype_CharInput: {
            if (ev.win != gformwin)
                break;
            char_req = false;
            changed = true;
            switch (ev.val1) {
            case keycode_Up:     act = form_key(f, focus, FormKey::Up); break;
            case keycode_Tab:
            case keycode_Down:   act = form_key(f, focus, FormKey::Down); break;
            case keycode_Left:
            case '-':            act = form_key(f, focus, FormKey::Left); break;
            case keycode_Right:
            case '+': case '=':
            case ' ':            act = form_key(f, focus, FormKey::Right); break;
            case keycode_Return: act = form_key(f, focus, FormKey::Activate); break;
            default:             changed = false; break;
            }
            break;
        }
        case evtype_Hyperlink:
            if (ev.win != gformwin)
                break;
            link_req = false;
            changed = true;
            act = form_click(f, focus, ev.val1);
            break;
        case evtype_Arrange:
        case evtype_Redraw:
            handle_arrange(in);
            changed = true;
            break;
        /* Timer ticks are dropped: the game has not started. */
        }
        if (act == FormAct::Done)
            break;
        if (act == FormAct::EditName) {
            if (char_req) {
                glk_cancel_char_event(gformwin);
                char_req = false;
            }
            edit_name();
        }
        if (changed)
            redraw();
    }
    if (char_req)
        glk_cancel_char_event(gformwin);
    if (link_req)
        glk_cancel_hyperlink_event(gformwin);
    glk_window_close(gformwin, nullptr);
    gformwin = nullptr;
    return true;
}

/* The same questions asked one at a time in the story window, for a Glk
 * without grids.  An empty or unusable answer keeps the default, so a script
 * that was not written for the dialog cannot get stuck in it. */
void run_character_prompts(Interp &in, aslxform::CharacterForm &f)
{
    auto ask = [&](const std::string &prompt, std::string &out) {
        for (int tries = 0; tries < 8; tries++) {
            InResult r = read_line(in, true, prompt.c_str());
            if (r.kind == InEnd::State)
                return false;
            if (r.kind == InEnd::Line || r.kind == InEnd::Command) {
                out = trim(r.text);
                return true;
            }
        }
        return false;
    };
    auto choose = [&](aslxform::RadioGroup &g, const char *what) {
        std::string menu = std::string("\n") + what + ":\n";
        for (size_t i = 0; i < g.labels.size(); i++)
            menu += "  " + std::to_string(i + 1) + ": " + g.labels[i] + "\n";
        put_uni_string(menu);
        std::string a;
        if (!ask("Choose [1-" + std::to_string(g.labels.size()) + ", default " +
                 std::to_string(g.checked + 1) + "]: ", a))
            return false;
        char *end = nullptr;
        long n = strtol(a.c_str(), &end, 10);
        if (!a.empty() && end && *end == 0 && n >= 1 &&
            (size_t) n <= g.labels.size())
            g.checked = (size_t) n - 1;
        return true;
    };

    put_uni_string("\n" + (f.title.empty() ? "Your Character" : f.title) + "\n");
    std::string a;
    if (!ask("\nName [" + f.name + "]: ", a))
        return;
    f.set_name(a);
    if (!choose(f.sex, "Sex"))
        return;
    put_uni_string("\nAttributes: " + std::to_string(f.points) +
                   " points to share out.\n");
    for (aslxform::Counter &c : f.counters) {
        const int most = c.value + f.points;
        if (!ask(c.label + " [0-" + std::to_string(most) + ", default " +
                 std::to_string(c.value) + "]: ", a))
            return;
        char *end = nullptr;
        long n = strtol(a.c_str(), &end, 10);
        if (!a.empty() && end && *end == 0 && n >= 0 && n <= most) {
            f.points += c.value - (int) n;
            c.value = (int) n;
        }
    }
    choose(f.bonus, "Bonus item");
}

/* Run the pending character dialog and hand its answer to the game, the way
 * the page's "Done" button does. */
void run_character_dialog(Interp &in)
{
    [[maybe_unused]] AutosaveSuspend no_autosave;
    aslxform::CharacterForm f = g_form;
    if (!run_character_grid(in, f))
        run_character_prompts(in, f);
    /* The dialog leaves nothing behind in the browser; here the choices go
     * into the scrollback, which is otherwise the only record of them. */
    put_uni_string("\n" + f.summary() + "\n");
    LinkAction act;
    act.event_func = f.event;
    act.event_param = f.answer();
    run_asl_event(in, act);
}

/* A restored game never runs the start script that prints the status panel,
 * so read the panel straight from the game.  InitInterface then refills most
 * cells (Deeper's ends in UpdateStatus); the three it leaves alone, which
 * the game only writes as rooms are entered, are filled from the same
 * attributes those scripts read. */
void seed_custom_status(Interp &in)
{
    World &w = in.world();
    Element *io = w.find("interface_obj");
    const Value *stuff = io ? in.resolve_field(io, "stuff") : nullptr;
    if (!stuff || stuff->type != Value::Type::String ||
        !aslxform::parse_status_panel(stuff->str, g_cstatus))
        return;
    g_pane_dirty = true;

    Element *pl = object_field(in, game_element(w), "pov");
    if (!pl)
        return;
    auto seed = [&](const char *id, Element *e, const char *field,
                    const char *suffix) {
        const Value *v = e ? in.resolve_field(e, field) : nullptr;
        if (v && v->type == Value::Type::Int && !g_cstatus_vals.count(id))
            g_cstatus_vals[id] = std::to_string(v->integer) + suffix;
    };
    seed("level-span", object_field(in, pl, "parent"), "level", "");
    seed("max-level-span", pl, "maxlevel", "");
    seed("artefact-span", pl, "artefactcount", "/14");
}

/* ---------------------------------------------------------- save/restore -- */

/* The same as a path the engine and the file helpers can always take:
 * "" while there is none. */
const char *story_path() { return g_storyfile ? g_storyfile : ""; }

/* Write the engine snapshot to a player-chosen file. Runs mid-turn, from the
 * request (RequestSave) hook -- Core's `save` command -- so there is never a
 * pending line request here. */
void do_save_ui(Interp &in)
{
    prompt_write_save(in.save_game(story_path()));
}

/* Prompt for a save file and read it, validating it against a scratch reload
 * of the game (so a wrong-game or corrupt save never tears down the running
 * session). Returns true with `data` filled when the caller should reboot
 * the session from it. */
bool do_restore_ui(std::string &data)
{
    if (!prompt_read_save(data))
        return false;

    /* Accept both our v1 snapshot and a native Quest/QuestViva `.quest-save`
     * (the interoperable format -- a save exported from the desktop Quest
     * player or QuestViva imports here). restore_game auto-detects which. */
    if (!Interp::is_save_data(data.data(), data.size()) &&
        !Interp::is_native_save_data(data.data(), data.size())) {
        glk_put_string((char *) "Sorry, that does not look like a saved game"
                                " for this story.\n");
        return false;
    }
    /* Full validation: apply it to a scratch world before committing. */
    World probe_w;
    std::string err = "could not reload the game";
    if (load_file(story_path(), probe_w, core_dir_path())) {
        Interp probe(probe_w);
        probe.print = [](const std::string &) {};
        if (probe.restore_game(data, err))
            return true;
    }
    glk_put_string((char *) "Sorry, that save cannot be restored (");
    put_uni_string(err);
    glk_put_string((char *) ").\n");
    return false;
}

/* ------------------------------------------------------------- metaverbs -- */

/* #HELP: list the system commands, the way the classic runner's #HELP does for
 * Quest 4.  Only some of them are the frontend's -- RESTORE, the transcript
 * pair, VERBS and #HELP itself; SAVE, RESTART, UNDO and QUIT are Core's own
 * commands.  What they have in common, and what the player wants from this
 * list, is that they work in any game rather than being one game's invention,
 * so the list names them all and the heading claims no more than that.
 *
 * Kept behind the '#' so it never shadows the game's own HELP (Core's
 * DefaultHelp template, which the game may localize or replace); the
 * plain-HELP hint below points players at it. */
bool handle_help_command(const std::string &raw)
{
    if (!match_help_command(raw))
        return false;
    echo_metaverb_command(raw);

    print_system_commands(
        "  QUIT              Stop playing and leave Quest.\n",
        "",                     /* OOPS is a question-runner command */
        "  VERBS <object>    List the actions available for an object, the\n"
        "                    same menu clicking its name pops up.\n",
        "");                    /* so is ABOUT */
    return true;
}

/* After a plain HELP, point the player at #HELP -- the game's own help never
 * mentions the commands this port adds.  English only: HELP itself is matched
 * by the game's (possibly localized) command pattern, so a game in another
 * language reaches its help by a word we would not recognize here, and saying
 * nothing beats attaching an English footnote to Greek help text. */
void hint_system_commands(const std::string &raw, bool parser_game)
{
    /* Only on a parser turn: a gamebook has no HELP command at all (the line
     * would only ever follow "no page named 'help'"), and under a `get input`
     * the word is the game's answer, not a command. */
    if (!parser_game || lower(trim(raw)) != "help")
        return;
    glk_put_string((char *)
        "\n(Type #HELP for the SAVE, RESTORE, UNDO and other system"
        " commands.)\n");
}

/* STATUS: print the status attributes in full.  The banner is one grid line
 * and cuts a status too long for it down from the left, so this is how the
 * player reads the fields that did not fit.  Shared wording and formatting
 * with the classic runner. */
bool handle_status_command(const std::string &raw)
{
    if (!match_status_command(raw))
        return false;
    echo_metaverb_command(raw);
    std::string status = g_status_line;
    for (const std::string &line : custom_status_lines())
        status += (status.empty() ? "" : " | ") + line;
    print_status_report(status, true);
    return true;
}

/* SIDEBAR WIDE / SIDEBAR NORMAL: double the pane's width, or put it back.
 * Shared with the classic runner. */
bool handle_sidebar_command(Interp &in, const std::string &raw)
{
    int which = match_sidebar_command(raw);
    if (!which)
        return false;
    echo_metaverb_command(raw);
    change_side_pane_width(which, g_use_objpane, gobjwin, gdivider);
    /* Written out again at its new width, the pane shows its top, rather
     * than wherever the rewrapped text happened to leave it scrolled. */
    g_pane_dirty = true;
    redraw_side_pane(in);
    return true;
}

/* Transcript recording, same commands (and shared code) as the classic
 * runner. */
bool handle_transcript_command(const std::string &raw)
{
    int on = match_transcript_command(raw);
    if (!on)
        return false;
    echo_metaverb_command(raw);
    toggle_transcript(on, gwin, &gtranscript);
    return true;
}

/* VERBS <object>: list the verb menu a QuestViva object-name hyperlink would
 * pop up for that object -- its displayverbs, or inventoryverbs when carried.
 * The reference player only exposes these by clicking a name; this makes the
 * same list typable.  A pure front-end convenience, handled like the other
 * metaverbs (it never reaches the engine's parser). */
bool handle_verbs_command(Interp &in, const std::string &raw)
{
    std::string t = trim(raw);
    std::string c = lower(t);
    if (c != "verbs" && c.rfind("verbs ", 0) != 0)
        return false;
    echo_metaverb_command(raw);

    strid_t s = glk_window_get_stream(gwin);

    /* The object name, minus a leading article the parser would also drop. */
    std::string want = lower(trim(t.substr(5)));
    if (want.rfind("the ", 0) == 0)      want = trim(want.substr(4));
    else if (want.rfind("an ", 0) == 0)  want = trim(want.substr(3));
    else if (want.rfind("a ", 0) == 0)   want = trim(want.substr(2));

    if (want.empty()) {
        glk_put_string((char *) "Type VERBS followed by an object name, "
                                "e.g. VERBS lamp.\n");
        return true;
    }

    /* Match a currently-clickable object by the alias a typed command uses
     * (or its pane label), inventory winning a clash -- see verb_menu_objects. */
    const ListData *match = nullptr;
    std::vector<ListData> objs = in.verb_menu_objects();
    for (const ListData &d : objs) {
        if (lower(trim(d.display_alias)) == want ||
            lower(trim(plain_text(d.text))) == want) {
            match = &d;
            break;
        }
    }

    if (!match) {
        /* The game's own localized template ("Δεν βλέπω κάτι τέτοιο."), run
         * through Core's ASL text processor when this Core has one -- the
         * template may carry {if:}/{random:} directives (Dream Pieces). */
        std::string msg = template_text_or(in.world(), "UnresolvedObject",
                                           "You can't see any such thing.");
        if (in.world().find_function("ProcessText")) {
            Value v = in.call_function("ProcessText", {vstr(msg)}, nullptr);
            if (v.type == Value::Type::String)
                msg = v.str;
        } else {
            /* Pre-580 Cores have no ProcessText wrapper; their OutputText
             * calls ProcessTextSection(text, data) with a fresh dictionary
             * carrying "fulltext" -- do the same. */
            if (in.world().find_function("ProcessTextSection")) {
                Value data;
                data.type = Value::Type::StringDict;
                data.dict().push_back({"fulltext", vstr(msg)});
                Value v = in.call_function("ProcessTextSection",
                                           {vstr(msg), data}, nullptr);
                if (v.type == Value::Type::String)
                    msg = v.str;
            }
        }
        put_stream_utf8(s, plain_text(msg) + "\n");
        return true;
    }

    /* The reference UI pops a bare verb menu on an object-name click -- no
     * prose around it -- and English framing sits wrongly in a translated
     * game ("Verbs for τόξο: εξέτασε, πάρε").  Print the label and the verb
     * list uncommented; an (unlikely) empty list is just the label. */
    std::string alias = plain_text(match->display_alias);
    put_stream_utf8(s, cap_first(alias));
    for (size_t i = 0; i < match->verbs.size(); i++) {
        put_stream_utf8(s, i ? ", " : ": ");
        put_stream_utf8(s, plain_text(match->verbs[i]));
    }
    put_stream_utf8(s, ".\n");
    return true;
}

}  // namespace aslxglk
