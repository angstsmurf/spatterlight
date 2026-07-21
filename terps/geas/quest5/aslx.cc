// aslx.cc -- Quest 5 loader implementation. See aslx.hh.

#include "aslx.hh"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <sstream>
#include <stdexcept>

#include <expat.h>
#include <zlib.h>

namespace aslx {

// ---------------------------------------------------------------------------
// Value / Element / World helpers
// ---------------------------------------------------------------------------

std::string Value::debug_string() const {
    std::ostringstream o;
    switch (type) {
    case Type::Null: o << "null"; break;
    case Type::String: o << '"' << str << '"'; break;
    case Type::Int: o << integer; break;
    case Type::Double: o << dbl; break;
    case Type::Boolean: o << (boolean ? "true" : "false"); break;
    case Type::Script: {
        // Scripts can be long/multiline; show a compact one-line preview.
        std::string s = str;
        for (char &c : s) if (c == '\n' || c == '\t') c = ' ';
        // collapse runs of spaces
        std::string t;
        bool sp = false;
        for (char c : s) {
            if (c == ' ') { if (!sp) t += ' '; sp = true; }
            else { t += c; sp = false; }
        }
        while (!t.empty() && t.front() == ' ') t.erase(t.begin());
        while (!t.empty() && t.back() == ' ') t.pop_back();
        if (t.size() > 60) t = t.substr(0, 57) + "...";
        o << "script{ " << t << " }";
        break;
    }
    case Type::StringList:
    case Type::ObjectList: {
        o << (type == Type::ObjectList ? "objectlist" : "list");
        if (list_extend) o << "+";   // extends the inherited list
        o << '[';
        for (size_t i = 0; i < list().size(); ++i) {
            if (i) o << ", ";
            // String/ObjectRef entries (the common case) print bare, keeping
            // dumps stable; anything else shows its typed form.
            const Value &e = list()[i];
            if (e.type == Type::String || e.type == Type::ObjectRef) o << e.str;
            else o << e.debug_string();
        }
        o << ']';
        break;
    }
    case Type::ObjectRef: o << "->" << str; break;
    case Type::StringDict:
    case Type::ObjectDict:
    case Type::ScriptDict: {
        o << "dict{";
        for (size_t i = 0; i < dict().size(); ++i) {
            if (i) o << ", ";
            o << dict()[i].first << "=" << dict()[i].second.debug_string();
        }
        o << '}';
        break;
    }
    }
    return o.str();
}

// Wrap a raw dictionary-entry string as a typed Value according to the
// dictionary's declared kind (ObjectDict -> object ref, ScriptDict -> script,
// otherwise a plain string). Dictionary values are typed per-entry now, so the
// loader materialises each one here.
static Value dict_entry_value(const std::string &s, Value::Type dict_type) {
    Value v;
    if (dict_type == Value::Type::ObjectDict) {
        v.type = Value::Type::ObjectRef;
        v.str = s;
    } else if (dict_type == Value::Type::ScriptDict) {
        v.type = Value::Type::Script;
        v.str = s;
    } else {
        v.type = Value::Type::String;
        v.str = s;
    }
    return v;
}

// Wrap a raw list-entry string as a typed Value: an ObjectList holds object
// refs, everything else plain strings. List entries are typed per-entry (like
// dictionary values); the loader materialises each one here.
static Value list_entry_value(const std::string &s, Value::Type list_type) {
    Value v;
    v.type = (list_type == Value::Type::ObjectList) ? Value::Type::ObjectRef
                                                    : Value::Type::String;
    v.str = s;
    return v;
}

static void fill_list(Value &v, const std::vector<std::string> &items) {
    for (const std::string &s : items)
        v.list().push_back(list_entry_value(s, v.type));
}

const Value *Element::field(const std::string &n) const {
    auto it = field_index_.find(n);
    return it == field_index_.end() ? nullptr : &fields[it->second].second;
}

Value &Element::set_field(const std::string &n, Value v) {
    auto it = field_index_.find(n);
    if (it != field_index_.end()) {
        fields[it->second].second = std::move(v);
        return fields[it->second].second;
    }
    field_index_.emplace(n, fields.size());
    fields.emplace_back(n, std::move(v));
    return fields.back().second;
}

void Element::remove_field(const std::string &n) {
    auto it = field_index_.find(n);
    if (it == field_index_.end()) return;
    size_t idx = it->second;
    fields.erase(fields.begin() + idx);
    field_index_.erase(it);
    // Erasing from the middle shifts every later slot down by one.
    for (auto &e : field_index_)
        if (e.second > idx) --e.second;
}

void Element::clear_all_fields() {
    fields.clear();
    field_index_.clear();
}

ElemKind elem_kind_from_string(const std::string &s) {
    // Small fixed set; a first-char switch keeps this off the memcmp path for
    // the common kinds without a map.
    switch (s.empty() ? '\0' : s[0]) {
    case 'o': if (s == "object") return ElemKind::Object;
              if (s == "output") return ElemKind::Output; break;
    case 'c': if (s == "command") return ElemKind::Command; break;
    case 'v': if (s == "verb") return ElemKind::Verb; break;
    case 'f': if (s == "function") return ElemKind::Function; break;
    case 'g': if (s == "game") return ElemKind::Game; break;
    case 't': if (s == "type") return ElemKind::Type;
              if (s == "turnscript") return ElemKind::Turnscript;
              if (s == "timer") return ElemKind::Timer; break;
    case 'w': if (s == "walkthrough") return ElemKind::Walkthrough; break;
    case 'e': if (s == "exit") return ElemKind::Exit; break;
    case 'r': if (s == "resource") return ElemKind::Resource; break;
    case 'j': if (s == "javascript") return ElemKind::Javascript; break;
    case 'd': if (s == "delegate") return ElemKind::Delegate; break;
    default: break;
    }
    return ElemKind::Other;
}

Element *World::find(const std::string &n) const {
    auto it = by_name.find(n);
    return it == by_name.end() ? nullptr : it->second;
}

// Case-insensitive lookup of a <delegate> element. `rundelegate`/HasDelegate...
// are invoked with the implementation FIELD name (e.g. the tag "addscript"),
// which QuestViva matches OrdinalIgnoreCase against the <delegate name="AddScript">
// declaration to recover the parameter list. Delegate names are ASCII identifiers,
// so an ASCII fold is exact. Delegates are static (never created/destroyed at
// runtime), so the index is built once and cached.
Element *World::find_delegate(const std::string &n) const {
    if (Element *e = find(n); e && e->kind == ElemKind::Delegate) return e;
    auto lc = [](std::string s) {
        for (char &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    if (!delegate_ci_built_) {
        for (const auto &up : elements)
            if (up->kind == ElemKind::Delegate)
                delegate_ci_.emplace(lc(up->name), up.get());
        delegate_ci_built_ = true;
    }
    auto it = delegate_ci_.find(lc(n));
    return it == delegate_ci_.end() ? nullptr : it->second;
}

void World::register_name(const std::string &name, Element *ep) {
    auto it = by_name.find(name);
    if (it != by_name.end()) {
        if (it->second == ep) { ep->registered = true; return; }
        it->second->registered = false;  // last-create-wins shadows the old one
        it->second = ep;
    } else {
        by_name.emplace(name, ep);
    }
    ep->registered = true;
    note_containment_change();
}

void World::unregister_name(const std::string &name) {
    auto it = by_name.find(name);
    if (it == by_name.end()) return;
    it->second->registered = false;
    by_name.erase(it);
    note_containment_change();
}

// Live object-kind children of `parent` (nullptr = roots), SortIndex order.
// Rebuilds the whole index lazily on a containment change -- mirrors the old
// direct_children scan exactly (kind==Object, live, parent read from the own
// `parent` field), just amortised across the many reads in a turn.
const std::vector<Element *> &World::children_of(Element *parent) {
    if (children_index_gen_ != containment_gen) {
        children_index_.clear();
        for (const auto &up : elements) {
            Element *x = up.get();
            if (x->kind != ElemKind::Object || !x->registered)
                continue;
            const Value *p = x->field("parent");
            Element *par = (p && p->type == Value::Type::ObjectRef)
                               ? find(p->str)
                               : nullptr;
            children_index_[par].push_back(x);
        }
        for (auto &kv : children_index_)
            std::stable_sort(kv.second.begin(), kv.second.end(),
                             [](const Element *a, const Element *b) {
                                 return a->sort_index < b->sort_index;
                             });
        children_index_gen_ = containment_gen;
    }
    static const std::vector<Element *> empty;
    auto it = children_index_.find(parent);
    return it == children_index_.end() ? empty : it->second;
}

const std::string *World::implied_type(const std::string &elem_type,
                                       const std::string &property) const {
    auto it = implied_types.find(elem_type + "~" + property);
    return it == implied_types.end() ? nullptr : &it->second;
}

// Expose the element's kind as fields, the way QuestViva does on Element
// construction: `elementtype` is the ElementType string, and -- for the
// "object" family only (object/exit/command/verb/game/turnscript are all
// ElementType.Object with an ObjectType subtype) -- `type` is the ObjectType
// string. Core branches on both (AddToResolvedNames checks obj.type =
// "object" to build game.lastobjects, which is what makes "it" resolve).
static void set_kind_fields(Element *ep) {
    static const struct { const char *et; const char *elementtype; const char *type; }
        kMap[] = {
            {"object", "object", "object"},
            {"exit", "object", "exit"},
            {"command", "object", "command"},
            {"verb", "object", "command"},
            {"game", "object", "game"},
            {"turnscript", "object", "turnscript"},
        };
    Value ev2;
    ev2.type = Value::Type::String;
    ev2.str = ep->elem_type;
    for (const auto &m : kMap) {
        if (ep->elem_type == m.et) {
            ev2.str = m.elementtype;
            Value tv;
            tv.type = Value::Type::String;
            tv.str = m.type;
            ep->set_field("type", tv);
            break;
        }
    }
    ep->set_field("elementtype", ev2);
}

Element *World::create_object(const std::string &name, const std::string &type,
                              const std::string &elem_type) {
    auto e = std::make_unique<Element>();
    Element *ep = e.get();
    ep->elem_type = elem_type;
    ep->kind = elem_kind_from_string(elem_type);
    ep->name = name;
    ep->sort_index = next_sort_index++;
    set_kind_fields(ep);
    if (!name.empty()) {
        // Same `name` field every loaded element gets (FieldDefinitions.Name)
        // -- a runtime-created timer reads this.name to destroy itself.
        Value nv;
        nv.type = Value::Type::String;
        nv.str = name;
        ep->set_field("name", nv);
    }
    // Every runtime-created element inherits its implicit default type (lowest
    // priority), then the caller's explicit type -- QuestViva
    // ObjectFactory.CreateObject inserts the default type at the front of
    // initialTypes. Same mapping as apply_default_types; timers have no default
    // type. resolve_field skips a default the game never defines -- harmless.
    if (elem_type == "object") ep->inherits.push_back("defaultobject");
    else if (elem_type == "turnscript") ep->inherits.push_back("defaultturnscript");
    else if (elem_type == "exit") ep->inherits.push_back("defaultexit");
    if (!type.empty()) ep->inherits.push_back(type);
    roots.push_back(ep);
    register_name(name, ep);       // last create wins, like ObjectFactory
    note_containment_change();
    elements.push_back(std::move(e));
    return ep;
}

void World::destroy_element(const std::string &name) {
    unregister_name(name);
    // The Element storage stays in `elements` (other references may exist); it
    // simply becomes unreachable by name, which is what scope walks rely on.
}

// ---------------------------------------------------------------------------
// Small utilities matching QuestViva's Utility.cs
// ---------------------------------------------------------------------------

static std::string trim(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

// Utility.ListSplit: split on "; " then ";" -- i.e. semicolons, with an
// optional following space collapsed. StringSplitOptions.None keeps empties.
static std::vector<std::string> list_split(const std::string &value) {
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == ';') {
            out.push_back(cur);
            cur.clear();
            if (i + 1 < value.size() && value[i + 1] == ' ') ++i;  // "; "
        } else {
            cur += value[i];
        }
    }
    out.push_back(cur);
    return out;
}

// Utility.SplitIntoLines: split on newlines, trim, drop empties.
static std::vector<std::string> split_into_lines(const std::string &text) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : text) {
        if (c == '\n') {
            std::string t = trim(cur);
            if (!t.empty()) out.push_back(t);
            cur.clear();
        } else if (c != '\r') {
            cur += c;
        }
    }
    std::string t = trim(cur);
    if (!t.empty()) out.push_back(t);
    return out;
}

