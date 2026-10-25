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


/* aslxglk-output.cc -- HTML rendering, retraction and output sections of the
 * Quest 5 Glk frontend (see
 * aslxglk-internal.hh). */

#include "aslxglk-internal.hh"

namespace aslxglk {

/* ---------------------------------------------------------------- output -- */

void put_uni_char(glui32 c)
{
    glk_put_char_uni(c);
    if (g_tee)
        g_tee->push_back((char32_t) c);
}

void put_uni_string(const std::string &utf8)
{
    for (size_t i = 0; i < utf8.size();)
        put_uni_char(utf8_next_cp(utf8, i));
}

void InlineStyle::push(const StylePush &p)
{
    bold += p.b; italic += p.i; under += p.u; mono += p.m;
    if (p.size > 0)
        sizes.push_back(p.size);
    if (p.align)
        aligns.push_back(p.align);
}

void InlineStyle::pop(const StylePush &p)
{
    bold = std::max(bold - p.b, 0);
    italic = std::max(italic - p.i, 0);
    under = std::max(under - p.u, 0);
    mono = std::max(mono - p.m, 0);
    if (p.size > 0 && !sizes.empty())
        sizes.pop_back();
    if (p.align && !aligns.empty())
        aligns.pop_back();
}

StyleCombo InlineStyle::combo(double default_points, int div_align) const
{
    int align = aligns.empty() ? div_align : aligns.back();
    return style_combo(bold > 0, italic > 0, under > 0, mono > 0,
                       align == ALIGN_CENTER, align == ALIGN_RIGHT,
                       points(default_points), default_points);
}

void apply_style()
{
    glk_set_style(quest_style_for(g_style.combo(g_default_points,
                                                g_div_align)));
}

/* Value of attr="..." (or attr='...') inside a raw tag body, or "". */
std::string tag_attr(const std::string &tag, const std::string &attr)
{
    std::string t = lower(tag);
    size_t pos = 0;
    while ((pos = t.find(attr, pos)) != std::string::npos) {
        /* Whole-attribute match only: `src` must not hit inside `data-src`.
         * The char before the name must not be part of an attribute name. */
        if (pos > 0 && (isalnum((unsigned char) t[pos - 1]) ||
                        t[pos - 1] == '-' || t[pos - 1] == '_')) {
            pos += attr.size();
            continue;
        }
        size_t eq = pos + attr.size();
        while (eq < t.size() && (t[eq] == ' ' || t[eq] == '\t')) eq++;
        if (eq >= t.size() || t[eq] != '=') { pos = eq; continue; }
        eq++;
        while (eq < t.size() && (t[eq] == ' ' || t[eq] == '\t')) eq++;
        if (eq >= t.size() || (t[eq] != '"' && t[eq] != '\'')) { pos = eq; continue; }
        char q = t[eq];
        size_t end = tag.find(q, eq + 1);
        if (end == std::string::npos) return "";
        return tag.substr(eq + 1, end - eq - 1);   /* original case */
    }
    return "";
}

/* True if a boolean attribute (`autoplay`, `loop`) is present on a raw tag.
 * tag_attr only sees the attr="value" form, and HTML boolean attributes are
 * normally bare.  This walks the attribute list properly, skipping quoted
 * values, so L Too's <audio autoplay onplay="this.autoplay=''"> is not read
 * as carrying a second autoplay from inside the onplay script. */
bool tag_flag(const std::string &tag, const std::string &attr)
{
    std::string t = lower(tag);
    size_t i = 0;
    while (i < t.size() && !isspace((unsigned char) t[i]))
        i++;                                    /* skip the tag name */
    while (i < t.size()) {
        while (i < t.size() &&
               (isspace((unsigned char) t[i]) || t[i] == '/'))
            i++;
        size_t start = i;
        while (i < t.size() && !isspace((unsigned char) t[i]) &&
               t[i] != '=' && t[i] != '/')
            i++;
        std::string name = t.substr(start, i - start);
        while (i < t.size() && isspace((unsigned char) t[i]))
            i++;
        if (i < t.size() && t[i] == '=') {       /* skip the value */
            i++;
            while (i < t.size() && isspace((unsigned char) t[i]))
                i++;
            if (i < t.size() && (t[i] == '"' || t[i] == '\'')) {
                char q = t[i++];
                while (i < t.size() && t[i] != q)
                    i++;
                if (i < t.size())
                    i++;
            } else {
                while (i < t.size() && !isspace((unsigned char) t[i]))
                    i++;
            }
        }
        if (name == attr)
            return true;
    }
    return false;
}

/* A CSS font-size (12pt, 16px, 1.5em, 130%, large, smaller) in points, with
 * `points` the size it is relative to; 0 for anything else.  The keywords
 * are the CSS absolute sizes around the browser's 12pt medium. */
static double css_font_size(const std::string &value, double points)
{
    static const struct { const char *name; double pt; } keywords[] = {
        {"xx-small", 7.2}, {"x-small", 9}, {"small", 10.67}, {"medium", 12},
        {"large", 14.4}, {"x-large", 18}, {"xx-large", 24},
    };
    std::string v = trim(value);
    for (const auto &k : keywords)
        if (v == k.name)
            return k.pt;
    if (v == "smaller")
        return points / 1.2;
    if (v == "larger")
        return points * 1.2;
    char *end = nullptr;
    double n = strtod(v.c_str(), &end);
    if (end == v.c_str() || n <= 0)
        return 0;
    std::string unit = trim(end);
    if (unit == "pt")
        return n;
    if (unit == "px")
        return n * 0.75;
    if (unit == "em" || unit == "rem")
        return n * points;
    if (unit == "%")
        return n * points / 100;
    return 0;
}

/* Whether a font-family list names a monospaced face anywhere in it, the
 * way a browser would end up drawing one. */
bool mono_family(const std::string &families)
{
    static const char *mono[] = {
        "monospace", "courier", "courier new", "consolas", "monaco", "menlo",
        "lucida console", "lucida sans typewriter", "andale mono", "fixedsys",
        "dejavu sans mono", "source code pro", "ubuntu mono",
    };
    std::string f = lower(families);
    for (size_t at = 0; at <= f.size();) {
        size_t comma = f.find(',', at);
        if (comma == std::string::npos)
            comma = f.size();
        std::string name = trim(f.substr(at, comma - at));
        if (name.size() >= 2 && (name[0] == '\'' || name[0] == '"'))
            name = name.substr(1, name.size() - 2);
        for (const char *m : mono)
            if (name == m)
                return true;
        at = comma + 1;
    }
    return false;
}

static int align_of(const std::string &value)
{
    std::string v = lower(trim(value));
    if (v == "center" || v == "centre")
        return ALIGN_CENTER;
    if (v == "right")
        return ALIGN_RIGHT;
    if (v == "left" || v == "justify")
        return ALIGN_LEFT;
    return 0;
}

/* Style contribution of a style="..." CSS string, `points` being the size
 * around it.  A face the game's own default is not counts as monospaced
 * only when it is one and the default is not. */
StylePush css_style(const std::string &css, double points)
{
    StylePush p;
    std::string c = lower(css);
    for (size_t at = 0; at < c.size();) {
        size_t semi = c.find(';', at);
        if (semi == std::string::npos)
            semi = c.size();
        std::string decl = c.substr(at, semi - at);
        at = semi + 1;
        size_t colon = decl.find(':');
        if (colon == std::string::npos)
            continue;
        std::string key = trim(decl.substr(0, colon));
        std::string value = trim(decl.substr(colon + 1));
        if (key == "font-weight")
            p.b = value == "bold" || value == "bolder" ||
                  atoi(value.c_str()) >= 600;
        else if (key == "font-style")
            p.i = value == "italic" || value == "oblique";
        else if (key == "text-decoration")
            p.u = value.find("underline") != std::string::npos;
        else if (key == "font-size")
            p.size = css_font_size(value, points);
        else if (key == "font-family")
            p.m = mono_family(value) && !g_default_mono;
        else if (key == "text-align")
            p.align = align_of(value);
    }
    return p;
}

/* The block and font tags, whose style goes on a stack and is undone by the
 * matching close, as opposed to the flag tags, whose close just counts down. */
static bool tag_is_heading(const std::string &name)
{
    return name.size() == 2 && name[0] == 'h' && name[1] >= '1' &&
           name[1] <= '6';
}

bool tag_is_block(const std::string &name)
{
    return name == "font" || name == "center" || name == "p" ||
           name == "div" || tag_is_heading(name);
}

/* The style a formatting tag sets, given the size around it: what both the
 * printer and the style census understand of HTML.  False for every other
 * tag.  Headings are the browser's (bold, 2em down to 0.67em); legacy
 * <font size> is the browser's table, in points. */
bool tag_style(const std::string &name, const std::string &tag, double points,
               StylePush &p)
{
    p = StylePush();
    if (name == "b" || name == "strong") {
        p.b = 1;
    } else if (name == "i" || name == "em") {
        p.i = 1;
    } else if (name == "u") {
        p.u = 1;
    } else if (name == "tt" || name == "code" || name == "kbd" ||
               name == "samp" || name == "pre") {
        p.m = !g_default_mono;
    } else if (name == "span") {
        p = css_style(tag_attr(tag, "style"), points);
    } else if (name == "font") {
        p = css_style(tag_attr(tag, "style"), points);
        static const double legacy[] = {7.5, 9.75, 12, 13.5, 18, 24, 36};
        std::string size = trim(tag_attr(tag, "size"));
        if (!size.empty()) {
            int n = atoi(size.c_str());
            if (size[0] == '+' || size[0] == '-')
                n += 3;
            if (n >= 1 || size[0] == '-')
                p.size = legacy[std::min(std::max(n, 1), 7) - 1];
        }
        std::string face = tag_attr(tag, "face");
        if (!face.empty())
            p.m = mono_family(face) && !g_default_mono;
    } else if (name == "center") {
        p.align = ALIGN_CENTER;
    } else if (name == "p" || name == "div") {
        p = css_style(tag_attr(tag, "style"), points);
        if (!p.align)
            p.align = align_of(tag_attr(tag, "align"));
    } else if (tag_is_heading(name)) {
        static const double em[] = {2, 1.5, 1.17, 1, 0.83, 0.67};
        p = css_style(tag_attr(tag, "style"), points);
        p.b = 1;
        if (p.size <= 0)
            p.size = em[name[1] - '1'] * points;
    } else {
        return false;
    }
    return true;
}

void push_style(const StylePush &p)
{
    g_style.push(p);
    apply_style();
}

void pop_style(const StylePush &p)
{
    g_style.pop(p);
    apply_style();
}

/* Extract the click action of an <a> or <span> tag, if we understand it. */
LinkAction link_action(const std::string &tag)
{
    LinkAction a;
    a.command = tag_attr(tag, "data-command");
    if (a.command.empty()) {
        std::string oc = tag_attr(tag, "onclick");
        /* Core's ShowMenu options: onclick="ASLEvent('Func','param')". */
        size_t at = oc.find("ASLEvent(");
        if (at != std::string::npos) {
            size_t q1 = oc.find('\'', at);
            size_t q2 = q1 == std::string::npos ? q1 : oc.find('\'', q1 + 1);
            size_t q3 = q2 == std::string::npos ? q2 : oc.find('\'', q2 + 1);
            size_t q4 = q3 == std::string::npos ? q3 : oc.find('\'', q3 + 1);
            if (q4 != std::string::npos) {
                a.event_func = oc.substr(q1 + 1, q2 - q1 - 1);
                a.event_param = oc.substr(q3 + 1, q4 - q3 - 1);
            }
        }
        /* The web player's own command bridge, which games call directly from
         * hand-written markup (spondre's keyword spans, and 8 other corpus
         * games): sendCommand("literal") runs that command, while
         * sendCommand(this.innerText || this.textContent) runs whatever the
         * element says -- the renderer fills that in, having the content. */
        size_t sc = oc.find("sendCommand(");
        if (a.event_func.empty() && sc != std::string::npos) {
            size_t p = sc + strlen("sendCommand(");
            while (p < oc.size() && isspace((unsigned char) oc[p])) p++;
            if (p < oc.size() && (oc[p] == '"' || oc[p] == '\'')) {
                size_t end = oc.find(oc[p], p + 1);
                if (end != std::string::npos)
                    a.command = decode_entities(oc.substr(p + 1, end - p - 1));
            } else if (oc.compare(p, 5, "this.") == 0) {
                a.inner_text = true;
            }
        }
        /* onclick="endWait()": the page's "click to continue". */
        if (oc.find("endWait(") != std::string::npos)
            a.end_wait = true;
    }
    if (a.command.empty() && a.event_func.empty()) {
        /* An object link (class="cmdlink elementmenu") pops a verb menu in the
         * JS UI.  Carry the ELEMENT name, not a command: the menu -- and the
         * alias its chosen verb has to be phrased with -- can only be built
         * from the live world at click time (see the element branch in
         * read_line).  Sending "look at <element name>" here was wrong twice
         * over: it ignored every verb but the first, and element names are not
         * what the parser resolves, so any object whose name differs from its
         * alias answered "I can't see that." */
        std::string cls = tag_attr(tag, "class");
        std::string elem = tag_attr(tag, "data-elementid");
        if (cls.find("elementmenu") != std::string::npos && !elem.empty())
            a.element = elem;
    }
    return a;
}

/* Decode one HTML entity starting at s[i] ('&').  Returns the codepoint and
 * advances i past it, or returns '&' advancing 1 if not an entity. */
glui32 decode_entity(const std::string &s, size_t &i)
{
    size_t semi = s.find(';', i);
    if (semi == std::string::npos || semi - i > 10) { i++; return '&'; }
    std::string e = s.substr(i + 1, semi - i - 1);
    glui32 cp = 0;
    if (!e.empty() && e[0] == '#') {
        if (e.size() > 1 && (e[1] == 'x' || e[1] == 'X'))
            cp = (glui32) strtoul(e.c_str() + 2, nullptr, 16);
        else
            cp = (glui32) strtoul(e.c_str() + 1, nullptr, 10);
        if (!cp) { i++; return '&'; }
        i = semi + 1;
        return cp;
    }
    /* Games hand-write HTML, so the typographic entities an author pastes in
     * belong here alongside the markup ones -- an unknown name renders as a
     * literal "&bull;" mid-sentence. */
    struct { const char *name; glui32 cp; } ents[] = {
        {"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'},
        {"apos", '\''}, {"nbsp", ' '}, {"mdash", 0x2014}, {"ndash", 0x2013},
        {"hellip", 0x2026}, {"copy", 0xa9}, {"eacute", 0xe9},
        {"bull", 0x2022}, {"middot", 0xb7}, {"deg", 0xb0},
        {"lsquo", 0x2018}, {"rsquo", 0x2019},
        {"ldquo", 0x201c}, {"rdquo", 0x201d},
        {"laquo", 0xab}, {"raquo", 0xbb},
        {"reg", 0xae}, {"trade", 0x2122},
    };
    for (auto &en : ents)
        if (e == en.name) { i = semi + 1; return en.cp; }
    i++;
    return '&';
}

/* Entity-decode a whole attribute value (an <img src> may carry &amp;). */
std::string decode_entities(const std::string &s)
{
    std::string out;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '&')
            utf8_append(out, decode_entity(s, i));
        else
            out += s[i++];
    }
    return out;
}

