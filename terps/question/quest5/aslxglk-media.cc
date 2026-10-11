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


/* aslxglk-media.cc -- the resources of the Quest 5 Glk frontend: pictures,
 * sounds and panels (see
 * aslxglk-internal.hh). */

#include "aslxglk-internal.hh"

namespace aslxglk {

/* ------------------------------------------------------------- resources -- */

void init_resources(const char *storyfile)
{
    g_package_entries.clear();
    g_is_package = false;
    g_story_dir.clear();
    if (!storyfile)
        return;
    std::string path = storyfile;
    size_t slash = path.find_last_of("/\\");
    g_story_dir = slash == std::string::npos ? "." : path.substr(0, slash);
    std::string buf;
    if (!slurp_file(path, buf) || buf.size() < 4 ||
        memcmp(buf.data(), "PK\x03\x04", 4) != 0)
        return;
    g_is_package = zip_list_entries((const uint8_t *) buf.data(), buf.size(),
                                    g_package_entries);
}

/* Read one byte range of a file (a zip entry's payload). */
bool read_file_range(const std::string &path, size_t offset, size_t len,
                     std::string &out)
{
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return false;
    out.resize(len);
    bool ok = fseek(f, (long) offset, SEEK_SET) == 0 &&
              fread(&out[0], 1, len, f) == len;
    fclose(f);
    return ok;
}

/* Bytes of a game resource by name: a package entry (exact match first,
 * then case-insensitive -- QuestViva's resource table is OrdinalIgnoreCase)
 * or a file next to a raw .aslx game. ".." is rejected like
 * WorldModel.GetExternalUrlAsync does. */
bool resource_bytes(const std::string &name, std::string &out)
{
    if (name.empty() || name.find("..") != std::string::npos)
        return false;
    if (g_is_package) {
        const ZipEntryInfo *e = zip_find_entry(g_package_entries, name);
        if (!e)
            return false;
        std::string comp;
        if (!read_file_range(story_path(), e->offset,
                             e->comp_size, comp))
            return false;
        if (e->method == 0) { out = std::move(comp); return true; }
        if (e->method != 8)
            return false;
        return inflate_raw((const uint8_t *) comp.data(), comp.size(),
                           e->raw_size, out);
    }
    if (g_story_dir.empty())
        return false;
    return slurp_file(g_story_dir + "/" + name, out);
}

/* One string literal starting at js[i] (which must be a quote).  Fills `out`
 * with the unescaped contents and advances i past the closing quote; returns
 * false if the literal is unterminated. Handles \" \\ \n \t and \' -- enough
 * for the ASLEvent("Name","param") calls these bundled scripts use. */
bool js_string_literal(const std::string &js, size_t &i, std::string &out)
{
    char q = js[i++];
    out.clear();
    for (; i < js.size(); i++) {
        char c = js[i];
        if (c == '\\' && i + 1 < js.size()) {
            char n = js[++i];
            switch (n) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            default:  out += n;    break;   /* \" \\ \' and the rest verbatim */
            }
        } else if (c == q) {
            i++;                            /* past the closing quote */
            return true;
        } else {
            out += c;
        }
    }
    return false;
}

/* Scan a JS function body for the ASLEvent(func, param) calls it fires, in
 * source order, appending each to `out`.  Only string-literal arguments are
 * read -- a computed callback name is beyond a static scan and is left alone
 * (fire_js_events ignores any name that is not a real <function> anyway).  The
 * param argument is optional; ASLEvent("X") records an empty param. */
void js_scan_aslevents(const std::string &body, JsEvents &out)
{
    const std::string tok = "ASLEvent";
    size_t p = 0;
    while ((p = body.find(tok, p)) != std::string::npos) {
        size_t q = p + tok.size();
        p = q;                                      /* advance past this hit */
        while (q < body.size() && isspace((unsigned char) body[q])) q++;
        if (q >= body.size() || body[q] != '(') continue;
        q++;
        while (q < body.size() && isspace((unsigned char) body[q])) q++;
        if (q >= body.size() || (body[q] != '"' && body[q] != '\'')) continue;
        std::string func, param;
        if (!js_string_literal(body, q, func)) continue;
        while (q < body.size() && isspace((unsigned char) body[q])) q++;
        if (q < body.size() && body[q] == ',') {
            q++;
            while (q < body.size() && isspace((unsigned char) body[q])) q++;
            if (q < body.size() && (body[q] == '"' || body[q] == '\''))
                js_string_literal(body, q, param);
        }
        if (!func.empty())
            out.push_back(std::make_pair(func, param));
    }
}

