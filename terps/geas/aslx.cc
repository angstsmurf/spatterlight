// aslx.cc -- Quest 5 loader implementation. See aslx.hh.

#include "aslx.hh"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <sstream>

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
        for (size_t i = 0; i < list.size(); ++i) {
            if (i) o << ", ";
            o << list[i];
        }
        o << ']';
        break;
    }
    case Type::ObjectRef: o << "->" << str; break;
    case Type::StringDict:
    case Type::ObjectDict:
    case Type::ScriptDict: {
        o << "dict{";
        for (size_t i = 0; i < dict.size(); ++i) {
            if (i) o << ", ";
            o << dict[i].first << "=" << dict[i].second;
        }
        o << '}';
        break;
    }
    }
    return o.str();
}

const Value *Element::field(const std::string &n) const {
    for (const auto &kv : fields)
        if (kv.first == n) return &kv.second;
    return nullptr;
}

Value &Element::set_field(const std::string &n, Value v) {
    for (auto &kv : fields) {
        if (kv.first == n) {
            kv.second = std::move(v);
            return kv.second;
        }
    }
    fields.emplace_back(n, std::move(v));
    return fields.back().second;
}

Element *World::find(const std::string &n) const {
    auto it = by_name.find(n);
    return it == by_name.end() ? nullptr : it->second;
}

const std::string *World::implied_type(const std::string &elem_type,
                                       const std::string &property) const {
    auto it = implied_types.find(elem_type + "~" + property);
    return it == implied_types.end() ? nullptr : &it->second;
}

Element *World::create_object(const std::string &name, const std::string &type) {
    auto e = std::make_unique<Element>();
    Element *ep = e.get();
    ep->elem_type = "object";
    ep->name = name;
    // Every runtime-created object inherits defaultobject (lowest priority),
    // then the caller's explicit type -- QuestViva ObjectFactory.CreateObject
    // inserts the default type at the front of initialTypes. resolve_field skips
    // it in a Core-less game that never defines defaultobject.
    ep->inherits.push_back("defaultobject");
    if (!type.empty()) ep->inherits.push_back(type);
    roots.push_back(ep);
    by_name[name] = ep;            // last create wins, like ObjectFactory
    elements.push_back(std::move(e));
    return ep;
}

