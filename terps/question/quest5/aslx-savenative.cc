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

// aslx-savenative.cc -- Quest 5 native `.quest-save` compatibility (TODO §5).
//
// This is the interoperable save format: an ASLX document Quest's own desktop
// player and QuestViva read and write, as opposed to the compact v1 snapshot
// in aslx-state.cc (whose `save_family` it shares).
//
//   <!-- Saved by Question ... -->
//   <asl version="550" original="/path/Game.quest">
//     <object name="player"> <inherit name="editor_object"/> ... fields ... </object>
//     <game name="My Game"> ... </game>
//     <timer name="t1"> ... </timer>
//   </asl>
//
// Port of QuestViva's GameSaver (SaveMode.SavedGame) + FieldSaver + ObjectSaver,
// restricted to the mutable object family. Loading is the inverse: the host has
// already reloaded the original game (a full World); restore_game_native
// overlays this document onto it through overlay_aslx_buffer -- each saved
// element DISPLACES its freshly-loaded twin (QuestViva Elements.Add override) --
// then drops any family element the save omitted (destroyed at save time).
//
// Why only the object family, when Quest re-emits the whole world (functions,
// types, delegates, templates, ...)? Those never change at runtime, so they
// come back identically from the reload; omitting them keeps the save small and
// still yields a document Quest accepts (its loader overlays our elements onto
// the same reloaded original). The reader is symmetric: it ignores non-family
// elements a Quest-written save carries, since the reload already has them.

#include "aslx-runtime-internal.hh"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <set>

namespace aslx {

namespace {

// ---- XML writing ----------------------------------------------------------

// QuestViva's GameXmlWriter uses CDATA when a string contains <, > or &.
bool needs_cdata(const std::string &s) {
    return s.find('<') != std::string::npos || s.find('>') != std::string::npos ||
           s.find('&') != std::string::npos;
}

void xml_escape_into(std::string &out, const std::string &s, bool attr) {
    for (char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += attr ? "&quot;" : "\""; break;
        default:  out += c; break;
        }
    }
}

// Emit text content, matching GameXmlWriter.WriteString: CDATA when it holds
// XML metacharacters (a bare "]]>" is split so it cannot close the section
// early), otherwise entity-escaped.
void write_text(std::string &out, const std::string &s) {
    if (!needs_cdata(s)) {
        xml_escape_into(out, s, false);
        return;
    }
    out += "<![CDATA[";
    size_t pos = 0;
    for (;;) {
        size_t close = s.find("]]>", pos);
        if (close == std::string::npos) {
            out += s.substr(pos);
            break;
        }
        out += s.substr(pos, close - pos);
        out += "]]]]><![CDATA[>";  // split the "]]>" across two sections
        pos = close + 3;
    }
    out += "]]>";
}

using Attrs = std::vector<std::pair<std::string, std::string>>;

struct SaveWriter {
    std::string out;
    int indent = 0;
    int version = 0;

    void pad() { out.append((size_t)indent * 2, ' '); }

    // The indented `<tag a="v" ...`, left for the caller to finish.
    void start(const std::string &tag, const Attrs &attrs) {
        pad();
        out += '<';
        out += tag;
        for (const auto &a : attrs) {
            out += ' ';
            out += a.first;
            out += "=\"";
            xml_escape_into(out, a.second, true);
            out += '"';
        }
    }

    // <tag a="v" ...> opening a nested body, which close() ends.
    void open(const std::string &tag, const Attrs &attrs) {
        start(tag, attrs);
        out += ">\n";
        ++indent;
    }

    void close(const std::string &tag) {
        --indent;
        pad();
        out += "</";
        out += tag;
        out += ">\n";
    }