/* Scan a JS function body for calls to other bundled functions (an identifier
 * in `known` immediately followed by `(`), in source order, appending each to
 * `out`.  These are the call-graph edges index_bundled_js follows to reach an
 * ASLEvent that lives in a helper rather than the entry function -- Moquette's
 * heatherText() just calls doHeatherText(), which is where the completion is
 * fired. */
void js_scan_calls(const std::string &body, const std::set<std::string> &known,
                   std::vector<std::string> &out)
{
    size_t n = body.size();
    for (size_t i = 0; i < n; ) {
        char c = body[i];
        if (c == '"' || c == '\'') {
            std::string dummy;
            if (!js_string_literal(body, i, dummy)) break;
            continue;
        }
        if (c == '/' && i + 1 < n && body[i + 1] == '/') {
            while (i < n && body[i] != '\n') i++;
            continue;
        }
        if (c == '/' && i + 1 < n && body[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(body[i] == '*' && body[i + 1] == '/')) i++;
            i += 2;
            continue;
        }
        if (isalpha((unsigned char) c) || c == '_' || c == '$') {
            size_t s = i;
            while (i < n && (isalnum((unsigned char) body[i]) ||
                             body[i] == '_' || body[i] == '$')) i++;
            std::string id = body.substr(s, i - s);
            size_t j = i;
            while (j < n && isspace((unsigned char) body[j])) j++;
            if (j < n && body[j] == '(' && known.count(id))
                out.push_back(id);
            continue;
        }
        i++;
    }
}

/* Index the game's bundled JavaScript once per session.  The browser runs a
 * game's `function NAME(...) {...}` callbacks from real timers and animation
 * completions, ending in ASLEvent(completion) to hand control back to the
 * engine; with no JS engine we fire those completions ourselves when the
 * matching zero-arg JS.NAME() call is made.  For each function we record the
 * ASLEvents reachable from its body -- directly, and TRANSITIVELY through
 * calls to other bundled functions, since the entry the engine calls often
 * just delegates to a helper where the ASLEvent actually lives.  A brace scan
 * that skips strings and comments bounds each body; a completion whose name is
 * computed rather than a string literal is simply not indexed. */