/* The lowercased element name of a raw tag body ("/a href=.." -> "a"), with
 * `closing` set for an end tag.  Shared by every markup walker below. */
std::string tag_name(const std::string &tag, bool &closing)
{
    closing = !tag.empty() && tag[0] == '/';
    std::string name = lower(tag.substr(closing ? 1 : 0));
    size_t sp = name.find_first_of(" \t\r\n/");
    if (sp != std::string::npos)
        name.erase(sp);
    return name;
}

/* Case-insensitive find, for tag names in raw markup. */
/* The text content of the element whose opening tag ends at `pos`, with
 * nested same-name tags respected -- what a browser's this.innerText would
 * give an onclick="sendCommand(this.innerText)" handler. */
std::string element_inner_text(const std::string &html, size_t pos,
                               const std::string &name)
{
    std::string inner;
    int depth = 1;
    for (size_t i = pos; i < html.size();) {
        if (html[i] != '<') { inner += html[i++]; continue; }
        size_t close = html.find('>', i);
        if (close == std::string::npos) break;
        std::string tag = html.substr(i + 1, close - i - 1);
        bool closing;
        if (tag_name(tag, closing) == name) {
            if (closing && --depth == 0) break;
            if (!closing && tag[tag.size() - 1] != '/') depth++;
        }
        inner += html.substr(i, close - i + 1);
        i = close + 1;
    }
    return trim(plain_text(inner, " "));
}

