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

// aslx-state.cc -- Quest 5 save/restore: the v1 snapshot format (TODO §5).
//
// Design: a restore RELOADS the original game file into a fresh World/Interp
// and applies this snapshot on top, so only DYNAMIC state is serialized --
// every live element of the mutable family (object / exit / command / verb /
// game / turnscript / timer) with its identity (elem_type, inherits,
// anonymous, sort_index) and own fields as full recursive Values, plus the
// `firsttime` flags (keyed by the script cache key -- "<scope>\x1F<source>"
// for a script run from an owning attribute, the bare source text otherwise;
// preorder within each cached script body). A save from before scoping keys
// every body by source alone; restore seeds legacy_firsttime_ from those so
// each scoped instance compiled later starts with the same flags. Static
// elements (functions, types, delegates, walkthroughs)
// come back verbatim from the reload and are not recorded; templates likewise.
// Undo history, RNG streams and the error counters reset on restore, exactly
// as they do across QuestViva's save/load (a saved game starts with empty undo
// stacks and fresh per-expression evaluators).  The Spatterlight autosave
// carries the first two separately -- capture_rng_streams, and
// capture_undo_history at the bottom of this file.
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

#include "aslx-runtime-internal.hh"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

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
    int digits = 0;
    while (p < end && std::isdigit((unsigned char)*p)) {
        if (++digits > 18) return false;  // would overflow long
        v = v * 10 + (*p++ - '0');
    }
    if (p >= end || *p != ';') return false;
    ++p;
    n = neg ? -v : v;
    return true;
}

bool rd_blob(const char *&p, const char *end, std::string &s) {
    if (p >= end || !std::isdigit((unsigned char)*p)) return false;
    size_t len = 0;
    int digits = 0;
    while (p < end && std::isdigit((unsigned char)*p)) {
        if (++digits > 18) return false;  // would overflow size_t
        len = len * 10 + (size_t)(*p++ - '0');
    }
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

void wr_value(std::string &out, const Value &v, int depth = 0) {
    // A self-referential collection (`list add (l, l)`) would recurse
    // forever; truncate levels past the cap to null.
    if (depth > 100) {
        out += 'n';
        wr_blob(out, std::string());
        out += '0';
        return;
    }
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
    case Value::Type::Double:
        wr_blob(out, c_format_g(v.dbl, 17));
        break;
    case Value::Type::Boolean:
        out += v.boolean ? '1' : '0';
        break;
    case Value::Type::StringList:
    case Value::Type::ObjectList:
        wr_num(out, (long)v.list().size());
        for (const Value &e : v.list())
            wr_value(out, e, depth + 1);
        break;
    case Value::Type::StringDict:
    case Value::Type::ObjectDict:
    case Value::Type::ScriptDict:
        wr_num(out, (long)v.dict().size());
        for (const auto &kv : v.dict()) {
            wr_blob(out, kv.first);
            wr_value(out, kv.second, depth + 1);
        }
        break;
    }
}

bool rd_value(const char *&p, const char *end, Value &v, int depth = 0) {
    // ~6 bytes encode one nesting level, so an unbounded recursion would let
    // a small hostile save overflow the stack; no real game nests near this.
    if (depth > 1000) return false;
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
        v.dbl = c_strtod(s.c_str());
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
            if (!rd_value(p, end, e, depth + 1)) return false;
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
            if (!rd_blob(p, end, key) || !rd_value(p, end, e, depth + 1))
                return false;
            v.dict().emplace_back(std::move(key), std::move(e));
        }
        return true;
    }
    default:
        return false;
    }
}

}  // namespace

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

