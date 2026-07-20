// aslx-state.inc -- Quest 5 save/restore: the v1 snapshot format (TODO §5).
//
// Included at the end of aslx-runtime.cc (same single-TU layout as the parse
// and builtins units).
//
// Design: a restore RELOADS the original game file into a fresh World/Interp
// and applies this snapshot on top, so only DYNAMIC state is serialized --
// every live element of the mutable family (object / exit / command / verb /
// game / turnscript / timer) with its identity (elem_type, inherits,
// anonymous, sort_index) and own fields as full recursive Values, plus the
// `firsttime` flags (keyed by script source text, preorder within each cached
// script body). Static elements (functions, types, delegates, walkthroughs)
// come back verbatim from the reload and are not recorded; templates likewise.
// Undo history, RNG streams and the error counters reset on restore, exactly
// as they do across QuestViva's save/load (a saved game starts with empty undo
// stacks and fresh per-expression evaluators).
//
// This is deliberately NOT Quest's native save format (a full re-serialization
// of the world to standalone ASLX, GameSaver.cs) -- that is the optional
// native-compat milestone. What IS mirrored from QuestViva is the saved-game
// boot path (WorldModel.BeginInternalAsync with _loadedFromSaved): the host
// must skip StartGame and begin_timers, re-run InitInterface, and print
// "Loaded saved game" (the reference's no-transcript fallback).
//
// Encoding: a line-ish text format with netstring-style length-prefixed blobs
// ("<len>:<bytes>") so script bodies and game text round-trip byte-exactly.