    // A leaf element: <tag type="..">text</tag> on one line, or a self-closed
    // <tag/> when `have_text` is false.
    void leaf(const std::string &tag, const Attrs &attrs,
              const std::string &text, bool have_text = true) {
        start(tag, attrs);
        if (!have_text) {
            out += "/>\n";
            return;
        }
        out += '>';
        write_text(out, text);
        out += "</";
        out += tag;
        out += ">\n";
    }
};

// Shortest decimal that round-trips a double (QuestViva writes the invariant
// ToString; C++ has no built-in shortest, so try increasing precision).
std::string double_to_str(double d) {
    for (int prec = 15; prec < 17; ++prec) {
        std::string text = c_format_g(d, prec);
        if (c_strtod(text.c_str()) == d) return text;
    }
    return c_format_g(d, 17);
}

// A string/object dictionary can faithfully hold ONLY strings / object refs; a
// value of any other type (int, double, boolean, or a nested collection) forces
// the generic typed "dictionary" form with per-value types. (Quest's grid
// coordinate sub-dictionaries hold x/y/z doubles + a grid_isdrawn boolean.)
bool dict_needs_generic(const Value &v) {
    Value::Type want = v.type == Value::Type::ObjectDict ? Value::Type::ObjectRef
                                                         : Value::Type::String;
    for (const auto &kv : v.dict())
        if (kv.second.type != want) return true;
    return false;
}

// Likewise a stringlist can hold only strings; anything else needs the generic
// typed "list".
bool list_needs_generic(const Value &v) {
    for (const Value &e : v.list())
        if (e.type != Value::Type::String) return true;
    return false;
}

// Canonical string for a dictionary entry value. Our loader materialises
// string/object-dictionary entries as String/ObjectRef only (typed dict values
// are not round-tripped), so a runtime-added boxed entry (e.g. a boolean flag
// on game.currentcommandresolvedelements) is coerced to its textual form rather
// than serialized as an empty <value>. QuestViva's legacy SaveString does the
// same (ToString of the boxed value).
std::string dict_value_string(const Value &v) {
    switch (v.type) {
    case Value::Type::Null:      return "";
    case Value::Type::String:
    case Value::Type::Script:
    case Value::Type::ObjectRef: return v.str;
    case Value::Type::Int:       return std::to_string(v.integer);
    case Value::Type::Double:    return double_to_str(v.dbl);
    case Value::Type::Boolean:   return v.boolean ? "true" : "false";
    default:                     return "";  // nested collections: not expressible
    }
}

// The entries of a string/object list, joined with `sep`.
std::string join_names(const Value &v, const char *sep) {
    std::string joined;
    const auto &l = v.list();
    for (size_t i = 0; i < l.size(); ++i) {
        if (i) joined += sep;
        joined += l[i].str;
    }
    return joined;
}

// A delegate-implementation script keeps the delegate name as its type
// (DelegateImplementationSaver); a plain script is type="script".
std::string script_type(const Value &v) {
    return v.declared_type.empty() ? "script" : v.declared_type;
}

// A scalar/joined field name QuestViva writes as its own tag (<myattr .../>)
// vs. wrapped (<attr name="my attr" .../>) -- OnlyLettersAndNumbers in
// FieldSaverBase.WriteAttribute (underscores force the wrapper).
bool simple_attr_name(const std::string &n) {
    if (n.empty()) return false;
    for (char c : n)
        if (!std::isalnum((unsigned char)c)) return false;
    return true;
}

// A v540+ NESTED collection (stringlist / list / *dictionary) is written with
// the attribute name AS the tag directly (QuestViva's WriteStartElement(attr),
// no OnlyLettersAndNumbers guard) -- so underscores are kept, unlike the scalar
// path. Guard only against a name that is not a legal XML element name (e.g.
// one with a space), which would fall back to the <attr name=..> wrapper.
bool xml_tag_ok(const std::string &n) {
    if (n.empty()) return false;
    unsigned char c0 = (unsigned char)n[0];
    if (!(std::isalpha(c0) || c0 == '_' || c0 >= 0x80)) return false;
    for (char c : n) {
        unsigned char u = (unsigned char)c;
        if (!(std::isalnum(u) || u == '_' || u == '-' || u == '.' || u >= 0x80))
            return false;
    }
    return true;
}

// The ObjectSaver family, saved nested by containment; timers save flat
// (TimerSaver).
bool nested_family(const std::string &t) {
    return t == "object" || t == "game" || t == "command" || t == "verb" ||
           t == "exit" || t == "turnscript";
}

// Ignore-field sets, mirroring ObjectSaver/ElementSaverBase/TimerSaver. `name`,
// `elementtype` and our synthetic bookkeeping fields are handled structurally
// (element attributes / nesting) and must not be re-emitted as <field>s.
bool ignore_field(const std::string &elem_type, const std::string &attr) {
    if (attr == "name" || attr == "elementtype") return true;
    // ObjectSaver family: parent is nesting, type is the element tag /
    // inherits.
    if (nested_family(elem_type) && (attr == "type" || attr == "parent"))
        return true;
    if (elem_type == "game" && attr == "gamename") return true;
    if ((elem_type == "command" || elem_type == "verb") &&
        (attr == "isverb" || attr == "anonymous"))
        return true;
    if (elem_type == "exit" &&
        (attr == "to" || attr == "alias" || attr == "anonymous"))
        return true;
    if (elem_type == "turnscript" && attr == "anonymous") return true;
    if (elem_type == "timer" && attr == "parent") return true;
    return false;
}

// The default type auto-added on load; excluded from the saved <inherit> list
// (QuestViva's CanSaveTypeName), re-added by apply_default_types on reload.
bool is_default_type(const std::string &t) {
    return t == "defaultobject" || t == "defaultgame" || t == "defaultexit" ||
           t == "defaultcommand" || t == "defaultturnscript" ||
           t == "defaultverb";
}

// ---- firsttime baking -----------------------------------------------------
//
// QuestViva persists `firsttime` state IN the saved script: FirstTimeScript.Save
// re-serializes a firsttime that has ALREADY RUN as just its `otherwise` body
// (or nothing). We store scripts as raw source with a separate per-instance ran
// flag, so to produce a Quest-loadable save we must reproduce that bake --
// otherwise every firsttime block (intros, one-time hints) re-fires on reload.
//
// The transform walks the source statement by statement exactly like
// parse_statements, alongside the statements parse_statements made of it: a
// block is found in the text where the parser found it and baked against the
// statements the parser put there, and whether a firsttime has run is read off
// its own statement. Text the parser built nothing from is not descended into:
// a statement it rejected is emitted verbatim, and so is a trailing script-
// block argument (a separately compiled script, with flags of its own), while
// an `else`, `otherwise` or `default` it dropped -- one with nothing to
// continue, or replaced by a later one -- is dead and left out, since behind a
// baked-out firsttime it would continue whatever statement came before.

std::string bake_stmt_list(const std::string &inner,
                           const std::vector<Stmt> &stmts);

// "<head> {" around `block`, baked against the statements parsed from it.
std::string bake_block(const std::string &head, const std::string &block,
                       const std::vector<Stmt> &stmts) {
    return head + " {\n" + bake_stmt_list(block, stmts) + "\n}";
}

std::string bake_switch(const std::string &stmt, const Stmt &s) {
    // Split off the switch expression like the parser (get_parameter), not on
    // the first '{': a one-line switch arrives with its outer braces already
    // stripped by get_script, where split_block would take the first CASE's
    // brace instead and lose the body.
    std::string after;
    bool found = false;
    get_parameter(stmt, after, found);
    if (!found) return stmt;
    std::string prefix = stmt.substr(0, stmt.size() - after.size());
    std::string body = remove_surrounding_braces(rt_trim(after));
    // Split the switch body into its case/default chunks (source order).
    std::vector<std::string> chunks;
    size_t last_default = std::string::npos;
    std::string line = rt_trim(body);
    while (!line.empty()) {
        std::string cafter;
        std::string c = rt_trim(get_script(line, cafter));
        line = cafter;
        if (starts_with_word(c, "case")) {
            chunks.push_back(c);
        } else if (starts_with_word(c, "default")) {
            last_default = chunks.size();
            chunks.push_back(c);
        }
        if (rt_trim(line).empty()) break;
    }
    // Re-emit in source order: each `case` is the parser's next one, and the
    // last `default` is its else_body.
    std::string out = prefix + " {\n";
    size_t ncase = 0;
    for (size_t i = 0; i < chunks.size(); ++i) {
        const std::string &c = chunks[i];
        if (i == last_default) {
            out += bake_block("default", rt_trim(c.substr(7)), s.else_body) + "\n";
        } else if (starts_with_word(c, "case") && ncase < s.cases.size()) {
            const std::vector<Stmt> &block = s.cases[ncase++].second;
            std::string a2;
            bool f2 = false;
            get_parameter(c, a2, f2);
            if (f2)
                out += bake_block(c.substr(0, c.size() - a2.size()),
                                  rt_trim(a2), block) + "\n";
        }
    }
    out += "}";
    return out;
}

// "<keyword> (<parameter>) <block>": the block follows the parameter, as in
// parse_one_statement. split_block (first '{') would mis-split a one-liner
// whose own braces get_script already stripped -- the first brace it finds
// then belongs to a NESTED statement, and the body text corrupts.
std::string bake_after_parameter(const std::string &stmt,
                                 const std::vector<Stmt> &stmts) {
    std::string after;
    bool found = false;
    get_parameter(stmt, after, found);
    std::string body = rt_trim(after);
    if (!found || body.empty()) return stmt;
    return bake_block(stmt.substr(0, stmt.size() - after.size()), body, stmts);
}

// "<keyword> <block>": the block follows the keyword itself.
std::string bake_after_keyword(const std::string &stmt, size_t keyword_len,
                               const std::vector<Stmt> &stmts) {
    std::string body = rt_trim(stmt.substr(keyword_len));
    if (body.empty()) return stmt;
    return bake_block(stmt.substr(0, keyword_len), body, stmts);
}

// Bake a statement that is neither a firsttime nor a continuation of the one
// before; `s` is what the parser made of it.
std::string bake_generic(const std::string &stmt, const Stmt &s) {
    using Shape = BlockKeyword::Shape;
    // Which statements carry a block, and of what shape, is the parser's own
    // table. A statement of any other kind has none: not a block statement at
    // all, or one the parser turned down (a `show menu` or `ask` with the
    // wrong parameters, anything that failed to compile).
    const BlockKeyword *bk = block_keyword(stmt);
    if (!bk || s.kind != bk->kind) return stmt;
    switch (bk->shape) {
        case Shape::Switch:    return bake_switch(stmt, s);
        case Shape::Bare:      return bake_after_keyword(stmt, std::strlen(bk->word), s.body);
        case Shape::Parameter: break;
    }
    return bake_after_parameter(stmt, s.body);
}

std::string bake_stmt_list(const std::string &inner,
                           const std::vector<Stmt> &stmts) {
    std::string line = remove_comments(remove_surrounding_braces(inner));
    // The statements, split as parse_statements splits them.
    std::vector<std::string> text;
    while (true) {
        std::string after;
        std::string stmt = rt_trim(get_script(line, after));
        line = after;
        if (!stmt.empty()) text.push_back(stmt);
        if (rt_trim(line).empty()) break;
    }
    // parse_statements folds an `otherwise` or an `else` into the statement
    // before it instead of making it one of its own.
    auto is_otherwise = [&](size_t i) { return starts_with_word(text[i], "otherwise"); };
    auto continues = [&](size_t i) {
        return is_otherwise(i) || starts_with_word(text[i], "else");
    };
    auto is_else_if = [&](size_t i) {
        return !is_otherwise(i) &&
               starts_with_word(rt_trim(text_after(text[i], "else")), "if");
    };

    std::vector<std::string> pieces;
    size_t next = 0;  // stmts[next] is the next statement's
    for (size_t i = 0; i < text.size();) {
        if (continues(i)) { ++i; continue; }  // nothing before it to continue
        const std::string &stmt = text[i++];
        if (next >= stmts.size()) { pieces.push_back(stmt); continue; }
        const Stmt &s = stmts[next++];
        // The continuations that follow, [i, end). Each kind's last one is the
        // one the parser kept, as the statement's else_body.
        size_t end = i, last_otherwise = std::string::npos,
               last_else = std::string::npos;
        for (; end < text.size() && continues(end); ++end) {
            if (is_otherwise(end)) last_otherwise = end;
            else if (!is_else_if(end)) last_else = end;
        }

        if (s.kind == Stmt::Kind::FirstTime) {
            bool ran = s.ran && *s.ran;
            // The body is everything after the keyword, exactly as the parser
            // sees it -- get_script has already stripped the braces off a
            // one-line block, so split_block would find no '{' (or a nested
            // statement's) and lose the body.
            std::string body =
                ran ? std::string()
                    : bake_block("firsttime", rt_trim(stmt.substr(9)), s.body);
            if (last_otherwise == std::string::npos) {
                // A run firsttime emits nothing, else it stays.
                if (!ran) pieces.push_back(body);
            } else {
                std::string other = bake_stmt_list(
                    rt_trim(text[last_otherwise].substr(9)), s.else_body);
                // Run: just the otherwise body.
                pieces.push_back(ran ? other
                                     : body + " otherwise {\n" + other + "\n}");
            }
        } else {
            pieces.push_back(bake_generic(stmt, s));
            if (s.kind == Stmt::Kind::If) {
                // An else's block sits the same two ways a statement's does:
                // straight after the keyword, or after the condition of an
                // `else if`. Those are the parser's elseifs, in order.
                size_t nelseif = 0;
                for (size_t j = i; j < end; ++j) {
                    if (j == last_else)
                        pieces.push_back(
                            bake_after_keyword(text[j], 4, s.else_body));
                    else if (is_else_if(j) && nelseif < s.elseifs.size())
                        pieces.push_back(bake_after_parameter(
                            text[j], s.elseifs[nelseif++].second));
                }
            }
        }
        i = end;
    }

    std::string out;
    bool first = true;
    for (const std::string &p : pieces) {
        if (p.empty()) continue;  // a run firsttime with no otherwise
        if (!first) out += "\n";
        out += p;
        first = false;
    }
    return out;
}

// Emit one entry as a `<value ...>` element (QuestViva's FieldSaver.SaveValue):
// a general list's entries and a generic `dictionary`'s item values are typed
// and may themselves be collections (Quest's nested grid dictionaries), so this
// recurses. Unlike a field, a value-position scalar ALWAYS carries its type
// (IsImpliedType's element==null path). The depth cap breaks the infinite
// recursion of a self-referential collection (`list add (l, l)`): levels past
// it truncate to null rather than overflowing the stack.
void write_value(SaveWriter &w, Interp &in, const Value &v, int depth = 0);

// The type a nested collection is saved under. `generic` is set for the forms
// whose entries carry their own types: a general list (declared_type "list",
// or mixed/boxed entries) as opposed to a homogeneous stringlist, and a generic
// dictionary (declared_type "dictionary", boxed values) as opposed to a
// string/object dictionary of plain string/object-name values.
const char *collection_type(const Value &v, bool &generic) {
    using T = Value::Type;
    generic = false;
    switch (v.type) {
    case T::StringList:
        generic = v.declared_type == "list" || list_needs_generic(v);
        return generic ? "list" : "stringlist";
    case T::StringDict:
    case T::ObjectDict:
        generic = v.declared_type == "dictionary" || dict_needs_generic(v);
        return generic ? "dictionary"
               : v.type == T::ObjectDict ? "objectdictionary"
                                         : "stringdictionary";
    default:
        return "scriptdictionary";
    }
}

// The entries of a nested collection, between its open and close tags. `depth`
// is the nesting level those entries sit at.
void write_entries(SaveWriter &w, Interp &in, const Value &v, bool generic,
                   int depth) {
    using T = Value::Type;
    if (v.type == T::StringList) {
        for (const Value &e : v.list()) {
            if (generic)
                write_value(w, in, e, depth);  // typed, recursive
            else
                w.leaf("value", {}, e.str);    // homogeneous stringlist
        }
    } else if (v.type == T::ScriptDict) {
        // Scriptdict entries are compiled scripts like any Script field: bake
        // their run firsttimes or they re-fire after a reload.
        for (const auto &kv : v.dict())
            w.leaf("item", {{"key", kv.first}},
                   in.bake_firsttime_source(kv.second.str));
    } else {
        for (const auto &kv : v.dict()) {
            w.open("item", {});
            w.leaf("key", {}, kv.first);
            if (generic) {
                write_value(w, in, kv.second, depth);  // typed, recursive
            } else {
                std::string vs = dict_value_string(kv.second);
                w.leaf("value", {}, vs, !vs.empty());
            }
            w.close("item");
        }
    }
}

// Serialize one field value onto `w` under attribute `attr` of the element
// named `elem_name`. A QuestViva saved game emits NO <implied> declarations, so
// its own reload resolves every type-less attribute to string (or boolean when
// empty). It exploits this by writing STRING fields type-less and everything
// else -- ints, booleans, objects, scripts, delegate impls, collections -- with
// an explicit type. We do the same: it is what makes an already-converted
// command <pattern> reload as raw regex instead of being re-run through the
// simplepattern loader (which would mangle it). An EMPTY string keeps
// type="string" -- without it a type-less empty tag reloads as boolean.
void write_field(SaveWriter &w, Interp &in, const std::string &elem_name,
                 const std::string &attr, const Value &v) {
    using T = Value::Type;
    // Scalars and the joined-string containers (objectlist, and the legacy
    // v530- list/dictionary forms) go through WriteAttribute's tag rule.
    bool simple = simple_attr_name(attr);
    std::string tag = simple ? attr : "attr";
    Attrs base;
    if (!simple) base.emplace_back("name", attr);
    auto typed = [&](const std::string &type, const std::string &text) {
        Attrs a = base;
        a.emplace_back("type", type);
        w.leaf(tag, a, text);
    };
    // A v540+ nested container uses the attribute name as the tag directly.
    auto nested = [&](const char *type, bool generic) {
        bool xml_tag = xml_tag_ok(attr);
        std::string ctag = xml_tag ? attr : "attr";
        Attrs a;
        if (!xml_tag) a.emplace_back("name", attr);
        a.emplace_back("type", type);
        w.open(ctag, a);
        write_entries(w, in, v, generic, 0);
        w.close(ctag);
    };

    bool generic = false;
    switch (v.type) {
    case T::Null:
        return;  // a removed/never-set attribute is simply absent
    case T::String:
        if (v.str.empty()) typed("string", v.str);
        else w.leaf(tag, base, v.str);
        return;
    case T::Script:
        // Own script attribute: bake the flags of THIS element's instance
        // (the firsttime scope its runs were cached under).
        typed(script_type(v),
              in.bake_firsttime_source(v.str, Interp::scope_key(elem_name, attr)));
        return;
    case T::Int:
        typed("int", std::to_string(v.integer));
        return;
    case T::Double:
        typed("double", double_to_str(v.dbl));
        return;
    case T::Boolean:
        // QuestViva's BooleanSaver: a TRUE flag with a simple name is a bare
        // <attr/> (no type, no text) -- which reloads as boolean true; a spaced
        // name or a FALSE value writes the value with its type.
        if (v.boolean && simple) w.leaf(tag, base, "", false);
        else typed("boolean", v.boolean ? "true" : "false");
        return;
    case T::ObjectRef:
        typed("object", v.str);
        return;
    case T::StringList: {
        const char *type = collection_type(v, generic);
        // Legacy homogeneous stringlist: semicolon-joined, type="list".
        if (w.version <= 530 && !generic) typed("list", join_names(v, "; "));
        else nested(type, generic);
        return;
    }
    case T::ObjectList:
        // ObjectListSaver: always semicolon-joined element names, both versions.
        typed("objectlist", join_names(v, "; "));
        return;
    case T::StringDict:
    case T::ObjectDict: {
        const char *type = collection_type(v, generic);
        if (w.version <= 530 && !generic) {
            // Legacy SaveString: "k = v;k2 = v2".
            std::string s;
            const auto &d = v.dict();
            for (size_t i = 0; i < d.size(); ++i) {
                if (i) s += ";";
                s += d[i].first;
                s += " = ";
                s += dict_value_string(d[i].second);
            }
            typed(type, s);
        } else {
            nested(type, generic);
        }
        return;
    }
    case T::ScriptDict:
        nested(collection_type(v, generic), generic);
        return;
    }
}

// See the forward declaration above collection_type.
void write_value(SaveWriter &w, Interp &in, const Value &v, int depth) {
    using T = Value::Type;
    auto typed = [&](const std::string &type, const std::string &text) {
        w.leaf("value", {{"type", type}}, text);
    };
    if (depth > 100) {  // self-referential collection: truncate to null
        w.leaf("value", {}, "");
        return;
    }
    switch (v.type) {
    case T::Null:
        w.leaf("value", {}, "");
        return;
    case T::String:
        typed("string", v.str);
        return;
    case T::Script:
        typed(script_type(v), in.bake_firsttime_source(v.str));
        return;
    case T::Int:
        typed("int", std::to_string(v.integer));
        return;
    case T::Double:
        typed("double", double_to_str(v.dbl));
        return;
    case T::Boolean:
        if (v.boolean) w.leaf("value", {}, "", false);  // bare <value/>
        else typed("boolean", "false");
        return;
    case T::ObjectRef:
        typed("object", v.str);
        return;
    case T::ObjectList:
        typed("objectlist", join_names(v, "; "));
        return;
    case T::StringList:
    case T::StringDict:
    case T::ObjectDict:
    case T::ScriptDict: {
        bool generic = false;
        const char *type = collection_type(v, generic);
        w.open("value", {{"type", type}});
        write_entries(w, in, v, generic, depth + 1);
        w.close("value");
        return;
    }
    }
}

// Common inherit + own-field body of an element.
void write_fields(SaveWriter &w, Interp &in, const Element *e) {
    for (const std::string &t : e->inherits)
        if (!is_default_type(t)) w.leaf("inherit", {{"name", t}}, "", false);
    for (const auto &kv : e->fields)
        if (!ignore_field(e->elem_type, kv.first))
            write_field(w, in, e->name, kv.first, kv.second);
}

}  // namespace