void drop_unsaved_elements(World &w, const std::set<std::string> &keep) {
    std::vector<std::string> drop;
    for (const auto &up : w.elements)
        if (is_saved_element(*up) && !keep.count(up->name))
            drop.push_back(up->name);
    for (const std::string &n : drop)
        w.unregister_name(n);
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

bool any_firsttime_ran(const std::vector<std::shared_ptr<bool>> &flags) {
    for (const auto &f : flags)
        if (*f) return true;
    return false;
}

void apply_firsttime(const std::vector<Stmt> &body,
                     const std::vector<bool> &bits) {
    std::vector<std::shared_ptr<bool>> flags;
    collect_firsttime(body, flags);
    for (size_t i = 0; i < flags.size() && i < bits.size(); ++i)
        *flags[i] = bits[i];
}

// Preorder walk of a compiled script body collecting every expression ROOT
// (each carries its own lazily-seeded RNG stream, see Expr::rng).  The
// statement parser compiles these with compile_expr_str, outside expr_cache_,
// so the autosave's RNG capture has to reach them through the script cache;
// the ordinal in this walk is the stream's identity within its script key.
// Same order on capture and restore, because both walk a compile of the same
// source text.
void collect_expr_roots(std::vector<Stmt> &body, std::vector<ExprP> &out) {
    for (Stmt &s : body) {
        if (s.expr) out.push_back(s.expr);
        if (s.obj) out.push_back(s.obj);
        if (s.from) out.push_back(s.from);
        if (s.to) out.push_back(s.to);
        if (s.step) out.push_back(s.step);
        if (s.list) out.push_back(s.list);
        for (ExprP &a : s.call_args)
            if (a) out.push_back(a);
        collect_expr_roots(s.body, out);
        for (auto &ei : s.elseifs) {
            if (ei.first) out.push_back(ei.first);
            collect_expr_roots(ei.second, out);
        }
        collect_expr_roots(s.else_body, out);
        for (auto &c : s.cases) {
            for (ExprP &m : c.first)
                if (m) out.push_back(m);
            collect_expr_roots(c.second, out);
        }
    }
}

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
    std::vector<const Element *> saved;
    for (const auto &up : world_.elements)
        if (is_saved_element(*up)) saved.push_back(up.get());
    wr_num(out, (long)saved.size());
    for (const Element *e : saved) {
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
        if (!any_firsttime_ran(flags))
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

bool Interp::restore_game(const std::string &data, std::string &err) try {
    // Native Quest/QuestViva `.quest-save` (ASLX re-serialization): overlay it
    // onto this freshly-reloaded world instead of applying the v1 snapshot.
    if (is_native_save_data(data.data(), data.size()))
        return restore_game_native(data, err);

    auto fail = [&err](const char *why) {
        err = why;
        return false;
    };
    const char *p = data.data();
    const char *end = p + data.size();
    if (!is_save_data(p, data.size()))
        return fail("not an aslx save file");
    p += sizeof kSaveMagic - 1;

    std::string game_name, original;
    long version = 0, next_sort = 0, elem_count = 0;
    if (!rd_blob(p, end, game_name) || !rd_num(p, end, version) ||
        !rd_blob(p, end, original) || !rd_num(p, end, next_sort) ||
        !rd_num(p, end, elem_count) || elem_count < 0)
        return fail("malformed save header");
    if (game_name != world_.game_name || version != world_.asl_version)
        return fail("this save belongs to a different game");

    // Parse everything up front so a malformed save leaves the world alone.
    struct SavedElement {
        std::string name, elem_type;
        bool anonymous = false;
        long sort_index = 0;
        std::vector<std::string> inherits;
        std::vector<std::pair<std::string, Value>> fields;
    };
    // Counts come from the file: never preallocate from them (a tiny hostile
    // save can claim billions). Each element/inherit/field costs input bytes,
    // so growing as items actually parse keeps memory proportional to the
    // save's real size; the `> end - p` checks reject absurd counts up front.
    if (elem_count > end - p)
        return fail("malformed save header");
    std::vector<SavedElement> saved;
    for (long ei = 0; ei < elem_count; ++ei) {
        saved.emplace_back();
        SavedElement &se = saved.back();
        long n = 0;
        if (!rd_char(p, end, 'E') || !rd_blob(p, end, se.name) ||
            !rd_blob(p, end, se.elem_type) || p >= end)
            return fail("malformed save element");
        se.anonymous = *p++ == '1';
        if (!rd_num(p, end, se.sort_index) || !rd_num(p, end, n) || n < 0 ||
            n > end - p)
            return fail("malformed save element");
        for (long i = 0; i < n; ++i) {
            std::string t;
            if (!rd_blob(p, end, t)) return fail("malformed save element");
            se.inherits.push_back(std::move(t));
        }
        if (!rd_num(p, end, n) || n < 0 || n > end - p)
            return fail("malformed save element");
        for (long i = 0; i < n; ++i) {
            std::string key;
            Value val;
            if (!rd_blob(p, end, key) || !rd_value(p, end, val))
                return fail("malformed save field");
            se.fields.emplace_back(std::move(key), std::move(val));
        }
        // The writer only ever emits the save family; anything else is a
        // crafted save trying to overwrite a static element (function, type).
        if (!save_family(se.elem_type))
            return fail("malformed save element");
    }
    long ft_count = 0;
    if (!rd_num(p, end, ft_count) || ft_count < 0 || ft_count > end - p)
        return fail("malformed save (firsttime section)");
    std::vector<std::pair<std::string, std::vector<bool>>> fts;
    for (long fi = 0; fi < ft_count; ++fi) {
        fts.emplace_back();
        auto &ft = fts.back();
        long n = 0;
        if (!rd_blob(p, end, ft.first) || !rd_num(p, end, n) || n < 0 ||
            end - p < n)
            return fail("malformed save (firsttime section)");
        ft.second.resize((size_t)n);
        for (long i = 0; i < n; ++i)
            ft.second[(size_t)i] = *p++ == '1';
    }
    if (end - p < 3 || std::memcmp(p, "END", 3) != 0)
        return fail("malformed save (truncated)");

    // The drop phase below filters on save_family; the apply phase must too,
    // or a save naming an existing static element (function, type, ...) would
    // overwrite it. Validate before mutating anything.
    for (const SavedElement &se : saved) {
        Element *e = world_.find(se.name);
        if (e && !save_family(e->elem_type))
            return fail("save element collides with a static element");
    }

    // Apply. First unregister every live mutable-family element the save does
    // not know (it was destroyed before saving) ...
    std::set<std::string> in_save;
    for (const SavedElement &se : saved)
        in_save.insert(se.name);
    drop_unsaved_elements(world_, in_save);

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
    // An unscoped key (pre-scoping save, or an `invoke`d script) also seeds
    // legacy_firsttime_, applied to every scoped instance of that source --
    // the ones already compiled here and now, and (in compile_script) the
    // ones compiled later.
    legacy_firsttime_.clear();
    for (auto &ft : fts) {
        std::string scope, src;
        split_script_cache_key(ft.first, scope, src);
        auto body = compile_script(src, scope);
        if (!body)
            continue;
        apply_firsttime(*body, ft.second);
        if (scope.empty()) {
            legacy_firsttime_[src] = ft.second;
            const std::string tail = '\x1F' + src;
            for (auto &kv : script_cache_) {
                if (kv.first.size() <= tail.size() ||
                    kv.first.compare(kv.first.size() - tail.size(), tail.size(),
                                     tail) != 0)
                    continue;
                apply_firsttime(*kv.second, ft.second);
            }
        }
    }

    // Per-(element,attr) merged-listextend slots may hold pre-restore state.
    extend_cache_.clear();
    return true;
} catch (const std::bad_alloc &) {
    // Backstop for save data that still reaches the allocator: fail the
    // restore cleanly instead of aborting the host.
    err = "out of memory restoring save";
    return false;
} catch (const std::length_error &) {
    err = "out of memory restoring save";
    return false;
}

// -- undo history (Spatterlight autosave) ------------------------------------
//
// The logger's records are full of references: a list action holds the list
// it changed, an old field value shares its backing with the actions logged
// after it, a destroyed element is kept alive by the action that can bring it
// back, a firsttime action points at the flag inside a compiled script.  The
// capture therefore writes identities, not copies.  Every list or dictionary
// backing gets a number the first time it is met.  One that the world still
// holds is written as an ALIAS -- the element, the attribute and the
// positions leading to it -- and looked up again in the restored world, so an
// undo after a relaunch changes the list the game is using; any other is
// written in full where it is first met and by number afterwards.  Destroyed
// elements work the same way, and a firsttime flag is named by its script's
// cache key and its position in that script, like the flags in a save.
//
// Layout, after the magic:
//   logging flag
//   aliases       count, then  kind(L|D) id element attr depth position*
//   transactions  count, then  description previous(-1 = none) count action*
//   undo stack    count, then  transaction index*
//   current       transaction index (-1 = none)
//   END
// A value is written like a save's (type, declared_type, list_extend,
// payload) except that a collection's payload is `R id` (already defined, or
// an alias) or `N id count entry*`.

namespace {

constexpr const char kUndoMagic[] = "ASLXUNDO 1\n";

bool is_list_type(Value::Type t) {
    return t == Value::Type::StringList || t == Value::Type::ObjectList;
}
bool is_dict_type(Value::Type t) {
    return t == Value::Type::StringDict || t == Value::Type::ObjectDict ||
           t == Value::Type::ScriptDict;
}
char value_type_char(Value::Type t) {
    switch (t) {
    case Value::Type::Null:       return 'n';
    case Value::Type::String:     return 's';
    case Value::Type::Script:     return 'c';
    case Value::Type::Int:        return 'i';
    case Value::Type::Double:     return 'f';
    case Value::Type::Boolean:    return 'b';
    case Value::Type::ObjectRef:  return 'o';
    case Value::Type::StringList: return 'L';
    case Value::Type::ObjectList: return 'M';
    case Value::Type::StringDict: return 'D';
    case Value::Type::ObjectDict: return 'E';
    case Value::Type::ScriptDict: return 'F';
    }
    return 'n';
}

// Where the world holds a backing: an own field of a live element, then the
// positions down through nested collections.
struct LivePath {
    const Element *elem = nullptr;
    const std::string *attr = nullptr;
    std::vector<long> path;
};

struct UndoWriter {
    World &world;
    std::unordered_map<const void *, LivePath> live;
    std::unordered_map<const void *, long> ids;
    std::unordered_map<const Element *, long> detached;
    long next_id = 0;
    std::string aliases;
    long alias_count = 0;

    explicit UndoWriter(World &w) : world(w) {
        std::vector<long> path;
        for (const auto &up : world.elements) {
            if (!up->registered) continue;
            for (const auto &kv : up->fields)
                walk_live(up.get(), kv.first, kv.second, path);
        }
    }

    void walk_live(const Element *e, const std::string &attr, const Value &v,
                   std::vector<long> &path) {
        // Deeper than a save writes (see wr_value) there is nothing to find
        // after the restore.  A backing met twice keeps its first path, which
        // also stops a self-referential collection.
        if (path.size() > 100) return;
        const void *key = is_list_type(v.type)   ? (const void *)v.list_store.get()
                          : is_dict_type(v.type) ? (const void *)v.dict_store.get()
                                                 : nullptr;
        if (!key || live.count(key)) return;
        LivePath &lp = live[key];
        lp.elem = e;
        lp.attr = &attr;
        lp.path = path;
        long i = 0;
        if (is_list_type(v.type)) {
            for (const Value &entry : *v.list_store) {
                path.push_back(i++);
                walk_live(e, attr, entry, path);
                path.pop_back();
            }
        } else {
            for (const auto &entry : *v.dict_store) {
                path.push_back(i++);
                walk_live(e, attr, entry.second, path);
                path.pop_back();
            }
        }
    }

    // The number of a backing, and whether its contents still have to be
    // written (false: seen before, or an alias into the world).
    long backing_id(const void *key, bool is_list, bool &define) {
        auto it = ids.find(key);
        if (it != ids.end()) {
            define = false;
            return it->second;
        }
        long id = next_id++;
        ids.emplace(key, id);
        auto lv = live.find(key);
        define = lv == live.end();
        if (!define) {
            const LivePath &lp = lv->second;
            ++alias_count;
            aliases += is_list ? 'L' : 'D';
            wr_num(aliases, id);
            wr_blob(aliases, lp.elem->name);
            wr_blob(aliases, *lp.attr);
            wr_num(aliases, (long)lp.path.size());
            for (long at : lp.path)
                wr_num(aliases, at);
        }
        return id;
    }

    void value(std::string &out, const Value &v, int depth = 0) {
        const bool list = is_list_type(v.type), dict = is_dict_type(v.type);
        const void *key = list   ? (const void *)v.list_store.get()
                          : dict ? (const void *)v.dict_store.get()
                                 : nullptr;
        // A collection with no backing yet is an empty one of its own.
        if (depth > 100 || ((list || dict) && !key)) {
            out += depth > 100 ? 'n' : value_type_char(v.type);
            wr_blob(out, depth > 100 ? std::string() : v.declared_type);
            out += v.list_extend && depth <= 100 ? '1' : '0';
            if (depth <= 100) {
                out += 'N';
                wr_num(out, next_id++);
                wr_num(out, 0);
            }
            return;
        }
        if (!list && !dict) {
            wr_value(out, v);
            return;
        }
        out += value_type_char(v.type);
        wr_blob(out, v.declared_type);
        out += v.list_extend ? '1' : '0';
        bool define = false;
        long id = backing_id(key, list, define);
        out += define ? 'N' : 'R';
        wr_num(out, id);
        if (!define) return;
        if (list) {
            wr_num(out, (long)v.list_store->size());
            for (const Value &entry : *v.list_store)
                value(out, entry, depth + 1);
        } else {
            wr_num(out, (long)v.dict_store->size());
            for (const auto &entry : *v.dict_store) {
                wr_blob(out, entry.first);
                value(out, entry.second, depth + 1);
            }
        }
    }

    // The storage a Destroy action brings back: `l` when it is the live
    // element of that name (the destroy was undone since), else its contents
    // the first time (`d`) and its number afterwards (`r`).
    void element(std::string &out, const std::string &name, const Element *e) {
        if (!e) {
            out += '-';
            return;
        }
        if (world.find(name) == e) {
            out += 'l';
            return;
        }
        auto it = detached.find(e);
        if (it != detached.end()) {
            out += 'r';
            wr_num(out, it->second);
            return;
        }
        long id = (long)detached.size();
        detached.emplace(e, id);
        out += 'd';
        wr_num(out, id);
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
            value(out, kv.second);
        }
    }
};

struct UndoReader {
    World &world;
    const char *p;
    const char *end;
    std::unordered_map<long, std::shared_ptr<ValueList>> lists;
    std::unordered_map<long, std::shared_ptr<ValueDict>> dicts;
    // Destroyed elements read so far; handed to the world only once the whole
    // history has parsed.
    std::vector<std::unique_ptr<Element>> detached;
    std::unordered_map<long, Element *> detached_by_id;

    bool num(long &n) { return rd_num(p, end, n); }
    bool blob(std::string &s) { return rd_blob(p, end, s); }
    bool flag(bool &b) {
        if (p >= end || (*p != '0' && *p != '1')) return false;
        b = *p++ == '1';
        return true;
    }
    // A count of things that each take at least one byte of input.
    bool count(long &n) { return num(n) && n >= 0 && n <= end - p; }

    // The world's backing at an alias, or null when the world no longer has
    // that shape (the caller then makes an empty one: the undo that needed it
    // changes nothing the game can see, as it would for a stale reference).
    const Value *resolve(const std::string &elem, const std::string &attr,
                         const std::vector<long> &path) {
        Element *e = world.find(elem);
        const Value *v = e ? e->field(attr) : nullptr;
        for (long at : path) {
            if (!v || at < 0) return nullptr;
            if (is_list_type(v->type) && v->list_store &&
                (size_t)at < v->list_store->size())
                v = &(*v->list_store)[(size_t)at];
            else if (is_dict_type(v->type) && v->dict_store &&
                     (size_t)at < v->dict_store->size())
                v = &(*v->dict_store)[(size_t)at].second;
            else
                return nullptr;
        }
        return v;
    }

    bool alias() {
        if (p >= end) return false;
        char kind = *p++;
        long id = 0, depth = 0;
        std::string elem, attr;
        if ((kind != 'L' && kind != 'D') || !num(id) || !blob(elem) ||
            !blob(attr) || !count(depth))
            return false;
        std::vector<long> path;
        for (long i = 0; i < depth; ++i) {
            long at = 0;
            if (!num(at)) return false;
            path.push_back(at);
        }
        const Value *v = resolve(elem, attr, path);
        if (kind == 'L') {
            auto &slot = lists[id];
            if (v && is_list_type(v->type)) slot = v->list_store;
            if (!slot) slot = std::make_shared<ValueList>();
            slot->undo_logged = true;
        } else {
            auto &slot = dicts[id];
            if (v && is_dict_type(v->type)) slot = v->dict_store;
            if (!slot) slot = std::make_shared<ValueDict>();
            slot->undo_logged = true;
        }
        return true;
    }

    bool value(Value &v, int depth = 0) {
        if (depth > 1000 || p >= end) return false;
        char t = *p;
        if (t != 'L' && t != 'M' && t != 'D' && t != 'E' && t != 'F')
            return rd_value(p, end, v);
        ++p;
        const bool list = t == 'L' || t == 'M';
        v.type = t == 'L'   ? Value::Type::StringList
                 : t == 'M' ? Value::Type::ObjectList
                 : t == 'D' ? Value::Type::StringDict
                 : t == 'E' ? Value::Type::ObjectDict
                            : Value::Type::ScriptDict;
        long id = 0, n = 0;
        if (!blob(v.declared_type) || !flag(v.list_extend) || p >= end)
            return false;
        char mode = *p++;
        if ((mode != 'N' && mode != 'R') || !num(id)) return false;
        if (mode == 'R') {
            if (list) {
                auto it = lists.find(id);
                if (it == lists.end()) return false;
                v.list_store = it->second;
            } else {
                auto it = dicts.find(id);
                if (it == dicts.end()) return false;
                v.dict_store = it->second;
            }
            return true;
        }
        if (!count(n)) return false;
        if (list) {
            // Registered before its entries are read: one of them may be the
            // list itself.
            if (lists.count(id)) return false;
            auto store = std::make_shared<ValueList>();
            store->undo_logged = true;   // everything in the log had the logger
            lists[id] = store;
            v.list_store = store;
            for (long i = 0; i < n; ++i) {
                Value entry;
                if (!value(entry, depth + 1)) return false;
                store->push_back(std::move(entry));
            }
        } else {
            if (dicts.count(id)) return false;
            auto store = std::make_shared<ValueDict>();
            store->undo_logged = true;
            dicts[id] = store;
            v.dict_store = store;
            for (long i = 0; i < n; ++i) {
                std::string key;
                Value entry;
                if (!blob(key) || !value(entry, depth + 1)) return false;
                store->emplace_back(std::move(key), std::move(entry));
            }
        }
        return true;
    }

    bool element(const std::string &name, Element *&out) {
        out = nullptr;
        if (p >= end) return false;
        char mode = *p++;
        long id = 0, n = 0;
        switch (mode) {
        case '-':
            return true;
        case 'l':
            out = world.find(name);
            return true;
        case 'r': {
            if (!num(id)) return false;
            auto it = detached_by_id.find(id);
            if (it == detached_by_id.end()) return false;
            out = it->second;
            return true;
        }
        case 'd': {
            auto e = std::make_unique<Element>();
            if (!num(id) || detached_by_id.count(id) || !blob(e->name) ||
                !blob(e->elem_type) || !flag(e->anonymous) ||
                !num(e->sort_index) || !count(n))
                return false;
            // Only the save family is ever created or destroyed at runtime.
            if (!save_family(e->elem_type)) return false;
            e->kind = elem_kind_from_string(e->elem_type);
            for (long i = 0; i < n; ++i) {
                std::string t;
                if (!blob(t)) return false;
                e->inherits.push_back(std::move(t));
            }
            if (!count(n)) return false;
            out = e.get();
            detached_by_id[id] = out;
            detached.push_back(std::move(e));
            for (long i = 0; i < n; ++i) {
                std::string key;
                Value val;
                if (!blob(key) || !value(val)) return false;
                out->set_field(key, std::move(val));
            }
            return true;
        }
        default:
            return false;
        }
    }
};

}  // namespace

std::string Interp::capture_undo_history() {
    // The transactions worth keeping: the newest of the stack, the open one,
    // and the chain behind it (which rollback_transaction steps back along).
    std::vector<std::shared_ptr<UndoTransaction>> txns;
    std::unordered_map<const UndoTransaction *, long> index;
    auto keep = [&](const std::shared_ptr<UndoTransaction> &t) {
        if (index.emplace(t.get(), (long)txns.size()).second)
            txns.push_back(t);
    };
    const size_t first = undo_stack_.size() > kUndoHistoryKept
                             ? undo_stack_.size() - kUndoHistoryKept : 0;
    for (size_t i = first; i < undo_stack_.size(); ++i)
        keep(undo_stack_[i]);
    size_t steps = 0;
    for (std::shared_ptr<UndoTransaction> t = current_txn_;
         t && steps <= kUndoHistoryKept; t = t->previous, ++steps)
        keep(t);
    auto index_of = [&](const std::shared_ptr<UndoTransaction> &t) {
        auto it = t ? index.find(t.get()) : index.end();
        return it == index.end() ? -1L : it->second;
    };

    UndoWriter w(world_);
    // firsttime flag -> (script cache key, position); built on first need.
    std::unordered_map<const bool *, std::pair<const std::string *, long>> flags;
    bool flags_built = false;

    std::string body;
    wr_num(body, (long)txns.size());
    for (const auto &t : txns) {
        wr_blob(body, t->description);
        wr_num(body, index_of(t->previous));
        wr_num(body, (long)t->actions.size());
        for (const UndoAction &a : t->actions) {
            body += (char)('a' + (int)a.kind);
            wr_blob(body, a.element);
            wr_blob(body, a.attr);
            body += a.added ? '1' : '0';
            wr_num(body, a.index);
            w.value(body, a.old_value);
            switch (a.kind) {
            case UndoAction::Kind::ListAdd:
            case UndoAction::Kind::ListRemove: {
                Value holder;
                holder.type = Value::Type::StringList;
                holder.list_store = a.list_backing;
                w.value(body, holder);
                break;
            }
            case UndoAction::Kind::DictAdd:
            case UndoAction::Kind::DictRemove: {
                Value holder;
                holder.type = Value::Type::StringDict;
                holder.dict_store = a.dict_backing;
                w.value(body, holder);
                break;
            }
            case UndoAction::Kind::Destroy:
                w.element(body, a.element, a.element_ptr);
                break;
            case UndoAction::Kind::FirstTime: {
                if (!flags_built) {
                    flags_built = true;
                    for (const auto &kv : script_cache_) {
                        std::vector<std::shared_ptr<bool>> in_body;
                        collect_firsttime(*kv.second, in_body);
                        for (size_t i = 0; i < in_body.size(); ++i)
                            flags.emplace(in_body[i].get(),
                                          std::make_pair(&kv.first, (long)i));
                    }
                }
                auto it = flags.find(a.ran.get());
                if (it == flags.end()) {
                    body += '-';
                } else {
                    body += 'f';
                    wr_blob(body, *it->second.first);
                    wr_num(body, it->second.second);
                }
                break;
            }
            default:
                break;
            }
        }
    }
    wr_num(body, (long)(undo_stack_.size() - first));
    for (size_t i = first; i < undo_stack_.size(); ++i)
        wr_num(body, index_of(undo_stack_[i]));
    wr_num(body, index_of(current_txn_));

    std::string out = kUndoMagic;
    out += undo_logging_ ? '1' : '0';
    wr_num(out, w.alias_count);
    out += w.aliases;
    out += body;
    out += "END";
    return out;
}

bool Interp::restore_undo_history(const std::string &data) try {
    const size_t magic = sizeof kUndoMagic - 1;
    if (data.size() < magic || std::memcmp(data.data(), kUndoMagic, magic) != 0)
        return false;
    UndoReader r{world_, data.data() + magic, data.data() + data.size(),
                 {}, {}, {}, {}};
    bool logging = false;
    long n = 0;
    if (!r.flag(logging) || !r.count(n)) return false;
    for (long i = 0; i < n; ++i)
        if (!r.alias()) return false;

    long txn_count = 0;
    if (!r.count(txn_count)) return false;
    std::vector<std::shared_ptr<UndoTransaction>> txns;
    std::vector<long> previous;
    for (long ti = 0; ti < txn_count; ++ti) {
        auto t = std::make_shared<UndoTransaction>();
        long prev = 0, actions = 0;
        if (!r.blob(t->description) || !r.num(prev) || prev < -1 ||
            prev >= txn_count || !r.count(actions))
            return false;
        for (long ai = 0; ai < actions; ++ai) {
            if (r.p >= r.end) return false;
            int kind = *r.p++ - 'a';
            if (kind < 0 || kind > (int)UndoAction::Kind::FirstTime)
                return false;
            UndoAction a((UndoAction::Kind)kind);
            if (!r.blob(a.element) || !r.blob(a.attr) || !r.flag(a.added) ||
                !r.num(a.index) || !r.value(a.old_value))
                return false;
            switch (a.kind) {
            case UndoAction::Kind::ListAdd:
            case UndoAction::Kind::ListRemove: {
                Value holder;
                if (!r.value(holder) || !holder.list_store) return false;
                a.list_backing = holder.list_store;
                break;
            }
            case UndoAction::Kind::DictAdd:
            case UndoAction::Kind::DictRemove: {
                Value holder;
                if (!r.value(holder) || !holder.dict_store) return false;
                a.dict_backing = holder.dict_store;
                break;
            }
            case UndoAction::Kind::Destroy:
                if (!r.element(a.element, a.element_ptr)) return false;
                break;
            case UndoAction::Kind::FirstTime: {
                if (r.p >= r.end) return false;
                char mode = *r.p++;
                if (mode == '-') break;
                std::string key, scope, src;
                long at = 0;
                if (mode != 'f' || !r.blob(key) || !r.num(at)) return false;
                // Compiling the source fills the cache slot the capture keyed
                // on, as for the flags in a save.
                split_script_cache_key(key, scope, src);
                std::vector<std::shared_ptr<bool>> in_body;
                if (auto compiled = compile_script(src, scope))
                    collect_firsttime(*compiled, in_body);
                if (at >= 0 && (size_t)at < in_body.size())
                    a.ran = in_body[(size_t)at];
                break;
            }
            default:
                break;
            }
            t->actions.push_back(std::move(a));
        }
        txns.push_back(std::move(t));
        previous.push_back(prev);
    }
    std::vector<std::shared_ptr<UndoTransaction>> stack;
    long current = 0;
    if (!r.count(n)) return false;
    for (long i = 0; i < n; ++i) {
        long at = 0;
        if (!r.num(at) || at < 0 || at >= txn_count) return false;
        stack.push_back(txns[(size_t)at]);
    }
    if (!r.num(current) || current < -1 || current >= txn_count ||
        r.end - r.p < 3 || std::memcmp(r.p, "END", 3) != 0)
        return false;

    // All of it parsed: install.
    for (long i = 0; i < txn_count; ++i)
        if (previous[(size_t)i] >= 0)
            txns[(size_t)i]->previous = txns[(size_t)previous[(size_t)i]];
    for (auto &e : r.detached)
        world_.elements.push_back(std::move(e));
    undo_stack_ = std::move(stack);
    redo_stack_.clear();
    current_txn_ = current >= 0 ? txns[(size_t)current] : nullptr;
    undo_logging_ = false;
    undo_logging_resume_ = logging;
    return true;
} catch (const std::bad_alloc &) {
    return false;
} catch (const std::length_error &) {
    return false;
}

}  // namespace aslx
