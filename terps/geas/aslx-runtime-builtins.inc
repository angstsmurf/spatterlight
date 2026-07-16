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

// ASLX type name for a value, matching WorldModel.ConvertTypeToTypeName.
static std::string type_name(const Value &v) {
    switch (v.type) {
    case Value::Type::Null: return "null";
    case Value::Type::String: return "string";
    case Value::Type::Int: return "int";
    case Value::Type::Double: return "double";
    case Value::Type::Boolean: return "boolean";
    case Value::Type::ObjectRef: return "object";
    case Value::Type::Script: return "script";
    case Value::Type::StringList: return "stringlist";
    case Value::Type::ObjectList: return "objectlist";
    case Value::Type::StringDict: return "stringdictionary";
    case Value::Type::ObjectDict: return "objectdictionary";
    case Value::Type::ScriptDict: return "scriptdictionary";
    }
    return "null";
}

// Template lookups: later registrations win (Quest's m_templateLookup overwrite).
static const std::string *find_template(World &w, const std::string &name) {
    for (auto it = w.templates.rbegin(); it != w.templates.rend(); ++it)
        if (it->first == name) return &it->second;
    return nullptr;
}
static const std::string *find_dyn_template(World &w, const std::string &name) {
    for (auto it = w.dynamic_templates.rbegin(); it != w.dynamic_templates.rend();
         ++it)
        if (it->first == name) return &it->second;
    return nullptr;
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
    // The current containment parent, read from the (own) `parent` field so a
    // runtime MoveObject (which sets obj.parent) is reflected -- not the static
    // load-time Element::parent pointer.
    auto parent_of = [&](Element *e) -> Element * {
        if (!e) return nullptr;
        const Value *p = e->field("parent");
        return (p && p->type == Value::Type::ObjectRef) ? world_.find(p->str)
                                                        : nullptr;
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
    if (fn == "hasdouble" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vbool(f && f->type == Value::Type::Double);
    }
    // HasScript / HasDelegateImplementation: a field of Script type. We store a
    // delegate-implementation body as Script too, so both map to the same check.
    if ((fn == "hasscript" || fn == "hasdelegateimplementation") && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vbool(f && f->type == Value::Type::Script);
    }
    if (fn == "getobject" && n == 1) {
        // Exits, turnscripts and the game element are all ElementType.Object in
        // QuestViva (ObjectType.Exit/TurnScript/Game), so GetObject resolves
        // them -- ProcessTextCommand_Exit and dynamically-created turnscripts
        // ("turnscript = GetObject(name)") rely on this.
        Element *e = world_.find(sarg(0));
        return e && (e->elem_type == "object" || e->elem_type == "exit" ||
                     e->elem_type == "turnscript" || e->elem_type == "game")
                   ? vobj(e->name) : vnull();
    }
    if (fn == "gettimer" && n == 1) {
        Element *e = world_.find(sarg(0));
        return e && e->elem_type == "timer" ? vobj(e->name) : vnull();
    }
    // RunDelegateFunction(obj, "delegatename", params...): the expression form
    // of rundelegate; returns the implementation script's return value.
    if (fn == "rundelegatefunction" && n >= 2) {
        std::vector<Value> params(args.begin() + 2, args.end());
        return interp_run_delegate(*this, obj_of(arg(0)), sarg(1), params);
    }
    // GetUIOption: player-UI configuration lookup. Headless has no overrides, so
    // every option reads back empty (Core's "= true"/LengthOf > 0 guards fail).
    if (fn == "getuioption" && n == 1) return vstr(std::string());
    // GetFileURL: front-end URL for a packaged resource. Headless there is no
    // web root; the filename itself is a serviceable stand-in (it only ever
    // lands inside HTML the presentation milestone will interpret).
    if (fn == "getfileurl" && n == 1) return vstr(sarg(0));
    // GetUniqueElementName (WorldModel.GetUniqueElementName): the name itself
    // if free, else the name minus trailing digits with the first free number
    // appended (timeout -> timeout1, timeout2, ...).
    if (fn == "getuniqueelementname" && n == 1) {
        std::string base = sarg(0);
        if (!world_.find(base)) return vstr(base);
        std::string root = base;
        while (!root.empty() && std::isdigit((unsigned char)root.back()))
            root.pop_back();
        for (long k = 1;; ++k) {
            std::string cand = root + std::to_string(k);
            if (!world_.find(cand)) return vstr(cand);
        }
    }
    // IsGameRunning: State != Finished (ExpressionOwner.IsGameRunning). Turn
    // scripts guard on this every turn.
    if (fn == "isgamerunning" && n == 0) return vbool(!world_.finished);
    if (fn == "doesinherit" && n == 2)
        return vbool(does_inherit(world_, obj_of(arg(0)), sarg(1)));
    if (fn == "typeof" && n == 1) return vstr(type_name(arg(0)));
    if (fn == "typeof" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vstr(f ? type_name(*f) : std::string("null"));
    }
    if (fn == "isdefined" && n == 1)
        return vbool(ctx.locals.count(sarg(0)) > 0);
    // Eval(expr[, scope]): evaluate an expression string, optionally with a
    // dictionary supplying local variables (QuestViva ExpressionOwner.Eval). The
    // text processor's {=...} uses this; ParamsForTextProcessor is usually empty.
    if (fn == "eval" && (n == 1 || n == 2)) {
        Context local;
        if (n == 2)
            for (auto &kv : arg(1).dict())
                local.locals[kv.first] = kv.second;  // values are typed per-entry
        return eval(sarg(0), local);
    }

    // ---- world model: objects, children, attributes -----------------------
    if (fn == "allobjects" && n == 0) {
        std::vector<std::string> names;
        for (auto &up : world_.elements)
            if (up->elem_type == "object") names.push_back(up->name);
        return vobjlist(names);
    }
    if (fn == "allcommands" && n == 0) {
        // Verbs ARE commands (QuestViva loads <verb> as ElementType.Command
        // with IsVerb=true) -- ScopeCommands depends on seeing them here.
        std::vector<std::string> names;
        for (auto &up : world_.elements)
            if (up->elem_type == "command" || up->elem_type == "verb")
                names.push_back(up->name);
        return vobjlist(names);
    }
    if (fn == "allexits" && n == 0) {
        std::vector<std::string> names;
        for (auto &up : world_.elements)
            if (up->elem_type == "exit") names.push_back(up->name);
        return vobjlist(names);
    }
    if (fn == "allturnscripts" && n == 0) {
        std::vector<std::string> names;
        for (auto &up : world_.elements)
            if (up->elem_type == "turnscript") names.push_back(up->name);
        return vobjlist(names);
    }
    // Children enumerate in SortIndex order (Elements.GetDirectChildren sorts
    // by the SortIndex MetaField): creation order, except a runtime parent
    // change moves the element to the end of its new parent's children.
    auto direct_children = [&](Element *e) {
        std::vector<Element *> kids;
        for (auto &up : world_.elements)
            if (up->elem_type == "object" && parent_of(up.get()) == e)
                kids.push_back(up.get());
        std::stable_sort(kids.begin(), kids.end(),
                         [](const Element *a, const Element *b) {
                             return a->sort_index < b->sort_index;
                         });
        return kids;
    };
    if (fn == "getdirectchildren" && n == 1) {
        Element *e = obj_of(arg(0));
        std::vector<std::string> names;
        if (e)
            for (Element *k : direct_children(e)) names.push_back(k->name);
        return vobjlist(names);
    }
    if (fn == "getallchildobjects" && n == 1) {
        Element *e = obj_of(arg(0));
        std::vector<std::string> names;
        // Recurse through the parent-field tree. A visited set guards against a
        // cycle introduced by a bad runtime parent assignment.
        std::function<void(Element *)> rec = [&](Element *x) {
            for (Element *k : direct_children(x)) {
                if (std::find(names.begin(), names.end(), k->name) != names.end())
                    continue;
                names.push_back(k->name);
                rec(k);
            }
        };
        if (e) rec(e);
        return vobjlist(names);
    }
    if (fn == "contains" && n == 2) {
        Element *parent = obj_of(arg(0)), *child = obj_of(arg(1));
        bool found = false;
        int guard = 0;
        for (Element *p = parent_of(child); p && guard < 10000;
             p = parent_of(p), ++guard)
            if (p == parent) { found = true; break; }
        return vbool(found);
    }
    if (fn == "getattributenames" && (n == 1 || n == 2)) {
        Element *e = obj_of(arg(0));
        bool inh = (n == 2) && truthy(arg(1));
        std::vector<std::string> names;
        auto collect = [&](Element *x) {
            if (!x) return;
            for (auto &kv : x->fields)
                if (std::find(names.begin(), names.end(), kv.first) == names.end())
                    names.push_back(kv.first);
        };
        collect(e);
        if (inh && e)
            for (auto it = e->inherits.rbegin(); it != e->inherits.rend(); ++it)
                collect(world_.find(*it));
        return vstrlist(names);
    }

    // ---- dictionaries -----------------------------------------------------
    if ((fn == "newdictionary" || fn == "newstringdictionary") && n == 0) {
        Value v; v.type = Value::Type::StringDict; return v;
    }
    if (fn == "newobjectdictionary" && n == 0) {
        Value v; v.type = Value::Type::ObjectDict; return v;
    }
    if (fn == "newscriptdictionary" && n == 0) {
        Value v; v.type = Value::Type::ScriptDict; return v;
    }
    if (fn == "dictionarycount" && n == 1) return vint((long)arg(0).dict().size());
    if (fn == "dictionarycontains" && n == 2) {
        for (auto &kv : arg(0).dict()) if (kv.first == sarg(1)) return vbool(true);
        return vbool(false);
    }
    if (fn == "dictionaryitem" && n == 2) {
        for (auto &kv : arg(0).dict()) if (kv.first == sarg(1)) return kv.second;
        return vnull();
    }
    if (fn == "stringdictionaryitem" && n == 2) {
        for (auto &kv : arg(0).dict())
            if (kv.first == sarg(1)) return vstr(to_string(kv.second));
        return vnull();
    }
    if (fn == "objectdictionaryitem" && n == 2) {
        for (auto &kv : arg(0).dict())
            if (kv.first == sarg(1))
                return kv.second.type == Value::Type::ObjectRef
                           ? kv.second : vobj(to_string(kv.second));
        return vnull();
    }
    if (fn == "scriptdictionaryitem" && n == 2) {
        for (auto &kv : arg(0).dict()) if (kv.first == sarg(1)) return kv.second;
        return vnull();
    }
    if (fn == "stringlistitem" && n == 2) {
        const Value &l = arg(0); long i = iarg(1);
        if (i < 0 || (size_t)i >= l.list().size()) return vnull();
        return vstr(to_string(l.list()[i]));
    }
    if (fn == "objectlistitem" && n == 2) {
        const Value &l = arg(0); long i = iarg(1);
        if (i < 0 || (size_t)i >= l.list().size()) return vnull();
        const Value &e = l.list()[i];
        return e.type == Value::Type::ObjectRef ? e : vobj(to_string(e));
    }

    // ---- templates --------------------------------------------------------
    if (fn == "template" && n == 1) {
        const std::string *t = find_template(world_, sarg(0));
        return vstr(t ? *t : std::string());
    }
    if (fn == "dynamictemplate" && n >= 1) {
        const std::string *t = find_dyn_template(world_, sarg(0));
        if (!t) {  // fall back to a static template of the same name
            const std::string *st = find_template(world_, sarg(0));
            return vstr(st ? *st : std::string());
        }
        Context tc;
        if (n == 2) {
            // One trailing arg binds "object" (an element) or "text" (a string).
            tc.locals[arg(1).type == Value::Type::ObjectRef ? "object" : "text"] =
                arg(1);
        } else {
            for (size_t i = 1; i < n; ++i)
                tc.locals["object" + std::to_string(i)] = arg(i);
        }
        return vstr(to_string(eval(*t, tc)));
    }

    // ---- misc string ------------------------------------------------------
    if (fn == "capfirst" && n == 1) {
        std::string s = sarg(0);
        if (!s.empty()) s[0] = (char)std::toupper((unsigned char)s[0]);
        return vstr(s);
    }
    if (fn == "safexml" && n == 1) {
        return vstr(safe_xml_escape(sarg(0)));
    }

    // ---- lists ------------------------------------------------------------
    if ((fn == "newlist" || fn == "newstringlist") && n == 0) return vstrlist({});
    if (fn == "newobjectlist" && n == 0) return vobjlist({});
    if (fn == "listcount" && n == 1) return vint((long)arg(0).list().size());
    if (fn == "listitem" && n == 2) {
        const Value &l = arg(0);
        long i = iarg(1);
        if (i < 0 || (size_t)i >= l.list().size()) return vnull();
        return l.list()[i];  // entries are typed
    }
    if (fn == "listcontains" && n == 2) {
        for (const Value &e : arg(0).list())
            if (values_equal(e, arg(1))) return vbool(true);
        return vbool(false);
    }
    if (fn == "listexclude" && n == 2) {
        // QuestList.Exclude filters ALL matching entries into a new list; the
        // second arg may be a single element or a list of elements to exclude.
        Value out = arg(0);
        out.detach();  // return a new list; do not mutate the input's backing
        auto excluded = [&](const Value &e) {
            if (is_list(arg(1))) {
                for (const Value &x : arg(1).list())
                    if (values_equal(e, x)) return true;
                return false;
            }
            return values_equal(e, arg(1));
        };
        auto &v = out.list();
        v.erase(std::remove_if(v.begin(), v.end(), excluded), v.end());
        return out;
    }
    if (fn == "listcombine" && n == 2) {
        Value out = arg(0);
        out.detach();  // return a new list; do not mutate the input's backing
        for (const Value &e : arg(1).list()) out.list().push_back(e);
        return out;
    }
    // ObjectListSort(list, attr[, attr2...]): stably order an object list by one
    // or more attributes (ExpressionOwner.ObjectListSort -> OrderBy/ThenBy). A
    // missing attribute sorts as empty. Descending reverses the result.
    if ((fn == "objectlistsort" || fn == "objectlistsortdescending") && n >= 2) {
        Value out = arg(0);
        out.detach();
        std::vector<std::string> attrs;
        for (int i = 1; i < n; ++i) attrs.push_back(to_string(arg(i)));
        auto key_at = [&](const Value &el, size_t k) -> std::string {
            Element *e = world_.find(to_string(el));
            const Value *f = e ? resolve_field(e, attrs[k]) : nullptr;
            return f ? to_string(*f) : std::string();
        };
        std::stable_sort(out.list().begin(), out.list().end(),
                         [&](const Value &a, const Value &b) {
                             for (size_t k = 0; k < attrs.size(); ++k) {
                                 std::string ka = key_at(a, k), kb = key_at(b, k);
                                 if (ka != kb) return ka < kb;
                             }
                             return false;
                         });
        if (fn == "objectlistsortdescending")
            std::reverse(out.list().begin(), out.list().end());
        return out;
    }
    if ((fn == "stringlistsort" || fn == "stringlistsortdescending") && n == 1) {
        Value out = arg(0);
        out.detach();
        std::stable_sort(out.list().begin(), out.list().end(),
                         [](const Value &a, const Value &b) {
                             return to_string(a) < to_string(b);
                         });
        if (fn == "stringlistsortdescending")
            std::reverse(out.list().begin(), out.list().end());
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
    // ExpressionOwner.IsInt/IsDouble: int.TryParse / double.TryParse (decimal
    // point only, no exponent/thousands) on the string form.
    if (fn == "isint" && n == 1) {
        std::string s = rt_trim(sarg(0));
        size_t i = (s.size() && (s[0] == '-' || s[0] == '+')) ? 1 : 0;
        bool ok = i < s.size();
        for (size_t k = i; k < s.size(); ++k)
            if (!std::isdigit((unsigned char)s[k])) { ok = false; break; }
        return vbool(ok);
    }
    if (fn == "isdouble" && n == 1) {
        std::string s = rt_trim(sarg(0));
        size_t i = (s.size() && (s[0] == '-' || s[0] == '+')) ? 1 : 0;
        bool digits = false, dot = false, ok = i < s.size();
        for (size_t k = i; k < s.size() && ok; ++k) {
            if (std::isdigit((unsigned char)s[k])) digits = true;
            else if (s[k] == '.' && !dot) dot = true;
            else ok = false;
        }
        return vbool(ok && digits);
    }
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
        for (size_t i = 0; i < l.list().size(); ++i) {
            if (i) out += delim;
            out += to_string(l.list()[i]);
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

    // ---- regex (parser primitives) ----------------------------------------
    // IsRegexMatch/GetMatchStrength/Populate mirror QuestViva's Utility.*; the
    // optional 3rd arg is a RegexCache cacheID (the command name). See the
    // rewrite_dotnet_regex note at the top of aslx-runtime.cc.
    if (fn == "isregexmatch" && (n == 2 || n == 3)) {
        std::string cid = sarg(2);
        auto cr = compiled_regex(sarg(0), n == 3 ? &cid : nullptr);
        if (!cr->valid) return vbool(false);
        return vbool(std::regex_search(sarg(1), cr->re));
    }
    if (fn == "getmatchstrength" && (n == 2 || n == 3)) {
        std::string cid = sarg(2), input = sarg(1);
        auto cr = compiled_regex(sarg(0), n == 3 ? &cid : nullptr);
        std::smatch m;
        if (!cr->valid || !std::regex_search(input, m, cr->re)) {
            world_.errors.push_back("String '" + input +
                                    "' is not a match for Regex '" + sarg(0) + "'");
            return vint((long)input.size());
        }
        return vint((long)input.size() - named_group_len(*cr, m));
    }
    if (fn == "populate" && (n == 2 || n == 3)) {
        std::string cid = sarg(2), input = sarg(1);
        auto cr = compiled_regex(sarg(0), n == 3 ? &cid : nullptr);
        Value out; out.type = Value::Type::StringDict;
        std::smatch m;
        if (!cr->valid || !std::regex_search(input, m, cr->re)) {
            world_.errors.push_back("String '" + input +
                                    "' is not a match for Regex '" + sarg(0) + "'");
            return out;
        }
        for (const auto &g : named_groups(*cr, m))
            out.dict().emplace_back(g.first, vstr(g.second));
        return out;
    }

    // ---- numbers ----------------------------------------------------------
    auto num_of = [&](const Value &v) -> double {
        if (v.type == Value::Type::String) return std::strtod(v.str.c_str(), nullptr);
        return as_double(v);
    };
    if (fn == "cint" && n == 1) return vint((long)std::llround(num_of(arg(0))));
    if (fn == "cdbl" && n == 1) return vdouble(num_of(arg(0)));
    // ToInt/ToDouble parse a string; ToString formats a number. (CInt/CDbl
    // coerce any value; ToInt/ToDouble are the string-parse forms.)
    if (fn == "toint" && n == 1) return vint((long)std::llround(num_of(arg(0))));
    if (fn == "todouble" && n == 1) return vdouble(num_of(arg(0)));
    if (fn == "tostring" && n == 1) return vstr(to_string(arg(0)));
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
