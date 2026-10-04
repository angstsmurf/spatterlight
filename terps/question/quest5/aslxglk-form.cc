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

/* aslxglk-form.cc -- Deeper's character dialog and status panel, recovered
 * from the HTML the game sends (see aslxglk-form.hh). */

#include "aslxglk-form.hh"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace aslxform {

namespace {

std::string trim_ws(const std::string &s)
{
    size_t b = 0, e = s.size();
    while (b < e && isspace((unsigned char) s[b]))
        b++;
    while (e > b && isspace((unsigned char) s[e - 1]))
        e--;
    return s.substr(b, e - b);
}

/* Trimmed, with every run of whitespace reduced to one space. */
std::string squeeze(const std::string &s)
{
    std::string out;
    for (char c : trim_ws(s)) {
        if (isspace((unsigned char) c)) {
            if (!out.empty() && out.back() != ' ')
                out += ' ';
        } else {
            out += c;
        }
    }
    return out;
}

/* The value of attr="..." (either quote) inside one tag's text, or "". */
std::string attr(const std::string &tag, const std::string &name)
{
    for (size_t at = 0; (at = tag.find(name, at)) != std::string::npos;
         at += name.size()) {
        if (at > 0 && !isspace((unsigned char) tag[at - 1]))
            continue;
        size_t p = at + name.size();
        if (p >= tag.size() || tag[p] != '=')
            continue;
        p++;
        if (p >= tag.size() || (tag[p] != '"' && tag[p] != '\''))
            continue;
        size_t end = tag.find(tag[p], p + 1);
        if (end == std::string::npos)
            return "";
        return tag.substr(p + 1, end - p - 1);
    }
    return "";
}

/* The text between the '>' that closes the tag starting at `tag_at` and the
 * next '<'. */
std::string text_after_tag(const std::string &html, size_t tag_at)
{
    size_t gt = html.find('>', tag_at);
    if (gt == std::string::npos)
        return "";
    size_t lt = html.find('<', gt);
    return squeeze(html.substr(gt + 1, lt == std::string::npos
                                           ? std::string::npos : lt - gt - 1));
}

/* The whole "<name ...>" tag starting at `at`, without the brackets. */
std::string tag_at(const std::string &html, size_t at)
{
    size_t gt = html.find('>', at);
    if (gt == std::string::npos)
        return "";
    return html.substr(at + 1, gt - at - 1);
}

/* Where the tag carrying id="ID" starts, or npos. */
size_t find_id(const std::string &html, const std::string &id)
{
    for (const char *q : {"\"", "'"}) {
        size_t at = html.find("id=" + std::string(q) + id + q);
        if (at != std::string::npos)
            return html.rfind('<', at);
    }
    return std::string::npos;
}

std::string cap_first(std::string s)
{
    if (!s.empty())
        s[0] = (char) toupper((unsigned char) s[0]);
    return s;
}

size_t cp_len(const std::string &s)
{
    size_t n = 0;
    for (unsigned char c : s)
        if ((c & 0xc0) != 0x80)
            n++;
    return n;
}

/* The first `n` characters of a UTF-8 string. */
std::string cp_prefix(const std::string &s, size_t n)
{
    size_t i = 0;
    for (; i < s.size(); i++)
        if (((unsigned char) s[i] & 0xc0) != 0x80 && n-- == 0)
            break;
    return s.substr(0, i);
}

const char kArrowUp[] = "\xe2\x96\xb2";     /* U+25B2, as the page shows */
const char kArrowDown[] = "\xe2\x96\xbc";   /* U+25BC */
const char kRadioOn[] = "(\xe2\x80\xa2) ";  /* U+2022 */
const char kRadioOff[] = "( ) ";

}  // namespace

bool CharacterForm::inc(size_t i)
{
    if (i >= counters.size() || points <= 0)
        return false;
    counters[i].value++;
    points--;
    return true;
}

bool CharacterForm::dec(size_t i)
{
    if (i >= counters.size() || counters[i].value <= 0)
        return false;
    counters[i].value--;
    points++;
    return true;
}