/* The candidate files of the <audio> element whose opening tag `open_tag`
 * ends at `pos`, in document order: the element's own src, then every nested
 * <source src>.  *end is left just past the matching </audio> -- or at the end
 * of the chunk when the game never closed it -- so the caller can skip the
 * whole element, fallback text included. */
std::vector<std::string> audio_sources(const std::string &html, size_t pos,
                                       const std::string &open_tag, size_t *end)
{
    std::vector<std::string> srcs;
    std::string own = decode_entities(tag_attr(open_tag, "src"));
    if (!own.empty())
        srcs.push_back(own);
    int depth = 1;
    size_t i = pos;
    while (i < html.size()) {
        if (html[i] != '<') { i++; continue; }
        size_t close = html.find('>', i);
        if (close == std::string::npos) { i = html.size(); break; }
        std::string tag = html.substr(i + 1, close - i - 1);
        bool closing;
        std::string tn = tag_name(tag, closing);
        if (tn == "audio") {
            if (closing && --depth == 0) { i = close + 1; break; }
            if (!closing && tag[tag.size() - 1] != '/') depth++;
        } else if (tn == "source" && !closing && depth == 1) {
            std::string s = decode_entities(tag_attr(tag, "src"));
            if (!s.empty())
                srcs.push_back(s);
        }
        i = close + 1;
    }
    *end = i;
    return srcs;
}