void index_bundled_js(World &w)
{
    g_js_callbacks.clear();
    std::string all;
    for (const std::unique_ptr<Element> &up : w.elements) {
        Element *e = up.get();
        if (!e || e->kind != ElemKind::Javascript) continue;
        const Value *src = e->field("src");
        if (!src || src->str.empty()) continue;
        std::string bytes;
        if (resource_bytes(src->str, bytes))
            all += bytes + "\n";
    }
    if (all.empty())
        return;

    /* Pass 1: every named function's body text (later defs win, as in JS). */
    std::map<std::string, std::string> bodies;
    size_t i = 0;
    const std::string kw = "function";
    while ((i = all.find(kw, i)) != std::string::npos) {
        /* A real `function` keyword, not a substring of an identifier. */
        bool lok = (i == 0) || !(isalnum((unsigned char) all[i - 1]) ||
                                 all[i - 1] == '_' || all[i - 1] == '$');
        size_t j = i + kw.size();
        i = j;
        if (!lok || j >= all.size() || !isspace((unsigned char) all[j]))
            continue;
        while (j < all.size() && isspace((unsigned char) all[j])) j++;
        size_t ns = j;
        while (j < all.size() && (isalnum((unsigned char) all[j]) ||
                                  all[j] == '_' || all[j] == '$')) j++;
        std::string name = all.substr(ns, j - ns);
        if (name.empty()) continue;               /* an anonymous function */
        while (j < all.size() && all[j] != '{' && all[j] != ';') j++;
        if (j >= all.size() || all[j] != '{') continue;
        /* Brace-match the body, ignoring braces inside strings and comments. */
        size_t depth = 0, k = j;
        for (; k < all.size(); k++) {
            char c = all[k];
            if (c == '"' || c == '\'') {
                std::string dummy;
                if (!js_string_literal(all, k, dummy)) { k = all.size(); break; }
                k--;                              /* the for-loop re-increments */
            } else if (c == '/' && k + 1 < all.size() && all[k + 1] == '/') {
                while (k < all.size() && all[k] != '\n') k++;
            } else if (c == '/' && k + 1 < all.size() && all[k + 1] == '*') {
                k += 2;
                while (k + 1 < all.size() &&
                       !(all[k] == '*' && all[k + 1] == '/')) k++;
                k++;
            } else if (c == '{') {
                depth++;
            } else if (c == '}') {
                if (--depth == 0) { k++; break; }
            }
        }
        bodies[name] = all.substr(j, k - j);
        i = k;
    }
    if (bodies.empty())
        return;

    std::set<std::string> known;
    for (const auto &fn : bodies)
        known.insert(fn.first);

    /* Pass 2: per function, the transitively-reachable ASLEvents.  DFS with a
     * visited guard -- a JS animation loop calls itself (doHeatherText) until
     * its terminal frame fires the completion, so a function legitimately
     * recurses; the guard just stops the WALK from looping. */
    for (const std::string &entry : known) {
        JsEvents evs;
        std::set<std::string> seen;
        std::vector<std::string> stack{entry};
        while (!stack.empty()) {
            std::string f = stack.back();
            stack.pop_back();
            if (!seen.insert(f).second) continue;
            const std::string &body = bodies[f];
            js_scan_aslevents(body, evs);
            std::vector<std::string> calls;
            js_scan_calls(body, known, calls);
            /* Push in reverse so the first callee is visited first. */
            for (size_t c = calls.size(); c-- > 0; )
                stack.push_back(calls[c]);
        }
        if (!evs.empty())
            g_js_callbacks[entry] = evs;
    }

    if (getenv("ASLXGLK_JS_TRACE")) {
        for (const auto &entry : g_js_callbacks) {
            fprintf(stderr, "[js-index] %s ->", entry.first.c_str());
            for (const auto &ev : entry.second)
                fprintf(stderr, " %s", ev.first.c_str());
            fprintf(stderr, "\n");
        }
    }
}

#ifdef SPATTERLIGHT

/* Stage bytes in a temporary file for the backend to load.  An IMAGE file
 * is only needed until the load round-trips (see load_media_resource),
 * after which it is unlinked.  A SOUND file must outlive the session: the
 * app re-reads it from a bookmark when it resumes an interrupted sound
 * after an autorestore (SoundHandler restartAll), so it is staged in
 * glkimp's per-session temp directory -- swept on a normal glk_exit but
 * deliberately preserved on an autosave-quit, exactly like the temp files
 * backing open file streams -- and never unlinked here. */
bool stage_temp_file(const std::string &bytes, std::string &path,
                     bool keep_for_session)
{
    std::string t;
    if (keep_for_session) {
        gettempdir();
        if (tempdir[0])
            t = tempdir;
    }
    if (t.empty()) {
        const char *tmpdir = getenv("TMPDIR");
        t = (tmpdir && *tmpdir) ? tmpdir : "/tmp";
    }
    if (t.back() != '/')
        t += '/';
    t += "aslximgXXXXXX";
    std::vector<char> buf(t.begin(), t.end());
    buf.push_back('\0');
    int fd = mkstemp(buf.data());
    if (fd < 0)
        return false;
    bool ok = write(fd, bytes.data(), bytes.size()) == (ssize_t) bytes.size();
    close(fd);
    path.assign(buf.data());
    if (!ok) {
        unlink(path.c_str());
        path.clear();
        return false;
    }
    return true;
}

/* Resolve + register one image or sound resource. Returns its resource
 * number, or 0. A stored package entry is read by the app straight from the
 * .quest at its byte offset; a deflated entry (the common case -- Quest's
 * packager deflates everything) or an adjacent file goes through
 * resource_bytes and a temp file. register_path_resource's verification
 * round-trip to the app both confirms the resource really decoded and means
 * the temp file has been consumed. */