namespace aslx {

namespace {

constexpr const char kSaveMagic[] = "ASLXSAVE 1\n";

void wr_blob(std::string &out, const std::string &s) {
    out += std::to_string(s.size());
    out += ':';
    out += s;
}

void wr_num(std::string &out, long n) {
    out += std::to_string(n);
    out += ';';
}

// Cursor-based readers: advance p, return false on malformed input.
bool rd_num(const char *&p, const char *end, long &n) {
    bool neg = p < end && *p == '-';
    if (neg) ++p;
    if (p >= end || !std::isdigit((unsigned char)*p)) return false;
    long v = 0;
    while (p < end && std::isdigit((unsigned char)*p))
        v = v * 10 + (*p++ - '0');
    if (p >= end || *p != ';') return false;
    ++p;
    n = neg ? -v : v;
    return true;
}

bool rd_blob(const char *&p, const char *end, std::string &s) {
    if (p >= end || !std::isdigit((unsigned char)*p)) return false;
    size_t len = 0;
    while (p < end && std::isdigit((unsigned char)*p))
        len = len * 10 + (size_t)(*p++ - '0');
    if (p >= end || *p != ':') return false;
    ++p;
    if ((size_t)(end - p) < len) return false;
    s.assign(p, len);
    p += len;
    return true;
}

bool rd_char(const char *&p, const char *end, char want) {
    if (p >= end || *p != want) return false;
    ++p;
    return true;
}

// -- Value serialization (recursive; nested collections round-trip) ---------

void wr_value(std::string &out, const Value &v) {
    // Common prefix: type letter, declared_type blob, list_extend flag.
    char t = 'n';
    switch (v.type) {
    case Value::Type::Null:       t = 'n'; break;
    case Value::Type::String:     t = 's'; break;
    case Value::Type::Script:     t = 'c'; break;
    case Value::Type::Int:        t = 'i'; break;
    case Value::Type::Double:     t = 'f'; break;
    case Value::Type::Boolean:    t = 'b'; break;
    case Value::Type::ObjectRef:  t = 'o'; break;
    case Value::Type::StringList: t = 'L'; break;
    case Value::Type::ObjectList: t = 'M'; break;
    case Value::Type::StringDict: t = 'D'; break;
    case Value::Type::ObjectDict: t = 'E'; break;
    case Value::Type::ScriptDict: t = 'F'; break;
    }
    out += t;
    wr_blob(out, v.declared_type);
    out += v.list_extend ? '1' : '0';
    switch (v.type) {
    case Value::Type::Null:
        break;
    case Value::Type::String:
    case Value::Type::Script:
    case Value::Type::ObjectRef:
        wr_blob(out, v.str);
        break;
    case Value::Type::Int:
        wr_num(out, v.integer);
        break;
    case Value::Type::Double: {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.17g", v.dbl);
        wr_blob(out, buf);
        break;
    }
    case Value::Type::Boolean:
        out += v.boolean ? '1' : '0';
        break;
    case Value::Type::StringList:
    case Value::Type::ObjectList:
        wr_num(out, (long)v.list().size());
        for (const Value &e : v.list())
            wr_value(out, e);
        break;
    case Value::Type::StringDict:
    case Value::Type::ObjectDict:
    case Value::Type::ScriptDict:
        wr_num(out, (long)v.dict().size());
        for (const auto &kv : v.dict()) {
            wr_blob(out, kv.first);
            wr_value(out, kv.second);
        }
        break;
    }
}

bool rd_value(const char *&p, const char *end, Value &v) {
    if (p >= end) return false;
    char t = *p++;
    if (!rd_blob(p, end, v.declared_type)) return false;
    if (p >= end) return false;
    v.list_extend = *p++ == '1';
    switch (t) {
    case 'n':
        v.type = Value::Type::Null;
        return true;
    case 's':
    case 'c':
    case 'o':
        v.type = t == 's'   ? Value::Type::String
                 : t == 'c' ? Value::Type::Script
                            : Value::Type::ObjectRef;
        return rd_blob(p, end, v.str);
    case 'i':
        v.type = Value::Type::Int;
        return rd_num(p, end, v.integer);
    case 'f': {
        v.type = Value::Type::Double;
        std::string s;
        if (!rd_blob(p, end, s)) return false;
        v.dbl = std::atof(s.c_str());
        return true;
    }
    case 'b':
        v.type = Value::Type::Boolean;
        if (p >= end) return false;
        v.boolean = *p++ == '1';
        return true;
    case 'L':
    case 'M': {
        v.type = t == 'L' ? Value::Type::StringList : Value::Type::ObjectList;
        v.ensure_backing();
        long n = 0;
        if (!rd_num(p, end, n) || n < 0) return false;
        for (long i = 0; i < n; ++i) {
            Value e;
            if (!rd_value(p, end, e)) return false;
            v.list().push_back(std::move(e));
        }
        return true;
    }
    case 'D':
    case 'E':
    case 'F': {
        v.type = t == 'D'   ? Value::Type::StringDict
                 : t == 'E' ? Value::Type::ObjectDict
                            : Value::Type::ScriptDict;
        v.ensure_backing();
        long n = 0;
        if (!rd_num(p, end, n) || n < 0) return false;
        for (long i = 0; i < n; ++i) {
            std::string key;
            Value e;
            if (!rd_blob(p, end, key) || !rd_value(p, end, e)) return false;
            v.dict().emplace_back(std::move(key), std::move(e));
        }
        return true;
    }
    default:
        return false;
    }
}

// The element family whose state can change at runtime through the script
// layer (field writes need an ObjectRef, and GetObject/AllCommands only hand
// out this family; `create`/`destroy` only make/unmake it). Functions, types,
// delegates and walkthroughs are static and come back from the reload.
bool save_family(const std::string &elem_type) {
    return elem_type == "object" || elem_type == "exit" ||
           elem_type == "command" || elem_type == "verb" ||
           elem_type == "game" || elem_type == "turnscript" ||
           elem_type == "timer";
}

// Preorder walk of a compiled script body collecting every firsttime flag.
// Save and restore both use this, so the bit order is consistent as long as
// the script source text -- the cache key -- is identical, which it is,
// because the snapshot is applied onto a reload of the same game file.
void collect_firsttime(const std::vector<Stmt> &body,
                       std::vector<std::shared_ptr<bool>> &out) {
    for (const Stmt &s : body) {
        if (s.kind == Stmt::Kind::FirstTime && s.ran)
            out.push_back(s.ran);
        collect_firsttime(s.body, out);
        for (const auto &ei : s.elseifs)
            collect_firsttime(ei.second, out);
        collect_firsttime(s.else_body, out);
        for (const auto &c : s.cases)
            collect_firsttime(c.second, out);
    }
}

}  // namespace

bool Interp::is_save_data(const char *data, size_t len) {
    return len >= sizeof kSaveMagic - 1 &&
           std::memcmp(data, kSaveMagic, sizeof kSaveMagic - 1) == 0;
}

std::string Interp::save_game(const std::string &original_file) {
    std::string out = kSaveMagic;
    // Identity: a save only applies to a reload of the same game.
    wr_blob(out, world_.game_name);
    wr_num(out, world_.asl_version);
    wr_blob(out, original_file);
    wr_num(out, world_.next_sort_index);

    // Live mutable-family elements, in storage (creation) order, so a restore
    // recreates runtime-created elements in the same enumeration order.
    long count = 0;
    for (const auto &up : world_.elements)
        if (save_family(up->elem_type) && world_.find(up->name) == up.get())
            ++count;
    wr_num(out, count);
    for (const auto &up : world_.elements) {
        Element *e = up.get();
        if (!save_family(e->elem_type) || world_.find(e->name) != e)
            continue;
        out += 'E';
        wr_blob(out, e->name);
        wr_blob(out, e->elem_type);
        out += e->anonymous ? '1' : '0';
        wr_num(out, e->sort_index);
        wr_num(out, (long)e->inherits.size());
        for (const std::string &t : e->inherits)
            wr_blob(out, t);
        wr_num(out, (long)e->fields.size());
        for (const auto &kv : e->fields) {
            wr_blob(out, kv.first);
            wr_value(out, kv.second);
        }
    }

    // firsttime flags: only cached script bodies where at least one flag is
    // set (a fresh reload starts them all false).
    std::string ft;
    long ft_count = 0;
    for (const auto &kv : script_cache_) {
        std::vector<std::shared_ptr<bool>> flags;
        collect_firsttime(*kv.second, flags);
        bool any = false;
        for (const auto &f : flags)
            if (*f) { any = true; break; }
        if (!any)
            continue;
        ++ft_count;
        wr_blob(ft, kv.first);
        wr_num(ft, (long)flags.size());
        for (const auto &f : flags)
            ft += *f ? '1' : '0';
    }
    wr_num(out, ft_count);
    out += ft;
    out += "END";
    return out;
}

bool Interp::restore_game(const std::string &data, std::string &err) {
    // Native Quest/QuestViva `.quest-save` (ASLX re-serialization): overlay it
    // onto this freshly-reloaded world instead of applying the v1 snapshot.
    if (is_native_save_data(data.data(), data.size()))
        return restore_game_native(data, err);

    const char *p = data.data();
    const char *end = p + data.size();
    if (!is_save_data(p, data.size())) {
        err = "not an aslx save file";
        return false;
    }
    p += sizeof kSaveMagic - 1;

    std::string game_name, original;
    long version = 0, next_sort = 0, elem_count = 0;
    if (!rd_blob(p, end, game_name) || !rd_num(p, end, version) ||
        !rd_blob(p, end, original) || !rd_num(p, end, next_sort) ||
        !rd_num(p, end, elem_count) || elem_count < 0) {
        err = "malformed save header";
        return false;
    }
    if (game_name != world_.game_name || version != world_.asl_version) {
        err = "this save belongs to a different game";
        return false;
    }

    // Parse everything up front so a malformed save leaves the world alone.
    struct SavedElement {
        std::string name, elem_type;
        bool anonymous = false;
        long sort_index = 0;
        std::vector<std::string> inherits;
        std::vector<std::pair<std::string, Value>> fields;
    };
    std::vector<SavedElement> saved((size_t)elem_count);
    for (SavedElement &se : saved) {
        long n = 0;
        if (!rd_char(p, end, 'E') || !rd_blob(p, end, se.name) ||
            !rd_blob(p, end, se.elem_type) || p >= end) {
            err = "malformed save element";
            return false;
        }
        se.anonymous = *p++ == '1';
        if (!rd_num(p, end, se.sort_index) || !rd_num(p, end, n) || n < 0) {
            err = "malformed save element";
            return false;
        }
        se.inherits.resize((size_t)n);
        for (std::string &t : se.inherits)
            if (!rd_blob(p, end, t)) { err = "malformed save element"; return false; }
        if (!rd_num(p, end, n) || n < 0) { err = "malformed save element"; return false; }
        se.fields.resize((size_t)n);
        for (auto &kv : se.fields)
            if (!rd_blob(p, end, kv.first) || !rd_value(p, end, kv.second)) {
                err = "malformed save field";
                return false;
            }
    }
    long ft_count = 0;
    if (!rd_num(p, end, ft_count) || ft_count < 0) {
        err = "malformed save (firsttime section)";
        return false;
    }
    std::vector<std::pair<std::string, std::vector<bool>>> fts((size_t)ft_count);
    for (auto &ft : fts) {
        long n = 0;
        if (!rd_blob(p, end, ft.first) || !rd_num(p, end, n) || n < 0 ||
            end - p < n) {
            err = "malformed save (firsttime section)";
            return false;
        }
        ft.second.resize((size_t)n);
        for (long i = 0; i < n; ++i)
            ft.second[(size_t)i] = *p++ == '1';
    }
    if (end - p < 3 || std::memcmp(p, "END", 3) != 0) {
        err = "malformed save (truncated)";
        return false;
    }

    // Apply. First unregister every live mutable-family element the save does
    // not know (it was destroyed before saving) ...
    std::set<std::string> in_save;
    for (const SavedElement &se : saved)
        in_save.insert(se.name);
    std::vector<std::string> drop;
    for (const auto &up : world_.elements)
        if (save_family(up->elem_type) && world_.find(up->name) == up.get() &&
            !in_save.count(up->name))
            drop.push_back(up->name);
    for (const std::string &n : drop)
        world_.unregister_name(n);

    // ... then bring each saved element to its saved state: identity, type
    // stack and the own-field bag replaced wholesale (the saved fields include
    // name/type/elementtype, so a plain clear+set round-trips them).
    for (SavedElement &se : saved) {
        Element *e = world_.find(se.name);
        if (!e)  // runtime-created before saving
            e = world_.create_object(se.name, "", se.elem_type);
        e->elem_type = se.elem_type;
        e->kind = elem_kind_from_string(se.elem_type);
        e->anonymous = se.anonymous;
        e->sort_index = se.sort_index;
        e->inherits = se.inherits;
        e->clear_all_fields();
        for (auto &kv : se.fields)
            e->set_field(kv.first, std::move(kv.second));
    }
    world_.next_sort_index = next_sort;
    world_.note_containment_change();  // containment fully rewritten by restore

    // firsttime flags: compiling the source re-populates the cache slot the
    // save keyed on; a body that no longer parses identically is skipped.
    for (auto &ft : fts) {
        auto body = compile_script(ft.first);
        if (!body)
            continue;
        std::vector<std::shared_ptr<bool>> flags;
        collect_firsttime(*body, flags);
        for (size_t i = 0; i < flags.size() && i < ft.second.size(); ++i)
            *flags[i] = ft.second[i];
    }

    // Per-(element,attr) merged-listextend slots may hold pre-restore state.
    extend_cache_.clear();
    return true;
}

}  // namespace aslx