/* While g_swallow is armed (a send_command whose prompt and echo the host
 * already printed), watch for the game side's own echo of the command --
 * msg("")'s blank line then "&gt; cmd" (Core's echocommand), or the bare
 * "> cmd" print of pre-540 engines -- and drop it.  Any other chunk disarms
 * and prints normally (a game with a custom HandleCommand), giving back the
 * provisionally swallowed blank.  True = the chunk was the echo. */
bool swallow_echo_chunk(const std::string &html)
{
    std::string t = trim(plain_text(html, " "));
    if (t == "> " + g_swallow_cmd) {
        g_swallow = 0;
        return true;
    }
    if (g_swallow == 2 && t.empty()) {
        g_swallow = 1;
        return true;
    }
    if (g_swallow == 1)
        put_uni_char('\n');     /* not the echo after all: restore the blank */
    g_swallow = 0;
    return false;
}

/* Remove <style>/<script> elements, and the whitespace run on either side of
 * them, from a chunk.  These are not text: a browser consumes them silently,
 * while games embed whole stylesheets inline (spondre opens with ~30 lines of
 * CSS).  Taking the surrounding whitespace too keeps the blank gap the markup
 * indentation would otherwise leave behind. */
std::string strip_hidden_elements(const std::string &html)
{
    static const char *kHidden[] = { "style", "script" };
    std::string out = html;
    for (const char *name : kHidden) {
        std::string open = std::string("<") + name;
        for (size_t at = qstr::ifind(out, open, 0); at != std::string::npos;
             at = qstr::ifind(out, open, at)) {
            /* "<style" must be the whole tag name, not a prefix. */
            size_t after = at + open.size();
            if (after < out.size() && !isspace((unsigned char) out[after]) &&
                out[after] != '>' && out[after] != '/') {
                at = after;
                continue;
            }
            size_t end = qstr::ifind(out, std::string("</") + name, at);
            if (end != std::string::npos) {
                size_t gt = out.find('>', end);
                end = gt == std::string::npos ? out.size() : gt + 1;
            } else {
                end = out.size();   /* unterminated: drop the rest */
            }
            while (at > 0 && isspace((unsigned char) out[at - 1])) at--;
            while (end < out.size() && isspace((unsigned char) out[end])) end++;
            out.erase(at, end - at);
        }
    }
    return out;
}

/* ------------------------------------------------------------- retract -- */

std::u32string u32_from_utf8(const std::string &s)
{
    std::u32string out;
    for (size_t i = 0; i < s.size();)
        out.push_back((char32_t) utf8_next_cp(s, i));
    return out;
}

/* The shared tail-match retract (questglk::unput_tail_exact), always against
 * the transcript window: false if the window has moved on, so every caller
 * degrades to "leave the text alone". */
bool unput_exact(const std::u32string &s)
{
    return unput_tail_exact(gwin, s);
}

/* ------------------------------------------------------ output sections -- */