glui32 load_media_resource(const std::string &name, bool sound)
{
    glui32 id = 0;
    std::string temp_path;

    const ZipEntryInfo *e =
        g_is_package && name.find("..") == std::string::npos
            ? zip_find_entry(g_package_entries, name) : nullptr;
    if (e && e->method == 0 && g_storyfile) {
        id = (glui32) register_path_resource(g_storyfile, sound,
                                             (glui32) e->offset,
                                             (glui32) e->comp_size, true);
    } else {
        std::string bytes;
        if (!resource_bytes(name, bytes) || bytes.empty())
            return 0;
        if (!stage_temp_file(bytes, temp_path, /*keep_for_session=*/sound))
            return 0;
        id = (glui32) register_path_resource(temp_path, sound, 0,
                                             (glui32) bytes.size(), true);
    }

    /* Images are consumed by the round-trip; staged sounds stay for the
     * app's autorestore restart (glkimp's tempdir sweep owns them). */
    if (!temp_path.empty() && !sound)
        unlink(temp_path.c_str());
    return id;
}

/* Cached name -> resource-number lookup (failures cached as 0, so an
 * unresolvable file is only chased once). */
glui32 cached_media_id(std::map<std::string, glui32> &ids,
                       const std::string &name, bool sound)
{
    auto it = ids.find(lower(name));
    if (it != ids.end())
        return it->second;
    glui32 id = load_media_resource(name, sound);
    ids[lower(name)] = id;
    return id;
}

#endif /* SPATTERLIGHT */

void stop_sound_ui()
{
    stop_single_sound(&g_schannel);
}

/* A hand-written <audio> element's sound effect (see the renderer's audio
 * branch).  Takes the first source that resolves, like a browser taking the
 * first <source> it can decode -- L Too offers .ogg then .mp3, and the app
 * plays both.  Never blocks: an autoplaying element is asynchronous, unlike
 * the synchronous `play sound` above.  True when playback started. */
bool play_audio_inline(const std::vector<std::string> &srcs, bool loop)
{
#ifdef SPATTERLIGHT
    if (!glk_gestalt(gestalt_Sound2, 0))
        return false;
    for (const std::string &src : srcs) {
        glui32 id = cached_media_id(g_sound_ids, src, true);
        if (id && play_single_sound(&g_schannel, id, loop, 0))
            return true;
    }
#else
    /* No way to register sounds by name outside Spatterlight's Glk. */
    (void) srcs; (void) loop;
#endif
    return false;
}

/* Block until the playing sound reports finished -- the synchronous
 * `play sound`, whose turn resumes only when the sound ends (QuestViva's
 * awaited wait slot). A keypress skips: the sound is stopped and the turn
 * resumes at once (also the safety valve if the notification never comes).
 * Timer events are deliberately ignored -- the turn is suspended, engine
 * ticks resume at prompt level.
 * Reached only from play_sound_ui, so only under SPATTERLIGHT. */
void wait_for_sound(Interp &in, glui32 id)
{
    glk_request_char_event(gwin);
    event_t ev = wait_for_event(
        [id](const event_t &e) {
            return (e.type == evtype_SoundNotify && e.val1 == id) ||
                   (e.type == evtype_CharInput && e.win == gwin);
        },
        [&] { handle_arrange(in); });
    if (ev.type == evtype_SoundNotify)
        glk_cancel_char_event(gwin);
    else
        stop_sound_ui();
}

/* The play_sound host hook. Degrades to an instantly-finished sound whenever
 * it cannot really play (sound disabled, unresolvable resource): the turn
 * always continues -- only the hook-less headless engine keeps the oracle's
 * abandoned-turn semantics. A looping synchronous sound never finishes, so
 * it plays without blocking; deterministic mode (gli_sa_delays off) never
 * blocks, like the timer heartbeat.
 * Installed (and so referenced) only under SPATTERLIGHT. */
void play_sound_ui(Interp &in, const std::string &name, bool sync, bool loop)
{
#ifdef SPATTERLIGHT
    if (!glk_gestalt(gestalt_Sound2, 0))
        return;

    glui32 id = cached_media_id(g_sound_ids, name, true);
    if (!id)
        return;

    bool block = sync && !loop && gli_sa_delays;
    if (!play_single_sound(&g_schannel, id, loop, block ? 1 : 0))
        return;
    if (block)
        wait_for_sound(in, id);
#else
    /* No way to register sounds by name outside Spatterlight's Glk. */
    (void) in; (void) name; (void) sync; (void) loop;
#endif
}