// SimpleStringListLoader.GetValues: newline form vs semicolon form.
static std::vector<std::string> simple_list_values(const std::string &value) {
    if (value.find('\n') != std::string::npos) return split_into_lines(value);
    return list_split(value);
}

// SimplePatternLoader.LoadCommand: turn a "take #object#; get #object#" simple
// pattern into an anchored .NET regex "^take (?<object>.*)$|^get (?<object>.*)$".
static std::string convert_simple_pattern(const std::string &pattern) {
    // Escape regex metacharacters that Quest escapes: ( ) . ?
    std::string v;
    for (char c : pattern) {
        if (c == '(' || c == ')' || c == '.' || c == '?') v += '\\';
        v += c;
    }
    // Replace #name# (name = letter followed by 1+ word chars) with a named group.
    std::string out;
    for (size_t i = 0; i < v.size();) {
        if (v[i] == '#') {
            size_t j = i + 1;
            if (j < v.size() && (std::isalpha((unsigned char)v[j]))) {
                size_t k = j + 1;
                while (k < v.size() &&
                       (std::isalnum((unsigned char)v[k]) || v[k] == '_'))
                    ++k;
                if (k > j + 1 && k < v.size() && v[k] == '#') {
                    out += "(?<" + v.substr(j, k - j) + ">.*)";
                    i = k + 1;
                    continue;
                }
            }
        }
        out += v[i++];
    }
    // Split on ';' and wrap each alternative with ^...$.
    std::string result;
    for (const std::string &p : list_split(out)) {
        if (!result.empty()) result += "|";
        result += "^" + p + "$";
    }
    return result;
}

// Escape the regex metacharacters .NET's Regex.Escape escapes that matter for
// matching (whitespace is left literal -- it matches the same either way).
static std::string regex_escape(const std::string &s) {
    static const std::string meta = "\\.^$|?*+()[]{}";
    std::string out;
    for (char c : s) {
        if (meta.find(c) != std::string::npos) out += '\\';
        out += c;
    }
    return out;
}

// Utility.ConvertVerbSimplePattern: turn a verbtemplate list
// "look at; look; x" into "^look at (?<object>.*?)$|^look (?<object>.*?)$|...".
// A verb containing "#object#" places the object mid-pattern
// ("switch #object# on" -> "^switch (?<object>.*?) on$"). Lazy quantifier,
// unlike SimplePatternLoader's greedy one (matches QuestViva). `separator`, if
// set, appends the "( SEP (?<object2>.*))?" tail for multi-object verbs.
static std::string convert_verb_simple_pattern(const std::string &pattern,
                                               const std::string &separator) {
    std::string sep_regex;
    if (!separator.empty()) {
        sep_regex = "(";
        bool first = true;
        for (const std::string &s : list_split(separator)) {
            if (!first) sep_regex += "|";
            sep_regex += regex_escape(trim(s));
            first = false;
        }
        sep_regex += ")";
    }
    static const std::string object_regex = "(?<object>.*?)";
    std::string result;
    for (const std::string &verb : list_split(pattern)) {
        if (!result.empty()) result += "|";
        std::string add;
        size_t hash = verb.find("#object#");
        if (hash != std::string::npos) {
            add = "^";
            size_t pos = 0;
            bool first = true;
            while (true) {
                size_t h = verb.find("#object#", pos);
                std::string seg = verb.substr(pos, h == std::string::npos
                                                        ? std::string::npos
                                                        : h - pos);
                if (!first) add += object_regex;
                add += regex_escape(seg);
                first = false;
                if (h == std::string::npos) break;
                pos = h + 8;  // strlen("#object#")
            }
        } else {
            add = "^" + regex_escape(verb) + " " + object_regex;
        }
        if (!sep_regex.empty())
            add += "( " + sep_regex + " (?<object2>.*))?";
        add += "$";
        result += add;
    }
    return result;
}