void CharacterForm::set_name(const std::string &typed)
{
    std::string clean;
    for (char c : typed)
        if (c != '|' && c != '<' && c != '>' && c != '&' &&
            (unsigned char) c >= ' ')
            clean += c;
    clean = cp_prefix(trim_ws(clean), kFormNameMax);
    if (!clean.empty())
        name = clean;
}

std::string CharacterForm::answer() const
{
    std::string a = name + "|" +
        (sex.checked < sex.values.size() ? sex.values[sex.checked] : "");
    for (const Counter &c : counters)
        a += "|" + std::to_string(c.value);
    a += "|" +
        (bonus.checked < bonus.values.size() ? bonus.values[bonus.checked] : "");
    return a;
}

std::string CharacterForm::summary() const
{
    std::string s = name;
    if (sex.checked < sex.labels.size())
        s += " (" + sex.labels[sex.checked] + ")";
    for (const Counter &c : counters)
        s += ", " + c.label + " " + std::to_string(c.value);
    if (bonus.checked < bonus.labels.size())
        s += ", " + bonus.labels[bonus.checked];
    return s;
}

bool parse_character_form(const std::string &html, CharacterForm &f)
{
    size_t dlg = find_id(html, "dialog_window_1");
    size_t ev = html.find("ASLEvent(\"HandleDialogue\"");
    size_t name = find_id(html, "name_input");
    if (dlg == std::string::npos || ev == std::string::npos ||
        name == std::string::npos)
        return false;

    CharacterForm out;
    out.event = "HandleDialogue";
    out.title = attr(tag_at(html, dlg), "title");
    out.name = attr(tag_at(html, name), "value");

    /* Radios in page order.  Several marked checked (the sex pair both are)
     * resolve the way a browser does: the last one wins. */
    for (size_t at = 0; (at = html.find("<input", at)) != std::string::npos;
         at++) {
        std::string tag = tag_at(html, at);
        if (attr(tag, "type") != "radio")
            continue;
        std::string group = attr(tag, "name");
        RadioGroup *g = group == "sex_input" ? &out.sex
                        : group == "bonus" ? &out.bonus : nullptr;
        if (!g)
            continue;
        if (!attr(tag, "checked").empty())
            g->checked = g->values.size();
        g->values.push_back(attr(tag, "value"));
        g->labels.push_back(text_after_tag(html, at));
    }

    /* One counter per incAtt('id') button; its <div id> holds the number. */
    const std::string inc = "incAtt('";
    for (size_t at = 0; (at = html.find(inc, at)) != std::string::npos;) {
        at += inc.size();
        size_t end = html.find('\'', at);
        if (end == std::string::npos)
            break;
        Counter c;
        c.id = html.substr(at, end - at);
        size_t div = find_id(html, c.id);
        if (div == std::string::npos)
            continue;
        c.label = cap_first(c.id);
        c.value = atoi(text_after_tag(html, div).c_str());
        out.counters.push_back(c);
    }
    size_t pts = find_id(html, "points");
    if (pts != std::string::npos)
        out.points = atoi(text_after_tag(html, pts).c_str());

    if (out.counters.empty() || out.sex.values.empty() ||
        out.bonus.values.empty())
        return false;
    f = out;
    return true;
}

unsigned form_link(Ctl c, size_t index)
{
    return ((unsigned) c << 8) | (unsigned) (index & 0xff);
}

Ctl form_link_ctl(unsigned link, size_t &index)
{
    index = link & 0xff;
    unsigned c = link >> 8;
    return c >= (unsigned) Ctl::Name && c <= (unsigned) Ctl::Done
               ? (Ctl) c : Ctl::None;
}

size_t form_focus_count(const CharacterForm &f)
{
    return f.counters.size() + 4;
}

namespace {

/* Focus slots: 0 name, 1 sex, 2.. counters, then bonus, then Done. */
size_t focus_bonus(const CharacterForm &f) { return f.counters.size() + 2; }
size_t focus_done(const CharacterForm &f) { return f.counters.size() + 3; }

void cycle(RadioGroup &g, bool forward)
{
    size_t n = g.values.size();
    if (n)
        g.checked = (g.checked + (forward ? 1 : n - 1)) % n;
}

}  // namespace