// Return `src` with already-run firsttime blocks baked out, or unchanged when
// none has run (the common case: ordinary scripts stay verbatim). See the
// firsttime-baking block above for the transform.
//
// Only a body that has RUN can hold a set flag, and running caches it, so a
// source with no cache entry (under its scope, or unscoped -- the `invoke`
// path and scriptdictionary items) is returned verbatim without compiling.
std::string Interp::bake_firsttime_source(const std::string &src,
                                          const std::string &scope) {
    std::shared_ptr<std::vector<Stmt>> body;
    if (!scope.empty()) {
        auto it = script_cache_.find(script_cache_key(scope, src));
        if (it != script_cache_.end()) body = it->second;
    }
    if (!body) {
        auto it = script_cache_.find(src);
        if (it == script_cache_.end()) return src;
        body = it->second;
    }
    std::vector<std::shared_ptr<bool>> flags;
    collect_firsttime(*body, flags);
    if (!any_firsttime_ran(flags)) return src;
    return bake_stmt_list(src, *body);
}

// ---- writer ---------------------------------------------------------------

std::string Interp::save_game_native(const std::string &original_file) {
    using T = Value::Type;
    SaveWriter w;
    w.version = world_.asl_version;

    w.out += "<!-- Saved by Question (native Quest 5 engine) -->\n";
    Attrs asl_attrs;
    asl_attrs.emplace_back(
        "version", world_.version_string.empty() ? std::to_string(world_.asl_version)
                                                  : world_.version_string);
    if (!original_file.empty())
        asl_attrs.emplace_back("original", original_file);
    w.open("asl", asl_attrs);

    auto by_sort = [](const Element *a, const Element *b) {
        return a->sort_index < b->sort_index;
    };
    // The live elements `want` picks, in sort_index order.
    auto live_sorted = [&](auto want) {
        std::vector<const Element *> v;
        for (const auto &up : world_.elements)
            if (up->registered && want(up.get())) v.push_back(up.get());
        std::sort(v.begin(), v.end(), by_sort);
        return v;
    };
    auto of_kind = [](ElemKind k) {
        return [k](const Element *e) { return e->kind == k; };
    };
    // The text of a field holding a value of type `t`, else "".
    auto text_of = [](const Element *e, const char *attr, T t) {
        const Value *v = e->field(attr);
        return v && v->type == t ? v->str : std::string();
    };

    // Runtime containment is the `parent` field (Element::children is the
    // load-time graph and goes stale on MoveObject). Build a live parent->
    // children map over the registered object family, ordered by sort_index.
    std::map<std::string, std::vector<const Element *>> children;
    std::vector<const Element *> roots;
    for (const auto &up : world_.elements) {
        const Element *e = up.get();
        if (!nested_family(e->elem_type) || !e->registered) continue;
        std::string pn = text_of(e, "parent", T::ObjectRef);
        if (!pn.empty() && world_.find(pn)) children[pn].push_back(e);
        else roots.push_back(e);
    }
    std::sort(roots.begin(), roots.end(), by_sort);
    for (auto &kv : children) std::sort(kv.second.begin(), kv.second.end(), by_sort);

    // Element tag + structural attributes (name / game name / exit alias+to).
    // The seen-set guards against a runtime `parent` cycle, which would
    // otherwise recurse forever through the children map.
    std::set<const Element *> emitted;
    std::function<void(const Element *)> emit = [&](const Element *e) {
        if (!emitted.insert(e).second) return;
        // object/game/command/verb/exit/turnscript
        const std::string &tag = e->elem_type;
        Attrs attrs;
        if (tag == "game") {
            const Value *gn = e->field("gamename");
            attrs.emplace_back("name", gn && gn->type == T::String
                                               ? gn->str : world_.game_name);
        } else {
            attrs.emplace_back("name", e->name);
        }
        if (tag == "exit") {
            std::string alias = text_of(e, "alias", T::String);
            if (!alias.empty()) attrs.emplace_back("alias", alias);
            std::string to = text_of(e, "to", T::ObjectRef);
            if (!to.empty()) attrs.emplace_back("to", to);
        }
        w.open(tag, attrs);
        write_fields(w, *this, e);
        auto it = children.find(e->name);
        if (it != children.end())
            for (const Element *c : it->second) emit(c);
        w.close(tag);
    };
    // <tag name="..">inherits + own fields</tag> for each element of a kind
    // that saves flat.
    auto emit_flat = [&](const char *tag, ElemKind kind) {
        for (const Element *e : live_sorted(of_kind(kind))) {
            w.open(tag, {{"name", e->name}});
            write_fields(w, *this, e);
            w.close(tag);
        }
    };
    // A QuestViva saved game is a COMPLETE, self-contained world: its loader
    // loads ONLY the save (the original .quest supplies resources, not
    // elements). So every element type must be re-emitted, in the ElementType
    // enum order the reference writer uses (implied/template/dynamictemplate/
    // delegate BEFORE objects so type-omitted implied attributes and
    // delegate-typed fields resolve; type/function AFTER, resolved lazily).
    // Our own overlay reader tolerates a partial save (it reloads the original
    // first), but Quest/QuestViva need the whole thing.
    auto join_params = [](const Element *e) {
        const Value *p = e->field("paramnames");
        return p && is_list(*p) ? join_names(*p, ", ") : std::string();
    };

    // NOTE: we deliberately emit NO <implied> declarations, exactly like
    // QuestViva's SavedGame mode. With no implied context on reload, every
    // type-less attribute resolves to string/boolean, which is precisely why
    // already-final values (command patterns as raw regex, etc.) survive a
    // round-trip instead of being re-interpreted.

    // <template>/<dynamictemplate>: scripts are stored post-substitution, so
    // static [refs] are already gone, but the runtime Template()/
    // DynamicTemplate() builtins still look these up by name.
    for (const auto &kv : world_.templates)
        w.leaf("template", {{"name", kv.first}}, kv.second);
    for (const auto &kv : world_.dynamic_templates)
        w.leaf("dynamictemplate", {{"name", kv.first}}, kv.second);
    // <delegate name=".." parameters=".." type="returntype"/>
    for (const auto &up : world_.elements) {
        const Element *e = up.get();
        if (e->kind != ElemKind::Delegate || !e->registered) continue;
        w.leaf("delegate",
               {{"name", e->name},
                {"parameters", join_params(e)},
                {"type", text_of(e, "returntype", T::String)}},
               "", false);
    }

    for (const Element *e : roots) emit(e);

    // Elements in a runtime `parent` cycle sit only in children[], so the
    // walk from roots never reaches them. Emit them as extra roots: the
    // cycle flattens (their parent comes from XML nesting on reload) instead
    // of silently vanishing from the save.
    for (const Element *e : live_sorted([&](const Element *x) {
             return nested_family(x->elem_type) && !emitted.count(x);
         }))
        emit(e);

    emit_flat("type", ElemKind::Type);
    // <function name=".." parameters=".." type="returntype">script</function>
    for (const Element *e : live_sorted(of_kind(ElemKind::Function))) {
        Attrs a{{"name", e->name}};
        std::string params = join_params(e);
        if (!params.empty()) a.emplace_back("parameters", params);
        std::string returns = text_of(e, "returntype", T::String);
        if (!returns.empty()) a.emplace_back("type", returns);
        const Value *sc = e->field("script");
        std::string body;
        if (sc && sc->type == T::Script)
            body = bake_firsttime_source(sc->str, scope_key(e->name, "script"));
        w.leaf("function", a, body, !body.empty());
    }
    // Timers save flat (TimerSaver), after the object graph.
    emit_flat("timer", ElemKind::Timer);

    w.close("asl");
    return w.out;
}