static bool starts_with(const std::string &s, const char *p) {
    return s.compare(0, std::strlen(p), p) == 0;
}

// ---------------------------------------------------------------------------
// Minimal in-memory zip reader for .quest packages
// ---------------------------------------------------------------------------

static uint16_t rd16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool inflate_raw(const uint8_t *src, size_t srclen, size_t rawlen,
                        std::string &out) {
    out.clear();
    // rawlen is the entry's own declared size; cap it before allocating (the
    // largest real .quest payloads are tens of MB).
    if (rawlen > ((size_t)1 << 29)) return false;
    out.resize(rawlen);
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) return false;
    zs.next_in = const_cast<Bytef *>(src);
    zs.avail_in = (uInt)srclen;
    zs.next_out = reinterpret_cast<Bytef *>(&out[0]);
    zs.avail_out = (uInt)rawlen;
    int r = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    return r == Z_STREAM_END || (r == Z_OK && zs.avail_out == 0);
}

bool zip_list_entries(const uint8_t *data, size_t len,
                      std::vector<ZipEntryInfo> &out) {
    out.clear();
    if (len < 22 || !(data[0] == 'P' && data[1] == 'K' && data[2] == 3 &&
                      data[3] == 4))
        return false;

    // Find End Of Central Directory record (signature PK\05\06), scanning back.
    const uint8_t *eocd = nullptr;
    size_t maxback = len < 65557 ? len : 65557;
    for (size_t i = len - 22; i + 22 <= len && i + maxback + 22 >= len; --i) {
        if (data[i] == 'P' && data[i + 1] == 'K' && data[i + 2] == 5 &&
            data[i + 3] == 6) {
            eocd = data + i;
            break;
        }
        if (i == 0) break;
    }
    if (!eocd) return false;

    uint16_t count = rd16(eocd + 10);
    uint32_t cd_off = rd32(eocd + 16);
    if (cd_off > len) return false;

    // Offsets stay index-based so file-supplied uint32s are range-checked
    // before any pointer is formed from them.
    size_t pos = cd_off;
    for (uint16_t e = 0; e < count && pos + 46 <= len; ++e) {
        const uint8_t *p = data + pos;
        if (!(p[0] == 'P' && p[1] == 'K' && p[2] == 1 && p[3] == 2)) break;
        ZipEntryInfo info;
        info.method = rd16(p + 10);
        info.comp_size = rd32(p + 20);
        info.raw_size = rd32(p + 24);
        uint16_t name_len = rd16(p + 28);
        uint16_t extra_len = rd16(p + 30);
        uint16_t comment_len = rd16(p + 32);
        uint32_t local_off = rd32(p + 42);
        if (pos + 46 + name_len > len) break;
        info.name.assign((const char *)p + 46, name_len);
        pos += 46 + (size_t)name_len + extra_len + comment_len;

        if (!info.name.empty() && info.name.back() == '/')  // directory
            continue;

        // The local header repeats name/extra with possibly DIFFERENT extra
        // length, so the payload offset must come from it, not the central
        // directory.
        if ((size_t)local_off + 30 > len) continue;
        const uint8_t *lh = data + local_off;
        if (!(lh[0] == 'P' && lh[1] == 'K' && lh[2] == 3 && lh[3] == 4))
            continue;
        uint16_t l_name = rd16(lh + 26);
        uint16_t l_extra = rd16(lh + 28);
        info.offset = (size_t)(lh - data) + 30 + l_name + l_extra;
        if (info.offset + info.comp_size > len) continue;

        out.push_back(std::move(info));
    }
    return true;
}

bool zip_extract_entry(const uint8_t *data, size_t len, const ZipEntryInfo &e,
                       std::string &out) {
    out.clear();
    if (e.offset + e.comp_size > len) return false;
    const uint8_t *src = data + e.offset;
    if (e.method == 0) {  // stored
        // Only comp_size was bounds-checked above; a stored entry's sizes
        // must agree, so copy comp_size rather than trusting raw_size.
        out.assign((const char *)src, e.comp_size);
        return true;
    }
    if (e.method == 8)  // deflate
        return inflate_raw(src, e.comp_size, e.raw_size, out);
    return false;
}

const ZipEntryInfo *zip_find_entry(const std::vector<ZipEntryInfo> &entries,
                                   const std::string &name) {
    for (const ZipEntryInfo &e : entries)
        if (e.name == name) return &e;
    for (const ZipEntryInfo &e : entries) {
        if (e.name.size() != name.size()) continue;
        bool match = true;
        for (size_t i = 0; i < name.size(); i++)
            if (std::tolower((unsigned char)e.name[i]) !=
                std::tolower((unsigned char)name[i])) {
                match = false;
                break;
            }
        if (match) return &e;
    }
    return nullptr;
}

bool extract_game_aslx(const uint8_t *data, size_t len, std::string &out) {
    std::vector<ZipEntryInfo> entries;
    if (!zip_list_entries(data, len, entries)) return false;
    // Exact name only, like QuestViva's zip.GetEntry("game.aslx").
    for (const ZipEntryInfo &e : entries)
        if (e.name == "game.aslx") return zip_extract_entry(data, len, e, out);
    return false;
}

// ---------------------------------------------------------------------------
// XML -> element tree (expat SAX, frame stack)
// ---------------------------------------------------------------------------

// Tags that create a container element and can hold nested fields/children.
static bool is_container_tag(const std::string &t) {
    return t == "object" || t == "command" || t == "verb" || t == "game" ||
           t == "exit" || t == "turnscript" || t == "type" || t == "timer" ||
           t == "walkthrough" || t == "resource" || t == "output";
}

// Leaf tags handled specially (not a container, not a field).
static bool is_leaf_tag(const std::string &t) {
    return t == "function" || t == "delegate" || t == "template" ||
           t == "verbtemplate" || t == "dynamictemplate" || t == "javascript" ||
           t == "inherit" || t == "implied" || t == "include" ||
           t == "library" || t == "asl";
}

// Editor-only tags: skipped entirely in play mode.
static bool is_editor_tag(const std::string &t) {
    return t == "editor" || t == "tab" || t == "control";
}

struct Frame {
    enum class Kind {
        Element,
        Field,
        Value,
        Item,
        ItemKey,
        ItemValue,
        Leaf,
        Ignore
    };
    Kind kind;
    std::string tag;
    Element *elem = nullptr;   // Element
    std::string attr_name;     // Field
    std::string type;          // Field / Value type= attribute
    std::string text;          // accumulated char data
    // Nested children as fully-built Values, so collections nest arbitrarily
    // (a dictionary whose values are dictionaries -- Quest's grid-coordinate
    // save state). A Field / Value / ItemValue frame collects <value> children
    // here; <item> children go into `items`.
    std::vector<Value> values;                 // <value> children (built, typed)
    std::vector<std::string> value_texts;      // <value> children raw text
    struct ItemData {
        std::string key;
        bool has_val = false;   // saw a nested <value> (vs scriptdict item text)
        Value val;              // the nested <value>'s built Value (generic dict)
        std::string val_text;   // the nested <value>'s raw text (string/object dict)
        std::string text;       // item's own text (scriptdictionary body)
    };
    std::vector<ItemData> items;               // <item> children (dict)
    std::string item_key;      // Item scratch
    bool item_has_key = false;
    bool item_has_value = false;
    Value item_val;            // Item scratch: its <value>'s built Value
    std::string item_val_text; // Item scratch: its <value>'s raw text
    std::map<std::string, std::string> attrs;  // start-tag attributes
    int ignore_depth = 0;      // Ignore
};

