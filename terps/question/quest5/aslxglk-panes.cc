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


/* aslxglk-panes.cc -- the banner and the side pane of the Quest 5 Glk frontend (see
 * aslxglk-internal.hh). */

#include "aslxglk-internal.hh"

namespace aslxglk {

/* ---------------------------------------------------------------- banner -- */

/* Flatten a scrap of game HTML to plain text for the banner / pane labels:
 * tags stripped, entities decoded.  Newlines (from <br/>) become the given
 * separator. */
std::string plain_text(const std::string &html, const char *brsep)
{
    std::string out;
    bool intag = false;
    for (size_t i = 0; i < html.size(); i++) {
        char c = html[i];
        if (intag) {
            if (c == '>') intag = false;
            continue;
        }
        if (c == '<') {
            /* <br> variants become the separator */
            std::string t = lower(html.substr(i, 5));
            if (t.compare(0, 3, "<br") == 0 && !out.empty())
                out += brsep;
            intag = true;
            continue;
        }
        out += c;
    }
    return decode_entities(out);
}

/* The <game> element, which is a root like any other. */
Element *game_element(World &w)
{
    for (Element *r : w.roots)
        if (r->kind == ElemKind::Game)
            return r;
    return nullptr;
}

/* The element that object attribute `attr` of `e` points at -- game.pov, the
 * player's parent -- or null when there is no such element or attribute. */
Element *object_field(Interp &in, Element *e, const char *attr)
{
    const Value *v = e ? in.resolve_field(e, attr) : nullptr;
    return v && v->type == Value::Type::ObjectRef ? in.world().find(v->str)
                                                  : nullptr;
}

/* Current room: what Core last sent through JS.updateLocation
 * (CapFirst(GetDisplayName(game.pov.parent)) -- the reference player's
 * location bar).  Old v500-era games embed pre-JS libraries that never call
 * it, so fall back to resolving game.pov.parent -- or the conventional
 * `player` object, Core's own StartGame fallback, since those games predate
 * game.pov too.  Purely cosmetic; any resolution failure blanks the banner. */
void update_banner(Interp &in)
{
    World &w = in.world();
    std::string room = g_location_line;
    if (room.empty()) {
        Element *pl = object_field(in, game_element(w), "pov");
        if (!pl)
            pl = w.find("player");
        Element *rm = object_field(in, pl, "parent");
        if (rm) {
            const Value *alias = in.resolve_field(rm, "alias");
            room = alias && alias->type == Value::Type::String ? alias->str
                                                               : rm->name;
        }
        room = cap_first(room);
    }
    g_room_name = plain_text(room);

    /* Room name left, status attributes (JS.updateStatus: "Score: 3 |
     * Health: 90%") right-aligned when they fit, in codepoint-aware UTF-8
     * mode. */
    std::string status = g_status_line;
    if (status.empty() && !g_cstatus.rows.empty())
        status = custom_status_lines().front();     /* the headline row */
    /* A status the banner has no room for goes to the pane, which opens for
     * it if need be -- unless the game has hidden its panes.  Asked on every
     * redraw: a resize and a new status both end up here. */
    bool in_pane = g_use_objpane && g_panes_visible &&
        status_wants_pane(gbanner, g_room_name, status, true,
                          g_status_in_pane);
    bool moved = in_pane != g_status_in_pane;
    g_status_in_pane = in_pane;
    draw_status_banner(gbanner, g_room_name,
                       in_pane ? std::string() : status, true);
    if (moved) {
        /* The pane lists it from now on, or no longer does.  Not every
         * caller repaints the pane after the banner (a resize does not). */
        g_pane_dirty = true;
        redraw_side_pane(in);
        if (g_links_requested && g_hyperlinks && gobjwin)
            glk_request_hyperlink_event(gobjwin);
    }
}

/* ------------------------------------------------------------- side pane -- */

/* Open the right-hand pane (and its divider) if the host supports splits.
 * The same arrangement as the classic runner's objwin (shared code). */
void ensure_pane_open()
{
    if (gobjwin || !g_use_objpane)
        return;
    open_side_pane_windows(gwin, &gobjwin, &gdivider);
    fill_pane_divider();
}

/* Close the pane so the main window reclaims the width. */
void close_side_pane()
{
    close_side_pane_windows(&gobjwin, &gdivider);
}

/* Paint the divider in the main text colour.  Graphics windows are blanked
 * on resize, so Arrange/Redraw events call this again. */
void fill_pane_divider()
{
    fill_side_divider(gwin, gdivider);
}

/* A registered [template] text, later registration winning (like the loader's
 * replace_templates). */
std::string template_text_or(World &w, const char *name, const char *fallback)
{
    for (auto it = w.templates.rbegin(); it != w.templates.rend(); ++it)
        if (it->first == name)
            return it->second;
    return fallback;
}

/* game.compassdirections (localized at load time via the [CompassN]...
 * templates): the reference UI filters these aliases out of "Places and
 * Objects" so they only appear in the compass. */
std::vector<std::string> compass_directions(Interp &in)
{
    std::vector<std::string> dirs;
    Element *game = game_element(in.world());
    const Value *v = game ? in.resolve_field(game, "compassdirections") : nullptr;
    if (v && v->list_store)
        for (const Value &e : *v->list_store)
            if (e.type == Value::Type::String)
                dirs.push_back(lower(e.str));
    return dirs;
}

/* Repaint the side pane from the stored lists when they changed.  The pane is
 * shown only while it has something to list (Quest's panes are permanent, but
 * an empty sidebar is dead width in a Glk layout -- the classic runner's
 * objwin behaves the same way).  Each item is a hyperlink: objects run their
 * first display verb ("Look at hat"), exits go their direction. */
void redraw_side_pane(Interp &in)
{
    if (!g_use_objpane || !g_pane_dirty)
        return;
    g_pane_dirty = false;

    /* The game hid the panes (JS.panesVisible(false)); the lists keep
     * arriving -- QuestViva computes them regardless -- but the pane stays
     * closed until the game shows them again. */
    if (!g_panes_visible) {
        close_side_pane();
        return;
    }

    std::vector<std::string> dirs = compass_directions(in);
    auto is_dir = [&](const ListData &d) {
        std::string a = lower(d.display_alias);
        for (const std::string &x : dirs)
            if (x == a)
                return true;
        return false;
    };
    std::vector<const ListData *> inv, places, exits;
    for (const ListData &d : g_pane_inv)
        inv.push_back(&d);
    for (const ListData &d : g_pane_places)
        if (!is_dir(d))
            places.push_back(&d);
    for (const ListData &d : g_pane_exits)
        if (is_dir(d))
            exits.push_back(&d);

    /* The status attributes, when the banner is too narrow for them. */
    bool status_rows = g_status_in_pane && !g_status_line.empty();
    if (inv.empty() && places.empty() && exits.empty() &&
        g_cstatus.rows.empty() && !status_rows) {
        close_side_pane();
        return;
    }
    ensure_pane_open();
    if (!gobjwin) {
        /* The host cannot split after all; stop asking the engine for lists. */
        g_use_objpane = false;
        return;
    }

    glk_window_clear(gobjwin);
    g_pane_links.clear();
    strid_t s = glk_window_get_stream(gobjwin);
    bool first = true;

    /* The link value for a pane entry that sends `command` or, for an object
     * name, folds `toggle_key`'s verb list: its slot in g_pane_links.  0 (the
     * entry is drawn as plain text) when the host has no hyperlinks. */
    auto pane_link = [&](const std::string &command,
                         const std::string &toggle_key = std::string()) {
        if (!g_hyperlinks)
            return (glui32) 0;
        LinkAction act;
        act.command = command;
        act.toggle_key = toggle_key;
        g_pane_links.push_back(act);
        return kPaneLinkBase + (glui32) g_pane_links.size() - 1;
    };

    auto section = [&](const std::string &header,
                       const std::vector<const ListData *> &items,
                       bool exit_links) {
        if (items.empty())
            return;
        if (!first)
            glk_put_char_stream(s, '\n');
        first = false;
        put_pane_header(s, header, true);
        for (const ListData *d : items) {
            std::string label = cap_first(plain_text(d->text));
            if (label.empty())
                label = cap_first(d->display_alias);
            /* A direction runs itself; an object unfolds its verb list in
             * place, the way QuestViva pops a verb menu on a click. */
            glui32 linkval = exit_links ? pane_link(d->display_alias)
                                        : pane_link("", d->element_name);
            put_pane_link(s, label, linkval, true);
            if (exit_links || d->element_name != g_pane_expanded)
                continue;
            /* The unfolded verb menu: one indented link per verb, each
             * running the command the reference menu item would ("Look at
             * hat") -- the same phrasing VERBS lists. */
            for (const std::string &verb : d->verbs) {
                std::string vtext = plain_text(verb);
                if (vtext.empty())
                    continue;
                glui32 vlink = pane_link(vtext + " " + d->display_alias);
                glk_put_string_stream(s, (char *) "    ");
                put_pane_link(s, cap_first(vtext), vlink, true);
            }
        }
    };

    World &w = in.world();
    /* The status attributes moved out of the banner, one per line; then the
     * game's own status panel, and under it the commands its icon buttons
     * send. */
    if (status_rows || !g_cstatus.rows.empty()) {
        first = false;
        put_pane_header(s, template_text_or(w, "StatusLabel", PANE_STATUS),
                        true);
        if (status_rows) {
            const std::string sep = " | ";      /* as update_status joins */
            for (size_t a = 0;;) {
                size_t b = g_status_line.find(sep, a);
                put_pane_link(s, g_status_line.substr(a, b == std::string::npos
                                                             ? b : b - a),
                              0, true);
                if (b == std::string::npos)
                    break;
                a = b + sep.size();
            }
        }
        for (const std::string &line : custom_status_lines())
            put_pane_link(s, line, 0, true);
        for (size_t i = 0; i < g_cstatus.buttons.size() && g_hyperlinks; i++) {
            glui32 link = pane_link(g_cstatus.buttons[i]);
            if (i)
                glk_put_string_stream(s, (char *) "  ");
            glk_set_hyperlink_stream(s, link);
            put_stream_utf8(s, cap_first(g_cstatus.buttons[i]));
            glk_set_hyperlink_stream(s, 0);
            if (i + 1 == g_cstatus.buttons.size())
                glk_put_char_stream(s, '\n');
        }
    }

    section(template_text_or(w, "InventoryLabel", PANE_INVENTORY), inv, false);
    section(template_text_or(w, "PlacesObjectsLabel", PANE_PLACES_OBJECTS),
            places, false);
    section(template_text_or(w, "CompassLabel", PANE_COMPASS), exits, true);
    fill_pane_divider();
}

/* An Arrange or Redraw event: everything drawn outside the main buffer has to
 * be laid out and painted again, since Glk blanks a resized graphics window and
 * a grid window's contents do not survive either.  Every event loop below (the
 * prompt, the wait, the pause, the sound block) handles the two events the same
 * way, so they all come here. */
void handle_arrange(Interp &in)
{
    update_banner(in);
    fill_pane_divider();
    grid_map_arrange();
    panel_arrange();
}

}  // namespace aslxglk