/* Drop the sections (and the chunks they cover) that can no longer be hidden. */
void trim_out_log()
{
    if (g_sections.size() > kMaxSections) {
        size_t drop = g_sections.size() - kMaxSections;
        size_t cut = g_sections[drop].start;
        g_sections.erase(g_sections.begin(), g_sections.begin() + drop);
        if (cut > 0) {
            g_out_log.erase(g_out_log.begin(), g_out_log.begin() + cut);
            for (OutSection &s : g_sections) {
                s.start -= cut;
                if (s.end != (size_t) -1)
                    s.end -= cut;
            }
        }
    }
    /* Bound the chunk log itself too.  A section clamped here retracts only
     * what remains -- the same best-effort bound kMaxUnput already imposes. */
    if (g_out_log.size() > kMaxLogChunks) {
        size_t cut = g_out_log.size() - kMaxLogChunks;
        g_out_log.erase(g_out_log.begin(), g_out_log.begin() + cut);
        while (!g_sections.empty() && g_sections.front().end != (size_t) -1 &&
               g_sections.front().end <= cut)
            g_sections.erase(g_sections.begin());
        for (OutSection &s : g_sections) {
            s.start = s.start > cut ? s.start - cut : 0;
            if (s.end != (size_t) -1)
                s.end = s.end > cut ? s.end - cut : 0;
        }
    }
}

void start_output_section(const std::string &name)
{
    OutSection s;
    s.name = name;
    s.start = g_out_log.size();
    g_sections.push_back(s);
    trim_out_log();
}

void end_output_section(const std::string &name)
{
    for (size_t i = g_sections.size(); i-- > 0;)
        if (g_sections[i].name == name && g_sections[i].end == (size_t) -1) {
            g_sections[i].end = g_out_log.size();
            return;
        }
}

void hide_output_section(const std::string &name)
{
#ifdef SPATTERLIGHT
    size_t at = (size_t) -1;
    for (size_t i = g_sections.size(); i-- > 0;)
        if (g_sections[i].name == name) { at = i; break; }
    if (at == (size_t) -1)
        return;
    const size_t start = g_sections[at].start;
    size_t end = g_sections[at].end;
    if (end == (size_t) -1 || end > g_out_log.size())
        end = g_out_log.size();
    if (start >= g_out_log.size())
        return;

    /* Everything from the section's first chunk to the end of the window has
     * to come off, since only a suffix can be retracted. */
    std::u32string tail;
    for (size_t i = start; i < g_out_log.size(); i++) {
        if (g_out_log[i].image)
            return;             /* an attachment char will not tail-match */
        tail += g_out_log[i].text;
        if (tail.size() > kMaxUnput)
            return;
    }
    if (tail.empty())
        return;

    /* The buffer window holds a trailing newline back rather than show a blank
     * line at the prompt (GlkTextBufferWindow+Output.m:187, storedNewline), so
     * the text storage lags this log by up to one newline.  Try the exact tail
     * first, then without that newline. */
    bool took = unput_exact(tail);
    if (!took && tail.back() == U'\n') {
        /* The held-back newline stays pending after the retract and would
         * come out in front of whatever is printed next, as a blank line
         * where the section was.  Let it stand in for the newline that ends
         * the line before the section by taking that one instead. */
        std::u32string body = tail.substr(0, tail.size() - 1);
        took = unput_exact(U"\n" + body) || unput_exact(body);
    }
    if (!took)
        return;                 /* tail moved on: leave the transcript alone */

    /* Put back what followed the section.  Replaying through render_html
     * re-logs the chunks at their new positions and rebuilds their styles and
     * hyperlinks, so the g_links indices the replayed anchors get are fresh. */
    std::vector<OutChunk> replay(g_out_log.begin() + end, g_out_log.end());
    g_out_log.erase(g_out_log.begin() + start, g_out_log.end());
    for (size_t i = g_sections.size(); i-- > 0;)
        if (g_sections[i].start >= start)
            g_sections.erase(g_sections.begin() + i);
    for (const OutChunk &c : replay)
        if (!c.html.empty())    /* a host prompt's trail: retracted for good */
            render_html(c.html);
#else
    (void) name;                /* CheapGlk has no unput: nothing to hide */
#endif
}

/* JS.clearScreen / request (ClearScreen): wipe the transcript.  Only the
 * story window -- the reference player's clearScreen empties #divOutput and
 * leaves the panes and the picture frame alone.  Everything whose meaning
 * died with the text goes too: the transcript links (their anchors are gone,
 * and a cleared-away link must not hand its number's action to a later link
 * -- same high-water scheme as disable_command_links) and the output-section
 * log (no recorded chunk can tail-match an emptied window; a section opened
 * before the clear hides as a no-op, like jQuery hiding a destroyed div).
 * Installed (and so referenced) only under SPATTERLIGHT. */
void clear_screen_ui()
{
    for (size_t i = g_links_spent; i < g_links.size(); i++)
        g_links[i] = LinkAction{};
    g_links_spent = g_links.size();
    g_out_log.clear();
    g_sections.clear();
    g_continue_shown.clear();   /* chrome died with the window */
    glk_window_clear(gwin);
    g_clear_gen++;
}