void World::destroy_element(const std::string &name) {
    by_name.erase(name);
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

bool extract_game_aslx(const uint8_t *data, size_t len, std::string &out) {
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
    const uint8_t *p = data + cd_off;

    for (uint16_t e = 0; e < count && p + 46 <= data + len; ++e) {
        if (!(p[0] == 'P' && p[1] == 'K' && p[2] == 1 && p[3] == 2)) break;
        uint16_t method = rd16(p + 10);
        uint32_t comp_size = rd32(p + 20);
        uint32_t raw_size = rd32(p + 24);
        uint16_t name_len = rd16(p + 28);
        uint16_t extra_len = rd16(p + 30);
        uint16_t comment_len = rd16(p + 32);
        uint32_t local_off = rd32(p + 42);
        std::string name((const char *)p + 46, name_len);

        if (name == "game.aslx") {
            const uint8_t *lh = data + local_off;
            if (lh + 30 > data + len ||
                !(lh[0] == 'P' && lh[1] == 'K' && lh[2] == 3 && lh[3] == 4))
                return false;
            uint16_t l_name = rd16(lh + 26);
            uint16_t l_extra = rd16(lh + 28);
            const uint8_t *src = lh + 30 + l_name + l_extra;
            if (src + comp_size > data + len) return false;
            if (method == 0) {  // stored
                out.assign((const char *)src, raw_size);
                return true;
            }
            if (method == 8)  // deflate
                return inflate_raw(src, comp_size, raw_size, out);
            return false;
        }
        p += 46 + name_len + extra_len + comment_len;
    }
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
    std::vector<std::string> values;       // Field: <value> children
    std::vector<std::string> value_types;  // Field: per-<value> type
    std::vector<std::pair<std::string, std::string>> items;  // Field: dict items
    std::string item_key;      // Item scratch
    std::string item_value;
    bool item_has_key = false;
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
        ep->name = name;
        ep->anonymous = anonymous;
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
        if (!name.empty() && !world.find(name)) world.by_name[name] = ep;
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

        // Nested value/item structure inside a field.
        if (ctx == Frame::Kind::Field && name == "value") {
            Frame f;
            f.kind = Frame::Kind::Value;
            f.tag = name;
            f.type = attr_get(attrs, "type");
            frames.push_back(std::move(f));
            return;
        }
        if (ctx == Frame::Kind::Field && name == "item") {
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
            if (!pattern.empty()) set_typed_field(e, "pattern", "simplepattern",
                                                  pattern);
            std::string tmpl = attr_get(attrs, "template");
            if (!tmpl.empty()) {
                Value t;
                t.type = Value::Type::String;
                t.str = tmpl;
                e->set_field("template", t);
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
            if (!frames.empty() && frames.back().kind == Frame::Kind::Field) {
                frames.back().values.push_back(f.text);
                frames.back().value_types.push_back(f.type);
            }
            return;
        case Frame::Kind::ItemKey:
            if (!frames.empty() && frames.back().kind == Frame::Kind::Item) {
                frames.back().item_key = trim(f.text);
                frames.back().item_has_key = true;
            }
            return;
        case Frame::Kind::ItemValue:
            if (!frames.empty() && frames.back().kind == Frame::Kind::Item)
                frames.back().item_value = f.text;
            return;
        case Frame::Kind::Item:
            if (!frames.empty() && frames.back().kind == Frame::Kind::Field) {
                // scriptdictionary uses the item's own text as the value.
                std::string val = f.item_value.empty() ? f.text : f.item_value;
                frames.back().items.emplace_back(f.item_key, val);
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
        if (e->elem_type == "command") {
            if (!trim(body).empty()) {
                Value v; v.type = Value::Type::Script;
                v.str = replace_templates(body);
                v.declared_type = "script";
                e->set_field("script", v);
            }
        } else if (e->elem_type == "verb") {
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
        if (tag == "template") {
            world.templates.emplace_back(attr_get(f.attrs, "name"), f.text);
            return;
        }
        if (tag == "dynamictemplate") {
            world.dynamic_templates.emplace_back(attr_get(f.attrs, "name"),
                                                 f.text);
            return;
        }
        if (tag == "verbtemplate") {
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
                        pv.list.push_back(trim(cur)); cur.clear();
                    } else cur += params[i];
                }
                pv.list.push_back(trim(cur));
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
        if (type.empty()) {
            const std::string *imp =
                world.implied_type(owner->elem_type, attr);
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

    // Build a Value of `type` from either the frame's text or nested children,
    // and set it on `owner`.
    void set_typed_field_from_frame(Element *owner, const std::string &attr,
                                    const std::string &type, Frame &f) {
        Value v;
        v.declared_type = type;
        const std::string &text = f.text;

        if (type == "string") {
            v.type = Value::Type::String; v.str = replace_templates(text);
        } else if (type == "script") {
            v.type = Value::Type::Script; v.str = replace_templates(text);
        } else if (type == "int") {
            v.type = Value::Type::Int; v.integer = std::atol(trim(text).c_str());
        } else if (type == "double") {
            v.type = Value::Type::Double; v.dbl = std::atof(trim(text).c_str());
        } else if (type == "boolean") {
            v.type = Value::Type::Boolean;
            std::string t = trim(text);
            v.boolean = (t.empty() || t == "true");
        } else if (type == "simplepattern") {
            v.type = Value::Type::String; v.str = convert_simple_pattern(text);
        } else if (type == "object") {
            v.type = Value::Type::ObjectRef; v.str = trim(text);
        } else if (type == "simplestringlist") {
            v.type = Value::Type::StringList; v.list = simple_list_values(text);
        } else if (type == "listextend") {
            // A list that appends to the inherited same-named list. Core uses it
            // heavily for displayverbs/inventoryverbs on its object types.
            v.type = Value::Type::StringList; v.list = simple_list_values(text);
            v.list_extend = true;
        } else if (type == "stringlist") {
            v.type = Value::Type::StringList; v.list = f.values;
        } else if (type == "list") {
            // A general list built from nested <value type="..."> children; each
            // value may carry its own type (QuestViva's QuestList<object>). Our
            // Value model has no heterogeneous list, so store an ObjectList when
            // every value is an object reference, otherwise a StringList.
            bool all_obj = !f.values.empty();
            for (const std::string &t : f.value_types)
                if (t != "object") { all_obj = false; break; }
            v.type = all_obj ? Value::Type::ObjectList : Value::Type::StringList;
            v.list = f.values;
        } else if (type == "objectlist") {
            v.type = Value::Type::ObjectList;
            for (const std::string &s : simple_list_values(text))
                if (!s.empty()) v.list.push_back(s);
        } else if (type == "simplestringdictionary" ||
                   type == "simpleobjectdictionary") {
            v.type = (type[6] == 'o') ? Value::Type::ObjectDict
                                      : Value::Type::StringDict;
            for (const std::string &pair : list_split(text)) {
                std::string p = trim(pair);
                if (p.empty()) continue;
                size_t eq = p.find('=');
                if (eq == std::string::npos) { error("dict missing '=' in " +
                    owner->name + "." + attr); continue; }
                v.dict.emplace_back(trim(p.substr(0, eq)), trim(p.substr(eq + 1)));
            }
        } else if (type == "stringdictionary" || type == "objectdictionary" ||
                   type == "dictionary") {
            v.type = (type[0] == 'o') ? Value::Type::ObjectDict
                                      : Value::Type::StringDict;
            v.dict = f.items;
        } else if (type == "scriptdictionary") {
            v.type = Value::Type::ScriptDict; v.dict = f.items;
        } else if (Element *del = world.find(type);
                   del && del->elem_type == "delegate") {
            // An attribute whose type is a registered <delegate>: the body is a
            // delegate implementation, i.e. a script matching the delegate's
            // signature (QuestViva stores a DelegateImplementation wrapping it).
            // The delegate name is kept in declared_type so `rundelegate` can
            // look up its parameter names at invoke time.
            v.type = Value::Type::Script; v.str = replace_templates(text);
        } else {
            // Unknown type: keep the raw text as a string, but flag it.
            v.type = Value::Type::String; v.str = text;
            error("unrecognised attribute type '" + type + "' in " +
                  owner->name + "." + attr);
        }
        owner->set_field(attr, std::move(v));
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

static bool finish_load(Loader &ld, World &world) {
    if (world.asl_version == 0)
        world.errors.push_back("no ASL version found (missing <asl version=>)");
    apply_default_types(world);
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

bool load_file(const std::string &path, World &world,
               const std::string &core_dir) {
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
