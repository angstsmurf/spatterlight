// aslx-savenative.inc -- Quest 5 native `.quest-save` compatibility (TODO §5).
//
// Included at the end of aslx-runtime.cc, after aslx-state.inc (whose anonymous
// `save_family` we reuse). This is the interoperable save format: an ASLX
// document Quest's own desktop player and QuestViva read and write, as opposed
// to the compact v1 snapshot in aslx-state.inc.
//
//   <!-- Saved by Geas ... -->
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

struct SaveWriter {
    std::string out;
    int indent = 0;
    int version = 0;

    void pad() { for (int i = 0; i < indent; ++i) out += "  "; }

    // <tag a="v" ...> with no body, self-closed if `selfclose`.
    void open(const std::string &tag,
              const std::vector<std::pair<std::string, std::string>> &attrs,
              bool selfclose) {
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
        out += selfclose ? "/>\n" : ">\n";
        if (!selfclose) ++indent;
    }

    void close(const std::string &tag) {
        --indent;
        pad();
        out += "</";
        out += tag;
        out += ">\n";
    }

    // A leaf field element: <tag type="..">text</tag> on one line, or a
    // self-closed <tag/> when text is empty and no type is given.
    void leaf(const std::string &tag,
              const std::vector<std::pair<std::string, std::string>> &attrs,
              const std::string &text, bool have_text) {
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
    char buf[64];
    for (int prec = 15; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof buf, "%.*g", prec, d);
        if (std::atof(buf) == d) return buf;
    }
    std::snprintf(buf, sizeof buf, "%.17g", d);
    return buf;
}