/* Render one HTML chunk (a JS.addText payload) into the main window. */
void render_html_inner(const std::string &raw)
{
    const std::string html =
        qstr::ifind(raw, "<style", 0) != std::string::npos ||
        qstr::ifind(raw, "<script", 0) != std::string::npos
            ? strip_hidden_elements(raw) : raw;
    const InlineStyle base = g_style;
    std::vector<StylePush> spans;   /* open <span>s */
    std::vector<StylePush> anchors; /* open <a>s */
    std::vector<char> span_link;    /* did that <span> open a hyperlink? */
    std::vector<std::pair<std::string, StylePush>> blocks;  /* open blocks */
    for (size_t i = 0; i < html.size();) {
        char ch = html[i];
        if (ch == '<') {
            size_t close = html.find('>', i);
            if (close == std::string::npos) break;
            std::string tag = html.substr(i + 1, close - i - 1);
            i = close + 1;
            bool closing;
            std::string name = tag_name(tag, closing);

            StylePush p;
            if (name == "br") {
                put_uni_char('\n');     /* teed: see g_tee */
            } else if (tag_is_block(name)) {
                /* The text stays as it was: only the style changes. */
                if (!closing) {
                    tag_style(name, tag, g_style.points(g_default_points), p);
                    blocks.emplace_back(name, p);
                    push_style(p);
                } else {
                    for (size_t b = blocks.size(); b-- > 0;)
                        if (blocks[b].first == name) {
                            /* Closing an outer block closes those inside. */
                            while (blocks.size() > b) {
                                pop_style(blocks.back().second);
                                blocks.pop_back();
                            }
                            break;
                        }
                }
            } else if (name == "span") {
                if (!closing) {
                    tag_style(name, tag, g_style.points(g_default_points), p);
                    /* Games hang clicks off bare spans too (spondre's
                     * keywords), so a span carries a link action like an
                     * anchor does. */
                    LinkAction act = link_action(tag);
                    if (act.inner_text)
                        act.command = element_inner_text(html, i, "span");
                    bool opened = false;
                    if (act.live() && g_hyperlinks) {
                        g_links.push_back(act);
                        glk_set_hyperlink((glui32) g_links.size());
                        opened = true;
                    } else if (act.live()) {
                        p.u = 1;   /* no hyperlink support: underline only */
                    }
                    spans.push_back(p);
                    span_link.push_back(opened ? 1 : 0);
                    push_style(p);
                } else if (!spans.empty()) {
                    if (span_link.back() && g_hyperlinks)
                        glk_set_hyperlink(0);
                    span_link.pop_back();
                    pop_style(spans.back());
                    spans.pop_back();
                }
            } else if (name == "a") {
                if (!closing) {
                    LinkAction act = link_action(tag);
                    if (act.inner_text)
                        act.command = element_inner_text(html, i, "a");
                    bool live = act.live();
                    if (live && g_hyperlinks) {
                        g_links.push_back(act);
                        glk_set_hyperlink((glui32) g_links.size());
                    } else if (live) {
                        p.u = 1;   /* no hyperlink support: underline only */
                    }
                    anchors.push_back(p);
                    push_style(p);
                } else if (!anchors.empty()) {
                    if (g_hyperlinks)
                        glk_set_hyperlink(0);
                    pop_style(anchors.back());
                    anchors.pop_back();
                }
            } else if (name == "img") {
                /* Core's `picture` and the {img:} text-processor command
                 * both emit <img src="filename"/> (GetFileURL returns the
                 * filename as-is in this engine); games also hand-write the
                 * tag around GetFileUrl(). Unresolvable images are dropped
                 * like any other unknown tag. */
                if (!closing) {
                    std::string src = decode_entities(tag_attr(tag, "src"));
                    if (!src.empty())
                        draw_image_inline(src);
                }
            } else if (name == "audio") {
                /* Games hand-write their own sound effects rather than call
                 * Core's `play sound`: L Too's play_sound and Night House both
                 * emit <audio autoplay><source src="x.ogg"/><source
                 * src="x.mp3"/>Your browser does not support the audio
                 * tag.</audio>.  Play the first source that resolves, the same
                 * choice a browser makes over the sources it can decode.
                 *
                 * The element is skipped whole either way.  Its text is
                 * FALLBACK content -- shown only by a browser too old to know
                 * the tag, never alongside playback -- so printing it (which is
                 * what dropping the tag but keeping its children used to do)
                 * was always wrong here, whether or not the sound starts. */
                if (!closing) {
                    size_t end = i;
                    std::vector<std::string> srcs =
                        audio_sources(html, i, tag, &end);
                    if (tag_flag(tag, "autoplay"))
                        play_audio_inline(srcs, tag_flag(tag, "loop"));
                    i = end;
                }
            } else if (tag_style(name, tag, 0, p)) {
                /* the flag tags (b, i, u, tt...): counters, so a stray close
                 * undoes nothing it did not open */
                closing ? pop_style(p) : push_style(p);
            }
            /* every other tag (table, ul...) is dropped: §4 says
             * best-effort subset, not a browser. */
        } else if (ch == '&') {
            put_uni_char(decode_entity(html, i));
        } else if (ch == '\r') {
            i++;
        } else {
            /* UTF-8 sequence or plain char (raw newlines kept, as in the
             * oracle transcripts). */
            put_uni_char(utf8_next_cp(html, i));
        }
    }
    /* The player appends each chunk as its own HTML fragment, and the browser
     * closes whatever the fragment leaves open: Warriors' "<b> Press any key
     * to continue. <b/>" opens bold twice and never closes it, yet the text
     * after it is plain.  So a chunk hands back the style it started with,
     * and closes any hyperlink it left open -- a span can have opened one
     * just as an anchor can, and a stray glk_set_hyperlink(0) is a no-op. */
    if ((!spans.empty() || !anchors.empty()) && g_hyperlinks)
        glk_set_hyperlink(0);
    if (g_style.bold != base.bold || g_style.italic != base.italic ||
        g_style.under != base.under || g_style.mono != base.mono ||
        g_style.sizes != base.sizes || g_style.aligns != base.aligns) {
        g_style = base;
        apply_style();
    }
}

