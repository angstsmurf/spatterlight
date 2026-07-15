// aslx-runtime-builtins.inc -- built-in expression functions (ExpressionOwner /
// StringFunctions subset). Textually included at the end of aslx-runtime.cc.
// A focused milestone-2 set: enough type/attribute, list, string and number
// helpers to run a hand-written .aslx. GetRandomInt/Double route through the
// deterministic Rng (erkyrath_random). String/list positions are 1-based, as in
// Quest (VB-style Mid/Left/Right/Instr).

namespace aslx {

static std::string to_lower(std::string s) {
    for (char &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Recursive "does e inherit type t" (directly or transitively).
static bool does_inherit(World &w, Element *e, const std::string &t) {
    if (!e) return false;
    for (const std::string &parent : e->inherits) {
        if (parent == t) return true;
        if (does_inherit(w, w.find(parent), t)) return true;
    }
    return false;
}

Value Interp::call_builtin(const std::string &name, std::vector<Value> &args,
                           bool &handled, Context &ctx) {
    handled = true;
    const std::string fn = to_lower(name);
    const size_t n = args.size();
    auto arg = [&](size_t i) -> const Value & {
        static Value nullv;
        nullv = vnull();
        return i < n ? args[i] : nullv;
    };
    auto obj_of = [&](const Value &v) -> Element * {
        return v.type == Value::Type::ObjectRef ? world_.find(v.str) : nullptr;
    };
    auto sarg = [&](size_t i) { return to_string(arg(i)); };
    auto iarg = [&](size_t i) { return (long)std::llround(as_double(arg(i))); };

    // ---- attribute / type -------------------------------------------------
    if (fn == "getattribute" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return f ? *f : vnull();
    }
    if (fn == "getboolean" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vbool(f && f->type == Value::Type::Boolean && f->boolean);
    }
    if (fn == "getint" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vint(f ? (long)std::llround(as_double(*f)) : 0);
    }
    if (fn == "getdouble" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vdouble(f ? as_double(*f) : 0);
    }
    if (fn == "getstring" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vstr(f ? to_string(*f) : std::string());
    }
    if (fn == "hasattribute" && n == 2)
        return vbool(resolve_field(obj_of(arg(0)), sarg(1)) != nullptr);
    if (fn == "hasstring" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vbool(f && f->type == Value::Type::String);
    }
    if (fn == "hasint" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vbool(f && (f->type == Value::Type::Int || f->type == Value::Type::Double));
    }
    if (fn == "hasboolean" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vbool(f && f->type == Value::Type::Boolean);
    }
    if (fn == "hasobject" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vbool(f && f->type == Value::Type::ObjectRef);
    }
    if (fn == "getobject" && n == 1) {
        Element *e = world_.find(sarg(0));
        return e ? vobj(e->name) : vnull();
    }
    if (fn == "doesinherit" && n == 2)
        return vbool(does_inherit(world_, obj_of(arg(0)), sarg(1)));
    if (fn == "typeof" && n == 1) {
        switch (arg(0).type) {
        case Value::Type::Int: return vstr("int");
        case Value::Type::Double: return vstr("double");
        case Value::Type::Boolean: return vstr("boolean");
        case Value::Type::String: return vstr("string");
        case Value::Type::ObjectRef: return vstr("object");
        case Value::Type::StringList:
        case Value::Type::ObjectList: return vstr("list");
        default: return vstr("null");
        }
    }
    if (fn == "isdefined" && n == 1)
        return vbool(ctx.locals.count(sarg(0)) > 0);

    // ---- lists ------------------------------------------------------------
    if ((fn == "newlist" || fn == "newstringlist") && n == 0) return vstrlist({});
    if (fn == "newobjectlist" && n == 0) return vobjlist({});
    if (fn == "listcount" && n == 1) return vint((long)arg(0).list.size());
    if (fn == "listitem" && n == 2) {
        const Value &l = arg(0);
        long i = iarg(1);
        if (i < 0 || (size_t)i >= l.list.size()) return vnull();
        return l.type == Value::Type::ObjectList ? vobj(l.list[i]) : vstr(l.list[i]);
    }
    if (fn == "listcontains" && n == 2) {
        const Value &l = arg(0);
        std::string item = to_string(arg(1));
        for (const std::string &s : l.list) if (s == item) return vbool(true);
        return vbool(false);
    }
    if (fn == "listexclude" && n == 2) {
        Value out = arg(0);
        std::string item = to_string(arg(1));
        out.list.erase(std::remove(out.list.begin(), out.list.end(), item),
                       out.list.end());
        return out;
    }
    if (fn == "listcombine" && n == 2) {
        Value out = arg(0);
        for (const std::string &s : arg(1).list) out.list.push_back(s);
        return out;
    }

    // ---- strings ----------------------------------------------------------
    if (fn == "lcase" && n == 1) return vstr(to_lower(sarg(0)));
    if (fn == "ucase" && n == 1) {
        std::string s = sarg(0);
        for (char &c : s) c = (char)std::toupper((unsigned char)c);
        return vstr(s);
    }
    if (fn == "left" && n == 2) {
        std::string s = sarg(0); long k = iarg(1);
        if (k < 0) k = 0;
        return vstr(s.substr(0, std::min((size_t)k, s.size())));
    }
    if (fn == "right" && n == 2) {
        std::string s = sarg(0); long k = iarg(1);
        if (k < 0) k = 0;
        size_t kk = std::min((size_t)k, s.size());
        return vstr(s.substr(s.size() - kk));
    }
    if (fn == "mid" && (n == 2 || n == 3)) {
        std::string s = sarg(0);
        long start = iarg(1);  // 1-based
        if (start < 1) start = 1;
        if ((size_t)start > s.size()) return vstr("");
        size_t from = (size_t)(start - 1);
        if (n == 3) {
            long len = iarg(2);
            if (len < 0) len = 0;
            return vstr(s.substr(from, (size_t)len));
        }
        return vstr(s.substr(from));
    }
    if (fn == "instr" && (n == 2 || n == 3)) {
        long start = 1; std::string s, find;
        if (n == 2) { s = sarg(0); find = sarg(1); }
        else { start = iarg(0); s = sarg(1); find = sarg(2); }
        if (start < 1) start = 1;
        if ((size_t)(start - 1) > s.size()) return vint(0);
        size_t pos = s.find(find, (size_t)(start - 1));
        return vint(pos == std::string::npos ? 0 : (long)pos + 1);
    }
    if (fn == "replace" && n == 3) {
        std::string s = sarg(0), a = sarg(1), b = sarg(2);
        if (!a.empty()) {
            size_t pos = 0;
            while ((pos = s.find(a, pos)) != std::string::npos) {
                s.replace(pos, a.size(), b);
                pos += b.size();
            }
        }
        return vstr(s);
    }
    if (fn == "trim" && n == 1) return vstr(rt_trim(sarg(0)));
    if ((fn == "lengthof") && n == 1) return vint((long)sarg(0).size());
    if (fn == "startswith" && n == 2) {
        std::string s = sarg(0), p = sarg(1);
        return vbool(s.compare(0, p.size(), p) == 0);
    }
    if (fn == "endswith" && n == 2) {
        std::string s = sarg(0), p = sarg(1);
        return vbool(s.size() >= p.size() &&
                     s.compare(s.size() - p.size(), p.size(), p) == 0);
    }
    if (fn == "split" && n == 2) {
        std::string s = sarg(0), delim = sarg(1);
        std::vector<std::string> parts;
        if (delim.empty()) { parts.push_back(s); }
        else {
            size_t pos = 0, next;
            while ((next = s.find(delim, pos)) != std::string::npos) {
                parts.push_back(s.substr(pos, next - pos));
                pos = next + delim.size();
            }
            parts.push_back(s.substr(pos));
        }
        return vstrlist(parts);
    }
    if (fn == "join" && n == 2) {
        std::string out, delim = sarg(1);
        const Value &l = arg(0);
        for (size_t i = 0; i < l.list.size(); ++i) {
            if (i) out += delim;
            out += l.list[i];
        }
        return vstr(out);
    }
    if ((fn == "cstr" || fn == "tostring") && n == 1) return vstr(sarg(0));
    if (fn == "isnumeric" && n == 1) {
        std::string s = rt_trim(sarg(0));
        if (s.empty()) return vbool(false);
        char *end = nullptr;
        std::strtod(s.c_str(), &end);
        return vbool(end && *end == 0);
    }

    // ---- numbers ----------------------------------------------------------
    auto num_of = [&](const Value &v) -> double {
        if (v.type == Value::Type::String) return std::strtod(v.str.c_str(), nullptr);
        return as_double(v);
    };
    if (fn == "cint" && n == 1) return vint((long)std::llround(num_of(arg(0))));
    if (fn == "cdbl" && n == 1) return vdouble(num_of(arg(0)));
    if (fn == "abs" && n == 1) {
        if (arg(0).type == Value::Type::Int) return vint(std::labs(arg(0).integer));
        return vdouble(std::fabs(as_double(arg(0))));
    }
    if (fn == "max" && n == 2)
        return as_double(arg(0)) >= as_double(arg(1)) ? arg(0) : arg(1);
    if (fn == "min" && n == 2)
        return as_double(arg(0)) <= as_double(arg(1)) ? arg(0) : arg(1);
    if (fn == "getrandomint" && n == 2)
        return vint(rng_.between(iarg(0), iarg(1)));
    if (fn == "getrandomdouble" && n == 0)
        return vdouble(rng_.next_double());

    handled = false;
    return vnull();
}

}  // namespace aslx