struct Loader;
// Run one .aslx/.library document (a fresh XML document) through `ld`, appending
// its elements/templates to the shared World. Used for both the top-level file
// and every resolved <include>. Defined below the expat trampolines.
static bool parse_buffer_into(Loader &ld, const char *data, size_t len);
// Read a whole file into `out`; false if it cannot be opened.
static bool slurp_file(const std::string &path, std::string &out);
static std::string ascii_lower(const std::string &s) {
    std::string r = s;
    for (char &c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}
static bool icontains(const std::string &hay, const char *needle) {
    return ascii_lower(hay).find(needle) != std::string::npos;
}

struct Loader {
    World &world;
    std::vector<Frame> frames;
    std::vector<Element *> containers;  // current container chain
    int auto_id = 0;

    // Library-include resolution. Search order mirrors QuestViva's
    // WorldModel.GetLibraryStream(): game-adjacent first, then the bundled
    // Core dir, then Core/Languages. `included` dedups (a library pulled in
    // twice would otherwise redefine element names) and guards cycles.
    std::string core_dir;                // bundled Core resource dir, or empty
    std::string game_dir;                // dir the game file lives in, or empty
    std::set<std::string> included;      // lowercased basenames already pulled
    int include_depth = 0;

    // <verb> elements whose pattern (from pattern= or template=) is a verb
    // simplepattern awaiting ConvertVerbSimplePattern. Deferred to finish_load
    // because the separator comes from the defaultverb type ("with; using"),
    // which may not be parsed yet (QuestViva's LazyFields.AddAction).
    std::vector<std::pair<Element *, std::string>> pending_verb_patterns;

    // Save-overlay mode (native .quest-save restore). When set, a container
    // whose name already exists DISPLACES the live element (QuestViva's
    // Elements.Add override: a same-named element of the same family replaces
    // the old, which drops out of every enumeration via the find()==this
    // filter). `overlaid_names`, when non-null, records every name the overlay
    // (re)defined so the caller can drop live elements the save omitted.
    bool override_existing = false;
    std::set<std::string> *overlaid_names = nullptr;

    explicit Loader(World &w) : world(w) {}

    Element *current() { return containers.empty() ? nullptr : containers.back(); }

    void error(const std::string &e) { world.errors.push_back(e); }

    // Read a library file by the search order in QuestViva's
    // WorldModel.GetLibraryStream(): game-adjacent, then Core/, then
    // Core/Languages/. Returns true and fills `buf` on the first hit.
    bool read_library(const std::string &ref, std::string &buf) {
        std::string dirs[] = {game_dir, core_dir,
                              core_dir.empty() ? std::string()
                                               : core_dir + "/Languages"};
        for (const std::string &d : dirs) {
            if (d.empty()) continue;
            if (slurp_file(d + "/" + ref, buf)) return true;
        }
        return false;
    }

    // Resolve and inline an <include ref="...">. The referenced library is
    // parsed into the shared World at this point in the file, so its templates
    // load before -- and are overridden by -- the including file's own
    // (Quest processes includes depth-first, in source order).
    void resolve_include(const std::string &ref) {
        // A ref is a bare library filename; refuse ".." path segments so a
        // hostile game file cannot pull arbitrary files into the loader.
        for (size_t i = 0; i != std::string::npos && i < ref.size();) {
            size_t j = ref.find_first_of("/\\", i);
            size_t seglen = (j == std::string::npos ? ref.size() : j) - i;
            if (seglen == 2 && ref[i] == '.' && ref[i + 1] == '.') {
                error("illegal include path: " + ref);
                return;
            }
            i = (j == std::string::npos) ? std::string::npos : j + 1;
        }
        std::string key = ascii_lower(ref);
        if (!included.insert(key).second) return;   // already pulled / in cycle
        if (include_depth > 64) {
            error("include nesting too deep at: " + ref);
            return;
        }
        std::string buf;
        if (!read_library(ref, buf)) {
            // Editor-only libraries (CoreEditor*.aslx, EditorEnglish.aslx, ...)
            // are referenced by Core/language files but intentionally not
            // bundled; skipping them is expected, not an error.
            if (!icontains(ref, "editor"))
                error("include not found: " + ref);
            return;
        }
        ++include_depth;
        parse_buffer_into(*this, buf.data(), buf.size());
        --include_depth;
    }

    std::string unique_id(const std::string &prefix) {
        std::string base = prefix.empty() ? "element" : prefix;
        for (;;) {
            std::string id = base + std::to_string(++auto_id);
            if (!world.find(id)) return id;
        }
    }

    Element *new_element(const std::string &type, const std::string &name,
                         bool anonymous) {
        auto e = std::make_unique<Element>();
        Element *ep = e.get();
        ep->elem_type = type;
        ep->kind = elem_kind_from_string(type);
        ep->name = name;
        ep->anonymous = anonymous;
        ep->sort_index = world.next_sort_index++;
        set_kind_fields(ep);
        // Expose the element id as a `name` field (QuestViva's
        // FieldDefinitions.Name). Core reads obj.name / cmd.name pervasively --
        // notably as the RegexCache key in HandleSingleCommand -- so an empty
        // name would collapse every command's cache entry onto one key.
        if (!name.empty()) {
            Value nv;
            nv.type = Value::Type::String;
            nv.str = name;
            ep->set_field("name", nv);
        }
        ep->parent = current();
        if (ep->parent) {
            ep->parent->children.push_back(ep);
            // Expose containment as a `parent` ObjectRef field, the way QuestViva
            // stores it (FieldDefinitions.Parent). Core reads/writes obj.parent
            // pervasively (scope, MoveObject); the runtime derives children from
            // this field so it stays correct after a runtime move.
            if (!ep->parent->name.empty()) {
                Value pv;
                pv.type = Value::Type::ObjectRef;
                pv.str = ep->parent->name;
                ep->set_field("parent", pv);
            }
        } else {
            world.roots.push_back(ep);
        }
        if (!name.empty()) {
            // Normal load: last-write is ignored (keep-first). Overlay: the
            // save's element DISPLACES the live one (Elements.Add override),
            // and we note the name so a live element the save omits can later
            // be dropped from the family.
            if (override_existing) {
                world.register_name(name, ep);
                if (overlaid_names) overlaid_names->insert(name);
            } else if (!world.find(name)) {
                world.register_name(name, ep);
            }
        }
        world.elements.push_back(std::move(e));
        return ep;
    }

    static std::string attr_get(const std::map<std::string, std::string> &a,
                                const char *k) {
        auto it = a.find(k);
        return it == a.end() ? std::string() : it->second;
    }

    // ---- start tag --------------------------------------------------------

    void start(const std::string &name,
               const std::map<std::string, std::string> &attrs) {
        // Inside an ignored subtree: swallow everything.
        if (!frames.empty() && frames.back().kind == Frame::Kind::Ignore) {
            frames.back().ignore_depth++;
            return;
        }
        if (is_editor_tag(name)) {
            Frame f;
            f.kind = Frame::Kind::Ignore;
            f.tag = name;
            f.ignore_depth = 1;
            frames.push_back(std::move(f));
            return;
        }

        Frame::Kind ctx =
            frames.empty() ? Frame::Kind::Element : frames.back().kind;

        // Nested value/item structure. A <value>/<item> may sit inside a Field
        // OR inside another Value / ItemValue (a collection whose entries are
        // themselves collections -- Quest's nested grid-coordinate dictionaries).
        bool value_ctx = ctx == Frame::Kind::Field || ctx == Frame::Kind::Value ||
                         ctx == Frame::Kind::ItemValue;
        if (value_ctx && name == "value") {
            Frame f;
            f.kind = Frame::Kind::Value;
            f.tag = name;
            f.type = attr_get(attrs, "type");
            frames.push_back(std::move(f));
            return;
        }
        if (value_ctx && name == "item") {
            Frame f;
            f.kind = Frame::Kind::Item;
            f.tag = name;
            if (attrs.count("key")) {  // scriptdictionary form
                f.item_key = attr_get(attrs, "key");
                f.item_has_key = true;
            }
            frames.push_back(std::move(f));
            return;
        }
        if (ctx == Frame::Kind::Item && name == "key") {
            Frame f;
            f.kind = Frame::Kind::ItemKey;
            f.tag = name;
            frames.push_back(std::move(f));
            return;
        }
        if (ctx == Frame::Kind::Item && name == "value") {
            Frame f;
            f.kind = Frame::Kind::ItemValue;
            f.tag = name;
            f.type = attr_get(attrs, "type");
            frames.push_back(std::move(f));
            return;
        }

        // asl root: pull the version out and treat as a leaf.
        if (name == "asl") {
            std::string v = attr_get(attrs, "version");
            world.version_string = v;
            world.asl_version = std::atoi(v.c_str());
            Frame f;
            f.kind = Frame::Kind::Leaf;
            f.tag = name;
            f.attrs = attrs;
            frames.push_back(std::move(f));
            return;
        }

        if (is_container_tag(name)) {
            start_container(name, attrs);
            return;
        }
        if (is_leaf_tag(name)) {
            Frame f;
            f.kind = Frame::Kind::Leaf;
            f.tag = name;
            f.attrs = attrs;
            frames.push_back(std::move(f));
            return;
        }

        // Otherwise: a field/attribute on the current element.
        start_field(name, attrs);
    }

    void start_container(const std::string &name,
                         const std::map<std::string, std::string> &attrs) {
        std::string nm = attr_get(attrs, "name");
        bool anon = false;
        Element *e = nullptr;

        if (name == "game") {
            // Quest's Game is always the element literally named "game"; the
            // name attribute is only its display name.
            e = new_element("game", "game", false);
            world.game_name = nm;
            if (!nm.empty()) {
                Value v;
                v.type = Value::Type::String;
                v.str = nm;
                e->set_field("gamename", v);
            }
        } else if (name == "command" || name == "verb") {
            std::string pattern = attr_get(attrs, "pattern");
            if (name == "verb" && nm.empty()) nm = attr_get(attrs, "property");
            if (nm.empty()) {
                anon = true;
                // Quest derives an id from the pattern's first alnum run.
                std::string base;
                for (char c : pattern) {
                    if (std::isalnum((unsigned char)c)) base += c;
                    else if (!base.empty()) break;
                }
                nm = unique_id(base.empty() ? name : base);
            }
            e = new_element(name, nm, anon);
            if (anon) {
                Value b;
                b.type = Value::Type::Boolean;
                b.boolean = true;
                e->set_field("anonymous", b);
            }
            std::string tmpl = attr_get(attrs, "template");
            if (!pattern.empty()) {
                // A `pattern=` attribute: template-substitute any [refs] and
                // store the result RAW as the regex, without simplepattern
                // conversion -- for verbs too. QuestViva's CommandLoader sets
                // Fields[Pattern] verbatim after GetTemplate ("[look]" ->
                // "^look$|^l$"; "^restart$" stays); VerbLoader only overrides
                // the template= path. Simplepattern conversion is reserved for
                // nested <pattern> elements (defaultcommand's declared type).
                Value p; p.type = Value::Type::String; p.declared_type = "string";
                p.str = replace_templates(pattern);
                e->set_field("pattern", p);
            } else if (!tmpl.empty()) {
                // A `template=` attribute names a verbtemplate; its combined
                // (";"-joined) text is a verb simplepattern
                // (Utility.ConvertVerbSimplePattern -- null separator for
                // commands, the type-provided separator for verbs, so verbs
                // defer like the pattern= path; LoadPattern is virtual).
                std::string ttext;
                if (template_text(tmpl, ttext)) {
                    if (name == "verb") {
                        pending_verb_patterns.emplace_back(e, ttext);
                    } else {
                        Value p; p.type = Value::Type::String;
                        p.declared_type = "string";
                        p.str = convert_verb_simple_pattern(ttext, "");
                        e->set_field("pattern", p);
                    }
                    // v530+: displayverb is the first verb in the list.
                    if (world.asl_version >= 530) {
                        std::vector<std::string> verbs = list_split(ttext);
                        if (!verbs.empty()) {
                            Value dv; dv.type = Value::Type::String;
                            dv.str = trim(verbs[0]);
                            e->set_field("displayverb", dv);
                        }
                    }
                }
            }
            if (name == "verb") {
                std::string prop = attr_get(attrs, "property");
                if (!prop.empty()) {
                    Value v; v.type = Value::Type::String; v.str = prop;
                    e->set_field("property", v);
                }
                Value b; b.type = Value::Type::Boolean; b.boolean = true;
                e->set_field("isverb", b);
            }
        } else if (name == "exit") {
            std::string id = nm;
            if (id.empty()) id = unique_id("exit");
            e = new_element("exit", id, nm.empty());
            std::string alias = attr_get(attrs, "alias");
            std::string to = attr_get(attrs, "to");
            if (alias.empty()) alias = to;
            if (!alias.empty()) {
                Value v; v.type = Value::Type::String; v.str = alias;
                e->set_field("alias", v);
            }
            if (!to.empty()) {
                Value v; v.type = Value::Type::ObjectRef; v.str = to;
                v.declared_type = "object";
                e->set_field("to", v);
            }
        } else {
            // object, type, turnscript, timer, walkthrough, resource, output
            std::string id = nm;
            if (id.empty()) id = unique_id(name);
            e = new_element(name, id, nm.empty());
        }

        Frame f;
        f.kind = Frame::Kind::Element;
        f.tag = name;
        f.elem = e;
        frames.push_back(std::move(f));
        containers.push_back(e);
    }

    void start_field(const std::string &name,
                     const std::map<std::string, std::string> &attrs) {
        if (!current()) {
            // A field with no enclosing element -- malformed; ignore its subtree.
            Frame f;
            f.kind = Frame::Kind::Ignore;
            f.tag = name;
            f.ignore_depth = 1;
            frames.push_back(std::move(f));
            return;
        }
        Frame f;
        f.kind = Frame::Kind::Field;
        f.tag = name;
        f.attr_name = (name == "attr") ? attr_get(attrs, "name") : name;
        f.type = attr_get(attrs, "type");
        f.attrs = attrs;
        frames.push_back(std::move(f));
    }

    // ---- character data ---------------------------------------------------

    void chardata(const char *s, int len) {
        if (frames.empty()) return;
        Frame &f = frames.back();
        if (f.kind == Frame::Kind::Ignore) return;
        f.text.append(s, len);
    }

    // ---- end tag ----------------------------------------------------------

    void end(const std::string &name) {
        if (frames.empty()) return;
        Frame f = std::move(frames.back());
        frames.pop_back();

        switch (f.kind) {
        case Frame::Kind::Ignore:
            if (--f.ignore_depth > 0) frames.push_back(std::move(f));
            return;
        case Frame::Kind::Element:
            finish_element(f);
            if (!containers.empty()) containers.pop_back();
            return;
        case Frame::Kind::Field:
            finish_field(f);
            return;
        case Frame::Kind::Value:
            // Build this <value> as a Value (scalar, or a nested collection from
            // its own children) and hand it to the enclosing collection frame.
            if (!frames.empty()) {
                Frame &p = frames.back();
                if (p.kind == Frame::Kind::Field || p.kind == Frame::Kind::Value ||
                    p.kind == Frame::Kind::ItemValue) {
                    p.values.push_back(build_value(f.type, f));
                    p.value_texts.push_back(f.text);
                }
            }
            return;
        case Frame::Kind::ItemKey:
            if (!frames.empty() && frames.back().kind == Frame::Kind::Item) {
                frames.back().item_key = trim(f.text);
                frames.back().item_has_key = true;
            }
            return;
        case Frame::Kind::ItemValue:
            if (!frames.empty() && frames.back().kind == Frame::Kind::Item) {
                frames.back().item_val = build_value(f.type, f);
                frames.back().item_val_text = f.text;
                frames.back().item_has_value = true;
            }
            return;
        case Frame::Kind::Item:
            if (!frames.empty()) {
                Frame &p = frames.back();
                if (p.kind == Frame::Kind::Field || p.kind == Frame::Kind::Value ||
                    p.kind == Frame::Kind::ItemValue) {
                    Frame::ItemData d;
                    d.key = f.item_key;
                    d.has_val = f.item_has_value;
                    d.val = std::move(f.item_val);
                    d.val_text = f.item_val_text;
                    d.text = f.text;  // scriptdictionary item body
                    p.items.push_back(std::move(d));
                }
            }
            return;
        case Frame::Kind::Leaf:
            finish_leaf(f);
            return;
        }
    }

    void finish_element(Frame &f) {
        Element *e = f.elem;
        std::string body = f.text;
        // command body text is its script; verb body text is defaulttext.
        if (e->kind == ElemKind::Command) {
            if (!trim(body).empty()) {
                Value v; v.type = Value::Type::Script;
                v.str = replace_templates(body);
                v.declared_type = "script";
                e->set_field("script", v);
            }
        } else if (e->kind == ElemKind::Verb) {
            if (!trim(body).empty()) {
                Value v; v.type = Value::Type::String;
                v.str = replace_templates(trim(body));
                e->set_field("defaulttext", v);
            }
        }
    }

    void finish_leaf(Frame &f) {
        const std::string &tag = f.tag;
        if (tag == "asl" || tag == "library") return;
        if (tag == "inherit") {
            if (current()) current()->inherits.push_back(attr_get(f.attrs, "name"));
            return;
        }
        if (tag == "implied") {
            std::string el = attr_get(f.attrs, "element");
            std::string prop = attr_get(f.attrs, "property");
            std::string ty = attr_get(f.attrs, "type");
            world.implied_types[el + "~" + prop] = ty;
            return;
        }
        if (tag == "include") {
            std::string ref = attr_get(f.attrs, "ref");
            if (!ref.empty()) {
                world.includes.push_back(ref);
                resolve_include(ref);
            }
            return;
        }
        // In a save overlay the original's templates are already registered,
        // and appending would duplicate them (unbounded across save/restore
        // cycles); a native save re-emits them only for a standalone
        // Quest/QuestViva reload.
        if (tag == "template") {
            if (!override_existing)
                world.templates.emplace_back(attr_get(f.attrs, "name"), f.text);
            return;
        }
        if (tag == "dynamictemplate") {
            if (!override_existing)
                world.dynamic_templates.emplace_back(attr_get(f.attrs, "name"),
                                                     f.text);
            return;
        }
        if (tag == "verbtemplate") {
            if (!override_existing)
                world.verb_templates.emplace_back(attr_get(f.attrs, "name"), f.text);
            return;
        }
        if (tag == "javascript") {
            Element *e = new_element("javascript", unique_id("javascript"), true);
            Value v; v.type = Value::Type::String; v.str = attr_get(f.attrs, "src");
            e->set_field("src", v);
            return;
        }
        if (tag == "function" || tag == "delegate") {
            std::string nm = attr_get(f.attrs, "name");
            if (nm.empty()) nm = unique_id(tag);
            Element *e = new_element(tag, nm, false);
            std::string params = attr_get(f.attrs, "parameters");
            if (!params.empty()) {
                Value pv; pv.type = Value::Type::StringList;
                pv.declared_type = "stringlist";
                // parameters split on ", " / ","
                std::string cur;
                for (size_t i = 0; i < params.size(); ++i) {
                    if (params[i] == ',') {
                        pv.list().push_back(list_entry_value(trim(cur), pv.type));
                        cur.clear();
                    } else cur += params[i];
                }
                pv.list().push_back(list_entry_value(trim(cur), pv.type));
                e->set_field("paramnames", pv);
            }
            std::string ret = attr_get(f.attrs, "type");
            if (!ret.empty()) {
                Value rv; rv.type = Value::Type::String; rv.str = ret;
                e->set_field("returntype", rv);
            }
            Value sv; sv.type = Value::Type::Script;
            sv.str = replace_templates(f.text);
            sv.declared_type = "script";
            e->set_field("script", sv);
            return;
        }
    }

    // ---- field value resolution ------------------------------------------

    // Resolve a field's declared/implied type, then build and set its Value.
    void finish_field(Frame &f) {
        Element *owner = current();
        if (!owner) return;
        const std::string &attr = f.attr_name;
        if (attr.empty()) return;

        std::string type = f.type;
        // A native saved game (override_existing) re-emits NO <implied>
        // declarations and stores already-final values (e.g. command patterns
        // as raw regex, not simplepattern), so QuestViva's own reload resolves
        // every type-less attribute to string/boolean, NOT its implied type.
        // Mirror that: skip implied resolution in a save overlay, or an
        // already-converted <pattern> would be simplepattern-converted again.
        if (type.empty() && !override_existing) {
            const std::string *imp =
                world.implied_type(owner->elem_type, attr);
            // Verbs ARE commands in QuestViva (ElementType.Command), so the
            // <implied element="command"> declarations apply to them -- this
            // is how a verb's nested <pattern> gets the simplepattern type.
            if (!imp && owner->kind == ElemKind::Verb)
                imp = world.implied_type("command", attr);
            if (imp) type = *imp;
        }

        // Legacy type-name remapping for old games (<= v530).
        if (!type.empty() && world.asl_version <= 530) {
            if (type == "list") type = "simplestringlist";
            else if (type == "stringdictionary") type = "simplestringdictionary";
            else if (type == "objectdictionary") type = "simpleobjectdictionary";
        }

        bool nested = !f.values.empty() || !f.items.empty();
        if (type.empty()) type = (!f.text.empty() || nested) ? "string" : "boolean";

        set_typed_field_from_frame(owner, attr, type, f);
    }

    // Build a Value of `type` from a frame's text or nested (recursive) child
    // values/items. `apply_templates` is true only at the field level -- list
    // and dictionary ENTRIES keep their raw text (matching the historical
    // loader, which template-substituted attributes but not collection
    // entries). `ctx` labels errors. Verb simplepatterns have an owner side
    // effect and are handled by set_typed_field_from_frame, not here.
    Value build_value(const std::string &type_in, Frame &f,
                      bool apply_templates, const std::string &ctx) {
        std::string type = type_in;
        // Legacy type-name remapping for old games (<= v530); idempotent.
        if (!type.empty() && world.asl_version <= 530) {
            if (type == "list") type = "simplestringlist";
            else if (type == "stringdictionary") type = "simplestringdictionary";
            else if (type == "objectdictionary") type = "simpleobjectdictionary";
        }
        bool nested = !f.values.empty() || !f.items.empty();
        if (type.empty()) type = (!f.text.empty() || nested) ? "string" : "boolean";

        Value v;
        v.declared_type = type;
        const std::string &text = f.text;
        auto tmpl = [&](const std::string &s) {
            return apply_templates ? replace_templates(s) : s;
        };

        if (type == "string") {
            v.type = Value::Type::String; v.str = tmpl(text);
        } else if (type == "script") {
            v.type = Value::Type::Script; v.str = tmpl(text);
        } else if (type == "int") {
            v.type = Value::Type::Int; v.integer = std::atol(trim(text).c_str());
        } else if (type == "double") {
            v.type = Value::Type::Double; v.dbl = c_strtod(trim(text).c_str());
        } else if (type == "boolean") {
            v.type = Value::Type::Boolean;
            std::string t = trim(text);
            v.boolean = (t.empty() || t == "true");
        } else if (type == "simplepattern") {
            v.type = Value::Type::String; v.str = convert_simple_pattern(text);
        } else if (type == "object") {
            v.type = Value::Type::ObjectRef; v.str = trim(text);
        } else if (type == "simplestringlist") {
            v.type = Value::Type::StringList;
            fill_list(v, simple_list_values(text));
        } else if (type == "listextend") {
            // A list that appends to the inherited same-named list. Core uses it
            // heavily for displayverbs/inventoryverbs on its object types.
            v.type = Value::Type::StringList;
            fill_list(v, simple_list_values(text));
            v.list_extend = true;
        } else if (type == "stringlist") {
            // Homogeneous string list: entries are strings from raw <value>
            // text (an empty <value/> is the empty string, NOT a boolean).
            v.type = Value::Type::StringList;
            fill_list(v, f.value_texts);
        } else if (type == "list") {
            // A general list of nested <value type="..">s -- each already built
            // as a typed Value (and may itself be a collection). ObjectList when
            // every entry is an object ref, else StringList (TypeOf reports one
            // of those two; a distinct "list" name is still TODO).
            bool all_obj = !f.values.empty();
            for (const Value &cv : f.values)
                if (cv.type != Value::Type::ObjectRef) { all_obj = false; break; }
            v.type = all_obj ? Value::Type::ObjectList : Value::Type::StringList;
            for (const Value &cv : f.values) v.list().push_back(cv);
        } else if (type == "objectlist") {
            v.type = Value::Type::ObjectList;
            for (const std::string &s : simple_list_values(text))
                if (!s.empty()) v.list().push_back(list_entry_value(s, v.type));
        } else if (type == "simplestringdictionary" ||
                   type == "simpleobjectdictionary") {
            v.type = (type[6] == 'o') ? Value::Type::ObjectDict
                                      : Value::Type::StringDict;
            for (const std::string &pair : list_split(text)) {
                std::string p = trim(pair);
                if (p.empty()) continue;
                size_t eq = p.find('=');
                if (eq == std::string::npos) {
                    error("dict missing '=' in " + ctx); continue;
                }
                v.dict().emplace_back(trim(p.substr(0, eq)),
                    dict_entry_value(trim(p.substr(eq + 1)), v.type));
            }
        } else if (type == "stringdictionary" || type == "objectdictionary" ||
                   type == "dictionary") {
            // A string/object dictionary holds plain string / object-ref values
            // (from raw <value> text; empty is the empty string). The GENERIC
            // "dictionary" holds typed, possibly nested values (grid coords).
            v.type = (type[0] == 'o') ? Value::Type::ObjectDict
                                      : Value::Type::StringDict;
            bool generic = type == "dictionary";
            for (const Frame::ItemData &it : f.items) {
                Value ev;
                if (generic && it.has_val)
                    ev = it.val;                          // typed / nested
                else
                    ev = dict_entry_value(it.has_val ? it.val_text : it.text,
                                          v.type);        // string / object ref
                v.dict().emplace_back(it.key, std::move(ev));
            }
        } else if (type == "scriptdictionary") {
            v.type = Value::Type::ScriptDict;
            for (const Frame::ItemData &it : f.items)
                v.dict().emplace_back(
                    it.key, dict_entry_value(it.has_val ? it.val_text : it.text,
                                             v.type));
        } else if (Element *del = world.find(type);
                   del && del->kind == ElemKind::Delegate) {
            // An attribute whose type is a registered <delegate>: the body is a
            // delegate implementation, i.e. a script matching the delegate's
            // signature (QuestViva stores a DelegateImplementation wrapping it).
            // The delegate name is kept in declared_type so `rundelegate` can
            // look up its parameter names at invoke time.
            v.type = Value::Type::Script; v.str = tmpl(text);
        } else {
            // Unknown type: keep the raw text as a string, but flag it.
            v.type = Value::Type::String; v.str = text;
            error("unrecognised attribute type '" + type + "' in " + ctx);
        }
        // A loaded collection owns its backing even when empty (reference
        // semantics -- see Value::ensure_backing).
        v.ensure_backing();
        return v;
    }

    // Nested-value entry point (list <value> / dict item <value>): resolves the
    // type from the frame alone (no owner/implied), raw entries.
    Value build_value(const std::string &type, Frame &f) {
        return build_value(type, f, /*apply_templates=*/false, "collection entry");
    }

    // Build a Value of `type` from the frame and set it on `owner`.
    void set_typed_field_from_frame(Element *owner, const std::string &attr,
                                    const std::string &type, Frame &f) {
        // A verb's simplepattern has an owner side effect (displayverb + a
        // deferred ConvertVerbSimplePattern) that the generic builder can't do.
        if (type == "simplepattern" && owner->kind == ElemKind::Verb) {
            const std::string &text = f.text;
            pending_verb_patterns.emplace_back(owner, text);
            std::string first = trim(text.substr(0, text.find(';')));
            size_t h;
            while ((h = first.find("#object#")) != std::string::npos)
                first.erase(h, 8);
            Value dv; dv.type = Value::Type::String; dv.str = trim(first);
            owner->set_field("displayverb", dv);
            return;
        }
        owner->set_field(attr,
                         build_value(type, f, /*apply_templates=*/true,
                                     owner->name + "." + attr));
    }

    // Set a field from a plain string of a known type (used for pattern etc.).
    void set_typed_field(Element *owner, const std::string &attr,
                         const std::string &type, const std::string &text) {
        Frame tmp;
        tmp.text = text;
        set_typed_field_from_frame(owner, attr, type, tmp);
    }

    // Substitute `[name]` template references with the named template's text,
    // using templates registered so far (later registrations win, matching
    // QuestViva's Template.ReplaceTemplateText / m_templateLookup). An
    // unmatched reference is left verbatim -- this is what keeps regex character
    // classes like [^\w] intact. Applied at load time to script bodies and
    // string attributes, exactly where the GameLoader calls GetTemplate.
    // Template.GetText: the text registered for a template name. Verb templates
    // sharing a name are joined by "; " (QuestViva's AddVerbTemplate appends),
    // and command/plain templates fall back to the last registration. Verb
    // templates take priority since a `template=` command ref names one.
    bool template_text(const std::string &name, std::string &out) {
        std::string joined;
        bool any = false;
        for (auto &vt : world.verb_templates) {
            if (vt.first != name) continue;
            if (any) joined += "; ";
            joined += vt.second;
            any = true;
        }
        if (any) { out = joined; return true; }
        for (auto it = world.templates.rbegin(); it != world.templates.rend();
             ++it) {
            if (it->first == name) { out = it->second; return true; }
        }
        return false;
    }

    std::string replace_templates(const std::string &text) {
        if (text.find('[') == std::string::npos) return text;
        std::string out = text;
        size_t start = 0;
        while (start < out.size()) {
            size_t open = out.find('[', start);
            if (open == std::string::npos) break;
            size_t close = out.find(']', open + 1);
            if (close == std::string::npos) break;
            std::string name = out.substr(open + 1, close - open - 1);
            std::string value;
            bool found = false;
            for (auto it = world.templates.rbegin();
                 it != world.templates.rend(); ++it) {
                if (it->first == name) { value = it->second; found = true; break; }
            }
            if (found) {
                out = out.substr(0, open) + value + out.substr(close + 1);
                start = open + value.size();
            } else {
                start = close + 1;  // leave "[name]" as-is
            }
        }
        return out;
    }
};

// expat trampolines
static void XMLCALL cb_start(void *ud, const XML_Char *name,
                             const XML_Char **atts) {
    Loader *ld = static_cast<Loader *>(ud);
    std::map<std::string, std::string> attrs;
    for (int i = 0; atts[i]; i += 2) attrs[atts[i]] = atts[i + 1] ? atts[i + 1] : "";
    ld->start(name, attrs);
}
static void XMLCALL cb_end(void *ud, const XML_Char *name) {
    static_cast<Loader *>(ud)->end(name);
}
static void XMLCALL cb_char(void *ud, const XML_Char *s, int len) {
    static_cast<Loader *>(ud)->chardata(s, len);
}

// Parse one document into an existing Loader (shared World). A parse error is
// recorded but leaves whatever was already loaded intact.
static bool parse_buffer_into(Loader &ld, const char *data, size_t len) {
    if (len > (size_t)INT_MAX) {  // XML_Parse takes an int
        ld.world.errors.push_back("document too large");
        return false;
    }
    XML_Parser p = XML_ParserCreate("UTF-8");
    if (!p) { ld.world.errors.push_back("XML_ParserCreate failed"); return false; }
    XML_SetUserData(p, &ld);
    XML_SetElementHandler(p, cb_start, cb_end);
    XML_SetCharacterDataHandler(p, cb_char);
    // CDATA sections are delivered through the normal character handler, which
    // is what we want (script bodies use CDATA).
    bool ok = true;
    if (XML_Parse(p, data, (int)len, 1) == XML_STATUS_ERROR) {
        std::ostringstream o;
        o << "XML parse error at line " << XML_GetCurrentLineNumber(p) << ": "
          << XML_ErrorString(XML_GetErrorCode(p));
        ld.world.errors.push_back(o.str());
        ok = false;
    }
    XML_ParserFree(p);
    return ok;
}

static bool slurp_file(const std::string &path, std::string &out) {
    FILE *fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;
    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (sz < 0) { std::fclose(fp); return false; }
    out.resize((size_t)sz);
    size_t got = sz ? std::fread(&out[0], 1, (size_t)sz, fp) : 0;
    std::fclose(fp);
    out.resize(got);
    return true;
}

// The bundled Core dir, resolved once. An explicit argument wins; otherwise the
// ASLX_CORE environment variable (set by the test harness / app wiring).
static std::string resolve_core_dir(const std::string &explicit_dir) {
    if (!explicit_dir.empty()) return explicit_dir;
    const char *env = std::getenv("ASLX_CORE");
    return env ? std::string(env) : std::string();
}

// Apply the implicit default type every element gets by its kind, mirroring
// QuestViva's ObjectFactory.CreateObject + WorldModel.DefaultTypeNames: an
// object inherits defaultobject, the game inherits defaultgame, and so on. The
// default type is prepended (lowest priority) so game-declared and user types
// override it. A Core-less game never defines these types, so resolve_field
// simply skips the missing name -- harmless.
static void apply_default_types(World &world) {
    for (auto &up : world.elements) {
        const std::string &t = up->elem_type;
        const char *dt = nullptr;
        if (t == "object") dt = "defaultobject";
        else if (t == "game") dt = "defaultgame";
        else if (t == "exit") dt = "defaultexit";
        else if (t == "command" || t == "verb") dt = "defaultcommand";
        else if (t == "turnscript") dt = "defaultturnscript";
        if (!dt) continue;
        auto &inh = up->inherits;
        if (std::find(inh.begin(), inh.end(), dt) == inh.end())
            inh.insert(inh.begin(), dt);
    }
}

// Own field, else inherited types most-recent-first (load-time inheritance
// walk; the runtime equivalent is Interp::resolve_field).
static const Value *find_field_rec(World &world, Element *e,
                                   const std::string &name,
                                   std::set<Element *> &seen) {
    if (!e || !seen.insert(e).second) return nullptr;
    if (const Value *own = e->field(name)) return own;
    for (auto it = e->inherits.rbegin(); it != e->inherits.rend(); ++it)
        if (const Value *v = find_field_rec(world, world.find(*it), name, seen))
            return v;
    return nullptr;
}

// Deferred <verb> pattern conversion (VerbLoader.LoadPattern's lazy action):
// now that every include is parsed, the separator -- the verb's own
// <separator> or the defaultverb type's ("with; using") -- is resolvable, and
// ConvertVerbSimplePattern can build the final regex, object2 tail included.
static void finish_verb_patterns(Loader &ld, World &world) {
    for (auto &pv : ld.pending_verb_patterns) {
        Element *e = pv.first;
        std::set<Element *> seen;
        const Value *sep = find_field_rec(world, e, "separator", seen);
        std::string separator =
            (sep && sep->type == Value::Type::String) ? sep->str : "";
        Value p; p.type = Value::Type::String; p.declared_type = "string";
        p.str = convert_verb_simple_pattern(pv.second, separator);
        e->set_field("pattern", p);
    }
}

static bool finish_load(Loader &ld, World &world) {
    if (world.asl_version == 0)
        world.errors.push_back("no ASL version found (missing <asl version=>)");
    // Verbs inherit the defaultverb type (VerbLoader's AddType): its generic
    // script dispatches on the object's `property` attribute, and it carries
    // the separator/multi-object defaults. Inserted at the front so explicit
    // <inherit>s override it, while it overrides the implicit defaultcommand
    // prepended by apply_default_types below.
    for (auto &up : world.elements) {
        if (up->kind != ElemKind::Verb) continue;
        auto &inh = up->inherits;
        if (std::find(inh.begin(), inh.end(), "defaultverb") == inh.end())
            inh.insert(inh.begin(), "defaultverb");
    }
    apply_default_types(world);
    finish_verb_patterns(ld, world);
    // A genuinely broken load surfaces as an XML/parse error or a missing
    // version; unresolved includes and per-field warnings are non-fatal.
    (void)ld;
    for (const std::string &e : world.errors)
        if (starts_with(e, "XML parse error") || starts_with(e, "no ASL"))
            return false;
    return true;
}

bool load_aslx_buffer(const char *data, size_t len, World &world,
                      const std::string &core_dir, const std::string &game_dir) {
    Loader ld(world);
    ld.core_dir = resolve_core_dir(core_dir);
    ld.game_dir = game_dir;
    parse_buffer_into(ld, data, len);
    return finish_load(ld, world);
}

bool overlay_aslx_buffer(const char *data, size_t len, World &world,
                         std::set<std::string> &overlaid,
                         const std::string &core_dir) {
    Loader ld(world);
    ld.core_dir = resolve_core_dir(core_dir);
    ld.override_existing = true;
    ld.overlaid_names = &overlaid;
    // Templates registered by the original load still live in `world`, so
    // replace_templates resolves against them during the overlay (matching
    // QuestViva's v530+ re-scan on saved-game reload). The overlay itself
    // rarely carries [refs] -- saved values are already substituted -- so this
    // is normally a no-op that only guards against re-mangling.
    parse_buffer_into(ld, data, len);
    return finish_load(ld, world);
}

bool load_file(const std::string &path, World &world,
               const std::string &core_dir) try {
    std::string buf;
    if (!slurp_file(path, buf)) {
        world.errors.push_back("cannot open " + path);
        return false;
    }
    if (buf.empty()) { world.errors.push_back("empty file"); return false; }

    // The game's own directory is searched first for <include> refs.
    std::string game_dir;
    size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) game_dir = path.substr(0, slash);

    const uint8_t *u = reinterpret_cast<const uint8_t *>(buf.data());
    if (buf.size() >= 4 && u[0] == 'P' && u[1] == 'K' && u[2] == 3 && u[3] == 4) {
        std::string aslx;
        if (!extract_game_aslx(u, buf.size(), aslx)) {
            world.errors.push_back("no game.aslx in package " + path);
            return false;
        }
        // Packaged games may bundle their own libraries inside the zip; those
        // are not yet extracted, so only the Core dir resolves for now.
        return load_aslx_buffer(aslx.data(), aslx.size(), world, core_dir, "");
    }
    return load_aslx_buffer(buf.data(), buf.size(), world, core_dir, game_dir);
} catch (const std::bad_alloc &) {
    // Untrusted length fields can demand absurd allocations; fail the load
    // instead of aborting the host.
    world.errors.push_back("out of memory loading " + path);
    return false;
} catch (const std::length_error &) {
    world.errors.push_back("out of memory loading " + path);
    return false;
}