/* Render a chunk, recording what it wrote while any section is live so
 * HideOutputSection can retract it later.  With no section open (every game
 * that never calls StartOutputSection, and the headless harness) this is
 * render_html_inner plus one branch. */
/* Keep Deeper's dialog and status panel markup out of the transcript and
 * remember what they describe. */
bool take_custom_ui(const std::string &raw)
{
    if (raw.find("dialog_window_1") != std::string::npos &&
        aslxform::parse_character_form(raw, g_form)) {
        g_form_pending = true;
        return true;
    }
    if (raw.find("status_div") != std::string::npos &&
        aslxform::parse_status_panel(raw, g_cstatus)) {
        g_pane_dirty = true;
        return true;
    }
    return false;
}

void render_html(const std::string &raw)
{
    if (take_custom_ui(raw))
        return;
    if (g_sections.empty()) {
        if (g_swallow && swallow_echo_chunk(raw))
            return;
        render_html_inner(raw);
        return;
    }
    OutChunk c;
    std::u32string *saved = g_tee;   /* replay re-enters through here */
    g_tee = &c.text;
    const size_t images = g_images_drawn;
    /* The swallow runs INSIDE the tee: when it decides a provisionally
     * swallowed blank was not the echo after all it puts that newline back,
     * and the recorded text has to account for it or the retract below stops
     * matching the window. */
    const bool swallowed = g_swallow && swallow_echo_chunk(raw);
    if (!swallowed)
        render_html_inner(raw);
    g_tee = saved;
    c.image = g_images_drawn != images;
    /* A swallowed chunk's own html must never be replayed -- the swallow is
     * spent, so re-rendering it would print the echo this time. All it can
     * leave behind is that restored newline. */
    c.html = swallowed ? "<br/>" : raw;
    if (!swallowed || !c.text.empty()) {
        g_out_log.push_back(c);
        trim_out_log();
    }
}

/* The custom status panel as "Label: value" lines.  An id the markup uses
 * twice (Deeper has two "defence-span" cells) is updated in its first cell
 * only, which is all a jQuery id selector reaches. */
std::vector<std::string> custom_status_lines()
{
    std::vector<std::string> out;
    std::vector<std::string> seen;
    for (const aslxform::StatusRow &r : g_cstatus.rows) {
        std::string value = r.value;
        auto it = g_cstatus_vals.find(r.id);
        if (it != g_cstatus_vals.end() &&
            std::find(seen.begin(), seen.end(), r.id) == seen.end())
            value = it->second;
        seen.push_back(r.id);
        out.push_back(r.label + " " + value);
    }
    return out;
}

/* ---------------------------------------------------------------- styles -- */

namespace {

/* One field of the game element, inherited types included (defaultgame
 * gives defaultfontsize and defaultfont), or nullptr. */
const Value *game_field(const World &w, const char *name)
{
    const Value *out = nullptr;
    walk_inherited(w, w.find("game"), [&](Element *e) {
        out = e->field(name);
        return out != nullptr;
    });
    return out;
}

std::string game_string(const World &w, const char *name)
{
    const Value *v = game_field(w, name);
    return v && v->type == Value::Type::String ? v->str : std::string();
}

/* The attribute states of the text a Quest 5 game can print, read from the
 * loaded world without running it: every string attribute, template and
 * string literal in a script, walked through the printer's own tag_style.
 * The text processor's {b:...} {i:...} {u:...} count as the tags they
 * become; other {...} commands count the text after their last colon. */
struct StyleScan {
    questglk::StyleCensus census;

    void html(const std::string &text, bool intro, int div_align = 0)
    {
        InlineStyle st;
        std::vector<std::pair<std::string, StylePush>> blocks;
        std::vector<StylePush> braces;
        std::string run;
        StyleCombo run_combo = st.combo(g_default_points, div_align);
        auto flush = [&] {
            census.count(run_combo, run, intro);
            run.clear();
            run_combo = st.combo(g_default_points, div_align);
        };
        for (size_t i = 0; i < text.size();) {
            char ch = text[i];
            if (ch == '<') {
                size_t close = text.find('>', i);
                if (close == std::string::npos)
                    break;
                std::string tag = text.substr(i + 1, close - i - 1);
                i = close + 1;
                bool closing;
                std::string name = tag_name(tag, closing);
                StylePush p;
                if (!tag_style(name, tag, st.points(g_default_points), p))
                    continue;
                flush();
                if (!tag_is_block(name) && name != "span") {
                    closing ? st.pop(p) : st.push(p);
                } else if (!closing) {
                    blocks.emplace_back(name, p);
                    st.push(p);
                } else {
                    for (size_t b = blocks.size(); b-- > 0;)
                        if (blocks[b].first == name) {
                            while (blocks.size() > b) {
                                st.pop(blocks.back().second);
                                blocks.pop_back();
                            }
                            break;
                        }
                }
                run_combo = st.combo(g_default_points, div_align);
            } else if (ch == '{') {
                size_t end = text.find('}', i);
                size_t colon = text.find(':', i);
                StylePush p;
                if (text.compare(i, 3, "{b:") == 0)
                    p.b = 1;
                else if (text.compare(i, 3, "{i:") == 0)
                    p.i = 1;
                else if (text.compare(i, 3, "{u:") == 0)
                    p.u = 1;
                if (p.b || p.i || p.u) {
                    flush();
                    st.push(p);
                    run_combo = st.combo(g_default_points, div_align);
                    i += 3;
                } else if (colon < end) {
                    i = colon + 1;
                } else {
                    i = end == std::string::npos ? text.size() : end + 1;
                    continue;               /* {=expr}: no text of its own */
                }
                braces.push_back(p);
            } else if (ch == '}' && !braces.empty()) {
                flush();
                st.pop(braces.back());
                braces.pop_back();
                run_combo = st.combo(g_default_points, div_align);
                i++;
            } else {
                run += ch;
                i++;
            }
        }
        flush();
    }