FormAct form_key(CharacterForm &f, size_t &focus, FormKey k)
{
    const size_t n = form_focus_count(f);
    if (focus >= n)
        focus = 0;
    switch (k) {
    case FormKey::Up:
        focus = (focus + n - 1) % n;
        return FormAct::None;
    case FormKey::Down:
        focus = (focus + 1) % n;
        return FormAct::None;
    case FormKey::Left:
    case FormKey::Right: {
        const bool fwd = k == FormKey::Right;
        if (focus == 1)
            cycle(f.sex, fwd);
        else if (focus == focus_bonus(f))
            cycle(f.bonus, fwd);
        else if (focus >= 2 && focus < focus_bonus(f))
            fwd ? f.inc(focus - 2) : f.dec(focus - 2);
        return FormAct::None;
    }
    case FormKey::Activate:
        if (focus == 0)
            return FormAct::EditName;
        if (focus == focus_done(f))
            return FormAct::Done;
        /* Anywhere else Return just moves on, so the form can be walked
         * from top to Done with one key. */
        focus++;
        return FormAct::None;
    }
    return FormAct::None;
}

FormAct form_click(CharacterForm &f, size_t &focus, unsigned link)
{
    size_t i = 0;
    switch (form_link_ctl(link, i)) {
    case Ctl::Name:
        focus = 0;
        return FormAct::EditName;
    case Ctl::Sex:
        focus = 1;
        if (i < f.sex.values.size())
            f.sex.checked = i;
        break;
    case Ctl::Inc:
    case Ctl::Dec:
        if (i < f.counters.size()) {
            focus = 2 + i;
            form_link_ctl(link, i) == Ctl::Inc ? f.inc(i) : f.dec(i);
        }
        break;
    case Ctl::Bonus:
        focus = focus_bonus(f);
        if (i < f.bonus.values.size())
            f.bonus.checked = i;
        break;
    case Ctl::Done:
        focus = focus_done(f);
        return FormAct::Done;
    case Ctl::None:
        break;
    }
    return FormAct::None;
}

Layout layout_character_form(const CharacterForm &f, int width, size_t focus)
{
    Layout lay;
    auto put = [&](int x, int y, const std::string &text, unsigned link = 0,
                   bool heading = false, bool focused = false) {
        Span s;
        s.x = x;
        s.y = y;
        s.text = text;
        s.link = link;
        s.heading = heading;
        s.focused = focused;
        lay.spans.push_back(s);
    };
    /* A label ending at column `right`. */
    auto put_right = [&](int right, int y, const std::string &text,
                         bool heading, bool focused) {
        put(right - (int) cp_len(text), y, text, 0, heading, focused);
    };
    auto radios = [&](const RadioGroup &g, Ctl ctl, int x, int y, bool down) {
        for (size_t i = 0; i < g.values.size(); i++) {
            std::string t = (i == g.checked ? kRadioOn : kRadioOff) + g.labels[i];
            put(x, y, t, form_link(ctl, i));
            if (down)
                y++;
            else
                x += (int) cp_len(t) + 2;
        }
    };

    /* Left column: labels end at kLabelEnd, then the two arrows and the
     * number.  The right column starts at kRight when there is room. */
    const int kLabelEnd = 14, kUp = 16, kDown = 18, kNum = 21, kRight = 30;
    size_t widest = cp_len("Bonus item");
    for (const std::string &l : f.bonus.labels)
        widest = std::max(widest, cp_len(l) + 4);
    size_t sexw = 5;
    for (const std::string &l : f.sex.labels)
        sexw += cp_len(l) + 6;
    const bool wide =
        width >= kRight + (int) std::max(widest, sexw) + 1;

    int y = 0;
    put(1, y, f.title.empty() ? "Your Character" : f.title, 0, true);
    y += 2;

    put(1, y, "Name:", 0, false, focus == 0);
    lay.name_x = 7;
    lay.name_y = y;
    put(lay.name_x, y, f.name, form_link(Ctl::Name));
    const int sex_x = wide ? kRight : 1;
    if (!wide)
        y++;
    put(sex_x, y, "Sex:", 0, false, focus == 1);
    radios(f.sex, Ctl::Sex, sex_x + 5, y, false);
    y += 2;

    put_right(kLabelEnd, y, "Attributes", true, false);
    const int attr_top = y;
    y++;
    for (size_t i = 0; i < f.counters.size(); i++, y++) {
        put_right(kLabelEnd, y, f.counters[i].label, false, focus == 2 + i);
        put(kUp, y, kArrowUp, form_link(Ctl::Inc, i));
        put(kDown, y, kArrowDown, form_link(Ctl::Dec, i));
        put(kNum, y, std::to_string(f.counters[i].value));
    }
    put_right(kLabelEnd, y, "Points left", false, false);
    put(kNum, y, std::to_string(f.points));
    y++;

    const bool bonus_focus = focus == f.counters.size() + 2;
    if (wide) {
        put(kRight, attr_top, "Bonus item", 0, true, bonus_focus);
        radios(f.bonus, Ctl::Bonus, kRight, attr_top + 1, true);
        y = std::max(y, attr_top + 1 + (int) f.bonus.values.size());
    } else {
        y++;
        put(1, y, "Bonus item", 0, true, bonus_focus);
        radios(f.bonus, Ctl::Bonus, 3, y + 1, true);
        y += 1 + (int) f.bonus.values.size();
    }

    y++;
    put(kUp, y, "[ Done ]", form_link(Ctl::Done), false,
        focus == f.counters.size() + 3);
    y += 2;
    put(1, y, width >= 53
                  ? "Up/Down: move   Left/Right: change   Return: select"
                  : width >= 36 ? "Arrows move/change, Return selects"
                                : "Arrows, Return");
    lay.height = y + 1;
    return lay;
}