// Canonical string for a dictionary entry value. Our loader materialises
// string/object-dictionary entries as String/ObjectRef only (typed dict values
// are not round-tripped), so a runtime-added boxed entry (e.g. a boolean flag
// on game.currentcommandresolvedelements) is coerced to its textual form rather
// than serialized as an empty <value>. QuestViva's legacy SaveString does the
// same (ToString of the boxed value).
// A Value that cannot be flattened to a single string: a nested collection.
// Such an entry forces a dictionary/list to serialize as the generic typed form
// (QuestViva's QuestList<object> "dictionary"/"list"), whatever its declared
// container type -- our runtime NewDictionary()/`dictionary add` can put a whole
// dictionary inside a "stringdictionary" (Quest's grid coordinates).
bool is_collection(const Value &v) {
    switch (v.type) {
    case Value::Type::StringList:
    case Value::Type::ObjectList:
    case Value::Type::StringDict:
    case Value::Type::ObjectDict:
    case Value::Type::ScriptDict:
        return true;
    default:
        return false;
    }
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

// Ignore-field sets, mirroring ObjectSaver/ElementSaverBase/TimerSaver. `name`,
// `elementtype` and our synthetic bookkeeping fields are handled structurally
// (element attributes / nesting) and must not be re-emitted as <field>s.
bool ignore_field(const std::string &elem_type, const std::string &attr) {
    if (attr == "name" || attr == "elementtype") return true;
    // ObjectSaver family (everything nested by containment): parent is nesting,
    // type is the element tag / inherits.
    bool obj_family = elem_type == "object" || elem_type == "game" ||
                      elem_type == "command" || elem_type == "verb" ||
                      elem_type == "exit" || elem_type == "turnscript";
    if (obj_family && (attr == "type" || attr == "parent")) return true;
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
// The transform walks the source statement-by-statement exactly like
// parse_statements and consumes ran flags in the SAME order collect_firsttime
// yields them (a statement's own firsttime flag, then its body, else-ifs,
// else/otherwise, then switch cases -- note: default before cases, matching
// collect's else_body-then-cases order). It recurses ONLY into the block-
// bearing keywords collect_firsttime descends into (if/while/for/foreach/
// switch/wait/ask/on ready/get input/show menu + else/otherwise), so a
// firsttime inside a trailing script-block argument -- a separately compiled
// script, not in this flag stream -- is left verbatim and stays aligned.

std::string bake_stmt_list(const std::string &inner, std::deque<bool> &q);

// A statement whose body collect_firsttime descends into (single-body forms;
// switch and firsttime/otherwise are handled by the callers).
bool is_recursing_keyword(const std::string &s) {
    return starts_with_word(s, "if") || starts_with_word(s, "while") ||
           starts_with_word(s, "foreach") || starts_with_word(s, "for") ||
           starts_with_word(s, "wait") || starts_with_word(s, "ask") ||
           starts_with_word(s, "else") ||
           (s.compare(0, 8, "on ready") == 0) ||
           (s.compare(0, 9, "get input") == 0) ||
           (s.compare(0, 9, "show menu") == 0);
}

std::string bake_switch(const std::string &stmt, std::deque<bool> &q) {
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
    struct Chunk { bool is_default; std::string head; std::string inner; };
    std::vector<Chunk> chunks;
    std::string line = rt_trim(body);
    while (!line.empty()) {
        std::string cafter;
        std::string c = rt_trim(get_script(line, cafter));
        line = cafter;
        if (!c.empty()) {
            if (starts_with_word(c, "case")) {
                std::string a2;
                bool f2 = false;
                get_parameter(c, a2, f2);
                if (f2)
                    chunks.push_back({false, c.substr(0, c.size() - a2.size()),
                                      rt_trim(a2)});
            } else if (starts_with_word(c, "default")) {
                chunks.push_back({true, "default", rt_trim(c.substr(7))});
            }
        }
        if (rt_trim(line).empty()) break;
    }
    // collect_firsttime does else_body (default) BEFORE cases, so bake the
    // default's flags first, then the cases in source order.
    std::vector<std::string> baked(chunks.size());
    for (size_t i = 0; i < chunks.size(); ++i)
        if (chunks[i].is_default) baked[i] = bake_stmt_list(chunks[i].inner, q);
    for (size_t i = 0; i < chunks.size(); ++i)
        if (!chunks[i].is_default) baked[i] = bake_stmt_list(chunks[i].inner, q);
    // Re-emit in source order.
    std::string out = prefix + " {\n";
    for (size_t i = 0; i < chunks.size(); ++i)
        out += chunks[i].head + " {\n" + baked[i] + "\n}\n";
    out += "}";
    return out;
}

// Bake a single (already firsttime/otherwise-stripped) statement.
std::string bake_generic(const std::string &stmt, std::deque<bool> &q) {
    if (starts_with_word(stmt, "switch")) return bake_switch(stmt, q);
    if (is_recursing_keyword(stmt)) {
        // Locate the body the way parse_one_statement does: after the
        // parenthesized parameter for the parameterized forms, after the
        // keyword itself for the parameterless ones. split_block (first '{')
        // would mis-split a one-liner whose own braces get_script already
        // stripped -- the first brace it finds then belongs to a NESTED
        // statement, so the flag stream and the body text both corrupt.
        std::string head, body;
        bool parameterless =
            starts_with_word(stmt, "wait") ||
            stmt.compare(0, 8, "on ready") == 0 ||
            stmt.compare(0, 9, "get input") == 0 ||
            (starts_with_word(stmt, "else") &&
             !starts_with_word(rt_trim(text_after(stmt, "else")), "if"));
        if (parameterless) {
            size_t k = starts_with_word(stmt, "wait")          ? 4
                       : stmt.compare(0, 8, "on ready") == 0   ? 8
                       : stmt.compare(0, 9, "get input") == 0  ? 9
                                                               : 4;  // else
            head = stmt.substr(0, k);
            body = rt_trim(stmt.substr(k));
        } else {
            std::string after;
            bool found = false;
            get_parameter(stmt, after, found);
            if (!found) return stmt;
            head = stmt.substr(0, stmt.size() - after.size());
            body = rt_trim(after);
        }
        if (!body.empty())
            return head + " {\n" + bake_stmt_list(body, q) + "\n}";
    }
    return stmt;  // no descent: emitted verbatim
}

std::string bake_stmt_list(const std::string &inner, std::deque<bool> &q) {
    std::string line = remove_comments(remove_surrounding_braces(inner));
    std::vector<std::string> pieces;

    // A firsttime awaiting its (optional) otherwise continuation.
    bool ft_pending = false, ft_ran = false;
    std::string ft_body;
    auto flush_ft = [&]() {
        if (!ft_pending) return;
        // No otherwise followed: a run firsttime emits nothing, else it stays.
        if (!ft_ran) pieces.push_back("firsttime {\n" + ft_body + "\n}");
        ft_pending = false;
    };

    while (true) {
        std::string after;
        std::string stmt = rt_trim(get_script(line, after));
        line = after;
        if (!stmt.empty()) {
            if (ft_pending && starts_with_word(stmt, "otherwise")) {
                // The body is everything after the keyword, exactly as
                // parse_statements sees it -- get_script has already stripped
                // the braces off a one-line block, so split_block would find
                // no '{' (or a nested statement's) and lose the body.
                std::string other =
                    bake_stmt_list(rt_trim(stmt.substr(9)), q);
                if (ft_ran)
                    pieces.push_back(other);  // just the otherwise body
                else
                    pieces.push_back("firsttime {\n" + ft_body +
                                     "\n} otherwise {\n" + other + "\n}");
                ft_pending = false;
            } else {
                flush_ft();
                if (starts_with_word(stmt, "firsttime")) {
                    ft_ran = !q.empty() && q.front();
                    if (!q.empty()) q.pop_front();
                    // Same as `otherwise`: take the body after the keyword
                    // (bake_stmt_list strips surrounding braces itself), so
                    // one-line bodies survive.
                    ft_body = bake_stmt_list(rt_trim(stmt.substr(9)), q);
                    ft_pending = true;
                } else {
                    pieces.push_back(bake_generic(stmt, q));
                }
            }
        }
        if (rt_trim(line).empty()) break;
    }
    flush_ft();

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

// Serialize one field value onto `w` under attribute `attr`, on an element of
// `elem_type`. A QuestViva saved game emits NO <implied> declarations, so its
// own reload resolves every type-less attribute to string (or boolean when
// empty). It exploits this by writing STRING fields type-less and everything
// else -- ints, booleans, objects, scripts, delegate impls, collections -- with
// an explicit type. We do the same: it is what makes an already-converted
// command <pattern> reload as raw regex instead of being re-run through the
// simplepattern loader (which would mangle it). An EMPTY string keeps
// type="string" -- without it a type-less empty tag reloads as boolean.
// `elem_type` is unused now (kept for symmetry / future need). Skips Null.

// Emit one entry as a `<value ...>` element (QuestViva's FieldSaver.SaveValue):
// a general list's entries and a generic `dictionary`'s item values are typed
// and may themselves be collections (Quest's nested grid dictionaries), so this
// recurses. Unlike a field, a value-position scalar ALWAYS carries its type
// (IsImpliedType's element==null path). The depth cap breaks the infinite
// recursion of a self-referential collection (`list add (l, l)`): levels past
// it truncate to null rather than overflowing the stack.
void write_value(SaveWriter &w, Interp &in, const Value &v, int depth = 0);

void write_field(SaveWriter &w, Interp &in, const World &world,
                 const std::string &elem_type, const std::string &attr,
                 const Value &v) {
    using T = Value::Type;
    (void)world; (void)elem_type;
    bool simple = simple_attr_name(attr);
    std::string tag = simple ? attr : "attr";
    std::vector<std::pair<std::string, std::string>> base;
    if (!simple) base.emplace_back("name", attr);

    // Scalar leaf: strings are type-less (empty string keeps its type so it
    // does not reload as boolean); every other scalar carries its type.
    auto scalar_attrs = [&](const std::string &type, bool empty_text) {
        auto a = base;
        if (type != "string" || empty_text) a.emplace_back("type", type);
        return a;
    };
    // Joined-string containers (objectlist, and the legacy v530- list/dict
    // forms) go through the SCALAR tag rule (WriteAttribute).
    auto with_type = [&](const std::string &type) {
        auto a = base;
        a.emplace_back("type", type);
        return a;
    };
    // v540+ nested containers use the attribute name as the tag directly.
    bool xml_tag = xml_tag_ok(attr);
    std::string ctag = xml_tag ? attr : "attr";
    std::vector<std::pair<std::string, std::string>> cbase;
    if (!xml_tag) cbase.emplace_back("name", attr);
    auto cwith_type = [&](const std::string &type) {
        auto a = cbase;
        a.emplace_back("type", type);
        return a;
    };

    switch (v.type) {
    case T::Null:
        return;  // a removed/never-set attribute is simply absent
    case T::String:
        w.leaf(tag, scalar_attrs("string", v.str.empty()), v.str, true);
        return;
    case T::Script: {
        // A delegate-implementation field keeps the delegate name as its type
        // (DelegateImplementationSaver); a plain script uses type="script".
        std::string type = (!v.declared_type.empty() &&
                             v.declared_type != "script")
                                ? v.declared_type
                                : "script";
        w.leaf(tag, scalar_attrs(type, false), in.bake_firsttime_source(v.str),
               true);
        return;
    }
    case T::Int:
        w.leaf(tag, scalar_attrs("int", false), std::to_string(v.integer), true);
        return;
    case T::Double:
        w.leaf(tag, scalar_attrs("double", false), double_to_str(v.dbl), true);
        return;
    case T::Boolean:
        // QuestViva's BooleanSaver: a TRUE flag with a simple name is a bare
        // <attr/> (no type, no text) -- which reloads as boolean true; a spaced
        // name or a FALSE value writes the value with its (usually explicit)
        // type.
        if (v.boolean && simple) {
            w.leaf(tag, base, "", false);
        } else {
            w.leaf(tag, scalar_attrs("boolean", false), v.boolean ? "true" : "false", true);
        }
        return;
    case T::ObjectRef:
        w.leaf(tag, scalar_attrs("object", false), v.str, true);
        return;
    case T::StringList: {
        // A general typed list (declared_type "list": mixed/boxed entries) is
        // written with per-value types; a homogeneous stringlist uses <value>.
        bool general = v.declared_type == "list" || list_needs_generic(v);
        if (w.version <= 530 && !general) {
            // Legacy: semicolon-joined, type="list".
            std::string joined;
            const auto &l = v.list();
            for (size_t i = 0; i < l.size(); ++i) {
                if (i) joined += "; ";
                joined += l[i].str;
            }
            w.leaf(tag, with_type("list"), joined, true);
            return;
        }
        w.open(ctag, cwith_type(general ? "list" : "stringlist"), false);
        for (const Value &e : v.list()) {
            if (general)
                write_value(w, in, e);        // typed, recursive
            else
                w.leaf("value", {}, e.str, true);  // homogeneous stringlist
        }
        w.close(ctag);
        return;
    }
    case T::ObjectList: {
        // ObjectListSaver: always semicolon-joined element names, both versions.
        std::string joined;
        const auto &l = v.list();
        for (size_t i = 0; i < l.size(); ++i) {
            if (i) joined += "; ";
            joined += l[i].str;
        }
        w.leaf(tag, with_type("objectlist"), joined, true);
        return;
    }
    case T::StringDict:
    case T::ObjectDict: {
        // A generic dictionary (declared_type "dictionary") holds boxed, typed
        // values (recursively serialised); a string/object dictionary holds
        // plain string/object-name values.
        bool generic = v.declared_type == "dictionary" || dict_needs_generic(v);
        const char *tn = generic ? "dictionary"
                         : v.type == T::ObjectDict ? "objectdictionary"
                                                   : "stringdictionary";
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
            w.leaf(tag, with_type(tn), s, true);
            return;
        }
        w.open(ctag, cwith_type(tn), false);
        for (const auto &kv : v.dict()) {
            w.open("item", {}, false);
            w.leaf("key", {}, kv.first, true);
            if (generic) {
                write_value(w, in, kv.second);  // typed, recursive
            } else {
                std::string vs = dict_value_string(kv.second);
                w.leaf("value", {}, vs, !vs.empty());
            }
            w.close("item");
        }
        w.close(ctag);
        return;
    }
    case T::ScriptDict: {
        w.open(ctag, cwith_type("scriptdictionary"), false);
        for (const auto &kv : v.dict()) {
            std::vector<std::pair<std::string, std::string>> ia;
            ia.emplace_back("key", kv.first);
            // Scriptdict entries are compiled scripts like any Script field:
            // bake their run firsttimes or they re-fire after a reload.
            w.leaf("item", ia, in.bake_firsttime_source(kv.second.str), true);
        }
        w.close(ctag);
        return;
    }
    }
}

// See the forward declaration above write_field.
void write_value(SaveWriter &w, Interp &in, const Value &v, int depth) {
    using T = Value::Type;
    auto ty = [](const std::string &t) {
        std::vector<std::pair<std::string, std::string>> a;
        a.emplace_back("type", t);
        return a;
    };
    if (depth > 100) {  // self-referential collection: truncate to null
        w.leaf("value", {}, "", true);
        return;
    }
    switch (v.type) {
    case T::Null:
        w.leaf("value", {}, "", true);
        return;
    case T::String:
        w.leaf("value", ty("string"), v.str, true);
        return;
    case T::Script: {
        std::string t = (!v.declared_type.empty() && v.declared_type != "script")
                            ? v.declared_type
                            : "script";
        w.leaf("value", ty(t), in.bake_firsttime_source(v.str), true);
        return;
    }
    case T::Int:
        w.leaf("value", ty("int"), std::to_string(v.integer), true);
        return;
    case T::Double:
        w.leaf("value", ty("double"), double_to_str(v.dbl), true);
        return;
    case T::Boolean:
        if (v.boolean) w.leaf("value", {}, "", false);       // bare <value/>
        else w.leaf("value", ty("boolean"), "false", true);
        return;
    case T::ObjectRef:
        w.leaf("value", ty("object"), v.str, true);
        return;
    case T::StringList: {
        bool general = v.declared_type == "list" || list_needs_generic(v);
        w.open("value", ty(general ? "list" : "stringlist"), false);
        for (const Value &e : v.list()) {
            if (general) write_value(w, in, e, depth + 1);
            else w.leaf("value", {}, e.str, true);
        }
        w.close("value");
        return;
    }
    case T::ObjectList: {
        std::string joined;
        const auto &l = v.list();
        for (size_t i = 0; i < l.size(); ++i) {
            if (i) joined += "; ";
            joined += l[i].str;
        }
        w.leaf("value", ty("objectlist"), joined, true);
        return;
    }
    case T::StringDict:
    case T::ObjectDict: {
        bool generic = v.declared_type == "dictionary" || dict_needs_generic(v);
        const char *tn = generic ? "dictionary"
                         : v.type == T::ObjectDict ? "objectdictionary"
                                                   : "stringdictionary";
        w.open("value", ty(tn), false);
        for (const auto &kv : v.dict()) {
            w.open("item", {}, false);
            w.leaf("key", {}, kv.first, true);
            if (generic) {
                write_value(w, in, kv.second, depth + 1);
            } else {
                std::string vs = dict_value_string(kv.second);
                w.leaf("value", {}, vs, !vs.empty());
            }
            w.close("item");
        }
        w.close("value");
        return;
    }
    case T::ScriptDict: {
        w.open("value", ty("scriptdictionary"), false);
        for (const auto &kv : v.dict()) {
            std::vector<std::pair<std::string, std::string>> ia;
            ia.emplace_back("key", kv.first);
            // Same as the field-position scriptdictionary: bake run
            // firsttimes out of each entry.
            w.leaf("item", ia, in.bake_firsttime_source(kv.second.str), true);
        }
        w.close("value");
        return;
    }
    }
}

// Common inherit + own-field body of an element.
void write_fields(SaveWriter &w, Interp &in, const World &world,
                  const Element *e) {
    for (const std::string &t : e->inherits) {
        if (is_default_type(t)) continue;
        std::vector<std::pair<std::string, std::string>> a;
        a.emplace_back("name", t);
        w.leaf("inherit", a, "", false);
    }
    for (const auto &kv : e->fields) {
        if (ignore_field(e->elem_type, kv.first)) continue;
        write_field(w, in, world, e->elem_type, kv.first, kv.second);
    }
}

}  // namespace

// Return `src` with already-run firsttime blocks baked out, or unchanged when
// none has run (the common case: ordinary scripts stay verbatim). See the
// firsttime-baking block above for the transform.
std::string Interp::bake_firsttime_source(const std::string &src) {
    auto body = compile_script(src);
    std::vector<std::shared_ptr<bool>> flags;
    collect_firsttime(*body, flags);
    bool any = false;
    for (const auto &f : flags)
        if (*f) { any = true; break; }
    if (!any) return src;
    std::deque<bool> q;
    for (const auto &f : flags) q.push_back(*f);
    return bake_stmt_list(src, q);
}

// ---- writer ---------------------------------------------------------------

std::string Interp::save_game_native(const std::string &original_file) {
    SaveWriter w;
    w.version = world_.asl_version;

    w.out += "<!-- Saved by Geas (native Quest 5 engine) -->\n";
    std::vector<std::pair<std::string, std::string>> asl_attrs;
    asl_attrs.emplace_back(
        "version", world_.version_string.empty() ? std::to_string(world_.asl_version)
                                                  : world_.version_string);
    if (!original_file.empty())
        asl_attrs.emplace_back("original", original_file);
    w.open("asl", asl_attrs, false);

    // Runtime containment is the `parent` field (Element::children is the
    // load-time graph and goes stale on MoveObject). Build a live parent->
    // children map over the registered object family, ordered by sort_index.
    auto live = [&](const Element *e) {
        return world_.find(e->name) == e;
    };
    auto parent_name = [&](const Element *e) -> std::string {
        const Value *p = e->field("parent");
        if (p && p->type == Value::Type::ObjectRef) return p->str;
        return std::string();
    };

    // Which families nest (ObjectSaver) vs. save flat (TimerSaver).
    auto nested_family = [](const std::string &t) {
        return t == "object" || t == "game" || t == "command" ||
               t == "verb" || t == "exit" || t == "turnscript";
    };

    std::map<std::string, std::vector<const Element *>> children;
    std::vector<const Element *> roots;
    for (const auto &up : world_.elements) {
        const Element *e = up.get();
        if (!nested_family(e->elem_type) || !live(e)) continue;
        std::string pn = parent_name(e);
        if (!pn.empty() && world_.find(pn)) children[pn].push_back(e);
        else roots.push_back(e);
    }
    auto by_sort = [](const Element *a, const Element *b) {
        return a->sort_index < b->sort_index;
    };
    std::sort(roots.begin(), roots.end(), by_sort);
    for (auto &kv : children) std::sort(kv.second.begin(), kv.second.end(), by_sort);

    // Element tag + structural attributes (name / game name / exit alias+to).
    // The seen-set guards against a runtime `parent` cycle, which would
    // otherwise recurse forever through the children map.
    std::set<const Element *> emitted;
    std::function<void(const Element *)> emit = [&](const Element *e) {
        if (!emitted.insert(e).second) return;
        const std::string &t = e->elem_type;
        std::string tag = t;  // object/game/command/verb/exit/turnscript
        std::vector<std::pair<std::string, std::string>> attrs;
        if (t == "game") {
            const Value *gn = e->field("gamename");
            attrs.emplace_back("name", gn && gn->type == Value::Type::String
                                               ? gn->str : world_.game_name);
        } else if (t == "exit") {
            attrs.emplace_back("name", e->name);
            const Value *al = e->field("alias");
            if (al && al->type == Value::Type::String && !al->str.empty())
                attrs.emplace_back("alias", al->str);
            const Value *to = e->field("to");
            if (to && to->type == Value::Type::ObjectRef && !to->str.empty())
                attrs.emplace_back("to", to->str);
        } else {
            attrs.emplace_back("name", e->name);
        }
        w.open(tag, attrs, false);
        write_fields(w, *this, world_, e);
        auto it = children.find(e->name);
        if (it != children.end())
            for (const Element *c : it->second) emit(c);
        w.close(tag);
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
        std::string s;
        const Value *p = e->field("paramnames");
        if (p && (p->type == Value::Type::StringList ||
                  p->type == Value::Type::ObjectList)) {
            const auto &l = p->list();
            for (size_t i = 0; i < l.size(); ++i) {
                if (i) s += ", ";
                s += l[i].str;
            }
        }
        return s;
    };

    // NOTE: we deliberately emit NO <implied> declarations, exactly like
    // QuestViva's SavedGame mode. With no implied context on reload, every
    // type-less attribute resolves to string/boolean, which is precisely why
    // already-final values (command patterns as raw regex, etc.) survive a
    // round-trip instead of being re-interpreted.

    // <template>/<dynamictemplate>: scripts are stored post-substitution, so
    // static [refs] are already gone, but the runtime Template()/
    // DynamicTemplate() builtins still look these up by name.
    for (const auto &kv : world_.templates) {
        std::vector<std::pair<std::string, std::string>> a;
        a.emplace_back("name", kv.first);
        w.leaf("template", a, kv.second, true);
    }
    for (const auto &kv : world_.dynamic_templates) {
        std::vector<std::pair<std::string, std::string>> a;
        a.emplace_back("name", kv.first);
        w.leaf("dynamictemplate", a, kv.second, true);
    }
    // <delegate name=".." parameters=".." type="returntype"/>
    for (const auto &up : world_.elements) {
        const Element *e = up.get();
        if (e->kind != ElemKind::Delegate || !live(e)) continue;
        const Value *rt = e->field("returntype");
        std::vector<std::pair<std::string, std::string>> a;
        a.emplace_back("name", e->name);
        a.emplace_back("parameters", join_params(e));
        a.emplace_back("type",
                       rt && rt->type == Value::Type::String ? rt->str : "");
        w.leaf("delegate", a, "", false);
    }

    for (const Element *e : roots) emit(e);

    // Elements in a runtime `parent` cycle sit only in children[], so the
    // walk from roots never reaches them. Emit them as extra roots: the
    // cycle flattens (their parent comes from XML nesting on reload) instead
    // of silently vanishing from the save.
    std::vector<const Element *> cycle_orphans;
    for (const auto &up : world_.elements) {
        const Element *e = up.get();
        if (nested_family(e->elem_type) && live(e) && !emitted.count(e))
            cycle_orphans.push_back(e);
    }
    std::sort(cycle_orphans.begin(), cycle_orphans.end(), by_sort);
    for (const Element *e : cycle_orphans) emit(e);

    // <type name="..">inherits + own fields</type>
    std::vector<const Element *> types;
    for (const auto &up : world_.elements) {
        const Element *e = up.get();
        if (e->kind == ElemKind::Type && live(e)) types.push_back(e);
    }
    std::sort(types.begin(), types.end(), by_sort);
    for (const Element *e : types) {
        std::vector<std::pair<std::string, std::string>> a;
        a.emplace_back("name", e->name);
        w.open("type", a, false);
        write_fields(w, *this, world_, e);
        w.close("type");
    }
    // <function name=".." parameters=".." type="returntype">script</function>
    std::vector<const Element *> funcs;
    for (const auto &up : world_.elements) {
        const Element *e = up.get();
        if (e->kind == ElemKind::Function && live(e)) funcs.push_back(e);
    }
    std::sort(funcs.begin(), funcs.end(), by_sort);
    for (const Element *e : funcs) {
        std::vector<std::pair<std::string, std::string>> a;
        a.emplace_back("name", e->name);
        std::string params = join_params(e);
        if (!params.empty()) a.emplace_back("parameters", params);
        const Value *rt = e->field("returntype");
        if (rt && rt->type == Value::Type::String && !rt->str.empty())
            a.emplace_back("type", rt->str);
        const Value *sc = e->field("script");
        std::string body = sc && sc->type == Value::Type::Script
                               ? bake_firsttime_source(sc->str)
                               : std::string();
        w.leaf("function", a, body, !body.empty());
    }

    // Timers save flat (TimerSaver), after the object graph.
    std::vector<const Element *> timers;
    for (const auto &up : world_.elements) {
        const Element *e = up.get();
        if (e->kind == ElemKind::Timer && live(e)) timers.push_back(e);
    }
    std::sort(timers.begin(), timers.end(), by_sort);
    for (const Element *e : timers) {
        std::vector<std::pair<std::string, std::string>> attrs;
        attrs.emplace_back("name", e->name);
        w.open("timer", attrs, false);
        write_fields(w, *this, world_, e);
        w.close("timer");
    }

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
        head.find("Saved by Geas") != std::string::npos)
        return true;
    // Otherwise: <asl ... original="..."> (a Quest/QuestViva saved game). A
    // plain game .aslx has <asl> with no `original`, so this stays specific.
    size_t asl = head.find("<asl");
    if (asl == std::string::npos) return false;
    size_t gt = head.find('>', asl);
    std::string tag = gt == std::string::npos ? head.substr(asl)
                                              : head.substr(asl, gt - asl);
    return tag.find("original") != std::string::npos;
}

bool Interp::restore_game_native(const std::string &data, std::string &err) {
    // Validate the ASL version matches this (already-reloaded) world.
    {
        const char *p = data.data(), *end = p + data.size();
        size_t off = data.find("<asl");
        if (off == std::string::npos) { err = "not an ASLX save"; return false; }
        p += off;
        size_t gt = data.find('>', off);
        std::string tag = gt == std::string::npos ? std::string(p, end)
                                                  : data.substr(off, gt - off);
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
    std::vector<std::string> drop;
    for (const auto &up : world_.elements) {
        Element *e = up.get();
        if (save_family(e->elem_type) && world_.find(e->name) == e &&
            !overlaid.count(e->name))
            drop.push_back(e->name);
    }
    for (const std::string &n : drop) world_.unregister_name(n);

    // Merged-listextend slots may hold pre-restore state (as in restore_game).
    extend_cache_.clear();
    return true;
}

}  // namespace aslx