/* The do_wait host hook (pre-v540 `request (Wait)` -- Core's WaitForKeyPress).
 * Block mid-script until the player presses a key or clicks a link, exactly
 * like the reference player's "press any key to continue". Deterministic mode
 * (gli_sa_delays off, as in the import tests and the smoke harness) never
 * blocks -- the request just returns, keeping transcripts wall-clock free. No
 * game-time tick happens: a Quest wait is real-world only. */
void do_wait_ui(Interp &in)
{
    bool resumed = false;
#ifdef SPATTERLIGHT
    /* A replay goes through the waits the player already has, and stops at
     * the one the autosave was taken under. */
    if (g_replaying) {
        if (replay_answer("w"))
            return;
        resumed = replay_reached(in, "a wait");
    }
#endif
    if (!gli_sa_delays)
        return;
    show_continue_link(in, resumed);
#ifdef SPATTERLIGHT
    if (!resumed)
        aslx_do_menu_autosave(in, true);
#endif
    glk_request_char_event(gwin);
    request_hyperlinks();
    wait_for_event(
        [](const event_t &e) {
            return (e.type == evtype_CharInput && e.win == gwin) ||
                   (e.type == evtype_Hyperlink &&
                    (e.win == gwin || e.win == gobjwin));
        },
        [&] { handle_arrange(in); });
    glk_cancel_char_event(gwin);
    cancel_hyperlinks();
    hide_continue_link();
#ifdef SPATTERLIGHT
    g_turn_answers.push_back("w");
#endif
}

/* The do_pause host hook (pre-v550 `request (Pause, ms)` -- Core's Pause).
 * Block mid-script for `ms` milliseconds using a one-shot timer; a keypress
 * skips it (the safety valve). Deterministic mode never blocks. The
 * prompt-level timer is overridden here and torn down afterwards --
 * g_timer_ms is reset so the next update_timer_request re-arms it. */
void do_pause_ui(Interp &in, int ms)
{
    if (!gli_sa_delays || ms <= 0)
        return;
    if (grid_map_animating()) {
        /* The one-shot pause owns the timer; finish the glide at once. */
        grid_map_anim_snap();
        redraw_grid_map();
    }
    glk_request_timer_events((glui32) ms);
    glk_request_char_event(gwin);
    event_t ev = wait_for_event(
        [](const event_t &e) {
            return e.type == evtype_Timer ||
                   (e.type == evtype_CharInput && e.win == gwin);
        },
        [&] { handle_arrange(in); });
    if (ev.type == evtype_Timer)
        glk_cancel_char_event(gwin);
    glk_request_timer_events(0);
    g_timer_ms = 0;
}

bool draw_image_inline(const std::string &name)
{
#ifdef SPATTERLIGHT
    if (!glk_gestalt(gestalt_Graphics, 0) ||
        !glk_gestalt(gestalt_DrawImage, wintype_TextBuffer))
        return false;

    glui32 id = cached_media_id(g_image_ids, name, false);
    if (!id)
        return false;

    /* An image is an attachment character in the window's text storage, which
     * no unput string can match -- a chunk that drew one pins its output
     * section (see hide_output_section). */
    g_images_drawn++;

    /* Original size with the aspect ratio preserved, but never wider than
     * the window -- re-resolved on every resize (Glk 0.7.6
     * glk_image_draw_scaled_ext). Baseline-aligned like an HTML <img>;
     * Core's OutputText supplies the surrounding <br/>s. */
    return glk_image_draw_scaled_ext(gwin, id, imagealign_InlineUp, 0,
                                     0, 0x10000,
                                     imagerule_WidthOrig | imagerule_AspectRatio,
                                     0x10000) != 0;
#else
    /* No way to register images by name outside Spatterlight's Glk. */
    (void) name;
    return false;
#endif
}

