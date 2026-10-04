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

/* aslxglk-form.hh -- the two pieces of custom HTML UI the Glk frontend
 * (aslxglk.cc) stands in for instead of printing as text: Deeper's character
 * creation dialog and its status panel.  Everything here is plain parsing and
 * layout with no Glk calls, so the harness can test it directly. */

#ifndef QUESTION_ASLXGLK_FORM_HH
#define QUESTION_ASLXGLK_FORM_HH

#include <string>
#include <vector>

namespace aslxform {

/* <input type="radio" name="..."> options sharing one name. */
struct RadioGroup {
    std::vector<std::string> values;    /* the value= each option submits */
    std::vector<std::string> labels;    /* the text printed after it */
    size_t checked = 0;
};

/* One point-buy attribute: the <div id="..."> the page's incAtt()/decAtt()
 * count up and down. */
struct Counter {
    std::string id;
    std::string label;
    int value = 0;
};

/* Deeper's jQuery-UI character dialog (game.gamestart): a name field, the sex
 * radios, attribute counters drawing on a shared pool of points and the
 * starting-bonus radios.  "Done" packs the lot into one string and hands it
 * to the game function named by `event`. */
struct CharacterForm {
    std::string title;
    std::string event;
    std::string name;
    RadioGroup sex, bonus;
    std::vector<Counter> counters;
    int points = 0;

    /* The page's incAtt()/decAtt(): one point moves only while the pool (or
     * the attribute) has one to give.  False when nothing changed. */
    bool inc(size_t i);
    bool dec(size_t i);
    /* Take a typed name.  The answer is "|"-separated and the name ends up in
     * game text printed as HTML, so those characters are dropped; a name with
     * nothing left keeps the old one. */
    void set_name(const std::string &typed);
    /* What the page's setValues() sends: "name|sex|n|n|n|n|bonus". */
    std::string answer() const;
    /* The choices as one transcript line. */
    std::string summary() const;
};

/* Recognise the dialog in a JS.addText chunk and read its defaults.  False
 * for any other chunk. */
bool parse_character_form(const std::string &html, CharacterForm &f);

/* The controls, in keyboard focus order: the name, the sex radios, each
 * counter, the bonus radios and Done. */
enum class Ctl { None, Name, Sex, Inc, Dec, Bonus, Done };

/* A grid hyperlink value for a control (never 0) and back. */
unsigned form_link(Ctl c, size_t index = 0);
Ctl form_link_ctl(unsigned link, size_t &index);

size_t form_focus_count(const CharacterForm &f);

enum class FormKey { Up, Down, Left, Right, Activate };
enum class FormAct { None, EditName, Done };

/* Apply a key to the focused control, or a click to the control a link
 * names (which also takes the focus). */
FormAct form_key(CharacterForm &f, size_t &focus, FormKey k);
FormAct form_click(CharacterForm &f, size_t &focus, unsigned link);

/* One run of text on the grid.  x and y are cells; text is UTF-8. */
struct Span {
    int x = 0, y = 0;
    std::string text;
    unsigned link = 0;
    bool heading = false;
    bool focused = false;
};

struct Layout {
    int height = 0;
    std::vector<Span> spans;
    int name_x = 0, name_y = 0;     /* where the name is edited */
};

/* Lay the form out for a grid `width` cells wide: attributes and bonus items
 * side by side when they fit, stacked otherwise. */
Layout layout_character_form(const CharacterForm &f, int width, size_t focus);

/* The longest name the dialog accepts, in characters: what fits between
 * "Name:" and the right column of the wide layout. */
const size_t kFormNameMax = 22;

/* One line of Deeper's status panel (interface_obj.stuff): a label and the
 * <span id="..."> the game rewrites through JS.eval. */
struct StatusRow {
    std::string label;
    std::string id;
    std::string value;      /* what the markup starts with */
};

struct StatusPanel {
    std::vector<StatusRow> rows;
    /* The icon buttons above it: the command each sends. */
    std::vector<std::string> buttons;
};

/* Recognise the panel in a JS.addText chunk.  False for any other chunk. */
bool parse_status_panel(const std::string &html, StatusPanel &p);

/* JS.eval("$('#ID').html('VALUE')"): the id and the new contents. */
bool parse_jquery_html(const std::string &js, std::string &id,
                       std::string &value);

}  // namespace aslxform

#endif  /* QUESTION_ASLXGLK_FORM_HH */