    /* The string literals of a script.  Those on a PrintCentered line, or
     * between SetAlignment ("center") and the next SetAlignment, are
     * centred; those after a SetFontSize (N) are at N points, the size
     * Core's wrapping span then gives them. */
    void script(const std::string &code, bool intro)
    {
        int div_align = 0;
        long points = 0;
        size_t line_start = 0;
        for (size_t i = 0; i < code.size(); i++) {
            if (code[i] == '\n') {
                line_start = i + 1;
                continue;
            }
            if (code.compare(i, 11, "SetFontSize") == 0) {
                size_t open = code.find_first_not_of(" \t(", i + 11);
                if (open != std::string::npos &&
                    isdigit((unsigned char) code[open]))
                    points = strtol(code.c_str() + open, nullptr, 10);
                i += 10;
                continue;
            }
            if (code.compare(i, 12, "SetAlignment") == 0) {
                size_t q = code.find('"', i);
                size_t nl = code.find('\n', i);
                div_align = q < nl && code.compare(q + 1, 6, "center") == 0
                                ? ALIGN_CENTER
                          : q < nl && code.compare(q + 1, 5, "right") == 0
                                ? ALIGN_RIGHT : 0;
                i = q < nl ? code.find('"', q + 1) : i;
                if (i == std::string::npos)
                    return;
                continue;
            }
            if (code[i] != '"')
                continue;
            std::string lit;
            for (i++; i < code.size() && code[i] != '"'; i++) {
                if (code[i] == '\\' && i + 1 < code.size())
                    i++;
                lit += code[i];
            }
            size_t nl = code.find('\n', line_start);
            bool centred = code.substr(line_start, nl - line_start)
                               .find("PrintCentered") != std::string::npos;
            if (points > 0)
                lit = "<span style=\"font-size:" + std::to_string(points) +
                      "pt\">" + lit + "</span>";
            html(lit, intro, centred ? ALIGN_CENTER : div_align);
        }
    }

    void value(const Value &v, bool intro)
    {
        switch (v.type) {
        case Value::Type::String:
            html(v.str, intro);
            break;
        case Value::Type::Script:
            script(v.str, intro);
            break;
        case Value::Type::StringList:
            for (const Value &e : v.list())
                value(e, intro);
            break;
        case Value::Type::StringDict:
        case Value::Type::ScriptDict:
            for (const Value::DictEntry &e : v.dict())
                value(e.second, intro);
            break;
        default:
            break;
        }
    }
};

}  // namespace

/* Count the attribute states the game's text uses and hand out the styles
 * for them, then set the hints that draw them, all before the first window
 * opens.  The game's start script and Core's centred title (StartGame:
 * the name at 260%, subtitle 130%, author 140%) are the introduction. */
void assign_game_styles(World &w)
{
    if (const Value *v = game_field(w, "defaultfontsize"))
        if (v->type == Value::Type::Int && v->integer > 0)
            g_default_points = (double) v->integer;
    g_default_mono = mono_family(game_string(w, "defaultwebfont") + "," +
                                 game_string(w, "defaultfont"));

    StyleScan scan;
    Element *game = w.find("game");
    for (const auto &e : w.elements)
        for (const auto &f : e->fields)
            scan.value(f.second, e.get() == game && f.first == "start");
    for (const auto &t : w.templates)
        scan.html(t.second, false);
    for (const auto &t : w.verb_templates)
        scan.html(t.second, false);
    for (const auto &t : w.dynamic_templates)
        scan.script(t.second, false);

    const Value *show = game_field(w, "showtitle");
    if (!show || show->type != Value::Type::Boolean || show->boolean) {
        std::string by = "by";
        for (const auto &t : w.templates)
            if (t.first == "By")
                by = t.second;
        static const struct { const char *field, *size; bool by; } title[] = {
            {"gamename", "260%", false}, {"subtitle", "130%", false},
            {"author", "140%", true},
        };
        for (const auto &t : title) {
            std::string text = game_string(w, t.field);
            if (!text.empty())
                scan.html(std::string("<span style=\"font-size:") + t.size +
                              "\">" + (t.by ? by + " " : "") + text +
                              "</span>",
                          true, ALIGN_CENTER);
        }
    }

    questglk::StyleTable table;
    questglk::style_assign(scan.census.ranked(), table);
    if (getenv("ASLXGLK_STYLE_CENSUS"))
        for (const questglk::StyleCensusRow &r : scan.census.ranked()) {
            int style = questglk::style_lookup(table, r.combo);
            fprintf(stderr, "census %-24s %8lu chars %6lu intro -> %s",
                    questglk::style_combo_describe(r.combo).c_str(),
                    r.characters, r.intro_characters,
                    questglk::style_name(style));
            if (table.combo[style] != r.combo)
                fprintf(stderr, " (%s)", questglk::style_combo_describe(
                                             table.combo[style]).c_str());
            fputc('\n', stderr);
        }
    questglk::style_install(table);
    questglk::style_set_hints();
}

}  // namespace aslxglk