bool panel_band_available()
{
#ifdef SPATTERLIGHT
    /* The band needs the by-name image registry (g_image_ids), which only
     * Spatterlight's Glk provides -- see cached_media_id. */
    return glk_gestalt(gestalt_Graphics, 0) &&
           glk_gestalt(gestalt_DrawImage, wintype_Graphics);
#else
    return false;
#endif
}

/* The band's own background: the buffer's style_Normal background, so the
 * letterbox around a picture that does not fill the band matches the text
 * below it under any theme (Glk offers no way to read a window's background
 * directly -- probe the style hint, as the Comprehend runner does). */
void panel_set_background()
{
    if (!gpanelwin)
        return;
    glui32 bg;
    if (!glk_style_measure(gwin, style_Normal, stylehint_BackColor, &bg))
        bg = 0x00FFFFFF;
    glk_window_set_background_color(gpanelwin, bg);
}

void panel_close()
{
    if (gpanelwin) {
        glk_window_close(gpanelwin, nullptr);
        gpanelwin = nullptr;
    }
    g_panel_id = 0;
    g_panel_last.clear();
}

/* Size the band to the picture and repaint it.  Re-run on every arrange, so
 * the picture re-fits when the window is resized. */
void panel_relayout()
{
    if (!gpanelwin || !g_panel_id)
        return;
    glui32 iw = 0, ih = 0;
    if (!glk_image_get_info(g_panel_id, &iw, &ih) || !iw || !ih)
        return;

    /* Glk hands out no window-pixel metric, so probe: parked at 50% the
     * band's own height IS the reference's max-height, and its width is the
     * full content width. */
    winid_t parent = glk_window_get_parent(gpanelwin);
    if (parent)
        glk_window_set_arrangement(parent,
                                   winmethod_Above | winmethod_Proportional,
                                   50, gpanelwin);
    glui32 bw = 0, maxh = 0;
    glk_window_get_size(gpanelwin, &bw, &maxh);
    if (!bw || !maxh)
        return;

    /* CSS max-width/max-height only ever shrink -- never blow a small
     * picture up to fill the band. */
    double scale = 1.0;
    if (iw > bw)
        scale = (double) bw / (double) iw;
    if ((double) ih * scale > (double) maxh)
        scale = (double) maxh / (double) ih;
    glui32 dw = (glui32) ((double) iw * scale);
    glui32 dh = (glui32) ((double) ih * scale);
    if (!dw || !dh)
        return;

    if (parent)
        glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                                   dh, gpanelwin);
    glk_window_get_size(gpanelwin, &bw, &maxh);
    panel_set_background();     /* a theme change arrives as an arrange */
    glk_window_clear(gpanelwin);
    glk_image_draw_scaled(gpanelwin, g_panel_id,
                          (glsi32) (bw > dw ? (bw - dw) / 2 : 0), 0, dw, dh);
}

void panel_arrange()
{
    if (gpanelwin)
        panel_relayout();
}

void panel_contents(const std::string &html)
{
    std::string src = decode_entities(tag_attr(html, "src"));
    if (src.empty()) {          /* ClearFramePicture (or non-img contents) */
        if (panel_band_available())
            panel_close();
        else
            g_panel_last.clear();
        return;
    }
    if (lower(src) == g_panel_last)
        return;

    if (!panel_band_available()) {
        /* No band: the old inline draw, so a picture is at least seen. */
        if (draw_image_inline(src)) {
            g_panel_last = lower(src);
            glk_put_char('\n');
        }
        return;
    }

#ifdef SPATTERLIGHT
    glui32 id = cached_media_id(g_image_ids, src, false);
#else
    glui32 id = 0;
#endif
    if (!id)                    /* unresolvable: leave the frame as it was */
        return;
    g_panel_id = id;
    g_panel_last = lower(src);
    if (!gpanelwin) {
        gpanelwin = glk_window_open(glk_window_get_root(),
                                    winmethod_Above | winmethod_Proportional,
                                    50, wintype_Graphics, 0);
        if (!gpanelwin) {       /* refused the split: fall back to inline */
            g_panel_id = 0;
            if (draw_image_inline(src))
                glk_put_char('\n');
            return;
        }
        panel_set_background();
    }
    panel_relayout();
}

}  // namespace aslxglk