bool parse_status_panel(const std::string &html, StatusPanel &p)
{
    size_t panel = find_id(html, "status_div");
    if (panel == std::string::npos)
        return false;
    StatusPanel out;

    /* One row per <tr> that carries a value <span id>; the label is the
     * first cell's text. */
    for (size_t tr = html.find("<tr", panel); tr != std::string::npos;) {
        size_t next = html.find("<tr", tr + 3);
        std::string row = html.substr(tr, next == std::string::npos
                                              ? std::string::npos : next - tr);
        tr = next;
        size_t span = row.find("<span");
        size_t td = row.find("<td");
        if (span == std::string::npos || td == std::string::npos || td > span)
            continue;
        StatusRow r;
        r.id = attr(tag_at(row, span), "id");
        r.label = text_after_tag(row, td);
        r.value = text_after_tag(row, span);
        if (!r.id.empty() && !r.label.empty())
            out.rows.push_back(r);
    }
    if (out.rows.empty())
        return false;

    const std::string click = "ASLEvent('HandleButtonClick',";
    for (size_t at = 0; (at = html.find(click, at)) != std::string::npos;) {
        at += click.size();
        size_t q = html.find('\'', at);
        size_t e = q == std::string::npos ? q : html.find('\'', q + 1);
        if (e == std::string::npos)
            break;
        out.buttons.push_back(html.substr(q + 1, e - q - 1));
        at = e;
    }
    p = out;
    return true;
}

bool parse_jquery_html(const std::string &js, std::string &id,
                       std::string &value)
{
    /* $('#ID').html('VALUE') with either kind of quote around each part. */
    size_t at = js.find("$(");
    if (at == std::string::npos || at + 4 >= js.size())
        return false;
    const char q1 = js[at + 2];
    if ((q1 != '\'' && q1 != '"') || js[at + 3] != '#')
        return false;
    at += 4;
    size_t m = js.find(q1, at);
    const std::string mid = ").html(";
    if (m == std::string::npos || js.compare(m + 1, mid.size(), mid) != 0)
        return false;
    size_t v = m + 1 + mid.size();
    if (v >= js.size() || (js[v] != '\'' && js[v] != '"'))
        return false;
    const std::string tail = std::string(1, js[v]) + ")";
    size_t end = js.rfind(tail);
    if (end == std::string::npos || end <= v)
        return false;
    id = js.substr(at, m - at);
    if (id.empty() || id.find_first_of(" '\")") != std::string::npos)
        return false;
    value = js.substr(v + 1, end - v - 1);
    return true;
}

}  // namespace aslxform