// ---- reader ---------------------------------------------------------------

bool Interp::is_native_save_data(const char *data, size_t len) {
    // Skip a UTF-8 BOM and leading whitespace, then require the document to
    // open with an XML comment or the <asl ...> root that carries `original`.
    const char *p = data, *end = data + len;
    if (len >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB &&
        (unsigned char)p[2] == 0xBF)
        p += 3;
    while (p < end && std::isspace((unsigned char)*p)) ++p;
    std::string head(p, (size_t)std::min<ptrdiff_t>(end - p, 512));
    if (head.rfind("<!--", 0) == 0 &&
        (head.find("Saved by Question") != std::string::npos ||
         head.find("Saved by Geas") != std::string::npos))   // pre-rename marker
        return true;
    // Otherwise: <asl ... original="..."> (a Quest/QuestViva saved game). A
    // plain game .aslx has <asl> with no `original`, so this stays specific.
    size_t asl = head.find("<asl");
    if (asl == std::string::npos) return false;
    size_t gt = head.find('>', asl);
    std::string tag =
        head.substr(asl, gt == std::string::npos ? gt : gt - asl);
    return tag.find("original") != std::string::npos;
}

bool Interp::restore_game_native(const std::string &data, std::string &err) {
    // Validate the ASL version matches this (already-reloaded) world.
    {
        size_t off = data.find("<asl");
        if (off == std::string::npos) { err = "not an ASLX save"; return false; }
        size_t gt = data.find('>', off);
        std::string tag =
            data.substr(off, gt == std::string::npos ? gt : gt - off);
        size_t vp = tag.find("version=\"");
        if (vp != std::string::npos) {
            size_t vs = vp + 9, ve = tag.find('"', vs);
            std::string ver = tag.substr(vs, ve - vs);
            if (!ver.empty() && std::atoi(ver.c_str()) != world_.asl_version) {
                err = "this save is for a different Quest version";
                return false;
            }
        }
    }

    std::set<std::string> overlaid;
    // Overlay onto world_ (the freshly reloaded original). Note: overlay_aslx
    // pushes its own <asl version> onto world_, which equals the current one.
    // A saved game carries no <include>, so the core dir is irrelevant here.
    if (!overlay_aslx_buffer(data.data(), data.size(), world_, overlaid, "")) {
        err = "the save could not be parsed";
        return false;
    }
    if (overlaid.empty()) {
        err = "the save defined no elements";
        return false;
    }

    // Drop every live mutable-family element the save did not (re)define: it was
    // destroyed before the save was written (Quest's saved game lists all
    // surviving objects; anything absent no longer exists).
    drop_unsaved_elements(world_, overlaid);

    // Merged-listextend slots may hold pre-restore state (as in restore_game).
    extend_cache_.clear();
    return true;
}

}  // namespace aslx