// ---------------------------------------------------------------------------
// Dump
// ---------------------------------------------------------------------------

static void dump_element(std::ostringstream &o, const Element *e, int depth) {
    std::string ind(depth * 2, ' ');
    o << ind << "[" << e->elem_type << "] " << e->name;
    if (e->anonymous) o << " (anon)";
    o << "\n";
    for (const std::string &t : e->inherits)
        o << ind << "  inherit " << t << "\n";
    for (const auto &kv : e->fields)
        o << ind << "  ." << kv.first << " = " << kv.second.debug_string() << "\n";
    for (const Element *c : e->children) dump_element(o, c, depth + 1);
}

std::string dump(const World &world) {
    std::ostringstream o;
    o << "ASL version: " << world.asl_version << "\n";
    if (!world.game_name.empty()) o << "game: " << world.game_name << "\n";
    o << "elements: " << world.elements.size()
      << " (roots: " << world.roots.size() << ")\n";
    o << "templates: " << world.templates.size()
      << ", implied types: " << world.implied_types.size()
      << ", includes: " << world.includes.size() << "\n";
    o << "----\n";
    for (const Element *r : world.roots) dump_element(o, r, 0);
    if (!world.errors.empty()) {
        o << "----\nerrors (" << world.errors.size() << "):\n";
        for (const std::string &e : world.errors) o << "  " << e << "\n";
    }
    return o.str();
}

}  // namespace aslx
